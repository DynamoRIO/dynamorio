/* **********************************************************
 * Copyright (c) 2024-2026 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "v2p_reader_unit_test.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <vector>

#include "reader/v2p_reader.h"
#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)
#    include <unistd.h>
#    include "tracer/v2p_tracefs/v2p_tracker.h"
#endif

namespace dynamorio {
namespace drmemtrace {

static void
check_v2p_info(const v2p_info_t &v2p_info)
{
    // Change NUM_ENTRIES if clients/drcachesim/tests/v2p_example.textproto is updated.
    // Must be equal to the number of "address_mapping {...}" blocks in the textproto.
    constexpr size_t NUM_ENTRIES = 4;
    if (v2p_info.v2p_map.size() != NUM_ENTRIES) {
        std::cerr << "v2p_map incorrect number of entries. Expected " << NUM_ENTRIES
                  << " got " << v2p_info.v2p_map.size() << ".\n";
        exit(1);
    }

    // Virtual and physical addresses must be aligned with
    // clients/drcachesim/tests/v2p_example.textproto.
    const std::vector<addr_t> virtual_addresses = { 0x123, 0x456, 0x0, 0x789 };
    const std::vector<addr_t> physical_addresses = { 0x3, 0x4, 0x6, 0x5 };
    for (size_t i = 0; i < virtual_addresses.size(); ++i) {
        auto key_val = v2p_info.v2p_map.find(virtual_addresses[i]);
        if (key_val != v2p_info.v2p_map.end()) {
            if (key_val->second != physical_addresses[i]) {
                std::cerr << "v2p_map incorrect physical address. Expected "
                          << physical_addresses[i] << " got " << key_val->second << ".\n";
                exit(1);
            }
        } else {
            std::cerr << "v2p_map incorrect virtual address. Expected "
                      << virtual_addresses[i] << " not found.\n";
            exit(1);
        }
    }

    // Check page_size.
    constexpr uint64_t EXPECTED_PAGE_SIZE = 0x200000;
    if (v2p_info.page_size != EXPECTED_PAGE_SIZE) {
        std::cerr << "Incorrect page size. Expected " << EXPECTED_PAGE_SIZE << " got "
                  << v2p_info.page_size << ".\n";
        exit(1);
    }

    // Check page_count.
    constexpr uint64_t PAGE_COUNT = 0x1;
    if (v2p_info.page_count != PAGE_COUNT) {
        std::cerr << "Incorrect page count. Expected " << PAGE_COUNT << " got "
                  << v2p_info.page_count << ".\n";
        exit(1);
    }

    // Check number of bytes_mapped.
    constexpr uint64_t BYTES_MAPPED = 0x18;
    if (v2p_info.bytes_mapped != BYTES_MAPPED) {
        std::cerr << "Incorrect number of bytes mapped. Expected " << BYTES_MAPPED
                  << " got " << v2p_info.bytes_mapped << ".\n";
        exit(1);
    }
}

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

namespace {

// Fails the unit test immediately when `cond` is false.
void
require_cond(bool cond, const std::string &msg)
{
    if (!cond) {
        std::cerr << "v2p_tracefs unit test failure: " << msg << "\n";
        exit(1);
    }
}

// Builds a synthetic modern (Linux 6.15+/7.1), transitional (Linux 6.1), or legacy
// (Linux 5.4) BTF binary.
std::string
build_synthetic_btf(bool modern_btf, uint32_t page_size_bytes = 64,
                    bool include_split_hook = true, bool include_folio_order_6_1 = false,
                    bool include_hugetlb = true)
{
    // Maintain the NUL-terminated BTF string section.
    std::string strs(1, '\0');
    auto add_str = [&](const char *s) {
        uint32_t off = static_cast<uint32_t>(strs.size());
        strs.append(s, strlen(s) + 1);
        return off;
    };

    // Append 12-byte common BTF type headers with 1-based type IDs.
    std::vector<uint32_t> types;
    uint32_t next_id = 1;
    auto push_type = [&](uint32_t name_off, uint32_t kind, uint32_t vlen,
                         uint32_t size_or_type) {
        types.push_back(name_off);
        types.push_back((kind << 24) | (vlen & 0xffff));
        types.push_back(size_or_type);
        return next_id++;
    };

    constexpr uint32_t kBtfInt = 1;
    constexpr uint32_t kBtfPtr = 2;
    constexpr uint32_t kBtfStruct = 4;
    constexpr uint32_t kBtfEnum = 6;
    constexpr uint32_t kBtfTypedef = 8;
    constexpr uint32_t kBtfFunc = 12;
    constexpr uint32_t kBtfFuncProto = 13;

    // Define primitive integer types (`u64`, `u32`, `u8`) and page/fault enums.
    uint32_t t_u64 = push_type(add_str("unsigned long"), kBtfInt, 0, 8);
    types.push_back(64);
    uint32_t t_u32 = push_type(add_str("unsigned int"), kBtfInt, 0, 4);
    types.push_back(32);
    uint32_t t_u8 = push_type(add_str("unsigned char"), kBtfInt, 0, 1);
    types.push_back(8);

    push_type(add_str("pageflags"), kBtfEnum, 1, 4);
    types.insert(types.end(), { add_str("PG_head"), modern_btf ? 6U : 15U });
    push_type(add_str("fault_flag"), kBtfEnum, 2, 4);
    types.insert(types.end(), { add_str("FAULT_FLAG_WRITE"), 1U });
    types.insert(types.end(), { add_str("FAULT_FLAG_UNSHARE"), 1024U });
    push_type(add_str("vm_fault_reason"), kBtfEnum, 2, 4);
    types.insert(types.end(), { add_str("VM_FAULT_OOM"), 1U });
    types.insert(types.end(), { add_str("VM_FAULT_RETRY"), 1024U });

    // Define core MM structs and pointer types.
    uint32_t t_mm = push_type(add_str("mm_struct"), kBtfStruct, 0, 128);
    uint32_t t_mm_p = push_type(0, kBtfPtr, 0, t_mm);
    uint32_t t_vmops = push_type(add_str("vm_operations_struct"), kBtfStruct, 0, 64);
    uint32_t t_vmops_p = push_type(0, kBtfPtr, 0, t_vmops);

    uint32_t t_vma = push_type(add_str("vm_area_struct"), kBtfStruct, 4, 192);
    types.insert(types.end(), { add_str("vm_end"), t_u64, 64 });
    types.insert(types.end(), { add_str("vm_mm"), t_mm_p, 128 });
    types.insert(types.end(), { add_str("vm_flags"), t_u64, 256 });
    types.insert(types.end(), { add_str("vm_ops"), t_vmops_p, 1088 });
    uint32_t t_vma_p = push_type(0, kBtfPtr, 0, t_vma);

    uint32_t t_vmf = push_type(add_str("vm_fault"), kBtfStruct, 2, 128);
    types.insert(types.end(), { add_str("vma"), t_vma_p, 0 });
    types.insert(types.end(), { add_str("address"), t_u64, 128 });
    uint32_t t_vmf_p = push_type(0, kBtfPtr, 0, t_vmf);

    uint32_t t_tlb = push_type(add_str("mmu_gather"), kBtfStruct, 1, 128);
    types.insert(types.end(), { add_str("mm"), t_mm_p, 0 });
    uint32_t t_tlb_p = push_type(0, kBtfPtr, 0, t_tlb);

    uint32_t t_page = 0;
    if (modern_btf) {
        uint32_t t_mdf_s = push_type(0, kBtfStruct, 1, 8);
        types.insert(types.end(), { add_str("f"), t_u64, 0 });
        uint32_t t_mdf = push_type(add_str("memdesc_flags_t"), kBtfTypedef, 0, t_mdf_s);
        t_page = push_type(add_str("page"), kBtfStruct, 1, page_size_bytes);
        types.insert(types.end(), { add_str("flags"), t_mdf, 0 });
    } else {
        t_page = push_type(add_str("page"), kBtfStruct, 2, page_size_bytes);
        types.insert(types.end(), { add_str("flags"), t_u64, 0 });
        types.insert(types.end(), { add_str("compound_order"), t_u8, 136 });
    }
    uint32_t t_page_p = push_type(0, kBtfPtr, 0, t_page);

    uint32_t t_folio_p = 0;
    if (modern_btf) {
        uint32_t t_folio =
            push_type(add_str("folio"), kBtfStruct, include_folio_order_6_1 ? 3 : 2, 128);
        types.insert(types.end(), { add_str("page"), t_page, 0 });
        types.insert(types.end(), { add_str("_flags_1"), t_u64, 512 });
        if (include_folio_order_6_1)
            types.insert(types.end(), { add_str("_folio_order"), t_u8, 648 });
        t_folio_p = push_type(0, kBtfPtr, 0, t_folio);
    }

    auto add_fn = [&](const char *name, const std::vector<uint32_t> &params) {
        uint32_t proto =
            push_type(0, kBtfFuncProto, static_cast<uint32_t>(params.size()), 0);
        for (uint32_t pt : params)
            types.insert(types.end(), { 0U, pt });
        push_type(add_str(name), kBtfFunc, 1, proto);
    };

    // Populate modern or legacy kernel function prototypes across all 6 categories.
    add_fn("handle_mm_fault", { t_vma_p, t_u64, t_u32, t_u64 });
    if (modern_btf) {
        uint32_t t_pmc = push_type(add_str("pagetable_move_control"), kBtfStruct, 4, 56);
        types.insert(types.end(), { add_str("old"), t_vma_p, 0 });
        types.insert(types.end(), { add_str("old_addr"), t_u64, 128 });
        types.insert(types.end(), { add_str("new_addr"), t_u64, 256 });
        types.insert(types.end(), { add_str("len_in"), t_u64, 320 });
        uint32_t t_pmc_p = push_type(0, kBtfPtr, 0, t_pmc);

        uint32_t t_ud = push_type(add_str("unmap_desc"), kBtfStruct, 2, 64);
        types.insert(types.end(), { add_str("vma_start"), t_u64, 256 });
        types.insert(types.end(), { add_str("vma_end"), t_u64, 320 });
        uint32_t t_ud_p = push_type(0, kBtfPtr, 0, t_ud);

        add_fn("mm_get_huge_zero_folio", { t_mm_p });
        add_fn("folio_add_new_anon_rmap", { t_folio_p, t_vma_p, t_u64, t_u32 });
        add_fn("folio_add_anon_rmap_ptes",
               { t_folio_p, t_page_p, t_u32, t_vma_p, t_u64 });
        add_fn("folio_add_anon_rmap_pmd", { t_folio_p, t_page_p, t_vma_p, t_u64 });
        add_fn("set_pte_range", { t_vmf_p, t_folio_p, t_page_p, t_u32, t_u64 });
        add_fn("do_set_pmd", { t_vmf_p, t_folio_p, t_page_p });
        add_fn("remove_migration_ptes", { t_folio_p, t_folio_p, t_u32 });
        add_fn("move_page_tables", { t_pmc_p });
        add_fn("unmap_vmas", { t_tlb_p, t_ud_p });
        add_fn("zap_vma_range_batched", { t_tlb_p, t_vma_p, t_u64, t_u64 });
        add_fn("try_to_unmap_one", { t_folio_p, t_vma_p, t_u64, t_u64 });
        add_fn("try_to_migrate_one", { t_folio_p, t_vma_p, t_u64, t_u64 });
    } else {
        add_fn("mm_get_huge_zero_page", { t_mm_p });
        add_fn("page_add_new_anon_rmap", { t_page_p, t_vma_p, t_u64, t_u8 });
        add_fn("page_add_anon_rmap", { t_page_p, t_vma_p, t_u64, t_u8 });
        add_fn("alloc_set_pte", { t_vmf_p, t_u64, t_page_p });
        add_fn("page_add_file_rmap", { t_page_p, t_u8 });
        add_fn("remove_migration_ptes", { t_page_p, t_page_p, t_u8 });
        add_fn("move_page_tables", { t_vma_p, t_u64, t_vma_p, t_u64, t_u64, t_u8, t_u8 });
        add_fn("unmap_page_range", { t_tlb_p, t_vma_p, t_u64, t_u64, t_u64 });
        add_fn("try_to_unmap_one", { t_page_p, t_vma_p, t_u64, t_u64 });
    }
    if (include_split_hook)
        add_fn("__split_huge_pmd", { t_vma_p, t_u64, t_u64, t_u8, t_page_p });
    if (include_hugetlb) {
        add_fn("hugetlb_fault", { t_mm_p, t_vma_p, t_u64, t_u32 });
        if (modern_btf) {
            add_fn("hugetlb_add_new_anon_rmap", { t_folio_p, t_vma_p, t_u64 });
            add_fn("folio_unlock", { t_folio_p });
            add_fn("__unmap_hugepage_range",
                   { t_tlb_p, t_vma_p, t_u64, t_u64, t_folio_p, t_u32 });
        } else {
            add_fn("hugepage_add_new_anon_rmap", { t_page_p, t_vma_p, t_u64 });
            add_fn("unlock_page", { t_page_p });
            add_fn("__unmap_hugepage_range",
                   { t_tlb_p, t_vma_p, t_u64, t_u64, t_page_p });
        }
    }

    // Pack the 24-byte BTF header, type section, and string section.
    struct {
        uint16_t magic;
        uint8_t version;
        uint8_t flags;
        uint32_t hdr_len;
        uint32_t type_off;
        uint32_t type_len;
        uint32_t str_off;
        uint32_t str_len;
    } hdr = { 0xeb9f,
              1,
              0,
              24,
              0,
              static_cast<uint32_t>(types.size() * sizeof(uint32_t)),
              static_cast<uint32_t>(types.size() * sizeof(uint32_t)),
              static_cast<uint32_t>(strs.size()) };
    std::string bin(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
    bin.append(reinterpret_cast<const char *>(types.data()), hdr.type_len);
    bin.append(strs);
    return bin;
}

// Verifies `v2p_check_kernel_support` on modern 7.1, legacy 5.4, and drifted BTF
// binaries, returning the resolved modern plan for offline 6-category tracker replay.
v2p_kernel_plan_t
unit_test_v2p_check_kernel_support(const std::string &base_dir)
{
    // Create temporary BTF and mock tracepoint format files.
    mkdir(base_dir.c_str(), 0755);
    mkdir((base_dir + "/events").c_str(), 0755);
    mkdir((base_dir + "/events/sched").c_str(), 0755);
    mkdir((base_dir + "/events/sched/sched_process_exec").c_str(), 0755);

    std::string btf_file = base_dir + "/vmlinux.btf";
    std::string exec_fmt = base_dir + "/events/sched/sched_process_exec/format";
    auto write_str = [](const std::string &p, const std::string &s) {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(s.data(), static_cast<std::streamsize>(s.size()));
    };
    write_str(exec_fmt, "print fmt: \"filename=%s pid=%d old_pid=%d\"\n");

    auto has_probe_def = [](const v2p_kernel_plan_t &p, const char *base,
                            const char *substr) {
        for (const v2p_probe_spec_t &s : p.probes) {
            if (s.base_name == base && s.definition.find(substr) != std::string::npos)
                return true;
        }
        return false;
    };

    // Verify modern folio/unmap_vmas BTF resolution.
    write_str(btf_file, build_synthetic_btf(/*modern_btf=*/true));
    v2p_kernel_plan_t modern_plan;
    std::string err;
    require_cond(v2p_check_kernel_support(btf_file, base_dir, modern_plan, err),
                 "modern BTF check failed: " + err);
    require_cond(has_probe_def(modern_plan, "anon_new", "folio_add_new_anon_rmap") &&
                     has_probe_def(modern_plan, "file_pte", "set_pte_range") &&
                     has_probe_def(modern_plan, "set_pmd_in", "do_set_pmd") &&
                     has_probe_def(modern_plan, "unmap_vmas", "unmap_vmas") &&
                     has_probe_def(modern_plan, "move_pt_in", "+16($arg1)") &&
                     modern_plan.has_hugetlb && modern_plan.ttu_ignore_arg4_mask == 0 &&
                     modern_plan.folio_order_byte_off == 64 &&
                     modern_plan.pg_head_bit == 6,
                 "unexpected modern plan fields");

    // Verify `CONFIG_HUGETLB_PAGE=n` kernels succeed cleanly with `has_hugetlb == false`.
    write_str(btf_file,
              build_synthetic_btf(/*modern_btf=*/true, /*page_size_bytes=*/64,
                                  /*include_split_hook=*/true,
                                  /*include_folio_order_6_1=*/false,
                                  /*include_hugetlb=*/false));
    v2p_kernel_plan_t no_hugetlb_plan;
    require_cond(v2p_check_kernel_support(btf_file, base_dir, no_hugetlb_plan, err) &&
                     !no_hugetlb_plan.has_hugetlb,
                 "CONFIG_HUGETLB_PAGE=n BTF check failed");

    // Verify Linux 5.18-6.5 `folio` prefers `_folio_order` (+81) over `_flags_1` (+64).
    write_str(btf_file,
              build_synthetic_btf(/*modern_btf=*/true, /*page_size_bytes=*/64,
                                  /*include_split_hook=*/true,
                                  /*include_folio_order_6_1=*/true));
    v2p_kernel_plan_t plan_6_1;
    require_cond(v2p_check_kernel_support(btf_file, base_dir, plan_6_1, err) &&
                     plan_6_1.folio_order_byte_off == 81,
                 "Linux 6.1 _folio_order precedence check failed");

    // Verify legacy Linux 5.4 page/alloc_set_pte/file-rmap BTF resolution.
    write_str(btf_file, build_synthetic_btf(/*modern_btf=*/false));
    v2p_kernel_plan_t legacy_plan;
    require_cond(v2p_check_kernel_support(btf_file, base_dir, legacy_plan, err),
                 "legacy 5.4 BTF check failed: " + err);
    require_cond(has_probe_def(legacy_plan, "anon_new", "comp=$arg4:u8") &&
                     has_probe_def(legacy_plan, "file_pte", "alloc_set_pte") &&
                     has_probe_def(legacy_plan, "file_rmap_pmd", "page_add_file_rmap") &&
                     has_probe_def(legacy_plan, "unmap_range", "unmap_page_range") &&
                     has_probe_def(legacy_plan, "move_pt_in", "old_addr=$arg2:x64") &&
                     legacy_plan.has_hugetlb && legacy_plan.ttu_ignore_arg4_mask == 0x3 &&
                     legacy_plan.folio_order_byte_off == 81 &&
                     legacy_plan.pg_head_bit == 15,
                 "unexpected legacy 5.4 plan fields");

    // Verify missing BTF, drifted `sizeof(struct page)`, missing hooks, and multi-error
    // collection across all checks.
    v2p_kernel_plan_t bad_plan;
    require_cond(
        !v2p_check_kernel_support(base_dir + "/missing.btf", base_dir, bad_plan, err) &&
            !err.empty(),
        "missing BTF should fail");
    write_str(btf_file,
              build_synthetic_btf(/*modern_btf=*/true, /*page_size_bytes=*/128,
                                  /*include_split_hook=*/false));
    require_cond(!v2p_check_kernel_support(btf_file, base_dir, bad_plan, err,
                                           /*ignore_missing_hooks=*/true) &&
                     err.find("(CRITICAL)") != std::string::npos &&
                     err.find("sizeof(struct page)") != std::string::npos &&
                     err.find("__split_huge_pmd") != std::string::npos &&
                     err.find("-v2p_tracefs_ignore_missing_hooks") == std::string::npos,
                 "multi-error BTF check should stay fatal and omit flag hint on critical "
                 "errors");

    // Verify non-critical missing hooks abort with the flag hint by default, and
    // succeed with warnings when `ignore_missing_hooks` is true.
    write_str(btf_file,
              build_synthetic_btf(/*modern_btf=*/true, /*page_size_bytes=*/64,
                                  /*include_split_hook=*/false));
    v2p_kernel_plan_t partial_plan;
    require_cond(!v2p_check_kernel_support(btf_file, base_dir, partial_plan, err,
                                           /*ignore_missing_hooks=*/false) &&
                     partial_plan.critical_errors.empty() &&
                     partial_plan.warnings.size() == 1 &&
                     err.find("transitions uncovered:") != std::string::npos &&
                     err.find("-v2p_tracefs_ignore_missing_hooks") != std::string::npos,
                 "missing non-critical hook should abort by default and suggest flag");
    require_cond(v2p_check_kernel_support(btf_file, base_dir, partial_plan, err,
                                          /*ignore_missing_hooks=*/true) &&
                     partial_plan.critical_errors.empty() &&
                     partial_plan.warnings.size() == 1,
                 "ignore_missing_hooks should allow non-critical missing hook");

    // Clean up temporary BTF test files.
    std::remove(btf_file.c_str());
    std::remove(exec_fmt.c_str());
    rmdir((base_dir + "/events/sched/sched_process_exec").c_str());
    rmdir((base_dir + "/events/sched").c_str());
    rmdir((base_dir + "/events").c_str());
    rmdir(base_dir.c_str());
    return modern_plan;
}

