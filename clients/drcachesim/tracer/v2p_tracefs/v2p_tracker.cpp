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
    shadow_pages_.clear();

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

    // Pre-clean any leftover instance or kprobe from a recycled PID killed via `SIGKILL`,
    // then create a fresh isolated tracefs instance directory for this process.
    instance_dir_ = tracefs_root_ + "/instances/drmemtrace_v2p_" + std::to_string(pid);
    anon_probe_name_ = "anon_new_" + std::to_string(pid);
    kernel_tracing_active_ = true;
    cleanup_kernel_tracing();
    anon_probe_name_ = "anon_new_" + std::to_string(pid);
    kernel_tracing_active_ = true;
    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST) {
        anon_probe_name_.clear();
        return fail();
    }

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
        anon_probe_name_.clear();
        return fail();
    }

    // Measure the offset from tracefs timestamps to DR's UTC/FILETIME microsecond clock.
    struct timespec clk_ts = {}, real_ts = {};
    if (clock_gettime(use_tai ? CLOCK_TAI : CLOCK_MONOTONIC, &clk_ts) != 0 ||
        clock_gettime(CLOCK_REALTIME, &real_ts) != 0) {
        anon_probe_name_.clear();
        return fail();
    }
    clock_offset_us_ =
        (real_ts.tv_sec * 1000000ULL + real_ts.tv_nsec / 1000ULL + kDrUtcToEpochMicros) -
        (clk_ts.tv_sec * 1000000ULL + clk_ts.tv_nsec / 1000ULL);

    // The kernel calls `folio_add_new_anon_rmap` (or `page_add_new_anon_rmap` on older
    // kernels) whenever it maps a newly allocated anonymous page into a process, so we
    // hook it to cover first-touch heap/stack/`MAP_ANONYMOUS` faults, Copy-on-Write
    // faults, and new 2 MB anonymous THP faults. Prefer `folio_add_new_anon_rmap` when
    // present and fall back to `page_add_new_anon_rmap`.
    const std::string kp = tracefs_root_ + "/kprobe_events";
    const std::string pfx = "p:kprobes/" + anon_probe_name_ + " ";
    const char *folio_def = "folio_add_new_anon_rmap page=$arg1:x64 vaddr=$arg3:x64\n";
    const char *page_def =
        "page_add_new_anon_rmap page=$arg1:x64 vaddr=$arg3:x64 comp=$arg4:u8\n";
    use_folio_anon_ = write_tracefs_file(kp, pfx + folio_def, /*append=*/true);
    if (!use_folio_anon_ && !write_tracefs_file(kp, pfx + page_def, /*append=*/true)) {
        anon_probe_name_.clear();
        return fail();
    }
    if (use_folio_anon_)
        kpageflags_fd_ = open("/proc/kpageflags", O_RDONLY | O_CLOEXEC);

    // Open `trace_pipe` for non-blocking reads and calibrate `vmemmap_base_` before
    // enabling live collection.
    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0 || !calibrate_vmemmap_base())
        return fail();

    // Re-enable the page-fault kprobe (disabled at the end of calibration) and turn on
    // ring-buffer recording BEFORE scanning `/proc/<pid>/pagemap` so any concurrent page
    // fault during the scan is captured.
    if (!write_tracefs_file(
            instance_dir_ + "/events/kprobes/" + anon_probe_name_ + "/enable", "1") ||
        !write_tracefs_file(instance_dir_ + "/tracing_on", "1")) {
        return fail();
    }

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

