/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "v2p_tracker.h"

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <algorithm>
#    include <ctype.h>
#    include <dirent.h>
#    include <limits>
#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <sys/syscall.h>
#    include <sys/utsname.h>
#    include <sys/wait.h>
#    include <time.h>
#    include <unistd.h>

#    ifndef __WCLONE
#        define __WCLONE 0x80000000
#    endif

namespace dynamorio {
namespace drmemtrace {
namespace {

/* -------------------------------------------------------------------------
 * Pagemap, Tracefs Control & Output Record Helpers
 * ------------------------------------------------------------------------- */

// Offset between Unix epoch (1970) and Windows FILETIME epoch (1601) used by DR
// raw trace timestamps (`instru_t::get_timestamp()`).
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

// `/proc/<pid>/pagemap` entry bits and `/proc/kpageflags` (`KPF_THP`) bit mask.
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
constexpr uint64_t kKpfThp = 1ULL << 22;

// Writes a 32-byte `v2p_record_t` entry to `out_fd`, retrying on `EINTR` or short writes
// so fixed-stride binary records are never misaligned.
void
write_record(int out_fd, uint64_t ts, uint64_t va, uint64_t pa, uint32_t pages,
             uint32_t type)
{
    if (out_fd < 0)
        return;
    v2p_record_t rec = { ts, va, pa, pages, type };
    const char *ptr = reinterpret_cast<const char *>(&rec);
    for (size_t rem = sizeof(rec); rem > 0;) {
        ssize_t n = write(out_fd, ptr, rem);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        ptr += n;
        rem -= static_cast<size_t>(n);
    }
}

// Allocates one temporary 4 KB anonymous page via raw `SYS_mmap` (bypassing DR's heap
// and syscall wrappers); returns `nullptr` on raw kernel `-errno` (`-4095..-1`).
void *
mmap_temp_page(int prot)
{
    long ret =
        syscall(SYS_mmap, nullptr, kPageSize4K, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ret <= 0 && ret >= -4095L) ? nullptr : reinterpret_cast<void *>(ret);
}

// Reads `/proc/<pid>/pagemap` for `vaddr` and returns its resident PFN, or 0 if absent.
uint64_t
lookup_pagemap_pfn(pid_t pid, uint64_t vaddr)
{
    int fd =
        open(("/proc/" + std::to_string(pid) + "/pagemap").c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    uint64_t entry = 0;
    off_t off = static_cast<off_t>((vaddr / kPageSize4K) * sizeof(uint64_t));
    bool ok = pread(fd, &entry, sizeof(entry), off) == sizeof(entry) &&
        (entry & kPagemapPresentBit) != 0;
    close(fd);
    return ok ? (entry & kPagemapPfnMask) : 0;
}

} // namespace

/* -------------------------------------------------------------------------
 * Tracker Lifecycle & Tracefs Instance Setup
 * ------------------------------------------------------------------------- */

bool
v2p_tracker_t::start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb)
{
    stop();
    target_pid_ = pid;
    vmemmap_base_ = 0;
    pipe_remainder_.clear();
    shadow_table_ = {};

    // Require 4 KB base pages and at least Linux version 5.4.
    int kmaj = 0, kmin = 0;
    struct utsname uts = {};
    if (dr_page_size() != kPageSize4K || uname(&uts) != 0 ||
        sscanf(uts.release, "%d.%d", &kmaj, &kmin) != 2 ||
        (kmaj < 5 || (kmaj == 5 && kmin < 4))) {
        return false;
    }

    // Locate the mounted tracefs root directory (`instances` support required).
    struct stat st = {};
    if (stat("/sys/kernel/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/tracing";
    else if (stat("/sys/kernel/debug/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/debug/tracing";
    else
        return false;

    // Parse `/sys/kernel/btf/vmlinux` to verify `struct page` layout, validate all
    // hooked kernel function signatures, and resolve struct offsets before attaching.
    std::string btf_error;
    if (!v2p_check_kernel_support("/sys/kernel/btf/vmlinux", plan_, btf_error)) {
        dr_fprintf(STDERR, "ERROR: drmemtrace v2p_tracker: %s\n", btf_error.c_str());
        return false;
    }

    // Open `raw/v2p.bin` for binary record output.
    out_fd_ = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out_fd_ < 0)
        return false;

    auto fail = [this]() {
        cleanup_kernel_tracing();
        if (out_fd_ >= 0) {
            close(out_fd_);
            out_fd_ = -1;
        }
        return false;
    };

    // Pre-clean any leftover instance or kprobes from a recycled PID killed via
    // `SIGKILL`, then create a fresh isolated tracefs instance directory.
    const std::string sfx = "_" + std::to_string(pid);
    instance_dir_ = tracefs_root_ + "/instances/drmemtrace_v2p" + sfx;
    registered_probes_.clear();
    for (const v2p_probe_spec_t &spec : plan_.probes)
        registered_probes_.push_back(spec.base_name + sfx);
    kernel_tracing_active_ = true;
    cleanup_kernel_tracing();
    kernel_tracing_active_ = true;
    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST)
        return fail();

    // Configure the instance ring buffer: use `CLOCK_TAI` on Linux 5.19+ (unaffected by
    // NTP slew) or `CLOCK_MONOTONIC`, and track child threads via `event-fork` and
    // `set_event_pid`.
    const bool use_tai = kmaj > 5 || (kmaj == 5 && kmin >= 19);
    if (!v2p_write_tracefs_file(instance_dir_ + "/trace_clock",
                                use_tai ? "tai" : "mono") ||
        (buffer_size_kb > 0 &&
         !v2p_write_tracefs_file(instance_dir_ + "/buffer_size_kb",
                                 std::to_string(buffer_size_kb))) ||
        !v2p_write_tracefs_file(instance_dir_ + "/options/event-fork", "1") ||
        !v2p_write_tracefs_file(instance_dir_ + "/set_event_pid", std::to_string(pid))) {
        return fail();
    }

    // Measure the offset from tracefs timestamps to DR's UTC/FILETIME microsecond clock.
    struct timespec clk_ts = {}, real_ts = {};
    if (clock_gettime(use_tai ? CLOCK_TAI : CLOCK_MONOTONIC, &clk_ts) != 0 ||
        clock_gettime(CLOCK_REALTIME, &real_ts) != 0) {
        return fail();
    }
    clock_offset_us_ =
        (real_ts.tv_sec * 1000000ULL + real_ts.tv_nsec / 1000ULL + kDrUtcToEpochMicros) -
        (clk_ts.tv_sec * 1000000ULL + clk_ts.tv_nsec / 1000ULL);

    // Register all BTF-validated kprobes and kretprobes from `plan_.probes`.
    const std::string kp = tracefs_root_ + "/kprobe_events";
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        std::string name = spec.base_name + sfx;
        std::string cmd = (spec.is_return ? "r4096:kprobes/" : "p:kprobes/") + name +
            " " + spec.definition + "\n";
        if (!v2p_write_tracefs_file(kp, cmd, /*append=*/true))
            return fail();
        registered_probes_.push_back(name);
    }

    // Open `trace_pipe` for non-blocking reads and calibrate `vmemmap_base_` before
    // enabling live collection.
    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0 || !calibrate_vmemmap_base())
        return fail();

    // Install any static probe filters, enable all registered probes, and turn on
    // ring-buffer recording BEFORE scanning `/proc/<pid>/pagemap` so any concurrent page
    // fault or unmap during the scan is captured.
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        const std::string ev = instance_dir_ + "/events/kprobes/" + spec.base_name + sfx;
        if (!spec.static_filter.empty() &&
            !v2p_write_tracefs_file(ev + "/filter", spec.static_filter)) {
            return fail();
        }
        if (!v2p_write_tracefs_file(ev + "/enable", "1"))
            return fail();
    }
    if (!v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "1"))
        return fail();

