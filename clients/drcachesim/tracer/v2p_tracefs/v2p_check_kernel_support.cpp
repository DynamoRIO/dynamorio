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

#include "v2p_check_kernel_support.h"

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <unistd.h>

#    include "v2p_tracefs_hooks.h"

namespace dynamorio {
namespace drmemtrace {

// Reads the full contents of `path` into `out`, returning false if missing or empty.
bool
v2p_read_file_bytes(const std::string &path, std::string &out)
{
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    out.clear();
    char buf[16384];
    for (ssize_t n = 0; (n = read(fd, buf, sizeof(buf))) > 0;)
        out.append(buf, static_cast<size_t>(n));
    close(fd);
    return !out.empty();
}

// Writes `content` to a tracefs control file at `path`.
bool
v2p_write_tracefs_file(const std::string &path, const std::string &content, bool append)
{
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC | (append ? O_APPEND : O_TRUNC));
    if (fd < 0)
        return false;
    ssize_t written = write(fd, content.data(), content.size());
    close(fd);
    return written == static_cast<ssize_t>(content.size());
}

// Parses an unsigned 64-bit integer value following `key` in `line`.
bool
v2p_extract_u64_field(const std::string &line, const char *key, uint64_t &out, int base,
                      size_t start_pos)
{
    // Require a whitespace boundary for `name=` keys so suffixes like `old_pid=` or
    // `vm_mm=` cannot match `pid=` or `mm=`.
    const size_t key_len = strlen(key);
    const bool require_ws = (key_len > 0 && key[0] != ':');
    size_t pos = start_pos;
    for (;;) {
        pos = line.find(key, pos);
        if (pos == std::string::npos)
            return false;
        if (!require_ws || pos == 0 || line[pos - 1] == ' ' || line[pos - 1] == '\t')
            break;
        ++pos;
    }
    const char *start = line.c_str() + pos + key_len;
    char *end = nullptr;
    errno = 0;
    unsigned long long val = strtoull(start, &end, base);
    if (end == start || errno != 0)
        return false;
    out = static_cast<uint64_t>(val);
    return true;
}

// Returns the number of 4 KB pages (`1 << order`) represented by a folio/page's
// `flags` word and compound `ord` byte using the BTF-resolved `pg_head_bit`.
uint32_t
v2p_folio_order_pages(uint64_t flags, uint64_t ord, uint32_t pg_head_bit)
{
    if (pg_head_bit >= 64 || (flags & (1ULL << pg_head_bit)) == 0)
        return 1;
    uint32_t shift = static_cast<uint32_t>(ord & 0xffU);
    return (shift == 0 || shift > 18) ? 1 : (1U << shift);
}

namespace {

// Linux `<uapi/linux/btf.h>` type kinds used when walking `/sys/kernel/btf/vmlinux`.
constexpr uint32_t kBtfInt = 1;
constexpr uint32_t kBtfPtr = 2;
constexpr uint32_t kBtfArray = 3;
constexpr uint32_t kBtfStruct = 4;
constexpr uint32_t kBtfUnion = 5;
constexpr uint32_t kBtfEnum = 6;
constexpr uint32_t kBtfFwd = 7;
constexpr uint32_t kBtfTypedef = 8;
constexpr uint32_t kBtfVolatile = 9;
constexpr uint32_t kBtfConst = 10;
constexpr uint32_t kBtfRestrict = 11;
constexpr uint32_t kBtfFunc = 12;
constexpr uint32_t kBtfFuncProto = 13;
constexpr uint32_t kBtfVar = 14;
constexpr uint32_t kBtfDatasec = 15;
constexpr uint32_t kBtfDeclTag = 17;
constexpr uint32_t kBtfTypeTag = 18;
constexpr uint32_t kBtfEnum64 = 19;

// On-disk 24-byte header at the start of `/sys/kernel/btf/vmlinux`.
struct btf_header_t {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t hdr_len;
    uint32_t type_off;
    uint32_t type_len;
    uint32_t str_off;
    uint32_t str_len;
};

} // namespace

// Appends a `(CRITICAL)` BTF layout diagnostic to `plan_.critical_errors`.
void
v2p_btf_checker_t::add_base_error(const std::string &detail)
{
    const std::string label = btf_path_.empty() ? "/sys/kernel/btf/vmlinux" : btf_path_;
    plan_.critical_errors.push_back(
        "(CRITICAL) " + label + " (" + detail +
        ") - cannot decode kernel page/folio metadata for live V2P tracking");
}

// Parses the BTF header, type table, and string section from `raw`, indexing named
// functions, structs, and enums, and validates base `page`/`folio` layouts.
bool
v2p_btf_checker_t::parse_and_check_base(const std::string &raw)
{
    // Step 1: Validate the 24-byte `btf_header` magic (`0xeb9f`) and section bounds.
    if (raw.size() < sizeof(btf_header_t)) {
        add_base_error("BTF header is truncated");
        return false;
    }
    btf_header_t hdr = {};
    memcpy(&hdr, raw.data(), sizeof(hdr));
    if (hdr.magic != 0xeb9f || hdr.hdr_len < sizeof(btf_header_t)) {
        add_base_error("BTF header has unexpected magic or length");
        return false;
    }
    uint64_t type_start = static_cast<uint64_t>(hdr.hdr_len) + hdr.type_off;
    uint64_t type_end = type_start + hdr.type_len;
    uint64_t str_start = static_cast<uint64_t>(hdr.hdr_len) + hdr.str_off;
    uint64_t str_end = str_start + hdr.str_len;
    if (type_end > raw.size() || str_end > raw.size()) {
        add_base_error("BTF sections exceed file bounds");
        return false;
    }

    raw_ = &raw;
    str_start_ = static_cast<size_t>(str_start);
    str_end_ = static_cast<size_t>(str_end);
    // BTF type IDs are 1-based (`0` denotes `void`), so reserve slot 0.
    types_.assign(1, {});

    // Step 2: Walk every 12-byte `btf_type` record in the type section, indexing named
    // functions, structs, and 32-bit/64-bit enum constants.
    size_t pos = static_cast<size_t>(type_start);
    while (pos < type_end) {
        if (pos + 12 > type_end) {
            add_base_error("BTF type record header is truncated");
            return false;
        }
        uint32_t words[3] = {};
        memcpy(words, raw.data() + pos, sizeof(words));
        pos += 12;
        btf_type_entry_t ent = {
            (words[1] >> 24) & 0x1f,      words[0], words[2], words[1] & 0xffff,
            ((words[1] >> 31) & 1U) != 0, pos
        };
        if (ent.kind == 0 || ent.kind > kBtfEnum64) {
            add_base_error("BTF contains unrecognized type kind " +
                           std::to_string(ent.kind));
            return false;
        }

        // Compute the variable-length trailing payload byte count for this BTF kind.
        size_t payload = 0;
        if (ent.kind == kBtfInt || ent.kind == kBtfVar || ent.kind == kBtfDeclTag) {
            payload = 4;
        } else if (ent.kind == kBtfArray) {
            payload = 12;
        } else if (ent.kind == kBtfStruct || ent.kind == kBtfUnion ||
                   ent.kind == kBtfDatasec || ent.kind == kBtfEnum64) {
            payload = 12 * static_cast<size_t>(ent.vlen);
        } else if (ent.kind == kBtfEnum || ent.kind == kBtfFuncProto) {
            payload = 8 * static_cast<size_t>(ent.vlen);
        }
        if (pos + payload > type_end) {
            add_base_error("BTF type payload exceeds type section");
            return false;
        }
        uint32_t type_id = static_cast<uint32_t>(types_.size());
        types_.push_back(ent);

        // Index named kernel functions (preferring global `BTF_FUNC_GLOBAL` linkage)
        // and structs (preferring complete definitions over zero-size stubs).
        const char *tname = get_str(ent.name_off);
        if (tname[0] != '\0') {
            if (ent.kind == kBtfFunc) {
                auto it = funcs_.find(tname);
                if (it == funcs_.end() || ent.vlen == 1)
                    funcs_[tname] = type_id;
            } else if (ent.kind == kBtfStruct) {
                auto it = structs_.find(tname);
                if (it == structs_.end() ||
                    ent.size_or_type >= types_[it->second].size_or_type) {
                    structs_[tname] = type_id;
                }
            }
        }

        // Index 32-bit (`BTF_KIND_ENUM`) and 64-bit (`BTF_KIND_ENUM64`) enumerators.
        if (ent.kind == kBtfEnum || ent.kind == kBtfEnum64) {
            size_t stride = (ent.kind == kBtfEnum64) ? 12 : 8;
            for (uint32_t i = 0; i < ent.vlen; ++i) {
                uint32_t ew[3] = {};
                memcpy(ew, raw.data() + pos + stride * static_cast<size_t>(i), stride);
                const char *ename = get_str(ew[0]);
                if (ename[0] != '\0') {
                    enums_[ename] = (ent.kind == kBtfEnum64)
                        ? ((static_cast<uint64_t>(ew[2]) << 32) | ew[1])
                        : ew[1];
                }
            }
        }
        pos += payload;
    }

    // Step 3: Verify `sizeof(struct page) == 64` (`1 << kStructPageShift`),
    // `folio::page` at byte offset `+0`, and 8-byte `page::flags` at byte offset `+0`.
    auto pit = structs_.find("page");
    uint32_t page_sz = (pit != structs_.end()) ? types_[pit->second].size_or_type : 0;
    if (page_sz != (1U << kStructPageShift)) {
        add_base_error("sizeof(struct page) in BTF is " + std::to_string(page_sz) +
                       " bytes, expected 64 bytes");
    }
    uint32_t off = 0, ftype = 0;
    if (structs_.find("folio") != structs_.end()) {
        uint32_t stripped = 0;
        if (!find_struct_field("folio", "page", off, ftype) || off != 0 ||
            (stripped = strip_qualifiers(ftype)) == 0 ||
            types_[stripped].kind != kBtfStruct ||
            strcmp(get_str(types_[stripped].name_off), "page") != 0) {
            add_base_error(
                "struct folio::page in BTF is missing or not at byte offset +0");
        }
    }
    if (!find_struct_field("page", "flags", off, ftype) || off != 0 ||
        !is_int_of_size(ftype, 8)) {
        add_base_error("struct page::flags in BTF is missing or not at byte offset +0");
    }

    // Step 4: Resolve `PG_head` in `enum pageflags` and locate the compound hugepage
    // order byte across Linux generations:
    // - `folio::_folio_order` (`u8` on Linux 6.6+),
    // - low byte of `folio::_flags_1` (little-endian `u64` on Linux 6.3-6.5), or
    // - `struct page[1].compound_order` (`page_sz + off` on Linux 5.4-6.2).
    uint64_t enum_val = 0;
    if (!find_enum_value("PG_head", enum_val) || enum_val >= 64)
        add_base_error("enum pageflags::PG_head not found in BTF");
    else
        plan_.pg_head_bit = static_cast<uint32_t>(enum_val);

    if (find_enum_value("FAULT_FLAG_WRITE", enum_val) && enum_val != 0)
        plan_.fault_flag_write = static_cast<uint32_t>(enum_val);
    if (find_enum_value("VM_FAULT_OOM", enum_val) && enum_val != 0) {
        uint32_t mask = static_cast<uint32_t>(enum_val);
        const char *err_names[] = { "VM_FAULT_SIGBUS",         "VM_FAULT_HWPOISON",
                                    "VM_FAULT_HWPOISON_LARGE", "VM_FAULT_SIGSEGV",
                                    "VM_FAULT_RETRY",          "VM_FAULT_FALLBACK" };
        for (const char *name : err_names) {
            if (find_enum_value(name, enum_val))
                mask |= static_cast<uint32_t>(enum_val);
        }
        plan_.vm_fault_error_mask = mask;
    }

    if (find_struct_field("folio", "_folio_order", off, ftype) &&
        is_int_of_size(ftype, 1)) {
        plan_.folio_order_byte_off = off;
    } else if (find_struct_field("folio", "_flags_1", off, ftype) &&
               is_int_of_size(ftype, 8)) {
        plan_.folio_order_byte_off = off;
    } else if (find_struct_field("page", "compound_order", off, ftype) &&
               is_int_of_size(ftype, 1)) {
        plan_.folio_order_byte_off = page_sz + off;
    } else {
        add_base_error("unable to resolve compound page/folio order offset in BTF");
    }
    return plan_.critical_errors.empty();
}

// Returns the NUL-terminated string at byte offset `off` in the BTF string section.
const char *
v2p_btf_checker_t::get_str(uint32_t off) const
{
    if (raw_ == nullptr || str_start_ + off >= str_end_)
        return "";
    const char *base = raw_->data() + str_start_ + off;
    return memchr(base, '\0', str_end_ - (str_start_ + off)) != nullptr ? base : "";
}

// Follows `typedef`, `const`, `volatile`, `restrict`, and `type_tag` wrappers to the
// underlying BTF type ID.
uint32_t
v2p_btf_checker_t::strip_qualifiers(uint32_t type_id) const
{
    for (int depth = 0; depth < 16 && type_id > 0 && type_id < types_.size(); ++depth) {
        uint32_t k = types_[type_id].kind;
        if (k == kBtfTypedef || k == kBtfVolatile || k == kBtfConst ||
            k == kBtfRestrict || k == kBtfTypeTag) {
            type_id = types_[type_id].size_or_type;
        } else {
            break;
        }
    }
    return type_id < types_.size() ? type_id : 0;
}

// Checks whether `type_id` is an integer, enum, or single-field wrapper struct (such as
// `atomic_t` or `swp_entry_t`) of exact width `byte_size`.
bool
v2p_btf_checker_t::is_int_of_size(uint32_t type_id, uint32_t byte_size) const
{
    type_id = strip_qualifiers(type_id);
    if (type_id == 0)
        return false;
    uint32_t k = types_[type_id].kind;
    if ((k == kBtfInt || k == kBtfEnum || k == kBtfEnum64) &&
        types_[type_id].size_or_type == byte_size) {
        return true;
    }
    if (k == kBtfStruct && types_[type_id].vlen == 1 &&
        types_[type_id].size_or_type == byte_size) {
        uint32_t mwords[3] = {};
        memcpy(mwords, raw_->data() + types_[type_id].payload_off, sizeof(mwords));
        uint32_t m_bits = types_[type_id].kflag ? (mwords[2] & 0xffffffU) : mwords[2];
        if (m_bits == 0)
            return is_int_of_size(mwords[1], byte_size);
    }
    return false;
}

// Resolves `struct_name::field_name` to a byte-aligned member offset and BTF type ID.
bool
v2p_btf_checker_t::find_struct_field(const char *struct_name, const char *field_name,
                                     uint32_t &byte_offset_out,
                                     uint32_t &field_type_out) const
{
    auto it = structs_.find(struct_name);
    if (it == structs_.end())
        return false;
    uint32_t bit_offset = 0;
    if (!search_members(it->second, field_name, 0, bit_offset, field_type_out) ||
        (bit_offset % 8) != 0) {
        return false;
    }
    byte_offset_out = bit_offset / 8;
    return true;
}

// Recursively searches struct/union `type_id` (including anonymous inner structs/unions)
// for a non-bitfield member named `field_name`.
bool
v2p_btf_checker_t::search_members(uint32_t type_id, const char *field_name,
                                  uint32_t base_bits, uint32_t &bit_offset_out,
                                  uint32_t &type_out, int depth) const
{
    type_id = strip_qualifiers(type_id);
    if (depth > 8 || type_id == 0 ||
        (types_[type_id].kind != kBtfStruct && types_[type_id].kind != kBtfUnion)) {
        return false;
    }
    const btf_type_entry_t &st = types_[type_id];
    for (uint32_t i = 0; i < st.vlen; ++i) {
        uint32_t mwords[3] = {};
        memcpy(mwords, raw_->data() + st.payload_off + 12 * static_cast<size_t>(i),
               sizeof(mwords));
        const char *mname = get_str(mwords[0]);
        uint32_t m_bits = base_bits + (st.kflag ? (mwords[2] & 0xffffffU) : mwords[2]);
        if (strcmp(mname, field_name) == 0) {
            // Reject bitfield members (`kflag` stores a non-zero bitfield width in
            // bits 24..31 of `mwords[2]`), which kprobe fetchargs cannot address as
            // byte-sized scalars.
            if (st.kflag && ((mwords[2] >> 24) != 0))
                return false;
            bit_offset_out = m_bits;
            type_out = mwords[1];
            return true;
        }
        // Recurse into anonymous `struct` / `union` members (`mname[0] == '\0'`).
        if (mname[0] == '\0' &&
            search_members(mwords[1], field_name, m_bits, bit_offset_out, type_out,
                           depth + 1)) {
            return true;
        }
    }
    return false;
}

// Looks up an enumerator constant in `enums_` and writes its value to `val_out`.
bool
v2p_btf_checker_t::find_enum_value(const char *enum_const, uint64_t &val_out) const
{
    auto it = enums_.find(enum_const);
    if (it == enums_.end())
        return false;
    val_out = it->second;
    return true;
}

// Checks whether `type_id` is a pointer to `struct <struct_name>` (either a complete
// `BTF_KIND_STRUCT` definition or an opaque `BTF_KIND_FWD` forward declaration).
bool
v2p_btf_checker_t::is_ptr_to_struct(uint32_t type_id, const char *struct_name) const
{
    type_id = strip_qualifiers(type_id);
    if (type_id == 0 || types_[type_id].kind != kBtfPtr)
        return false;
    uint32_t target = strip_qualifiers(types_[type_id].size_or_type);
    if (target == 0 ||
        (types_[target].kind != kBtfStruct && types_[target].kind != kBtfFwd)) {
        return false;
    }
    return strcmp(get_str(types_[target].name_off), struct_name) == 0;
}

// Matches a BTF parameter or struct-member `type_id` against a compact type spec
// (`"*"`, `"u8"`, `"u32"`, `"u64"`, `"folio|page*"`, or `"<struct_name>*"`).
bool
v2p_btf_checker_t::matches_type(uint32_t type_id, const char *spec) const
{
    if (strcmp(spec, "*") == 0)
        return true;
    if (strcmp(spec, "u8") == 0)
        return is_int_of_size(type_id, 1);
    if (strcmp(spec, "u32") == 0)
        return is_int_of_size(type_id, 4);
    if (strcmp(spec, "u64") == 0)
        return is_int_of_size(type_id, 8);
    if (strcmp(spec, "folio|page*") == 0)
        return is_ptr_to_struct(type_id, "folio") || is_ptr_to_struct(type_id, "page");
    size_t len = strlen(spec);
    if (len > 1 && spec[len - 1] == '*') {
        std::string sname(spec, len - 1);
        return is_ptr_to_struct(type_id, sname.c_str());
    }
    return false;
}

// Looks up `func_name`'s `BTF_KIND_FUNC_PROTO` record and extracts its parameter types.
bool
v2p_btf_checker_t::find_func_params(const char *func_name,
                                    std::vector<uint32_t> &param_types) const
{
    auto it = funcs_.find(func_name);
    if (it == funcs_.end())
        return false;
    uint32_t proto_id = strip_qualifiers(types_[it->second].size_or_type);
    if (proto_id == 0 || types_[proto_id].kind != kBtfFuncProto)
        return false;
    const btf_type_entry_t &proto = types_[proto_id];
    param_types.clear();
    for (uint32_t i = 0; i < proto.vlen; ++i) {
        uint32_t pwords[2] = {};
        memcpy(pwords, raw_->data() + proto.payload_off + 8 * static_cast<size_t>(i),
               sizeof(pwords));
        param_types.push_back(pwords[1]);
    }
    return true;
}

// Replaces `{<struct>.<field>:<type>}`, `{folio_order}`, and `{pg_head_mask}` tokens
// in a kprobe definition or filter string with the field's BTF-verified `+<byte_offset>`
// (or bitmask) for the running kernel.
bool
v2p_btf_checker_t::resolve_probe_offsets(std::string &str, std::string &err_out) const
{
    for (size_t lb = 0; (lb = str.find('{', lb)) != std::string::npos;) {
        size_t rb = str.find('}', lb + 1);
        if (rb == std::string::npos)
            return false;
        std::string tok = str.substr(lb + 1, rb - lb - 1);
        std::string repl;
        if (tok == "folio_order") {
            repl = "+" + std::to_string(plan_.folio_order_byte_off);
        } else if (tok == "pg_head_mask") {
            char buf[32];
            snprintf(buf, sizeof(buf), "0x%" PRIx64,
                     static_cast<uint64_t>(1) << plan_.pg_head_bit);
            repl = buf;
        } else {
            size_t dot = tok.find('.');
            size_t col = tok.find(':', dot == std::string::npos ? 0 : dot + 1);
            if (dot == std::string::npos || col == std::string::npos)
                return false;
            std::string sname = tok.substr(0, dot);
            std::string fname = tok.substr(dot + 1, col - dot - 1);
            std::string tspec = tok.substr(col + 1);
            uint32_t off = 0, ftype = 0;
            if (!find_struct_field(sname.c_str(), fname.c_str(), off, ftype) ||
                !matches_type(ftype, tspec.c_str())) {
                err_out =
                    "struct " + sname + "::" + fname + " missing or incompatible in BTF";
                return false;
            }
            repl = "+" + std::to_string(off);
        }
        str.replace(lb, rb - lb + 1, repl);
        lb += repl.size();
    }
    return true;
}

// Checks whether `cand` matches `cand.func_name`'s BTF parameter list and resolves all
// struct member byte offsets in its probes before appending them to `plan_.probes`.
bool
v2p_btf_checker_t::try_candidate(const v2p_hook_candidate_t &cand, std::string &err_out)
{
    // Parse comma-separated parameter type specs from `cand.param_sig`.
    std::vector<std::string> specs;
    for (size_t pos = 0; pos < cand.param_sig.size();) {
        size_t comma = cand.param_sig.find(',', pos);
        size_t end = (comma == std::string::npos) ? cand.param_sig.size() : comma;
        while (pos < end && cand.param_sig[pos] == ' ')
            ++pos;
        size_t trim = end;
        while (trim > pos && cand.param_sig[trim - 1] == ' ')
            --trim;
        if (trim > pos)
            specs.push_back(cand.param_sig.substr(pos, trim - pos));
        pos = (comma == std::string::npos) ? end : comma + 1;
    }

    // Verify parameter count and each parameter's BTF type against `specs`.
    std::vector<uint32_t> params;
    if (!find_func_params(cand.func_name, params) ||
        (cand.exact_param_count >= 0 &&
         params.size() != static_cast<size_t>(cand.exact_param_count)) ||
        params.size() < specs.size()) {
        err_out = "signature missing or incompatible in BTF";
        return false;
    }
    for (size_t i = 0; i < specs.size(); ++i) {
        if (!matches_type(params[i], specs[i].c_str())) {
            err_out = "signature missing or incompatible in BTF";
            return false;
        }
    }

    // Resolve `{struct.field:type}` byte offsets in each probe definition and filter.
    std::vector<v2p_probe_spec_t> resolved = cand.probes;
    for (v2p_probe_spec_t &p : resolved) {
        if (!resolve_probe_offsets(p.definition, err_out) ||
            !resolve_probe_offsets(p.static_filter, err_out)) {
            return false;
        }
        p.filter_by_mm = !p.is_return && p.definition.find("mm=") != std::string::npos;
        p.definition = std::string(cand.func_name) +
            (p.definition.empty() ? "" : " " + p.definition);
        p.hook_name = cand.func_name;
    }
    plan_.probes.insert(plan_.probes.end(), resolved.begin(), resolved.end());
    return true;
}

// Finds the first function in `candidates` present in kernel BTF and validates all
// candidate overloads for that symbol; records an error in `plan_.critical_errors` if
// no candidate matches.
bool
v2p_btf_checker_t::check_hook(const char *hook_name,
                              std::initializer_list<v2p_hook_candidate_t> candidates)
{
    const char *active_fn = nullptr;
    std::string detail_err = "signature missing or incompatible in BTF";
    for (const v2p_hook_candidate_t &cand : candidates) {
        if (!has_func(cand.func_name))
            continue;
        if (active_fn == nullptr)
            active_fn = cand.func_name;
        else if (strcmp(active_fn, cand.func_name) != 0)
            continue;
        if (try_candidate(cand, detail_err))
            return true;
    }
    const char *label = (active_fn != nullptr) ? active_fn : hook_name;
    const std::string detail = (active_fn != nullptr) ? detail_err : "not found in BTF";
    plan_.critical_errors.push_back(std::string(label) + " (" + detail + ")");
    return false;
}

// Parses `/sys/kernel/btf/vmlinux` at `btf_path`, validates base `page`/`folio`
// metadata layout and all registered kernel hooks, and populates `plan_out`.
bool
v2p_check_kernel_support(const std::string &btf_path, v2p_kernel_plan_t &plan_out,
                         std::string &error_out)
{
    plan_out = {};
    error_out.clear();

    v2p_btf_checker_t checker(btf_path, plan_out);
    std::string btf_raw;
    if (btf_path.empty() || !v2p_read_file_bytes(btf_path, btf_raw)) {
        const std::string label = btf_path.empty() ? "/sys/kernel/btf/vmlinux" : btf_path;
        error_out =
            label + " (unable to read kernel BTF, CONFIG_DEBUG_INFO_BTF required)";
        return false;
    }
    if (!checker.parse_and_check_base(btf_raw)) {
        error_out = plan_out.critical_errors.front();
        return false;
    }

    v2p_tracefs_hooks_t::check_all_hooks(checker, plan_out);
    if (!plan_out.critical_errors.empty()) {
        for (const std::string &err : plan_out.critical_errors) {
            if (!error_out.empty())
                error_out += "; ";
            error_out += err;
        }
        return false;
    }
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
