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

/* v2p_tracker: Virtual-to-physical (V2P) page-table transition tracker.
 *
 * Overview:
 * v2p_tracker_t records a timestamped history of virtual-to-physical address
 * mappings for the traced process during offline drmemtrace execution
 * (-offline -enable_v2p_tracefs). At startup, it enables an isolated Linux
 * tracefs instance, snapshots pre-existing present mappings from
 * /proc/<pid>/maps and /proc/<pid>/pagemap, and spawns a DynamoRIO client
 * thread that drains kernel memory-management events from trace_pipe into a
 * compact binary log (v2p.bin) consumed during raw2trace post-processing.
 * Requires Linux 6.12+ (confirmed through Linux 6.18) and CAP_SYS_ADMIN.
 *
 * Tracked Page-Table Transitions:
 *   1. Initial Address-Space Snapshot (/proc/<pid>/maps + /proc/<pid>/pagemap):
 *      Captures all virtual pages already resident when tracing begins (code,
 *      stack, loader, and DynamoRIO mappings) at timestamp_us = 0 after
 * enabling tracefs probes so concurrent faults are also captured in trace_pipe.
 *   2. Anonymous and Cold File Faults (handle_mm_fault + mm_page_alloc):
 *      Captures newly allocated 4 KB pages (order=0) and 2 MB Transparent
 *      Hugepages (order=9), as well as Copy-on-Write (CoW) page allocations.
 *   3. Unmaps (unmap_page_range):
 *      Captures virtual address range invalidations from munmap(),
 *      madvise(MADV_DONTNEED), MAP_FIXED overwrites, and exit_mmap().
 *   4. Transparent Hugepage Collapses (mm_khugepaged_scan_pmd):
 *      Captures asynchronous khugepaged 2 MB collapses (512x 4 KB PTEs -> 1x 2
 * MB PMD) filtered to the tracked process's mm_struct. (Note: 2 MB PMD splits
 * via
 *      __split_huge_pmd are intentionally not hooked because splitting a 2 MB
 * PMD into 512x 4 KB PTEs preserves the exact same physical frame for every 4
 * KB virtual sub-page.)
 *   5. Page Migrations and Compaction (remove_migration_ptes):
 *      Captures physical frame changes when pages are relocated by kcompactd*,
 *      NUMA balancing, move_pages(), or THP page reclamation, emitting a new
 *      V2P_RECORD_MAP record for the destination physical frame.
 *   6. Warm Page-Cache and Fault-Around File Mappings (set_pte_range):
 *      Captures file-backed PTE installations (including multi-page
 *      do_fault_around batches) where the backing folio already resides in the
 *      page cache and no mm_page_alloc event fires. The kernel vmemmap_base
 *      offset used to translate struct page* pointers to PFNs is calibrated
 *      dynamically from mm_page_alloc events (page vs. pfn); any set_pte_range
 *      events arriving prior to calibration are buffered and replayed once
 *      vmemmap_base is learned.
 *   7. Virtual Address Relocations (move_page_tables):
 *      Captures PTE/PMD moves across virtual addresses caused by mremap() and
 *      execve() initial stack randomization (shift_arg_pages), emitting an
 *      unmap of the source range and re-mapping any known physical pages at the
 *      destination range.
 *
 * Untracked Edge Cases and Limitations:
 *   - Pre-Existing Mappings Across fork() Without execve(): Present PTEs
 *     duplicated by copy_present_pte() during fork() are not re-emitted for the
 *     child unless subsequently faulted (e.g., via CoW) or replaced by
 * execve().
 *   - Shared Zero Page Reads: Read faults satisfied by the kernel zero page
 *     (my_zero_pfn) do not allocate a private frame and are not recorded until
 *     a subsequent write fault triggers CoW allocation.
 *   - 2 MB File-Backed PMD Hugepages and Shmem Collapses: File/shmem hugepages
 *     installed directly at the PMD level via do_set_pmd() or collapsed via
 *     collapse_pte_mapped_thp() bypass set_pte_range and
 *     mm_khugepaged_scan_pmd.
 *   - Swap-Cached Anonymous Faults: If swap is enabled and an anonymous page is
 *     faulted in from an existing swap-cache folio (do_swap_page() without a
 *     fresh mm_page_alloc), the re-installed PTE is not captured.
 */

#ifndef _V2P_TRACKER_H_
#define _V2P_TRACKER_H_

#if defined(LINUX) && defined(X64)

#    include <sys/types.h>

#    include <atomic>
#    include <cstdint>
#    include <string>
#    include <unordered_map>
#    include <vector>

#    include "dr_api.h"
#    include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// Tracks dynamic virtual-to-physical (V2P) page-table mapping changes for the
// traced process using Linux kernel tracefs events and an initial pagemap
// snapshot.
class v2p_tracker_t {
public:
    v2p_tracker_t() = default;
    ~v2p_tracker_t();

    // Starts tracking `pid` (and any threads it creates; forked child processes
    // are excluded from the parent's tracefs instance and start their own
    // tracker), snapshots initial present pages from /proc/<pid>/pagemap, and
    // spawns a client reader thread that writes binary v2p_record_t entries to
    // `output_path`. `buffer_size_kb` configures the per-CPU kernel tracefs ring
    // buffer size in KB.
    bool
    start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb = 16384);

    // Stops kernel tracing, waits for the reader thread to drain remaining
    // buffered events, writes overflow/end sentinel records, and removes probes.
    void
    stop();

    // Returns whether kernel tracing is currently active.
    bool
    is_running() const
    {
        return running_.load(std::memory_order_acquire);
    }

