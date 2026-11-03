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

#if defined(LINUX) && defined(X64)
#    include <dirent.h>
#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/ioctl.h>
#    include <sys/stat.h>
#    include <sys/syscall.h>
#    include <sys/time.h>
#    include <sys/utsname.h>
#    include <time.h>
#    include <unistd.h>
#    include <utility>
#    include <vector>

namespace dynamorio {
namespace drmemtrace {
namespace {

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPageSize2M = 2 * 1024 * 1024;
constexpr uint32_t kPagesPer2M = 512;
constexpr uint64_t kStructPageShift = 6;
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

// Linux 6.7+ PAGEMAP_SCAN ioctl definitions (for sysroots with older headers)
// and /proc/kpageflags bit masks for hugepage page-table verification.
struct v2p_page_region_t {
    uint64_t start;
    uint64_t end;
    uint64_t categories;
};

struct v2p_pm_scan_arg_t {
    uint64_t size;
    uint64_t flags;
    uint64_t start;
    uint64_t end;
    uint64_t walk_end;
    uint64_t vec;
    uint64_t vec_len;
    uint64_t max_pages;
    uint64_t category_inverted;
    uint64_t category_mask;
    uint64_t category_anyof_mask;
    uint64_t return_mask;
};

constexpr unsigned long kPagemapScanIoctl = _IOWR('f', 16, v2p_pm_scan_arg_t);
constexpr uint64_t kPageIsPresent = 1ULL << 3;
constexpr uint64_t kPageIsHuge = 1ULL << 6;
constexpr uint64_t kKpfHuge = 1ULL << 17;
constexpr uint64_t kKpfThp = 1ULL << 22;
constexpr uint64_t kKpfZeroPage = 1ULL << 24;

// Writes `content` to a tracefs control file at `path`.
bool
write_trace_file(const std::string &path, const std::string &content, bool append = false)
{
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC | (append ? O_APPEND : O_TRUNC));
    if (fd < 0)
        return false;
    ssize_t written = write(fd, content.data(), content.size());
    close(fd);
    return written == static_cast<ssize_t>(content.size());
}

// Writes a 32-byte `v2p_record_t` entry to `out_fd`, retrying on `EINTR` or
// partial writes so fixed-stride binary records are never misaligned.
void
write_record(int out_fd, uint64_t ts, uint64_t va, uint64_t pa, uint32_t pages,
             uint32_t type)
{
    if (out_fd < 0)
        return;
    v2p_record_t rec = { ts, va, pa, pages, type };
    const char *ptr = reinterpret_cast<const char *>(&rec);
    size_t remaining = sizeof(rec);
    while (remaining > 0) {
        ssize_t n = write(out_fd, ptr, remaining);
        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        ptr += n;
        remaining -= static_cast<size_t>(n);
    }
}

// Parses a hex (`0x...`) or decimal integer value following `key` in `line`
// starting at `start_pos`.
bool
extract_u64_field(const std::string &line, const char *key, uint64_t &out, int base = 0,
                  size_t start_pos = 0)
{
    size_t pos = line.find(key, start_pos);
    if (pos == std::string::npos)
        return false;
    const char *start = line.c_str() + pos + strlen(key);
    char *end = nullptr;
    errno = 0;
    unsigned long long val = strtoull(start, &end, base);
    if (end == start || errno != 0)
        return false;
    out = static_cast<uint64_t>(val);
    return true;
}

// Finds background kernel threads (`khugepaged` and `kcompactd*`) that
// asynchronously collapse or migrate pages on behalf of user `mm_struct`s.
std::vector<pid_t>
find_mm_helper_pids()
{
    std::vector<pid_t> pids;
    DIR *proc = opendir("/proc");
    if (proc == nullptr)
        return pids;
    for (struct dirent *ent = nullptr; (ent = readdir(proc)) != nullptr;) {
        char *endptr = nullptr;
        long val = strtol(ent->d_name, &endptr, 10);
        if (endptr == ent->d_name || *endptr != '\0' || val <= 0)
            continue;
        pid_t candidate = static_cast<pid_t>(val);
        char comm_path[64], buf[64] = {};
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", candidate);
        FILE *f = fopen(comm_path, "r");
        if (f == nullptr)
            continue;
        if (fgets(buf, sizeof(buf), f) != nullptr &&
            (strncmp(buf, "khugepaged", 10) == 0 || strncmp(buf, "kcompactd", 9) == 0)) {
            pids.push_back(candidate);
        }
        fclose(f);
    }
    closedir(proc);
    return pids;
}

} // namespace

// Enables an isolated tracefs instance for `pid`, snapshots pre-existing page
// mappings from `/proc/<pid>/pagemap`, and starts the background reader thread.
bool
v2p_tracker_t::start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb)
{
    if (running_.load(std::memory_order_acquire))
        stop();
    target_pid_ = pid;
    reader_tid_ = 0;
    vmemmap_base_ = 0;
    tracked_mm_ = 0;
    pipe_remainder_.clear();
    pending_by_tid_.clear();
    active_paddr_by_vpage_.clear();
    auto fail = [this]() {
        cleanup_kernel_tracing();
        if (out_fd_ >= 0) {
            close(out_fd_);
            out_fd_ = -1;
        }
        return false;
    };

    // Verify the running kernel is at least Linux version 5.11.
    struct utsname uts = {};
    if (uname(&uts) != 0 ||
        sscanf(uts.release, "%d.%d", &kernel_major_, &kernel_minor_) != 2 ||
        !kernel_at_least(5, 11)) {
        return false;
    }

    // Open the binary output file for v2p records.
    out_fd_ = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out_fd_ < 0)
        return false;

    // Create an isolated tracefs instance directory for this process.
    struct stat st = {};
    if (stat("/sys/kernel/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/tracing";
    else if (stat("/sys/kernel/debug/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/debug/tracing";
    else
        return fail();
    instance_dir_ = tracefs_root_ + "/instances/drmemtrace_v2p_" + std::to_string(pid);
    kernel_tracing_active_ = true;
    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST)
        return fail();

    // Select CLOCK_TAI on Linux 5.19+ and CLOCK_MONOTONIC on Linux 5.11-5.18.
    const bool use_tai = kernel_at_least(5, 19);
    const clockid_t trace_clk_id = use_tai ? CLOCK_TAI : CLOCK_MONOTONIC;
    if (!write_trace_file(instance_dir_ + "/trace_clock", use_tai ? "tai" : "mono"))
        return fail();

    // Include `khugepaged` and `kcompactd*` in the instance PID filter so
    // asynchronous THP collapses and physical page migrations are captured.
    std::string pid_list = std::to_string(pid);
    for (pid_t helper_pid : find_mm_helper_pids())
        pid_list += " " + std::to_string(helper_pid);

    // Configure the tracefs instance buffer size, PID filter, and raw pointer printing.
    if ((buffer_size_kb > 0 &&
         !write_trace_file(instance_dir_ + "/buffer_size_kb",
                           std::to_string(buffer_size_kb))) ||
        !write_trace_file(instance_dir_ + "/set_event_pid", pid_list) ||
        !write_trace_file(instance_dir_ + "/options/event-fork", "1")) {
        return fail();
    }
    write_trace_file(instance_dir_ + "/options/hash-ptr", "0");

    // Compute the offset from the selected kernel clock to DynamoRIO UTC microseconds.
    struct timespec clk_ts = {};
    struct timeval real_tv = {};
    if (clock_gettime(trace_clk_id, &clk_ts) != 0 || gettimeofday(&real_tv, nullptr) != 0)
        return fail();
    clock_offset_us_ =
        (real_tv.tv_sec * 1000000ULL + real_tv.tv_usec + kDrUtcToEpochMicros) -
        (clk_ts.tv_sec * 1000000ULL + clk_ts.tv_nsec / 1000ULL);

    // Select the `vma->vm_mm` offset explicitly by kernel version (+16 on Linux 6.1+
    // with maple_tree VMAs; +64 on Linux 5.11-6.0 with rb_tree VMAs).
    const std::string vm_mm_off = kernel_at_least(6, 1) ? "+16" : "+64";

    // Register kprobes for page faults, unmaps, and page migrations in tracefs.
    const std::string sfx = "_" + std::to_string(pid);
    std::string probes = "p:kprobes/fault_in" + sfx + " handle_mm_fault mm=" + vm_mm_off +
        "($arg1):x64 vaddr=$arg2:x64\n"
        "r:kprobes/fault_out" +
        sfx +
        " handle_mm_fault ret=$retval:x64\n"
        "p:kprobes/unmap" +
        sfx +
        " unmap_page_range mm=+0($arg1):x64 start=$arg3:x64 end=$arg4:x64\n"
        "p:kprobes/mig_ptes" +
        sfx + " remove_migration_ptes src=$arg1:x64 dst=$arg2:x64\n";
    if (!write_trace_file(tracefs_root_ + "/kprobe_events", probes, /*append=*/true))
        return fail();

    // Open trace_pipe for non-blocking reads by the background reader thread.
    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0)
        return fail();

    // Enable the tracefs instance tracepoints and kprobes.
    if (!write_trace_file(instance_dir_ + "/events/kmem/mm_page_alloc/enable", "1") ||
        !write_trace_file(
            instance_dir_ + "/events/huge_memory/mm_khugepaged_scan_pmd/enable", "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/fault_in" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/fault_out" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/unmap" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/mig_ptes" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/tracing_on", "1")) {
        return fail();
    }

    // Snapshot already-resident pages from /proc/<pid>/pagemap at timestamp 0.
    snapshot_initial_pagemap();

    // Spawn the background client thread to drain trace_pipe.
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

// Disables tracepoints/kprobes, removes the tracefs instance, and unregisters kprobes.
void
v2p_tracker_t::cleanup_kernel_tracing()
{
    if (trace_pipe_fd_ >= 0) {
        close(trace_pipe_fd_);
        trace_pipe_fd_ = -1;
    }
    if (!kernel_tracing_active_)
        return;
    const std::string sfx = "_" + std::to_string(target_pid_);
    write_trace_file(instance_dir_ + "/events/kmem/mm_page_alloc/enable", "0");
    write_trace_file(instance_dir_ + "/events/huge_memory/mm_khugepaged_scan_pmd/enable",
                     "0");
    write_trace_file(instance_dir_ + "/events/kprobes/fault_in" + sfx + "/enable", "0");
    write_trace_file(instance_dir_ + "/events/kprobes/fault_out" + sfx + "/enable", "0");
    write_trace_file(instance_dir_ + "/events/kprobes/unmap" + sfx + "/enable", "0");
    write_trace_file(instance_dir_ + "/events/kprobes/mig_ptes" + sfx + "/enable", "0");
    rmdir(instance_dir_.c_str());

    // Unregister each kprobe individually so one error cannot skip the rest.
    for (const char *name : { "fault_in", "fault_out", "unmap", "mig_ptes" }) {
        write_trace_file(tracefs_root_ + "/kprobe_events",
                         std::string("-:kprobes/") + name + sfx + "\n",
                         /*append=*/true);
    }
    kernel_tracing_active_ = false;
}

// Stops kernel tracing, waits for the reader thread to drain, and cleans up.
void
v2p_tracker_t::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;

    // Stop new events from entering the kernel ring buffer.
    if (kernel_tracing_active_)
        write_trace_file(instance_dir_ + "/tracing_on", "0");

    // Signal the reader thread to finish draining buffered lines.
    stop_requested_.store(true, std::memory_order_release);
    if (thread_running_.load(std::memory_order_acquire)) {
        while (!thread_exited_.load(std::memory_order_acquire))
            dr_sleep(1);
    } else if (kernel_tracing_active_) {
        drain_pipe();
    }

    // Check per-CPU tracefs stats for any events dropped due to ring-buffer overflow.
    uint64_t overflow_events = kernel_tracing_active_ ? read_total_overflows() : 0;
    if (overflow_events > 0) {
        dr_fprintf(STDERR,
                   "WARNING: drmemtrace v2p_tracker detected %" PRIu64
                   " dropped tracefs events due to buffer overflow (increase "
                   "-v2p_buffer_size_kb)\n",
                   overflow_events);
    }

    cleanup_kernel_tracing();

    // Record any overflow summary and the end-of-stream sentinel before closing.
    if (out_fd_ >= 0) {
        if (overflow_events > 0) {
            write_record(out_fd_, 0, 0, 0, static_cast<uint32_t>(overflow_events),
                         V2P_RECORD_OVERFLOW);
        }
        write_record(out_fd_, 0, 0, 0, 0, V2P_RECORD_END);
        close(out_fd_);
        out_fd_ = -1;
    }
}

// Scans `/proc/<pid>/maps` and `/proc/<pid>/pagemap` in 2 MB-aligned batches,
// coalescing a 2 MB chunk into a single 512-page `V2P_RECORD_MAP` entry only when
// the page table actually maps that virtual range as a hugepage.
void
v2p_tracker_t::snapshot_initial_pagemap()
{
    char maps_path[64], pagemap_path[64], line[512];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", target_pid_);
    snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", target_pid_);
    FILE *maps_file = fopen(maps_path, "r");
    int pagemap_fd = open(pagemap_path, O_RDONLY | O_CLOEXEC);
    if (maps_file == nullptr || pagemap_fd < 0) {
        if (pagemap_fd >= 0)
            close(pagemap_fd);
        if (maps_file != nullptr)
            fclose(maps_file);
        return;
    }

    int kpageflags_fd = -2;
    bool smaps_loaded = false;
    std::vector<std::pair<uint64_t, uint64_t>> smaps_huge_vmas;

    auto is_hugepage_in_pagetable = [&](uint64_t va, uint64_t base_pfn) {
        if (kernel_at_least(6, 7)) {
            // On Linux 6.7+, query the page table directly via PAGEMAP_SCAN.
            v2p_page_region_t region = {};
            v2p_pm_scan_arg_t arg = {};
            arg.size = sizeof(arg);
            arg.start = va;
            arg.end = va + kPageSize2M;
            arg.vec = reinterpret_cast<uintptr_t>(&region);
            arg.vec_len = 1;
            arg.category_mask = kPageIsPresent | kPageIsHuge;
            arg.return_mask = kPageIsPresent | kPageIsHuge;
            int rc = ioctl(pagemap_fd, kPagemapScanIoctl, &arg);
            return rc == 1 && region.start == va && region.end == va + kPageSize2M;
        } else {
            // On Linux 5.11-6.6, check /proc/kpageflags for a compound THP or hugetlb
            // page.
            if (kpageflags_fd == -2)
                kpageflags_fd = open("/proc/kpageflags", O_RDONLY | O_CLOEXEC);
            uint64_t kpf = 0;
            off_t kpf_off = static_cast<off_t>(base_pfn * sizeof(uint64_t));
            if (kpageflags_fd < 0 ||
                pread(kpageflags_fd, &kpf, sizeof(kpf), kpf_off) != sizeof(kpf) ||
                (kpf & (kKpfThp | kKpfHuge)) == 0) {
                return false;
            }
            if ((kpf & kKpfZeroPage) != 0)
                return true;

            // Cross-check /proc/<pid>/smaps to confirm the VMA maps PMD/hugetlb entries.
            if (!smaps_loaded) {
                smaps_loaded = true;
                char smaps_path[64], sline[512];
                snprintf(smaps_path, sizeof(smaps_path), "/proc/%d/smaps", target_pid_);
                FILE *sf = fopen(smaps_path, "r");
                if (sf != nullptr) {
                    uint64_t v_start = 0, v_end = 0;
                    bool v_recorded = false;
                    while (fgets(sline, sizeof(sline), sf) != nullptr) {
                        if (strchr(sline, '\n') == nullptr) {
                            for (int c = fgetc(sf); c != EOF && c != '\n'; c = fgetc(sf))
                                ;
                        }
                        uint64_t s = 0, e = 0, kb = 0;
                        if (strchr(sline, '-') != nullptr &&
                            sscanf(sline, "%" SCNx64 "-%" SCNx64, &s, &e) == 2) {
                            v_start = s;
                            v_end = e;
                            v_recorded = false;
                        } else if (!v_recorded &&
                                   (strncmp(sline, "AnonHugePages:", 14) == 0 ||
                                    strncmp(sline, "ShmemPmdMapped:", 15) == 0 ||
                                    strncmp(sline, "FilePmdMapped:", 14) == 0 ||
                                    strncmp(sline, "Shared_Hugetlb:", 15) == 0 ||
                                    strncmp(sline, "Private_Hugetlb:", 16) == 0) &&
                                   extract_u64_field(sline, ":", kb, 10) && kb > 0) {
                            smaps_huge_vmas.push_back({ v_start, v_end });
                            v_recorded = true;
                        }
                    }
                    fclose(sf);
                }
            }
            for (const auto &vma : smaps_huge_vmas) {
                if (vma.first <= va && va + kPageSize2M <= vma.second)
                    return true;
            }
            return false;
        }
    };

    // Read pagemap entries aligned to 2 MB boundaries within each VMA.
    uint64_t entries[kPagesPer2M];
    while (fgets(line, sizeof(line), maps_file) != nullptr) {
        if (strchr(line, '\n') == nullptr) {
            for (int c = fgetc(maps_file); c != EOF && c != '\n'; c = fgetc(maps_file))
                ;
        }
        uint64_t start_va = 0, end_va = 0;
        if (sscanf(line, "%" SCNx64 "-%" SCNx64, &start_va, &end_va) != 2 ||
            start_va >= 0xffff800000000000ULL || end_va <= start_va) {
            continue;
        }
        for (uint64_t va = start_va; va < end_va;) {
            uint64_t next_2m = (va + kPageSize2M) & ~(kPageSize2M - 1);
            uint64_t batch_end = next_2m < end_va ? next_2m : end_va;
            uint64_t batch_pages = (batch_end - va) / kPageSize4K;
            off_t off = static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t));
            ssize_t bytes =
                pread(pagemap_fd, entries, batch_pages * sizeof(uint64_t), off);
            if (bytes <= 0)
                break;
            size_t read_pages = static_cast<size_t>(bytes) / sizeof(uint64_t);

            // Check whether this batch is a contiguous 2 MB-aligned hugepage.
            uint64_t base_pfn = entries[0] & kPagemapPfnMask;
            bool is_2m_contig = read_pages == kPagesPer2M &&
                (va & (kPageSize2M - 1)) == 0 && (entries[0] & kPagemapPresentBit) != 0 &&
                base_pfn != 0 && (base_pfn & (kPagesPer2M - 1)) == 0;
            for (size_t i = 1; is_2m_contig && i < kPagesPer2M; ++i) {
                is_2m_contig = (entries[i] & kPagemapPresentBit) != 0 &&
                    (entries[i] & kPagemapPfnMask) == base_pfn + i;
            }
            if (is_2m_contig && is_hugepage_in_pagetable(va, base_pfn)) {
                record_page_map(0, va, base_pfn * kPageSize4K, kPagesPer2M);
                va += kPageSize2M;
                continue;
            }

            // Otherwise, emit individual 4 KB records for each resident page.
            for (size_t i = 0; i < read_pages; ++i) {
                uint64_t pfn = entries[i] & kPagemapPfnMask;
                if ((entries[i] & kPagemapPresentBit) != 0 && pfn != 0)
                    record_page_map(0, va + i * kPageSize4K, pfn * kPageSize4K, 1);
            }
            va += read_pages * kPageSize4K;
        }
    }

    if (kpageflags_fd >= 0)
        close(kpageflags_fd);
    close(pagemap_fd);
    fclose(maps_file);
}

