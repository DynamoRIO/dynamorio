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

#include "v2p_tracefs_hooks.h"

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <algorithm>
#    include <ctype.h>
#    include <limits>

#    include "v2p_tracker.h"

namespace dynamorio {
namespace drmemtrace {
namespace {

using event_type_t = v2p_tracefs_hooks_t::event_type_t;

/* -------------------------------------------------------------------------
 * Kernel Hook Selection (BTF Signature & Offset Resolution)
 * ------------------------------------------------------------------------- */

constexpr const char *kFolioOrdArg1 = "flags=+0($arg1):x64 ord={folio_order}($arg1):u8";
constexpr const char *kVmfMm =
    "mm={vm_area_struct.vm_mm:mm_struct*}({vm_fault.vma:vm_area_struct*}($arg1)):x64";
constexpr const char *kTlbMm = "mm={mmu_gather.mm:mm_struct*}($arg1):x64 ";

std::string
vma_mm(const char *arg)
{
    return std::string("mm={vm_area_struct.vm_mm:mm_struct*}(") + arg + "):x64";
}

// Builds `page=<p>:x64 mm=vma->vm_mm(<v>):x64 vaddr=<a>:x64 [extra]` for rmap hooks.
std::string
rmap_def(const char *page_arg, const char *vma_arg, const char *va_arg,
         const char *extra = nullptr)
{
    std::string s = std::string("page=") + page_arg + ":x64 " + vma_mm(vma_arg) +
        " vaddr=" + va_arg + ":x64";
    if (extra != nullptr) {
        s += " ";
        s += extra;
    }
    return s;
}

// Builds `mm=vmf->vma->vm_mm($arg1):x64 page=<p>:x64 vaddr=vmf->address($arg1):x64`.
std::string
vmf_page_def(const char *page_arg)
{
    return std::string(kVmfMm) + " page=" + page_arg +
        ":x64 vaddr={vm_fault.address:u64}($arg1):x64";
}

// Removes companion probes in `names` when their paired hook failed validation.
void
erase_probes(std::vector<v2p_probe_spec_t> &probes,
             std::initializer_list<const char *> names)
{
    probes.erase(std::remove_if(probes.begin(), probes.end(),
                                [&](const v2p_probe_spec_t &p) {
                                    for (const char *n : names) {
                                        if (p.base_name == n)
                                            return true;
                                    }
                                    return false;
                                }),
                 probes.end());
}

// Maps each base kprobe name (`v2p_probe_spec_t::base_name`) to its `event_type_t`.
const std::unordered_map<std::string, event_type_t> &
probe_name_map()
{
    static const std::unordered_map<std::string, event_type_t> kMap = {
        { "anon_new", event_type_t::kAnonRmapNew },
        { "anon_ptes", event_type_t::kAnonRmapPtes },
        { "anon_pmd", event_type_t::kAnonRmapPmd },
        { "file_pte", event_type_t::kFilePte },
        { "set_pmd_in", event_type_t::kDoSetPmdEntry },
        { "set_pmd_out", event_type_t::kDoSetPmdReturn },
        { "fault_in", event_type_t::kHandleMmFaultEntry },
        { "huge_zero_out", event_type_t::kGetHugeZeroReturn },
        { "fault_out", event_type_t::kHandleMmFaultReturn },
        { "collapse_in", event_type_t::kCollapseThpEntry },
        { "file_rmap_pmd", event_type_t::kFileRmapPmd },
        { "collapse_out", event_type_t::kCollapseThpReturn },
        { "mig_ptes", event_type_t::kRemoveMigrationPtes },
        { "move_pt_in", event_type_t::kMovePageTablesEntry },
        { "move_pt_out", event_type_t::kMovePageTablesReturn },
        { "unmap_range", event_type_t::kUnmapPageRange },
        { "unmap_vmas", event_type_t::kUnmapVmas },
        { "unmap_zap", event_type_t::kZapVmaRange },
        { "ttu_in", event_type_t::kTryToUnmapEntry },
        { "ttu_out", event_type_t::kTryToUnmapReturn },
        { "split_pmd", event_type_t::kSplitHugePmd },
        { "hugetlb_fault_in", event_type_t::kHugetlbFaultEntry },
        { "hugetlb_anon", event_type_t::kHugetlbAnonRmap },
        { "hugetlb_unlock", event_type_t::kHugetlbFolioUnlock },
        { "hugetlb_fault_out", event_type_t::kHugetlbFaultReturn },
        { "hugetlb_unmap", event_type_t::kHugetlbUnmap },
    };
    return kMap;
}

// Pops and erases `tid`'s in-flight entry state when a paired return probe fires.
template <typename Map, typename Val>
bool
pop_pending(Map &map, pid_t tid, Val &out)
{
    auto it = map.find(tid);
    if (it == map.end())
        return false;
    out = it->second;
    map.erase(it);
    return true;
}

// Returns whether `nr` matches an architectural 4 KB-base-page `hugetlbfs` pool size
// (64 KB, 2 MB, 32 MB, or 1 GB, ignoring 128 KB page-cache folio unlocks).
bool
is_supported_hugetlb_pages(uint32_t nr)
{
    return nr == 16 || nr == 512 || nr == 8192 || nr == 262144;
}

// Explicit hugepage pool (`hugetlbfs`) hooks, skipped when `CONFIG_HUGETLB_PAGE=n`:
// - The kernel calls `hugetlb_add_new_anon_rmap` (or `hugepage_add_new_anon_rmap`)
//   whenever it allocates a private `hugetlbfs` / `MAP_HUGETLB` page or resolves a
//   private Copy-on-Write fault.
// - Shared `hugetlbfs` faults and read-only private faults borrowing a file pool
//   page never call an rmap hook; instead `hugetlb_fault` unlocks the pool folio via
//   `folio_unlock` / `unlock_page` in process context right before returning, so we
//   bracket `hugetlb_fault` and record the unlocked folio when `hugetlb_fault`
//   succeeds without an error or retry flag.
// - The kernel calls `__unmap_hugepage_range` whenever it unmaps or truncates a
//   `hugetlbfs` range.
void
check_hugetlb_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan)
{
    if (!btf.has_func("hugetlb_fault"))
        return;
    plan.has_hugetlb = true;
    const bool fault_ok = btf.check_hook(
        "hugetlb_fault", "mm_struct*, vm_area_struct*, u64, u32",
        { { "hugetlb_fault_in",
            "mm=$arg1:x64 vm_flags={vm_area_struct.vm_flags:u64}($arg2):x64 "
            "vaddr=$arg3:x64" },
          { "hugetlb_fault_out", "ret=$retval:u32", true } });
    std::string anon_def = rmap_def("$arg1", "$arg2", "$arg3", kFolioOrdArg1);
    btf.check_hook("hugetlb_add_new_anon_rmap",
                   { { "hugetlb_add_new_anon_rmap", "folio|page*, vm_area_struct*, u64",
                       "hugetlb_anon", anon_def },
                     { "hugepage_add_new_anon_rmap", "folio|page*, vm_area_struct*, u64",
                       "hugetlb_anon", anon_def } });
    std::string unlock_def = std::string("page=$arg1:x64 ") + kFolioOrdArg1;
    const char *unlock_flt = "(flags & {pg_head_mask}) && ord >= 4";
    btf.check_hook(
        "folio_unlock/unlock_page",
        { { "folio_unlock", "folio|page*", "hugetlb_unlock", unlock_def, -1, unlock_flt },
          { "unlock_page", "folio|page*", "hugetlb_unlock", unlock_def, -1,
            unlock_flt } });
    if (!fault_ok)
        erase_probes(plan.probes, { "hugetlb_anon", "hugetlb_unlock" });
    btf.check_hook("__unmap_hugepage_range",
                   "mmu_gather*, vm_area_struct*, u64, u64, folio|page*", "hugetlb_unmap",
                   vma_mm("$arg2") + " start=$arg3:x64 end=$arg4:x64 ref=$arg5:x64");
}

} // namespace

void
v2p_tracefs_hooks_t::check_all_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan)
{
    // 1. Shared 4 KB and 2 MB zero-page read-fault installations:
    // The kernel installs the shared 4 KB zero page or 2 MB huge zero folio during
    // `handle_mm_fault` without calling any reverse-mapping function, so we bracket
    // `handle_mm_fault` (and hook `mm_get_huge_zero_folio` / `mm_get_huge_zero_page`)
    // to cover anonymous 4 KB and 2 MB zero-page read faults.
    const bool fault_ok = btf.check_hook(
        "handle_mm_fault", "vm_area_struct*, u64, u32",
        { { "fault_in",
            vma_mm("$arg1") +
                " vm_ops={vm_area_struct.vm_ops:vm_operations_struct*}($arg1):x64 "
                "vaddr=$arg2:x64 flags=$arg3:u32" },
          { "fault_out", "ret=$retval:u32", true, "ret == 0" } });
    v2p_probe_spec_t hz_probe("huge_zero_out", "ret=$retval:x64", true, "ret != 0");
    btf.check_hook("mm_get_huge_zero_folio/page",
                   { { "mm_get_huge_zero_folio", "mm_struct*", { hz_probe }, 1 },
                     { "mm_get_huge_zero_page", "mm_struct*", { hz_probe }, 1 } });
    if (!fault_ok)
        erase_probes(plan.probes, { "huge_zero_out" });

    // 2. Anonymous reverse-mapping (`rmap`) page and 2 MB THP installations:
    // - The kernel calls `folio_add_new_anon_rmap` (or `page_add_new_anon_rmap` on older
    //   kernels) whenever it maps a newly allocated anonymous page into a process, so we
    //   hook it to cover first-touch heap/stack/`MAP_ANONYMOUS` faults, Copy-on-Write
    //   faults, and new 2 MB anonymous THP faults.
    // - The kernel calls `folio_add_anon_rmap_ptes` and `folio_add_anon_rmap_pmd` (or
    //   `page_add_anon_rmap` on pre-6.8 kernels) whenever it binds an existing anonymous
    //   folio into PTEs or a 2 MB PMD, so we hook them to cover 2 MB anonymous THP
    //   collapses (`khugepaged` / `MADV_COLLAPSE`), swap-in faults, and KSM merges.
    std::string anon_new_folio = rmap_def("$arg1", "$arg2", "$arg3", kFolioOrdArg1);
    if (btf.has_func("folio_add_new_anon_rmap")) {
        btf.check_hook("folio_add_new_anon_rmap", "folio*, vm_area_struct*, u64",
                       "anon_new", anon_new_folio);
        btf.check_hook("folio_add_anon_rmap_ptes", "*, page*, u32, vm_area_struct*, u64",
                       "anon_ptes", rmap_def("$arg2", "$arg4", "$arg5", "nr=$arg3:u32"));
        btf.check_hook("folio_add_anon_rmap_pmd", "*, page*, vm_area_struct*, u64",
                       "anon_pmd", rmap_def("$arg2", "$arg3", "$arg4"));
    } else {
        std::string comp_u8 = rmap_def("$arg1", "$arg2", "$arg3", "comp=$arg4:u8");
        std::string comp_u32 = rmap_def("$arg1", "$arg2", "$arg3", "comp=$arg4:u32");
        btf.check_hook("page_add_new_anon_rmap",
                       { { "page_add_new_anon_rmap", "page*, vm_area_struct*, u64, u8",
                           "anon_new", comp_u8 },
                         { "page_add_new_anon_rmap", "page*, vm_area_struct*, u64",
                           "anon_new", anon_new_folio } });
        uint64_t rmap_comp = 0;
        plan.legacy_anon_compound_mask =
            (btf.find_enum_value("RMAP_COMPOUND", rmap_comp) && rmap_comp != 0)
            ? static_cast<uint32_t>(rmap_comp)
            : 2U;
        if (btf.check_hook("page_add_anon_rmap",
                           { { "page_add_anon_rmap", "page*, vm_area_struct*, u64, u8",
                               "anon_ptes", comp_u8 },
                             { "page_add_anon_rmap", "page*, vm_area_struct*, u64, u32",
                               "anon_ptes", comp_u32 } }) &&
            plan.probes.back().definition.find("comp=$arg4:u8") != std::string::npos) {
            plan.legacy_anon_compound_mask = 1;
        }
    }

    // 3. 4 KB file page-cache PTE installations:
    // The kernel calls `set_pte_range` (or `do_set_pte` / `alloc_set_pte` on older
    // kernels) whenever it maps one or more page-cache pages into 4 KB PTEs, so we
    // hook it to cover both single-page file demand faults and batched fault-around
    // mappings.
    btf.check_hook(
        "set_pte_range/do_set_pte/alloc_set_pte",
        { { "set_pte_range", "vm_fault*, *, page*, u32, u64", "file_pte",
            std::string(kVmfMm) + " page=$arg3:x64 nr=$arg4:u32 vaddr=$arg5:x64" },
          { "do_set_pte", "vm_fault*, page*, u64", "file_pte",
            std::string(kVmfMm) + " page=$arg2:x64 vaddr=$arg3:x64" },
          { "alloc_set_pte", "vm_fault*, page*", "file_pte", vmf_page_def("$arg2"), 2 },
          { "alloc_set_pte", "vm_fault*, *, page*", "file_pte",
            vmf_page_def("$arg3") } });

    // 4. 2 MB file-backed and shared-memory (`tmpfs`/`shmem`) THP PMD installations:
    // The kernel calls `do_set_pmd` (or `folio_add_file_rmap_pmd` / `page_add_file_rmap`
    // when `do_set_pmd` is inlined) whenever it installs a 2 MB file or shmem PMD, so we
    // hook it (paired with `handle_mm_fault` and `collapse_pte_mapped_thp` for the 2 MB
    // virtual address when `do_set_pmd` is inlined) to cover 2 MB file/shmem THP demand
    // faults and `khugepaged` / `MADV_COLLAPSE` file THP collapses.
    bool added_collapse = false;
    if (!btf.has_func("do_set_pmd") && btf.has_func("collapse_pte_mapped_thp")) {
        added_collapse =
            btf.check_hook("collapse_pte_mapped_thp", "mm_struct*, u64",
                           { { "collapse_in", "mm=$arg1:x64 vaddr=$arg2:x64" },
                             { "collapse_out", "", true } });
    }
    v2p_probe_spec_t pmd_out("set_pmd_out", "ret=$retval:u32", true, "ret == 0");
    const bool file_pmd_ok = btf.check_hook(
        "do_set_pmd/file_rmap_pmd",
        { { "do_set_pmd",
            "vm_fault*, page*",
            { { "set_pmd_in", vmf_page_def("$arg2") }, pmd_out },
            2 },
          { "do_set_pmd",
            "vm_fault*, folio*, *",
            { { "set_pmd_in", vmf_page_def("$arg2") }, pmd_out } },
          { "do_set_pmd",
            "vm_fault*, *, page*",
            { { "set_pmd_in", vmf_page_def("$arg3") }, pmd_out } },
          { "folio_add_file_rmap_pmd", "*, page*, vm_area_struct*", "file_rmap_pmd",
            "page=$arg2:x64 " + vma_mm("$arg3") },
          { "page_add_file_rmap", "page*, vm_area_struct*, u8", "file_rmap_pmd",
            "page=$arg1:x64 " + vma_mm("$arg2") + " comp=$arg3:u8", -1, "comp != 0" },
          { "page_add_file_rmap", "page*, u8", "file_rmap_pmd",
            "page=$arg1:x64 comp=$arg2:u8", -1, "comp != 0" } });
    if (added_collapse && !file_pmd_ok)
        erase_probes(plan.probes, { "collapse_in", "collapse_out" });

    // 5. Physical page migration:
    // The kernel calls `remove_migration_ptes` whenever it finishes copying a physical
    // folio to a new frame and rewrites PTEs to point to the destination, so we hook it
    // to cover physical page migrations across memory compaction, NUMA balancing,
    // `move_pages`, and memory tiering.
    btf.check_hook("remove_migration_ptes", "folio|page*, folio|page*", "mig_ptes",
                   std::string("src=$arg1:x64 dst=$arg2:x64 ") + kFolioOrdArg1);

    // 6. Virtual address relocation (`mremap`):
    // The kernel calls `move_page_tables` whenever `mremap` moves PTEs/PMDs to a new
    // virtual address without changing their physical frames, so we bracket it (using
    // `pagetable_move_control` on Linux 6.13+ or the 5-arg signature on earlier
    // kernels) and clamp to its `moved` return byte count.
    v2p_probe_spec_t move_out("move_pt_out", "moved=$retval:u64", true);
    btf.check_hook(
        "move_page_tables",
        { { "move_page_tables",
            "pagetable_move_control*",
            { { "move_pt_in",
                "mm={vm_area_struct.vm_mm:mm_struct*}({pagetable_move_control.old:"
                "vm_area_struct*}($arg1)):x64 "
                "old_addr={pagetable_move_control.old_addr:u64}($arg1):x64 "
                "new_addr={pagetable_move_control.new_addr:u64}($arg1):x64 "
                "len={pagetable_move_control.len_in:u64}($arg1):u64" },
              move_out },
            1 },
          { "move_page_tables",
            "vm_area_struct*, u64, *, u64, u64",
            { { "move_pt_in",
                vma_mm("$arg1") +
                    " old_addr=$arg2:x64 new_addr=$arg4:x64 len=$arg5:u64" },
              move_out } } });

    // 7. Explicit virtual range invalidation:
    // The kernel calls `unmap_page_range` (or `unmap_vmas` and `zap_vma_range_batched`
    // on Linux 6.12+ where `unmap_page_range` is inlined) whenever it tears down
    // virtual page-table entries for a range, so we hook it to cover explicit unmappings
    // (`munmap`, `MADV_DONTNEED`, `MADV_FREE`, `MAP_FIXED` overwrites, and file
    // truncation/hole-punching).
    if (btf.has_func("unmap_page_range")) {
        btf.check_hook("unmap_page_range", "mmu_gather*, *, u64, u64", "unmap_range",
                       std::string(kTlbMm) + "start=$arg3:x64 end=$arg4:x64");
    } else {
        btf.check_hook("unmap_vmas", "mmu_gather*, unmap_desc*", "unmap_vmas",
                       std::string(kTlbMm) +
                           "start={unmap_desc.vma_start:u64}($arg2):x64 "
                           "end={unmap_desc.vma_end:u64}($arg2):x64");
        btf.check_hook("zap_vma_range_batched", "mmu_gather*, *, u64, u64", "unmap_zap",
                       std::string(kTlbMm) + "start=$arg3:x64 size=$arg4:u64");
    }

    // 8. Asynchronous kernel reclaim and swap-out eviction:
    // The kernel calls `try_to_unmap_one` whenever memory reclaim (`kswapd`, direct
    // reclaim, `MADV_PAGEOUT`, or page-cache eviction) unmaps a folio from a VMA, so
    // we bracket it to unmap pages when `ret != 0`. Pre-5.14 kernels shared
    // `try_to_unmap_one` with migration and `munlock` before `try_to_migrate_one` was
    // split out, so ignore calls carrying `TTU_MIGRATION | TTU_MUNLOCK` there.
    std::string try_to_unmap_sig = "folio|page*, vm_area_struct*, u64";
    std::string try_to_unmap_extra;
    if (!btf.has_func("try_to_migrate_one")) {
        uint64_t mig = 0x1, munlock = 0x2;
        btf.find_enum_value("TTU_MIGRATION", mig);
        btf.find_enum_value("TTU_MUNLOCK", munlock);
        plan.ttu_ignore_arg4_mask = static_cast<uint32_t>(mig | munlock);
        try_to_unmap_sig += ", *";
        try_to_unmap_extra = " ttu_flags=$arg4:x64";
    }
    btf.check_hook(
        "try_to_unmap_one", std::move(try_to_unmap_sig),
        { { "ttu_in",
            rmap_def("$arg1", "$arg2", "$arg3", kFolioOrdArg1) +
                " vm_end={vm_area_struct.vm_end:u64}($arg2):x64" + try_to_unmap_extra },
          { "ttu_out", "ret=$retval:u8", true, "ret != 0" } });

    // 9. 2 MB transparent hugepage PMD split:
    // The kernel calls `__split_huge_pmd` whenever it breaks a 2 MB PMD mapping down
    // into 4 KB PTEs (or zaps a file/shmem PMD), so we hook it to cover in-place
    // anonymous 2 MB THP splits (`V2P_RECORD_SPLIT`) and file/shmem PMD invalidations
    // (`V2P_RECORD_UNMAP`).
    btf.check_hook("__split_huge_pmd", "vm_area_struct*, *, u64", "split_pmd",
                   vma_mm("$arg1") +
                       " vm_ops={vm_area_struct.vm_ops:vm_operations_struct*}($arg1):x64 "
                       "vaddr=$arg3:x64");

    // 10. Explicit hugepage pools (`hugetlbfs`).
    check_hugetlb_hooks(btf, plan);
}

