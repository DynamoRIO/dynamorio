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
        { "set_pmd_in", event_type_t::kDoSetPmdEntry },
        { "set_pmd_out", event_type_t::kDoSetPmdReturn },
        { "fault_in", event_type_t::kHandleMmFaultEntry },
        { "fault_out", event_type_t::kHandleMmFaultReturn },
        { "collapse_in", event_type_t::kCollapseThpEntry },
        { "file_rmap_pmd", event_type_t::kFileRmapPmd },
        { "collapse_out", event_type_t::kCollapseThpReturn },
        { "unmap_range", event_type_t::kUnmapPageRange },
        { "unmap_vmas", event_type_t::kUnmapVmas },
        { "unmap_zap", event_type_t::kZapVmaRange },
        { "split_pmd", event_type_t::kSplitHugePmd },
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

} // namespace

void
v2p_tracefs_hooks_t::check_all_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan)
{
    // 1. Anonymous reverse-mapping (`rmap`) page and 2 MB THP installations:
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

    // 2. 2 MB file-backed and shared-memory (`tmpfs`/`shmem`) THP PMD installations:
    // The kernel calls `do_set_pmd` (or `folio_add_file_rmap_pmd` / `page_add_file_rmap`
    // when `do_set_pmd` is inlined) whenever it installs a 2 MB file or shmem PMD, so we
    // hook it (paired with `handle_mm_fault` and `collapse_pte_mapped_thp` for the 2 MB
    // virtual address when `do_set_pmd` is inlined) to cover 2 MB file/shmem THP demand
    // faults and `khugepaged` / `MADV_COLLAPSE` file THP collapses.
    bool added_fallback_ctx = false;
    if (!btf.has_func("do_set_pmd")) {
        added_fallback_ctx = btf.check_hook(
            "handle_mm_fault", "vm_area_struct*, u64, u32",
            { { "fault_in",
                vma_mm("$arg1") +
                    " vm_ops={vm_area_struct.vm_ops:vm_operations_struct*}($arg1):x64 "
                    "vaddr=$arg2:x64" },
              { "fault_out", "", true } });
        if (btf.has_func("collapse_pte_mapped_thp")) {
            added_fallback_ctx =
                btf.check_hook("collapse_pte_mapped_thp", "mm_struct*, u64",
                               { { "collapse_in", "mm=$arg1:x64 vaddr=$arg2:x64" },
                                 { "collapse_out", "", true } }) ||
                added_fallback_ctx;
        }
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
    if (added_fallback_ctx && !file_pmd_ok) {
        erase_probes(plan.probes,
                     { "fault_in", "fault_out", "collapse_in", "collapse_out" });
    }

    // 3. Explicit virtual range invalidation:
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

    // 4. 2 MB transparent hugepage PMD split:
    // The kernel calls `__split_huge_pmd` whenever it breaks a 2 MB PMD mapping down
    // into 4 KB PTEs (or zaps a file/shmem PMD), so we hook it to cover in-place
    // anonymous 2 MB THP splits (`V2P_RECORD_SPLIT`) and file/shmem PMD invalidations
    // (`V2P_RECORD_UNMAP`).
    btf.check_hook("__split_huge_pmd", "vm_area_struct*, *, u64", "split_pmd",
                   vma_mm("$arg1") +
                       " vm_ops={vm_area_struct.vm_ops:vm_operations_struct*}($arg1):x64 "
                       "vaddr=$arg3:x64");
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
    pending_set_pmd_.clear();
    pending_collapse_vaddr_.clear();
}

// Commits a single-event `(mm, vaddr, page)` PTE/PMD installation.
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
    }
    return true;
}

bool
v2p_tracefs_hooks_t::handle_event(const std::string &line, size_t event_pos, pid_t tid,
                                  uint64_t timestamp_us, const v2p_kernel_plan_t &plan,
                                  v2p_tracker_t &tracker)
{
    const trace_event_t trace_event { line, event_pos, tid, timestamp_us };

    switch (trace_event.parse_event_type()) {
    case event_type_t::kUnknown: return false;

    // 1. Anonymous reverse-mapping (`rmap`) page and 2 MB THP installations.
    case event_type_t::kAnonRmapPmd:
        return install_pages(trace_event, tracker, kPagesPer2M, /*is_huge_2m=*/true);
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
        }
        return true;
    }
    case event_type_t::kHandleMmFaultEntry: {
        uint64_t vm_ops = 0, vaddr = 0;
        if (trace_event.is_target_mm(tracker) &&
            trace_event.extract_field("vm_ops=", vm_ops, 16) &&
            trace_event.extract_field("vaddr=", vaddr, 16)) {
            if (vm_ops != 0)
                pending_collapse_vaddr_[tid] = vaddr & ~(kPageSize2M - 1);
            else
                pending_collapse_vaddr_.erase(tid);
        }
        return true;
    }
    case event_type_t::kHandleMmFaultReturn:
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

    // 3. Explicit virtual range invalidations (`unmap_range`, `unmap_vmas`, `unmap_zap`).
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

    // 4. 2 MB PMD split (`split_pmd`): `vm_ops == 0` identifies anonymous VMAs.
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
    }

    return false;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