// Returns whether `mm` belongs to `target_pid_`, seeding `tracked_mm_` on the
// first event observed from a thread inside `target_pid_`'s thread group.
bool
v2p_tracker_t::is_tracked_mm(uint64_t mm, pid_t tid)
{
    if (mm == 0)
        return false;
    if (tracked_mm_ == 0) {
        if (tid != target_pid_) {
            char task_path[64];
            snprintf(task_path, sizeof(task_path), "/proc/%d/task/%d", target_pid_, tid);
            if (access(task_path, F_OK) != 0)
                return false;
        }
        tracked_mm_ = mm;
    }
    return mm == tracked_mm_;
}

// Updates `active_paddr_by_vpage_` for `page_count` contiguous 4 KB pages and
// writes a `V2P_RECORD_MAP` entry to `out_fd_`.
void
v2p_tracker_t::record_page_map(uint64_t timestamp_us, uint64_t vaddr, uint64_t paddr,
                               uint32_t page_count)
{
    for (uint32_t i = 0; i < page_count; ++i) {
        active_paddr_by_vpage_[vaddr + static_cast<uint64_t>(i) * kPageSize4K] =
            paddr + static_cast<uint64_t>(i) * kPageSize4K;
    }
    write_record(out_fd_, timestamp_us, vaddr, paddr, page_count, V2P_RECORD_MAP);
}