/* -------------------------------------------------------------------------
 * Tracefs Event Parsing & Transition Handlers
 * ------------------------------------------------------------------------- */

// Wraps a single `trace_pipe` line starting at `pos` (`<event_name>_<pid>: ...`),
// providing event classification, `mm` matching, `key=value` extraction, and PADDRs.
struct v2p_tracefs_hooks_t::trace_event_t {
    const std::string &line;
    size_t pos;
    pid_t tid;
    uint64_t ts;
    bool in_irq;

    // Extracts the event name before `:`, strips the per-process `_<pid>` suffix that
    // `v2p_tracker_t` appends to dynamic `kprobe_events`, and looks up its enum.
    event_type_t
    parse_event_type() const
    {
        size_t colon = line.find(':', pos);
        if (colon == std::string::npos)
            return event_type_t::kUnknown;
        std::string name = line.substr(pos, colon - pos);
        size_t pid_sep = name.rfind('_');
        if (pid_sep != std::string::npos && pid_sep + 1 < name.size() &&
            isdigit(static_cast<unsigned char>(name[pid_sep + 1]))) {
            name.resize(pid_sep);
        }
        const auto &map = probe_name_map();
        auto it = map.find(name);
        return it != map.end() ? it->second : event_type_t::kUnknown;
    }

