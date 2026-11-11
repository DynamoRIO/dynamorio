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

/* v2p_tracker: Tracefs virtual-to-physical (V2P) page-table tracker.
 *
 * Captures initial /proc/<pid>/pagemap state and live kernel page-table updates
 * into raw/v2p.bin during drmemtrace -offline collection (Linux 5.4+, 4 KB base
 * pages, CAP_SYS_ADMIN).
 */

#ifndef _V2P_TRACKER_H_
#define _V2P_TRACKER_H_

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <sys/types.h>
#    include <atomic>
#    include <cstdint>
#    include <set>
#    include <string>
#    include <utility>
#    include <vector>

#    include "dr_api.h"
#    include "raw2trace_shared.h"
#    include "trace_entry.h"
#    include "v2p_check_kernel_support.h"
#    include "v2p_tracefs_hooks.h"

namespace dynamorio {
namespace drmemtrace {

// Manages the isolated kernel tracefs instance and background `trace_pipe` reader,
// snapshots `/proc/<pid>/pagemap`, maintains the live shadow page table, and emits
// timestamped `v2p_record_t` records to `raw/v2p.bin`.
class v2p_tracker_t {
public:
    v2p_tracker_t() = default;

    ~v2p_tracker_t()
    {
        stop();
    }

    // Enables an isolated tracefs instance for `pid`, calibrates kernel clock and
    // `vmemmap` constants, snapshots resident pages from `/proc/<pid>/pagemap`, and
    // starts the background reader thread writing `v2p_record_t` entries to
    // `output_path`.
    bool
    start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb = 16384);

    // Stops kernel tracing, drains buffered trace lines, writes any ring-buffer
    // overflow summary plus `V2P_RECORD_END`, and removes registered probes.
    void
    stop();

    // Shadow page-table helpers called by `v2p_tracefs_hooks_t` from the reader thread.
    uint64_t
    zero_pfn() const
    {
        return zero_pfn_;
    }
    bool
    has_shadow_mapping(uint64_t va, uint32_t pages = 1) const
    {
        auto it =
            const_cast<v2p_shadow_page_table_t &>(shadow_table_).find_overlapping(va);
        return pages > 0 && it != shadow_table_.spans.end() &&
            it->first < v2p_shadow_page_table_t::end_of(va, pages);
    }
    bool
    page_ptr_to_paddr(uint64_t page_ptr, uint64_t &paddr_out) const;
    // Returns whether `mm` belongs to `target_pid_` (learning `tracked_mm_` on
    // first hit).
    bool
    is_tracked_mm(uint64_t mm, pid_t tid);
    void
    bind_pages(uint64_t timestamp_us, uint64_t aligned_vaddr, uint64_t aligned_paddr,
               uint32_t page_count, bool is_huge_2m = false, bool is_huge_zero = false);
    // Rebinds all virtual spans overlapping `src_paddr` to `dst_paddr` after migration.
    void
    rebind_migrated_folio(uint64_t timestamp_us, uint64_t src_paddr, uint64_t dst_paddr,
                          uint32_t folio_pages);
    void
    unbind_range(uint64_t timestamp_us, uint64_t start_vaddr, uint64_t end_vaddr);
    void
    split_huge_pmd(uint64_t timestamp_us, uint64_t aligned_2m_va, bool is_anon);

private:
    using shadow_span_t = v2p_shadow_page_table_t::span_t;
    // `(paddr_base, vaddr_start)` entry in the physical-to-virtual reverse index.
    using paddr_ref_t = std::pair<uint64_t, uint64_t>;

    struct rebound_slice_t {
        uint64_t vaddr = 0;
        uint64_t paddr = 0;
        uint32_t page_count = 0;
        bool is_huge_2m = false;
        bool is_huge_zero = false;
    };

    bool
    set_event(const std::string &rel_dir, bool enable,
              const std::string *filter = nullptr) const;
    std::string
    probe_filter_expr(const v2p_probe_spec_t &spec) const;
    bool
    apply_probe_filters() const;
    bool
    calibrate_live_vmemmap_and_mm();
    void
    snapshot_pagemap();
    void
    cleanup_kernel_tracing();
    void
    clear_shadow_tables();
    void
    clear_huge_2m_window(uint64_t aligned_2m_va);
    void
    rebind_zero_ptes(uint64_t timestamp_us, uint64_t start_va, uint64_t end_va);
    void
    split_overlapping_huge_zero(uint64_t timestamp_us, uint64_t lo, uint64_t hi,
                                bool partial_only);
    void
    update_span_reverse(uint64_t vaddr_start, const shadow_span_t &span, bool insert);
    void
    carve_range(uint64_t timestamp_us, uint64_t start_vaddr, uint64_t end_vaddr,
                bool emit_unmap, bool preserve_huge);
    void
    bind_coalesced_slices(uint64_t timestamp_us,
                          const std::vector<rebound_slice_t> &slices);

    uint64_t
    read_total_overflows() const;
    static void
    reader_thread_entry(void *arg);
    void
    drain_pipe();
    void
    process_trace_line(const std::string &line);

    pid_t target_pid_ = 0;
    std::atomic<pid_t> reader_tid_ { 0 };
    std::string tracefs_root_;
    std::string instance_dir_;
    std::string pipe_remainder_;
    std::vector<std::string> registered_probes_;
    bool kernel_tracing_active_ = false;
    // Offset added to tracefs clock microseconds to match DR's UTC/FILETIME epoch.
    uint64_t clock_offset_us_ = 0;
    // KASLR-randomized base of the kernel's 64-byte `struct page` array (`vmemmap`).
    uint64_t vmemmap_base_ = 0;
    // PFN of the kernel's shared 4 KB zero page.
    uint64_t zero_pfn_ = 0;
    // Kernel `mm_struct *` pointer of `target_pid_`.
    uint64_t tracked_mm_ = 0;
    int out_fd_ = -1;
    int trace_pipe_fd_ = -1;
    v2p_kernel_plan_t plan_;
    std::atomic<bool> running_ { false };
    std::atomic<bool> stop_requested_ { false };
    std::atomic<bool> thread_running_ { false };
    std::atomic<bool> thread_exited_ { false };
    v2p_shadow_page_table_t shadow_table_;
    // In-memory physical-to-virtual map (`paddr_base -> vaddr_start`) mirroring
    // `shadow_table_`. Needed because `remove_migration_ptes` reports only the old and
    // new physical frames without a virtual address or process, so we look up migrated
    // physical frames here to find and update any affected virtual pages in our process.
    // Bucket 0 holds single 4 KB pages and bucket 1 holds multi-page spans (bounded by
    // `max_multi_span_bytes_`) so large spans never widen 4 KB migration lookups.
    std::set<paddr_ref_t> paddr_spans_[2];
    uint64_t max_multi_span_bytes_ = 0;
    v2p_tracefs_hooks_t hooks_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
#endif /* _V2P_TRACKER_H_ */
