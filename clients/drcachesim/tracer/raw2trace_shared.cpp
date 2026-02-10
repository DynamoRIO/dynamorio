/* **********************************************************
 * Copyright (c) 2016-2026 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "raw2trace_shared.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>

#include "drmemtrace.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

#define WARN(msg, ...)                                        \
    do {                                                      \
        fprintf(stderr, "WARNING: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                       \
    } while (0)

#define VPRINT_HEADER()                    \
    do {                                   \
        fprintf(stderr, "[drmemtrace]: "); \
    } while (0)

// We fflush for Windows cygwin where stderr is not flushed.
#undef VPRINT
#define VPRINT(level, ...)                 \
    do {                                   \
        if (this->verbosity_ >= (level)) { \
            VPRINT_HEADER();               \
            fprintf(stderr, __VA_ARGS__);  \
            fflush(stderr);                \
        }                                  \
    } while (0)

bool
trace_metadata_reader_t::is_thread_start(const offline_entry_t *entry,
                                         DR_PARAM_OUT std::string *error,
                                         DR_PARAM_OUT int *version,
                                         DR_PARAM_OUT offline_file_type_t *file_type)
{
    *error = "";
    if (entry->extended.type != OFFLINE_TYPE_EXTENDED ||
        (entry->extended.ext != OFFLINE_EXT_TYPE_HEADER_DEPRECATED &&
         entry->extended.ext != OFFLINE_EXT_TYPE_HEADER)) {
        return false;
    }
    int ver;
    offline_file_type_t type;
    if (entry->extended.ext == OFFLINE_EXT_TYPE_HEADER_DEPRECATED) {
        ver = static_cast<int>(entry->extended.valueA);
        type = static_cast<offline_file_type_t>(entry->extended.valueB);
        if (ver >= OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP) {
            return false;
        }
    } else {
        ver = static_cast<int>(entry->extended.valueB);
        type = static_cast<offline_file_type_t>(entry->extended.valueA);
        if (ver < OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP) {
            return false;
        }
    }
    type = static_cast<offline_file_type_t>(static_cast<int>(type) |
                                            OFFLINE_FILE_TYPE_ENCODINGS);
    if (version != nullptr)
        *version = ver;
    if (file_type != nullptr)
        *file_type = type;
    if (ver < OFFLINE_FILE_VERSION_OLDEST_SUPPORTED || ver > OFFLINE_FILE_VERSION) {
        std::stringstream ss;
        ss << "Version mismatch: found " << ver << " but we require between "
           << OFFLINE_FILE_VERSION_OLDEST_SUPPORTED << " and " << OFFLINE_FILE_VERSION;
        *error = ss.str();
        return false;
    }
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL, type) &&
        !TESTANY(build_target_arch_type(), type)) {
        std::stringstream ss;
        ss << "Architecture mismatch: trace recorded on " << trace_arch_string(type)
           << " but tools built for " << trace_arch_string(build_target_arch_type());
        *error = ss.str();
        return false;
    }
    return true;
}

std::string
trace_metadata_reader_t::check_entry_thread_start(const offline_entry_t *entry)
{
    std::string error;
    if (is_thread_start(entry, &error, nullptr, nullptr))
        return "";
    if (error.empty())
        return "Thread log file is corrupted: missing version entry";
    return error;
}

drmemtrace_status_t
drmemtrace_get_timestamp_from_offline_trace(const void *trace, size_t trace_size,
                                            DR_PARAM_OUT uint64 *timestamp)
{
    if (trace == nullptr || timestamp == nullptr)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    const offline_entry_t *offline_entries =
        reinterpret_cast<const offline_entry_t *>(trace);
    size_t size = trace_size / sizeof(offline_entry_t);
    if (size < 1)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    std::string error;
    if (!trace_metadata_reader_t::is_thread_start(offline_entries, &error, nullptr,
                                                  nullptr) &&
        !error.empty())
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    size_t timestamp_pos = 0;
    while (timestamp_pos < size &&
           offline_entries[timestamp_pos].timestamp.type != OFFLINE_TYPE_TIMESTAMP) {
        if (timestamp_pos > 15) // Something is wrong if we've gone this far.
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        // We only expect header-type entries.
        int type = offline_entries[timestamp_pos].tid.type;
        if (type != OFFLINE_TYPE_THREAD && type != OFFLINE_TYPE_PID &&
            type != OFFLINE_TYPE_EXTENDED)
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        ++timestamp_pos;
    }
    if (timestamp_pos == size)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    *timestamp = offline_entries[timestamp_pos].timestamp.usec;
    return DRMEMTRACE_SUCCESS;
}

std::string
read_module_file(const std::string &modfilename, file_t &modfile, char *&modfile_bytes)
{
    modfile = dr_open_file(modfilename.c_str(), DR_FILE_READ);
    if (modfile == INVALID_FILE)
        return "Failed to open module file " + modfilename;
    uint64 modfile_size;
    if (!dr_file_size(modfile, &modfile_size)) {
        dr_close_file(modfile);
        modfile = INVALID_FILE;
        return "Failed to get module file size: " + modfilename;
    }
    size_t modfile_size_ = (size_t)modfile_size;
    modfile_bytes = new char[modfile_size_];
    if (dr_read_file(modfile, modfile_bytes, modfile_size_) < (ssize_t)modfile_size_) {
        dr_close_file(modfile);
        delete[] modfile_bytes;
        modfile = INVALID_FILE;
        modfile_bytes = nullptr;
        return "Didn't read whole module file " + modfilename;
    }
    return "";
}

// The output range is really a segment and not the whole module.
app_pc
module_mapper_t::find_mapped_trace_bounds(app_pc trace_address,
                                          DR_PARAM_OUT app_pc *module_start,
                                          DR_PARAM_OUT size_t *module_size)
{
    if (modvec_.empty()) {
        last_error_ = "Failed to call get_loaded_modules() first";
        return nullptr;
    }

    // For simplicity we do a linear search, caching the prior hit.
    if (trace_address >= last_orig_base_ &&
        trace_address < last_orig_base_ + last_map_size_) {
        if (module_start != nullptr)
            *module_start = last_map_base_;
        if (module_size != nullptr)
            *module_size = last_map_size_;
        return trace_address - last_orig_base_ + last_map_base_;
    }
    for (std::vector<module_t>::iterator mvi = modvec_.begin(); mvi != modvec_.end();
         ++mvi) {
        if (trace_address >= mvi->orig_seg_base &&
            trace_address < mvi->orig_seg_base + mvi->seg_size) {
            app_pc mapped_address =
                trace_address - mvi->orig_seg_base + mvi->map_seg_base;
            last_orig_base_ = mvi->orig_seg_base;
            last_map_size_ = mvi->seg_size;
            last_map_base_ = mvi->map_seg_base;
            if (module_start != nullptr)
                *module_start = last_map_base_;
            if (module_size != nullptr)
                *module_size = last_map_size_;
            return mapped_address;
        }
    }
    last_error_ = "Trace address not found";
    return nullptr;
}

app_pc
module_mapper_t::find_mapped_trace_address(app_pc trace_address)
{
    return find_mapped_trace_bounds(trace_address, nullptr, nullptr);
}

drcovlib_status_t
module_mapper_t::write_module_data(char *buf, size_t buf_size,
                                   int (*print_cb)(void *data, char *dst, size_t max_len),
                                   DR_PARAM_OUT size_t *wrote)
{
    user_print_ = print_cb;
    drcovlib_status_t res =
        drmodtrack_add_custom_data(nullptr, print_custom_module_data,
                                   parse_custom_module_data, free_custom_module_data);
    if (res == DRCOVLIB_SUCCESS) {
        res = drmodtrack_offline_write(modhandle_, buf, buf_size, wrote);
    }
    user_print_ = nullptr;
    return res;
}

// Maps each module into the address space.
// There are several types of mapping entries in the module list:
// 1) Raw bits directly stored.  It is simply pointed at.
// 2) Extra segments for a module.  A single mapping is used for all
//    segments, so extras are ignored.
// 3) A main segment.  The module's file is located by first looking in
//    the alt_module_dir_; if not found, the path present during tracing
//    is searched.
void
module_mapper_t::read_and_map_modules()
{
    if (!last_error_.empty())
        return;
    for (auto it = modlist_.begin(); it != modlist_.end(); ++it) {
        drmodtrack_info_t &info = *it;
        custom_module_data_t *custom_data = (custom_module_data_t *)info.custom;
        if (custom_data != nullptr && custom_data->contents_size > 0) {
            // These raw bytes for vdso is only present for legacy traces; we
            // use encoding entries for new traces.
            // XXX i#2062: Delete this code once we stop supporting legacy traces.
            VPRINT(1, "Using module %d %s stored %zd-byte contents @" PFX "\n",
                   (int)modvec_.size(), info.path, custom_data->contents_size,
                   custom_data->contents);
            modvec_.push_back(
                module_t(info.path, info.start, (byte *)custom_data->contents, 0,
                         custom_data->contents_size, custom_data->contents_size,
                         true /*external data*/));
        } else if (strcmp(info.path, "<unknown>") == 0 ||
                   // This should only happen with legacy trace data that's missing
                   // the vdso contents.
                   (!has_custom_data_ && strcmp(info.path, "[vdso]") == 0)) {
            // We won't be able to decode.
            modvec_.push_back(module_t(info.path, info.start, NULL, 0, 0, 0));
        } else if (info.containing_index != info.index) {
            // For split segments, we assume our mapped layout matches the original.
            byte *seg_map_base = modvec_[info.containing_index].map_seg_base -
                modvec_[info.containing_index].orig_seg_base + info.start;
            VPRINT(1, "Secondary segment: module %d seg %p-%p = %s\n",
                   (int)modvec_.size(), seg_map_base, seg_map_base + info.size,
                   info.path);
            // We did not map writable segments.  We can't easily detect an internal
            // unmapped writable segment, but for those off the end of our mapping we
            // can avoid pretending there's anything there.
            bool off_end =
                (size_t)(info.start - modvec_[info.containing_index].orig_seg_base) >=
                modvec_[info.containing_index].total_map_size;
            DR_ASSERT(off_end ||
                      info.start - modvec_[info.containing_index].orig_seg_base +
                              info.size <=
                          modvec_[info.containing_index].total_map_size);
            modvec_.push_back(module_t(
                info.path, info.start, off_end ? NULL : seg_map_base,
                off_end ? 0 : info.start - modvec_[info.containing_index].orig_seg_base,
                off_end ? 0 : info.size,
                // 0 total size indicates this is a secondary segment.
                0));
        } else {
            size_t map_size = 0;
            byte *base_pc = NULL;
            if (!alt_module_dir_.empty()) {
                // First try the specified module dir.  It takes precedence to allow
                // overriding the recorded path even when an identical-seeming path
                // exists on the processing machine (e.g., system libraries).
                // XXX: We should add a checksum on UNIX to match Windows and have
                // a sanity check on the library version.
                std::string basename(info.path);
                size_t sep_index = basename.find_last_of(DIRSEP ALT_DIRSEP);
                if (sep_index != std::string::npos)
                    basename = std::string(basename, sep_index + 1, std::string::npos);
                std::string new_path = alt_module_dir_ + DIRSEP + basename;
                VPRINT(2, "Trying to map %s\n", new_path.c_str());
                base_pc = dr_map_executable_file(new_path.c_str(),
                                                 DR_MAPEXE_SKIP_WRITABLE, &map_size);
            }
            if (base_pc == NULL) {
                // Try the recorded path.
                VPRINT(2, "Trying to map %s\n", info.path);
                base_pc =
                    dr_map_executable_file(info.path, DR_MAPEXE_SKIP_WRITABLE, &map_size);
            }
            if (base_pc == NULL) {
                // We expect to fail to map dynamorio.dll for x64 Windows as it
                // is built /fixed.  (We could try to have the map succeed w/o relocs,
                // but we expect to not care enough about code in DR).
                // We also expect to fail for vdso, for which we have encoding entries.
                if (strstr(info.path, "dynamorio") != nullptr ||
                    strstr(info.path, "linux-gate") != nullptr ||
                    strstr(info.path, "vdso") != nullptr)
                    modvec_.push_back(module_t(info.path, info.start, NULL, 0, 0, 0));
                else {
                    last_error_ = "Failed to map module " + std::string(info.path);
                    return;
                }
            } else {
                VPRINT(1, "Mapped module %d @%p-%p (-%p segment) = %s\n",
                       (int)modvec_.size(), base_pc, base_pc + map_size,
                       base_pc + info.size, info.path);
                // Be sure to only use the initial segment size to avoid covering
                // another mapping in a segment gap (i#4731).
                modvec_.push_back(
                    module_t(info.path, info.start, base_pc, 0, info.size, map_size));
            }
        }
    }
    VPRINT(1, "Successfully read %zu modules\n", modlist_.size());
}