    snapshot_pagemap();

    // Spawn the background client reader thread to drain `trace_pipe_fd_` continuously.
    reader_tid_.store(0, std::memory_order_relaxed);
    stop_requested_.store(false, std::memory_order_relaxed);
    thread_exited_.store(false, std::memory_order_relaxed);
    thread_running_.store(true, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    if (!dr_create_client_thread(&v2p_tracker_t::reader_thread_entry, this)) {
        thread_running_.store(false, std::memory_order_release);
        running_.store(false, std::memory_order_release);
        return fail();
    }
    return true;
}

// Calibrates `vmemmap_base_` before live tracing starts:
//   - Why `vmemmap_base_` is needed: hooked kernel MM functions never pass physical RAM
//     addresses; they pass a kernel virtual `struct page *` / `struct folio *` pointer
//     into Linux's flat array of 64-byte page descriptors indexed by physical frame
//     number (`page_ptr = vmemmap_base + pfn * 64`). Because KASLR randomizes where that
//     array starts on every boot, we must measure `vmemmap_base_` at runtime to convert
//     any hooked page pointer into a physical address.
//   - How we measure it: fault one temporary 4 KB page on this thread to observe both
//     its kernel `page=` pointer (from `anon_new`) and its physical frame number (from
//     `/proc/self/pagemap`), giving `vmemmap_base_ = page_ptr - pfn * 64`.
bool
v2p_tracker_t::calibrate_vmemmap_base()
{
    const std::string ev_dir = "/events/kprobes/anon_new_" + std::to_string(target_pid_);
    const std::string tid_flt =
        "common_pid == " + std::to_string(static_cast<long>(syscall(SYS_gettid)));
    if (!v2p_write_tracefs_file(instance_dir_ + ev_dir + "/filter", tid_flt) ||
        !v2p_write_tracefs_file(instance_dir_ + ev_dir + "/enable", "1") ||
        !v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "1")) {
        return false;
    }