// Replays synthetic `trace_pipe` lines across all 6 V2P categories and verifies
// the emitted `v2p.bin` record stream.
void
unit_test_v2p_six_categories(const std::string &v2p_bin_path,
                             const v2p_kernel_plan_t &modern_plan)
{
    // Use a 64-byte-aligned, non-4KB-aligned base to exercise ARM64 `vmemmap` offsets.
    constexpr uint64_t kVmemmap = 0xfffffdffc0000840ULL;
    constexpr uint64_t kZeroPfn = 0x50;
    auto page_ptr_hex = [&](uint64_t pfn) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llx",
                 static_cast<unsigned long long>(kVmemmap + (pfn << 6)));
        return std::string(buf);
    };

    v2p_tracker_t tracker;
    require_cond(tracker.init_for_testing(/*pid=*/1000, v2p_bin_path, modern_plan,
                                          kVmemmap, kZeroPfn),
                 "init_for_testing failed");

    // Category 2: Anonymous faults on a thread with a bracketed `comm` (`tokio-rt[0]`).
    tracker.feed_trace_line_for_testing(
        " tokio-rt[0]-1000 [000] ....     0.000010: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x100) + " mm=0xffff888012340000 vaddr=0x10000 flags=0x0 ord=0");
    tracker.feed_trace_line_for_testing(
        " tokio-rt[0]-1000 [000] ....     0.000015: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x101) + " mm=0xffff888012340000 vaddr=0x11000 flags=0x0 ord=0");

    // Category 2: 4 KB shared zero-page read fault, CoW overwrite, and idempotent repeat.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000030: fault_in_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x20123 flags=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000031: fault_out_1000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000040: fault_in_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x20123 flags=1");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000041: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x200) + " mm=0xffff888012340000 vaddr=0x20123 flags=0x0 ord=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000042: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x200) + " mm=0xffff888012340000 vaddr=0x20123 flags=0x0 ord=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000043: fault_out_1000: (0x0 <- 0x0) ret=0");

    // Category 2 + 6 + 5: 2 MB huge zero-page read fault, split to 4 KB zero PTEs, unmap.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000050: fault_in_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x400000 flags=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000051: huge_zero_out_1000: (0x0 <- 0x0) ret=" +
        page_ptr_hex(0x600));
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000052: fault_out_1000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000060: split_pmd_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x400000");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000065: unmap_vmas_1000: (0x0) "
        "mm=0xffff888012340000 start=0x400000 end=0x600000");

    // Category 2 + 3 + 6: 2 MB THP install, losing concurrent 2 MB huge-zero read race
    // (must not clobber), order-9 folio migration, and in-place split.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000070: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x1000) + " mm=0xffff888012340000 vaddr=0x800000 flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " app-1001 [001] ....     0.000071: fault_in_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x804000 flags=0");
    tracker.feed_trace_line_for_testing(
        " app-1001 [001] ....     0.000072: huge_zero_out_1000: (0x0 <- 0x0) ret=" +
        page_ptr_hex(0x600));
    tracker.feed_trace_line_for_testing(
        " app-1001 [001] ....     0.000073: fault_out_1000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " kcompactd0-55 [001] ....     0.000080: mig_ptes_1000: (0x0) src=" +
        page_ptr_hex(0x1000) + " dst=" + page_ptr_hex(0x2000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000090: split_pmd_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0x0 vaddr=0x804000");

    // Category 2 + 5: File fault-around batch, partial CoW, and `vm_end`-clamped reclaim.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000100: file_pte_1000: (0x0) mm=0xffff888012340000 "
        "page=" +
        page_ptr_hex(0x300) + " nr=4 vaddr=0x30000");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000110: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x399) + " mm=0xffff888012340000 vaddr=0x31000 flags=0x0 ord=0");
    tracker.feed_trace_line_for_testing(
        " kswapd0-66 [002] ....     0.000120: ttu_in_1000: (0x0) page=" +
        page_ptr_hex(0x300) +
        " mm=0xffff888012340000 vaddr=0x30000 vm_end=0x33000 flags=0x40 ord=2");
    tracker.feed_trace_line_for_testing(
        " kswapd0-66 [002] ....     0.000121: ttu_out_1000: (0x0 <- 0x0) ret=1");

    // Category 2 + 6: File 2 MB PMD subpage fault (`do_set_pmd` tail page `0x3005`
    // masked to head `0x3000`) and split-zap (`vm_ops != 0`).
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000130: set_pmd_in_1000: (0x0) "
        "mm=0xffff888012340000 page=" +
        page_ptr_hex(0x3005) + " vaddr=0xc05000");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000131: set_pmd_out_1000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000140: split_pmd_1000: (0x0) mm=0xffff888012340000 "
        "vm_ops=0xffffffff81200000 vaddr=0xc00000");

    // Category 4: Partial `mremap` (`moved=4096` out of `len=8192`) relocates only
    // 0x10000.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000150: move_pt_in_1000: (0x0) "
        "mm=0xffff888012340000 old_addr=0x10000 new_addr=0x90000 len=8192");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000151: move_pt_out_1000: (0x0 <- 0x0) moved=4096");

    // Category 1: `sched_process_exec` parses trailing `pid=` even if `filename=` has
    // `" pid="`, tears down old mappings, and learns a new `mm`.
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000160: sched_process_exec: filename=/tmp/my pid=999 "
        "old_pid=9999 pid=1000");
    tracker.feed_trace_line_for_testing(
        " app-1000 [000] ....     0.000170: anon_new_1000: (0x0) page=" +
        page_ptr_hex(0x444) + " mm=0xffff888099990000 vaddr=0x7fff0000 flags=0x0 ord=0");

    tracker.finish_for_testing();

    // Read back all emitted `v2p_record_t` entries and verify key transitions.
    std::ifstream in(v2p_bin_path, std::ios::binary);
    std::vector<v2p_record_t> recs;
    for (v2p_record_t r = {}; in.read(reinterpret_cast<char *>(&r), sizeof(r));)
        recs.push_back(r);
    in.close();
    std::remove(v2p_bin_path.c_str());

    // Expected total records:
    // 2 (anon_new) + 1 (4K zero) + 1 (CoW, deduped) + 1 (2M huge zero)
    // + 512 (huge zero split -> 4K zero) + 1 (unmap_vmas) + 1 (2M THP) + 1 (mig_ptes)
    // + 1 (V2P_RECORD_SPLIT) + 1 (file_pte x4) + 1 (CoW 0x31000) + 2 (ttu_out clamped)
    // + 1 (file 2M PMD) + 1 (file split-zap) + 2 (mremap unmap+map)
    // + 6 (exec unmap runs: 0x11000, 0x20000, 0x31000, 0x33000, 0x90000, 0x800000)
    // + 1 (post-exec new mm map) + 1 (END) = 537 records.
    require_cond(recs.size() == 537,
                 "expected 537 v2p records, got " + std::to_string(recs.size()));
    require_cond(recs[0].timestamp_us == 10 && recs[0].vaddr == 0x10000 &&
                     recs[0].paddr == 0x100000 && recs[0].type == V2P_RECORD_MAP,
                 "initial anon_new record mismatch");
    require_cond(recs[2].timestamp_us == 31 && recs[2].vaddr == 0x20000 &&
                     recs[2].paddr == 0x50000 && recs[2].type == V2P_RECORD_MAP,
                 "4 KB zero-page record mismatch");
    require_cond(recs[3].timestamp_us == 41 && recs[3].vaddr == 0x20000 &&
                     recs[3].paddr == 0x200000 && recs[3].type == V2P_RECORD_MAP,
                 "CoW overwrite / dedup mismatch");
    require_cond(recs[4].vaddr == 0x400000 && recs[4].page_count == 512 &&
                     recs[5].vaddr == 0x400000 && recs[5].paddr == 0x50000 &&
                     recs[517].vaddr == 0x400000 && recs[517].page_count == 512 &&
                     recs[517].type == V2P_RECORD_UNMAP,
                 "2 MB huge zero-page split/unmap mismatch");
    require_cond(recs[519].vaddr == 0x800000 && recs[519].paddr == 0x2000000 &&
                     recs[519].page_count == 512 && recs[519].type == V2P_RECORD_MAP &&
                     recs[520].vaddr == 0x800000 && recs[520].paddr == 0x2000000 &&
                     recs[520].page_count == 512 && recs[520].type == V2P_RECORD_SPLIT,
                 "order-9 migration or anonymous V2P_RECORD_SPLIT mismatch");
    require_cond(recs[523].vaddr == 0x30000 && recs[523].page_count == 1 &&
                     recs[523].type == V2P_RECORD_UNMAP && recs[524].vaddr == 0x32000 &&
                     recs[524].page_count == 1 && recs[524].type == V2P_RECORD_UNMAP,
                 "CoW-preserving and vm_end-clamped try_to_unmap_one eviction mismatch");
    require_cond(recs[525].vaddr == 0xc00000 && recs[525].paddr == 0x3000000 &&
                     recs[525].page_count == 512 && recs[525].type == V2P_RECORD_MAP &&
                     recs[526].vaddr == 0xc00000 && recs[526].page_count == 512 &&
                     recs[526].type == V2P_RECORD_UNMAP,
                 "file 2 MB PMD subpage alignment or split-zap mismatch");
    require_cond(recs[527].vaddr == 0x10000 && recs[527].page_count == 1 &&
                     recs[527].type == V2P_RECORD_UNMAP && recs[528].vaddr == 0x90000 &&
                     recs[528].paddr == 0x100000 && recs[528].type == V2P_RECORD_MAP,
                 "partial mremap relocation mismatch");
    require_cond(recs[535].vaddr == 0x7fff0000 && recs[535].paddr == 0x444000 &&
                     recs[535].type == V2P_RECORD_MAP && recs[536].type == V2P_RECORD_END,
                 "post-exec mapping or END sentinel mismatch");
}