std::string
module_mapper_t::do_module_parsing()
{
    uint num_mods;
    VPRINT(1, "Reading module file from memory\n");
    if (drmodtrack_add_custom_data(nullptr, nullptr, parse_custom_module_data,
                                   free_custom_module_data) != DRCOVLIB_SUCCESS) {
        return "Failed to set up custom module parser";
    }
    if (drmodtrack_offline_read(INVALID_FILE, modmap_, NULL, &modhandle_, &num_mods) !=
        DRCOVLIB_SUCCESS)
        return "Failed to parse module file";
    modlist_.resize(num_mods);
    for (uint i = 0; i < num_mods; i++) {
        modlist_[i].struct_size = sizeof(modlist_[i]);
        if (drmodtrack_offline_lookup(modhandle_, i, &modlist_[i]) != DRCOVLIB_SUCCESS)
            return "Failed to query module file";
        if (user_process_ != nullptr) {
            custom_module_data_t *custom = (custom_module_data_t *)modlist_[i].custom;
            std::string error =
                (*user_process_)(&modlist_[i], custom->user_data, user_process_data_);
            if (!error.empty())
                return error;
        }
    }
    return "";
}

std::string
module_mapper_t::do_encoding_parsing()
{
    if (encoding_file_ == INVALID_FILE)
        return "";
    uint64 file_size;
    if (!dr_file_size(encoding_file_, &file_size))
        return "Failed to obtain size of encoding file";
    size_t map_size = (size_t)file_size;
    byte *map_start = reinterpret_cast<byte *>(
        dr_map_file(encoding_file_, &map_size, 0, NULL, DR_MEMPROT_READ, 0));
    if (map_start == nullptr || map_size < file_size)
        return "Failed to map encoding file";
    byte *map_at = map_start;
    byte *map_end = map_start + file_size;
    uint64_t encoding_file_version = *reinterpret_cast<uint64_t *>(map_at);
    map_at += sizeof(uint64_t);
    if (encoding_file_version > ENCODING_FILE_VERSION)
        return "Encoding file has invalid version";
    if (encoding_file_version >= ENCODING_FILE_VERSION_HAS_FILE_TYPE) {
        if (map_at + sizeof(uint64_t) > map_end)
            return "Encoding file header is truncated";
        uint64_t encoding_file_type = *reinterpret_cast<uint64_t *>(map_at);
        map_at += sizeof(uint64_t);
        separate_non_mod_instrs_ =
            TESTANY(ENCODING_FILE_TYPE_SEPARATE_NON_MOD_INSTRS, encoding_file_type);
    }
    uint64_t cumulative_encoding_length = 0;
    while (map_at < map_end) {
        encoding_entry_t *entry = reinterpret_cast<encoding_entry_t *>(map_at);
        if (entry->length <= sizeof(encoding_entry_t))
            return "Encoding file is corrupted";
        if (map_at + entry->length > map_end)
            return "Encoding file is truncated";
        cum_block_enc_len_to_encoding_id_[cumulative_encoding_length] = entry->id;
        cumulative_encoding_length += (entry->length - sizeof(encoding_entry_t));
        encodings_[entry->id] = entry;
        map_at += entry->length;
    }
    return "";
}