    // Trigger a single 4 KB write fault, freeze `tracing_on` immediately, and look up
    // the resulting PFN in `/proc/self/pagemap` before unmapping the temporary page.
    void *cal_page = mmap_temp_page(PROT_READ | PROT_WRITE);
    uint64_t cal_addr = reinterpret_cast<uint64_t>(cal_page);
    uint64_t cal_pfn = 0;
    if (cal_page != nullptr) {
        *static_cast<volatile uint8_t *>(cal_page) = 1;
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
        cal_pfn = lookup_pagemap_pfn(getpid(), cal_addr);
        syscall(SYS_munmap, cal_page, kPageSize4K);
    } else {
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    }
    v2p_write_tracefs_file(instance_dir_ + ev_dir + "/enable", "0");
    v2p_write_tracefs_file(instance_dir_ + ev_dir + "/filter", "0");

    // Drain `trace_pipe` directly so the temporary calibration page is not recorded in
    // `v2p.bin`.
    char chunk[4096];
    std::string buf;
    for (ssize_t n = 0; (n = read(trace_pipe_fd_, chunk, sizeof(chunk))) > 0;)
        buf.append(chunk, static_cast<size_t>(n));

    uint64_t page = 0, va = 0;
    if (cal_pfn != 0 && v2p_extract_u64_field(buf, "vaddr=", va, 16, 0) &&
        (va & ~(kPageSize4K - 1)) == cal_addr &&
        v2p_extract_u64_field(buf, "page=", page, 16, 0) &&
        page > (cal_pfn << kStructPageShift)) {
        uint64_t base = page - (cal_pfn << kStructPageShift);
        if ((base >> 63) != 0 && (base & ((1ULL << kStructPageShift) - 1)) == 0)
            vmemmap_base_ = base;
    }
    return vmemmap_base_ != 0;
}

