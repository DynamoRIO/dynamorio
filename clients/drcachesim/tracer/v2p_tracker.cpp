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

#ifdef LINUX
#    include <dirent.h>
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
#    include <sys/types.h>
#    include <sys/utsname.h>
#    include <time.h>
#    include <unistd.h>
#endif

namespace dynamorio {
namespace drmemtrace {

#ifdef LINUX
namespace {

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPageSize2M = 2 * 1024 * 1024;
constexpr uint32_t kPagesPer2M = 512;
constexpr uint64_t kStructPageShift = 6; // sizeof(struct page) == 64 bytes.
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
// Epoch offset used by DynamoRIO's query_time_micros() in core/unix/os.c.
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

struct kprobe_spec_t {
    char type;          // 'p' for entry kprobe, 'r' for kretprobe.
    const char *name;   // Event name prefix (suffixed with `_<pid>`).
    const char *symbol; // Kernel function to attach to.
    const char *args;   // Tracefs fetchargs expression.
};

constexpr kprobe_spec_t kKprobeSpecs[] = {
    // 1. Anonymous and cold file faults (`vma->vm_mm` is at +16):
    { 'p', "fault_in", "handle_mm_fault", "mm=+16($arg1):x64 vaddr=$arg2:x64" },
    { 'r', "fault_out", "handle_mm_fault", "ret=$retval:x64" },
    // 2. Unmaps (`tlb->mm` is at +0):
    { 'p', "unmap", "unmap_page_range",
      "mm=+0($arg1):x64 start=$arg3:x64 end=$arg4:x64" },
    // 3. Page migrations and compaction (`remove_migration_ptes(src, dst, ...)`
    //    is a non-static cross-TU function in `mm/migrate.c`; `khugepaged` 2 MB
    //    collapses are captured via the static
    //    `huge_memory/mm_khugepaged_scan_pmd`
    //    tracepoint):
    { 'p', "mig_ptes", "remove_migration_ptes", "src=$arg1:x64 dst=$arg2:x64" },
    // 4. Warm page-cache and fault-around file mappings:
    { 'p', "pte_range", "set_pte_range", "page=$arg3:x64 nr=$arg4:u32 addr=$arg5:x64" },
    // 5. Virtual address relocations (`struct pagetable_move_control *pmc` on
    //    Linux >= 6.15: `pmc->old->vm_mm` via +16(+0($arg1)), `old_addr` at
    //    +16,
    //    `new_addr` at +32, `len_in` at +40; direct register arguments on
    //    Linux < 6.15):
    { 'p', "move_pt", "move_page_tables",
      "mm=+16(+0($arg1)):x64 old_addr=+16($arg1):x64 new_addr=+32($arg1):x64 "
      "len=+40($arg1):u64" },
};

// Forward declarations of low-level helpers defined at the bottom of this file.
bool
check_kernel_version(bool *use_pmc_out);
std::vector<pid_t>
find_mm_helper_pids();
uint64_t
compute_clock_to_dr_offset_us(clockid_t clk_id);
std::string
find_tracefs_root();
bool
write_trace_file(const std::string &path, const std::string &content,
                 bool append = false);
bool
extract_hex_field(const std::string &line, const char *key, uint64_t &out);
bool
extract_dec_field(const std::string &line, const char *key, uint64_t &out);

} // namespace
#endif // LINUX

/***************************************************************************
 * 1. Tracker Lifecycle (start / stop)
 */

v2p_tracker_t::~v2p_tracker_t()
{
    stop();
}

bool
v2p_tracker_t::start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb)
{
#ifdef LINUX
    if (running_.load(std::memory_order_acquire)) {
        if (pid != target_pid_) {
            // We are in a forked child process. Exclude this child (and via
            // event-fork, any threads or grandchildren it spawns) from the
            // parent's tracefs instance, and drop the inherited file descriptors
            // without stopping the parent's tracing session.
            if (!instance_dir_.empty()) {
                write_trace_file(instance_dir_ + "/set_event_notrace_pid",
                                 std::to_string(pid) + "\n", /*append=*/true);
            }
            if (trace_pipe_fd_ >= 0) {
                close(trace_pipe_fd_);
                trace_pipe_fd_ = -1;
            }
            if (out_fd_ >= 0) {
                close(out_fd_);
                out_fd_ = -1;
            }
            kernel_tracing_active_ = false;
            running_.store(false, std::memory_order_release);
        } else {
            stop();
        }
    }
    target_pid_ = pid;
    reader_tid_ = 0;
    buffer_size_kb_ = buffer_size_kb;
    dropped_events_ = 0;
    vmemmap_base_ = 0;
    tracked_mm_ = 0;
    pending_by_tid_.clear();
    active_paddr_by_vpage_.clear();
    deferred_pte_ranges_.clear();
    pipe_remainder_.clear();

    out_fd_ = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out_fd_ < 0)
        return false;