private:
    // Tracks in-flight page fault or khugepaged allocation state per thread ID
    // between fault_in (handle_mm_fault entry) and fault_out (kretprobe).
    struct pending_op_t {
        uint64_t vaddr = 0;
        uint64_t pfn = 0;
        uint64_t order = 0;
        bool has_alloc = false;
        bool handled_by_pte_range = false;
    };

    // Buffers warm page-cache set_pte_range events that arrive before the first
    // mm_page_alloc event calibrates vmemmap_base_.
    struct deferred_pte_range_t {
        uint64_t timestamp_us = 0;
        uint64_t page_ptr = 0;
        uint32_t nr = 0;
        uint64_t aligned_vaddr = 0;
    };

    // Creates the isolated tracefs instance, registers kprobes, and enables
    // tracepoints and tracing_on.
    bool
    setup_kernel_tracing();

    // Walks /proc/<target_pid_>/maps and /proc/<target_pid_>/pagemap to seed
    // active_paddr_by_vpage_ and emit coalesced V2P_RECORD_MAP runs at
    // timestamp_us = 0 for all pages already resident at tracker startup.
    void
    snapshot_initial_pagemap();

    // Disables tracepoints/kprobes, removes the tracefs instance directory, and
    // unregisters the kprobe definitions from kprobe_events.
    void
    cleanup_kernel_tracing();

    // Parses a single kernel trace_pipe text line and emits v2p_record_t entries
    // if a page-table mapping or unmapping transition completed.
    void
    process_trace_line(const std::string &line);

    // Returns whether `mm` belongs to `target_pid_`, seeding `tracked_mm_` on
    // the first event from a thread in `target_pid_`'s thread group.
    bool
    is_tracked_mm(uint64_t mm, pid_t tid);

    // Updates active_paddr_by_vpage_ for `page_count` contiguous 4 KB pages and
    // writes a V2P_RECORD_MAP entry to out_fd_.
    void
    record_page_map(uint64_t timestamp_us, uint64_t vaddr, uint64_t paddr,
                    uint32_t page_count);

    // Translates and emits any set_pte_range events buffered in
    // deferred_pte_ranges_ once vmemmap_base_ has been calibrated.
    void
    flush_deferred_pte_ranges();

    // Writes a single 32-byte v2p_record_t to out_fd_, retrying on EINTR.
    void
    write_record(const v2p_record_t &rec);

    // Entry point passed to dr_create_client_thread().
    static void
    reader_thread_entry(void *arg);

    // Polls trace_pipe_fd_ in a loop until stop_requested_ is set, then performs
    // a final drain before signaling thread_exited_.
    void
    reader_loop();

    // Reads all currently available bytes from non-blocking trace_pipe_fd_ and
    // dispatches complete newline-terminated lines to process_trace_line().
    void
    drain_pipe();

    // Sums the `overrun:` counters across all per_cpu/cpu*/stats files in the
    // tracefs instance to detect ring-buffer overflows (dropped events).
    uint64_t
    read_total_overflows() const;

    pid_t target_pid_ = 0;
    // OS TID of the client reader thread; events from this TID are ignored to
    // avoid self-tracing feedback when the reader thread allocates heap memory.
    pid_t reader_tid_ = 0;
    std::string tracefs_root_;
    std::string instance_dir_;
    uint32_t buffer_size_kb_ = 16384;
    // Offset in microseconds added to tracefs timestamps (CLOCK_TAI or
    // CLOCK_MONOTONIC) to convert them into DynamoRIO's UTC epoch (Jan 1, 1601).
    uint64_t clock_offset_us_ = 0;
    int out_fd_ = -1;
    int trace_pipe_fd_ = -1;
    std::string pipe_remainder_;

    std::atomic<bool> running_ { false };
    std::atomic<bool> stop_requested_ { false };
    std::atomic<bool> thread_running_ { false };
    std::atomic<bool> thread_exited_ { false };
    bool kernel_tracing_active_ = false;
    uint64_t dropped_events_ = 0;

    // Base address of the kernel `struct page` array (`vmemmap`), calibrated at
    // runtime from `mm_page_alloc` events via `page_ptr - (pfn << 6)`.
    uint64_t vmemmap_base_ = 0;

    // Kernel `struct mm_struct*` address for `target_pid_`, used to isolate
    // events belonging to this process's address space from forked children and
    // other processes scanned by `khugepaged`.
    uint64_t tracked_mm_ = 0;
    std::unordered_map<pid_t, pending_op_t> pending_by_tid_;

    // Active 4 KB virtual-page -> physical-address mappings for the current
    // address space, used to translate `move_page_tables`,
    // `remove_migration_ptes`, and `mm_khugepaged_scan_pmd` into destination
    // `V2P_RECORD_MAP` events.
    std::unordered_map<uint64_t, uint64_t> active_paddr_by_vpage_;
    std::vector<deferred_pte_range_t> deferred_pte_ranges_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX */

#endif /* _V2P_TRACKER_H_ */