// Snapshots already-resident 4 KB and 2 MB THP mappings from `/proc/<pid>/smaps` (or
// `maps`), `/proc/<pid>/pagemap`, and `/proc/kpageflags` at timestamp 0.
void
v2p_tracker_t::snapshot_pagemap()
{
    const std::string proc_dir = "/proc/" + std::to_string(target_pid_);
    FILE *maps = fopen((proc_dir + "/smaps").c_str(), "r");
    const bool have_smaps = (maps != nullptr);
    if (!have_smaps)
        maps = fopen((proc_dir + "/maps").c_str(), "r");
    int pm_fd = open((proc_dir + "/pagemap").c_str(), O_RDONLY | O_CLOEXEC);
    int kpf_fd = open("/proc/kpageflags", O_RDONLY | O_CLOEXEC);

    // Step 1: Collect user-space VMAs and note which VMAs have PMD-mapped THPs in
    // `smaps` (falling back to checking every 2 MB contiguous run when `smaps` is
    // unavailable).
    struct vma_desc_t {
        uint64_t start = 0;
        uint64_t end = 0;
        bool has_pmd_thp = false;
    };
    std::vector<vma_desc_t> vmas;
    char line[512];
    while (maps != nullptr && pm_fd >= 0 && fgets(line, sizeof(line), maps) != nullptr) {
        uint64_t s = 0, e = 0, kb = 0;
        if (isxdigit(static_cast<unsigned char>(line[0])) &&
            sscanf(line, "%" SCNx64 "-%" SCNx64, &s, &e) == 2) {
            // Skip kernel-space pseudo-VMAs such as `[vsyscall]` (`>= 0xffff8000...`).
            if (s < 0xffff800000000000ULL && e > s)
                vmas.push_back({ s, e, !have_smaps });
        } else if (!vmas.empty() &&
                   (strncmp(line, "AnonHugePages:", 14) == 0 ||
                    strncmp(line, "ShmemPmdMapped:", 15) == 0 ||
                    strncmp(line, "FilePmdMapped:", 14) == 0) &&
                   sscanf(strchr(line, ':') + 1, "%" SCNu64, &kb) == 1 && kb > 0) {
            vmas.back().has_pmd_thp = true;
        }
    }

    // Step 2: Read `pagemap` in up to 2 MB-aligned batches across each VMA so pre-
    // existing 2 MB THP PMDs are recorded as `is_huge_2m` spans (matching subsequent
    // `__split_huge_pmd` events) before falling back to per-4 KB PTE bindings.
    uint64_t entries[kPagesPer2M];
    for (const vma_desc_t &vma : vmas) {
        for (uint64_t va = vma.start; va < vma.end;) {
            uint64_t batch_end =
                std::min(vma.end, (va + kPageSize2M) & ~(kPageSize2M - 1));
            ssize_t bytes =
                pread(pm_fd, entries, ((batch_end - va) / kPageSize4K) * sizeof(uint64_t),
                      static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t)));
            if (bytes <= 0)
                break;
            size_t n = static_cast<size_t>(bytes) / sizeof(uint64_t);

            // Check whether this batch is a 2 MB-aligned run of 512 contiguous PFNs
            // backed by a `KPF_THP` compound page.
            uint64_t base_pfn = entries[0] & kPagemapPfnMask;
            uint64_t kpf = 0;
            bool is_2m_contig = n == kPagesPer2M && (va & (kPageSize2M - 1)) == 0 &&
                (entries[0] & kPagemapPresentBit) != 0 && base_pfn != 0 &&
                (base_pfn & (kPagesPer2M - 1)) == 0;
            for (size_t i = 1; is_2m_contig && i < kPagesPer2M; ++i) {
                is_2m_contig = (entries[i] & kPagemapPresentBit) != 0 &&
                    (entries[i] & kPagemapPfnMask) == base_pfn + i;
            }
            if (is_2m_contig && vma.has_pmd_thp && kpf_fd >= 0 &&
                pread(kpf_fd, &kpf, sizeof(kpf),
                      static_cast<off_t>(base_pfn * sizeof(uint64_t))) == sizeof(kpf) &&
                (kpf & kKpfThp) != 0) {
                bind_pages(0, va, base_pfn * kPageSize4K, kPagesPer2M,
                           /*is_huge_2m=*/true);
                va += kPageSize2M;
                continue;
            }

            // Otherwise bind each resident 4 KB page in the batch individually.
            for (size_t i = 0; i < n; ++i) {
                uint64_t pfn = entries[i] & kPagemapPfnMask;
                if ((entries[i] & kPagemapPresentBit) != 0 && pfn != 0)
                    bind_pages(0, va + i * kPageSize4K, pfn * kPageSize4K, 1);
            }
            va += n * kPageSize4K;
        }
    }

    if (kpf_fd >= 0)
        close(kpf_fd);
    if (pm_fd >= 0)
        close(pm_fd);
    if (maps != nullptr)
        fclose(maps);
}