    bool
    extract_field(const char *key, uint64_t &out, int base = 0) const
    {
        return v2p_extract_u64_field(line, key, out, base, pos);
    }

    uint64_t
    extract_field_or(const char *key, uint64_t default_val = 0) const
    {
        uint64_t val = default_val;
        extract_field(key, val, 0);
        return val;
    }

    bool
    is_target_mm(v2p_tracker_t &tracker) const
    {
        uint64_t mm = 0;
        return extract_field("mm=", mm, 16) && tracker.is_tracked_mm(mm, tid);
    }

    bool
    extract_page_paddr(v2p_tracker_t &tracker, uint64_t &paddr_out,
                       const char *key = "page=") const
    {
        uint64_t ptr = 0;
        return extract_field(key, ptr, 16) && tracker.page_ptr_to_paddr(ptr, paddr_out);
    }

    uint32_t
    extract_folio_pages(uint32_t pg_head_bit) const
    {
        uint64_t flags = 0;
        extract_field("flags=", flags, 16);
        return v2p_folio_order_pages(flags, extract_field_or("ord="), pg_head_bit);
    }

    // Parses `start=` plus `end=` (or `size=`) into overflow-safe 4 KB-aligned `[lo,
    // hi)`.
    bool
    extract_unmap_bounds(uint64_t &lo_out, uint64_t &hi_out) const
    {
        uint64_t start = 0, end = 0, size = 0;
        if (!extract_field("start=", start, 16))
            return false;
        if (!extract_field("end=", end, 16)) {
            if (!extract_field("size=", size) || size == 0)
                return false;
            end = (size > (std::numeric_limits<uint64_t>::max)() - start)
                ? (std::numeric_limits<uint64_t>::max)()
                : start + size;
        }
        if (end <= start)
            return false;
        constexpr uint64_t kMaxAligned =
            (std::numeric_limits<uint64_t>::max)() & ~(kPageSize4K - 1);
        lo_out = start & ~(kPageSize4K - 1);
        hi_out = (end > kMaxAligned) ? kMaxAligned
                                     : ((end + kPageSize4K - 1) & ~(kPageSize4K - 1));
        return hi_out > lo_out;
    }
};

