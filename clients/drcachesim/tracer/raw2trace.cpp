/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drcovlib.h"
#include "raw2trace.h"
#include "instru.h"
#include "../common/memref.h"
#include "../common/trace_entry.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

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
#define VPRINT(level, ...)                 \
    do {                                   \
        if (this->verbosity_ >= (level)) { \
            VPRINT_HEADER();               \
            fprintf(stderr, __VA_ARGS__);  \
            fflush(stderr);                \
        }                                  \
    } while (0)

static online_instru_t instru(NULL, false, NULL);

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
                                // Truncated for 32-bit, as documented.
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
    const std::string &alt_module_dir)
    : modmap_(module_map)
    , cached_user_free_(free_cb)
    , verbosity_(verbosity)
    , alt_module_dir_(alt_module_dir)
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

    last_error_ = do_module_parsing();

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
        if (!mvi->is_external && mvi->map_base != NULL && mvi->map_size != 0) {
            bool ok = dr_unmap_executable_file(mvi->map_base, mvi->map_size);
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
        module_mapper_ = module_mapper_t::create(modmap_, user_parse_, user_process_,
                                                 user_process_data_, user_free_,
                                                 verbosity_, alt_module_dir_);
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
raw2trace_t::read_and_map_modules()
{
    if (!module_mapper_) {
        auto err = do_module_parsing();
        if (!err.empty())
            return err;
    }

    set_modvec_(&module_mapper_->get_loaded_modules());
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
            VPRINT(1, "Using module %d %s stored %zd-byte contents @" PFX "\n",
                   (int)modvec_.size(), info.path, custom_data->contents_size,
                   custom_data->contents);
            modvec_.push_back(
                module_t(info.path, info.start, (byte *)custom_data->contents,
                         custom_data->contents_size, true /*external data*/));
        } else if (strcmp(info.path, "<unknown>") == 0 ||
                   // This should only happen with legacy trace data that's missing
                   // the vdso contents.
                   (!has_custom_data_ && strcmp(info.path, "[vdso]") == 0)) {
            // We won't be able to decode.
            modvec_.push_back(module_t(info.path, info.start, NULL, 0));
        } else if (info.containing_index != info.index) {
            // For split segments, drmodtrack_lookup() gave the lowest base addr,
            // so our PC offsets are from that.  We assume that the single mmap of
            // the first segment thus includes the other segments and that we don't
            // need another mmap.
            VPRINT(1, "Separate segment assumed covered: module %d seg " PFX " = %s\n",
                   (int)modvec_.size(), info.start, info.path);
            modvec_.push_back(module_t(info.path,
                                       // We want the low base not segment base.
                                       modvec_[info.containing_index].orig_base,
                                       // 0 size indicates this is a secondary segment.
                                       modvec_[info.containing_index].map_base, 0));
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
                    modvec_.push_back(module_t(info.path, info.start, NULL, 0));
                else {
                    last_error_ = "Failed to map module " + std::string(info.path);
                    return;
                }
            } else {
                VPRINT(1, "Mapped module %d @" PFX " = %s\n", (int)modvec_.size(),
                       base_pc, info.path);
                modvec_.push_back(module_t(info.path, info.start, base_pc, map_size));
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

app_pc
module_mapper_t::find_mapped_trace_bounds(app_pc trace_address, OUT app_pc *module_start,
                                          OUT size_t *module_size)
{
    if (modhandle_ == nullptr || modlist_.empty()) {
        last_error_ = "Failed to call get_module_list() first";
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
        if (trace_address >= mvi->orig_base &&
            trace_address < mvi->orig_base + mvi->map_size) {
            app_pc mapped_address = trace_address - mvi->orig_base + mvi->map_base;
            last_orig_base_ = mvi->orig_base;
            last_map_size_ = mvi->map_size;
            last_map_base_ = mvi->map_base;
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
raw2trace_t::process_header(raw2trace_thread_data_t *tdata)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_HEADER;
    entry.size = 0;
    entry.addr = TRACE_ENTRY_VERSION;
    if (!tdata->out_file->write((char *)&entry, sizeof(entry)))
        return "Failed to write header to output file";

    // First read the tid and pid entries which precede any timestamps.
    trace_header_t header = { static_cast<process_id_t>(INVALID_PROCESS_ID),
                              INVALID_THREAD_ID, 0 };
    std::string error = read_header(tdata, &header);
    if (!error.empty())
        return error;
    VPRINT(2, "File %u is thread %u\n", tdata->index, (uint)header.tid);
    VPRINT(2, "File %u is process %u\n", tdata->index, (uint)header.pid);
    thread_id_t tid = header.tid;
    tdata->tid = tid;
    tdata->cache_line_size = header.cache_line_size;
    process_id_t pid = header.pid;
    DR_ASSERT(tid != INVALID_THREAD_ID);
    DR_ASSERT(pid != (process_id_t)INVALID_PROCESS_ID);
    byte *buf_base = reinterpret_cast<byte *>(get_write_buffer(tdata));
    byte *buf = buf_base;
    // Write the arch and other type flags.
    buf += instru.append_marker(buf, TRACE_MARKER_TYPE_FILETYPE, tdata->file_type);
    // Write out the tid, pid, and timestamp.
    buf += trace_metadata_writer_t::write_tid(buf, tid);
    buf += trace_metadata_writer_t::write_pid(buf, pid);
    buf += trace_metadata_writer_t::write_marker(buf, TRACE_MARKER_TYPE_CACHE_LINE_SIZE,
                                                 header.cache_line_size);

    if (header.timestamp != 0) // Legacy traces have the timestamp in the header.
        buf += trace_metadata_writer_t::write_timestamp(buf, (uintptr_t)header.timestamp);
    // We have to write this now before we append any bb entries.
    CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
    if (!tdata->out_file->write((char *)buf_base, buf - buf_base))
        return "Failed to write to output file";
    return "";
}

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
            byte *buf = buf_base +
                trace_metadata_writer_t::write_timestamp(buf_base,
                                                         (uintptr_t)entry.timestamp.usec);
            CHECK((uint)(buf - buf_base) < WRITE_BUFFER_SIZE, "Too many entries");
            if (!tdata->out_file->write((char *)buf_base, buf - buf_base)) {
                tdata->error = "Failed to write to output file";
                return tdata->error;
            }
            continue;
        }
        // Append any delayed branch, but not until we output all (non-xfer) markers to
        // ensure we group them all with the timestamp for this thread segment.
        if (!(entry.extended.type == OFFLINE_TYPE_EXTENDED &&
              entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
              entry.extended.valueB != TRACE_MARKER_TYPE_KERNEL_EVENT &&
              entry.extended.valueB != TRACE_MARKER_TYPE_KERNEL_XFER)) {
            tdata->error = append_delayed_branch(tdata);
            if (!tdata->error.empty())
                return tdata->error;
        }
        tdata->error = process_offline_entry(tdata, &entry, tdata->tid, end_of_record,
                                             &last_bb_handled);
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
        if (!tdata->error.empty()) {
            if (thread_file_at_eof(tdata)) {
                // Rather than a fatal error we try to continue to provide partial
                // results in case the disk was full or there was some other issue.
                WARN("Input file for thread %d is truncated", (uint)tdata->tid);
                offline_entry_t entry;
                entry.extended.type = OFFLINE_TYPE_EXTENDED;
                entry.extended.ext = OFFLINE_EXT_TYPE_FOOTER;
                bool last_bb_handled = true;
                tdata->error = process_offline_entry(tdata, &entry, tdata->tid,
                                                     &end_of_file, &last_bb_handled);
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
            error = process_thread_file(&thread_data_[i]);
            if (!error.empty())
                return error;
            count_elided_ += thread_data_[i].count_elided;
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
            if (!tdata.error.empty())
                return tdata.error;
            count_elided_ += tdata.count_elided;
        }
    }
    VPRINT(1, "Reconstructed " UINT64_FORMAT_STRING " elided addresses.\n",
           count_elided_);
    VPRINT(1, "Successfully converted %zu thread files\n", thread_data_.size());
    return "";
}

raw2trace_t::block_summary_t *
raw2trace_t::lookup_block_summary(void *tls, app_pc block_start)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    if (block_start == tdata->last_decode_block_start) {
        VPRINT(5, "Using last block summary " PFX " for " PFX "\n",
               tdata->last_block_summary, tdata->last_decode_block_start);
        return tdata->last_block_summary;
    }
    block_summary_t *ret = static_cast<block_summary_t *>(
        hashtable_lookup(&decode_cache_[tdata->worker], block_start));
    if (ret != nullptr) {
        DEBUG_ASSERT(ret->start_pc == block_start);
        tdata->last_decode_block_start = block_start;
        tdata->last_block_summary = ret;
        VPRINT(5, "Caching last block summary " PFX " for " PFX "\n",
               tdata->last_block_summary, tdata->last_decode_block_start);
    }
    return ret;
}

instr_summary_t *
raw2trace_t::lookup_instr_summary(void *tls, uint64 modidx, uint64 modoffs,
                                  app_pc block_start, int index, app_pc pc,
                                  OUT block_summary_t **block_summary)
{
    block_summary_t *block = lookup_block_summary(tls, block_start);
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
raw2trace_t::instr_summary_exists(void *tls, uint64 modidx, uint64 modoffs,
                                  app_pc block_start, int index, app_pc pc)
{
    return lookup_instr_summary(tls, modidx, modoffs, block_start, index, pc, nullptr) !=
        nullptr;
}

instr_summary_t *
raw2trace_t::create_instr_summary(void *tls, uint64 modidx, uint64 modoffs,
                                  block_summary_t *block, app_pc block_start,
                                  int instr_count, int index, INOUT app_pc *pc,
                                  app_pc orig)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    if (block == nullptr) {
        block = new block_summary_t(block_start, instr_count);
        DEBUG_ASSERT(index >= 0 && index < static_cast<int>(block->instrs.size()));
        hashtable_add(&decode_cache_[tdata->worker], block_start, block);
        VPRINT(5, "Created new block summary " PFX " for " PFX "\n", block, block_start);
        tdata->last_decode_block_start = block_start;
        tdata->last_block_summary = block;
    }
    instr_summary_t *desc = &block->instrs[index];
    if (!instr_summary_t::construct(dcontext_, block_start, pc, orig, desc, verbosity_)) {
        WARN("Encountered invalid/undecodable instr @ %s+" PIFX,
             modvec_()[static_cast<size_t>(modidx)].path, IF_NOT_X64((uint)) modoffs);
        return nullptr;
    }
    return desc;
}

const instr_summary_t *
raw2trace_t::get_instr_summary(void *tls, uint64 modidx, uint64 modoffs,
                               app_pc block_start, int instr_count, int index,
                               INOUT app_pc *pc, app_pc orig)
{
    block_summary_t *block;
    const instr_summary_t *ret =
        lookup_instr_summary(tls, modidx, modoffs, block_start, index, *pc, &block);
    if (ret == nullptr) {
        return create_instr_summary(tls, modidx, modoffs, block, block_start, instr_count,
                                    index, pc, orig);
    }
    *pc = ret->next_pc();
    return ret;
}

// These flags are difficult to set on construction: because one instr_t may have
// multiple flags, we'd need get_instr_summary() to take in a vector or sthg.
// Instead we set after the fact.
bool
raw2trace_t::set_instr_summary_flags(void *tls, uint64 modidx, uint64 modoffs,
                                     app_pc block_start, int instr_count, int index,
                                     app_pc pc, app_pc orig, bool write, int memop_index,
                                     bool use_remembered_base, bool remember_base)
{
    block_summary_t *block;
    instr_summary_t *desc =
        lookup_instr_summary(tls, modidx, modoffs, block_start, index, pc, &block);
    if (desc == nullptr) {
        app_pc pc_copy = pc;
        desc = create_instr_summary(tls, modidx, modoffs, block, block_start, instr_count,
                                    index, &pc_copy, orig);
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
    desc->next_pc_ = *pc;
    DEBUG_ASSERT(desc->next_pc_ > desc->pc_);
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
    if (instr_is_cti(instr))
        desc->packed_ |= kIsCtiMask;

#ifdef AARCH64
    bool is_dc_zva = instru_t::is_aarch64_dc_zva_instr(instr);
    if (is_dc_zva)
        desc->packed_ |= kIsAarch64DcZvaMask;
#endif

    desc->type_ = instru_t::instr_to_instr_type(instr);
    desc->prefetch_type_ = is_prefetch ? instru_t::instr_to_prefetch_type(instr) : 0;
    desc->flush_type_ = is_flush ? instru_t::instr_to_flush_type(instr) : 0;
    desc->length_ = static_cast<byte>(instr_length(dcontext, instr));

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
raw2trace_t::get_next_entry(void *tls)
{
    // We do our own buffering to avoid performance problems for some istreams where
    // seekg is slow.  We expect just 1 entry peeked and put back the vast majority of the
    // time, but we use a vector for generality.  We expect our overall performance to
    // be i/o bound (or ISA decode bound) and aren't worried about some extra copies
    // from the vector.
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    if (!tdata->pre_read.empty()) {
        tdata->last_entry = tdata->pre_read[0];
        tdata->pre_read.erase(tdata->pre_read.begin(), tdata->pre_read.begin() + 1);
    } else {
        if (!tdata->thread_file->read((char *)&tdata->last_entry,
                                      sizeof(tdata->last_entry)))
            return nullptr;
    }
    VPRINT(5, "[get_next_entry]: type=%d\n",
           // Some compilers think .addr.type is "int" while others think it's "unsigned
           // long".  We avoid dueling warnings by casting to int.
           static_cast<int>(tdata->last_entry.addr.type));
    return &tdata->last_entry;
}

void
raw2trace_t::unread_last_entry(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    tdata->pre_read.push_back(tdata->last_entry);
}

bool
raw2trace_t::thread_file_at_eof(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->pre_read.empty() && tdata->thread_file->eof();
}

std::string
raw2trace_t::append_delayed_branch(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    if (tdata->delayed_branch.empty())
        return "";
    VPRINT(4, "Appending delayed branch for thread %d\n", tdata->index);
    if (!tdata->out_file->write(&tdata->delayed_branch[0], tdata->delayed_branch.size()))
        return "Failed to write to output file";
    tdata->delayed_branch.clear();
    return "";
}

trace_entry_t *
raw2trace_t::get_write_buffer(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->out_buf.data();
}

bool
raw2trace_t::write(void *tls, const trace_entry_t *start, const trace_entry_t *end)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return !!tdata->out_file->write(reinterpret_cast<const char *>(start),
                                    reinterpret_cast<const char *>(end) -
                                        reinterpret_cast<const char *>(start));
}

std::string
raw2trace_t::write_delayed_branches(void *tls, const trace_entry_t *start,
                                    const trace_entry_t *end)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    CHECK(tdata->delayed_branch.empty(), "Failed to flush delayed branch");
    tdata->delayed_branch.insert(tdata->delayed_branch.begin(),
                                 reinterpret_cast<const char *>(start),
                                 reinterpret_cast<const char *>(end));
    return "";
}

std::string
raw2trace_t::write_footer(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    trace_entry_t entry;
    entry.type = TRACE_TYPE_FOOTER;
    entry.size = 0;
    entry.addr = 0;
    if (!tdata->out_file->write((char *)&entry, sizeof(entry)))
        return "Failed to write footer to output file";
    return "";
}

std::string
raw2trace_t::on_thread_end(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
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
                              false /*bytes*/);
    }
}