    if (!setup_kernel_tracing()) {
        cleanup_kernel_tracing();
        close(out_fd_);
        out_fd_ = -1;
        return false;
    }

    // Snapshot existing mappings after enabling tracefs probes so any concurrent
    // fault or remap is also recorded in the trace_pipe stream with timestamp >
    // 0.
    snapshot_initial_pagemap();

    stop_requested_.store(false, std::memory_order_relaxed);
    thread_running_.store(false, std::memory_order_relaxed);
    thread_exited_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);

    if (!dr_create_client_thread(&v2p_tracker_t::reader_thread_entry, this)) {
        running_.store(false, std::memory_order_release);
        cleanup_kernel_tracing();
        close(out_fd_);
        out_fd_ = -1;
        return false;
    }
    return true;
#else
    return false;
#endif
}

void
v2p_tracker_t::stop()
{
#ifdef LINUX
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;

    if (kernel_tracing_active_) {
        write_trace_file(instance_dir_ + "/tracing_on", "0");
    }
    stop_requested_.store(true, std::memory_order_release);

    if (thread_running_.load(std::memory_order_acquire)) {
        while (!thread_exited_.load(std::memory_order_acquire)) {
            dr_sleep(1);
        }
    } else if (kernel_tracing_active_) {
        // If stop() was called before the client thread began executing,
        // drain any buffered events directly on the calling thread.
        drain_pipe();
    }

    if (kernel_tracing_active_) {
        dropped_events_ = read_total_overflows();
        if (dropped_events_ > 0) {
            dr_fprintf(STDERR,
                       "WARNING: drmemtrace v2p_tracker detected %" PRIu64
                       " dropped tracefs events due to buffer overflow (increase "
                       "-v2p_buffer_size_kb)\n",
                       dropped_events_);
        }
        cleanup_kernel_tracing();
    }

    flush_deferred_pte_ranges();

    if (out_fd_ >= 0) {
        if (dropped_events_ > 0) {
            v2p_record_t overflow_rec = { };
            overflow_rec.timestamp_us = 0;
            overflow_rec.vaddr = 0;
            overflow_rec.paddr = 0;
            overflow_rec.page_count = static_cast<uint32_t>(dropped_events_);
            overflow_rec.type = V2P_RECORD_OVERFLOW;
            write_record(overflow_rec);
        }
        v2p_record_t end_rec = { };
        end_rec.timestamp_us = 0;
        end_rec.vaddr = 0;
        end_rec.paddr = 0;
        end_rec.page_count = 0;
        end_rec.type = V2P_RECORD_END;
        write_record(end_rec);
        close(out_fd_);
        out_fd_ = -1;
    }
#endif
}

/***************************************************************************
 * 2. Kernel Tracing Setup, Initial Snapshot, and Cleanup
 */