const char *
module_mapper_t::parse_custom_module_data(const char *src, DR_PARAM_OUT void **data)
{
    const char *buf = src;
    const char *skip_comma = strchr(buf, ',');
    // Check the version # to try and handle legacy and newer formats.
    int version = -1;
    if (skip_comma == nullptr || dr_sscanf(buf, "v#%d,", &version) != 1 ||
        version != CUSTOM_MODULE_VERSION) {
        // It's not what we expect.  We try to handle legacy formats before bailing.
        static bool warned_once;
        has_custom_data_global_ = false;
        if (!warned_once) { // Race is fine: modtrack parsing is global already.
            WARN("Incorrect module field version %d: attempting to handle legacy format",
                 version);
            warned_once = true;
        }
        // First, see if the user_parse_ is happy:
        if (user_parse_ != nullptr) {
            void *user_data;
            buf = (*user_parse_)(buf, &user_data);
            if (buf != nullptr) {
                // Assume legacy format w/ user data but none of our own.
                custom_module_data_t *custom_data = new custom_module_data_t;
                custom_data->user_data = user_data;
                custom_data->contents_size = 0;
                custom_data->contents = nullptr;
                *data = custom_data;
                return buf;
            }
        }
        // Now look for no custom field at all.
        // If the next field looks like a path, we assume it's the old format with
        // no user field and we continue w/o vdso data.
        if (buf[0] == '/' || strstr(buf, "[vdso]") == buf) {
            *data = nullptr;
            return buf;
        }
        // Else, bail.
        WARN("Unable to parse module data: custom field mismatch");
        return nullptr;
    }
    buf = skip_comma + 1;
    skip_comma = strchr(buf, ',');
    size_t size;
    if (skip_comma == nullptr || dr_sscanf(buf, "%zu,", &size) != 1)
        return nullptr; // error
    custom_module_data_t *custom_data = new custom_module_data_t;
    custom_data->contents_size = size;
    buf = skip_comma + 1;
    if (custom_data->contents_size == 0)
        custom_data->contents = nullptr;
    else {
        custom_data->contents = buf;
        buf += custom_data->contents_size;
    }
    if (user_parse_ != nullptr)
        buf = (*user_parse_)(buf, &custom_data->user_data);
    *data = custom_data;
    return buf;
}

