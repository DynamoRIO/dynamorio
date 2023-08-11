/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* Post-processes offline traces and converts them to the format expected
 * by the cache simulator and other analysis tools.
 */

#define NOMINMAX // Avoid windows.h messing up std::min.
#include "archive_ostream.h"
#include "dr_api.h"
#include "drcovlib.h"
#include "raw2trace.h"
#include "reader.h"
#include "memref.h"
#include "trace_entry.h"
#include "drmemtrace.h"
#include "instru.h"
#include "utils.h"
#ifdef LINUX
// XXX: We should have the core export this to an include dir.
#    include "../../core/unix/include/syscall.h"
#endif
#ifdef BUILD_PT_POST_PROCESSOR
#    include <unistd.h>
#    include "../common/options.h"
#    include "../drpt2trace/ir2trace.h"
#endif

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

// Assumes we return an error string by convention.
#define CHECK(val, msg) \
    do {                \
        if (!(val))     \
            return msg; \
    } while (0)

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

static online_instru_t instru(NULL, NULL, NULL);

int
trace_metadata_writer_t::write_thread_exit(byte *buffer, thread_id_t tid)
{
    return instru.append_thread_exit(buffer, tid);
}
int
trace_metadata_writer_t::write_marker(byte *buffer, trace_marker_type_t type,
                                      uintptr_t val)
{
    return instru.append_marker(buffer, type, val);
}
int
trace_metadata_writer_t::write_iflush(byte *buffer, addr_t start, size_t size)
{
    return instru.append_iflush(buffer, start, size);
}
int
trace_metadata_writer_t::write_pid(byte *buffer, process_id_t pid)
{
    return instru.append_pid(buffer, pid);
}
int
trace_metadata_writer_t::write_tid(byte *buffer, thread_id_t tid)
{
    return instru.append_tid(buffer, tid);
}
int
trace_metadata_writer_t::write_timestamp(byte *buffer, uint64 timestamp)
{
    return instru.append_marker(buffer, TRACE_MARKER_TYPE_TIMESTAMP,
                                // XXX i#5634: Truncated for 32-bit, as documented.
                                (uintptr_t)timestamp);
}

/***************************************************************************
 * Module list
 */

const char *(*module_mapper_t::user_parse_)(const char *src, OUT void **data) = nullptr;
void (*module_mapper_t::user_free_)(void *data) = nullptr;
int (*module_mapper_t::user_print_)(void *data, char *dst, size_t max_len) = nullptr;
bool module_mapper_t::has_custom_data_global_ = true;

module_mapper_t::module_mapper_t(
    const char *module_map, const char *(*parse_cb)(const char *src, OUT void **data),
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

std::string
raw2trace_t::handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                                std::string (*process_cb)(drmodtrack_info_t *info,
                                                          void *data, void *user_data),
                                void *process_cb_user_data, void (*free_cb)(void *data))
{
    user_parse_ = parse_cb;
    user_process_ = process_cb;
    user_process_data_ = process_cb_user_data;
    user_free_ = free_cb;
    return "";
}

const char *
module_mapper_t::parse_custom_module_data(const char *src, OUT void **data)
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
    return offline_instru_t::print_module_data_fields(
        dst, max_len, custom_data->contents, custom_data->contents_size, user_print_,
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

std::string
raw2trace_t::do_module_parsing()
{
    if (!module_mapper_) {
        module_mapper_ = module_mapper_t::create(
            modmap_bytes_, user_parse_, user_process_, user_process_data_, user_free_,
            verbosity_, alt_module_dir_, encoding_file_);
    }
    return module_mapper_->get_last_error();
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
    if (*reinterpret_cast<uint64_t *>(map_start) != ENCODING_FILE_VERSION)
        return "Encoding file has invalid version";
    size_t offs = sizeof(uint64_t);
    while (offs < file_size) {
        encoding_entry_t *entry = reinterpret_cast<encoding_entry_t *>(map_start + offs);
        if (entry->length < sizeof(encoding_entry_t))
            return "Encoding file is corrupted";
        if (offs + entry->length > file_size)
            return "Encoding file is truncated";
        encodings_[entry->id] = entry;
        offs += entry->length;
    }
    return "";
}

std::string
raw2trace_t::read_and_map_modules()
{
    if (!module_mapper_) {
        auto err = do_module_parsing();
        if (!err.empty())
            return err;
    }

    set_modvec_(&module_mapper_->get_loaded_modules());
    set_modmap_(module_mapper_.get());
    return module_mapper_->get_last_error();
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
            // XXX i#2062: We could eliminate this raw bytes in the module data in
            // favor of the new encoding file used for generated code.
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
            byte *seg_map_base = modvec_[info.containing_index].map_seg_base +
                (info.start - modvec_[info.containing_index].orig_seg_base);
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
                if (strstr(info.path, "dynamorio") != NULL)
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
raw2trace_t::do_module_parsing_and_mapping()
{
    std::string error = read_and_map_modules();
    if (!error.empty())
        return error;
    return "";
}

std::string
raw2trace_t::find_mapped_trace_address(app_pc trace_address, OUT app_pc *mapped_address)
{
    *mapped_address = module_mapper_->find_mapped_trace_address(trace_address);
    return module_mapper_->get_last_error();
}

// The output range is really a segment and not the whole module.
app_pc
module_mapper_t::find_mapped_trace_bounds(app_pc trace_address, OUT app_pc *module_start,
                                          OUT size_t *module_size)
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
                                   OUT size_t *wrote)
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

/***************************************************************************
 * Top-level
 */

std::string
raw2trace_t::process_offline_entry(raw2trace_thread_data_t *tdata,
                                   const offline_entry_t *in_entry, thread_id_t tid,
                                   OUT bool *end_of_record, OUT bool *last_bb_handled,
                                   OUT bool *flush_decode_cache)
{
    trace_entry_t *buf_base = get_write_buffer(tdata);
    byte *buf = reinterpret_cast<byte *>(buf_base);
    if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED) {
        if (in_entry->extended.ext == OFFLINE_EXT_TYPE_FOOTER) {
            DR_CHECK(tid != INVALID_THREAD_ID, "Missing thread id");
            log(2, "Thread %d exit\n", (uint)tid);
            buf += trace_metadata_writer_t::write_thread_exit(buf, tid);
            *end_of_record = true;
            std::string error =
                write(tdata, buf_base, reinterpret_cast<trace_entry_t *>(buf));
            if (!error.empty())
                return error;
            // Let the user determine what other actions to take, e.g. account for
            // the ending of the current thread, etc.
            return on_thread_end(tdata);
        } else if (in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER) {
            uintptr_t marker_val = 0;
            std::string err = get_marker_value(tdata, &in_entry, &marker_val);
            if (!err.empty())
                return err;
            buf += trace_metadata_writer_t::write_marker(
                buf, (trace_marker_type_t)in_entry->extended.valueB, marker_val);
            if (in_entry->extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT) {
                log(4, "Signal/exception between bbs\n");
                // An rseq side exit may next hit a signal which is then the
                // boundary of the rseq region.
                if (tdata->rseq_past_end_) {
                    err = adjust_and_emit_rseq_buffer(tdata, marker_val);
                    if (!err.empty())
                        return err;
                }
            } else if (in_entry->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ABORT) {
                log(4, "Rseq abort %d\n", tdata->rseq_past_end_);
                err = adjust_and_emit_rseq_buffer(tdata, marker_val, marker_val);
                if (!err.empty())
                    return err;
            } else if (in_entry->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ENTRY) {
                if (tdata->rseq_want_rollback_) {
                    if (tdata->rseq_buffering_enabled_) {
                        // Our rollback schemes do the minimal rollback: for a side
                        // exit, taking the last branch.  This means we don't need the
                        // prior iterations in the buffer.
                        log(4, "Rseq was already buffered: assuming loop; emitting\n");
                        err = adjust_and_emit_rseq_buffer(tdata, marker_val);
                        if (!err.empty())
                            return err;
                    }
                    log(4,
                        "--- Reached rseq entry (end=0x%zx): buffering all output ---\n",
                        marker_val);
                    if (!tdata->rseq_ever_saw_entry_)
                        tdata->rseq_ever_saw_entry_ = true;
                    tdata->rseq_buffering_enabled_ = true;
                    tdata->rseq_end_pc_ = marker_val;
                }
            } else if (in_entry->extended.valueB == TRACE_MARKER_TYPE_FILTER_ENDPOINT) {
                log(2, "Reached filter endpoint\n");

                // The file type needs to be updated during the switch to correctly
                // process the entries that follow after. This does not affect the
                // written-out type.
                int file_type = get_file_type(tdata);
                // We do not remove OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP here
                // because that still stands true for this trace.
                file_type &= ~(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED |
                               OFFLINE_FILE_TYPE_DFILTERED);
                set_file_type(tdata, (offline_file_type_t)file_type);

                // For the full trace, the cache contains block-level info unlike the
                // filtered trace which contains instr-level info. Since we cannot use
                // the decode cache entries after the transition, we need to flush the
                // cache here.
                *flush_decode_cache = true;
            } else if (in_entry->extended.valueB == TRACE_MARKER_TYPE_SYSCALL &&
                       is_maybe_blocking_syscall(marker_val)) {
                log(2, "Maybe-blocking syscall %zu\n", marker_val);
                buf += trace_metadata_writer_t::write_marker(
                    buf, TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL, 0);
            }
            // If there is currently a delayed branch that has not been emitted yet,
            // delay most markers since intra-block markers can cause issues with
            // tools that do not expect markers amid records for a single instruction
            // or inside a basic block. We don't delay TRACE_MARKER_TYPE_CPU_ID which
            // identifies the CPU on which subsequent records were collected and
            // OFFLINE_TYPE_TIMESTAMP which is handled at a higher level in
            // process_next_thread_buffer() so there is no need to have a separate
            // check for it here.
            if (in_entry->extended.valueB != TRACE_MARKER_TYPE_CPU_ID) {
                if (delayed_branches_exist(tdata)) {
                    std::string error = write_delayed_branches(
                        tdata, buf_base, reinterpret_cast<trace_entry_t *>(buf));
                    if (!error.empty())
                        return error;
                    return "";
                }
            }
            log(3, "Appended marker type %u value " PIFX "\n",
                (trace_marker_type_t)in_entry->extended.valueB,
                (uintptr_t)in_entry->extended.valueA);
        } else {
            std::stringstream ss;
            ss << "Invalid extension type " << (int)in_entry->extended.ext;
            return ss.str();
        }
    } else if (in_entry->addr.type == OFFLINE_TYPE_MEMREF ||
               in_entry->addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
        if (!*last_bb_handled &&
            // Shouldn't get here if encodings are present.
            get_version(tdata) < OFFLINE_FILE_VERSION_ENCODINGS) {
            // For legacy traces with unhandled non-module code, memrefs are handled
            // here where we can easily handle the transition out of the bb.
            trace_entry_t *entry = reinterpret_cast<trace_entry_t *>(buf);
            entry->type = TRACE_TYPE_READ; // Guess.
            entry->size = 1;               // Guess.
            entry->addr = (addr_t)in_entry->combined_value;
            log(4, "Appended non-module memref to " PFX "\n", (ptr_uint_t)entry->addr);
            buf += sizeof(*entry);
        } else {
            // We should see an instr entry first
            log(3, "extra memref entry: %p\n", in_entry->addr.addr);
            return "memref entry found outside of bb";
        }
    } else if (in_entry->pc.type == OFFLINE_TYPE_PC) {
        DR_CHECK(reinterpret_cast<trace_entry_t *>(buf) == buf_base,
                 "We shouldn't have buffered anything before calling "
                 "append_bb_entries");
        std::string result = append_bb_entries(tdata, in_entry, last_bb_handled);
        if (!result.empty())
            return result;
    } else if (in_entry->addr.type == OFFLINE_TYPE_IFLUSH) {
        const offline_entry_t *entry = get_next_entry(tdata);
        if (entry == nullptr || entry->addr.type != OFFLINE_TYPE_IFLUSH)
            return "Flush missing 2nd entry";
        log(2, "Flush " PFX "-" PFX "\n", (ptr_uint_t)in_entry->addr.addr,
            (ptr_uint_t)entry->addr.addr);
        buf += trace_metadata_writer_t::write_iflush(
            buf, in_entry->addr.addr, (size_t)(entry->addr.addr - in_entry->addr.addr));
    } else {
        std::stringstream ss;
        ss << "Unknown trace type " << (int)in_entry->timestamp.type;
        return ss.str();
    }
    size_t size = reinterpret_cast<trace_entry_t *>(buf) - buf_base;
    DR_CHECK((uint)size < WRITE_BUFFER_SIZE, "Too many entries");
    if (size > 0) {
        std::string error =
            write(tdata, buf_base, reinterpret_cast<trace_entry_t *>(buf));
        if (!error.empty())
            return error;
    }
    return "";
}

std::string
raw2trace_t::read_header(raw2trace_thread_data_t *tdata, OUT trace_header_t *header)
{
    const offline_entry_t *in_entry = get_next_entry(tdata);
    if (in_entry == nullptr)
        return "Failed to read header from input file";
    // Handle legacy traces which have the timestamp first.
    if (in_entry->tid.type == OFFLINE_TYPE_TIMESTAMP) {
        header->timestamp = in_entry->timestamp.usec;
        in_entry = get_next_entry(tdata);
        if (in_entry == nullptr)
            return "Failed to read header from input file";
    }
    DR_ASSERT(in_entry->tid.type == OFFLINE_TYPE_THREAD);
    header->tid = in_entry->tid.tid;

    in_entry = get_next_entry(tdata);
    if (in_entry == nullptr)
        return "Failed to read header from input file";
    DR_ASSERT(in_entry->pid.type == OFFLINE_TYPE_PID);
    header->pid = in_entry->pid.pid;

    in_entry = get_next_entry(tdata);
    if (in_entry == nullptr)
        return "Failed to read header from input file";
    if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
        in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
        in_entry->extended.valueB == TRACE_MARKER_TYPE_CACHE_LINE_SIZE) {
        header->cache_line_size = in_entry->extended.valueA;
    } else {
        log(2,
            "Cache line size not found in raw trace header. Adding "
            "current processor's cache line size to final trace instead.\n");
        header->cache_line_size = proc_get_cache_line_size();
        unread_last_entry(tdata);
    }
    return "";
}

std::string
raw2trace_t::process_header(raw2trace_thread_data_t *tdata)
{
    int version = tdata->version < OFFLINE_FILE_VERSION_KERNEL_INT_PC
        ? TRACE_ENTRY_VERSION_NO_KERNEL_PC
        : TRACE_ENTRY_VERSION;
    trace_entry_t entry;
    entry.type = TRACE_TYPE_HEADER;
    entry.size = 0;
    entry.addr = version;
    std::string error = write(tdata, &entry, &entry + 1);
    if (!error.empty())
        return error;

    // First read the tid and pid entries which precede any timestamps.
    trace_header_t header = { static_cast<process_id_t>(INVALID_PROCESS_ID),
                              INVALID_THREAD_ID, 0 };
    error = read_header(tdata, &header);
    if (!error.empty())
        return error;
    VPRINT(2, "File %u is thread %u\n", tdata->index, (uint)header.tid);
    VPRINT(2, "File %u is process %u\n", tdata->index, (uint)header.pid);
    thread_id_t tid = header.tid;
    tdata->tid = tid;
#ifdef BUILD_PT_POST_PROCESSOR
    if (TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS, tdata->file_type)) {
        DR_ASSERT(tdata->kthread_file == nullptr);
        auto it = kthread_files_map_.find(tid);
        if (it != kthread_files_map_.end()) {
            tdata->kthread_file = it->second;
        }
    }