bool
v2p_tracker_t::setup_kernel_tracing()
{
#ifdef LINUX
    tracefs_root_ = find_tracefs_root();
    if (tracefs_root_.empty())
        return false;

    instance_dir_ =
        tracefs_root_ + "/instances/drmemtrace_v2p_" + std::to_string(target_pid_);
    kernel_tracing_active_ = true;

    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST)
        return false;

    if (buffer_size_kb_ > 0) {
        if (!write_trace_file(instance_dir_ + "/buffer_size_kb",
                              std::to_string(buffer_size_kb_)))
            return false;
    }

    // Prefer CLOCK_TAI because it tracks CLOCK_REALTIME (gettimeofday(), used by
    // DynamoRIO's query_time_micros()) at a constant integer-second offset
    // without leap-second steps, falling back to CLOCK_MONOTONIC if unavailable.
    if (write_trace_file(instance_dir_ + "/trace_clock", "tai")) {
        clock_offset_us_ = compute_clock_to_dr_offset_us(CLOCK_TAI);
    } else if (write_trace_file(instance_dir_ + "/trace_clock", "mono")) {
        clock_offset_us_ = compute_clock_to_dr_offset_us(CLOCK_MONOTONIC);
    } else {
        return false;
    }

    std::string pid_list = std::to_string(target_pid_);
    for (pid_t helper_pid : find_mm_helper_pids()) {
        pid_list += " " + std::to_string(helper_pid);
    }
    if (!write_trace_file(instance_dir_ + "/set_event_pid", pid_list))
        return false;
    if (!write_trace_file(instance_dir_ + "/options/event-fork", "1"))
        return false;

    bool use_pmc = false;
    if (!check_kernel_version(&use_pmc))
        return false;
    const std::string sfx = "_" + std::to_string(target_pid_);
    std::string probes;
    for (const kprobe_spec_t &spec : kKprobeSpecs) {
        const char *args = spec.args;
        if (strcmp(spec.name, "move_pt") == 0 && !use_pmc) {
            args = "mm=+16($arg1):x64 old_addr=$arg2:x64 new_addr=$arg4:x64 "
                   "len=$arg5:u64";
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "%c:kprobes/%s%s %s %s\n", spec.type, spec.name,
                 sfx.c_str(), spec.symbol, args);
        probes += buf;
    }
    if (!write_trace_file(tracefs_root_ + "/kprobe_events", probes,
                          /*append=*/true))
        return false;

    write_trace_file(instance_dir_ + "/options/hash-ptr", "0");

    if (!write_trace_file(instance_dir_ + "/events/kmem/mm_page_alloc/enable", "1"))
        return false;
    if (!write_trace_file(
            instance_dir_ + "/events/huge_memory/mm_khugepaged_scan_pmd/enable", "1"))
        return false;

    for (const kprobe_spec_t &spec : kKprobeSpecs) {
        std::string enable_path =
            instance_dir_ + "/events/kprobes/" + spec.name + sfx + "/enable";
        if (!write_trace_file(enable_path, "1"))
            return false;
    }

    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0)
        return false;

    return write_trace_file(instance_dir_ + "/tracing_on", "1");
#else
    return false;
#endif
}

void
v2p_tracker_t::snapshot_initial_pagemap()
{
#ifdef LINUX
    char maps_path[64];
    char pagemap_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", target_pid_);
    snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", target_pid_);

    FILE *maps_file = fopen(maps_path, "r");
    if (maps_file == nullptr)
        return;
    int pagemap_fd = open(pagemap_path, O_RDONLY | O_CLOEXEC);
    if (pagemap_fd < 0) {
        fclose(maps_file);
        return;
    }

    uint64_t run_vaddr = 0;
    uint64_t run_paddr = 0;
    uint32_t run_pages = 0;

    auto flush_run = [&]() {
        if (run_pages > 0) {
            v2p_record_t rec = { };
            rec.timestamp_us = 0;
            rec.vaddr = run_vaddr;
            rec.paddr = run_paddr;
            rec.page_count = run_pages;
            rec.type = V2P_RECORD_MAP;
            write_record(rec);
            run_pages = 0;
        }
    };

    char line[512];
    uint64_t entries[kPagesPer2M];
    while (fgets(line, sizeof(line), maps_file) != nullptr) {
        uint64_t start_vaddr = 0;
        uint64_t end_vaddr = 0;
        if (sscanf(line, "%" SCNx64 "-%" SCNx64, &start_vaddr, &end_vaddr) != 2)
            continue;
        if (start_vaddr >= 0xffff800000000000ULL || end_vaddr <= start_vaddr)
            continue;

        for (uint64_t vaddr = start_vaddr; vaddr < end_vaddr;) {
            uint64_t remaining_pages = (end_vaddr - vaddr) / kPageSize4K;
            uint64_t batch_pages =
                remaining_pages < kPagesPer2M ? remaining_pages : kPagesPer2M;
            off_t offset = static_cast<off_t>((vaddr / kPageSize4K) * sizeof(uint64_t));
            ssize_t bytes =
                pread(pagemap_fd, entries, batch_pages * sizeof(uint64_t), offset);
            if (bytes <= 0)
                break;
            size_t read_pages = static_cast<size_t>(bytes) / sizeof(uint64_t);
            for (size_t i = 0; i < read_pages; ++i) {
                uint64_t entry = entries[i];
                if ((entry & kPagemapPresentBit) == 0) {
                    flush_run();
                    continue;
                }
                uint64_t pfn = entry & kPagemapPfnMask;
                if (pfn == 0) {
                    flush_run();
                    continue;
                }
                uint64_t page_vaddr = vaddr + i * kPageSize4K;
                uint64_t page_paddr = pfn * kPageSize4K;
                active_paddr_by_vpage_[page_vaddr] = page_paddr;

                if (run_pages > 0 && page_vaddr == run_vaddr + run_pages * kPageSize4K &&
                    page_paddr == run_paddr + run_pages * kPageSize4K) {
                    ++run_pages;
                } else {
                    flush_run();
                    run_vaddr = page_vaddr;
                    run_paddr = page_paddr;
                    run_pages = 1;
                }
            }
            vaddr += read_pages * kPageSize4K;
        }
    }
    flush_run();

    close(pagemap_fd);
    fclose(maps_file);
#endif
}

