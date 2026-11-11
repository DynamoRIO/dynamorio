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

/* v2p_check_kernel_support: Kernel BTF compatibility checker for v2p_tracefs. */

#ifndef _V2P_CHECK_KERNEL_SUPPORT_H_
#define _V2P_CHECK_KERNEL_SUPPORT_H_

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <cstddef>
#    include <cstdint>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace dynamorio {
namespace drmemtrace {

constexpr uint64_t kPageSize4K = 4096;
constexpr uint64_t kPageSize2M = 2 * 1024 * 1024;
constexpr uint32_t kPagesPer2M = 512;
constexpr uint64_t kStructPageShift = 6;

// Kernel `struct page` and `struct folio` layout parameters resolved from BTF.
struct v2p_kernel_plan_t {
    uint32_t pg_head_bit = 0;
    uint32_t folio_order_byte_off = 0;
    std::vector<std::string> critical_errors;
};

// Parses `/sys/kernel/btf/vmlinux` types, struct member offsets, and enum constants,
// verifying base `struct page` and `struct folio` invariants at runtime.
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
    is_int_of_size(uint32_t type_id, uint32_t byte_size) const;
    bool
    search_members(uint32_t type_id, const char *field_name, uint32_t base_bits,
                   uint32_t &bit_offset_out, uint32_t &type_out, int depth = 0) const;
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

// Parses `/sys/kernel/btf/vmlinux` at `btf_path` and validates kernel `page`/`folio`
// metadata layout, populating `plan_out` (`pg_head_bit` and `folio_order_byte_off`) or
// `error_out` on failure.
bool
v2p_check_kernel_support(const std::string &btf_path, v2p_kernel_plan_t &plan_out,
                         std::string &error_out);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
#endif /* _V2P_CHECK_KERNEL_SUPPORT_H_ */