#endif
    tdata->cache_line_size = header.cache_line_size;
    // We can't adjust filtered instructions, so we disable buffering.
    if (!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                 get_file_type(tdata)))
        tdata->rseq_want_rollback_ = true;
    process_id_t pid = header.pid;
    DR_ASSERT(tid != INVALID_THREAD_ID);
    DR_ASSERT(pid != (process_id_t)INVALID_PROCESS_ID);
    byte *buf_base = reinterpret_cast<byte *>(get_write_buffer(tdata));
    byte *buf = buf_base;
    // Write the version, arch, and other type flags.
    buf += instru.append_marker(buf, TRACE_MARKER_TYPE_VERSION, version);
    buf += instru.append_marker(buf, TRACE_MARKER_TYPE_FILETYPE, tdata->file_type);
    buf += trace_metadata_writer_t::write_tid(buf, tid);
    buf += trace_metadata_writer_t::write_pid(buf, pid);
    buf += trace_metadata_writer_t::write_marker(buf, TRACE_MARKER_TYPE_CACHE_LINE_SIZE,
                                                 header.cache_line_size);
    // The buffer can only hold 5 entries so write it now.
    CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
    error = write(tdata, reinterpret_cast<trace_entry_t *>(buf_base),
                  reinterpret_cast<trace_entry_t *>(buf));
    if (!error.empty())
        return error;
    buf_base = reinterpret_cast<byte *>(get_write_buffer(tdata));
    buf = buf_base;
    // Write out further markers.
    // Even if tdata->out_archive == nullptr we write out a (0-valued) marker,
    // partly to simplify our test output.
    buf += instru.append_marker(buf, TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT,
                                // i#5634: 32-bit is limited to 4G.
                                static_cast<uintptr_t>(chunk_instr_count_));
    if (header.timestamp != 0) {
        // Legacy traces have the timestamp in the header.
        buf += trace_metadata_writer_t::write_timestamp(buf, (uintptr_t)header.timestamp);
        tdata->last_timestamp_ = header.timestamp;
    }
    // We have to write this now before we append any bb entries.
    CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
    error = write(tdata, reinterpret_cast<trace_entry_t *>(buf_base),
                  reinterpret_cast<trace_entry_t *>(buf));
    if (!error.empty())
        return error;
    return "";
}

#ifdef BUILD_PT_POST_PROCESSOR
std::string
raw2trace_t::process_syscall_pt(raw2trace_thread_data_t *tdata, uint64_t syscall_idx)
{
    DR_ASSERT(tdata->kthread_file != nullptr);
    DR_ASSERT(TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS, tdata->file_type));

    if (!tdata->pt_metadata_processed) {
        DR_ASSERT(syscall_idx == 0);
        syscall_pt_entry_t header[PT_METADATA_PDB_HEADER_ENTRY_NUM];
        if (!tdata->kthread_file->read((char *)&header[0], PT_METADATA_PDB_HEADER_SIZE)) {
            return "Unable to read the PDB header of PT metadate form kernel thread log "
                   "file";
        }
        if (header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_metadata_boundary.type !=
            SYSCALL_PT_ENTRY_TYPE_PT_METADATA_BOUNDARY) {
            return "Invalid PT raw trace format";
        }

        struct {
            uint16_t cpu_family;
            uint8_t cpu_model;
            uint8_t cpu_stepping;
            uint16_t time_shift;
            uint32_t time_mult;
            uint64_t time_zero;
        } __attribute__((__packed__)) metadata;
        if (!tdata->kthread_file->read((char *)&metadata, sizeof(metadata))) {
            return "Unable to read the PT metadate form kernel thread log file";
        }

        pt2ir_config_t config = {};
        config.elf_file_path = kcore_path_;
        config.init_with_metadata(&metadata);

        /* Set the buffer size to be at least the maximum stream data size.
         */
#    define RING_BUFFER_SIZE_SHIFT 8
        config.pt_raw_buffer_size =
            (1L << RING_BUFFER_SIZE_SHIFT) * sysconf(_SC_PAGESIZE);
        if (!tdata->pt2ir.init(config, verbosity_)) {
            return "Unable to initialize PT2IR";
        }
        tdata->pt_metadata_processed = true;
    }

    if (tdata->pre_read_pt_entries.empty() && tdata->kthread_file->eof()) {
        VPRINT(1, "Finished decoding all PT data for thread %d\n", tdata->tid);
        return "";
    }

    if (tdata->pre_read_pt_entries.empty()) {
        syscall_pt_entry_t header[PT_DATA_PDB_HEADER_ENTRY_NUM];
        if (!tdata->kthread_file->read((char *)&header[0], PT_DATA_PDB_HEADER_SIZE)) {
            if (tdata->kthread_file->eof()) {
                VPRINT(1, "Finished decoding all PT data for thread %d\n", tdata->tid);
                return "";
            }
            return "Unable to read the PDB header of next syscall's PT data form kernel "
                   "thread log "
                   "file";
        }
        tdata->pre_read_pt_entries.insert(tdata->pre_read_pt_entries.end(), header,
                                          header + PT_DATA_PDB_HEADER_ENTRY_NUM);
    }

    if (tdata->pre_read_pt_entries[PDB_HEADER_DATA_BOUNDARY_IDX].pt_data_boundary.type !=
            SYSCALL_PT_ENTRY_TYPE_PT_DATA_BOUNDARY ||
        tdata->pre_read_pt_entries[PDB_HEADER_SYSCALL_IDX_IDX].syscall_idx.type !=
            SYSCALL_PT_ENTRY_TYPE_SYSCALL_IDX ||
        tdata->pre_read_pt_entries[PDB_HEADER_SYSCALL_IDX_IDX].syscall_idx.idx !=
            syscall_idx) {
        return "Invalid PT raw trace format";
    }

    uint64_t sysnum = tdata->pre_read_pt_entries[PDB_HEADER_SYSNUM_IDX].sysnum.sysnum;
    uint64_t syscall_args_num =
        tdata->pre_read_pt_entries[PDB_HEADER_NUM_ARGS_IDX].syscall_args_num.args_num;
    uint64_t pt_data_size = tdata->pre_read_pt_entries[PDB_HEADER_DATA_BOUNDARY_IDX]
                                .pt_data_boundary.data_size -
        SYSCALL_METADATA_SIZE - syscall_args_num * sizeof(uint64_t);
    tdata->pre_read_pt_entries.clear();
    std::unique_ptr<uint8_t[]> pt_data(new uint8_t[pt_data_size]);
    if (!tdata->kthread_file->read((char *)pt_data.get(), pt_data_size)) {
        return "Unable to read the PT data of syscall " + std::to_string(syscall_idx) +
            " sysnum " + std::to_string(sysnum) + " form kernel thread log file";
    }

    /* Convert the PT Data to DR IR. */
    drir_t drir(GLOBAL_DCONTEXT);
    pt2ir_convert_status_t pt2ir_convert_status =
        tdata->pt2ir.convert(pt_data.get(), pt_data_size, drir);
    if (pt2ir_convert_status != PT2IR_CONV_SUCCESS) {
        return "Failed to convert PT raw trace to DR IR [error status: " +
            std::to_string(pt2ir_convert_status) + "]";
    }

    /* Convert the DR IR to trace entries. */
    std::vector<trace_entry_t> entries;
    trace_entry_t start_entry = { .type = TRACE_TYPE_MARKER,
                                  .size = TRACE_MARKER_TYPE_SYSCALL_TRACE_START,
                                  .addr = 0 };
    entries.push_back(start_entry);
    ir2trace_convert_status_t ir2trace_convert_status =
        ir2trace_t::convert(drir, entries);
    if (ir2trace_convert_status != IR2TRACE_CONV_SUCCESS) {
        return "Failed to convert DR IR to trace entries [error status: " +
            std::to_string(ir2trace_convert_status) + "]";
    }
    trace_entry_t end_entry = { .type = TRACE_TYPE_MARKER,
                                .size = TRACE_MARKER_TYPE_SYSCALL_TRACE_END,
                                .addr = 0 };
    entries.push_back(end_entry);
    if (entries.size() == 2) {
        return "No trace entries generated from PT data";
    }

    if (!tdata->out_file->write(reinterpret_cast<const char *>(entries.data()),
                                sizeof(trace_entry_t) * entries.size())) {
        return "Failed to write to output file";
    }

    return "";
}
#endif

std::string
raw2trace_t::process_next_thread_buffer(raw2trace_thread_data_t *tdata,
                                        OUT bool *end_of_record)
{
    // We now convert each offline entry into a trace_entry_t.
    // We fill in instr entries and memref type and size.
    const offline_entry_t *in_entry = get_next_entry(tdata);
    if (!tdata->saw_header) {
        // We look for the initial header here rather than the top of
        // process_thread_file() to support use cases where buffers are passed from
        // another source.
        tdata->saw_header = trace_metadata_reader_t::is_thread_start(
            in_entry, &tdata->error, &tdata->version, &tdata->file_type);
        VPRINT(2, "Trace file version is %d; type is %d\n", tdata->version,
               tdata->file_type);
        if (!tdata->error.empty())
            return tdata->error;
        // We do not complain if tdata->version >= OFFLINE_FILE_VERSION_ENCODINGS
        // and encoding_file_ == INVALID_FILE since we have several tests with
        // that setup.  We do complain during processing about unknown instructions.
        if (tdata->saw_header) {
            tdata->error = process_header(tdata);
            if (!tdata->error.empty())
                return tdata->error;
        }
        in_entry = get_next_entry(tdata);
    }
    byte *buf_base = reinterpret_cast<byte *>(get_write_buffer(tdata));
    bool last_bb_handled = true;
    for (; in_entry != nullptr; in_entry = get_next_entry(tdata)) {
        // Make a copy to avoid clobbering the entry we pass to process_offline_entry()
        // when it calls get_next_entry() on its own.
        offline_entry_t entry = *in_entry;
        if (entry.timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
            VPRINT(2, "Thread %u timestamp 0x" ZHEX64_FORMAT_STRING "\n",
                   (uint)tdata->tid, (uint64)entry.timestamp.usec);
            accumulate_to_statistic(tdata, RAW2TRACE_STAT_EARLIEST_TRACE_TIMESTAMP,
                                    static_cast<uint64>(entry.timestamp.usec));
            accumulate_to_statistic(tdata, RAW2TRACE_STAT_LATEST_TRACE_TIMESTAMP,
                                    static_cast<uint64>(entry.timestamp.usec));
            byte *buf = buf_base +
                trace_metadata_writer_t::write_timestamp(buf_base,
                                                         (uintptr_t)entry.timestamp.usec);
            tdata->last_timestamp_ = entry.timestamp.usec;
            CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
            tdata->error = write(tdata, reinterpret_cast<trace_entry_t *>(buf_base),
                                 reinterpret_cast<trace_entry_t *>(buf));
            if (!tdata->error.empty())
                return tdata->error;
            continue;
        }
#ifdef BUILD_PT_POST_PROCESSOR
        if (entry.extended.type == OFFLINE_TYPE_EXTENDED &&
            entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
            entry.extended.valueB == TRACE_MARKER_TYPE_SYSCALL_IDX) {
            tdata->error = process_syscall_pt(tdata, entry.extended.valueA);
            if (!tdata->error.empty())
                return tdata->error;
            continue;
        }
#endif
        // Append delayed branches at the end or before xfer or window-change
        // markers; else, delay until we see a non-cti inside a block, to handle
        // double branches (i#5141) and to group all (non-xfer) markers with a new
        // timestamp.
        if (entry.extended.type == OFFLINE_TYPE_EXTENDED &&
            (entry.extended.ext == OFFLINE_EXT_TYPE_FOOTER ||
             (entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
              (entry.extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT ||
               entry.extended.valueB == TRACE_MARKER_TYPE_KERNEL_XFER ||
               (entry.extended.valueB == TRACE_MARKER_TYPE_WINDOW_ID &&
                entry.extended.valueA != tdata->last_window))))) {
            app_pc next_pc = nullptr;
            // Get the next instr's pc from the interruption value in the marker
            // (a record for the next instr itself won't appear until the signal
            // returns, if that happens).
            if (entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
                entry.extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT) {
                uintptr_t marker_val = 0;
                std::string err = get_marker_value(tdata, &in_entry, &marker_val);
                if (!err.empty())
                    return err;
                next_pc = reinterpret_cast<app_pc>(marker_val);
                // Restore in case it was a two-record value.
                unread_last_entry(tdata);
                in_entry = get_next_entry(tdata);
                entry = *in_entry;
            } // Else we will delete the final branch in append_delayed_branch().
            tdata->error = append_delayed_branch(tdata, next_pc);
            if (!tdata->error.empty())
                return tdata->error;
        }
        if (entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
            entry.extended.valueB == TRACE_MARKER_TYPE_WINDOW_ID)
            tdata->last_window = entry.extended.valueA;
        bool flush_decode_cache = false;
        tdata->error = process_offline_entry(tdata, &entry, tdata->tid, end_of_record,
                                             &last_bb_handled, &flush_decode_cache);
        if (flush_decode_cache)
            decode_cache_[tdata->worker].clear();
        if (!tdata->error.empty())
            return tdata->error;
    }
    tdata->error = "";
    return "";
}

std::string
raw2trace_t::process_thread_file(raw2trace_thread_data_t *tdata)
{
    bool end_of_file = false;
    while (!end_of_file) {
        VPRINT(4, "About to read thread #%d==%d at pos %d\n", tdata->index,
               (uint)tdata->tid, (int)tdata->thread_file->tellg());
        tdata->error = process_next_thread_buffer(tdata, &end_of_file);
        if (!tdata->error.empty() || (!end_of_file && thread_file_at_eof(tdata))) {
            if (thread_file_at_eof(tdata)) {
                // Rather than a fatal error we try to continue to provide partial
                // results in case the disk was full or there was some other issue.
                WARN("Input file for thread %d is truncated", (uint)tdata->tid);
                offline_entry_t entry;
                entry.extended.type = OFFLINE_TYPE_EXTENDED;
                entry.extended.ext = OFFLINE_EXT_TYPE_FOOTER;
                bool last_bb_handled = true, flush_decode_cache = false;
                tdata->error =
                    process_offline_entry(tdata, &entry, tdata->tid, &end_of_file,
                                          &last_bb_handled, &flush_decode_cache);
                if (flush_decode_cache)
                    decode_cache_[tdata->worker].clear();
                CHECK(end_of_file, "Synthetic footer failed");
                if (!tdata->error.empty())
                    return tdata->error;
            } else {
                std::stringstream ss;
                ss << "Failed to process file for thread " << (uint)tdata->tid << ": "
                   << tdata->error;
                tdata->error = ss.str();
                return tdata->error;
            }
        }
    }
    // The footer is written out by on_thread_end().
    tdata->error = "";
    return "";
}

std::string
raw2trace_t::check_thread_file(std::istream *f)
{
    // Check version header.
    offline_entry_t ver_entry;
    if (!f->read((char *)&ver_entry, sizeof(ver_entry))) {
        return "Unable to read thread log file";
    }
    // Put it back.
    f->seekg(-(std::streamoff)sizeof(ver_entry), f->cur);
    return trace_metadata_reader_t::check_entry_thread_start(&ver_entry);
}

#ifdef BUILD_PT_POST_PROCESSOR
std::string
raw2trace_t::get_kthread_file_tid(std::istream *f, thread_id_t *tid)
{
    syscall_pt_entry_t header[PT_METADATA_PDB_HEADER_ENTRY_NUM];
    if (!f->read((char *)&header[0], PT_METADATA_PDB_HEADER_SIZE)) {
        return "Unable to read kernel thread trace file";
    }
    f->seekg(-(std::streamoff)(PT_METADATA_PDB_HEADER_SIZE), f->cur);
    if (header[PDB_HEADER_TID_IDX].tid.type != SYSCALL_PT_ENTRY_TYPE_THREAD_ID) {
        *tid = INVALID_THREAD_ID;
        return "Unable to read thread id from kernel thread trace file";
    }
    *tid = header[PDB_HEADER_TID_IDX].tid.tid;
    return "";
}

std::string
raw2trace_t::check_kthread_file(std::istream *f)
{
    syscall_pt_entry_t header[PT_METADATA_PDB_HEADER_ENTRY_NUM];
    if (!f->read((char *)&header[0], PT_METADATA_PDB_HEADER_SIZE)) {
        return "Unable to read kernel thread log file";
    }
    f->seekg(-(std::streamoff)(PT_METADATA_PDB_HEADER_SIZE), f->cur);
    if (header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_metadata_boundary.type !=
        SYSCALL_PT_ENTRY_TYPE_PT_METADATA_BOUNDARY) {
        return "Kernel thread log file is corrupted: missing thread shared PT metadata";
    }
    return "";
}
#endif

