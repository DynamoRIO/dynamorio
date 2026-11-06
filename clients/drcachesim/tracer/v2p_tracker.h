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
 * Requires Linux x86_64 of at least version 5.11 and CAP_SYS_ADMIN.
 *
 * Tracked Page-Table Transitions:
 *   1. Initial Address-Space Snapshot (/proc/<pid>/maps + /proc/<pid>/pagemap):
 *      Captures all virtual pages already resident when tracing begins (code,
 *      stack, loader, and DynamoRIO mappings) at timestamp_us = 0 after
 *      enabling tracefs probes, coalescing 2 MB-aligned hugepages only when
 *      verified in the page table (via PAGEMAP_SCAN on Linux 6.7+ or
 *      /proc/kpageflags + /proc/<pid>/smaps on Linux 5.11-6.6).
 *   2. Page-Allocating Faults (handle_mm_fault + mm_page_alloc):
 *      Captures newly allocated 4 KB pages (order=0) and 2 MB Transparent
 *      Hugepages (order=9) on anonymous faults, Copy-on-Write (CoW) faults,
 *      and file faults not yet in the page cache.
 *   3. Unmaps (unmap_page_range):
 *      Captures virtual address range invalidations from munmap(),
 *      madvise(MADV_DONTNEED), MAP_FIXED overwrites, and exit_mmap().
 *   4. Transparent Hugepage Collapses (mm_khugepaged_scan_pmd):
 *      Captures asynchronous khugepaged 2 MB collapses (512x 4 KB PTEs -> 1x
 *      2 MB PMD) filtered to the tracked process's mm_struct. (2 MB PMD splits
 *      via __split_huge_pmd are intentionally not hooked because splitting a
 *      2 MB PMD into 512x 4 KB PTEs preserves the exact same physical frame
 *      for every 4 KB virtual sub-page.)
 *   5. Page Migrations and Compaction (remove_migration_ptes):
 *      Captures physical frame changes when pages are relocated by kcompactd*,
 *      NUMA balancing, move_pages(), or THP page reclamation, emitting a new
 *      V2P_RECORD_MAP record for the destination physical frame.
 *   6. File-Backed Memory in the Page Cache (set_pte_range on Linux 6.6+,
 *      do_set_pte on Linux 5.11-6.5):
 *      Captures PTE installations (including multi-page do_fault_around
 *      batches) for file-backed memory already resident in the page cache.
 *      Because the kernel wires PTEs directly to existing page-cache frames
 *      without calling mm_page_alloc—and once mapped, that file-backed memory
 *      is not faulted again—hooking PTE installation is required to record
 *      its physical frames. The kernel vmemmap_base offset used to translate
 *      struct page* pointers to PFNs is calibrated dynamically from
 *      mm_page_alloc events (page vs. pfn); any events arriving prior to
 *      calibration are buffered and replayed once vmemmap_base is learned.
 *   7. Virtual Address Relocations (move_page_tables):
 *      Captures PTE/PMD moves across virtual addresses caused by mremap() and
 *      execve() initial stack randomization (shift_arg_pages), emitting an
 *      unmap of the source range and re-mapping any known physical pages at
 *      the destination range.
 *
 * Untracked Edge Cases and Limitations:
 *   - Pre-Existing Mappings Across fork() Without execve(): Present PTEs
 *     duplicated by copy_present_pte() during fork() are not re-emitted for
 *     the child unless subsequently faulted (e.g., via CoW) or replaced by
 *     execve().
 *   - Shared Zero Page Reads: Read faults satisfied by the kernel zero page
 *     (my_zero_pfn) do not allocate a private frame and are not recorded until
 *     a subsequent write fault triggers CoW allocation.
 *   - 2 MB File-Backed PMD Hugepages and Shmem Collapses: File/shmem hugepages
 *     installed directly at the PMD level via do_set_pmd() or collapsed via
 *     collapse_pte_mapped_thp() bypass set_pte_range/do_set_pte and
 *     mm_khugepaged_scan_pmd.
 *   - Swap-Cached Anonymous Faults: If swap is enabled and an anonymous page
 *     is faulted in from an existing swap-cache folio (do_swap_page() without
 *     a fresh mm_page_alloc), the re-installed PTE is not captured.
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
    ~v2p_tracker_t()
    {
        stop();
    }

    // Starts tracking `pid` (excluding forked child processes from the parent's
    // tracefs instance so each process maintains an isolated tracker),
    // snapshots initial resident pages from /proc/<pid>/pagemap, and spawns a
    // client reader thread writing binary v2p_record_t entries to `output_path`.
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
    struct pending_op_t {
        uint64_t vaddr = 0;
        uint64_t pfn = 0;
        uint64_t order = 0;
        bool has_alloc = false;
        bool handled_by_pte_range = false;
    };

    struct deferred_pte_range_t {
        uint64_t timestamp_us = 0;
        uint64_t page_ptr = 0;
        uint32_t nr = 0;
        uint64_t aligned_vaddr = 0;
    };

    bool
    kernel_at_least(int major, int minor) const
    {
        return kernel_major_ > major ||
            (kernel_major_ == major && kernel_minor_ >= minor);
    }
    void
    cleanup_kernel_tracing();
    static void
    reader_thread_entry(void *arg);
    void
    drain_pipe();
    void
    snapshot_initial_pagemap();
    void
    process_trace_line(const std::string &line);
    bool
    is_tracked_mm(uint64_t mm, pid_t tid);
    void
    record_page_map(uint64_t timestamp_us, uint64_t vaddr, uint64_t paddr,
                    uint32_t page_count);
    void
    flush_deferred_pte_ranges();
    uint64_t
    read_total_overflows() const;

    pid_t target_pid_ = 0;
    pid_t reader_tid_ = 0;
    int kernel_major_ = 0;
    int kernel_minor_ = 0;
    std::string tracefs_root_;
    std::string instance_dir_;
    std::string pipe_remainder_;
    uint64_t clock_offset_us_ = 0;
    uint64_t vmemmap_base_ = 0;
    uint64_t tracked_mm_ = 0;
    int out_fd_ = -1;
    int trace_pipe_fd_ = -1;
    bool kernel_tracing_active_ = false;
    std::atomic<bool> running_ { false };
    std::atomic<bool> stop_requested_ { false };
    std::atomic<bool> thread_running_ { false };
    std::atomic<bool> thread_exited_ { false };
    std::unordered_map<pid_t, pending_op_t> pending_by_tid_;
    std::unordered_map<uint64_t, uint64_t> active_paddr_by_vpage_;
    std::vector<deferred_pte_range_t> deferred_pte_ranges_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX */
#endif /* _V2P_TRACKER_H_ */