// Replays Category 7 (`hugetlbfs` explicit hugepage pool) events across 64 KB, 2 MB,
// and 1 GB (`262,144` x 4 KB) spans, including migration, `mremap`, CoW,
// `VM_FAULT_RETRY`, `unmap_ref_private`, and explicit `munmap`.
void
unit_test_v2p_hugetlb_category(const std::string &v2p_bin_path,
                               const v2p_kernel_plan_t &modern_plan)
{
    constexpr uint64_t kVmemmap = 0xfffffdffc0000840ULL;
    constexpr uint64_t kZeroPfn = 0x50;
    auto page_ptr_hex = [&](uint64_t pfn) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llx",
                 static_cast<unsigned long long>(kVmemmap + (pfn << 6)));
        return std::string(buf);
    };

    v2p_tracker_t tracker;
    require_cond(tracker.init_for_testing(/*pid=*/2000, v2p_bin_path, modern_plan,
                                          kVmemmap, kZeroPfn),
                 "hugetlb init_for_testing failed");

    // 1. 1 GB (`ord=18`, `262,144` pages) private `hugetlb_anon` fault, order-18 physical
    // migration, and 1 GB `mremap` (`move_page_tables`) relocation.
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000010: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400073 vaddr=0x40000123 flags=1");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000011: hugetlb_anon_2000: (0x0) page=" +
        page_ptr_hex(0x40000) +
        " mm=0xffff888022220000 vaddr=0x40000123 flags=0x40 ord=18");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000012: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x40000) + " flags=0x40 ord=18");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000013: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " kcompactd0-55 [001] ....     0.000020: mig_ptes_2000: (0x0) src=" +
        page_ptr_hex(0x40000) + " dst=" + page_ptr_hex(0x80000) + " flags=0x40 ord=18");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000030: move_pt_in_2000: (0x0) "
        "mm=0xffff888022220000 old_addr=0x40000000 new_addr=0xc0000000 len=1073741824");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000031: move_pt_out_2000: (0x0 <- 0x0) "
        "moved=1073741824");

    // 2. Reject `VM_FAULT_RETRY` (`0x400`) on `hugetlb_no_page`, then bind two 2 MB
    // (`ord=9`) private read faults borrowing file hugepages.
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000040: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400071 vaddr=0x10000000 flags=0");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000041: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x5000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000042: hugetlb_fault_out_2000: (0x0 <- 0x0) "
        "ret=0x400");

    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000050: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400073 vaddr=0x10000000 flags=0");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000051: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x5000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000052: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");

    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000053: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400073 vaddr=0x10200000 flags=0");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000054: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x5200) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000055: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");

    // 3. Private CoW write fault on `0x10000000` where `pagecache_folio` (`0x5000`) is
    // unlocked after `hugetlb_anon` (`0x6000`), plus a losing concurrent read race on
    // thread 2001 (neither may overwrite `0x6000`).
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000060: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400073 vaddr=0x10000000 flags=1");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000061: hugetlb_anon_2000: (0x0) page=" +
        page_ptr_hex(0x6000) +
        " mm=0xffff888022220000 vaddr=0x10000000 flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000062: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x5000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000063: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " db-2001 [001] ....     0.000064: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x400073 vaddr=0x10000000 flags=0");
    tracker.feed_trace_line_for_testing(
        " db-2001 [001] ....     0.000065: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x5000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2001 [001] ....     0.000066: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");

    // 4. `unmap_ref_private` (`ref != 0`) preserves CoW-diverged `0x10000000` (`0x6000`)
    // while unmapping `0x10200000` still backed by `0x5200`.
    tracker.feed_trace_line_for_testing(
        " other-3000 [002] ....     0.000070: hugetlb_unmap_2000: (0x0) "
        "mm=0xffff888022220000 start=0x10000000 end=0x10200000 ref=" +
        page_ptr_hex(0x5000));
    tracker.feed_trace_line_for_testing(
        " other-3000 [002] ....     0.000071: hugetlb_unmap_2000: (0x0) "
        "mm=0xffff888022220000 start=0x10200000 end=0x10400000 ref=" +
        page_ptr_hex(0x5200));

    // 5. Shared `VM_SHARED` (`vm_flags` bit `0x8`) 64 KB (`ord=4`, `16` pages) fault
    // with intervening softirq (`d.s1.`) and order-5 (`128 KB`) page-cache unlocks
    // (both ignored), followed by explicit 1 GB `munmap` (`ref=0x0`).
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000080: hugetlb_fault_in_2000: (0x0) "
        "mm=0xffff888022220000 vm_flags=0x4000fb vaddr=0x20012345 flags=1");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000081: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x700) + " flags=0x40 ord=4");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] d.s1.    0.000081: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x9000) + " flags=0x40 ord=9");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000081: hugetlb_unlock_2000: (0x0) page=" +
        page_ptr_hex(0x9100) + " flags=0x40 ord=5");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000082: hugetlb_fault_out_2000: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " db-2000 [000] ....     0.000090: hugetlb_unmap_2000: (0x0) "
        "mm=0xffff888022220000 start=0xc0000000 end=0x100000000 ref=0x0");

    tracker.finish_for_testing();

    std::ifstream in(v2p_bin_path, std::ios::binary);
    std::vector<v2p_record_t> recs;
    for (v2p_record_t r = {}; in.read(reinterpret_cast<char *>(&r), sizeof(r));)
        recs.push_back(r);
    in.close();
    std::remove(v2p_bin_path.c_str());

    constexpr uint32_t kPages1G = 1U << 18;
    constexpr uint32_t kPages64K = 16;
    require_cond(recs.size() == 11,
                 "expected 11 hugetlb v2p records, got " + std::to_string(recs.size()));
    require_cond(recs[0].vaddr == 0x40000000 && recs[0].paddr == 0x40000000 &&
                     recs[0].page_count == kPages1G && recs[0].type == V2P_RECORD_MAP,
                 "1 GB private hugetlb fault mismatch");
    require_cond(recs[1].vaddr == 0x40000000 && recs[1].paddr == 0x80000000 &&
                     recs[1].page_count == kPages1G && recs[1].type == V2P_RECORD_MAP,
                 "1 GB hugetlb physical migration mismatch");
    require_cond(recs[2].vaddr == 0x40000000 && recs[2].page_count == kPages1G &&
                     recs[2].type == V2P_RECORD_UNMAP && recs[3].vaddr == 0xc0000000 &&
                     recs[3].paddr == 0x80000000 && recs[3].page_count == kPages1G &&
                     recs[3].type == V2P_RECORD_MAP,
                 "1 GB hugetlb mremap relocation mismatch");
    require_cond(recs[4].vaddr == 0x10000000 && recs[4].paddr == 0x5000000 &&
                     recs[4].page_count == kPagesPer2M && recs[5].vaddr == 0x10200000 &&
                     recs[5].paddr == 0x5200000 && recs[5].page_count == kPagesPer2M,
                 "2 MB borrowed file hugetlb read fault or VM_FAULT_RETRY mismatch");
    require_cond(recs[6].vaddr == 0x10000000 && recs[6].paddr == 0x6000000 &&
                     recs[6].page_count == kPagesPer2M && recs[6].type == V2P_RECORD_MAP,
                 "2 MB private CoW hugetlb fault mismatch");
    require_cond(recs[7].vaddr == 0x10200000 && recs[7].page_count == kPagesPer2M &&
                     recs[7].type == V2P_RECORD_UNMAP,
                 "hugetlb unmap_ref_private CoW preservation mismatch");
    require_cond(recs[8].vaddr == 0x20010000 && recs[8].paddr == 0x700000 &&
                     recs[8].page_count == kPages64K && recs[8].type == V2P_RECORD_MAP,
                 "64 KB shared hugetlb fault mismatch");
    require_cond(recs[9].vaddr == 0xc0000000 && recs[9].page_count == kPages1G &&
                     recs[9].type == V2P_RECORD_UNMAP && recs[10].type == V2P_RECORD_END,
                 "1 GB explicit hugetlb unmap or END sentinel mismatch");
}