void
raw2trace_t::process_tasks(std::vector<raw2trace_thread_data_t *> *tasks)
{
    if (tasks->empty()) {
        VPRINT(1, "Worker has no tasks\n");
        return;
    }
    VPRINT(1, "Worker %d assigned %zd task(s)\n", (*tasks)[0]->worker, tasks->size());
    for (raw2trace_thread_data_t *tdata : *tasks) {
        VPRINT(1, "Worker %d starting on trace thread %d\n", tdata->worker, tdata->index);
        std::string error = process_thread_file(tdata);
        if (!error.empty()) {
            VPRINT(1, "Worker %d hit error %s on trace thread %d\n", tdata->worker,
                   error.c_str(), tdata->index);
            break;
        }
        VPRINT(1, "Worker %d finished trace thread %d\n", tdata->worker, tdata->index);
    }
}

std::string
raw2trace_t::do_conversion()
{
    std::string error = read_and_map_modules();
    if (!error.empty())
        return error;
    if (thread_data_.empty())
        return "No thread files found.";
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (worker_count_ == 0) {
        for (size_t i = 0; i < thread_data_.size(); ++i) {
            error = process_thread_file(thread_data_[i].get());
            if (!error.empty())
                return error;
            count_elided_ += thread_data_[i]->count_elided;
            count_duplicate_syscall_ += thread_data_[i]->count_duplicate_syscall;
            count_false_syscall_ += thread_data_[i]->count_false_syscall;
            count_rseq_abort_ += thread_data_[i]->count_rseq_abort;
            count_rseq_side_exit_ += thread_data_[i]->count_rseq_side_exit;
            earliest_trace_timestamp_ = std::min(
                earliest_trace_timestamp_, thread_data_[i]->earliest_trace_timestamp);
            latest_trace_timestamp_ = std::max(latest_trace_timestamp_,
                                               thread_data_[i]->latest_trace_timestamp);
        }
    } else {
        // The files can be converted concurrently.
        std::vector<std::thread> threads;
        VPRINT(1, "Creating %d worker threads\n", worker_count_);
        threads.reserve(worker_count_);
        for (int i = 0; i < worker_count_; ++i) {
            threads.push_back(
                std::thread(&raw2trace_t::process_tasks, this, &worker_tasks_[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        for (auto &tdata : thread_data_) {
            if (!tdata->error.empty())
                return tdata->error;
            count_elided_ += tdata->count_elided;
            count_duplicate_syscall_ += tdata->count_duplicate_syscall;
            count_false_syscall_ += tdata->count_false_syscall;
            count_rseq_abort_ += tdata->count_rseq_abort;
            count_rseq_side_exit_ += tdata->count_rseq_side_exit;
            earliest_trace_timestamp_ =
                std::min(earliest_trace_timestamp_, tdata->earliest_trace_timestamp);
            latest_trace_timestamp_ =
                std::max(latest_trace_timestamp_, tdata->latest_trace_timestamp);
        }
    }
    error = aggregate_and_write_schedule_files();
    if (!error.empty())
        return error;
    VPRINT(1, "Omitted " UINT64_FORMAT_STRING " duplicate system calls.\n",
           count_duplicate_syscall_);
    VPRINT(1, "Omitted " UINT64_FORMAT_STRING " false system calls.\n",
           count_false_syscall_);
    VPRINT(1, "Reconstructed " UINT64_FORMAT_STRING " elided addresses.\n",
           count_elided_);
    VPRINT(1, "Adjusted " UINT64_FORMAT_STRING " rseq aborts.\n", count_rseq_abort_);
    VPRINT(1, "Adjusted " UINT64_FORMAT_STRING " rseq side exits.\n",
           count_rseq_side_exit_);
    VPRINT(1, "Trace duration %.3fs.\n",
           (latest_trace_timestamp_ - earliest_trace_timestamp_) / 1000000.0);
    VPRINT(1, "Successfully converted %zu thread files\n", thread_data_.size());
    return "";
}

std::string
raw2trace_t::aggregate_and_write_schedule_files()
{
    if (serial_schedule_file_ == nullptr && cpu_schedule_file_ == nullptr)
        return "";
    std::vector<schedule_entry_t> serial;
    std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched;
    for (auto &tdata : thread_data_) {
        serial.insert(serial.end(), tdata->sched.begin(), tdata->sched.end());
        for (auto &keyval : tdata->cpu2sched) {
            auto &vec = cpu2sched[keyval.first];
            vec.insert(vec.end(), keyval.second.begin(), keyval.second.end());
        }
    }
    std::sort(serial.begin(), serial.end(),
              [](const schedule_entry_t &l, const schedule_entry_t &r) {
                  return l.timestamp < r.timestamp;
              });
    if (serial_schedule_file_ != nullptr) {
        if (!serial_schedule_file_->write(reinterpret_cast<const char *>(serial.data()),
                                          serial.size() * sizeof(serial[0])))
            return "Failed to write to serial schedule file";
    }
    if (cpu_schedule_file_ == nullptr)
        return "";
    for (auto &keyval : cpu2sched) {
        std::sort(keyval.second.begin(), keyval.second.end(),
                  [](const schedule_entry_t &l, const schedule_entry_t &r) {
                      return l.timestamp < r.timestamp;
                  });
        std::ostringstream stream;
        stream << keyval.first;
        std::string err = cpu_schedule_file_->open_new_component(stream.str());
        if (!err.empty())
            return err;
        if (!cpu_schedule_file_->write(
                reinterpret_cast<const char *>(keyval.second.data()),
                keyval.second.size() * sizeof(keyval.second[0])))
            return "Failed to write to cpu schedule file";
    }
    return "";
}

/***************************************************************************
 * Block and memref handling
 */

std::string
raw2trace_t::analyze_elidable_addresses(raw2trace_thread_data_t *tdata, uint64 modidx,
                                        uint64 modoffs, app_pc start_pc, uint instr_count)
{
    int version = get_version(tdata);
    // Old versions have no elision.
    if (version <= OFFLINE_FILE_VERSION_NO_ELISION)
        return "";
    // Filtered and instruction-only traces have no elision.
    if (TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS |
                    OFFLINE_FILE_TYPE_INSTRUCTION_ONLY,
                get_file_type(tdata)))
        return "";
    // We build an ilist to use identify_elidable_addresses() and fill in
    // state needed to reconstruct elided addresses.
    instrlist_t *ilist = instrlist_create(dcontext_);
    app_pc pc = start_pc;
    for (uint count = 0; count < instr_count; ++count) {
        instr_t *inst = instr_create(dcontext_);
        app_pc next_pc = decode(dcontext_, pc, inst);
        DR_ASSERT(next_pc != NULL);
        instr_set_translation(inst, pc);
        instr_set_note(inst, reinterpret_cast<void *>(static_cast<ptr_int_t>(count)));
        pc = next_pc;
        instrlist_append(ilist, inst);
    }

    instru_offline_.identify_elidable_addresses(dcontext_, ilist, version, false);

    for (instr_t *inst = instrlist_first(ilist); inst != nullptr;
         inst = instr_get_next(inst)) {
        int index, memop_index;
        bool write, needs_base;
        if (!instru_offline_.label_marks_elidable(inst, &index, &memop_index, &write,
                                                  &needs_base))
            continue;
        // There could be multiple labels for one instr (e.g., "push (%rsp)".
        instr_t *meminst = instr_get_next(inst);
        while (meminst != nullptr && instr_is_label(meminst))
            meminst = instr_get_next(meminst);
        DR_ASSERT(meminst != nullptr);
        pc = instr_get_app_pc(meminst);
        int index_in_bb =
            static_cast<int>(reinterpret_cast<ptr_int_t>(instr_get_note(meminst)));
        app_pc orig_pc = modmap_().get_orig_pc_from_map_pc(pc, modidx, modoffs);
        log(5, "Marking < " PFX ", " PFX "> %s #%d to use remembered base\n", start_pc,
            pc, write ? "write" : "read", memop_index);
        if (!set_instr_summary_flags(tdata, modidx, modoffs, start_pc, instr_count,
                                     index_in_bb, pc, orig_pc, write, memop_index,
                                     true /*use_remembered*/,
                                     false /*don't change "remember"*/))
            return "Failed to set flags for elided base address";
        // We still need to set the use_remember flag for rip-rel, even though it
        // does not need a prior base, because we do not elide *all* rip-rels
        // (e.g., predicated rip-rels).
        if (!needs_base)
            continue;
        // Find the source of the base.  It has to be the first instance when
        // walking backward.
        opnd_t elided_op =
            write ? instr_get_dst(meminst, index) : instr_get_src(meminst, index);
        reg_id_t base;
        bool got_base = instru_offline_.opnd_is_elidable(elided_op, base, version);
        DR_ASSERT(got_base && base != DR_REG_NULL);
        int remember_index = -1;
        for (instr_t *prev = meminst; prev != nullptr; prev = instr_get_prev(prev)) {
            if (!instr_is_app(prev))
                continue;
            // Use instr_{reads,writes}_memory() to rule out LEA and NOP.
            if (!instr_reads_memory(prev) && !instr_writes_memory(prev))
                continue;
            bool remember_write = false;
            int mem_count = 0;
            if (prev != meminst || write) {
                for (int i = 0; i < instr_num_srcs(prev); i++) {
                    reg_id_t prev_base;
                    if (opnd_is_memory_reference(instr_get_src(prev, i))) {
                        if (instru_offline_.opnd_is_elidable(instr_get_src(prev, i),
                                                             prev_base, version) &&
                            prev_base == base) {
                            remember_index = mem_count;
                            break;
                        }
                        ++mem_count;
                    }
                }
            }
            if (remember_index == -1 && prev != meminst) {
                mem_count = 0;
                for (int i = 0; i < instr_num_dsts(prev); i++) {
                    reg_id_t prev_base;
                    if (opnd_is_memory_reference(instr_get_dst(prev, i))) {
                        if (instru_offline_.opnd_is_elidable(instr_get_dst(prev, i),
                                                             prev_base, version) &&
                            prev_base == base) {
                            remember_index = mem_count;
                            remember_write = true;
                            break;
                        }
                        ++mem_count;
                    }
                }
            }
            if (remember_index == -1)
                continue;
            app_pc pc_prev = instr_get_app_pc(prev);
            app_pc orig_pc_prev =
                modmap_().get_orig_pc_from_map_pc(pc_prev, modidx, modoffs);
            int index_prev =
                static_cast<int>(reinterpret_cast<ptr_int_t>(instr_get_note(prev)));
            if (!set_instr_summary_flags(
                    tdata, modidx, modoffs, start_pc, instr_count, index_prev, pc_prev,
                    orig_pc_prev, remember_write, remember_index,
                    false /*don't change "use_remembered"*/, true /*remember*/))
                return "Failed to set flags for elided base address";
            log(5, "Asking <" PFX ", " PFX "> %s #%d to remember base\n", start_pc,
                pc_prev, remember_write ? "write" : "read", remember_index);
            break;
        }
        if (remember_index == -1)
            return "Failed to find the source of the elided base";
    }
    instrlist_clear_and_destroy(dcontext_, ilist);
    return "";
}

std::string
raw2trace_t::process_memref(raw2trace_thread_data_t *tdata, trace_entry_t **buf_in,
                            const instr_summary_t *instr,
                            instr_summary_t::memref_summary_t memref, bool write,
                            std::unordered_map<reg_id_t, addr_t> &reg_vals,
                            uint64_t cur_pc, uint64_t cur_offs, bool instrs_are_separate,
                            OUT bool *reached_end_of_memrefs, OUT bool *interrupted)
{
    std::string error = append_memref(tdata, buf_in, instr, memref, write, reg_vals,
                                      reached_end_of_memrefs);
    if (!error.empty())
        return error;
    error = handle_kernel_interrupt_and_markers(tdata, buf_in, cur_pc, cur_offs,
                                                instr->length(), instrs_are_separate,
                                                interrupted);
    return error;
}

std::string
raw2trace_t::append_bb_entries(raw2trace_thread_data_t *tdata,
                               const offline_entry_t *in_entry, OUT bool *handled)
{
    std::string error = "";
    uint instr_count = in_entry->pc.instr_count;
    const instr_summary_t *instr = nullptr;
    app_pc start_pc = modmap_().get_map_pc(in_entry->pc.modidx, in_entry->pc.modoffs);
    app_pc pc, decode_pc = start_pc;
    if (in_entry->pc.modidx == PC_MODIDX_INVALID) {
        log(3, "Appending %u instrs in bb " PFX " in generated code\n", instr_count,
            reinterpret_cast<ptr_uint_t>(
                modmap_().get_orig_pc(in_entry->pc.modidx, in_entry->pc.modoffs)));
    } else if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
               modvec_()[in_entry->pc.modidx].map_seg_base == NULL) {
        if (get_version(tdata) >= OFFLINE_FILE_VERSION_ENCODINGS) {
            // This is a fatal error if this trace should have encodings.
            return "Non-module instructions found with no encoding information.";
        }
        //  This is a legacy trace without generated code support.
        //  A race is fine for our visible ~one-time warning at level 0.
        static volatile bool warned_once;
        if (!warned_once) {
            log(0, "WARNING: Skipping ifetch for unknown-encoding instructions\n");
            warned_once = true;
        }
        log(1, "Skipping ifetch for %u unknown-encoding instrs (idx %d, +" PIFX ")\n",
            instr_count, in_entry->pc.modidx, in_entry->pc.modoffs);
        // XXX i#2062: If we abandon this graceful skip (maybe once all legacy
        // traces have expired), we can remove the bool return value and handle the
        // memrefs in this function.
        *handled = false;
        return "";
    } else {
        log(3, "Appending %u instrs in bb " PFX " in mod %u +" PIFX " = %s\n",
            instr_count, (ptr_uint_t)start_pc, (uint)in_entry->pc.modidx,
            (ptr_uint_t)in_entry->pc.modoffs, modvec_()[in_entry->pc.modidx].path);
    }
    bool skip_icache = false;
    // This indicates that each memref has its own PC entry and that each
    // icache entry does not need to be considered a memref PC entry as well.
    bool instrs_are_separate = TESTANY(
        OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED, get_file_type(tdata));
    bool is_instr_only_trace =
        TESTANY(OFFLINE_FILE_TYPE_INSTRUCTION_ONLY, get_file_type(tdata));
    // Cast to unsigned pointer-sized int first to avoid sign-extending.
    uint64_t cur_pc = static_cast<uint64_t>(reinterpret_cast<ptr_uint_t>(
        modmap_().get_orig_pc(in_entry->pc.modidx, in_entry->pc.modoffs)));
    // Legacy traces need the offset, not the pc.
    uint64_t cur_offs = in_entry->pc.modoffs;
    std::unordered_map<reg_id_t, addr_t> reg_vals;
    if (instr_count == 0) {
        // L0 filtering adds a PC entry with a count of 0 prior to each memref.
        skip_icache = true;
        instr_count = 1;
        // We should have set a flag to avoid peeking forward on instr entries
        // based on OFFLINE_FILE_TYPE_FILTERED and
        // OFFLINE_FILE_TYPE_IFILTERED.
        DR_ASSERT(instrs_are_separate);
    } else {
        if (!instr_summary_exists(tdata, in_entry->pc.modidx, in_entry->pc.modoffs,
                                  start_pc, 0, decode_pc)) {
            std::string res = analyze_elidable_addresses(
                tdata, in_entry->pc.modidx, in_entry->pc.modoffs, start_pc, instr_count);
            if (!res.empty())
                return res;
        }
    }
    DR_CHECK(!instrs_are_separate || instr_count == 1, "cannot mix 0-count and >1-count");
    for (uint i = 0; i < instr_count; ++i) {
        trace_entry_t *buf_start = get_write_buffer(tdata);
        trace_entry_t *buf = buf_start;
        app_pc saved_decode_pc = decode_pc;
        app_pc orig_pc = modmap_().get_orig_pc_from_map_pc(decode_pc, in_entry->pc.modidx,
                                                           in_entry->pc.modoffs);
        // To avoid repeatedly decoding the same instruction on every one of its
        // dynamic executions, we cache the decoding in a hashtable.
        pc = decode_pc;
        log_instruction(4, decode_pc, orig_pc);
        instr = get_instr_summary(tdata, in_entry->pc.modidx, in_entry->pc.modoffs,
                                  start_pc, instr_count, i, &pc, orig_pc);
        if (instr == nullptr) {
            // We hit some error somewhere, and already reported it. Just exit the
            // loop.
            break;
        }
        DR_CHECK(pc > decode_pc, "error advancing inside block");
        DR_CHECK(!instr->is_cti() || i == instr_count - 1, "invalid cti");
        if (instr->is_syscall() && should_omit_syscall(tdata)) {
            accumulate_to_statistic(tdata, RAW2TRACE_STAT_FALSE_SYSCALL, 1);
            log(3, "Omitting syscall instr without subsequent number marker.\n");
            // Exit and do not append this syscall instruction.  It must be the
            // final instruction in the block; since the tracer requests callbacks
            // on all syscalls, none are inlined.
            DR_CHECK(i == instr_count - 1, "syscall not last in block");
            break;
        }
        // TODO i#5934: This is a workaround for the trace invariant error triggered
        // by consecutive duplicate system call instructions. Remove this when the
        // error is fixed in the drmemtrace tracer.
        // TODO i#6102: This actually does the wrong thing for SIG_IGN interrupting
        // an auto-restart syscall; we live with that until we remove it after fixing
        // the incorrect duplicate syscall error.
        if (instr->is_syscall() && get_last_pc_if_syscall(tdata) == orig_pc &&
            instr_count == 1) {
            // Also remove the syscall marker.  It could be after a timestamp+cpuid
            // pair; we're fine removing those too and having the prior timestamp
            // go with the next block.
            if (TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, tdata->file_type)) {
                const offline_entry_t *entry = get_next_entry(tdata);
                if (entry->timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
                    entry = get_next_entry(tdata);
                    if (entry->extended.type == OFFLINE_TYPE_EXTENDED &&
                        entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
                        entry->extended.valueB == TRACE_MARKER_TYPE_CPU_ID) {
                        entry = get_next_entry(tdata);
                    }
                }
                DR_CHECK(entry->extended.type == OFFLINE_TYPE_EXTENDED &&
                             entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
                             entry->extended.valueB == TRACE_MARKER_TYPE_SYSCALL,
                         "Syscall without marker should have been removed");
                // We've consumed these records and we just drop them.
            }
            accumulate_to_statistic(tdata, RAW2TRACE_STAT_DUPLICATE_SYSCALL, 1);
            log(3, "Found block with duplicate system call instruction. Skipping.\n");
            // Since this instr is in its own block, we're done.
            // Note that this will result in a pair of timestamp+cpu markers without
            // anything before the next timestamp+cpu pair for legacy traces w/o
            // syscall number markers.
            break;
        }
        if (!instr->is_cti()) {
            // Write out delayed branches now that we have a target.
            error = append_delayed_branch(tdata, orig_pc);
            if (!error.empty())
                return error;
        }
        if (tdata->rseq_buffering_enabled_) {
            addr_t instr_pc = reinterpret_cast<addr_t>(orig_pc);
            if (tdata->rseq_past_end_) {
                error = adjust_and_emit_rseq_buffer(tdata, instr_pc);
                if (!error.empty())
                    return error;
            } else if (instr_pc < tdata->rseq_start_pc_ ||
                       instr_pc >= tdata->rseq_end_pc_) {
                log(4, "Hit exit to 0x%zx during instrumented rseq run\n", orig_pc);
                error = adjust_and_emit_rseq_buffer(tdata, instr_pc);
                if (!error.empty())
                    return error;
            } else {
                if (instr->is_cti()) {
                    log(4, "Remembering rseq branch %p -> %p\n", orig_pc,
                        instr->branch_target_pc());
                    tdata->rseq_branch_targets_.emplace_back(
                        orig_pc, instr->branch_target_pc(),
                        // The branch may be delayed so this index may point to
                        // markers that will precede the branch.  The using code
                        // will walk it forward to the branch.
                        static_cast<int>(tdata->rseq_buffer_.size()));
                }
                if (tdata->rseq_start_pc_ == 0) {
                    tdata->rseq_start_pc_ = instr_pc;
                }
                if (instr_pc + instr->length() == tdata->rseq_end_pc_) {
                    log(4, "Hit rseq endpoint 0x%zx @ %p\n", tdata->rseq_end_pc_,
                        orig_pc);
                    tdata->rseq_commit_pc_ = instr_pc;
                    tdata->rseq_past_end_ = true;
                    tdata->rseq_commit_idx_ =
                        static_cast<int>(tdata->rseq_buffer_.size());
                }
            }
        }
        if (!skip_icache && record_encoding_emitted(tdata, decode_pc)) {
            error = append_encoding(tdata, decode_pc, instr->length(), buf, buf_start);
            if (!error.empty())
                return error;
        }

        // XXX i#1729: make bundles via lazy accum until hit memref/end, if
        // we don't need encodings.
        buf->type = instr->type();
        if (buf->type == TRACE_TYPE_INSTR_MAYBE_FETCH) {
            // We want it to look like the original rep string, with just one instr
            // fetch for the whole loop, instead of the drutil-expanded loop.
            // We fix up the maybe-fetch here so our offline file doesn't have to
            // rely on our own reader.
            if (!was_prev_instr_rep_string(tdata)) {
                set_prev_instr_rep_string(tdata, true);
                buf->type = TRACE_TYPE_INSTR;
            } else {
                log(3, "Skipping instr fetch for " PFX "\n", (ptr_uint_t)decode_pc);
                // We still include the instr to make it easier for core simulators
                // (i#2051).
                buf->type = TRACE_TYPE_INSTR_NO_FETCH;
            }
        } else
            set_prev_instr_rep_string(tdata, false);
        if (instr->is_syscall())
            set_last_pc_if_syscall(tdata, orig_pc);
        else
            set_last_pc_if_syscall(tdata, 0);
        buf->size = (ushort)(skip_icache ? 0 : instr->length());
        buf->addr = (addr_t)orig_pc;
        ++buf;
        log(4, "Appended instr fetch for original %p\n", orig_pc);
        decode_pc = pc;
        if (tdata->rseq_past_end_) {
            // In case handle_kernel_interrupt_and_markers() calls
            // adjust_and_emit_rseq_buffer() we need to have written this instr to the
            // rseq buffer.
            error = write(tdata, buf_start, buf, &saved_decode_pc, 1);
            if (!error.empty())
                return error;
            buf = buf_start;
        }
        // Check for a signal *after* the instruction.  The trace is recording
        // instruction *fetches*, not instruction retirement, and we want to
        // include a faulting instruction before its raised signal.
        bool interrupted = false;
        error = handle_kernel_interrupt_and_markers(tdata, &buf, cur_pc, cur_offs,
                                                    instr->length(), instrs_are_separate,
                                                    &interrupted);
        if (!error.empty())
            return error;
        if (interrupted) {
            log(3, "Stopping bb at kernel interruption point %p\n", cur_pc);
        }
        // We need to interleave instrs with memrefs.
        // There is no following memref for (instrs_are_separate && !skip_icache).
        if (!interrupted && (!instrs_are_separate || skip_icache) &&
            // Rule out OP_lea.
            (instr->reads_memory() || instr->writes_memory()) &&
            // No following memref for instruction-only trace type.
            !is_instr_only_trace) {
            if (instr->is_scatter_or_gather()) {
                // The instr should either load or store, but not both. Also,
                // it should have a single src or dest operand.
                DR_ASSERT(instr->num_mem_srcs() + instr->num_mem_dests() == 1);
                bool is_scatter = instr->num_mem_dests() == 1;
                bool reached_end_of_memrefs = false;
                // For expanded scatter/gather instrs, we do not have prior knowledge
                // of the number of store/load memrefs that will be present. So we
                // continue reading entries until we find a non-memref entry.
                // This works only because drx_expand_scatter_gather ensures that the
                // expansion has its own basic block, with no other app instr in it.
                while (!reached_end_of_memrefs) {
                    // XXX: Add sanity check for max count of store/load memrefs
                    // possible for a given scatter/gather instr.
                    error = process_memref(
                        tdata, &buf, instr,
                        // These memrefs were output by multiple store/load instrs in
                        // the expanded scatter/gather sequence. In raw2trace we see
                        // only the original app instr though. So we use the 0th
                        // dest/src of the original scatter/gather instr for all.
                        is_scatter ? instr->mem_dest_at(0) : instr->mem_src_at(0),
                        is_scatter, reg_vals, cur_pc, cur_offs, instrs_are_separate,
                        &reached_end_of_memrefs, &interrupted);
                    if (!error.empty())
                        return error;
                    if (interrupted)
                        break;
                }
            } else {
                for (uint j = 0; j < instr->num_mem_srcs(); j++) {
                    error = process_memref(tdata, &buf, instr, instr->mem_src_at(j),
                                           false, reg_vals, cur_pc, cur_offs,
                                           instrs_are_separate, nullptr, &interrupted);
                    if (!error.empty())
                        return error;
                    if (interrupted)
                        break;
                }
                // We break before subsequent memrefs on an interrupt, though with
                // today's tracer that will never happen (i#3958).
                for (uint j = 0; !interrupted && j < instr->num_mem_dests(); j++) {
                    error = process_memref(tdata, &buf, instr, instr->mem_dest_at(j),
                                           true, reg_vals, cur_pc, cur_offs,
                                           instrs_are_separate, nullptr, &interrupted);
                    if (!error.empty())
                        return error;
                    if (interrupted)
                        break;
                }
            }
        }
        cur_pc += instr->length();
        cur_offs += instr->length();
        DR_CHECK((size_t)(buf - buf_start) < WRITE_BUFFER_SIZE, "Too many entries");
        if (instr->is_cti()) {
            // In case this is the last branch prior to a thread switch, buffer it. We
            // avoid swapping threads immediately after a branch so that analyzers can
            // more easily find the branch target.  Doing this in the tracer would
            // incur extra overhead, and in the reader would be more complex and messy
            // than here (and we are ok bailing on doing this for online traces), so
            // we handle it in post-processing by delaying a thread-block-final branch
            // (and its memrefs) to that thread's next block.  This changes the
            // timestamp of the branch, which we live with. To avoid marker
            // misplacement (e.g. in the middle of a basic block), we also
            // delay markers.
            log(4, "Delaying %d entries for decode=" PIFX "\n", buf - buf_start,
                saved_decode_pc);
            error = write_delayed_branches(tdata, buf_start, buf, saved_decode_pc,
                                           instr->branch_target_pc());
            if (!error.empty())
                return error;
        } else if (buf > buf_start) {
            error = write(tdata, buf_start, buf, &saved_decode_pc, 1);
            if (!error.empty())
                return error;
        }
        if (interrupted)
            break;
    }
    *handled = true;
    return "";
}

// Returns true if a kernel interrupt happened at cur_pc.
// Outputs a kernel interrupt if this is the right location.
// Outputs any other markers observed if !instrs_are_separate, since they
// are part of this block and need to be inserted now. Inserts all
// intra-block markers (i.e., the higher level process_offline_entry() will
// never insert a marker intra-block) and all inter-block markers are
// handled at a higher level (process_offline_entry()) and are never
// inserted here.
std::string
raw2trace_t::handle_kernel_interrupt_and_markers(
    raw2trace_thread_data_t *tdata, INOUT trace_entry_t **buf_in, uint64_t cur_pc,
    uint64_t cur_offs, int instr_length, bool instrs_are_separate, OUT bool *interrupted)
{
    // To avoid having to backtrack later, we read ahead to ensure we insert
    // an interrupt at the right place between memrefs or between instructions.
    *interrupted = false;
    bool append = false;
    trace_entry_t *buf_start = get_write_buffer(tdata);
    do {
        const offline_entry_t *in_entry = get_next_entry(tdata);
        if (in_entry == nullptr)
            return "";
        append = false;
        if (in_entry->extended.type != OFFLINE_TYPE_EXTENDED ||
            in_entry->extended.ext != OFFLINE_EXT_TYPE_MARKER) {
            // Not a marker: just put it back.
            unread_last_entry(tdata);
            continue;
        }
        // The kernel markers can take two entries, so we have to read both
        // if present to get to the type.  There is support for unreading
        // both.
        uintptr_t marker_val = 0;
        std::string err = get_marker_value(tdata, &in_entry, &marker_val);
        if (!err.empty())
            return err;
        // An abort always ends a block.
        if (in_entry->extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT ||
            in_entry->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ABORT) {
            // A signal/exception marker in the next entry could be at any point
            // among non-memref instrs, or it could be after this bb.
            // We check the stored PC.
            int version = get_version(tdata);
            bool at_interrupted_pc = false;
            bool legacy_rseq_rollback = false;
            if (version < OFFLINE_FILE_VERSION_KERNEL_INT_PC) {
                // We have only the offs, so we can't handle differing modules for
                // the source and target for legacy traces.
                if (marker_val == cur_offs)
                    at_interrupted_pc = true;
            } else {
                if (marker_val == cur_pc)
                    at_interrupted_pc = true;
            }
            // Support legacy traces before we added TRACE_MARKER_TYPE_RSEQ_ENTRY.  We
            // identify them by not seeing an entry before an abort.  We won't fix up
            // rseq aborts with timestamps (i#5954) nor rseq side exits (i#5953) for
            // such traces but we can at least fix up typical aborts.
            if (!tdata->rseq_ever_saw_entry_ &&
                in_entry->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ABORT &&
                // I-filtered don't have every instr so we can't roll back.
                !TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                         get_file_type(tdata))) {
                // For the older version, we will not get here for Windows
                // callbacks, the other event with a 0 modoffs, because they are
                // always between bbs.  (Unfortunately there's no simple way to
                // assert or check that here or in the tracer.)
                legacy_rseq_rollback = true;
            }
            log(4,
                "Checking whether reached signal/exception %p vs "
                "cur 0x" HEX64_FORMAT_STRING "\n",
                marker_val, cur_pc);
            if (marker_val == 0 || at_interrupted_pc || legacy_rseq_rollback) {
                log(4, "Signal/exception interrupted the bb @ %p\n", cur_pc);
                if (tdata->rseq_past_end_) {
                    addr_t rseq_abort_pc =
                        in_entry->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ABORT
                        ? marker_val
                        : 0;
                    err = adjust_and_emit_rseq_buffer(tdata, static_cast<addr_t>(cur_pc),
                                                      rseq_abort_pc);
                    if (!err.empty())
                        return err;
                }
                append = true;
                *interrupted = true;
                if (legacy_rseq_rollback) {
                    // This happens on rseq native aborts, where the trace instru
                    // includes the rseq committing store before the native rseq
                    // execution hits the native abort.  Pretend the native abort
                    // happened *before* the committing store by walking the store
                    // backward.  Everything in the buffer is for the store;
                    // there should be no (other) intra-bb markers not for the store.
                    log(4, "Rolling back %d entries for rseq abort\n",
                        *buf_in - buf_start);
                    accumulate_to_statistic(tdata, RAW2TRACE_STAT_RSEQ_ABORT, 1);
                    // If we recorded and emitted an encoding we would not emit
                    // it next time and be missing the encoding so we must clear
                    // the cache for that entry.  This will only happen once
                    // for any new encoding (one synchronous signal/rseq abort
                    // per instr) so we will satisfy the one-time limit of
                    // rollback_last_encoding() (it has an assert to verify).
                    for (trace_entry_t *entry = buf_start; entry < *buf_in; ++entry) {
                        if (entry->type == TRACE_TYPE_ENCODING) {
                            rollback_last_encoding(tdata);
                            break;
                        }
                    }
                    *buf_in = buf_start;
                }
            } else {
                // Put it back (below). We do not have a problem with other markers
                // following this, because we will have to hit the correct point
                // for this interrupt marker before we hit a memref entry, avoiding
                // the danger of wanting a memref entry, seeing a marker, continuing,
                // and hitting a fatal error when we find the memref back in the
                // not-inside-a-block main loop.
            }
        } else {
            // Other than kernel event markers checked above, markers should be
            // only at block boundaries, as we cannot figure out where they should go
            // (and could easily insert them in the middle of this block instead
            // of between this and the next block, with implicit instructions added).
            // Thus, we do not append any other markers.
        }
        if (append) {
            byte *buf = reinterpret_cast<byte *>(*buf_in);
            buf += trace_metadata_writer_t::write_marker(
                buf, (trace_marker_type_t)in_entry->extended.valueB, marker_val);
            *buf_in = reinterpret_cast<trace_entry_t *>(buf);
            log(3, "Appended marker type %u value " PIFX "\n",
                (trace_marker_type_t)in_entry->extended.valueB, marker_val);
            // There can be many markers in a row, esp for function tracing on
            // longjmp or some other xfer that skips many post-call points.
            // But, we can't easily write the buffer out, b/c we want to defer
            // it for branches, and roll it back for rseq.
            // XXX i#4159: We could switch to dynamic storage (and update all uses
            // that assume no re-allocation), but this should be pathological so for
            // now we have a release-build failure.
            DR_CHECK((size_t)(*buf_in - buf_start) < WRITE_BUFFER_SIZE,
                     "Too many entries");
        } else {
            // Put it back.
            unread_last_entry(tdata);
        }
    } while (append);
    return "";
}