/* -------------------------------------------------------------------------
 * Shadow Page Table Operations & Teardown
 * ------------------------------------------------------------------------- */

// Converts a kernel `struct page *` or `struct folio *` pointer into its physical
// byte address by subtracting `vmemmap_base_` (`pfn = (page_ptr - vmemmap_base_) / 64`).
bool
v2p_tracker_t::page_ptr_to_paddr(uint64_t page_ptr, uint64_t &paddr_out) const
{
    if (vmemmap_base_ == 0 || page_ptr < vmemmap_base_)
        return false;
    uint64_t pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
    if (pfn >= (1ULL << 52))
        return false;
    paddr_out = pfn * kPageSize4K;
    return true;
}

// Unmaps `[start_vaddr, end_vaddr)` and emits coalesced `V2P_RECORD_UNMAP` records.
void
v2p_tracker_t::unbind_range(uint64_t timestamp_us, uint64_t start_vaddr,
                            uint64_t end_vaddr)
{
    if (end_vaddr > start_vaddr) {
        carve_range(timestamp_us, start_vaddr, end_vaddr, /*emit_unmap=*/true,
                    /*preserve_huge=*/true);
    }
}

// Carves `[start_vaddr, end_vaddr)` out of `shadow_table_`, optionally emitting
// coalesced `V2P_RECORD_UNMAP` records for all previously mapped sub-ranges.
void
v2p_tracker_t::carve_range(uint64_t timestamp_us, uint64_t start_vaddr,
                           uint64_t end_vaddr, bool emit_unmap, bool preserve_huge)
{
    uint64_t run_start = 0;
    uint32_t run_pages = 0;
    shadow_table_.carve(
        start_vaddr, end_vaddr, preserve_huge,
        // Coalesce contiguous carved virtual ranges into a single `UNMAP` record.
        [&](uint64_t m_lo, uint64_t m_hi) {
            if (!emit_unmap)
                return;
            uint32_t m_pages = static_cast<uint32_t>((m_hi - m_lo) / kPageSize4K);
            if (run_pages > 0 &&
                m_pages <= (std::numeric_limits<uint32_t>::max)() - run_pages &&
                m_lo == run_start + static_cast<uint64_t>(run_pages) * kPageSize4K) {
                run_pages += m_pages;
                return;
            }
            if (run_pages > 0) {
                write_record(out_fd_, timestamp_us, run_start, 0, run_pages,
                             V2P_RECORD_UNMAP);
            }
            run_start = m_lo;
            run_pages = m_pages;
        });
    if (emit_unmap && run_pages > 0)
        write_record(out_fd_, timestamp_us, run_start, 0, run_pages, V2P_RECORD_UNMAP);
}

