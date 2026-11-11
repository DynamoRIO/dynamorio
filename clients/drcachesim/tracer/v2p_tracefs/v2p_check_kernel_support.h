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

/* v2p_check_kernel_support: Kernel BTF compatibility checker and per-hook kprobe
 * signature and struct-offset resolver for v2p_tracefs.
 */

#ifndef _V2P_CHECK_KERNEL_SUPPORT_H_
#define _V2P_CHECK_KERNEL_SUPPORT_H_

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <cstddef>
#    include <cstdint>
#    include <initializer_list>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace dynamorio {
namespace drmemtrace {

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPageSize2M = 2 * 1024 * 1024;
constexpr uint32_t kPagesPer2M = 512;
constexpr uint64_t kStructPageShift = 6;

// Describes a single dynamic kprobe or kretprobe registered in tracefs.
struct v2p_probe_spec_t {
    std::string base_name;
    std::string definition;
    bool is_return = false;
    bool filter_by_mm = false;
    std::string static_filter;
    std::string hook_name;
    std::string uncovered_transitions;
    bool is_critical = false;

    v2p_probe_spec_t() = default;
    v2p_probe_spec_t(std::string name, std::string def = "", bool is_ret = false,
                     std::string filter = "")
        : base_name(std::move(name))
        , definition(std::move(def))
        , is_return(is_ret)
        , static_filter(std::move(filter))
    {
    }
};

// BTF-resolved kernel layout parameters, validated probe specs, and startup errors.
struct v2p_kernel_plan_t {
    uint32_t pg_head_bit = 0;
    uint32_t folio_order_byte_off = 0;
    uint32_t fault_flag_write = 1;
    uint32_t vm_shared_mask = 0x8;
    uint32_t vm_fault_error_mask = 0x0c73;
    uint32_t legacy_anon_compound_mask = 1;
    uint32_t ttu_ignore_arg4_mask = 0;
    bool has_hugetlb = false;
    std::vector<v2p_probe_spec_t> probes;
    std::vector<std::string> critical_errors;
    std::vector<std::string> warnings;
};

// Describes one candidate kernel function signature and the probe(s) it registers.
// `param_sig` is a comma-separated list of parameter type specs (`"*"` for any,
// `"u8"`, `"u32"`, `"u64"`, `"folio|page*"`, or `"<struct_name>*"`). Probe
// `definition` and `static_filter` strings may embed `{<struct>.<field>:<type>}`,
// `{folio_order}`, and `{pg_head_mask}` placeholders.
struct v2p_hook_candidate_t {
    const char *func_name;
    std::string param_sig;
    std::vector<v2p_probe_spec_t> probes;
    int exact_param_count = -1;

    v2p_hook_candidate_t(const char *fn, std::string sig,
                         std::initializer_list<v2p_probe_spec_t> probe_list,
                         int exact_params = -1)
        : func_name(fn)
        , param_sig(std::move(sig))
        , probes(probe_list)
        , exact_param_count(exact_params)
    {
    }
    v2p_hook_candidate_t(const char *fn, std::string sig, const char *probe_name,
                         std::string probe_def, int exact_params = -1,
                         std::string filter = "")
        : func_name(fn)
        , param_sig(std::move(sig))
        , probes({ { probe_name, std::move(probe_def), false, std::move(filter) } })
        , exact_param_count(exact_params)
    {
    }
};

// Parses `/sys/kernel/btf/vmlinux` types, function prototypes, struct member offsets,
// and enum constants, validating both base `page`/`folio` layout and hooked functions.
class v2p_btf_checker_t {
public:
    v2p_btf_checker_t(const std::string &btf_path, v2p_kernel_plan_t &plan)
        : btf_path_(btf_path)
        , plan_(plan)
    {
    }

    // Parses `/sys/kernel/btf/vmlinux` bytes and resolves base page/folio layout.
    bool
    parse_and_check_base(const std::string &raw_btf);

    // Returns whether `func_name` is present in the kernel BTF function table.
    bool
    has_func(const char *func_name) const
    {
        return funcs_.find(func_name) != funcs_.end();
    }

    // Looks up an enumerator constant in BTF and writes its value to `val_out`.
    bool
    find_enum_value(const char *enum_const, uint64_t &val_out) const;