bool
raw2trace_t::should_omit_syscall(raw2trace_thread_data_t *tdata)
{
    if (!TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, tdata->file_type))
        return false;
    // We have 2 scenarios where we record a syscall instr yet it doesn't
    // execute right away and so there's no syscall number marker:
    // 1) An asynchronous signal arrives during the block and is delivered
    //    from dispatch before handling the syscall.
    // 2) A syscall ends a tracing window and we disable tracing before the
    //    syscall is handled.
    // In both cases, we want to just remove the syscall instr: so we remove
    // if we find no subsequent marker either immediately following or after
    // a buffer header of timestamp+cpuid.
    // (For the window case there are alternatives where we try to emit
    // the marker by passing info to the pre-syscall event handler or by moving
    // the marker to the block instrumentation but these all incur more complexity
    // than this relatively simple solution.)
    const offline_entry_t *in_entry = get_next_entry(tdata);
    std::vector<offline_entry_t> saved;
    if (in_entry->timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
        saved.push_back(*in_entry);
        in_entry = get_next_entry(tdata);
        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
            in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
            in_entry->extended.valueB == TRACE_MARKER_TYPE_CPU_ID) {
            saved.push_back(*in_entry);
            in_entry = get_next_entry(tdata);
        }
    }
    bool omit = false;
    if (in_entry->extended.type != OFFLINE_TYPE_EXTENDED ||
        in_entry->extended.ext != OFFLINE_EXT_TYPE_MARKER ||
        in_entry->extended.valueB != TRACE_MARKER_TYPE_SYSCALL) {
        omit = true;
    }
    saved.push_back(*in_entry);
    for (auto &entry : saved) {
        queue_entry(tdata, entry);
    }
    return omit;
}