// Installs `[aligned_vaddr, +page_count * 4 KB) -> aligned_paddr` in `shadow_table_`
// and writes a `V2P_RECORD_MAP` record whenever the mapping changed.
void
v2p_tracker_t::bind_pages(uint64_t timestamp_us, uint64_t aligned_vaddr,
                          uint64_t aligned_paddr, uint32_t page_count, bool is_huge_2m)
{
    if (page_count == 0)
        return;
    uint64_t end_va = aligned_vaddr + static_cast<uint64_t>(page_count) * kPageSize4K;

    // Suppress duplicate `V2P_RECORD_MAP` records if `shadow_table_` already holds the
    // identical mapping across `[aligned_vaddr, end_va)`.
    uint64_t covered = aligned_vaddr;
    for (auto it = shadow_table_.find_overlapping(aligned_vaddr);
         it != shadow_table_.spans.end() && it->first <= covered && covered < end_va;
         ++it) {
        if (it->second.paddr_base + (covered - it->first) !=
                aligned_paddr + (covered - aligned_vaddr) ||
            it->second.is_huge_2m != is_huge_2m) {
            break;
        }
        covered = std::min(
            end_va, v2p_shadow_page_table_t::end_of(it->first, it->second.page_count));
    }
    if (covered == end_va)
        return;

    // A 4 KB mapping inside a 2 MB window means that PMD is now split into 4 KB PTEs.
    if (!is_huge_2m) {
        for (auto it = shadow_table_.find_overlapping(aligned_vaddr & ~(kPageSize2M - 1));
             it != shadow_table_.spans.end() &&
             it->first < ((end_va + kPageSize2M - 1) & ~(kPageSize2M - 1));
             ++it) {
            it->second.is_huge_2m = false;
        }
    }

    carve_range(timestamp_us, aligned_vaddr, end_va, /*emit_unmap=*/false,
                /*preserve_huge=*/false);
    shadow_table_.spans.emplace(
        aligned_vaddr,
        v2p_shadow_page_table_t::span_t { aligned_paddr, page_count, is_huge_2m });
    write_record(out_fd_, timestamp_us, aligned_vaddr, aligned_paddr, page_count,
                 V2P_RECORD_MAP);
}

// Handles `__split_huge_pmd` on `[aligned_2m_va, +2 MB)`:
// - Anonymous 2 MB THP: physical frames stay in place at 4 KB PTE granularity
//   (`V2P_RECORD_SPLIT`).
// - File/shmem 2 MB THP (`!is_anon`): `__split_huge_pmd_locked` clears the PMD without
//   installing 4 KB PTEs, so unmap the 2 MB range (`V2P_RECORD_UNMAP`).
void
v2p_tracker_t::split_huge_pmd(uint64_t timestamp_us, uint64_t aligned_2m_va, bool is_anon)
{
    uint64_t end_2m = aligned_2m_va + kPageSize2M;
    uint64_t base_pa = 0;
    size_t huge_spans = 0;
    bool full_2m = false;
    for (auto it = shadow_table_.find_overlapping(aligned_2m_va);
         it != shadow_table_.spans.end() && it->first < end_2m; ++it) {
        if (!it->second.is_huge_2m)
            continue;
        if (huge_spans++ == 0) {
            base_pa = it->second.paddr_base + (aligned_2m_va - it->first);
            full_2m = it->first == aligned_2m_va &&
                v2p_shadow_page_table_t::end_of(it->first, it->second.page_count) ==
                    end_2m;
        }
        if (is_anon)
            it->second.is_huge_2m = false;
    }
    if (huge_spans == 0)
        return;

    if (is_anon) {
        if (huge_spans == 1 && full_2m) {
            write_record(out_fd_, timestamp_us, aligned_2m_va, base_pa, kPagesPer2M,
                         V2P_RECORD_SPLIT);
        }
    } else {
        carve_range(timestamp_us, aligned_2m_va, end_2m, /*emit_unmap=*/true,
                    /*preserve_huge=*/true);
    }
}

// Sums ring-buffer `overrun:` counts across `per_cpu/cpu*/stats` in the instance.
uint64_t
v2p_tracker_t::read_total_overflows() const
{
    std::string per_cpu = instance_dir_ + "/per_cpu";
    DIR *dir = opendir(per_cpu.c_str());
    if (dir == nullptr)
        return 0;
    uint64_t total = 0;
    for (struct dirent *e = nullptr; (e = readdir(dir)) != nullptr;) {
        if (strncmp(e->d_name, "cpu", 3) != 0)
            continue;
        FILE *fp = fopen((per_cpu + "/" + e->d_name + "/stats").c_str(), "r");
        char buf[256];
        // Match line-initial `overrun:` rather than `commit overrun:`.
        while (fp != nullptr && fgets(buf, sizeof(buf), fp) != nullptr) {
            if (strncmp(buf, "overrun:", 8) == 0)
                total += strtoull(buf + 8, nullptr, 10);
        }
        if (fp != nullptr)
            fclose(fp);
    }
    closedir(dir);
    return total;
}

