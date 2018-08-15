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
#include <vector>

// XXX: DR should export this
#define INVALID_THREAD_ID 0

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

#define VPRINT(level, ...)                     \
    do {                                       \
        if (this->verbosity >= (level)) {      \
            fprintf(stderr, "[drmemtrace]: "); \
            fprintf(stderr, __VA_ARGS__);      \
        }                                      \
    } while (0)

#define DO_VERBOSE(level, x)              \
    do {                                  \
        if (verbosity >= (level)) {       \
            x; /* ; makes vera++ happy */ \
        }                                 \
    } while (0)

#define RETURN_IF_ERROR(expr) \
    do {                      \
        auto error = expr;    \
        if (!error.empty())   \
            return error;     \
    } while (0)

/***************************************************************************
 * Module list
 */

const char *(*module_mapper_t::user_parse)(const char *src, OUT void **data) = nullptr;
void (*module_mapper_t::user_free)(void *data) = nullptr;
bool module_mapper_t::has_custom_data_global = true;

int
instruction_converter_t::write_thread_exit(byte *buffer, pid_t tid)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_thread_exit(buffer, tid);
}

int
instruction_converter_t::write_marker(byte *buffer, trace_marker_type_t type,
                                      uintptr_t val)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_marker(buffer, type, val);
}

int
instruction_converter_t::write_iflush(byte *buffer, addr_t start, size_t size)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_iflush(buffer, start, size);
}

int
instruction_converter_t::write_pid(byte *buffer, pid_t pid)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_pid(buffer, pid);
}
int
instruction_converter_t::write_tid(byte *buffer, pid_t tid)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_tid(buffer, tid);
}

int
instruction_converter_t::write_timestamp(byte *buffer, uint64 timestamp)
{
    online_instru_t instru(NULL, false, NULL);
    return instru.append_marker(buffer, TRACE_MARKER_TYPE_TIMESTAMP,
                                // Truncated for 32-bit, as documented.
                                (uintptr_t)timestamp);
}

module_mapper_t::module_mapper_t(
    const char *module_map_in, const char *(*parse_cb)(const char *src, OUT void **data),
    std::string (*process_cb)(drmodtrack_info_t *info, void *data, void *user_data),
    void *process_cb_user_data, void (*free_cb)(void *data), uint verbosity_in)
    : modmap(module_map_in)
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
    // drmodtrack_offline_exit requires the parameter to be non-null, but we
    // may not have even initialized the modhandle yet.
    if (modhandle != nullptr && drmodtrack_offline_exit(modhandle) != DRCOVLIB_SUCCESS) {
        WARN("Failed to clean up module table data");
    }
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
    if (!module_mapper)
        module_mapper.reset(new module_mapper_t(modmap, user_parse, user_process,
                                                user_process_data, user_free, verbosity));
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
    if (!module_mapper)
        RETURN_IF_ERROR(do_module_parsing());

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
    return module_mapper->find_mapped_trace_address(trace_address, mapped_address);
}

std::string
module_mapper_t::find_mapped_trace_address(app_pc trace_address,
                                           OUT app_pc *mapped_address)
{
    if (modhandle == nullptr || modlist.empty())
        return "Failed to call do_module_parsing_and_mapping() first";
    if (mapped_address == nullptr)
        return "Invalid parameter";
    // For simplicity we do a linear search, caching the prior hit.
    if (trace_address >= last_orig_base &&
        trace_address < last_orig_base + last_map_size) {
        *mapped_address = trace_address - last_orig_base + last_map_base;
        return "";
    }
    for (std::vector<module_t>::iterator mvi = modvec.begin(); mvi != modvec.end();
         ++mvi) {
        if (trace_address >= mvi->orig_base &&
            trace_address < mvi->orig_base + mvi->map_size) {
            *mapped_address = trace_address - mvi->orig_base + mvi->map_base;
            last_orig_base = mvi->orig_base;
            last_map_size = mvi->map_size;
            last_map_base = mvi->map_base;
            return "";
        }
    }
    return "Trace address not found";
}

/***************************************************************************
 * Disassembly to fill in instr and memref entries
 */

// We do our own buffering to avoid performance problems for some istreams where
// seekg is slow.  We expect just 1 entry peeked and put back the vast majority of the
// time, but we use a vector for generality.  We expect our overall performance to
// be i/o bound (or ISA decode bound) and aren't worried about some extra copies
// from the vector.
bool
raw2trace_t::read_from_thread_file(uint tidx, offline_entry_t *dest, size_t count,
                                   OUT size_t *num_read)
{
    size_t from_buf = 0;
    if (!pre_read[tidx].empty()) {
        from_buf = (std::min)(pre_read[tidx].size(), count);
        memcpy(dest, &pre_read[tidx][0], from_buf * sizeof(*dest));
        pre_read[tidx].erase(pre_read[tidx].begin(), pre_read[tidx].begin() + from_buf);
        dest += from_buf;
        count -= from_buf;
    }
    if (count > 0) {
        if (!thread_files[tidx]->read((char *)dest, count * sizeof(*dest))) {
            if (num_read != nullptr)
                *num_read = from_buf + (size_t)thread_files[tidx]->gcount();
            return false;
        }
    }
    if (num_read != nullptr)
        *num_read = from_buf + count;
    return true;
}