std::string
raw2trace_t::get_marker_value(raw2trace_thread_data_t *tdata,
                              INOUT const offline_entry_t **entry, OUT uintptr_t *value)
{
    uintptr_t marker_val = static_cast<uintptr_t>((*entry)->extended.valueA);
    if ((*entry)->extended.valueB == TRACE_MARKER_TYPE_SPLIT_VALUE) {
#ifdef X64
        // Keep the prior so we can unread both at once if we roll back.
        const offline_entry_t *next = get_next_entry_keep_prior(tdata);
        if (next == nullptr || next->extended.ext != OFFLINE_EXT_TYPE_MARKER)
            return "SPLIT_VALUE marker is not adjacent to 2nd entry";
        marker_val = (marker_val << 32) | static_cast<uintptr_t>(next->extended.valueA);
        *entry = next;
#else
        return "TRACE_MARKER_TYPE_SPLIT_VALUE unexpected for 32-bit";
#endif
    }
#ifdef X64 // 32-bit always had the absolute pc as the raw marker value.
    if ((*entry)->extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT ||
        (*entry)->extended.valueB == TRACE_MARKER_TYPE_RSEQ_ABORT ||
        (*entry)->extended.valueB == TRACE_MARKER_TYPE_KERNEL_XFER) {
        if (get_version(tdata) >= OFFLINE_FILE_VERSION_KERNEL_INT_PC &&
            get_version(tdata) < OFFLINE_FILE_VERSION_XFER_ABS_PC) {
            // We convert the idx:offs to an absolute PC.
            // This doesn't work for non-module-code and is thus only present in
            // legacy traces.
            kernel_interrupted_raw_pc_t raw_pc;
            raw_pc.combined_value = marker_val;
            DR_ASSERT(raw_pc.pc.modidx != PC_MODIDX_INVALID);
            app_pc pc = modvec_()[raw_pc.pc.modidx].orig_seg_base +
                (raw_pc.pc.modoffs - modvec_()[raw_pc.pc.modidx].seg_offs);
            log(3,
                "Kernel marker: converting 0x" ZHEX64_FORMAT_STRING
                " idx=" INT64_FORMAT_STRING " with base=%p to %p\n",
                marker_val, raw_pc.pc.modidx, modvec_()[raw_pc.pc.modidx].orig_seg_base,
                pc);
            marker_val = reinterpret_cast<uintptr_t>(pc);
        } // For really old, we've already marked as TRACE_ENTRY_VERSION_NO_KERNEL_PC.
    }
#endif
    *value = marker_val;
    return "";
}

std::string
raw2trace_t::append_memref(raw2trace_thread_data_t *tdata, INOUT trace_entry_t **buf_in,
                           const instr_summary_t *instr,
                           instr_summary_t::memref_summary_t memref, bool write,
                           std::unordered_map<reg_id_t, addr_t> &reg_vals,
                           OUT bool *reached_end_of_memrefs)
{
    DR_ASSERT(!TESTANY(OFFLINE_FILE_TYPE_INSTRUCTION_ONLY, get_file_type(tdata)));
    trace_entry_t *buf = *buf_in;
    const offline_entry_t *in_entry = nullptr;
    bool have_addr = false;
    bool have_type = false;
    reg_id_t base;
    int version = get_version(tdata);
    if (memref.use_remembered_base) {
        DR_ASSERT(!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED |
                               OFFLINE_FILE_TYPE_DFILTERED,
                           get_file_type(tdata)));
        bool is_elidable = instru_offline_.opnd_is_elidable(memref.opnd, base, version);
        DR_ASSERT(is_elidable);
        if (base == DR_REG_NULL) {
            DR_ASSERT(IF_REL_ADDRS(opnd_is_near_rel_addr(memref.opnd) ||)
                          opnd_is_near_abs_addr(memref.opnd));
            buf->addr = reinterpret_cast<addr_t>(opnd_get_addr(memref.opnd));
            log(4, "Filling in elided rip-rel addr with %p\n", buf->addr);
        } else {
            buf->addr = reg_vals[base];
            log(4, "Filling in elided addr with remembered %s: %p\n",
                get_register_name(base), buf->addr);
        }
        have_addr = true;
        accumulate_to_statistic(tdata, RAW2TRACE_STAT_COUNT_ELIDED, 1);
    }
    if (!have_addr) {
        if (memref.use_remembered_base)
            return "Non-elided base mislabeled to use remembered base";
        in_entry = get_next_entry(tdata);
    }
    if (in_entry != nullptr && in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
        in_entry->extended.ext == OFFLINE_EXT_TYPE_MEMINFO) {
        // For -L0_filter we have to store the type for multi-memref instrs where
        // we can't tell which memref it is (we'll still come here for the subsequent
        // memref operands but we'll exit early in the check below).
        have_type = true;
        buf->type = in_entry->extended.valueB;
        buf->size = in_entry->extended.valueA;
        log(4, "Found type entry type %s (%d) size %d\n", trace_type_names[buf->type],
            buf->type, buf->size);
        in_entry = get_next_entry(tdata);
        if (in_entry == nullptr)
            return "Trace ends mid-block";
    }
    if (!have_addr &&
        (in_entry == nullptr ||
         (in_entry->addr.type != OFFLINE_TYPE_MEMREF &&
          in_entry->addr.type != OFFLINE_TYPE_MEMREF_HIGH))) {
        // This happens when there are predicated memrefs in the bb, or for a
        // zero-iter rep string loop, or for a multi-memref instr with -L0_filter.
        // For predicated memrefs, they could be earlier, so "instr"
        // may not itself be predicated.
        // XXX i#2015: if there are multiple predicated memrefs, our instr vs
        // data stream may not be in the correct order here.
        log(4,
            "Missing memref from predication, 0-iter repstr, filter, "
            "or reached end of memrefs output by scatter/gather seq "
            "(next type is 0x" ZHEX64_FORMAT_STRING ")\n",
            in_entry == nullptr ? 0 : in_entry->combined_value);
        if (in_entry != nullptr) {
            unread_last_entry(tdata);
        }
        if (reached_end_of_memrefs != nullptr) {
            *reached_end_of_memrefs = true;
        }
        return "";
    }
    if (!have_type) {
        if (instr->is_prefetch()) {
            buf->type = instr->prefetch_type();
            buf->size = 1;
        } else if (instr->is_flush()) {
            buf->type = instr->flush_type();
            // TODO i#4398: Handle flush sizes larger than ushort.
            // TODO i#4406: Handle flush instrs with sizes other than cache line.
            buf->size = (ushort)get_cache_line_size(tdata);
        } else {
            if (write)
                buf->type = TRACE_TYPE_WRITE;
            else
                buf->type = TRACE_TYPE_READ;
            buf->size = (ushort)opnd_size_in_bytes(opnd_get_size(memref.opnd));
        }
    }
    if (!have_addr) {
        DR_ASSERT(in_entry != nullptr);
        // We take the full value, to handle low or high.
        buf->addr = (addr_t)in_entry->combined_value;
    }
    if (memref.remember_base &&
        instru_offline_.opnd_is_elidable(memref.opnd, base, version)) {
        log(5, "Remembering base " PFX " for %s\n", buf->addr, get_register_name(base));
        reg_vals[base] = buf->addr;
    }
    if (!TESTANY(OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS, get_file_type(tdata)) &&
        instru_offline_.opnd_disp_is_elidable(memref.opnd)) {
        // We stored only the base reg, as an optimization.
        buf->addr += opnd_get_disp(memref.opnd);
    }
    log(4, "Appended memref type %s (%d) size %d to " PFX "\n",
        trace_type_names[buf->type], buf->type, buf->size, (ptr_uint_t)buf->addr);

#ifdef AARCH64
    // TODO i#4400: Following is a workaround to correctly represent DC ZVA in
    // offline traces. Note that this doesn't help with online traces.
    // TODO i#3339: This workaround causes us to lose the address that was present
    // in the original instruction. For re-encoding fidelity, we may want the
    // original address in the IR.
    if (instr->is_aarch64_dc_zva()) {
        buf->addr = ALIGN_BACKWARD(buf->addr, get_cache_line_size(tdata));
        buf->size = get_cache_line_size(tdata);
        buf->type = TRACE_TYPE_WRITE;
    }
#endif
    *buf_in = ++buf;
    return "";
}

bool
raw2trace_t::record_encoding_emitted(raw2trace_thread_data_t *tdata, app_pc pc)
{
    if (tdata->encoding_emitted.find_and_insert(pc) == false)
        return false;
    tdata->last_encoding_emitted = pc;
    return true;
}

// This can only be called once between calls to record_encoding_emitted()
// and only after record_encoding_emitted() returns true.
void
raw2trace_t::rollback_last_encoding(raw2trace_thread_data_t *tdata)
{
    DEBUG_ASSERT(tdata->last_encoding_emitted != nullptr);
    tdata->encoding_emitted.erase(tdata->last_encoding_emitted);
    tdata->last_encoding_emitted = nullptr;
}

std::string
raw2trace_t::rollback_rseq_buffer(raw2trace_thread_data_t *tdata,
                                  int remove_start_rough_idx,
                                  // This is inclusive.
                                  int remove_end_rough_idx)
{
    // First, advance to encoding/instruction boundaries, but include any timestamp
    // in what's being removed to avoid violating branch invariants (i#5986).
    // XXX: We could try to look for a prior branch and keep the timestamp if there
    // is none but it's not worth that complexity.
    int remove_start = remove_start_rough_idx;
    while (remove_start < static_cast<int>(tdata->rseq_buffer_.size()) &&
           tdata->rseq_buffer_[remove_start].type != TRACE_TYPE_ENCODING &&
           !type_is_instr(
               static_cast<trace_type_t>(tdata->rseq_buffer_[remove_start].type)) &&
           (tdata->rseq_buffer_[remove_start].type != TRACE_TYPE_MARKER ||
            tdata->rseq_buffer_[remove_start].size != TRACE_MARKER_TYPE_TIMESTAMP))
        ++remove_start;
    int remove_end = remove_end_rough_idx;
    while (
        remove_end < static_cast<int>(tdata->rseq_buffer_.size()) &&
        (tdata->rseq_buffer_[remove_end].type == TRACE_TYPE_ENCODING ||
         type_is_instr(static_cast<trace_type_t>(tdata->rseq_buffer_[remove_end].type)) ||
         type_is_data(static_cast<trace_type_t>(tdata->rseq_buffer_[remove_end].type)) ||
         (tdata->rseq_buffer_[remove_end].type == TRACE_TYPE_MARKER &&
          (tdata->rseq_buffer_[remove_end].size == TRACE_MARKER_TYPE_TIMESTAMP ||
           tdata->rseq_buffer_[remove_end].size == TRACE_MARKER_TYPE_CPU_ID))))
        ++remove_end;
    log(4, "rseq rollback: advanced rough %d-%d to %d-%d\n", remove_start_rough_idx,
        remove_end_rough_idx, remove_start, remove_end);
    // Now find the corresponding decode_pc start index.
    // XXX: Should we merge the decode_pc field into the rseq_buffer_ to simplify
    // this, and then pull it out for write() (or have write() pull it out or
    // write every entry singly)?
    int decode_start = 0;
    for (int i = 0; i < remove_start; i++) {
        if (type_is_instr(static_cast<trace_type_t>(tdata->rseq_buffer_[i].type)))
            ++decode_start;
    }
    // Now walk to the end, erasing from the encoding cache to ensure it's emitted next
    // time.
    int decode_end = decode_start;
    for (int i = remove_start; i < remove_end; i++) {
        if (tdata->rseq_buffer_[i].type == TRACE_TYPE_ENCODING) {
            log(4, "Erasing cached encoding for %p\n",
                tdata->rseq_decode_pcs_[decode_end]);
            tdata->encoding_emitted.erase(tdata->rseq_decode_pcs_[decode_end]);
        }
        if (type_is_instr(static_cast<trace_type_t>(tdata->rseq_buffer_[i].type)))
            ++decode_end;
    }
    log(4, "Rolling back %d entries for rseq: %d-%d\n", remove_end - remove_start,
        remove_start, remove_end);
    tdata->rseq_buffer_.erase(tdata->rseq_buffer_.begin() + remove_start,
                              tdata->rseq_buffer_.begin() + remove_end);
    tdata->rseq_decode_pcs_.erase(tdata->rseq_decode_pcs_.begin() + decode_start,
                                  tdata->rseq_decode_pcs_.begin() + decode_end);
    return "";
}