// Closes `trace_pipe`, disables events in the isolated instance,
// removes the instance directory, and unregisters this session's kprobes.
void
v2p_tracker_t::cleanup_kernel_tracing()
{
    if (trace_pipe_fd_ >= 0) {
        close(trace_pipe_fd_);
        trace_pipe_fd_ = -1;
    }
    if (!kernel_tracing_active_)
        return;

    v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    v2p_write_tracefs_file(instance_dir_ + "/events/enable", "0");
    rmdir(instance_dir_.c_str());
    for (const std::string &p : registered_probes_) {
        v2p_write_tracefs_file(tracefs_root_ + "/kprobe_events", "-:kprobes/" + p + "\n",
                               /*append=*/true);
    }
    registered_probes_.clear();
    kernel_tracing_active_ = false;
}

// Stops tracefs collection, joins the reader thread to drain all buffered events, checks
// per-CPU ring-buffer drop counts, tears down the kernel instance, and appends any
// `V2P_RECORD_OVERFLOW` summary plus `V2P_RECORD_END`.
void
v2p_tracker_t::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;

    // Freeze new ring-buffer writes first so `drain_pipe()` reaches EOF cleanly.
    if (kernel_tracing_active_)
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");

    // Signal the non-suspendable client reader thread to exit and reap its clone task.
    stop_requested_.store(true, std::memory_order_release);
    if (thread_running_.exchange(false, std::memory_order_acq_rel)) {
        while (!thread_exited_.load(std::memory_order_acquire))
            dr_sleep(1);
        pid_t rtid = reader_tid_.exchange(0, std::memory_order_acq_rel);
        while (rtid > 0 && waitpid(rtid, nullptr, __WCLONE) < 0 && errno == EINTR) {
        }
    } else if (kernel_tracing_active_) {
        drain_pipe();
    }

    // Check per-CPU ring-buffer drop counts before removing the tracefs instance.
    uint64_t overflows = kernel_tracing_active_ ? read_total_overflows() : 0;
    if (overflows > 0) {
        dr_fprintf(STDERR,
                   "WARNING: drmemtrace v2p_tracker detected %" PRIu64
                   " dropped tracefs events due to buffer overflow (increase "
                   "-v2p_buffer_size_kb)\n",
                   overflows);
    }

    cleanup_kernel_tracing();

    // Write any overflow summary and the end-of-stream sentinel before closing `v2p.bin`.
    if (out_fd_ >= 0) {
        if (overflows > 0) {
            uint32_t clamped = static_cast<uint32_t>(
                std::min<uint64_t>(overflows, (std::numeric_limits<uint32_t>::max)()));
            write_record(out_fd_, 0, 0, 0, clamped, V2P_RECORD_OVERFLOW);
        }
        write_record(out_fd_, 0, 0, 0, 0, V2P_RECORD_END);
        close(out_fd_);
        out_fd_ = -1;
    }
}

/* -------------------------------------------------------------------------
 * Background Trace-Pipe Reader & Event Parsing
 * ------------------------------------------------------------------------- */

// Reads available non-blocking bytes from `trace_pipe_fd_`, reassembling partial lines
// across reads in `pipe_remainder_` and dispatching each complete line.
void
v2p_tracker_t::drain_pipe()
{
    if (trace_pipe_fd_ < 0)
        return;
    char chunk[16384];
    for (ssize_t n = 0; (n = read(trace_pipe_fd_, chunk, sizeof(chunk))) > 0;) {
        pipe_remainder_.append(chunk, static_cast<size_t>(n));
        for (size_t nl = 0; (nl = pipe_remainder_.find('\n')) != std::string::npos;) {
            process_trace_line(pipe_remainder_.substr(0, nl));
            pipe_remainder_.erase(0, nl + 1);
        }
    }
}

