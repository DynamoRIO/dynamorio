/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

#define VPRINT(level, ...)                \
    do {                                  \
        if (this->verbosity >= (level)) { \
            VPRINT_HEADER();              \
            fprintf(stderr, __VA_ARGS__); \
        }                                 \
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

const char *(*module_mapper_t::user_parse)(const char *src, OUT void **data) = nullptr;
void (*module_mapper_t::user_free)(void *data) = nullptr;
bool module_mapper_t::has_custom_data_global = true;

module_mapper_t::module_mapper_t(
    const char *module_map_in, const char *(*parse_cb)(const char *src, OUT void **data),
    std::string (*process_cb)(drmodtrack_info_t *info, void *data, void *user_data),
    void *process_cb_user_data, void (*free_cb)(void *data), uint verbosity_in)
    : modmap(module_map_in)
    , cached_user_free(free_cb)
    , verbosity(verbosity_in)
{
    // We mutate global state because do_module_parsing() uses drmodtrack, which
    // wants global functions. The state isn't needed past do_module_parsing(), so
    // we make sure to reset it afterwards.
    DR_ASSERT(user_parse == nullptr);
    DR_ASSERT(user_free == nullptr);

    user_parse = parse_cb;
    user_process = process_cb;
    user_process_data = process_cb_user_data;
    user_free = free_cb;
    // has_custom_data_global is potentially mutated in parse_custom_module_data.
    // It is assumed to be set to 'true' initially.
    has_custom_data_global = true;

    last_error = do_module_parsing();

    // capture has_custom_data_global's value for this instance.
    has_custom_data = has_custom_data_global;

    user_parse = nullptr;
    user_free = nullptr;
}

module_mapper_t::~module_mapper_t()
{
    // update user_free
    user_free = cached_user_free;
    // drmodtrack_offline_exit requires the parameter to be non-null, but we
    // may not have even initialized the modhandle yet.
    if (modhandle != nullptr && drmodtrack_offline_exit(modhandle) != DRCOVLIB_SUCCESS) {
        WARN("Failed to clean up module table data");
    }
    user_free = nullptr;
    for (std::vector<module_t>::iterator mvi = modvec.begin(); mvi != modvec.end();
         ++mvi) {
        if (!mvi->is_external && mvi->map_base != NULL && mvi->map_size != 0) {
            bool ok = dr_unmap_executable_file(mvi->map_base, mvi->map_size);
            if (!ok)
                WARN("Failed to unmap module %s", mvi->path);
        }
    }
    modhandle = nullptr;
    modvec.clear();
}

std::string
raw2trace_t::handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                                std::string (*process_cb)(drmodtrack_info_t *info,
                                                          void *data, void *user_data),
                                void *process_cb_user_data, void (*free_cb)(void *data))
{
    user_parse = parse_cb;
    user_process = process_cb;
    user_process_data = process_cb_user_data;
    user_free = free_cb;
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
        has_custom_data_global = false;
        if (!warned_once) { // Race is fine: modtrack parsing is global already.
            WARN("Incorrect module field version %d: attempting to handle legacy format",
                 version);
            warned_once = true;
        }
        // First, see if the user_parse is happy:
        if (user_parse != nullptr) {
            void *user_data;
            buf = (*user_parse)(buf, &user_data);
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
    if (user_parse != nullptr)
        buf = (*user_parse)(buf, &custom_data->user_data);
    *data = custom_data;
    return buf;
}

void
module_mapper_t::free_custom_module_data(void *data)
{
    custom_module_data_t *custom_data = (custom_module_data_t *)data;
    if (user_free != nullptr)
        (*user_free)(custom_data->user_data);
    delete custom_data;
}

std::string
raw2trace_t::do_module_parsing()
{
    if (!module_mapper) {
        module_mapper = module_mapper_t::create(modmap, user_parse, user_process,
                                                user_process_data, user_free, verbosity);
    }
    return module_mapper->get_last_error();
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
    if (drmodtrack_offline_read(INVALID_FILE, modmap, NULL, &modhandle, &num_mods) !=
        DRCOVLIB_SUCCESS)
        return "Failed to parse module file";
    modlist.resize(num_mods);
    for (uint i = 0; i < num_mods; i++) {
        modlist[i].struct_size = sizeof(modlist[i]);
        if (drmodtrack_offline_lookup(modhandle, i, &modlist[i]) != DRCOVLIB_SUCCESS)
            return "Failed to query module file";
        if (user_process != nullptr) {
            custom_module_data_t *custom = (custom_module_data_t *)modlist[i].custom;
            std::string error =
                (*user_process)(&modlist[i], custom->user_data, user_process_data);
            if (!error.empty())
                return error;
        }
    }
    return "";
}

std::string
raw2trace_t::read_and_map_modules()
{
    if (!module_mapper) {
        auto err = do_module_parsing();
        if (!err.empty())
            return err;
    }

    set_modvec(&module_mapper->get_loaded_modules());
    return module_mapper->get_last_error();
}

void
module_mapper_t::read_and_map_modules()
{
    if (!last_error.empty())
        return;
    for (auto it = modlist.begin(); it != modlist.end(); ++it) {
        drmodtrack_info_t &info = *it;
        custom_module_data_t *custom_data = (custom_module_data_t *)info.custom;
        if (custom_data != nullptr && custom_data->contents_size > 0) {
            VPRINT(1, "Using module %d %s stored %zd-byte contents @" PFX "\n",
                   (int)modvec.size(), info.path, custom_data->contents_size,
                   (ptr_uint_t)custom_data->contents);
            modvec.push_back(
                module_t(info.path, info.start, (byte *)custom_data->contents,
                         custom_data->contents_size, true /*external data*/));
        } else if (strcmp(info.path, "<unknown>") == 0 ||
                   // This should only happen with legacy trace data that's missing
                   // the vdso contents.
                   (!has_custom_data && strcmp(info.path, "[vdso]") == 0)) {
            // We won't be able to decode.
            modvec.push_back(module_t(info.path, info.start, NULL, 0));
        } else if (info.containing_index != info.index) {
            // For split segments, drmodtrack_lookup() gave the lowest base addr,
            // so our PC offsets are from that.  We assume that the single mmap of
            // the first segment thus includes the other segments and that we don't
            // need another mmap.
            VPRINT(1, "Separate segment assumed covered: module %d seg " PFX " = %s\n",
                   (int)modvec.size(), (ptr_uint_t)info.start, info.path);
            modvec.push_back(module_t(info.path,
                                      // We want the low base not segment base.
                                      modvec[info.containing_index].orig_base,
                                      // 0 size indicates this is a secondary segment.
                                      modvec[info.containing_index].map_base, 0));
        } else {
            size_t map_size;
            byte *base_pc =
                dr_map_executable_file(info.path, DR_MAPEXE_SKIP_WRITABLE, &map_size);
            if (base_pc == NULL) {
                // We expect to fail to map dynamorio.dll for x64 Windows as it
                // is built /fixed.  (We could try to have the map succeed w/o relocs,
                // but we expect to not care enough about code in DR).
                if (strstr(info.path, "dynamorio") != NULL)
                    modvec.push_back(module_t(info.path, info.start, NULL, 0));
                else {
                    last_error = "Failed to map module " + std::string(info.path);
                    return;
                }
            } else {
                VPRINT(1, "Mapped module %d @" PFX " = %s\n", (int)modvec.size(),
                       (ptr_uint_t)base_pc, info.path);
                modvec.push_back(module_t(info.path, info.start, base_pc, map_size));
            }
        }
    }
    VPRINT(1, "Successfully read %zu modules\n", modlist.size());
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
    *mapped_address = module_mapper->find_mapped_trace_address(trace_address);
    return module_mapper->get_last_error();
}

app_pc
module_mapper_t::find_mapped_trace_bounds(app_pc trace_address, OUT app_pc *module_start,
                                          OUT size_t *module_size)
{
    if (modhandle == nullptr || modlist.empty()) {
        last_error = "Failed to call get_module_list() first";
        return nullptr;
    }

    // For simplicity we do a linear search, caching the prior hit.
    if (trace_address >= last_orig_base &&
        trace_address < last_orig_base + last_map_size) {
        if (module_start != nullptr)
            *module_start = last_map_base;
        if (module_size != nullptr)
            *module_size = last_map_size;
        return trace_address - last_orig_base + last_map_base;
    }
    for (std::vector<module_t>::iterator mvi = modvec.begin(); mvi != modvec.end();
         ++mvi) {
        if (trace_address >= mvi->orig_base &&
            trace_address < mvi->orig_base + mvi->map_size) {
            app_pc mapped_address = trace_address - mvi->orig_base + mvi->map_base;
            last_orig_base = mvi->orig_base;
            last_map_size = mvi->map_size;
            last_map_base = mvi->map_base;
            if (module_start != nullptr)
                *module_start = last_map_base;
            if (module_size != nullptr)
                *module_size = last_map_size;
            return mapped_address;
        }
    }
    last_error = "Trace address not found";
    return nullptr;
}

app_pc
module_mapper_t::find_mapped_trace_address(app_pc trace_address)
{
    return find_mapped_trace_bounds(trace_address, nullptr, nullptr);
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
    process_id_t pid = header.pid;
    DR_ASSERT(tid != INVALID_THREAD_ID);
    DR_ASSERT(pid != (process_id_t)INVALID_PROCESS_ID);
    // Write out the tid, pid, and timestamp.
    byte *buf_base = reinterpret_cast<byte *>(get_write_buffer(tdata));
    byte *buf = buf_base;
    buf += trace_metadata_writer_t::write_tid(buf, tid);
    buf += trace_metadata_writer_t::write_pid(buf, pid);
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
        tdata->saw_header =
            trace_metadata_reader_t::is_thread_start(in_entry, &tdata->error);
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
        // Append any delayed branch, but not until we output all markers to
        // ensure we group them all with the timestamp for this thread segment.
        if (entry.extended.type != OFFLINE_TYPE_EXTENDED ||
            entry.extended.ext != OFFLINE_EXT_TYPE_MARKER) {
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
                ss << "Failed to process file for thread " << (uint)tdata->tid;
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
    if (thread_data.empty())
        return "No thread files found.";
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (worker_count == 0) {
        for (size_t i = 0; i < thread_data.size(); ++i) {
            error = process_thread_file(&thread_data[i]);
            if (!error.empty())
                return error;
        }
    } else {
        // The files can be converted concurrently.
        std::vector<std::thread> threads;
        VPRINT(1, "Creating %d worker threads\n", worker_count);
        threads.reserve(worker_count);
        for (int i = 0; i < worker_count; ++i) {
            threads.push_back(
                std::thread(&raw2trace_t::process_tasks, this, &worker_tasks[i]));
        }
        for (std::thread &thread : threads)
            thread.join();
        for (auto &tdata : thread_data) {
            if (!tdata.error.empty())
                return error;
        }
    }
    VPRINT(1, "Successfully converted %zu thread files\n", thread_data.size());
    return "";
}

const instr_summary_t *
raw2trace_t::get_instr_summary(void *tls, uint64 modidx, uint64 modoffs, INOUT app_pc *pc,
                               app_pc orig)
{
    auto tdata = reinterpret_cast<raw2trace_thread_data_t *>(tls);
    const app_pc decode_pc = *pc;
    // For rep string loops we expect the same PC many times in a row.
    if (decode_pc == tdata->last_decode_pc)
        return tdata->last_summary;
    const instr_summary_t *ret = static_cast<const instr_summary_t *>(
        hashtable_lookup(&decode_cache[tdata->worker], decode_pc));
    if (ret == nullptr) {
        instr_summary_t *desc = new instr_summary_t();
        if (!instr_summary_t::construct(dcontext, pc, orig, desc, verbosity)) {
            WARN("Encountered invalid/undecodable instr @ %s+" PFX,
                 modvec()[static_cast<size_t>(modidx)].path, (ptr_uint_t)modoffs);
            delete desc;
            return nullptr;
        }
        hashtable_add(&decode_cache[tdata->worker], decode_pc, desc);
        ret = desc;
    } else {
        /* XXX i#3129: Log some rendering of the instruction summary that will be
         * returned.
         */
        *pc = ret->next_pc();
    }
    tdata->last_decode_pc = decode_pc;
    tdata->last_summary = ret;
    return ret;
}

bool
instr_summary_t::construct(void *dcontext, INOUT app_pc *pc, app_pc orig_pc,
                           OUT instr_summary_t *desc, uint verbosity)
{
    struct instr_destroy_t {
        instr_destroy_t(void *dcontext_in, instr_t *instr_in)
            : dcontext(dcontext_in)
            , instr(instr_in)
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

    *pc = decode(dcontext, *pc, instr);
    if (*pc == nullptr || !instr_valid(instr)) {
        return false;
    }
    if (verbosity > 3) {
        instr_set_translation(instr, orig_pc);
        dr_print_instr(dcontext, STDOUT, instr, "");
    }
    desc->next_pc_ = *pc;
    desc->packed_ = 0;

    bool is_prefetch = instr_is_prefetch(instr);
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

    desc->type_ = instru_t::instr_to_instr_type(instr);
    desc->prefetch_type_ = is_prefetch ? instru_t::instr_to_prefetch_type(instr) : 0;
    desc->length_ = static_cast<byte>(instr_length(dcontext, instr));

    if (reads_memory || writes_memory) {
        for (int i = 0, e = instr_num_srcs(instr); i < e; ++i) {
            opnd_t op = instr_get_src(instr, i);
            if (opnd_is_memory_reference(op))
                desc->mem_srcs_and_dests_.push_back(op);
        }
        desc->num_mem_srcs_ = static_cast<uint8_t>(desc->mem_srcs_and_dests_.size());

        for (int i = 0, e = instr_num_dsts(instr); i < e; ++i) {
            opnd_t op = instr_get_dst(instr, i);
            if (opnd_is_memory_reference(op))
                desc->mem_srcs_and_dests_.push_back(op);
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
    va_list args;
    va_start(args, fmt);
    if (verbosity >= level) {
        VPRINT_HEADER();
        vfprintf(stderr, fmt, args);
    }
    va_end(args);
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

raw2trace_t::raw2trace_t(const char *module_map_in,
                         const std::vector<std::istream *> &thread_files_in,
                         const std::vector<std::ostream *> &out_files_in,
                         void *dcontext_in, unsigned int verbosity_in,
                         int worker_count_in)
    : trace_converter_t(dcontext_in)
    , worker_count(worker_count_in)
    , user_process(nullptr)
    , user_process_data(nullptr)
    , modmap(module_map_in)
    , verbosity(verbosity_in)
{
    if (dcontext == NULL) {
#ifdef ARM
        // We keep the mode at ARM and rely on LSB=1 offsets in the modoffs fields
        // to trigger Thumb decoding.
        dr_set_isa_mode(dcontext, DR_ISA_ARM_A32, NULL);
#endif
    }
    thread_data.resize(thread_files_in.size());
    for (size_t i = 0; i < thread_data.size(); ++i) {
        thread_data[i].index = static_cast<int>(i);
        thread_data[i].thread_file = thread_files_in[i];
        thread_data[i].out_file = out_files_in[i];
    }
    // Since we know the traced-thread count up front, we use a simple round-robin
    // static work assigment.  This won't be as load balanced as a dynamic work
    // queue but it is much simpler.
    if (worker_count < 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count > kDefaultJobMax)
            worker_count = kDefaultJobMax;
    }
    int cache_count = worker_count;
    if (worker_count > 0) {
        worker_tasks.resize(worker_count);
        int worker = 0;
        for (size_t i = 0; i < thread_data.size(); ++i) {
            VPRINT(2, "Worker %d assigned trace thread %zd\n", worker, i);
            worker_tasks[worker].push_back(&thread_data[i]);
            thread_data[i].worker = worker;
            worker = (worker + 1) % worker_count;
        }
    } else
        cache_count = 1;
    decode_cache.resize(cache_count);
    for (int i = 0; i < cache_count; ++i) {
        // We go ahead and start with a reasonably large capacity.
        // We do not want the built-in mutex: this is per-worker so it can be lockless.
        hashtable_init_ex(&decode_cache[i], 16, HASH_INTPTR, false, false, NULL, NULL,
                          NULL);
        // We pay a little memory to get a lower load factor, unless we have
        // many duplicated tables.
        hashtable_config_t config = { sizeof(config), true,
                                      worker_count <= 8
                                          ? 40U
                                          : (worker_count <= 16 ? 50U : 60U) };
        hashtable_configure(&decode_cache[i], &config);
    }
}

raw2trace_t::~raw2trace_t()
{
    module_mapper.reset();
    for (size_t i = 0; i < decode_cache.size(); ++i) {
        // XXX: We can't use a free-payload function b/c we can't get the dcontext there,
        // so we have to explicitly free the payloads.
        for (uint j = 0; j < HASHTABLE_SIZE(decode_cache[i].table_bits); j++) {
            for (hash_entry_t *e = decode_cache[i].table[j]; e != NULL; e = e->next) {
                delete (static_cast<instr_summary_t *>(e->payload));
            }
        }
        hashtable_delete(&decode_cache[i]);
    }
}

bool
trace_metadata_reader_t::is_thread_start(const offline_entry_t *entry, std::string *error)
{
    *error = "";
    if (entry->extended.type != OFFLINE_TYPE_EXTENDED ||
        entry->extended.ext != OFFLINE_EXT_TYPE_HEADER) {
        return false;
    }
    if (entry->extended.valueA != OFFLINE_FILE_VERSION) {
        std::stringstream ss;
        ss << "Version mismatch: expect " << OFFLINE_FILE_VERSION << " vs "
           << (int)entry->extended.valueA;
        *error = ss.str();
        return false;
    }
    return true;
}

std::string
trace_metadata_reader_t::check_entry_thread_start(const offline_entry_t *entry)
{
    std::string error;
    if (is_thread_start(entry, &error))
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
    if (trace_metadata_reader_t::is_thread_start(offline_entries, &error) &&
        error.empty()) {
        if (size < 4)
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        if (offline_entries[++timestamp_pos].tid.type != OFFLINE_TYPE_THREAD ||
            offline_entries[++timestamp_pos].pid.type != OFFLINE_TYPE_PID)
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        ++timestamp_pos;
    }

    if (offline_entries[timestamp_pos].timestamp.type != OFFLINE_TYPE_TIMESTAMP)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    *timestamp = offline_entries[timestamp_pos].timestamp.usec;
    return DRMEMTRACE_SUCCESS;
}