int
module_mapper_t::print_custom_module_data(void *data, char *dst, size_t max_len)
{
    custom_module_data_t *custom_data = (custom_module_data_t *)data;
    return print_module_data_fields(dst, max_len, custom_data->contents,
                                    custom_data->contents_size, user_print_,
                                    custom_data->user_data);
}

void
module_mapper_t::free_custom_module_data(void *data)
{
    custom_module_data_t *custom_data = (custom_module_data_t *)data;
    if (user_free_ != nullptr)
        (*user_free_)(custom_data->user_data);
    delete custom_data;
}

const char *(*module_mapper_t::user_parse_)(const char *src,
                                            DR_PARAM_OUT void **data) = nullptr;
void (*module_mapper_t::user_free_)(void *data) = nullptr;
int (*module_mapper_t::user_print_)(void *data, char *dst, size_t max_len) = nullptr;
bool module_mapper_t::has_custom_data_global_ = true;

module_mapper_t::module_mapper_t(
    const char *module_map,
    const char *(*parse_cb)(const char *src, DR_PARAM_OUT void **data),
    std::string (*process_cb)(drmodtrack_info_t *info, void *data, void *user_data),
    void *process_cb_user_data, void (*free_cb)(void *data), uint verbosity,
    const std::string &alt_module_dir, file_t encoding_file)
    : modmap_(module_map)
    , cached_user_free_(free_cb)
    , verbosity_(verbosity)
    , alt_module_dir_(alt_module_dir)
    , encoding_file_(encoding_file)
{
    // We mutate global state because do_module_parsing() uses drmodtrack, which
    // wants global functions. The state isn't needed past do_module_parsing(), so
    // we make sure to reset it afterwards.
    DR_ASSERT(user_parse_ == nullptr);
    DR_ASSERT(user_free_ == nullptr);
    DR_ASSERT(user_print_ == nullptr);

    user_parse_ = parse_cb;
    user_process_ = process_cb;
    user_process_data_ = process_cb_user_data;
    user_free_ = free_cb;
    // has_custom_data_global_ is potentially mutated in parse_custom_module_data.
    // It is assumed to be set to 'true' initially.
    has_custom_data_global_ = true;

    if (modmap_ != nullptr)
        last_error_ = do_module_parsing();
    if (encoding_file_ != INVALID_FILE)
        last_error_ += do_encoding_parsing();

    // capture has_custom_data_global_'s value for this instance.
    has_custom_data_ = has_custom_data_global_;

    user_parse_ = nullptr;
    user_free_ = nullptr;
}