void
raw2trace_t::unread_from_thread_file(uint tidx, offline_entry_t *dest, size_t count)
{
    // We expect 1 the vast majority of the time, 2 occasionally.
    for (size_t i = 0; i < count; ++i)
        pre_read[tidx].push_back(*(dest + i));
}

bool
raw2trace_t::thread_file_at_eof(uint tidx)
{
    return pre_read[tidx].empty() && thread_files[tidx]->eof();
}

std::string
raw2trace_t::append_delayed_branch(uint tidx)
{
    if (delayed_branch[tidx].empty())
        return "";
    VPRINT(4, "Appending delayed branch for thread %d\n", tidx);
    if (!out_file->write(&delayed_branch[tidx][0], delayed_branch[tidx].size()))
        return "Failed to write to output file";
    delayed_branch[tidx].clear();
    return "";
}

const offline_entry_t *
raw2trace_t::get_next_entry()
{
    if (!read_from_thread_file(tidx, &last_entry, 1))
        return nullptr;
    return &last_entry;
}

void
raw2trace_t::unread_last_entry()
{
    unread_from_thread_file(tidx, &last_entry, 1);
}

trace_entry_t *
raw2trace_t::get_write_buffer()
{
    return out_buf;
}

bool
raw2trace_t::write(const trace_entry_t *start, const trace_entry_t *end)
{
    return !!out_file->write(reinterpret_cast<const char *>(start),
                             reinterpret_cast<const char *>(end) -
                                 reinterpret_cast<const char *>(start));
}

std::string
raw2trace_t::write_delayed_branches(const trace_entry_t *start, const trace_entry_t *end)
{
    CHECK(delayed_branch[tidx].empty(), "Failed to flush delayed branch");
    delayed_branch[tidx].insert(delayed_branch[tidx].begin(), (char *)start, (char *)end);
    return "";
}
std::string
raw2trace_t::on_thread_end()
{
    offline_entry_t entry;
    if (read_from_thread_file(tidx, &entry, 1) || !thread_file_at_eof(tidx))
        return "Footer is not the final entry";
    --thread_count;
    return "";
}

bool
instr_summary_t::construct(void *dcontext, INOUT app_pc *pc, app_pc orig_pc,
                           OUT instr_summary_t *desc, uint verbosity)
{
    instr_t *instr = instr_create(dcontext);
    *pc = decode(dcontext, *pc, instr);
    if (*pc == nullptr || !instr_valid(instr)) {
        instr_destroy(dcontext, instr);
        return false;
    }
    DO_VERBOSE(3, {
        instr_set_translation(instr, orig_pc);
        dr_print_instr(dcontext, STDOUT, instr, "");
    });
    desc->next_pc_ = *pc;
    desc->packed_ = 0;

    bool is_prefetch = instr_is_prefetch(instr);
    bool reads_memory = instr_reads_memory(instr);
    bool writes_memory = instr_writes_memory(instr);

    desc->packed_ |= reads_memory << kReadsMemPos;
    desc->packed_ |= writes_memory << kWritesMemPos;
    desc->packed_ |= is_prefetch << kIsPrefetchPos;
    desc->packed_ |= instru_t::instr_is_flush(instr) << kIsFlushPos;
    desc->packed_ |= instr_is_cti(instr) << kIsCtiPos;

    desc->type_ = instru_t::instr_to_instr_type(instr);
    desc->prefetch_type_ = is_prefetch ? instru_t::instr_to_prefetch_type(instr) : 0;
    desc->length_ = instr_length(dcontext, instr);

    if (reads_memory || writes_memory) {
#define TRAVERSE_SRCS(WHAT_TO_DO)                            \
    for (int i = 0, e = instr_num_srcs(instr); i < e; ++i) { \
        opnd_t op = instr_get_src(instr, i);                 \
        if (opnd_is_memory_reference(op)) {                  \
            WHAT_TO_DO;                                      \
        }                                                    \
    }

#define TRAVERSE_DSTS(WHAT_TO_DO)                            \
    for (int i = 0, e = instr_num_dsts(instr); i < e; ++i) { \
        opnd_t op = instr_get_dst(instr, i);                 \
        if (opnd_is_memory_reference(op)) {                  \
            WHAT_TO_DO;                                      \
        }                                                    \
    }

        TRAVERSE_SRCS(++desc->srcs_)
        TRAVERSE_DSTS(++desc->dests_)
        desc->srcs_and_dests_.resize(desc->srcs_ + desc->dests_);
        int pos = 0;
        TRAVERSE_SRCS(desc->srcs_and_dests_[pos++] = op)
        TRAVERSE_DSTS(desc->srcs_and_dests_[pos++] = op)
    }
    instr_destroy(dcontext, instr);
    return true;
}