// Snapshots already-resident mappings from `/proc/<pid>/maps` and `/proc/<pid>/pagemap`
// at timestamp 0 (`V2P_RECORD_MAP`).
void
v2p_tracker_t::snapshot_pagemap()
{
    const std::string proc_dir = "/proc/" + std::to_string(target_pid_);
    FILE *maps = fopen((proc_dir + "/maps").c_str(), "r");
    int pm_fd = open((proc_dir + "/pagemap").c_str(), O_RDONLY | O_CLOEXEC);
    char line[512];
    uint64_t entries[kPagesPer2M];

    while (maps != nullptr && pm_fd >= 0 && fgets(line, sizeof(line), maps) != nullptr) {
        uint64_t s = 0, e = 0;
        // Skip malformed lines and kernel-space pseudo-VMAs such as `[vsyscall]`.
        if (sscanf(line, "%" SCNx64 "-%" SCNx64, &s, &e) != 2 ||
            s >= 0xffff800000000000ULL || e <= s) {
            continue;
        }

        // Read `pagemap` in 512-page (2 MB) batches across each user-space VMA.
        for (uint64_t va = s; va < e;) {
            size_t batch = static_cast<size_t>(
                std::min<uint64_t>((e - va) / kPageSize4K, kPagesPer2M));
            ssize_t bytes =
                pread(pm_fd, entries, batch * sizeof(uint64_t),
                      static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t)));
            if (bytes <= 0)
                break;
            size_t n = static_cast<size_t>(bytes) / sizeof(uint64_t);
            for (size_t i = 0; i < n; ++i) {
                uint64_t pfn = entries[i] & kPagemapPfnMask;
                if ((entries[i] & kPagemapPresentBit) != 0 && pfn != 0)
                    bind_pages(0, va + i * kPageSize4K, pfn * kPageSize4K, 1);
            }
            va += n * kPageSize4K;
        }
    }

    if (pm_fd >= 0)
        close(pm_fd);
    if (maps != nullptr)
        fclose(maps);
}

// Updates `shadow_pages_` for `[aligned_vaddr, +page_count * 4 KB)` and writes a
// `V2P_RECORD_MAP` record whenever at least one covered 4 KB page changed physical frame.
void
v2p_tracker_t::bind_pages(uint64_t timestamp_us, uint64_t aligned_vaddr,
                          uint64_t aligned_paddr, uint32_t page_count)
{
    bool changed = false;
    for (uint32_t i = 0; i < page_count; ++i) {
        uint64_t va = aligned_vaddr + static_cast<uint64_t>(i) * kPageSize4K;
        uint64_t pa = aligned_paddr + static_cast<uint64_t>(i) * kPageSize4K;
        auto it = shadow_pages_.find(va);
        if (it == shadow_pages_.end() || it->second != pa) {
            shadow_pages_[va] = pa;
            changed = true;
        }
    }
    if (changed) {
        write_record(out_fd_, timestamp_us, aligned_vaddr, aligned_paddr, page_count,
                     V2P_RECORD_MAP);
    }
}

// Closes `trace_pipe` and `/proc/kpageflags`, disables events in the isolated instance,
// removes the instance directory, and unregisters this session's kprobe.
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
    if (!anon_probe_name_.empty()) {
        write_tracefs_file(tracefs_root_ + "/kprobe_events",
                           "-:kprobes/" + anon_probe_name_ + "\n", /*append=*/true);
        anon_probe_name_.clear();
    }
    kernel_tracing_active_ = false;
}

// Stops tracefs collection, joins the reader thread to drain all buffered events, tears
// down the kernel instance, and appends the trailing `V2P_RECORD_END` sentinel.
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

    cleanup_kernel_tracing();

    // Write the end-of-stream sentinel before closing `raw/v2p.bin`.
    if (out_fd_ >= 0) {
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
// anon_new_<pid>: ...`), converts its `page=` pointer to a physical frame via
// `vmemmap_base_`, and binds the newly installed 4 KB page or 2 MB THP folio.
void
v2p_tracker_t::process_trace_line(const std::string &line)
{
    size_t ev = line.find(": " + anon_probe_name_ + ":");
    if (ev == std::string::npos)
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

    // Extract `page=` and `vaddr=` and translate `page_ptr` to its physical frame number.
    uint64_t page_ptr = 0, vaddr = 0;
    if (!extract_u64_field(line, "page=", page_ptr, 16, ev) || page_ptr < vmemmap_base_ ||
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
               is_2m ? kPagesPer2M : 1);
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