module_mapper_t::~module_mapper_t()
{
    // update user_free_
    user_free_ = cached_user_free_;
    // drmodtrack_offline_exit requires the parameter to be non-null, but we
    // may not have even initialized the modhandle yet.
    if (modhandle_ != nullptr &&
        drmodtrack_offline_exit(modhandle_) != DRCOVLIB_SUCCESS) {
        WARN("Failed to clean up module table data");
    }
    user_free_ = nullptr;
    for (std::vector<module_t>::iterator mvi = modvec_.begin(); mvi != modvec_.end();
         ++mvi) {
        if (!mvi->is_external && mvi->map_seg_base != NULL && mvi->total_map_size != 0) {
            bool ok = dr_unmap_executable_file(mvi->map_seg_base, mvi->total_map_size);
            if (!ok)
                WARN("Failed to unmap module %s", mvi->path);
        }
    }
    modhandle_ = nullptr;
    modvec_.clear();
}

int
print_module_data_fields(char *dst, size_t max_len, const void *custom_data,
                         size_t custom_size,
                         int (*user_print_cb)(void *data, char *dst, size_t max_len),
                         void *user_cb_data)
{
    char *cur = dst;
    int len = dr_snprintf(dst, max_len, "v#%d,%zu,", CUSTOM_MODULE_VERSION, custom_size);
    if (len < 0)
        return -1;
    cur += len;
    if (cur - dst + custom_size > max_len)
        return -1;
    if (custom_size > 0) {
        memcpy(cur, custom_data, custom_size);
        cur += custom_size;
    }
    if (user_print_cb != nullptr) {
        int res = (*user_print_cb)(user_cb_data, cur, max_len - (cur - dst));
        if (res == -1)
            return -1;
        cur += res;
    }
    return (int)(cur - dst);
}

