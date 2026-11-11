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

/* v2p_tracefs_hooks: BTF hook registration and tracefs event handlers for
 * virtual-to-physical page-table transitions.
 */

#ifndef _V2P_TRACEFS_HOOKS_H_
#define _V2P_TRACEFS_HOOKS_H_

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <sys/types.h>
#    include <cstddef>
#    include <cstdint>
#    include <string>
#    include <unordered_map>

#    include "v2p_check_kernel_support.h"

namespace dynamorio {
namespace drmemtrace {

class v2p_tracker_t;

// Validates and registers kernel hooks against BTF, maintains per-thread in-flight
// kernel hook state, and translates `trace_pipe` event lines into `v2p_tracker_t`
// shadow page-table updates.
class v2p_tracefs_hooks_t {
public:
    // Classifies each registered dynamic kprobe/kretprobe event line in `trace_pipe`.
    // Paired `*Entry` and `*Return` values bracket a single kernel function call via
    // entry (`p:`) and return (`r:`) probes on the same thread (`tid`).
    enum class event_type_t {
        kUnknown,

        // Shared zero-page read-fault detection (`handle_mm_fault` entry/return):
        kHandleMmFaultEntry,  // handle_mm_fault entry: fault vaddr & R/W flags.
        kGetHugeZeroReturn,   // mm_get_huge_zero_folio/page return: 2M zero folio.
        kHandleMmFaultReturn, // handle_mm_fault return: commits silent zero page.

        // Anonymous reverse-mapping (`rmap`) page and 2 MB THP installations:
        kAnonRmapNew,  // folio/page_add_new_anon_rmap: new anon folio/page.
        kAnonRmapPtes, // folio_add_anon_rmap_ptes / page_add_anon_rmap.
        kAnonRmapPmd,  // folio_add_anon_rmap_pmd: 2 MB anonymous THP PMD.
        kFilePte,      // set_pte_range/do_set_pte/alloc_set_pte: 4 KB file PTEs.

        // 2 MB file/shmem THP PMD installations (`do_set_pmd` or file-rmap fallback):
        kDoSetPmdEntry,     // do_set_pmd entry: candidate 2 MB file/shmem PMD.
        kDoSetPmdReturn,    // do_set_pmd return (ret == 0): commits 2M file PMD.
        kCollapseThpEntry,  // collapse_pte_mapped_thp entry: saves 2 MB vaddr.
        kFileRmapPmd,       // folio_add_file_rmap_pmd / page_add_file_rmap.
        kCollapseThpReturn, // collapse_pte_mapped_thp return: clears 2M vaddr.

        // Physical page migration (`remove_migration_ptes`).
        kRemoveMigrationPtes,

        // Virtual address relocation (`mremap`).
        kMovePageTablesEntry,  // move_page_tables entry: mremap range & target.
        kMovePageTablesReturn, // move_page_tables return: actual moved byte span.

        // Mapping invalidations (explicit unmaps and reclaim/swap-out evictions).
        kUnmapPageRange,
        kUnmapVmas,
        kZapVmaRange,
        kTryToUnmapEntry,  // try_to_unmap_one entry: candidate reclaim folio.
        kTryToUnmapReturn, // try_to_unmap_one return: evicts unmapped folio.

        // 2 MB transparent hugepage PMD splits (`__split_huge_pmd`).
        kSplitHugePmd,

        // Explicit hugepage pools (`hugetlbfs`: 64 KB, 2 MB, 32 MB, and 1 GB).
        kHugetlbFaultEntry,  // hugetlb_fault entry: begins hugetlbfs page fault.
        kHugetlbAnonRmap,    // hugetlb/hugepage_add_new_anon_rmap: private CoW.
        kHugetlbFolioUnlock, // folio_unlock/unlock_page: shared/file pool folio.
        kHugetlbFaultReturn, // hugetlb_fault return: commits unlocked pool folio.
        kHugetlbUnmap,       // __unmap_hugepage_range: tears down hugetlbfs span.
    };

    // Validates every registered V2P hook against `btf` and populates `plan.probes`.
    static void
    check_all_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan);

    // Clears all in-flight per-thread fault, PMD, mremap, reclaim, and hugetlb state.
    void
    clear();

    // Dispatches a `trace_pipe` event line starting at `event_pos` and returns true if
    // `line` matched a registered hook.
    bool
    handle_event(const std::string &line, size_t event_pos, pid_t tid,
                 uint64_t timestamp_us, const v2p_kernel_plan_t &plan,
                 v2p_tracker_t &tracker, bool in_irq = false);

private:
    struct trace_event_t;

    // Tracks `handle_mm_fault` entry -> return on anonymous VMAs (`vm_ops == nullptr`)
    // so an unhandled read fault (`ret == 0`) identifies silent zero-page installs.
    struct pending_fault_t {
        uint64_t vaddr = 0;
        uint64_t huge_zero_ptr = 0;
        bool is_write = false;
        bool handled = false;
    };

    // Tracks `do_set_pmd` entry -> return (`ret == 0` commits a 2 MB file/shmem PMD).
    struct pending_pmd_t {
        uint64_t vaddr = 0;
        uint64_t paddr = 0;
    };

    // Tracks `move_page_tables` entry -> return (`moved` gives actual relocated bytes
    // in case `mremap` stops partway through `[old_addr, +len)`).
    struct pending_move_t {
        uint64_t old_addr = 0;
        uint64_t new_addr = 0;
        uint64_t len = 0;
    };

    // Tracks `try_to_unmap_one` entry -> return (`ret != 0` confirms reclaim succeeded).
    struct pending_try_to_unmap_t {
        uint64_t aligned_vaddr = 0;
        uint64_t folio_paddr = 0;
        uint32_t max_pages = 1;
        uint32_t folio_pages = 1;
    };

    // Tracks `hugetlb_fault` entry -> return, pairing inner `hugetlb_add_new_anon_rmap`
    // (private CoW) or `folio_unlock` (shared / file-borrowed pool folio) with return.
    struct pending_hugetlb_t {
        uint64_t vaddr = 0;
        uint64_t vm_flags = 0;
        uint64_t folio_paddr = 0;
        uint32_t folio_pages = 0;
        bool saw_anon_rmap = false;
    };

    void
    mark_fault_handled(pid_t tid);
    bool
    install_pages(const trace_event_t &trace_event, v2p_tracker_t &tracker, uint32_t nr,
                  bool is_huge_2m);
    void
    bind_hugetlb(uint64_t ts, pid_t tid, uint64_t vaddr, uint64_t paddr, uint32_t nr,
                 v2p_tracker_t &tracker);

    std::unordered_map<pid_t, pending_fault_t> pending_faults_;
    std::unordered_map<pid_t, pending_pmd_t> pending_set_pmd_;
    // 2 MB-aligned fault/collapse address for legacy file-PMD rmap hooks lacking `vaddr`.
    std::unordered_map<pid_t, uint64_t> pending_collapse_vaddr_;
    std::unordered_map<pid_t, pending_move_t> pending_moves_;
    std::unordered_map<pid_t, pending_try_to_unmap_t> pending_try_to_unmap_;
    std::unordered_map<pid_t, pending_hugetlb_t> pending_hugetlb_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
#endif /* _V2P_TRACEFS_HOOKS_H_ */