void
raw2trace_t::set_prev_instr_rep_string(void *tls, bool value)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    tdata->prev_instr_was_rep_string = value;
}

bool
raw2trace_t::was_prev_instr_rep_string(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->prev_instr_was_rep_string;
}

int
raw2trace_t::get_version(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->version;
}

size_t
raw2trace_t::get_cache_line_size(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->cache_line_size;
}

offline_file_type_t
raw2trace_t::get_file_type(void *tls)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    return tdata->file_type;
}

raw2trace_t::raw2trace_t(const char *module_map,
                         const std::vector<std::istream *> &thread_files,
                         const std::vector<std::ostream *> &out_files, void *dcontext,
                         unsigned int verbosity, int worker_count,
                         const std::string &alt_module_dir)
    : trace_converter_t(dcontext)
    , worker_count_(worker_count)
    , user_process_(nullptr)
    , user_process_data_(nullptr)
    , modmap_(module_map)
    , verbosity_(verbosity)
    , alt_module_dir_(alt_module_dir)
{
    if (dcontext == NULL) {
#ifdef ARM
        // We keep the mode at ARM and rely on LSB=1 offsets in the modoffs fields
        // to trigger Thumb decoding.
        dr_set_isa_mode(dcontext, DR_ISA_ARM_A32, NULL);
#endif
    }
    thread_data_.resize(thread_files.size());
    for (size_t i = 0; i < thread_data_.size(); ++i) {
        thread_data_[i].index = static_cast<int>(i);
        thread_data_[i].thread_file = thread_files[i];
        thread_data_[i].out_file = out_files[i];
    }
    // Since we know the traced-thread count up front, we use a simple round-robin
    // static work assigment.  This won't be as load balanced as a dynamic work
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
            worker_tasks_[worker].push_back(&thread_data_[i]);
            thread_data_[i].worker = worker;
            worker = (worker + 1) % worker_count_;
        }
    } else
        cache_count = 1;
    decode_cache_.resize(cache_count);
    for (int i = 0; i < cache_count; ++i) {
        // We go ahead and start with a reasonably large capacity.
        // We do not want the built-in mutex: this is per-worker so it can be lockless.
        hashtable_init_ex(&decode_cache_[i], 16, HASH_INTPTR, false, false, nullptr,
                          nullptr, nullptr);
        // We pay a little memory to get a lower load factor, unless we have
        // many duplicated tables.
        hashtable_config_t config = { sizeof(config), true,
                                      worker_count_ <= 8
                                          ? 40U
                                          : (worker_count_ <= 16 ? 50U : 60U) };
        hashtable_configure(&decode_cache_[i], &config);
    }
}