// Sums dropped event counts across `per_cpu/cpu*/stats` in the tracefs instance.
uint64_t
v2p_tracker_t::read_total_overflows() const
{
    std::string per_cpu_dir = instance_dir_ + "/per_cpu";
    DIR *dir = opendir(per_cpu_dir.c_str());
    if (dir == nullptr)
        return 0;
    uint64_t total_overflow = 0;
    for (struct dirent *ent = nullptr; (ent = readdir(dir)) != nullptr;) {
        if (strncmp(ent->d_name, "cpu", 3) != 0)
            continue;
        std::string stats_path = per_cpu_dir + "/" + ent->d_name + "/stats";
        FILE *f = fopen(stats_path.c_str(), "r");
        if (f == nullptr)
            continue;
        char line[256];
        while (fgets(line, sizeof(line), f) != nullptr) {
            uint64_t val = 0;
            if (strncmp(line, "overrun:", 8) == 0 &&
                extract_u64_field(line, "overrun:", val, 10)) {
                total_overflow += val;
                break;
            }
        }
        fclose(f);
    }
    closedir(dir);
    return total_overflow;
}

// Reads all currently available bytes from non-blocking `trace_pipe_fd_` and
// dispatches complete newline-terminated lines to `process_trace_line()`.
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

// Polls `trace_pipe_fd_` and dispatches complete lines until `stop()` is called.
void
v2p_tracker_t::reader_thread_entry(void *arg)
{
    auto *self = static_cast<v2p_tracker_t *>(arg);
    dr_client_thread_set_suspendable(false);
    self->reader_tid_ = static_cast<pid_t>(syscall(SYS_gettid));

    // Poll trace_pipe until stop() requests termination.
    while (!self->stop_requested_.load(std::memory_order_acquire)) {
        struct pollfd pfd = { self->trace_pipe_fd_, POLLIN, 0 };
        if (poll(&pfd, 1, 10) <= 0)
            continue;
        self->drain_pipe();
    }

    // Drain any final events buffered before tracing_on was set to 0.
    self->drain_pipe();
    self->thread_exited_.store(true, std::memory_order_release);
}