void
v2p_tracker_t::cleanup_kernel_tracing()
{
#ifdef LINUX
    if (trace_pipe_fd_ >= 0) {
        close(trace_pipe_fd_);
        trace_pipe_fd_ = -1;
    }
    if (!instance_dir_.empty()) {
        write_trace_file(instance_dir_ + "/events/kmem/mm_page_alloc/enable", "0");
        write_trace_file(
            instance_dir_ + "/events/huge_memory/mm_khugepaged_scan_pmd/enable", "0");
        const std::string sfx = "_" + std::to_string(target_pid_);
        for (const kprobe_spec_t &spec : kKprobeSpecs) {
            write_trace_file(
                instance_dir_ + "/events/kprobes/" + spec.name + sfx + "/enable", "0");
        }
        rmdir(instance_dir_.c_str());
        for (const kprobe_spec_t &spec : kKprobeSpecs) {
            write_trace_file(tracefs_root_ + "/kprobe_events",
                             std::string("-:kprobes/") + spec.name + sfx + "\n",
                             /*append=*/true);
        }
    }
    kernel_tracing_active_ = false;
#endif
}

/***************************************************************************
 * 3. Event Processing State Machine and Record Emission
 */

void
v2p_tracker_t::process_trace_line(const std::string &line)
{
#ifdef LINUX
    // Expected format: "<comm>-<tid> [<cpu>] <flags> <sec>.<usec>: <event>: ..."
    size_t bracket = line.find('[');
    if (bracket == std::string::npos)
        return;
    size_t dash = line.rfind('-', bracket);
    if (dash == std::string::npos)
        return;
    pid_t tid = static_cast<pid_t>(strtol(line.c_str() + dash + 1, nullptr, 10));
    // Ignore events generated by our own client reader thread so heap growth
    // inside `v2p_tracker_t` does not trigger a self-amplifying fault loop.
    if (tid <= 0 || (reader_tid_ != 0 && tid == reader_tid_))
        return;

    size_t colon = line.find(": ", bracket);
    if (colon == std::string::npos)
        return;
    size_t ts_start = line.rfind(' ', colon);
    if (ts_start == std::string::npos)
        return;
    size_t dot = line.find('.', ts_start + 1);
    if (dot == std::string::npos || dot >= colon)
        return;

    uint64_t secs = strtoull(line.c_str() + ts_start + 1, nullptr, 10);
    uint64_t usecs = strtoull(line.c_str() + dot + 1, nullptr, 10);
    uint64_t timestamp_us = secs * 1000000ULL + usecs + clock_offset_us_;
    const size_t event_pos = colon + 2;

    if (line.find("fault_in_", event_pos) != std::string::npos) {
        uint64_t mm = 0;
        uint64_t vaddr = 0;
        if (!extract_hex_field(line, "mm=0x", mm) || !is_tracked_mm(mm, tid))
            return;
        if (extract_hex_field(line, "vaddr=0x", vaddr)) {
            pending_op_t op;
            op.vaddr = vaddr;
            pending_by_tid_[tid] = op;
        }
    } else if (line.find("mm_page_alloc:", event_pos) != std::string::npos) {
        uint64_t page_ptr = 0;
        uint64_t pfn = 0;
        uint64_t order = 0;
        bool has_pfn = extract_hex_field(line, "pfn=0x", pfn) ||
            extract_dec_field(line, "pfn=", pfn);
        if (vmemmap_base_ == 0 && has_pfn && pfn != ~0ULL &&
            extract_hex_field(line, "page=", page_ptr) &&
            page_ptr > (pfn << kStructPageShift)) {
            vmemmap_base_ = page_ptr - (pfn << kStructPageShift);
            flush_deferred_pte_ranges();
        }
        auto it = pending_by_tid_.find(tid);
        if (it != pending_by_tid_.end() && has_pfn &&
            extract_dec_field(line, "order=", order)) {
            // A 2 MB THP fault first allocates the order-9 user folio and then
            // allocates an order-0 PTE page table to deposit for future splits.
            // Keeping `order >= current_order` preserves the order-9 folio while
            // still letting the final order-0 data page win over earlier order-0
            // page-table allocations on 4 KB faults.
            if (!it->second.has_alloc || order >= it->second.order) {
                it->second.pfn = pfn;
                it->second.order = order;
                it->second.has_alloc = true;
            }
        } else if (has_pfn && extract_dec_field(line, "order=", order) && order == 9) {
            // Stash the order-9 allocation for `tid` (`khugepaged`) so
            // `mm_khugepaged_scan_pmd` can consume it upon completion.
            pending_op_t op;
            op.pfn = pfn;
            op.order = order;
            op.has_alloc = true;
            pending_by_tid_[tid] = op;
        }
    } else if (line.find("pte_range_", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it == pending_by_tid_.end())
            return;
        uint64_t page_ptr = 0;
        uint64_t nr = 0;
        uint64_t addr = 0;
        if (extract_hex_field(line, "page=0x", page_ptr) &&
            extract_dec_field(line, "nr=", nr) && nr > 0 &&
            extract_hex_field(line, "addr=0x", addr)) {
            uint64_t aligned_vaddr = addr & ~(kPageSize4K - 1);
            if (vmemmap_base_ != 0 && page_ptr > vmemmap_base_) {
                uint64_t pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
                record_page_map(timestamp_us, aligned_vaddr, pfn * kPageSize4K,
                                static_cast<uint32_t>(nr));
            } else if (vmemmap_base_ == 0) {
                // Buffer early warm page-cache faults that arrive prior to the
                // first `mm_page_alloc` calibrating `vmemmap_base_`.
                deferred_pte_range_t def;
                def.timestamp_us = timestamp_us;
                def.page_ptr = page_ptr;
                def.nr = static_cast<uint32_t>(nr);
                def.aligned_vaddr = aligned_vaddr;
                deferred_pte_ranges_.push_back(def);
            }
            it->second.handled_by_pte_range = true;
        }
    } else if (line.find("fault_out_", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it != pending_by_tid_.end()) {
            pending_op_t op = it->second;
            pending_by_tid_.erase(it);
            if (!op.handled_by_pte_range && op.has_alloc && op.pfn != 0 &&
                op.pfn != ~0ULL) {
                uint32_t page_count = 1U << op.order;
                uint64_t length = static_cast<uint64_t>(page_count) * kPageSize4K;
                uint64_t aligned_vaddr = op.vaddr & ~(length - 1);
                record_page_map(timestamp_us, aligned_vaddr, op.pfn * kPageSize4K,
                                page_count);
            }
        }
    } else if (line.find("move_pt_", event_pos) != std::string::npos) {
        uint64_t mm = 0;
        uint64_t old_addr = 0;
        uint64_t new_addr = 0;
        uint64_t len = 0;
        if (!extract_hex_field(line, "mm=0x", mm) || !is_tracked_mm(mm, tid))
            return;
        if (extract_hex_field(line, "old_addr=0x", old_addr) &&
            extract_hex_field(line, "new_addr=0x", new_addr) &&
            extract_dec_field(line, "len=", len) && len >= kPageSize4K) {
            uint64_t aligned_old = old_addr & ~(kPageSize4K - 1);
            uint64_t aligned_new = new_addr & ~(kPageSize4K - 1);
            uint32_t num_pages = static_cast<uint32_t>(len / kPageSize4K);

            v2p_record_t unmap_rec = { };
            unmap_rec.timestamp_us = timestamp_us;
            unmap_rec.vaddr = aligned_old;
            unmap_rec.paddr = 0;
            unmap_rec.page_count = num_pages;
            unmap_rec.type = V2P_RECORD_UNMAP;
            write_record(unmap_rec);

            struct moved_page_t {
                uint64_t dst_vpage;
                uint64_t paddr;
            };
            std::vector<moved_page_t> moved;
            if (num_pages <= active_paddr_by_vpage_.size()) {
                for (uint64_t off = 0; off < len; off += kPageSize4K) {
                    auto it = active_paddr_by_vpage_.find(aligned_old + off);
                    if (it != active_paddr_by_vpage_.end()) {
                        moved.push_back({ aligned_new + off, it->second });
                        active_paddr_by_vpage_.erase(it);
                    }
                }
            } else {
                const uint64_t old_end = aligned_old + len;
                for (auto it = active_paddr_by_vpage_.begin();
                     it != active_paddr_by_vpage_.end();) {
                    if (it->first >= aligned_old && it->first < old_end) {
                        moved.push_back(
                            { aligned_new + (it->first - aligned_old), it->second });
                        it = active_paddr_by_vpage_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            for (const moved_page_t &mp : moved) {
                record_page_map(timestamp_us, mp.dst_vpage, mp.paddr, 1);
            }
        }
    } else if (line.find("mig_ptes_", event_pos) != std::string::npos) {
        uint64_t src = 0;
        uint64_t dst = 0;
        if (vmemmap_base_ != 0 && extract_hex_field(line, "src=0x", src) &&
            src > vmemmap_base_ && extract_hex_field(line, "dst=0x", dst) &&
            dst > vmemmap_base_ && dst != src) {
            uint64_t src_pfn = (src - vmemmap_base_) >> kStructPageShift;
            uint64_t dst_pfn = (dst - vmemmap_base_) >> kStructPageShift;
            uint64_t src_paddr = src_pfn * kPageSize4K;
            uint64_t dst_paddr = dst_pfn * kPageSize4K;
            std::vector<uint64_t> migrated_vpages;
            for (const auto &kv : active_paddr_by_vpage_) {
                if (kv.second == src_paddr)
                    migrated_vpages.push_back(kv.first);
            }
            for (uint64_t vpage : migrated_vpages) {
                record_page_map(timestamp_us, vpage, dst_paddr, 1);
            }
        }
    } else if (line.find("unmap_", event_pos) != std::string::npos) {
        uint64_t mm = 0;
        uint64_t start = 0;
        uint64_t end = 0;
        if (!extract_hex_field(line, "mm=0x", mm) || !is_tracked_mm(mm, tid))
            return;
        if (extract_hex_field(line, "start=0x", start) &&
            extract_hex_field(line, "end=0x", end) && end > start) {
            uint64_t aligned_start = start & ~(kPageSize4K - 1);
            uint64_t aligned_end = (end + kPageSize4K - 1) & ~(kPageSize4K - 1);
            for (auto it = active_paddr_by_vpage_.begin();
                 it != active_paddr_by_vpage_.end();) {
                if (it->first >= aligned_start && it->first < aligned_end) {
                    it = active_paddr_by_vpage_.erase(it);
                } else {
                    ++it;
                }
            }
            v2p_record_t unmap_rec = { };
            unmap_rec.timestamp_us = timestamp_us;
            unmap_rec.vaddr = aligned_start;
            unmap_rec.paddr = 0;
            unmap_rec.page_count =
                static_cast<uint32_t>((aligned_end - aligned_start) / kPageSize4K);
            unmap_rec.type = V2P_RECORD_UNMAP;
            write_record(unmap_rec);
        }
    } else if (line.find("mm_khugepaged_scan_pmd:", event_pos) != std::string::npos) {
        auto it = pending_by_tid_.find(tid);
        if (it == pending_by_tid_.end())
            return;
        pending_op_t op = it->second;
        pending_by_tid_.erase(it);
        uint64_t mm = 0;
        uint64_t scan_pfn = 0;
        uint64_t page_ptr = 0;
        if (!extract_hex_field(line, "mm=", mm) || tracked_mm_ == 0 ||
            mm != tracked_mm_ ||
            line.find("status=succeeded", event_pos) == std::string::npos) {
            return;
        }
        if (!op.has_alloc || op.order != 9 || op.pfn == 0 || op.pfn == ~0ULL)
            return;
        if (!extract_hex_field(line, "scan_pfn=", scan_pfn) || scan_pfn == 0 ||
            scan_pfn == ~0ULL) {
            if (vmemmap_base_ != 0 && extract_hex_field(line, "page=", page_ptr) &&
                page_ptr > vmemmap_base_) {
                scan_pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
            }
        }
        if (scan_pfn == 0)
            return;
        uint64_t scan_paddr = scan_pfn * kPageSize4K;
        uint64_t collapsed_vaddr = 0;
        for (const auto &kv : active_paddr_by_vpage_) {
            if (kv.second == scan_paddr) {
                collapsed_vaddr = kv.first & ~(kPageSize2M - 1);
                break;
            }
        }
        if (collapsed_vaddr != 0) {
            record_page_map(timestamp_us, collapsed_vaddr, op.pfn * kPageSize4K,
                            kPagesPer2M);
        }
    }
#endif
}

bool
v2p_tracker_t::is_tracked_mm(uint64_t mm, pid_t tid)
{
#ifdef LINUX
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
#else
    return false;
#endif
}

void
v2p_tracker_t::record_page_map(uint64_t timestamp_us, uint64_t vaddr, uint64_t paddr,
                               uint32_t page_count)
{
#ifdef LINUX
    for (uint32_t i = 0; i < page_count; ++i) {
        active_paddr_by_vpage_[vaddr + static_cast<uint64_t>(i) * kPageSize4K] =
            paddr + static_cast<uint64_t>(i) * kPageSize4K;
    }
    v2p_record_t rec = { };
    rec.timestamp_us = timestamp_us;
    rec.vaddr = vaddr;
    rec.paddr = paddr;
    rec.page_count = page_count;
    rec.type = V2P_RECORD_MAP;
    write_record(rec);
#endif
}

void
v2p_tracker_t::flush_deferred_pte_ranges()
{
#ifdef LINUX
    if (vmemmap_base_ == 0 || deferred_pte_ranges_.empty())
        return;
    for (const deferred_pte_range_t &def : deferred_pte_ranges_) {
        if (def.page_ptr > vmemmap_base_) {
            uint64_t pfn = (def.page_ptr - vmemmap_base_) >> kStructPageShift;
            record_page_map(def.timestamp_us, def.aligned_vaddr, pfn * kPageSize4K,
                            def.nr);
        }
    }
    deferred_pte_ranges_.clear();
#endif
}

void
v2p_tracker_t::write_record(const v2p_record_t &rec)
{
#ifdef LINUX
    if (out_fd_ < 0)
        return;
    const char *ptr = reinterpret_cast<const char *>(&rec);
    size_t remaining = sizeof(rec);
    while (remaining > 0) {
        ssize_t n = write(out_fd_, ptr, remaining);
        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        ptr += n;
        remaining -= static_cast<size_t>(n);
    }
#endif
}

/***************************************************************************
 * 4. Background Reader Thread, Pipe Draining, and Overflow Accounting
 */

void
v2p_tracker_t::reader_thread_entry(void *arg)
{
    static_cast<v2p_tracker_t *>(arg)->reader_loop();
}

void
v2p_tracker_t::reader_loop()
{
#ifdef LINUX
    dr_client_thread_set_suspendable(false);
    reader_tid_ = static_cast<pid_t>(syscall(SYS_gettid));
    thread_running_.store(true, std::memory_order_release);

    while (!stop_requested_.load(std::memory_order_acquire)) {
        struct pollfd pfd = { trace_pipe_fd_, POLLIN, 0 };
        if (poll(&pfd, 1, 10) <= 0)
            continue;
        drain_pipe();
    }
    // Drain any final events written before tracing_on was set to 0.
    drain_pipe();
    thread_exited_.store(true, std::memory_order_release);
#endif
}

void
v2p_tracker_t::drain_pipe()
{
#ifdef LINUX
    if (trace_pipe_fd_ < 0)
        return;
    char chunk[16384];
    while (true) {
        ssize_t bytes_read = read(trace_pipe_fd_, chunk, sizeof(chunk));
        if (bytes_read <= 0)
            break;
        pipe_remainder_.append(chunk, static_cast<size_t>(bytes_read));
        size_t line_start = 0;
        size_t newline_pos = 0;
        while ((newline_pos = pipe_remainder_.find('\n', line_start)) !=
               std::string::npos) {
            process_trace_line(
                pipe_remainder_.substr(line_start, newline_pos - line_start));
            line_start = newline_pos + 1;
        }
        if (line_start > 0)
            pipe_remainder_.erase(0, line_start);
    }
#endif
}

uint64_t
v2p_tracker_t::read_total_overflows() const
{
#ifdef LINUX
    std::string per_cpu_dir = instance_dir_ + "/per_cpu";
    DIR *dir = opendir(per_cpu_dir.c_str());
    if (dir == nullptr)
        return 0;
    uint64_t total_overflow = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, "cpu", 3) != 0)
            continue;
        std::string stats_path = per_cpu_dir + "/" + ent->d_name + "/stats";
        FILE *f = fopen(stats_path.c_str(), "r");
        if (f == nullptr)
            continue;
        char line[256];
        while (fgets(line, sizeof(line), f) != nullptr) {
            if (strncmp(line, "overrun:", 8) == 0) {
                uint64_t val = 0;
                std::string s(line);
                if (extract_dec_field(s, "overrun: ", val))
                    total_overflow += val;
                break;
            }
        }
        fclose(f);
    }
    closedir(dir);
    return total_overflow;
#else
    return 0;
#endif
}

