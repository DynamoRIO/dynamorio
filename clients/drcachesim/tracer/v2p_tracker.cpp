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
#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/stat.h>
#    include <sys/syscall.h>
#    include <sys/time.h>
#    include <sys/utsname.h>
#    include <time.h>
#    include <unistd.h>

namespace dynamorio {
namespace drmemtrace {
namespace {

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

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
    pipe_remainder_.clear();
    pending_by_tid_.clear();
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

    // Configure the tracefs instance buffer size and PID filter.
    if ((buffer_size_kb > 0 &&
         !write_trace_file(instance_dir_ + "/buffer_size_kb",
                           std::to_string(buffer_size_kb))) ||
        !write_trace_file(instance_dir_ + "/set_event_pid", std::to_string(pid)) ||
        !write_trace_file(instance_dir_ + "/options/event-fork", "1")) {
        return fail();
    }

    // Compute the offset from the selected kernel clock to DynamoRIO UTC microseconds.
    struct timespec clk_ts = {};
    struct timeval real_tv = {};
    if (clock_gettime(trace_clk_id, &clk_ts) != 0 || gettimeofday(&real_tv, nullptr) != 0)
        return fail();
    clock_offset_us_ =
        (real_tv.tv_sec * 1000000ULL + real_tv.tv_usec + kDrUtcToEpochMicros) -
        (clk_ts.tv_sec * 1000000ULL + clk_ts.tv_nsec / 1000ULL);

    // Register entry/return kprobes on handle_mm_fault in tracefs.
    const std::string sfx = "_" + std::to_string(pid);
    std::string probes = "p:kprobes/fault_in" + sfx +
        " handle_mm_fault vaddr=$arg2:x64\n"
        "r:kprobes/fault_out" +
        sfx + " handle_mm_fault ret=$retval:x64\n";
    if (!write_trace_file(tracefs_root_ + "/kprobe_events", probes, /*append=*/true))
        return fail();

    // Open trace_pipe for non-blocking reads by the background reader thread.
    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0)
        return fail();

    // Enable the tracefs instance events (mm_page_alloc, fault_in, fault_out).
    if (!write_trace_file(instance_dir_ + "/events/kmem/mm_page_alloc/enable", "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/fault_in" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/events/kprobes/fault_out" + sfx + "/enable",
                          "1") ||
        !write_trace_file(instance_dir_ + "/tracing_on", "1")) {
        return fail();
    }

    // Snapshot already-resident pages from /proc/<pid>/pagemap at timestamp 0.
    char maps_path[64], pagemap_path[64], line[512];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", pid);
    FILE *maps_file = fopen(maps_path, "r");
    int pagemap_fd = open(pagemap_path, O_RDONLY | O_CLOEXEC);
    if (maps_file != nullptr && pagemap_fd >= 0) {
        while (fgets(line, sizeof(line), maps_file) != nullptr) {
            uint64_t start_va = 0, end_va = 0, entry = 0;
            if (sscanf(line, "%" SCNx64 "-%" SCNx64, &start_va, &end_va) != 2 ||
                start_va >= 0xffff800000000000ULL) {
                continue;
            }
            for (uint64_t va = start_va; va < end_va; va += kPageSize4K) {
                off_t off = static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t));
                if (pread(pagemap_fd, &entry, sizeof(entry), off) == sizeof(entry) &&
                    (entry & kPagemapPresentBit) != 0 && (entry & kPagemapPfnMask) != 0) {
                    write_record(out_fd_, 0, va, (entry & kPagemapPfnMask) * kPageSize4K,
                                 1, V2P_RECORD_MAP);
                }
            }
        }
    }
    if (pagemap_fd >= 0)
        close(pagemap_fd);
    if (maps_file != nullptr)
        fclose(maps_file);

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
    write_trace_file(instance_dir_ + "/events/kprobes/fault_in" + sfx + "/enable", "0");
    write_trace_file(instance_dir_ + "/events/kprobes/fault_out" + sfx + "/enable", "0");
    rmdir(instance_dir_.c_str());

    // Unregister each kprobe individually so one error cannot skip the rest.
    for (const char *name : { "fault_in", "fault_out" }) {
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

    cleanup_kernel_tracing();

    // Write the end-of-stream sentinel before closing the output file.
    if (out_fd_ >= 0) {
        write_record(out_fd_, 0, 0, 0, 0, V2P_RECORD_END);
        close(out_fd_);
        out_fd_ = -1;
    }
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

// Parses a single trace_pipe event line and emits a V2P_RECORD_MAP entry when a
// page fault (`fault_in` -> `mm_page_alloc` -> `fault_out`) completes.
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

    // Fault entry: record the faulting virtual address for `tid`.
    if (line.find("fault_in_", event_pos) != std::string::npos) {
        uint64_t vaddr = 0;
        if (extract_u64_field(line, "vaddr=", vaddr, 0, event_pos))
            pending_by_tid_[tid] = { vaddr, 0, 0, false };
        return;
    }

    // Page allocation inside fault: capture PFN and order (order=0 for 4 KB,
    // order=9 for 2 MB THP; `order >= current` preserves order-9 THP folios).
    if (line.find("mm_page_alloc:", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        uint64_t pfn = 0, order = 0;
        if (it != pending_by_tid_.end() &&
            extract_u64_field(line, "pfn=", pfn, 0, event_pos) &&
            extract_u64_field(line, "order=", order, 10, event_pos) &&
            (!it->second.has_alloc || order >= it->second.order)) {
            it->second.pfn = pfn;
            it->second.order = order;
            it->second.has_alloc = true;
        }
        return;
    }

    // Fault exit: emit V2P_RECORD_MAP for the newly mapped page or hugepage.
    if (line.find("fault_out_", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it != pending_by_tid_.end()) {
            pending_fault_t op = it->second;
            pending_by_tid_.erase(it);
            if (op.has_alloc && op.pfn != 0 && op.pfn != ~0ULL) {
                uint32_t page_count = 1U << op.order;
                uint64_t length = static_cast<uint64_t>(page_count) * kPageSize4K;
                uint64_t aligned_vaddr = op.vaddr & ~(length - 1);
                write_record(out_fd_, timestamp_us, aligned_vaddr, op.pfn * kPageSize4K,
                             page_count, V2P_RECORD_MAP);
            }
        }
    }
}

} // namespace drmemtrace
} // namespace dynamorio
#endif // LINUX