std::string
raw2trace_t::adjust_and_emit_rseq_buffer(raw2trace_thread_data_t *tdata, addr_t next_pc,
                                         addr_t abort_pc)
{
    if (!tdata->rseq_want_rollback_)
        return "";
    log(4, "--- Rseq region exited at %p ---\n", next_pc);
    if (verbosity_ >= 4) {
        log(4, "Rseq buffer contents:\n");
        for (int i = 0; i < static_cast<int>(tdata->rseq_buffer_.size()); i++) {
            log(4, "  #%2d: type=%d, size=%d, addr=0x%zx\n", i,
                tdata->rseq_buffer_[i].type, tdata->rseq_buffer_[i].size,
                tdata->rseq_buffer_[i].addr);
        }
    }
    // We need this in the outer scope for use by write() below.
    byte encoding[MAX_ENCODING_LENGTH];

    if ((abort_pc != 0 && next_pc == abort_pc) ||
        // Old traces have the commit PC instead of the handler in the abort and
        // signal events (so not really the "next_pc" but the "exit_pc") and
        // don't have abort events on a signal aborting an rseq region.
        (!tdata->rseq_ever_saw_entry_ && next_pc == tdata->rseq_commit_pc_)) {
        // An abort.  It could have aborted earlier but we have no way of knowing
        // so we do the simplest thing and only roll back the committing store.
        log(4, "Rseq aborted\n");
        accumulate_to_statistic(tdata, RAW2TRACE_STAT_RSEQ_ABORT, 1);
        if (tdata->rseq_commit_idx_ < 0) {
            if (tdata->rseq_buffer_.empty()) {
                // This is a graceful failure: we consider this a bug to
                // have an extra abort marker but we do not want to make it
                // fatal if the buffer is empty as we can continue.
                // XXX: Add an invariant check for this.
                log(1, "Extra abort marker found");
                return "";
            }
            // Else this is an abort in the instrumented run, such as a
            // fault or signal, so no rollback is needed.
        } else {
            std::string error = rollback_rseq_buffer(tdata, tdata->rseq_commit_idx_,
                                                     tdata->rseq_commit_idx_);
            if (!error.empty())
                return error;
        }
    } else if (next_pc == tdata->rseq_end_pc_) {
        // Normal fall-through of the committing store: nothing to roll back.  We give
        // up on distinguishing a side exit whose target is the end PC fall-through from
        // a completion.
        log(4, "Rseq completed normally\n");
    } else if (!tdata->rseq_past_end_ &&
               (next_pc < tdata->rseq_start_pc_ || next_pc >= tdata->rseq_end_pc_)) {
        // The instrumented execution took a side exit.
        DEBUG_ASSERT(tdata->rseq_commit_pc_ == 0);
        DEBUG_ASSERT(abort_pc == 0);
        log(4, "Rseq instrumented side exit\n");
    } else {
        log(4, "Rseq exited on the side: searching for where\n");
        accumulate_to_statistic(tdata, RAW2TRACE_STAT_RSEQ_SIDE_EXIT, 1);
        bool found_direct = false;
        bool found_skip = false;
        branch_info_t info;
        for (const auto &branch : tdata->rseq_branch_targets_) {
            if (reinterpret_cast<addr_t>(branch.target_pc) == next_pc) {
                info = branch;
                found_direct = true;
                // We do not break as we want the last such branch for a shorter
                // rollback.
            }
        }
        if (!found_direct) {
            // An indirect branch exit that we traced is impossible so the
            // only option left is a conditional that jumped over the exit.
            for (const auto &branch : tdata->rseq_branch_targets_) {
                if (branch.target_pc > branch.pc &&
                    reinterpret_cast<addr_t>(branch.target_pc) < tdata->rseq_end_pc_) {
                    // A forward branch that remains inside the region.
                    // We've skipped over something: likely a direct jump exiting
                    // the region.
                    info = branch;
                    found_skip = true;
                    break;
                }
            }
        }
        if (!found_direct && !found_skip) {
            log(4, "Failed to find rseq side exit\n");
            return "Failed to find rseq side exit";
        }
        log(4, "Found rseq%s side exit: %p -> %p idx=%d tid=%d\n",
            found_skip ? " skipped" : "", info.pc, info.target_pc, info.buf_idx,
            tdata->tid);
        // Walk forward to the branch itself: there may be markers (since delayed) or
        // encoding records in between.
        int post_branch = info.buf_idx;
        while (post_branch < static_cast<int>(tdata->rseq_buffer_.size()) &&
               !type_is_instr(
                   static_cast<trace_type_t>(tdata->rseq_buffer_[post_branch].type)))
            ++post_branch;
        DEBUG_ASSERT(type_is_instr_branch(
            static_cast<trace_type_t>(tdata->rseq_buffer_[post_branch].type)));
        if (found_direct) {
            // It wasn't taken in the instrumented run, but we have to make it
            // appear taken to match the actual exit.
            DEBUG_ASSERT(tdata->rseq_buffer_[post_branch].type ==
                         TRACE_TYPE_INSTR_UNTAKEN_JUMP);
            tdata->rseq_buffer_[post_branch].type = TRACE_TYPE_INSTR_TAKEN_JUMP;
        } else {
            // It was taken in the instrumented run, but we have to make it
            // appear untaken to match the actual exit.
            DEBUG_ASSERT(tdata->rseq_buffer_[post_branch].type ==
                         TRACE_TYPE_INSTR_TAKEN_JUMP);
            tdata->rseq_buffer_[post_branch].type = TRACE_TYPE_INSTR_UNTAKEN_JUMP;
        }
        int branch_size = tdata->rseq_buffer_[post_branch].size;
        ++post_branch; // Now skip instr entry itself.
        std::string error =
            rollback_rseq_buffer(tdata, post_branch, tdata->rseq_commit_idx_);
        if (!error.empty())
            return error;
        if (found_skip) {
            // Append a synthetic jump.  This may not match the actual exit instruction:
            // there could have been several non-branches before the exit branch; the
            // exit could be indirect; etc.  But this is the best we can readily do.
            instr_t *instr = XINST_CREATE_jump(
                dcontext_, opnd_create_pc(reinterpret_cast<app_pc>(next_pc)));
            byte *enc_next =
                instr_encode_to_copy(dcontext_, instr, encoding, info.pc + branch_size);
            instr_destroy(dcontext_, instr);
            if (enc_next == nullptr)
                return "Failed to encode synthetic rseq exit jump";
            trace_entry_t jump;
            jump.type = TRACE_TYPE_INSTR_DIRECT_JUMP;
            jump.addr = reinterpret_cast<addr_t>(info.pc) + branch_size;
            jump.size = static_cast<unsigned short>(enc_next - encoding);
            trace_entry_t toadd[WRITE_BUFFER_SIZE];
            bool exists =
                !record_encoding_emitted(tdata, reinterpret_cast<app_pc>(jump.addr));
            trace_entry_t *buf = toadd;
            error = append_encoding(tdata, encoding, jump.size, buf, toadd);
            if (!error.empty())
                return error;
            if (exists) {
                // Uh-oh, we've already seen this PC!  We don't cache the actual
                // encoding though so we can't try to use the real instructions;
                // we don't bail because this seems like it could easily happen;
                // instead we pretend the code changed, and we throw the jump
                // encoding out so it will change back if we see this PC again.
                rollback_last_encoding(tdata);
            }
            for (trace_entry_t *e = toadd; e < buf; e++)
                tdata->rseq_buffer_.push_back(*e);
            tdata->rseq_buffer_.push_back(jump);
            tdata->rseq_decode_pcs_.push_back(encoding);
            log(4, "Appended synthetic jump 0x%zx -> 0x%zx\n", jump.addr, next_pc);
        }
    }

    tdata->rseq_buffering_enabled_ = false;

    log(4, "Writing out rseq buffer: %zd entries\n", tdata->rseq_buffer_.size());
    std::string error =
        write(tdata, &tdata->rseq_buffer_[0], &tdata->rseq_buffer_.back() + 1,
              tdata->rseq_decode_pcs_.data(), tdata->rseq_decode_pcs_.size());
    if (!error.empty())
        return error;

    tdata->rseq_past_end_ = false;
    tdata->rseq_commit_pc_ = 0;
    tdata->rseq_start_pc_ = 0;
    tdata->rseq_end_pc_ = 0;
    tdata->rseq_buffer_.clear();
    tdata->rseq_commit_idx_ = -1;
    tdata->rseq_branch_targets_.clear();
    tdata->rseq_decode_pcs_.clear();

    return "";
}

raw2trace_t::block_summary_t *
raw2trace_t::lookup_block_summary(raw2trace_thread_data_t *tdata, uint64 modidx,
                                  uint64 modoffs, app_pc block_start)
{
    // There is no sentinel available for modidx+modoffs so we use block_start for that.
    if (block_start == tdata->last_decode_block_start &&
        modidx == tdata->last_decode_modidx && modoffs == tdata->last_decode_modoffs) {
        VPRINT(5, "Using last block summary " PFX " for " PFX "\n",
               tdata->last_block_summary, tdata->last_decode_block_start);
        return tdata->last_block_summary;
    }
    block_summary_t *ret = decode_cache_[tdata->worker].lookup(modidx, modoffs);
    if (ret != nullptr) {
        DEBUG_ASSERT(ret->start_pc == block_start);
        tdata->last_decode_block_start = block_start;
        tdata->last_decode_modidx = modidx;
        tdata->last_decode_modoffs = modoffs;
        tdata->last_block_summary = ret;
        VPRINT(5, "Caching last block summary " PFX " for " PFX "\n",
               tdata->last_block_summary, tdata->last_decode_block_start);
    }
    return ret;
}

instr_summary_t *
raw2trace_t::lookup_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx,
                                  uint64 modoffs, app_pc block_start, int index,
                                  app_pc pc, OUT block_summary_t **block_summary)
{
    block_summary_t *block = lookup_block_summary(tdata, modidx, modoffs, block_start);
    if (block_summary != nullptr)
        *block_summary = block;
    if (block == nullptr)
        return nullptr;
    DEBUG_ASSERT(index >= 0 && index < static_cast<int>(block->instrs.size()));
    if (block->instrs[index].pc() == nullptr)
        return nullptr;
    DEBUG_ASSERT(pc == block->instrs[index].pc());
    return &block->instrs[index];
}

bool
raw2trace_t::instr_summary_exists(raw2trace_thread_data_t *tdata, uint64 modidx,
                                  uint64 modoffs, app_pc block_start, int index,
                                  app_pc pc)
{
    return lookup_instr_summary(tdata, modidx, modoffs, block_start, index, pc,
                                nullptr) != nullptr;
}

instr_summary_t *
raw2trace_t::create_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx,
                                  uint64 modoffs, block_summary_t *block,
                                  app_pc block_start, int instr_count, int index,
                                  INOUT app_pc *pc, app_pc orig)
{
    if (block == nullptr) {
        block = new block_summary_t(block_start, instr_count);
        DEBUG_ASSERT(index >= 0 && index < static_cast<int>(block->instrs.size()));
        decode_cache_[tdata->worker].add(modidx, modoffs, block);
        VPRINT(5,
               "Created new block summary " PFX " for " PFX " modidx=" INT64_FORMAT_STRING
               " modoffs=" HEX64_FORMAT_STRING "\n",
               block, block_start, modidx, modoffs);
        tdata->last_decode_block_start = block_start;
        tdata->last_decode_modidx = modidx;
        tdata->last_decode_modoffs = modoffs;
        tdata->last_block_summary = block;
    }
    instr_summary_t *desc = &block->instrs[index];
    if (!instr_summary_t::construct(dcontext_, block_start, pc, orig, desc, verbosity_)) {
        WARN("Encountered invalid/undecodable instr @ idx=" INT64_FORMAT_STRING
             " offs=" INT64_FORMAT_STRING " %s",
             modidx, modoffs,
             modidx == PC_MODIDX_INVALID ? "<gencode>"
                                         : modvec_()[static_cast<size_t>(modidx)].path);
        return nullptr;
    }
    return desc;
}

const instr_summary_t *
raw2trace_t::get_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx,
                               uint64 modoffs, app_pc block_start, int instr_count,
                               int index, INOUT app_pc *pc, app_pc orig)
{
    block_summary_t *block;
    const instr_summary_t *ret =
        lookup_instr_summary(tdata, modidx, modoffs, block_start, index, *pc, &block);
    if (ret == nullptr) {
        return create_instr_summary(tdata, modidx, modoffs, block, block_start,
                                    instr_count, index, pc, orig);
    }
    *pc = ret->next_pc();
    return ret;
}

// These flags are difficult to set on construction: because one instr_t may have
// multiple flags, we'd need get_instr_summary() to take in a vector or sthg.
// Instead we set after the fact.
bool
raw2trace_t::set_instr_summary_flags(raw2trace_thread_data_t *tdata, uint64 modidx,
                                     uint64 modoffs, app_pc block_start, int instr_count,
                                     int index, app_pc pc, app_pc orig, bool write,
                                     int memop_index, bool use_remembered_base,
                                     bool remember_base)
{
    block_summary_t *block;
    instr_summary_t *desc =
        lookup_instr_summary(tdata, modidx, modoffs, block_start, index, pc, &block);
    if (desc == nullptr) {
        app_pc pc_copy = pc;
        desc = create_instr_summary(tdata, modidx, modoffs, block, block_start,
                                    instr_count, index, &pc_copy, orig);
    }
    if (desc == nullptr)
        return false;
    if (write)
        desc->set_mem_dest_flags(memop_index, use_remembered_base, remember_base);
    else
        desc->set_mem_src_flags(memop_index, use_remembered_base, remember_base);
    return true;
}

bool
instr_summary_t::construct(void *dcontext, app_pc block_start, INOUT app_pc *pc,
                           app_pc orig_pc, OUT instr_summary_t *desc, uint verbosity)
{
    struct instr_destroy_t {
        instr_destroy_t(void *dcontext, instr_t *instr)
            : dcontext(dcontext)
            , instr(instr)
        {
        }
        void *dcontext;
        instr_t *instr;
        ~instr_destroy_t()
        {
            instr_destroy(dcontext, instr);
        }
    };

    instr_t *instr = instr_create(dcontext);
    instr_destroy_t instr_collector(dcontext, instr);

    desc->pc_ = *pc;
    *pc = decode_from_copy(dcontext, *pc, orig_pc, instr);
    if (*pc == nullptr || !instr_valid(instr)) {
        return false;
    }
    if (verbosity > 4) {
        // This is called for look-ahead and look-behind too so we leave the
        // main instr disasm to log_instruction and have high verbosity here.
        instr_set_translation(instr, orig_pc);
        dr_fprintf(STDERR, "<caching for start=" PFX "> ", block_start);
        dr_print_instr(dcontext, STDERR, instr, "");
    }
    DEBUG_ASSERT(*pc > desc->pc_);
    desc->length_ = static_cast<byte>(*pc - desc->pc_);
    DEBUG_ASSERT(*pc - desc->pc_ == instr_length(dcontext, instr));

    desc->packed_ = 0;

    bool is_prefetch = instr_is_prefetch(instr);
    bool is_flush = instru_t::instr_is_flush(instr);
    bool reads_memory = instr_reads_memory(instr);
    bool writes_memory = instr_writes_memory(instr);

    if (reads_memory)
        desc->packed_ |= kReadsMemMask;
    if (writes_memory)
        desc->packed_ |= kWritesMemMask;
    if (is_prefetch)
        desc->packed_ |= kIsPrefetchMask;
    if (instru_t::instr_is_flush(instr))
        desc->packed_ |= kIsFlushMask;
    if (instr_is_cti(instr)) {
        desc->packed_ |= kIsCtiMask;
        if (instr_is_ubr(instr) || instr_is_cbr(instr) || instr_is_call_direct(instr))
            desc->branch_target_pc_ = opnd_get_pc(instr_get_target(instr));
    }
    // XXX i#5949: This has some OS-specific behavior that should be preserved
    // even when decoding a trace collected on a different platform.
    if (instr_is_syscall(instr))
        desc->packed_ |= kIsSyscallMask;

#ifdef AARCH64
    bool is_dc_zva = instru_t::is_aarch64_dc_zva_instr(instr);
    if (is_dc_zva)
        desc->packed_ |= kIsAarch64DcZvaMask;
#endif

#ifdef X86
    if (instr_is_scatter(instr) || instr_is_gather(instr))
        desc->packed_ |= kIsScatterOrGatherMask;
#endif
    desc->type_ = instru_t::instr_to_instr_type(instr);
    desc->prefetch_type_ = is_prefetch ? instru_t::instr_to_prefetch_type(instr) : 0;
    desc->flush_type_ = is_flush ? instru_t::instr_to_flush_type(instr) : 0;

    if (reads_memory || writes_memory) {
        for (int i = 0, e = instr_num_srcs(instr); i < e; ++i) {
            opnd_t op = instr_get_src(instr, i);
            if (opnd_is_memory_reference(op))
                desc->mem_srcs_and_dests_.push_back(memref_summary_t(op));
        }
        desc->num_mem_srcs_ = static_cast<uint8_t>(desc->mem_srcs_and_dests_.size());

        for (int i = 0, e = instr_num_dsts(instr); i < e; ++i) {
            opnd_t op = instr_get_dst(instr, i);
            if (opnd_is_memory_reference(op))
                desc->mem_srcs_and_dests_.push_back(memref_summary_t(op));
        }
    }
    return true;
}

const offline_entry_t *
raw2trace_t::get_next_entry(raw2trace_thread_data_t *tdata)
{
    // We do our own buffering to avoid performance problems for some istreams where
    // seekg is slow.  We expect just 1 entry peeked and put back the vast majority of the
    // time, but we use a vector for generality.  We expect our overall performance to
    // be i/o bound (or ISA decode bound) and aren't worried about some extra copies
    // from the vector.
    tdata->last_entry_is_split = false;
    if (!tdata->pre_read.empty()) {
        tdata->last_entry = tdata->pre_read.front();
        VPRINT(5, "Reading from queue\n");
        tdata->pre_read.pop_front();
    } else {
        if (!tdata->thread_file->read((char *)&tdata->last_entry,
                                      sizeof(tdata->last_entry)))
            return nullptr;
    }
    VPRINT(5, "[get_next_entry]: type=%d val=" HEX64_FORMAT_STRING "\n",
           // Some compilers think .addr.type is "int" while others think it's "unsigned
           // long".  We avoid dueling warnings by casting to int.
           static_cast<int>(tdata->last_entry.addr.type),
           // Cast to long to avoid Mac warning on "long long" using "long" format.
           static_cast<uint64>(tdata->last_entry.combined_value));
    return &tdata->last_entry;
}