/***************************************************************************
 * 5. Low-Level Utilities
 */

#ifdef LINUX
namespace {

// Verifies the running kernel is at least 6.12 (where the kprobe struct
// offsets in `kKprobeSpecs` are valid), and sets `*use_pmc_out = true` on
// Linux 6.15+ (where `move_page_tables` takes `struct pagetable_move_control *`
// instead of register arguments).
bool
check_kernel_version(bool *use_pmc_out)
{
    struct utsname uts = { };
    if (uname(&uts) != 0)
        return false;
    int major = 0;
    int minor = 0;
    if (sscanf(uts.release, "%d.%d", &major, &minor) != 2)
        return false;
    if (major < 6 || (major == 6 && minor < 12)) {
        dr_fprintf(STDERR,
                   "WARNING: drmemtrace v2p_tracker requires Linux >= 6.12 (found %s)\n",
                   uts.release);
        return false;
    }
    *use_pmc_out = (major > 6) || (major == 6 && minor >= 15);
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
    struct dirent *ent;
    while ((ent = readdir(proc)) != nullptr) {
        char *endptr = nullptr;
        long val = strtol(ent->d_name, &endptr, 10);
        if (endptr == ent->d_name || *endptr != '\0' || val <= 0)
            continue;
        pid_t candidate = static_cast<pid_t>(val);
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", candidate);
        FILE *f = fopen(comm_path, "r");
        if (f == nullptr)
            continue;
        char buf[64] = { };
        if (fgets(buf, sizeof(buf), f) != nullptr &&
            (strncmp(buf, "khugepaged", 10) == 0 || strncmp(buf, "kcompactd", 9) == 0)) {
            pids.push_back(candidate);
        }
        fclose(f);
    }
    closedir(proc);
    return pids;
}

// Computes the additive offset in microseconds from the selected kernel trace
// clock (`CLOCK_TAI` or `CLOCK_MONOTONIC`) to DynamoRIO's UTC epoch
// (microseconds since Jan 1, 1601, matching `query_time_micros()` in
// `core/unix/os.c`).
uint64_t
compute_clock_to_dr_offset_us(clockid_t clk_id)
{
    struct timespec clk_ts = { };
    struct timeval real_tv = { };
    clock_gettime(clk_id, &clk_ts);
    gettimeofday(&real_tv, nullptr);

    uint64_t clk_us = static_cast<uint64_t>(clk_ts.tv_sec) * 1000000ULL +
        static_cast<uint64_t>(clk_ts.tv_nsec) / 1000ULL;
    uint64_t dr_real_us = static_cast<uint64_t>(real_tv.tv_sec) * 1000000ULL +
        static_cast<uint64_t>(real_tv.tv_usec) + kDrUtcToEpochMicros;
    return dr_real_us - clk_us;
}

// Locates the mounted tracefs root directory (`/sys/kernel/tracing` or
// `/sys/kernel/debug/tracing`).
std::string
find_tracefs_root()
{
    struct stat st = { };
    if (stat("/sys/kernel/tracing/instances", &st) == 0)
        return "/sys/kernel/tracing";
    if (stat("/sys/kernel/debug/tracing/instances", &st) == 0)
        return "/sys/kernel/debug/tracing";
    return "";
}

// Writes `content` to a tracefs control file at `path`, truncating by default
// or appending when `append` is true (e.g., when registering/removing kprobes).
bool
write_trace_file(const std::string &path, const std::string &content, bool append)
{
    int flags = O_WRONLY | O_CLOEXEC | (append ? O_APPEND : O_TRUNC);
    int fd = open(path.c_str(), flags);
    if (fd < 0)
        return false;
    ssize_t written = write(fd, content.data(), content.size());
    close(fd);
    return written == static_cast<ssize_t>(content.size());
}

// Parses a hexadecimal integer value immediately following `key` in `line`.
bool
extract_hex_field(const std::string &line, const char *key, uint64_t &out)
{
    size_t pos = line.find(key);
    if (pos == std::string::npos)
        return false;
    pos += strlen(key);
    if (pos + 2 <= line.size() && line[pos] == '0' &&
        (line[pos + 1] == 'x' || line[pos + 1] == 'X')) {
        pos += 2;
    }
    if (pos >= line.size())
        return false;
    const char *start = line.c_str() + pos;
    char *endptr = nullptr;
    errno = 0;
    unsigned long long val = strtoull(start, &endptr, 16);
    if (endptr == start || errno != 0)
        return false;
    out = static_cast<uint64_t>(val);
    return true;
}

// Parses a decimal integer value immediately following `key` in `line`.
bool
extract_dec_field(const std::string &line, const char *key, uint64_t &out)
{
    size_t pos = line.find(key);
    if (pos == std::string::npos)
        return false;
    pos += strlen(key);
    if (pos >= line.size())
        return false;
    const char *start = line.c_str() + pos;
    char *endptr = nullptr;
    errno = 0;
    unsigned long long val = strtoull(start, &endptr, 10);
    if (endptr == start || errno != 0)
        return false;
    out = static_cast<uint64_t>(val);
    return true;
}

} // namespace
#endif // LINUX

} // namespace drmemtrace
} // namespace dynamorio