unsigned short
ir_utils_t::instr_to_instr_type(instr_t *instr, bool repstr_expanded)
{
    if (instr_is_call_direct(instr))
        return TRACE_TYPE_INSTR_DIRECT_CALL;
    if (instr_is_call_indirect(instr))
        return TRACE_TYPE_INSTR_INDIRECT_CALL;
    if (instr_is_return(instr))
        return TRACE_TYPE_INSTR_RETURN;
    if (instr_is_ubr(instr))
        return TRACE_TYPE_INSTR_DIRECT_JUMP;
    if (instr_is_mbr(instr)) // But not a return or call.
        return TRACE_TYPE_INSTR_INDIRECT_JUMP;
    if (instr_is_cbr(instr))
        return TRACE_TYPE_INSTR_CONDITIONAL_JUMP;
#ifdef X86
    if (instr_get_opcode(instr) == OP_sysenter
#    ifdef X86_32
        // i#7340: On x86-32, we assume that an OP_syscall may be present only
        // in the vdso __kernel_vsyscall on AMD machines. See notes in the
        // Linux implementation: https://github.com/torvalds/linux/blob/v6.13/
        // arch/x86/entry/entry_64_compat.S#L142
        // Also, as noted in PR #5037, this 32-bit AMD OP_syscall does _not_
        // return to the subsequent PC; the kernel sends control to a
        // hardcoded address in the __kernel_vsyscall sequence, thus acting
        // more like an OP_sysenter and requiring similar treatment.
        // We decided to not add a new TRACE_TYPE_INSTR_ for such syscalls,
        // as they are substantially similar to sysenter. However, note that
        // the user can still figure out the underlying opcode using the
        // instruction encodings in the trace.
        || instr_get_opcode(instr) == OP_syscall
#    endif
    )
        return TRACE_TYPE_INSTR_SYSENTER;
#endif
    // i#2051: to satisfy both cache and core simulators we mark subsequent iters
    // of string loops as TRACE_TYPE_INSTR_NO_FETCH, converted from this
    // TRACE_TYPE_INSTR_MAYBE_FETCH by reader_t (since online traces would need
    // extra instru to distinguish the 1st and subsequent iters).
    if (instr_is_rep_string_op(instr) || (repstr_expanded && instr_is_string_op(instr)))
        return TRACE_TYPE_INSTR_MAYBE_FETCH;
    return TRACE_TYPE_INSTR;
}

} // namespace drmemtrace
} // namespace dynamorio
