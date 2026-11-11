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

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPageSize2M = 2 * 1024 * 1024;
constexpr uint32_t kPagesPer2M = 512;
// `log2(sizeof(struct page))` for Linux's standard 64-byte `struct page` descriptor.
constexpr uint64_t kStructPageShift = 6;

// Offset between Unix epoch (1970) and Windows FILETIME epoch (1601) used by DR
// raw trace timestamps (`instru_t::get_timestamp()`).
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

// `/proc/<pid>/pagemap` entry bits and `/proc/kpageflags` (`KPF_THP`) bit mask.
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
constexpr uint64_t kKpfThp = 1ULL << 22;

// Writes `val` to a tracefs control file at `path` (truncating or appending).
bool
write_tracefs_file(const std::string &path, const std::string &val, bool append = false)
{
    int fd = open(path.c_str(), O_WRONLY | (append ? O_APPEND : O_TRUNC) | O_CLOEXEC);
    if (fd < 0)
        return false;
    ssize_t n = write(fd, val.data(), val.size());
    close(fd);
    return n == static_cast<ssize_t>(val.size());
}

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

// Parses an unsigned 64-bit integer value following whitespace-bounded `key` in `line`.
bool
extract_u64_field(const std::string &line, const char *key, uint64_t &out, int base,
                  size_t start_pos)
{
    const size_t key_len = strlen(key);
    for (size_t pos = start_pos; (pos = line.find(key, pos)) != std::string::npos;
         ++pos) {
        if (pos > 0 && line[pos - 1] != ' ' && line[pos - 1] != '\t')
            continue;
        const char *start = line.c_str() + pos + key_len;
        char *end = nullptr;
        errno = 0;
        out = strtoull(start, &end, base);
        return end != start && errno == 0;
    }
    return false;
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
    registered_probes_ = { "anon_new" + sfx, "unmap_range" + sfx, "unmap_vmas" + sfx,
                           "unmap_zap" + sfx, "split_pmd" + sfx };
    kernel_tracing_active_ = true;
    cleanup_kernel_tracing();
    kernel_tracing_active_ = true;
    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST)
        return fail();

    // Configure the instance ring buffer: use `CLOCK_TAI` on Linux 5.19+ (unaffected by
    // NTP slew) or `CLOCK_MONOTONIC`, and track child threads via `event-fork` and
    // `set_event_pid`.
    const bool use_tai = kmaj > 5 || (kmaj == 5 && kmin >= 19);
    if (!write_tracefs_file(instance_dir_ + "/trace_clock", use_tai ? "tai" : "mono") ||
        (buffer_size_kb > 0 &&
         !write_tracefs_file(instance_dir_ + "/buffer_size_kb",
                             std::to_string(buffer_size_kb))) ||
        !write_tracefs_file(instance_dir_ + "/options/event-fork", "1") ||
        !write_tracefs_file(instance_dir_ + "/set_event_pid", std::to_string(pid))) {
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

    const std::string kp = tracefs_root_ + "/kprobe_events";
    auto add_probe = [&](const char *base, const char *def) {
        std::string name = base + sfx;
        if (!write_tracefs_file(kp, "p:kprobes/" + name + " " + def + "\n", true))
            return false;
        registered_probes_.push_back(name);
        return true;
    };

    // The kernel calls `folio_add_new_anon_rmap` (or `page_add_new_anon_rmap` on older
    // kernels) whenever it maps a newly allocated anonymous page into a process, so we
    // hook it to cover first-touch heap/stack/`MAP_ANONYMOUS` faults, Copy-on-Write
    // faults, and new 2 MB anonymous THP faults. Prefer `folio_add_new_anon_rmap` when
    // present and fall back to `page_add_new_anon_rmap`.
    anon_probe_name_ = "anon_new" + sfx;
    use_folio_anon_ =
        add_probe("anon_new", "folio_add_new_anon_rmap page=$arg1:x64 vaddr=$arg3:x64");
    if (!use_folio_anon_ &&
        !add_probe(
            "anon_new",
            "page_add_new_anon_rmap page=$arg1:x64 vaddr=$arg3:x64 comp=$arg4:u8")) {
        return fail();
    }
    if (use_folio_anon_)
        kpageflags_fd_ = open("/proc/kpageflags", O_RDONLY | O_CLOEXEC);

    // The kernel calls `unmap_page_range` (or `unmap_vmas` and `zap_vma_range_batched`
    // on Linux 6.12+ where `unmap_page_range` is inlined) whenever it tears down
    // virtual page-table entries for a range, so we hook it to cover explicit unmappings
    // (`munmap`, `MADV_DONTNEED`, `MADV_FREE`, `MAP_FIXED` overwrites, and file
    // truncation/hole-punching).
    if (!add_probe("unmap_range", "unmap_page_range start=$arg3:x64 end=$arg4:x64") &&
        (!add_probe("unmap_vmas", "unmap_vmas start=+16($arg2):x64 end=+24($arg2):x64") ||
         !add_probe("unmap_zap",
                    "zap_vma_range_batched start=$arg3:x64 size=$arg4:u64"))) {
        return fail();
    }

    // The kernel calls `__split_huge_pmd` whenever it breaks a 2 MB PMD mapping down
    // into 4 KB PTEs (or zaps a file/shmem PMD), so we hook it to cover in-place
    // anonymous 2 MB THP splits (`V2P_RECORD_SPLIT`) and file/shmem PMD invalidations
    // (`V2P_RECORD_UNMAP`).
    if (!add_probe("split_pmd",
                   "__split_huge_pmd vm_ops=+112($arg1):x64 vaddr=$arg3:x64")) {
        return fail();
    }

    // Open `trace_pipe` for non-blocking reads and calibrate `vmemmap_base_` before
    // enabling live collection.
    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0 || !calibrate_vmemmap_base())
        return fail();

    // Enable all registered probes (including re-enabling `anon_new` after calibration)
    // and turn on ring-buffer recording BEFORE scanning `/proc/<pid>/pagemap` so any
    // concurrent page fault or unmap during the scan is captured.
    for (const std::string &p : registered_probes_) {
        if (!write_tracefs_file(instance_dir_ + "/events/kprobes/" + p + "/enable", "1"))
            return fail();
    }
    if (!write_tracefs_file(instance_dir_ + "/tracing_on", "1"))
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
    const std::string ev_dir = "/events/kprobes/" + anon_probe_name_;
    const std::string tid_flt =
        "common_pid == " + std::to_string(static_cast<long>(syscall(SYS_gettid)));
    if (!write_tracefs_file(instance_dir_ + ev_dir + "/filter", tid_flt) ||
        !write_tracefs_file(instance_dir_ + ev_dir + "/enable", "1") ||
        !write_tracefs_file(instance_dir_ + "/tracing_on", "1")) {
        return false;
    }

    // Trigger a single 4 KB write fault, freeze `tracing_on` immediately, and look up
    // the resulting PFN in `/proc/self/pagemap` before unmapping the temporary page.
    void *cal_page = mmap_temp_page(PROT_READ | PROT_WRITE);
    uint64_t cal_addr = reinterpret_cast<uint64_t>(cal_page);
    uint64_t cal_pfn = 0;
    if (cal_page != nullptr) {
        *static_cast<volatile uint8_t *>(cal_page) = 1;
        write_tracefs_file(instance_dir_ + "/tracing_on", "0");
        cal_pfn = lookup_pagemap_pfn(getpid(), cal_addr);
        syscall(SYS_munmap, cal_page, kPageSize4K);
    } else {
        write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    }
    write_tracefs_file(instance_dir_ + ev_dir + "/enable", "0");
    write_tracefs_file(instance_dir_ + ev_dir + "/filter", "0");

    // Drain `trace_pipe` directly so the temporary calibration page is not recorded in
    // `v2p.bin`.
    char chunk[4096];
    std::string buf;
    for (ssize_t n = 0; (n = read(trace_pipe_fd_, chunk, sizeof(chunk))) > 0;)
        buf.append(chunk, static_cast<size_t>(n));

    uint64_t page = 0, va = 0;
    if (cal_pfn != 0 && extract_u64_field(buf, "vaddr=", va, 16, 0) &&
        (va & ~(kPageSize4K - 1)) == cal_addr &&
        extract_u64_field(buf, "page=", page, 16, 0) &&
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

// Closes `trace_pipe` and `/proc/kpageflags`, disables events in the isolated instance,
// removes the instance directory, and unregisters this session's kprobes.
void
v2p_tracker_t::cleanup_kernel_tracing()
{
    if (trace_pipe_fd_ >= 0) {
        close(trace_pipe_fd_);
        trace_pipe_fd_ = -1;
    }
    if (kpageflags_fd_ >= 0) {
        close(kpageflags_fd_);
        kpageflags_fd_ = -1;
    }
    if (!kernel_tracing_active_)
        return;

    write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    write_tracefs_file(instance_dir_ + "/events/enable", "0");
    rmdir(instance_dir_.c_str());
    for (const std::string &p : registered_probes_) {
        write_tracefs_file(tracefs_root_ + "/kprobe_events", "-:kprobes/" + p + "\n",
                           /*append=*/true);
    }
    registered_probes_.clear();
    anon_probe_name_.clear();
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
        write_tracefs_file(instance_dir_ + "/tracing_on", "0");

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

// Parses one `trace_pipe` event line (`<comm>-<tid> [<cpu>] <flags> <secs>.<usecs>:
// <probe>_<pid>: ...`) and applies anonymous faults, range unmaps, and 2 MB PMD splits.
void
v2p_tracker_t::process_trace_line(const std::string &line)
{
    const std::string sfx = "_" + std::to_string(target_pid_) + ":";
    size_t sp = line.find(sfx);
    size_t ev = (sp == std::string::npos) ? sp : line.rfind(": ", sp);
    if (ev == std::string::npos || ev == 0)
        return;

    // Ignore any page faults generated by the reader thread itself.
    pid_t rtid = reader_tid_.load(std::memory_order_relaxed);
    size_t ts_p = line.rfind(' ', ev - 1);
    if (ts_p == std::string::npos ||
        (rtid > 0 && line.find("-" + std::to_string(rtid) + " [") < ts_p)) {
        return;
    }

    // Parse the `<secs>.<usecs>:` kernel clock timestamp and convert to DR UTC micros.
    unsigned long long secs = 0, usecs = 0;
    if (sscanf(line.c_str() + ts_p + 1, "%llu.%llu:", &secs, &usecs) != 2)
        return;
    uint64_t ts_us = secs * 1000000ULL + usecs + clock_offset_us_;
    std::string name = line.substr(ev + 2, sp - (ev + 2));

    // 1. Anonymous / CoW page fault (`anon_new`).
    if (name == "anon_new") {
        uint64_t page_ptr = 0, vaddr = 0;
        if (!extract_u64_field(line, "page=", page_ptr, 16, ev) ||
            page_ptr < vmemmap_base_ ||
            !extract_u64_field(line, "vaddr=", vaddr, 16, ev)) {
            return;
        }
        uint64_t pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
        if (pfn >= (1ULL << 52))
            return;

        // Check whether the fault installed a 2 MB anonymous THP (`comp != 0` on
        // `page_add_new_anon_rmap`, or `KPF_THP` in `/proc/kpageflags` on 2 MB-aligned
        // `folio_add_new_anon_rmap` faults) rather than a 4 KB page.
        bool is_2m = false;
        if (!use_folio_anon_) {
            uint64_t comp = 0;
            is_2m = extract_u64_field(line, "comp=", comp, 0, ev) && comp != 0;
        } else if ((vaddr & (kPageSize2M - 1)) == 0 && (pfn & (kPagesPer2M - 1)) == 0 &&
                   kpageflags_fd_ >= 0) {
            uint64_t kpf = 0;
            off_t off = static_cast<off_t>(pfn * sizeof(uint64_t));
            is_2m = pread(kpageflags_fd_, &kpf, sizeof(kpf), off) == sizeof(kpf) &&
                (kpf & kKpfThp) != 0;
        }
        uint64_t mask = (is_2m ? kPageSize2M : kPageSize4K) - 1;
        bind_pages(ts_us, vaddr & ~mask, (pfn * kPageSize4K) & ~mask,
                   is_2m ? kPagesPer2M : 1, is_2m);
        return;
    }

    // 2. Explicit virtual range unmapping (`unmap_range`, `unmap_vmas`, `unmap_zap`).
    if (name == "unmap_range" || name == "unmap_vmas" || name == "unmap_zap") {
        uint64_t st = 0, en = 0, sz = 0;
        if (!extract_u64_field(line, "start=", st, 16, ev))
            return;
        if (!extract_u64_field(line, "end=", en, 16, ev) &&
            extract_u64_field(line, "size=", sz, 0, ev) && sz > 0) {
            en = (sz > (std::numeric_limits<uint64_t>::max)() - st)
                ? (std::numeric_limits<uint64_t>::max)()
                : st + sz;
        }
        constexpr uint64_t kMax =
            (std::numeric_limits<uint64_t>::max)() & ~(kPageSize4K - 1);
        uint64_t lo = st & ~(kPageSize4K - 1);
        uint64_t hi = (en > kMax) ? kMax : ((en + kPageSize4K - 1) & ~(kPageSize4K - 1));
        if (en > st && hi > lo) {
            carve_range(ts_us, lo, hi, /*emit_unmap=*/true, /*preserve_huge=*/true);
        }
        return;
    }

    // 3. 2 MB PMD split (`split_pmd`): `vm_ops == 0` identifies anonymous VMAs.
    if (name == "split_pmd") {
        uint64_t vm_ops = 0, vaddr = 0;
        if (extract_u64_field(line, "vm_ops=", vm_ops, 16, ev) &&
            extract_u64_field(line, "vaddr=", vaddr, 16, ev)) {
            split_huge_pmd(ts_us, vaddr & ~(kPageSize2M - 1), /*is_anon=*/(vm_ops == 0));
        }
    }
}

// Entry point for the background client reader thread: marks itself non-suspendable by
// DR synchall, excludes its own TID from tracefs, and polls `trace_pipe` until `stop()`.
void
v2p_tracker_t::reader_thread_entry(void *arg)
{
    auto *self = static_cast<v2p_tracker_t *>(arg);
    dr_client_thread_set_suspendable(false);
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    write_tracefs_file(self->instance_dir_ + "/set_event_notrace_pid",
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