const offline_entry_t *
raw2trace_t::get_next_entry_keep_prior(raw2trace_thread_data_t *tdata)
{
    if (tdata->last_entry_is_split) {
        // Cannot record two live split entries.
        return nullptr;
    }
    VPRINT(4, "Remembering split entry for unreading both at once\n");
    tdata->last_split_first_entry = tdata->last_entry;
    const offline_entry_t *next = get_next_entry(tdata);
    // Set this *after* calling get_next_entry as it clears the field.
    tdata->last_entry_is_split = true;
    return next;
}

void
raw2trace_t::unread_last_entry(raw2trace_thread_data_t *tdata)
{
    VPRINT(5, "Unreading last entry\n");
    if (tdata->last_entry_is_split) {
        VPRINT(4, "Unreading both parts of split entry at once\n");
        tdata->pre_read.push_front(tdata->last_split_first_entry);
        tdata->last_entry_is_split = false;
    }
    tdata->pre_read.push_front(tdata->last_entry);
}

void
raw2trace_t::queue_entry(raw2trace_thread_data_t *tdata, offline_entry_t &entry)
{
    VPRINT(5, "Queueing a given entry type=%d\n", static_cast<int>(entry.addr.type));
    tdata->pre_read.push_back(entry);
}

bool
raw2trace_t::thread_file_at_eof(raw2trace_thread_data_t *tdata)
{
    return tdata->pre_read.empty() && tdata->thread_file->eof();
}

std::string
raw2trace_t::append_delayed_branch(raw2trace_thread_data_t *tdata, app_pc next_pc)
{
    // While we no longer document a guarantee that branches are delayed to make them
    // adjacent to their targets now that we have TRACE_ENTRY_VERSION_BRANCH_INFO, we
    // still use the delayed branch mechanism as it was already in place and is the
    // easiest way to implement TRACE_ENTRY_VERSION_BRANCH_INFO.  If we decide to
    // use a different implementation we should perhaps wait for all users to
    // update their clients.
    if (tdata->delayed_branch_empty_)
        return "";
    // We can't infer branch targets for filtered instructions.
    if (!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                 get_file_type(tdata))) {
        // We need to transform TRACE_TYPE_INSTR_CONDITIONAL_JUMP into
        // TRACE_TYPE_INSTR_{TAKEN,UNTAKEN}_JUMP based on which side of the branch
        // was taken.  We also need to add indirect branch target markers.
        int instr_count = 0;
        for (size_t i = 0; i < tdata->delayed_branch.size(); ++i) {
            const auto &entry = tdata->delayed_branch[i];
            if (type_is_instr(static_cast<trace_type_t>(entry.type)))
                ++instr_count;
        }
        int instr_index = instr_count - 1;
        // Walk backward so we have the next pc for stacked branches.
        app_pc next_instr_pc = next_pc;
        for (int i = static_cast<int>(tdata->delayed_branch.size()) - 1; i >= 0; --i) {
            auto &entry = tdata->delayed_branch[i];
            if (type_is_instr(static_cast<trace_type_t>(entry.type))) {
                DEBUG_ASSERT(type_is_instr_branch(static_cast<trace_type_t>(entry.type)));
                if (tdata->delayed_branch_target_pcs.size() <=
                    static_cast<size_t>(instr_index)) {
                    return "Delayed branch target vector mis-sized";
                }
                app_pc target = tdata->delayed_branch_target_pcs[instr_index];
                // Cache entry fields before we insert any markers at entry's position.
                app_pc branch_addr = reinterpret_cast<app_pc>(entry.addr);
                trace_type_t branch_type = static_cast<trace_type_t>(entry.type);
                if (next_instr_pc == nullptr &&
                    (target == nullptr ||
                     entry.type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP)) {
                    // This is a trace-final or window-final branch but we do not have
                    // its taken/target without a subsequent instr: just delete it.
                    DEBUG_ASSERT(instr_index == instr_count - 1);
                    int erase_from = i;
                    while (erase_from > 0 &&
                           tdata->delayed_branch[erase_from - 1].type ==
                               TRACE_TYPE_ENCODING) {
                        log(4, "Erasing cached encoding for %p\n",
                            tdata->delayed_branch_decode_pcs[instr_index]);
                        tdata->encoding_emitted.erase(
                            tdata->delayed_branch_decode_pcs[instr_index]);
                        --erase_from;
                    }
                    VPRINT(4,
                           "Discarded %zd entries for final branch without subsequent "
                           "instr\n",
                           tdata->delayed_branch.size() - erase_from);
                    tdata->delayed_branch.erase(tdata->delayed_branch.begin() +
                                                    erase_from,
                                                tdata->delayed_branch.end());
                    tdata->delayed_branch_decode_pcs.pop_back();
                    tdata->delayed_branch_target_pcs.pop_back();
                    break;
                } else {
                    if (target == nullptr) {
                        DEBUG_ASSERT(!type_is_instr_direct_branch(
                            static_cast<trace_type_t>(entry.type)));
                        trace_entry_t local[3];
                        int size = trace_metadata_writer_t::write_marker(
                            reinterpret_cast<byte *>(local),
                            TRACE_MARKER_TYPE_BRANCH_TARGET,
                            reinterpret_cast<uintptr_t>(next_instr_pc));
                        DEBUG_ASSERT(static_cast<size_t>(size) <= sizeof(local));
                        for (int local_idx = 0;
                             local_idx < size / static_cast<int>(sizeof(local[0]));
                             ++local_idx) {
                            tdata->delayed_branch.insert(
                                tdata->delayed_branch.begin() + i, local[local_idx]);
                        }
                        VPRINT(4, "Inserted indirect branch target %p\n", next_instr_pc);
                    } else if (entry.type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP) {
                        if (target == next_instr_pc) {
                            branch_type = TRACE_TYPE_INSTR_TAKEN_JUMP;
                        } else {
                            branch_type = TRACE_TYPE_INSTR_UNTAKEN_JUMP;
                        }
                        entry.type = static_cast<unsigned short>(branch_type);
                    }
                    VPRINT(
                        4,
                        "Appending delayed branch type=%d pc=%p decode=%p target=%p for "
                        "thread %d\n",
                        branch_type, branch_addr,
                        tdata->delayed_branch_decode_pcs[instr_index], target,
                        tdata->index);
                }
                next_instr_pc = branch_addr;
                --instr_index;
            } else {
                VPRINT(4,
                       "Appending delayed branch tagalong entry type %s (%d) for thread "
                       "%d\n",
                       trace_type_names[entry.type], entry.type, tdata->index);
            }
        }
    }
    if (!tdata->delayed_branch.empty()) {
        std::string error =
            write(tdata, tdata->delayed_branch.data(),
                  tdata->delayed_branch.data() + tdata->delayed_branch.size(),
                  tdata->delayed_branch_decode_pcs.data(),
                  tdata->delayed_branch_decode_pcs.size());
        if (!error.empty())
            return error;
    }
    tdata->delayed_branch.clear();
    tdata->delayed_branch_decode_pcs.clear();
    tdata->delayed_branch_target_pcs.clear();
    tdata->delayed_branch_empty_ = true;
    return "";
}

trace_entry_t *
raw2trace_t::get_write_buffer(raw2trace_thread_data_t *tdata)
{
    return tdata->out_buf.data();
}

std::string
raw2trace_t::emit_new_chunk_header(raw2trace_thread_data_t *tdata)
{
    // Re-emit the last timestamp + cpu from the prior chunk.  We don't
    // need to re-emit the top-level headers to make the chunk self-contained: we'll
    // need to read them from the first chunk to find the chunk size and we can cache
    // them there.  We only re-emit the timestamp and cpu.  On a linear read, we skip
    // over these duplicated headers.
    // We can't use get_write_buffer() because we may be in the middle of
    // processing its contents.
    std::array<trace_entry_t, WRITE_BUFFER_SIZE> local_out_buf;
    byte *buf_base = reinterpret_cast<byte *>(local_out_buf.data());
    byte *buf = buf_base;
    buf += trace_metadata_writer_t::write_marker(
        buf, TRACE_MARKER_TYPE_RECORD_ORDINAL,
        static_cast<uintptr_t>(tdata->cur_chunk_ref_count));
    log(2, "Chunk #" INT64_FORMAT_STRING " ord marker " INT64_FORMAT_STRING "\n",
        tdata->chunk_count_, tdata->cur_chunk_ref_count);
    buf +=
        trace_metadata_writer_t::write_timestamp(buf, (uintptr_t)tdata->last_timestamp_);
    buf += trace_metadata_writer_t::write_marker(buf, TRACE_MARKER_TYPE_CPU_ID,
                                                 tdata->last_cpu_);
    CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
    // We write directly to avoid recursion issues; these duplicated headers do not
    // need to go into the schedule file.
    if (!tdata->out_file->write((char *)buf_base, buf - buf_base))
        return "Failed to write to output file";
    // These didn't go through tdata->memref_counter but all 3 should be invisible
    // so we don't want to increment cur_chunk_ref_count.
    return "";
}

std::string
raw2trace_t::open_new_chunk(raw2trace_thread_data_t *tdata)
{
    if (tdata->out_archive == nullptr)
        return "Archive file was not specified";

    log(1, "Creating new chunk #" INT64_FORMAT_STRING " for thread %d\n",
        tdata->chunk_count_, tdata->tid);

    if (tdata->chunk_count_ != 0) {
        // We emit a chunk footer so we can identify truncation.
        std::array<trace_entry_t, WRITE_BUFFER_SIZE> local_out_buf;
        byte *buf_base = reinterpret_cast<byte *>(local_out_buf.data());
        byte *buf = buf_base;
        buf += trace_metadata_writer_t::write_marker(
            buf, TRACE_MARKER_TYPE_CHUNK_FOOTER,
            static_cast<uintptr_t>(tdata->chunk_count_ - 1));
        CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
        if (!tdata->out_file->write((char *)buf_base, buf - buf_base))
            return "Failed to write to output file";
        // This didn't go through tdata->memref_counter so we manually add.
        tdata->cur_chunk_ref_count += (buf - buf_base) / sizeof(trace_entry_t);
    }

    std::ostringstream stream;
    stream << TRACE_CHUNK_PREFIX << std::setfill('0') << std::setw(4)
           << tdata->chunk_count_;
    std::string error = tdata->out_archive->open_new_component(stream.str());
    if (!error.empty())
        return error;
    tdata->cur_chunk_instr_count = 0;
    ++tdata->chunk_count_;
    if (tdata->chunk_count_ == 1)
        return "";

    error = emit_new_chunk_header(tdata);
    if (!error.empty())
        return error;

    // We need to clear the encoding cache so that each chunk is self-contained
    // and repeats all encodings used inside it.
    tdata->encoding_emitted.clear();
    tdata->last_encoding_emitted = nullptr;

    // TODO i#5538: Add a virtual-to-physical cache and clear it here.
    // We'll need to add a routine for trace_converter_t to call to query our cache --
    // or we can put the cache in trace_converter_t and have it clear the cache via
    // a new new-chunk return value from write() and append_delayed_branch().
    // Insertion of v2p markers in all but the first chunk will need to prepend
    // them to the instr after observing its memref: we may want to reserve the
    // first out_buf slot to avoid a memmove.

    return "";
}

std::string
raw2trace_t::append_encoding(raw2trace_thread_data_t *tdata, app_pc pc,
                             size_t instr_length, trace_entry_t *&buf,
                             trace_entry_t *buf_start)
{
    size_t size_left = instr_length;
    size_t offs = 0;
#ifdef ARM
    // Remove any Thumb LSB.
    pc = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, pc);
#endif
    do {
        buf->type = TRACE_TYPE_ENCODING;
        buf->size =
            static_cast<unsigned short>(std::min(size_left, sizeof(buf->encoding)));
        memcpy(buf->encoding, pc + offs, buf->size);
        if (buf->size < sizeof(buf->encoding)) {
            // We don't have to set the rest to 0 but it is nice.
            memset(buf->encoding + buf->size, 0, sizeof(buf->encoding) - buf->size);
        }
        log(4, "Appended encoding entry for %p sz=%zu 0x%08x...\n", pc, buf->size,
            *(int *)buf->encoding);
        offs += buf->size;
        size_left -= buf->size;
        ++buf;
        CHECK(static_cast<size_t>(buf - buf_start) < WRITE_BUFFER_SIZE,
              "Too many entries for write buffer");
    } while (size_left > 0);
    return "";
}

std::string
raw2trace_t::insert_post_chunk_encodings(raw2trace_thread_data_t *tdata,
                                         const trace_entry_t *instr, app_pc decode_pc)
{
    trace_entry_t encodings[WRITE_BUFFER_SIZE];
    trace_entry_t *buf = encodings;
    log(4, "Adding post-chunk-boundary encoding entry for decode=%p app=%p\n", decode_pc,
        instr->addr);
    std::string err = append_encoding(tdata, decode_pc, instr->size, buf, encodings);
    if (!err.empty())
        return err;
    if (!tdata->out_file->write(reinterpret_cast<const char *>(encodings),
                                reinterpret_cast<const char *>(buf) -
                                    reinterpret_cast<const char *>(encodings)))
        return "Failed to write to output file";
    return "";
}