// Parses the ftrace prefix (`<comm>-<tid> [<cpu>] [<irq_flags>] <secs>.<usecs>: `) of a
// `trace_pipe` line and dispatches the event payload to `hooks_`.
void
v2p_tracker_t::process_trace_line(const std::string &line)
{
    pid_t tid = 0;
    uint64_t timestamp_us = 0;
    size_t event_pos = std::string::npos;
    const char *raw = line.c_str();

    // Scan each `" ["` candidate because thread `comm` names may contain `[...]`
    // (e.g., `" tokio-rt[0]-1000 [000] d.s1. 0.000081: ..."`).
    for (size_t cpu_open = 0; (cpu_open = line.find(" [", cpu_open)) != std::string::npos;
         ++cpu_open) {
        size_t tid_end = cpu_open;
        while (tid_end > 0 && line[tid_end - 1] == ' ')
            --tid_end;
        size_t dash = tid_end;
        while (dash > 0 && isdigit(static_cast<unsigned char>(line[dash - 1])))
            --dash;
        if (dash == 0 || dash == tid_end || line[dash - 1] != '-')
            continue;

        char cpu_s[16], flags_s[16] = {}, sec_s[24], usec_s[16];
        char sp1 = 0, sp2 = 0;
        int n = 0;
        bool ok =
            (sscanf(raw + cpu_open + 2, "%15[0-9]]%c %15[^] :] %23[0-9].%15[0-9]:%c%n",
                    cpu_s, &sp1, flags_s, sec_s, usec_s, &sp2, &n) == 6 &&
             sp1 == ' ' && sp2 == ' ') ||
            (sscanf(raw + cpu_open + 2, "%15[0-9]]%c %23[0-9].%15[0-9]:%c%n", cpu_s, &sp1,
                    sec_s, usec_s, &sp2, &n) == 5 &&
             sp1 == ' ' && sp2 == ' ');
        if (!ok)
            continue;
        tid = static_cast<pid_t>(strtol(raw + dash, nullptr, 10));
        timestamp_us = strtoull(sec_s, nullptr, 10) * 1000000ULL +
            strtoull(usec_s, nullptr, 10) + clock_offset_us_;
        event_pos = cpu_open + 2 + static_cast<size_t>(n);
        break;
    }

    // Ignore malformed lines and any page faults generated by the reader thread itself.
    pid_t rtid = reader_tid_.load(std::memory_order_relaxed);
    if (event_pos == std::string::npos || tid <= 0 || (rtid != 0 && tid == rtid) ||
        vmemmap_base_ == 0) {
        return;
    }

    hooks_.handle_event(line, event_pos, tid, timestamp_us, plan_, *this);
}

// Entry point for the background client reader thread: marks itself non-suspendable by
// DR synchall, excludes its own TID from tracefs, and polls `trace_pipe` until `stop()`.
void
v2p_tracker_t::reader_thread_entry(void *arg)
{
    auto *self = static_cast<v2p_tracker_t *>(arg);
    dr_client_thread_set_suspendable(false);
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    v2p_write_tracefs_file(self->instance_dir_ + "/set_event_notrace_pid",
                           std::to_string(tid) + "\n", /*append=*/true);
    self->reader_tid_.store(tid, std::memory_order_release);

    while (!self->stop_requested_.load(std::memory_order_acquire)) {
        struct pollfd pfd = { self->trace_pipe_fd_, POLLIN, 0 };
        if (poll(&pfd, 1, 10) > 0)
            self->drain_pipe();
    }

    // Perform one final drain after `tracing_on` was set to 0 before signaling exit.
    self->drain_pipe();
    self->thread_exited_.store(true, std::memory_order_release);
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