void
raw2trace_t::log(uint level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VPRINT(level, fmt, args);
    va_end(args);
}

const instr_summary_t *
raw2trace_t::get_instr_summary(int modidx, uint modoffs, app_pc *decode_pc,
                               app_pc orig_pc)
{
    const instr_summary_t *ret =
        (const instr_summary_t *)hashtable_lookup(&decode_cache, *decode_pc);
    if (ret == NULL) {
        instr_summary_t *desc = new instr_summary_t();
        if (!instr_summary_t::construct(dcontext, decode_pc, orig_pc, desc, verbosity)) {
            WARN("Encountered invalid/undecodable instr @ %s+" PFX,
                 get_modvec()[modidx].path, (ptr_uint_t)modoffs);
            return nullptr;
        }
        hashtable_add(&decode_cache, decode_pc, desc);
        ret = desc;
    } else {
        *decode_pc = ret->next_pc();
    }
    return ret;
}

/***************************************************************************
 * Top-level
 */

std::string
raw2trace_t::merge_and_process_thread_files()
{
    // The current thread we're processing is tidx.  If it's set to thread_files.size()
    // that means we need to pick a new thread.
    if (thread_files.empty())
        return "No thread files found.";
    tidx = (uint)thread_files.size();
    thread_count = (uint)thread_files.size();
    offline_entry_t in_entry;
    online_instru_t instru(NULL, false, NULL);
    bool last_bb_handled = true;
    size_t size;
    std::vector<thread_id_t> tids(thread_files.size(), INVALID_THREAD_ID);
    std::vector<process_id_t> pids(thread_files.size(), (process_id_t)INVALID_PROCESS_ID);
    std::vector<bool> wrote_pid(thread_files.size(), false);
    std::vector<uint64> times(thread_files.size(), 0);

    // First read the tid and pid entries which precede any timestamps.
    // We append the tid to the output on every thread switch, and the pid
    // the very first time (using wrote_pid[]).
    for (uint i = 0; i < thread_files.size(); ++i) {
        tidx = i;
        trace_header_t header = { 0, INVALID_THREAD_ID, INVALID_THREAD_ID };
        RETURN_IF_ERROR(read_header(&header));
        times[i] = header.timestamp;
        tids[i] = header.tid;
        pids[i] = header.pid;
        VPRINT(2, "File %u is thread %u\n", i, (uint)header.tid);
        VPRINT(2, "File %u is process %u\n", i, (uint)header.pid);
    }

    // We read the thread files simultaneously in lockstep and merge them into
    // a single output file in timestamp order.
    // When a thread file runs out we leave its times[] entry as 0 and its file at eof.
    // We convert each offline entry into a trace_entry_t.
    // We fill in instr entries and memref type and size.
    do {
        byte *buf_base = reinterpret_cast<byte *>(get_write_buffer());
        byte *buf = buf_base;
        if (tidx >= thread_files.size()) {
            // Pick the next thread by looking for the smallest timestamp.
            uint64 min_time = 0xffffffffffffffff;
            uint next_tidx = 0;
            for (uint i = 0; i < times.size(); ++i) {
                if (times[i] == 0 && !thread_file_at_eof(i)) {
                    offline_entry_t entry;
                    if (!read_from_thread_file(i, &entry, 1))
                        return "Failed to read from input file";
                    if (entry.timestamp.type != OFFLINE_TYPE_TIMESTAMP)
                        return "Missing timestamp entry";
                    times[i] = entry.timestamp.usec;
                    VPRINT(3, "Thread %u timestamp is @0x" ZHEX64_FORMAT_STRING "\n",
                           (uint)tids[i], times[i]);
                }
                if (times[i] != 0 && times[i] < min_time) {
                    min_time = times[i];
                    next_tidx = i;
                }
            }
            VPRINT(2,
                   "Next thread in timestamp order is %u @0x" ZHEX64_FORMAT_STRING "\n",
                   (uint)tids[next_tidx], times[next_tidx]);
            tidx = next_tidx;
            // Write out the tid (and pid for the first entry).
            DR_ASSERT(tids[tidx] != INVALID_THREAD_ID);
            buf += instruction_converter_t::write_tid(buf, tids[tidx]);
            if (!wrote_pid[tidx]) {
                DR_ASSERT(pids[tidx] != (process_id_t)INVALID_PROCESS_ID);
                buf += instruction_converter_t::write_pid(buf, pids[tidx]);
                wrote_pid[tidx] = true;
            }
            buf += instruction_converter_t::write_timestamp(buf, times[tidx]);
            // We have to write this now before we append any bb entries.
            size = buf - buf_base;
            CHECK((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
            if (!out_file->write((char *)buf_base, size))
                return "Failed to write to output file";
            buf = buf_base;
            times[tidx] = 0; // Read from file for this thread's next timestamp.
        }
        VPRINT(4, "About to read thread %d at pos %d\n", (uint)tids[tidx],
               (int)thread_files[tidx]->tellg());
        if (!read_from_thread_file(tidx, &in_entry, 1)) {
            if (thread_file_at_eof(tidx)) {
                // Rather than a fatal error we try to continue to provide partial
                // results in case the disk was full or there was some other issue.
                WARN("Input file for thread %d is truncated", (uint)tids[tidx]);
                in_entry.extended.type = OFFLINE_TYPE_EXTENDED;
                in_entry.extended.ext = OFFLINE_EXT_TYPE_FOOTER;
            } else {
                std::stringstream ss;
                ss << "Failed to read from file for thread " << (uint)tids[tidx];
                return ss.str();
            }
        }
        if (in_entry.timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
            VPRINT(2, "Thread %u timestamp 0x" ZHEX64_FORMAT_STRING "\n",
                   (uint)tids[tidx], in_entry.timestamp.usec);
            times[tidx] = in_entry.timestamp.usec;
            tidx = (uint)thread_files.size(); // Request thread scan.
            continue;
        }
        std::string result = append_delayed_branch(tidx);
        if (!result.empty())
            return result;
        bool end_of_record = false;
        RETURN_IF_ERROR(process_offline_entry(&in_entry, tids[tidx], &end_of_record,
                                              &last_bb_handled));
        if (end_of_record)
            tidx = thread_files.size();
    } while (thread_count > 0);
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
    return check_entry_thread_start(&ver_entry);
}

std::string
raw2trace_t::do_conversion()
{
    std::string error = read_and_map_modules();
    if (!error.empty())
        return error;
    trace_entry_t entry;
    entry.type = TRACE_TYPE_HEADER;
    entry.size = 0;
    entry.addr = TRACE_ENTRY_VERSION;
    if (!out_file->write((char *)&entry, sizeof(entry)))
        return "Failed to write header to output file";

    error = merge_and_process_thread_files();
    if (!error.empty())
        return error;

    entry.type = TRACE_TYPE_FOOTER;
    entry.size = 0;
    entry.addr = 0;
    if (!out_file->write((char *)&entry, sizeof(entry)))
        return "Failed to write footer to output file";
    VPRINT(1, "Successfully converted %zu thread files\n", thread_files.size());
    return "";
}

raw2trace_t::raw2trace_t(const char *module_map_in,
                         const std::vector<std::istream *> &thread_files_in,
                         std::ostream *out_file_in, void *dcontext_in,
                         unsigned int verbosity_in)
    : trace_converter_t(dcontext_in == NULL ? dr_standalone_init() : dcontext_in)
    , modmap(module_map_in)
    , thread_files(thread_files_in)
    , out_file(out_file_in)
    , verbosity(verbosity_in)
{
    if (dcontext == NULL) {
#ifdef ARM
        // We keep the mode at ARM and rely on LSB=1 offsets in the modoffs fields
        // to trigger Thumb decoding.
        dr_set_isa_mode(dcontext, DR_ISA_ARM_A32, NULL);
#endif
    }
    // We go ahead and start with a reasonably large capacity.
    hashtable_init_ex(&decode_cache, 16, HASH_INTPTR, false, false, NULL, NULL, NULL);
    // We pay a little memory to get a lower load factor.
    hashtable_config_t config = { sizeof(config), true, 40 };
    hashtable_configure(&decode_cache, &config);

    delayed_branch.resize(thread_files.size());

    pre_read.resize(thread_files.size());
}

raw2trace_t::~raw2trace_t()
{
    module_mapper.reset();
    // XXX: We can't use a free-payload function b/c we can't get the dcontext there,
    // so we have to explicitly free the payloads.
    for (uint i = 0; i < HASHTABLE_SIZE(decode_cache.table_bits); i++) {
        for (hash_entry_t *e = decode_cache.table[i]; e != NULL; e = e->next) {
            delete (static_cast<instr_summary_t *>(e->payload));
        }
    }
    hashtable_delete(&decode_cache);
}