    // Resolves `struct_name::field_name` (including anonymous unions) to a byte offset.
    bool
    find_struct_field(const char *struct_name, const char *field_name,
                      uint32_t &byte_offset_out, uint32_t &field_type_out) const;

    // Validates the first present function in `candidates` against BTF, resolves struct
    // field byte offsets in its probes, and appends them to `plan_.probes`. On failure,
    // records `uncovered_transitions` in `plan_` and returns false so remaining hooks
    // can still be checked.
    bool
    check_hook(const char *hook_name, const char *uncovered_transitions,
               std::initializer_list<v2p_hook_candidate_t> candidates,
               bool is_critical = false);

    // Convenience overload for a single kernel function with multiple probes.
    bool
    check_hook(const char *func_name, std::string param_sig,
               const char *uncovered_transitions,
               std::initializer_list<v2p_probe_spec_t> probes, bool is_critical = false)
    {
        return check_hook(func_name, uncovered_transitions,
                          { { func_name, std::move(param_sig), probes } }, is_critical);
    }

    // Convenience overload for a single kernel function with one entry probe.
    bool
    check_hook(const char *func_name, std::string param_sig, const char *probe_name,
               std::string probe_def, const char *uncovered_transitions,
               bool is_critical = false)
    {
        return check_hook(
            func_name, uncovered_transitions,
            { { func_name, std::move(param_sig), probe_name, std::move(probe_def) } },
            is_critical);
    }

private:
    struct btf_type_entry_t {
        uint32_t kind = 0;
        uint32_t name_off = 0;
        uint32_t size_or_type = 0;
        uint32_t vlen = 0;
        bool kflag = false;
        size_t payload_off = 0;
    };

    const char *
    get_str(uint32_t off) const;
    uint32_t
    strip_qualifiers(uint32_t type_id) const;
    bool
    is_ptr_to_struct(uint32_t type_id, const char *struct_name) const;
    bool
    is_int_of_size(uint32_t type_id, uint32_t byte_size) const;
    bool
    matches_type(uint32_t type_id, const char *spec) const;
    bool
    find_func_params(const char *func_name, std::vector<uint32_t> &param_types) const;
    bool
    search_members(uint32_t type_id, const char *field_name, uint32_t base_bits,
                   uint32_t &bit_offset_out, uint32_t &type_out, int depth = 0) const;
    bool
    resolve_probe_offsets(std::string &str, std::string &err_out) const;
    bool
    try_candidate(const char *uncovered_transitions, bool is_critical,
                  const v2p_hook_candidate_t &cand, std::string &err_out);
    void
    add_base_error(const std::string &detail);

    const std::string *raw_ = nullptr;
    size_t str_start_ = 0;
    size_t str_end_ = 0;
    std::string btf_path_;
    v2p_kernel_plan_t &plan_;
    std::vector<btf_type_entry_t> types_;
    std::unordered_map<std::string, uint32_t> funcs_;
    std::unordered_map<std::string, uint32_t> structs_;
    std::unordered_map<std::string, uint64_t> enums_;
};

// Reads the full contents of `path` into `out`, returning false if missing or empty.
bool
v2p_read_file_bytes(const std::string &path, std::string &out);

// Writes `content` to a tracefs control file at `path`.
bool
v2p_write_tracefs_file(const std::string &path, const std::string &content,
                       bool append = false);

// Parses an unsigned 64-bit integer value following `key` in `line`.
bool
v2p_extract_u64_field(const std::string &line, const char *key, uint64_t &out,
                      int base = 0, size_t start_pos = 0);

// Returns the number of 4 KB pages (`1 << order`) represented by a folio/page's
// `flags` word and compound `ord` byte using the BTF-resolved `pg_head_bit`.
uint32_t
v2p_folio_order_pages(uint64_t flags, uint64_t ord, uint32_t pg_head_bit);

// Parses `/sys/kernel/btf/vmlinux` at `btf_path` across all V2P hooks, populating
// `plan_out.critical_errors` and `plan_out.warnings`. Returns true if no critical
// errors occurred and either no hook warnings occurred or `ignore_missing_hooks` is set.
bool
v2p_check_kernel_support(const std::string &btf_path, v2p_kernel_plan_t &plan_out,
                         std::string &error_out, bool ignore_missing_hooks = false);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
#endif /* _V2P_CHECK_KERNEL_SUPPORT_H_ */