// Verifies partial 4 KB `unmap_page_range` entry followed by `__split_huge_pmd` on
// both a 2 MB shared huge zero page and a 2 MB file/shmem THP (`vm_ops != 0`).
void
unit_test_v2p_partial_zap_split(const std::string &v2p_bin_path,
                                const v2p_kernel_plan_t &modern_plan)
{
    constexpr uint64_t kVmemmap = 0xfffffdffc0000840ULL;
    constexpr uint64_t kZeroPfn = 0x50;
    auto page_ptr_hex = [&](uint64_t pfn) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%" PRIx64, kVmemmap + (pfn << kStructPageShift));
        return std::string(buf);
    };

    v2p_tracker_t tracker;
    require_cond(
        tracker.init_for_testing(2100, v2p_bin_path, modern_plan, kVmemmap, kZeroPfn),
        "init_for_testing failed for partial zap split test");

    // 1. Partial 4 KB zap on a 2 MB shared huge zero page (`0x30000000`): rebinds the
    // surviving 511 pages to `kZeroPfn` (`0x50000`) and unmaps `[0x30000000, +4 KB)`.
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000010: fault_in_2100: (0x0) mm=0xffff888033330000 "
        "vm_ops=0x0 vaddr=0x30000000 flags=0");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000011: huge_zero_out_2100: (0x0 <- 0x0) ret=" +
        page_ptr_hex(0x600));
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000012: fault_out_2100: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000020: unmap_range_2100: (0x0) "
        "mm=0xffff888033330000 start=0x30000000 end=0x30001000");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000021: split_pmd_2100: (0x0) mm=0xffff888033330000 "
        "vm_ops=0x0 vaddr=0x30000000");
    require_cond(!tracker.has_shadow_mapping(0x30000000) &&
                     tracker.has_shadow_mapping(0x30001000) &&
                     tracker.has_shadow_mapping(0x301ff000),
                 "partial zap on 2 MB huge zero page corrupted surviving zero PTEs");

    // 2. Partial 4 KB zap on a 2 MB file/shmem THP (`0x30200000`, `vm_ops != 0`):
    // `unmap_page_range` zaps `[0x30200000, +4 KB)` and `split_pmd_` clears the
    // remaining 511 pages of the file PMD.
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000030: set_pmd_in_2100: (0x0) "
        "mm=0xffff888033330000 page=" +
        page_ptr_hex(0x4000) + " vaddr=0x30200000");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000031: set_pmd_out_2100: (0x0 <- 0x0) ret=0");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000040: unmap_range_2100: (0x0) "
        "mm=0xffff888033330000 start=0x30200000 end=0x30201000");
    tracker.feed_trace_line_for_testing(
        " app-2100 [000] ....     0.000041: split_pmd_2100: (0x0) mm=0xffff888033330000 "
        "vm_ops=0xffffffff81200000 vaddr=0x30200000");
    require_cond(!tracker.has_shadow_mapping(0x30200000, kPagesPer2M),
                 "partial zap + split_pmd on 2 MB file THP left stale pages mapped");

    tracker.finish_for_testing();
    std::remove(v2p_bin_path.c_str());
}

} // namespace
#endif /* LINUX && X64 && !RISCV64 */