raw2trace_t::~raw2trace_t()
{
    module_mapper_.reset();
    for (size_t i = 0; i < decode_cache_.size(); ++i) {
        // XXX: We can't use a free-payload function b/c we can't get the dcontext there,
        // so we have to explicitly free the payloads.
        for (uint j = 0; j < HASHTABLE_SIZE(decode_cache_[i].table_bits); j++) {
            for (hash_entry_t *e = decode_cache_[i].table[j]; e != NULL; e = e->next) {
                delete (static_cast<block_summary_t *>(e->payload));
            }
        }
        hashtable_delete(&decode_cache_[i]);
    }
}

bool
trace_metadata_reader_t::is_thread_start(const offline_entry_t *entry,
                                         OUT std::string *error, OUT int *version,
                                         OUT offline_file_type_t *file_type)
{
    *error = "";
    if (entry->extended.type != OFFLINE_TYPE_EXTENDED ||
        entry->extended.ext != OFFLINE_EXT_TYPE_HEADER) {
        return false;
    }
    int ver = static_cast<int>(entry->extended.valueA);
    if (version != nullptr)
        *version = ver;
    offline_file_type_t type = static_cast<offline_file_type_t>(entry->extended.valueB);
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

    size_t timestamp_pos = 0;
    std::string error;
    if (trace_metadata_reader_t::is_thread_start(offline_entries, &error, nullptr,
                                                 nullptr) &&
        error.empty()) {
        if (size < 4)
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;

        // XXX: Make it easier to add more markers. Iterate over the entries until
        // the timestamp entry or some non-meta entry is encountered.
        if (offline_entries[++timestamp_pos].tid.type != OFFLINE_TYPE_THREAD ||
            offline_entries[++timestamp_pos].pid.type != OFFLINE_TYPE_PID ||
            (offline_entries[++timestamp_pos].extended.type != OFFLINE_TYPE_EXTENDED ||
             offline_entries[timestamp_pos].extended.ext != OFFLINE_EXT_TYPE_MARKER ||
             offline_entries[timestamp_pos].extended.valueB !=
                 TRACE_MARKER_TYPE_CACHE_LINE_SIZE))
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        ++timestamp_pos;
    }

    if (offline_entries[timestamp_pos].timestamp.type != OFFLINE_TYPE_TIMESTAMP)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    *timestamp = offline_entries[timestamp_pos].timestamp.usec;
    return DRMEMTRACE_SUCCESS;
}

void
raw2trace_t::add_to_statistic(void *tls, raw2trace_statistic_t stat, int value)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    switch (stat) {
    case RAW2TRACE_STAT_COUNT_ELIDED: tdata->count_elided += value; break;
    default: DR_ASSERT(false);
    }
}

uint64
raw2trace_t::get_statistic(raw2trace_statistic_t stat)
{
    switch (stat) {
    case RAW2TRACE_STAT_COUNT_ELIDED: return count_elided_;
    default: DR_ASSERT(false); return 0;
    }
}