void
v2p_tracefs_hooks_t::clear()
{
    pending_faults_.clear();
    pending_set_pmd_.clear();
    pending_collapse_vaddr_.clear();
    pending_moves_.clear();
    pending_try_to_unmap_.clear();
    pending_hugetlb_.clear();
}

void
v2p_tracefs_hooks_t::mark_fault_handled(pid_t tid)
{
    auto it = pending_faults_.find(tid);
    if (it != pending_faults_.end())
        it->second.handled = true;
}

// Commits a single-event `(mm, vaddr, page)` PTE/PMD installation and marks any
// enclosing `handle_mm_fault` on `trace_event.tid` as handled.
bool
v2p_tracefs_hooks_t::install_pages(const trace_event_t &trace_event,
                                   v2p_tracker_t &tracker, uint32_t nr, bool is_huge_2m)
{
    uint64_t vaddr = 0, paddr = 0;
    if (nr > 0 && trace_event.is_target_mm(tracker) &&
        trace_event.extract_field("vaddr=", vaddr, 16) &&
        trace_event.extract_page_paddr(tracker, paddr)) {
        uint64_t mask = is_huge_2m ? (kPageSize2M - 1) : (kPageSize4K - 1);
        tracker.bind_pages(trace_event.ts, vaddr & ~mask, paddr & ~mask, nr, is_huge_2m);
        mark_fault_handled(trace_event.tid);
    }
    return true;
}