// All writes to out_file go through this function, except new chunk headers
// and footers (to do so would cause recursion; we assume those do not need
// extra processing here).
std::string
raw2trace_t::write(raw2trace_thread_data_t *tdata, const trace_entry_t *start,
                   const trace_entry_t *end, app_pc *decode_pcs, size_t decode_pcs_size)
{
    if (end == start)
        return "Empty buffer passed to write()";
    if (tdata->rseq_buffering_enabled_) {
        for (const trace_entry_t *it = start; it < end; ++it)
            tdata->rseq_buffer_.push_back(*it);
        // Look for a runaway buffer which indicates a bug.
        // There are rseq regions with loops but they should be relatively short.
        static constexpr int MAX_REASONABLE_RSEQ_LENGTH = 4096;
        if (tdata->rseq_buffer_.size() > MAX_REASONABLE_RSEQ_LENGTH) {
            return "Runaway rseq buffer indicates an rseq exit was missed";
        }
        tdata->rseq_decode_pcs_.insert(tdata->rseq_decode_pcs_.end(), decode_pcs,
                                       decode_pcs + decode_pcs_size);
        return "";
    }
    if (tdata->out_archive != nullptr) {
        bool prev_was_encoding = false;
        int instr_ordinal = -1;
        for (const trace_entry_t *it = start; it < end; ++it) {
            tdata->cur_chunk_ref_count += tdata->memref_counter.entry_memref_count(it);
            // We wait until we're past the final instr to write, to ensure we
            // get all its memrefs, by not stopping until we hit an instr or an
            // encoding.  (We will put function markers for entry in the
            // prior chunk too: we live with that.)
            if ((type_is_instr(static_cast<trace_type_t>(it->type)) ||
                 it->type == TRACE_TYPE_ENCODING) &&
                tdata->cur_chunk_instr_count >= chunk_instr_count_) {
                DEBUG_ASSERT(tdata->cur_chunk_instr_count == chunk_instr_count_);
                if (!tdata->out_file->write(reinterpret_cast<const char *>(start),
                                            reinterpret_cast<const char *>(it) -
                                                reinterpret_cast<const char *>(start)))
                    return "Failed to write to output file";
                std::string error = open_new_chunk(tdata);
                if (!error.empty())
                    return error;
                start = it;
                DEBUG_ASSERT(tdata->cur_chunk_instr_count == 0);
            }
            if (type_is_instr(static_cast<trace_type_t>(it->type)) &&
                // Do not count PC-only i-filtered instrs.
                it->size > 0) {
                ++tdata->cur_chunk_instr_count;
                ++instr_ordinal;
                if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, tdata->file_type) &&
                    // We don't want encodings for the PC-only i-filtered entries.
                    it->size > 0 && instr_ordinal >= static_cast<int>(decode_pcs_size))
                    return "decode_pcs is missing entries for written instructions";
            }
            // Check for missing encodings after possibly opening a new chunk.
            // There can be multiple delayed branches in the same buffer here
            // so multiple could appear on the other side of a new chunk.
            //
            // XXX i#5724: Could we add a trace_entry_t-level invariant checker to
            // identify missing post-chunk encodings?  Or should we have the reader
            // deliberately clear its encoding history on a chunk boundary, raising
            // a fatal error on a missing encoding?  For now the only complex case
            // is these already-generated records which we handle here and have a
            // unit test covering so those further checks are lower priority.
            if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, tdata->file_type) &&
                type_is_instr(static_cast<trace_type_t>(it->type)) &&
                // We don't want encodings for the PC-only i-filtered entries.
                it->size > 0 && !prev_was_encoding &&
                record_encoding_emitted(tdata, *(decode_pcs + instr_ordinal))) {
                // Write any data we were waiting until post-loop to write.
                if (it > start &&
                    !tdata->out_file->write(reinterpret_cast<const char *>(start),
                                            reinterpret_cast<const char *>(it) -
                                                reinterpret_cast<const char *>(start)))
                    return "Failed to write to output file";
                std::string err =
                    insert_post_chunk_encodings(tdata, it, *(decode_pcs + instr_ordinal));
                if (!err.empty())
                    return err;
                if (!tdata->out_file->write(reinterpret_cast<const char *>(it),
                                            sizeof(*it)))
                    return "Failed to write to output file";
                start = it + 1;
            }
            if (it->type == TRACE_TYPE_ENCODING)
                prev_was_encoding = true;
            else
                prev_was_encoding = false;
            if (it->type == TRACE_TYPE_MARKER) {
                if (it->size == TRACE_MARKER_TYPE_TIMESTAMP)
                    tdata->last_timestamp_ = it->addr;
                else if (it->size == TRACE_MARKER_TYPE_CPU_ID) {
                    DR_CHECK(tdata->chunk_count_ > 0,
                             "chunk_count_ should have been incremented already");
                    uint64_t instr_count =
                        (tdata->chunk_count_ - 1) * chunk_instr_count_ +
                        tdata->cur_chunk_instr_count;
                    tdata->last_cpu_ = static_cast<uint>(it->addr);
                    tdata->sched.emplace_back(tdata->tid, tdata->last_timestamp_,
                                              tdata->last_cpu_, instr_count);
                    tdata->cpu2sched[it->addr].emplace_back(
                        tdata->tid, tdata->last_timestamp_, tdata->last_cpu_,
                        instr_count);
                }
            }
        }
    }
    if (end > start &&
        !tdata->out_file->write(reinterpret_cast<const char *>(start),
                                reinterpret_cast<const char *>(end) -
                                    reinterpret_cast<const char *>(start)))
        return "Failed to write to output file";
    // If we're at the end of a block (minus its delayed branch) we need
    // to split now to avoid going too far by waiting for the next instr.
    if (tdata->cur_chunk_instr_count >= chunk_instr_count_) {
        DEBUG_ASSERT(tdata->cur_chunk_instr_count == chunk_instr_count_);
        std::string error = open_new_chunk(tdata);
        if (!error.empty())
            return error;
    }
    log(4, "Chunk instr count is now " UINT64_FORMAT_STRING "\n",
        tdata->cur_chunk_instr_count);
    return "";
}

std::string
raw2trace_t::write_delayed_branches(raw2trace_thread_data_t *tdata,
                                    const trace_entry_t *start, const trace_entry_t *end,
                                    app_pc decode_pc, app_pc target_pc)
{
    int instr_count = 0;
    for (const trace_entry_t *it = start; it < end; ++it) {
        tdata->delayed_branch.push_back(*it);
        tdata->delayed_branch_empty_ = false;
        if (type_is_instr(static_cast<trace_type_t>(it->type)))
            ++instr_count;
    }
    if (instr_count > 1)
        return "Only one instruction per delayed branch bundle is supported";
    if (instr_count == 1) {
        if (decode_pc == nullptr)
            return "A delayed instruction must have a valid decode PC";
        log(4, "Remembered delayed branch decode=%p target=%p\n", decode_pc, target_pc);
        tdata->delayed_branch_decode_pcs.push_back(decode_pc);
        tdata->delayed_branch_target_pcs.push_back(target_pc);
    } else if (decode_pc != nullptr)
        return "Delayed non-instructions should not have a decode PC";
    return "";
}

bool
raw2trace_t::delayed_branches_exist(raw2trace_thread_data_t *tdata)
{
    return !tdata->delayed_branch_empty_;
}

std::string
raw2trace_t::write_footer(raw2trace_thread_data_t *tdata)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_FOOTER;
    entry.size = 0;
    entry.addr = 0;
    std::string error = write(tdata, &entry, &entry + 1);
    if (!error.empty())
        return error;
    return "";
}

std::string
raw2trace_t::on_thread_end(raw2trace_thread_data_t *tdata)
{
    if (get_next_entry(tdata) != nullptr || !thread_file_at_eof(tdata))
        return "Footer is not the final entry";
    return write_footer(tdata);
}

void
raw2trace_t::log(uint level, const char *fmt, ...)
{
    if (verbosity_ >= level) {
        va_list args;
        va_start(args, fmt);
        VPRINT_HEADER();
        vfprintf(stderr, fmt, args);
#ifdef WINDOWS
        // We fflush for Windows cygwin where stderr is not flushed.
        fflush(stderr);
#endif
        va_end(args);
    }
}

void
raw2trace_t::log_instruction(uint level, app_pc decode_pc, app_pc orig_pc)
{
    if (verbosity_ >= level) {
        disassemble_from_copy(dcontext_, decode_pc, orig_pc, STDOUT, true /*pc*/,
                              true /*bytes*/);
    }
}

void
raw2trace_t::set_last_pc_if_syscall(raw2trace_thread_data_t *tdata, app_pc value)
{
    tdata->last_pc_if_syscall_ = value;
}

app_pc
raw2trace_t::get_last_pc_if_syscall(raw2trace_thread_data_t *tdata)
{
    return tdata->last_pc_if_syscall_;
}

void
raw2trace_t::set_prev_instr_rep_string(raw2trace_thread_data_t *tdata, bool value)
{
    tdata->prev_instr_was_rep_string = value;
}

bool
raw2trace_t::was_prev_instr_rep_string(raw2trace_thread_data_t *tdata)
{
    return tdata->prev_instr_was_rep_string;
}

int
raw2trace_t::get_version(raw2trace_thread_data_t *tdata)
{
    return tdata->version;
}

size_t
raw2trace_t::get_cache_line_size(raw2trace_thread_data_t *tdata)
{
    return tdata->cache_line_size;
}

offline_file_type_t
raw2trace_t::get_file_type(raw2trace_thread_data_t *tdata)
{
    return tdata->file_type;
}

void
raw2trace_t::set_file_type(raw2trace_thread_data_t *tdata, offline_file_type_t file_type)
{
    // The file type needs to be updated during the switch to correctly process the
    // entries that follow after TRACE_MARKER_TYPE_FILTER_ENDPOINT. This does not affect
    // the written-out type.
    tdata->file_type = file_type;
}

raw2trace_t::raw2trace_t(
    const char *module_map, const std::vector<std::istream *> &thread_files,
    const std::vector<std::ostream *> &out_files,
    const std::vector<archive_ostream_t *> &out_archives, file_t encoding_file,
    std::ostream *serial_schedule_file, archive_ostream_t *cpu_schedule_file,
    void *dcontext, unsigned int verbosity, int worker_count,
    const std::string &alt_module_dir, uint64_t chunk_instr_count,
    const std::unordered_map<thread_id_t, std::istream *> &kthread_files_map,
    const std::string &kcore_path, const std::string &kallsyms_path)
    : dcontext_(dcontext == nullptr ? dr_standalone_init() : dcontext)
    , passed_dcontext_(dcontext != nullptr)
    , worker_count_(worker_count)
    , user_process_(nullptr)
    , user_process_data_(nullptr)
    , modmap_bytes_(module_map)
    , encoding_file_(encoding_file)
    , serial_schedule_file_(serial_schedule_file)
    , cpu_schedule_file_(cpu_schedule_file)
    , verbosity_(verbosity)
    , alt_module_dir_(alt_module_dir)
    , chunk_instr_count_(chunk_instr_count)
    , kthread_files_map_(kthread_files_map)
    , kcore_path_(kcore_path)
    , kallsyms_path_(kallsyms_path)
{
    // Exactly one of out_files and out_archives should be non-empty.
    // If thread_files is not empty it must match the input size.
    if ((!out_files.empty() && !out_archives.empty()) ||
        (out_files.empty() && out_archives.empty()) ||
        (!thread_files.empty() &&
         out_files.size() + out_archives.size() != thread_files.size())) {
        VPRINT(0, "Invalid input and output file lists\n");
        return;
    }
#ifdef ARM
    // We keep the mode at ARM and rely on LSB=1 offsets in the modoffs fields
    // and encoding_file to trigger Thumb decoding.
    dr_set_isa_mode(dcontext == NULL ? GLOBAL_DCONTEXT : dcontext, DR_ISA_ARM_A32, NULL);
#endif
    thread_data_.resize(thread_files.size());
    for (size_t i = 0; i < thread_data_.size(); ++i) {
        thread_data_[i] =
            std::unique_ptr<raw2trace_thread_data_t>(new raw2trace_thread_data_t);
        thread_data_[i]->index = static_cast<int>(i);
        thread_data_[i]->thread_file = thread_files[i];
        if (out_files.empty()) {
            thread_data_[i]->out_archive = out_archives[i];
            // Set out_file too for code that doesn't care which it writes to.
            thread_data_[i]->out_file = out_archives[i];
            open_new_chunk(thread_data_[i].get());
        } else {
            thread_data_[i]->out_archive = nullptr;
            thread_data_[i]->out_file = out_files[i];
        }
    }
    // Since we know the traced-thread count up front, we use a simple round-robin
    // static work assignment.  This won't be as load balanced as a dynamic work
    // queue but it is much simpler.
    if (worker_count_ < 0) {
        worker_count_ = std::thread::hardware_concurrency();
        if (worker_count_ > kDefaultJobMax)
            worker_count_ = kDefaultJobMax;
    }
    int cache_count = worker_count_;
    if (worker_count_ > 0) {
        worker_tasks_.resize(worker_count_);
        int worker = 0;
        for (size_t i = 0; i < thread_data_.size(); ++i) {
            VPRINT(2, "Worker %d assigned trace thread %zd\n", worker, i);
            worker_tasks_[worker].push_back(thread_data_[i].get());
            thread_data_[i]->worker = worker;
            worker = (worker + 1) % worker_count_;
        }
    } else
        cache_count = 1;
    decode_cache_.reserve(cache_count);
    for (int i = 0; i < cache_count; ++i)
        decode_cache_.emplace_back(cache_count);
}

raw2trace_t::~raw2trace_t()
{
    module_mapper_.reset();
    if (!passed_dcontext_)
        dr_standalone_exit();
}

bool
trace_metadata_reader_t::is_thread_start(const offline_entry_t *entry,
                                         OUT std::string *error, OUT int *version,
                                         OUT offline_file_type_t *file_type)
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
        if (ver >= OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP)
            return false;
    } else {
        ver = static_cast<int>(entry->extended.valueB);
        type = static_cast<offline_file_type_t>(entry->extended.valueA);
        if (ver < OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP)
            return false;
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
                                            OUT uint64 *timestamp)
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

void
raw2trace_t::accumulate_to_statistic(raw2trace_thread_data_t *tdata,
                                     raw2trace_statistic_t stat, uint64 value)
{
    switch (stat) {
    case RAW2TRACE_STAT_COUNT_ELIDED: tdata->count_elided += value; break;
    case RAW2TRACE_STAT_DUPLICATE_SYSCALL: tdata->count_duplicate_syscall += value; break;
    case RAW2TRACE_STAT_FALSE_SYSCALL: tdata->count_false_syscall += value; break;
    case RAW2TRACE_STAT_RSEQ_ABORT: tdata->count_rseq_abort += value; break;
    case RAW2TRACE_STAT_RSEQ_SIDE_EXIT: tdata->count_rseq_side_exit += value; break;
    case RAW2TRACE_STAT_EARLIEST_TRACE_TIMESTAMP:
        tdata->earliest_trace_timestamp =
            std::min(tdata->earliest_trace_timestamp, value);
        break;
    case RAW2TRACE_STAT_LATEST_TRACE_TIMESTAMP:
        tdata->latest_trace_timestamp = std::max(tdata->latest_trace_timestamp, value);
        break;
    default: DR_ASSERT(false);
    }
}

uint64
raw2trace_t::get_statistic(raw2trace_statistic_t stat)
{
    switch (stat) {
    case RAW2TRACE_STAT_COUNT_ELIDED: return count_elided_;
    case RAW2TRACE_STAT_DUPLICATE_SYSCALL: return count_duplicate_syscall_;
    case RAW2TRACE_STAT_FALSE_SYSCALL: return count_false_syscall_;
    case RAW2TRACE_STAT_RSEQ_ABORT: return count_rseq_abort_;
    case RAW2TRACE_STAT_RSEQ_SIDE_EXIT: return count_rseq_side_exit_;
    case RAW2TRACE_STAT_EARLIEST_TRACE_TIMESTAMP: return earliest_trace_timestamp_;
    case RAW2TRACE_STAT_LATEST_TRACE_TIMESTAMP: return latest_trace_timestamp_;
    default: DR_ASSERT(false); return 0;
    }
}

bool
raw2trace_t::is_maybe_blocking_syscall(uintptr_t number)
{
#ifdef LINUX
    // Some of these can be non-blocking if certain flags were set in prior system
    // calls.  If we want to track such state it would have to be done in the tracer.
    // For our purposes of adding context switches when scheduling user-mode threads,
    // "maybe blocking" is close enough.
    //
    // TODO i#5843: This is not an exhaustive up-to-date list: there are newer variants
    // of some of these and brand-new syscalls which may block.  We need to do a full
    // audit to be comprehensive, but this should cover most cases.
    switch (number) {
#    ifdef SYS_accept
    case SYS_accept:
#    endif
    case SYS_accept4:
    case SYS_close:
#    ifdef SYS_creat
    case SYS_creat:
#    endif
    case SYS_epoll_pwait:
#    ifdef SYS_epoll_wait
    case SYS_epoll_wait:
#    endif
    case SYS_fcntl:
    case SYS_flock:
    case SYS_fsync:
    case SYS_futex:
#    ifdef SYS_getpmsg
    case SYS_getpmsg:
#    endif
    case SYS_ioctl:
    case SYS_mq_open:
    case SYS_msgrcv:
    case SYS_msgsnd:
    case SYS_msync:
    case SYS_nanosleep:
#    ifdef SYS_open
    case SYS_open:
#    endif
    case SYS_openat:
#    ifdef SYS_openat2
    case SYS_openat2:
#    endif
#    ifdef SYS_pause
    case SYS_pause:
#    endif
#    ifdef SYS_poll
    case SYS_poll:
#    endif
    case SYS_ppoll:
    case SYS_pread64:
    case SYS_preadv:
    case SYS_pselect6:
#    ifndef X64
    case SYS_pselect6_time64:
#    endif
#    ifdef SYS_putpmsg
    case SYS_putpmsg:
#    endif
    case SYS_pwrite64:
    case SYS_pwritev:
    case SYS_read:
    case SYS_readv:
    case SYS_recvfrom:
    case SYS_recvmsg:
    case SYS_sched_yield:
#    ifdef SYS_select
    case SYS_select:
#    endif
    case SYS_semctl:
    case SYS_semget:
#    ifdef SYS_semop
    case SYS_semop:
#    endif
    case SYS_sendmsg:
    case SYS_sendto:
#    ifdef SYS_wait4
    case SYS_wait4:
#    endif
#    ifdef SYS_waitid
    case SYS_waitid:
#    endif
#    ifdef SYS_waitpid
    case SYS_waitpid:
#    endif
    case SYS_write:
    case SYS_writev: return true;
    default: return false;
    }
#else
    // XXX i#5843: We only support Linux for now.  For Windows we may want to
    // link in drsyscall and add maybe-blocking info to the drsyscall database.
    return false;
#endif
}

} // namespace drmemtrace
} // namespace dynamorio
