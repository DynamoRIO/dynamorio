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

#    include <ctype.h>
#    include <limits>
#    include <unordered_map>

#    include "v2p_tracker.h"

namespace dynamorio {
namespace drmemtrace {
namespace {

using event_type_t = v2p_tracefs_hooks_t::event_type_t;

/* -------------------------------------------------------------------------
 * Kernel Hook Selection (BTF Signature & Offset Resolution)
 * ------------------------------------------------------------------------- */

constexpr const char *kFolioOrdArg1 = "flags=+0($arg1):x64 ord={folio_order}($arg1):u8";

// Maps each base kprobe name (`v2p_probe_spec_t::base_name`) to its `event_type_t`.
const std::unordered_map<std::string, event_type_t> &
probe_name_map()
{
    static const std::unordered_map<std::string, event_type_t> kMap = {
        { "anon_new", event_type_t::kAnonRmapNew },
        { "unmap_range", event_type_t::kUnmapPageRange },
        { "unmap_vmas", event_type_t::kUnmapVmas },
        { "unmap_zap", event_type_t::kZapVmaRange },
        { "split_pmd", event_type_t::kSplitHugePmd },
    };
    return kMap;
}

} // namespace

void
v2p_tracefs_hooks_t::check_all_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan)
{
    (void)plan;

    // 1. Anonymous / Copy-on-Write page-fault installation:
    // The kernel calls `folio_add_new_anon_rmap` (or `page_add_new_anon_rmap` on older
    // kernels) whenever it maps a newly allocated anonymous page into a process, so we
    // hook it to cover first-touch heap/stack/`MAP_ANONYMOUS` faults, Copy-on-Write
    // faults, and new 2 MB anonymous THP faults.
    std::string anon_folio =
        std::string("page=$arg1:x64 vaddr=$arg3:x64 ") + kFolioOrdArg1;
    if (btf.has_func("folio_add_new_anon_rmap")) {
        btf.check_hook("folio_add_new_anon_rmap", "folio*, vm_area_struct*, u64",
                       "anon_new", anon_folio);
    } else {
        btf.check_hook("page_add_new_anon_rmap",
                       { { "page_add_new_anon_rmap", "page*, vm_area_struct*, u64, u8",
                           "anon_new", "page=$arg1:x64 vaddr=$arg3:x64 comp=$arg4:u8" },
                         { "page_add_new_anon_rmap", "page*, vm_area_struct*, u64",
                           "anon_new", anon_folio } });
    }

    // 2. Explicit virtual range invalidation:
    // The kernel calls `unmap_page_range` (or `unmap_vmas` and `zap_vma_range_batched`
    // on Linux 6.12+ where `unmap_page_range` is inlined) whenever it tears down
    // virtual page-table entries for a range, so we hook it to cover explicit unmappings
    // (`munmap`, `MADV_DONTNEED`, `MADV_FREE`, `MAP_FIXED` overwrites, and file
    // truncation/hole-punching).
    if (btf.has_func("unmap_page_range")) {
        btf.check_hook("unmap_page_range", "mmu_gather*, *, u64, u64", "unmap_range",
                       "start=$arg3:x64 end=$arg4:x64");
    } else {
        btf.check_hook("unmap_vmas", "mmu_gather*, unmap_desc*", "unmap_vmas",
                       "start={unmap_desc.vma_start:u64}($arg2):x64 "
                       "end={unmap_desc.vma_end:u64}($arg2):x64");
        btf.check_hook("zap_vma_range_batched", "mmu_gather*, *, u64, u64", "unmap_zap",
                       "start=$arg3:x64 size=$arg4:u64");
    }

    // 3. 2 MB transparent hugepage PMD split:
    // The kernel calls `__split_huge_pmd` whenever it breaks a 2 MB PMD mapping down
    // into 4 KB PTEs (or zaps a file/shmem PMD), so we hook it to cover in-place
    // anonymous 2 MB THP splits (`V2P_RECORD_SPLIT`) and file/shmem PMD invalidations
    // (`V2P_RECORD_UNMAP`).
    btf.check_hook("__split_huge_pmd", "vm_area_struct*, *, u64", "split_pmd",
                   "vm_ops={vm_area_struct.vm_ops:vm_operations_struct*}($arg1):x64 "
                   "vaddr=$arg3:x64");
}

/* -------------------------------------------------------------------------
 * Tracefs Event Parsing & Transition Handlers
 * ------------------------------------------------------------------------- */

// Wraps a single `trace_pipe` line starting at `pos` (`<event_name>_<pid>: ...`),
// providing event classification, `key=value` extraction, and PADDR conversion.
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

bool
v2p_tracefs_hooks_t::handle_event(const std::string &line, size_t event_pos, pid_t tid,
                                  uint64_t timestamp_us, const v2p_kernel_plan_t &plan,
                                  v2p_tracker_t &tracker)
{
    const trace_event_t trace_event { line, event_pos, tid, timestamp_us };

    switch (trace_event.parse_event_type()) {
    case event_type_t::kUnknown: return false;

    // 1. Anonymous / Copy-on-Write page fault (`anon_new`).
    case event_type_t::kAnonRmapNew: {
        uint64_t vaddr = 0, paddr = 0, comp = 0;
        if (!trace_event.extract_field("vaddr=", vaddr, 16) ||
            !trace_event.extract_page_paddr(tracker, paddr)) {
            return true;
        }
        uint32_t nr = trace_event.extract_field("comp=", comp)
            ? (comp != 0 ? kPagesPer2M : 1)
            : trace_event.extract_folio_pages(plan.pg_head_bit);
        bool is_2m = (nr == kPagesPer2M);
        uint64_t mask = (is_2m ? kPageSize2M : kPageSize4K) - 1;
        tracker.bind_pages(trace_event.ts, vaddr & ~mask, paddr & ~mask, nr, is_2m);
        return true;
    }

    // 2. Explicit virtual range invalidations (`unmap_range`, `unmap_vmas`, `unmap_zap`).
    case event_type_t::kUnmapPageRange:
    case event_type_t::kUnmapVmas:
    case event_type_t::kZapVmaRange: {
        uint64_t lo = 0, hi = 0;
        if (trace_event.extract_unmap_bounds(lo, hi))
            tracker.unbind_range(trace_event.ts, lo, hi);
        return true;
    }

    // 3. 2 MB PMD split (`split_pmd`): `vm_ops == 0` identifies anonymous VMAs.
    case event_type_t::kSplitHugePmd: {
        uint64_t vm_ops = 0, vaddr = 0;
        if (trace_event.extract_field("vm_ops=", vm_ops, 16) &&
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