void
unit_test_v2p_reader(const std::string &testdir)
{
    std::string v2p_file_path = testdir + "/v2p_example.textproto";
    v2p_info_t v2p_info;
    v2p_reader_t v2p_reader;
    std::ifstream fin;
    fin.open(v2p_file_path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open the v2p file '" << v2p_file_path << "'\n";
        exit(1);
    }
    std::string error_str = v2p_reader.create_v2p_info_from_file(fin, v2p_info);
    fin.close();
    if (!error_str.empty()) {
        std::cerr << "v2p_reader failed with: " << error_str << "\n";
        exit(1);
    }

    check_v2p_info(v2p_info);

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)
    const char *tmp = std::getenv("TEST_TMPDIR");
    std::string base = (tmp != nullptr && tmp[0] != '\0')
        ? (std::string(tmp) + "/v2p_tracefs_unit")
        : "v2p_tracefs_unit";
    v2p_kernel_plan_t modern_plan = unit_test_v2p_check_kernel_support(base);
    unit_test_v2p_six_categories(base + "_out.bin", modern_plan);
    unit_test_v2p_hugetlb_category(base + "_hugetlb.bin", modern_plan);
    unit_test_v2p_partial_zap_split(base + "_pzap.bin", modern_plan);
#endif /* LINUX && X64 && !RISCV64 */
}

} // namespace drmemtrace
} // namespace dynamorio