// Binds a power-of-two `hugetlbfs` pool page (`64 KB`, `2 MB`, `32 MB`, or `1 GB`).
void
v2p_tracefs_hooks_t::bind_hugetlb(uint64_t ts, pid_t tid, uint64_t vaddr, uint64_t paddr,
                                  uint32_t nr, v2p_tracker_t &tracker)
{
    uint64_t mask = static_cast<uint64_t>(nr) * kPageSize4K - 1;
    tracker.bind_pages(ts, vaddr & ~mask, paddr & ~mask, nr, /*is_huge_2m=*/false);
    mark_fault_handled(tid);
}

bool
v2p_tracefs_hooks_t::handle_event(const std::string &line, size_t event_pos, pid_t tid,
                                  uint64_t timestamp_us, const v2p_kernel_plan_t &plan,
                                  v2p_tracker_t &tracker, bool in_irq)
{
    const trace_event_t trace_event { line, event_pos, tid, timestamp_us, in_irq };

    switch (trace_event.parse_event_type()) {
    case event_type_t::kUnknown: return false;

    // 1. Anonymous reverse-mapping (`rmap`) page and 2 MB THP installations.
    case event_type_t::kAnonRmapPmd:
        return install_pages(trace_event, tracker, kPagesPer2M, /*is_huge_2m=*/true);
    case event_type_t::kFilePte: {
        if (trace_event.is_target_mm(tracker))
            pending_collapse_vaddr_.erase(tid);
        uint32_t nr = static_cast<uint32_t>(trace_event.extract_field_or("nr=", 1));
        return install_pages(trace_event, tracker, nr, /*is_huge_2m=*/false);
    }
    case event_type_t::kAnonRmapNew: {
        uint64_t comp = 0;
        uint32_t nr = trace_event.extract_field("comp=", comp)
            ? (comp != 0 ? kPagesPer2M : 1)
            : trace_event.extract_folio_pages(plan.pg_head_bit);
        return install_pages(trace_event, tracker, nr,
                             /*is_huge_2m=*/(nr == kPagesPer2M));
    }
    case event_type_t::kAnonRmapPtes: {
        bool huge =
            (trace_event.extract_field_or("comp=") & plan.legacy_anon_compound_mask) != 0;
        uint32_t nr = huge
            ? kPagesPer2M
            : static_cast<uint32_t>(trace_event.extract_field_or("nr=", 1));
        return install_pages(trace_event, tracker, nr, huge);
    }

    // 2. File/shmem 2 MB THP PMD installations: `do_set_pmd` commits when `ret == 0`;
    //    when inlined, `file_rmap_pmd` pairs with `pending_collapse_vaddr_`.
    case event_type_t::kDoSetPmdEntry: {
        uint64_t vaddr = 0, paddr = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vaddr=", vaddr, 16) &&
            trace_event.extract_page_paddr(tracker, paddr)) {
            pending_set_pmd_[tid] = { vaddr & ~(kPageSize2M - 1),
                                      paddr & ~(kPageSize2M - 1) };
        }
        return true;
    }
    case event_type_t::kDoSetPmdReturn: {
        pending_pmd_t pmd;
        uint64_t ret = 1;
        if (pop_pending(pending_set_pmd_, tid, pmd) &&
            trace_event.extract_field("ret=", ret) && ret == 0) {
            tracker.bind_pages(trace_event.ts, pmd.vaddr, pmd.paddr, kPagesPer2M,
                               /*is_huge_2m=*/true);
            mark_fault_handled(tid);
        }
        return true;
    }
    // Shared zero-page read faults (`vm_ops == 0` returning `ret == 0` without calling
    // an rmap hook) and file-VMA fault context (`vm_ops != 0`).
    case event_type_t::kHandleMmFaultEntry: {
        uint64_t vm_ops = 0, vaddr = 0, flags = 0;
        if (!trace_event.is_target_mm(tracker) ||
            !trace_event.extract_field("vm_ops=", vm_ops, 16) ||
            !trace_event.extract_field("vaddr=", vaddr, 16)) {
            return true;
        }
        if (vm_ops == 0) {
            pending_collapse_vaddr_.erase(tid);
            if (trace_event.extract_field("flags=", flags)) {
                pending_faults_[tid] = { vaddr, 0, (flags & plan.fault_flag_write) != 0,
                                         false };
            }
        } else {
            pending_faults_.erase(tid);
            pending_collapse_vaddr_[tid] = vaddr & ~(kPageSize2M - 1);
        }
        return true;
    }
    case event_type_t::kGetHugeZeroReturn: {
        auto it = pending_faults_.find(tid);
        uint64_t ptr = 0;
        if (it != pending_faults_.end() && trace_event.extract_field("ret=", ptr, 16) &&
            ptr != 0) {
            it->second.huge_zero_ptr = ptr;
        }
        return true;
    }
    case event_type_t::kHandleMmFaultReturn: {
        pending_collapse_vaddr_.erase(tid);
        pending_fault_t fault;
        uint64_t ret = 1, paddr = 0;
        if (!pop_pending(pending_faults_, tid, fault) ||
            !trace_event.extract_field("ret=", ret) || ret != 0 || fault.is_write ||
            fault.handled) {
            return true;
        }
        if (fault.huge_zero_ptr != 0 &&
            tracker.page_ptr_to_paddr(fault.huge_zero_ptr, paddr)) {
            uint64_t aligned_2m = fault.vaddr & ~(kPageSize2M - 1);
            if (!tracker.has_shadow_mapping(aligned_2m, kPagesPer2M)) {
                tracker.bind_pages(trace_event.ts, aligned_2m, paddr & ~(kPageSize2M - 1),
                                   kPagesPer2M, /*is_huge_2m=*/true,
                                   /*is_huge_zero=*/true);
            }
        } else {
            uint64_t vpage = fault.vaddr & ~(kPageSize4K - 1);
            if (tracker.zero_pfn() != 0 && !tracker.has_shadow_mapping(vpage)) {
                tracker.bind_pages(trace_event.ts, vpage,
                                   tracker.zero_pfn() * kPageSize4K, 1,
                                   /*is_huge_2m=*/false, /*is_huge_zero=*/false);
            }
        }
        return true;
    }
    case event_type_t::kCollapseThpReturn:
        pending_collapse_vaddr_.erase(tid);
        return true;
    case event_type_t::kCollapseThpEntry: {
        uint64_t vaddr = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vaddr=", vaddr, 16)) {
            pending_collapse_vaddr_[tid] = vaddr & ~(kPageSize2M - 1);
        }
        return true;
    }
    case event_type_t::kFileRmapPmd: {
        auto it = pending_collapse_vaddr_.find(tid);
        uint64_t mm = 0, paddr = 0;
        if (it != pending_collapse_vaddr_.end() &&
            (!trace_event.extract_field("mm=", mm, 16) ||
             tracker.is_tracked_mm(mm, tid)) &&
            trace_event.extract_page_paddr(tracker, paddr)) {
            tracker.bind_pages(trace_event.ts, it->second, paddr & ~(kPageSize2M - 1),
                               kPagesPer2M, /*is_huge_2m=*/true);
            pending_collapse_vaddr_.erase(it);
        }
        return true;
    }

    // 3. Physical page migration (`remove_migration_ptes`): rebinds spans mapped to
    // `src`.
    case event_type_t::kRemoveMigrationPtes: {
        uint64_t src_pa = 0, dst_pa = 0;
        if (trace_event.extract_page_paddr(tracker, src_pa, "src=") &&
            trace_event.extract_page_paddr(tracker, dst_pa, "dst=") && src_pa != dst_pa) {
            tracker.rebind_migrated_folio(
                trace_event.ts, src_pa, dst_pa,
                trace_event.extract_folio_pages(plan.pg_head_bit));
        }
        return true;
    }

    // 4. Virtual address relocation (`move_page_tables`): clamp to the actual `moved`
    //    return byte count in case `mremap` stopped partway through `[old_addr, +len)`.
    case event_type_t::kMovePageTablesEntry: {
        uint64_t old_va = 0, new_va = 0, len = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("old_addr=", old_va, 16) &&
            trace_event.extract_field("new_addr=", new_va, 16) &&
            trace_event.extract_field("len=", len) && old_va != new_va &&
            len >= kPageSize4K) {
            pending_moves_[tid] = { old_va, new_va, len };
        }
        return true;
    }
    case event_type_t::kMovePageTablesReturn: {
        pending_move_t move;
        uint64_t moved = 0;
        if (pop_pending(pending_moves_, tid, move) &&
            trace_event.extract_field("moved=", moved)) {
            uint64_t bytes = std::min(moved, move.len) & ~(kPageSize4K - 1);
            if (bytes >= kPageSize4K) {
                tracker.relocate_range(trace_event.ts, move.old_addr & ~(kPageSize4K - 1),
                                       move.new_addr & ~(kPageSize4K - 1), bytes);
            }
        }
        return true;
    }

    // 5. Mapping invalidations: explicit virtual unmaps and `vm_end`-clamped reclaim
    //    evictions (`try_to_unmap_one` unmaps only pages still backed by `folio_paddr`
    //    so private Copy-on-Write pages in the same VMA are preserved).
    case event_type_t::kUnmapPageRange:
    case event_type_t::kUnmapVmas:
    case event_type_t::kZapVmaRange: {
        uint64_t lo = 0, hi = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_unmap_bounds(lo, hi)) {
            tracker.unbind_range(trace_event.ts, lo, hi);
        }
        return true;
    }
    case event_type_t::kTryToUnmapEntry: {
        pending_try_to_unmap_.erase(tid);
        uint64_t vaddr = 0, folio_pa = 0, ttu_flags = 0, vm_end = 0;
        if (!trace_event.is_target_mm(tracker) ||
            !trace_event.extract_field("vaddr=", vaddr, 16) ||
            !trace_event.extract_page_paddr(tracker, folio_pa) ||
            (plan.ttu_ignore_arg4_mask != 0 &&
             trace_event.extract_field("ttu_flags=", ttu_flags, 16) &&
             (ttu_flags & plan.ttu_ignore_arg4_mask) != 0)) {
            return true;
        }
        uint32_t nr = trace_event.extract_folio_pages(plan.pg_head_bit);
        uint64_t aligned_va = vaddr & ~(kPageSize4K - 1);
        uint32_t max_pages = nr;
        if (trace_event.extract_field("vm_end=", vm_end, 16) && vm_end > 0) {
            if (vm_end <= aligned_va)
                return true;
            max_pages = static_cast<uint32_t>(
                std::min<uint64_t>(max_pages, (vm_end - aligned_va) / kPageSize4K));
        }
        pending_try_to_unmap_[tid] = { aligned_va, folio_pa, max_pages, nr };
        return true;
    }
    case event_type_t::kTryToUnmapReturn: {
        pending_try_to_unmap_t unmap;
        uint64_t ret = 0;
        if (pop_pending(pending_try_to_unmap_, tid, unmap) &&
            trace_event.extract_field("ret=", ret) && ret != 0) {
            tracker.unbind_folio_in_range(trace_event.ts, unmap.aligned_vaddr,
                                          unmap.max_pages, unmap.folio_paddr,
                                          unmap.folio_pages);
        }
        return true;
    }

    // 6. 2 MB PMD split (`split_pmd`): `vm_ops == 0` identifies anonymous VMAs.
    case event_type_t::kSplitHugePmd: {
        uint64_t vm_ops = 0, vaddr = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vm_ops=", vm_ops, 16) &&
            trace_event.extract_field("vaddr=", vaddr, 16)) {
            tracker.split_huge_pmd(trace_event.ts, vaddr & ~(kPageSize2M - 1),
                                   /*is_anon=*/(vm_ops == 0));
        }
        return true;
    }

    // 7. Explicit hugepage pools (`hugetlbfs`): private CoW faults commit via
    //    `hugetlb_anon`, while shared (`VM_SHARED`) and file-borrowed read faults
    //    unlock the pool folio in process context (`!in_irq`) before `hugetlb_fault`.
    case event_type_t::kHugetlbFaultEntry: {
        if (!plan.has_hugetlb)
            return false;
        pending_hugetlb_.erase(tid);
        uint64_t vm_flags = 0, vaddr = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vm_flags=", vm_flags, 16) &&
            trace_event.extract_field("vaddr=", vaddr, 16)) {
            pending_hugetlb_[tid] = { vaddr, vm_flags, 0, 0, false };
        }
        return true;
    }
    case event_type_t::kHugetlbAnonRmap: {
        if (!plan.has_hugetlb)
            return false;
        uint64_t vaddr = 0, paddr = 0;
        uint32_t nr = trace_event.extract_folio_pages(plan.pg_head_bit);
        if (is_supported_hugetlb_pages(nr) && trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vaddr=", vaddr, 16) &&
            trace_event.extract_page_paddr(tracker, paddr)) {
            bind_hugetlb(trace_event.ts, tid, vaddr, paddr, nr, tracker);
            auto it = pending_hugetlb_.find(tid);
            if (it != pending_hugetlb_.end())
                it->second.saw_anon_rmap = true;
        }
        return true;
    }
    case event_type_t::kHugetlbFolioUnlock: {
        if (!plan.has_hugetlb)
            return false;
        auto it = pending_hugetlb_.find(tid);
        uint64_t paddr = 0;
        uint32_t nr = trace_event.extract_folio_pages(plan.pg_head_bit);
        if (!trace_event.in_irq && it != pending_hugetlb_.end() &&
            is_supported_hugetlb_pages(nr) &&
            trace_event.extract_page_paddr(tracker, paddr)) {
            it->second.folio_paddr = paddr;
            it->second.folio_pages = nr;
        }
        return true;
    }
    case event_type_t::kHugetlbFaultReturn: {
        if (!plan.has_hugetlb)
            return false;
        pending_hugetlb_t hf;
        uint64_t ret = plan.vm_fault_error_mask;
        if (!pop_pending(pending_hugetlb_, tid, hf) ||
            !trace_event.extract_field("ret=", ret) ||
            (ret & plan.vm_fault_error_mask) != 0 || hf.saw_anon_rmap ||
            !is_supported_hugetlb_pages(hf.folio_pages)) {
            return true;
        }
        uint64_t huge_bytes = static_cast<uint64_t>(hf.folio_pages) * kPageSize4K;
        if ((hf.vm_flags & plan.vm_shared_mask) != 0 ||
            !tracker.has_shadow_mapping(hf.vaddr & ~(huge_bytes - 1), hf.folio_pages)) {
            bind_hugetlb(trace_event.ts, tid, hf.vaddr, hf.folio_paddr, hf.folio_pages,
                         tracker);
        }
        return true;
    }
    case event_type_t::kHugetlbUnmap: {
        if (!plan.has_hugetlb)
            return false;
        uint64_t lo = 0, hi = 0, ref = 0, ref_pa = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_unmap_bounds(lo, hi) &&
            trace_event.extract_field("ref=", ref, 16)) {
            if (ref == 0) {
                tracker.unbind_range(trace_event.ts, lo, hi);
            } else if ((hi - lo) / kPageSize4K <=
                           (std::numeric_limits<uint32_t>::max)() &&
                       tracker.page_ptr_to_paddr(ref, ref_pa)) {
                uint32_t pages = static_cast<uint32_t>((hi - lo) / kPageSize4K);
                tracker.unbind_folio_in_range(trace_event.ts, lo, pages, ref_pa, pages);
            }
        }
        return true;
    }
    }

    return false;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