// Parses a single trace_pipe event line and emits `v2p_record_t` updates for
// page faults, unmaps, background THP collapses, and page migrations.
void
v2p_tracker_t::process_trace_line(const std::string &line)
{
    // Parse the thread ID from "<comm>-<tid> [<cpu>]"; ignore our own reader thread.
    size_t bracket = line.find('[');
    size_t dash =
        (bracket != std::string::npos) ? line.rfind('-', bracket) : std::string::npos;
    if (dash == std::string::npos)
        return;
    pid_t tid = static_cast<pid_t>(strtol(line.c_str() + dash + 1, nullptr, 10));
    if (tid <= 0 || (reader_tid_ != 0 && tid == reader_tid_))
        return;

    // Extract the "<secs>.<usecs>:" kernel clock timestamp from the trace header.
    size_t colon = line.find(": ", bracket);
    size_t ts_start =
        (colon != std::string::npos) ? line.rfind(' ', colon) : std::string::npos;
    size_t dot = (ts_start != std::string::npos) ? line.find('.', ts_start + 1)
                                                 : std::string::npos;
    if (dot == std::string::npos || dot >= colon)
        return;

    // Convert the kernel clock timestamp to DynamoRIO UTC microseconds.
    uint64_t secs = strtoull(line.c_str() + ts_start + 1, nullptr, 10);
    uint64_t usecs = strtoull(line.c_str() + dot + 1, nullptr, 10);
    uint64_t timestamp_us = secs * 1000000ULL + usecs + clock_offset_us_;
    const size_t event_pos = colon + 2;

    // Fault entry: filter to `tracked_mm_` and record the faulting virtual address.
    if (line.find("fault_in_", event_pos) != std::string::npos) {
        uint64_t mm = 0, vaddr = 0;
        if (!extract_u64_field(line, "mm=", mm, 16, event_pos) || !is_tracked_mm(mm, tid))
            return;
        if (extract_u64_field(line, "vaddr=", vaddr, 0, event_pos))
            pending_by_tid_[tid] = { vaddr, 0, 0, false };
        return;
    }

    // Page allocation: calibrate `vmemmap_base_` and record fault or khugepaged PFNs.
    if (line.find("mm_page_alloc:", event_pos) != std::string::npos) {
        uint64_t page_ptr = 0, pfn = 0, order = 0;
        bool has_pfn = extract_u64_field(line, "pfn=", pfn, 0, event_pos);
        if (vmemmap_base_ == 0 && has_pfn && pfn != ~0ULL &&
            extract_u64_field(line, "page=", page_ptr, 16, event_pos) &&
            page_ptr > (pfn << kStructPageShift)) {
            vmemmap_base_ = page_ptr - (pfn << kStructPageShift);
        }
        auto it = pending_by_tid_.find(tid);
        if (it != pending_by_tid_.end() && has_pfn &&
            extract_u64_field(line, "order=", order, 10, event_pos)) {
            // Keep `order >= current` so a deposited order-0 PTE table page does not
            // overwrite an order-9 THP folio allocated earlier in the same fault.
            if (!it->second.has_alloc || order >= it->second.order) {
                it->second.pfn = pfn;
                it->second.order = order;
                it->second.has_alloc = true;
            }
        } else if (has_pfn && extract_u64_field(line, "order=", order, 10, event_pos) &&
                   order == 9) {
            // Stash `khugepaged`'s order-9 allocation until `mm_khugepaged_scan_pmd`.
            pending_by_tid_[tid] = { 0, pfn, order, true };
        }
        return;
    }

    // Fault exit: emit V2P_RECORD_MAP for the newly mapped page or hugepage.
    if (line.find("fault_out_", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it != pending_by_tid_.end()) {
            pending_op_t op = it->second;
            pending_by_tid_.erase(it);
            if (op.has_alloc && op.pfn != 0 && op.pfn != ~0ULL) {
                uint32_t page_count = 1U << op.order;
                uint64_t length = static_cast<uint64_t>(page_count) * kPageSize4K;
                uint64_t aligned_vaddr = op.vaddr & ~(length - 1);
                record_page_map(timestamp_us, aligned_vaddr, op.pfn * kPageSize4K,
                                page_count);
            }
        }
        return;
    }

    // Page migration (`kcompactd*`, NUMA balancing, compaction): remap any active
    // virtual pages backed by `src` to the destination physical frame `dst`.
    if (line.find("mig_ptes_", event_pos) != std::string::npos) {
        uint64_t src = 0, dst = 0;
        if (vmemmap_base_ != 0 && extract_u64_field(line, "src=", src, 16, event_pos) &&
            src > vmemmap_base_ && extract_u64_field(line, "dst=", dst, 16, event_pos) &&
            dst > vmemmap_base_ && dst != src) {
            uint64_t src_paddr =
                ((src - vmemmap_base_) >> kStructPageShift) * kPageSize4K;
            uint64_t dst_paddr =
                ((dst - vmemmap_base_) >> kStructPageShift) * kPageSize4K;
            std::vector<uint64_t> migrated_vpages;
            for (const auto &kv : active_paddr_by_vpage_) {
                if (kv.second == src_paddr)
                    migrated_vpages.push_back(kv.first);
            }
            for (uint64_t vpage : migrated_vpages)
                record_page_map(timestamp_us, vpage, dst_paddr, 1);
        }
        return;
    }

    // Page-table unmap: remove torn-down virtual pages from `active_paddr_by_vpage_`
    // and emit V2P_RECORD_UNMAP.
    if (line.find("unmap_", event_pos) != std::string::npos) {
        uint64_t mm = 0, start = 0, end = 0;
        if (!extract_u64_field(line, "mm=", mm, 16, event_pos) || !is_tracked_mm(mm, tid))
            return;
        if (extract_u64_field(line, "start=", start, 0, event_pos) &&
            extract_u64_field(line, "end=", end, 0, event_pos) && end > start) {
            uint64_t aligned_start = start & ~(kPageSize4K - 1);
            uint64_t aligned_end = (end + kPageSize4K - 1) & ~(kPageSize4K - 1);
            for (auto it = active_paddr_by_vpage_.begin();
                 it != active_paddr_by_vpage_.end();) {
                if (it->first >= aligned_start && it->first < aligned_end)
                    it = active_paddr_by_vpage_.erase(it);
                else
                    ++it;
            }
            uint32_t pages =
                static_cast<uint32_t>((aligned_end - aligned_start) / kPageSize4K);
            write_record(out_fd_, timestamp_us, aligned_start, 0, pages,
                         V2P_RECORD_UNMAP);
        }
        return;
    }

    // Asynchronous `khugepaged` 2 MB collapse: always clear any stashed order-9 PFN
    // for `tid` (since `collapse_huge_page` either consumed or freed it before
    // emitting `mm_khugepaged_scan_pmd`), then record the collapse if it succeeded
    // on `tracked_mm_`.
    if (line.find("mm_khugepaged_scan_pmd:", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it == pending_by_tid_.end())
            return;
        pending_op_t op = it->second;
        pending_by_tid_.erase(it);
        uint64_t mm = 0, scan_pfn = 0, page_ptr = 0;
        if (!extract_u64_field(line, "mm=", mm, 16, event_pos) || tracked_mm_ == 0 ||
            mm != tracked_mm_ ||
            line.find("status=succeeded", event_pos) == std::string::npos) {
            return;
        }
        if (!op.has_alloc || op.order != 9 || op.pfn == 0 || op.pfn == ~0ULL)
            return;
        if (!extract_u64_field(line, "scan_pfn=", scan_pfn, 16, event_pos) ||
            scan_pfn == 0 || scan_pfn == ~0ULL) {
            if (vmemmap_base_ != 0 &&
                extract_u64_field(line, "page=", page_ptr, 16, event_pos) &&
                page_ptr > vmemmap_base_) {
                scan_pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
            }
        }
        if (scan_pfn == 0)
            return;
        uint64_t scan_paddr = scan_pfn * kPageSize4K;
        for (const auto &kv : active_paddr_by_vpage_) {
            if (kv.second == scan_paddr) {
                uint64_t collapsed_vaddr = kv.first & ~(kPageSize2M - 1);
                record_page_map(timestamp_us, collapsed_vaddr, op.pfn * kPageSize4K,
                                kPagesPer2M);
                break;
            }
        }
    }
}

} // namespace drmemtrace
} // namespace dynamorio
#endif // LINUX
