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
    if (!module_mapper) {
        module_mapper.reset(new module_mapper_t(modmap, user_parse, user_process,
                                                user_process_data, user_free, verbosity));
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

    modvec_ptr = &module_mapper->get_loaded_modules();
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
module_mapper_t::find_mapped_trace_address(app_pc trace_address)
{
    if (modhandle == nullptr || modlist.empty()) {
        last_error = "Failed to call get_module_list() first";
        return nullptr;
    }

    // For simplicity we do a linear search, caching the prior hit.
    if (trace_address >= last_orig_base &&
        trace_address < last_orig_base + last_map_size) {
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
            return mapped_address;
        }
    }
    last_error = "Trace address not found";
    return nullptr;
}

/***************************************************************************
 * Disassembly to fill in instr and memref entries
 */

#define FAULT_INTERRUPTED_BB "INTERRUPTED"

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

// Returns FAULT_INTERRUPTED_BB if a fault occurred on this memref.
// Any other non-empty string is a fatal error.
std::string
raw2trace_t::append_memref(INOUT trace_entry_t **buf_in, uint tidx,
                           const instr_summary_t *instr, opnd_t ref, bool write)
{
    trace_entry_t *buf = *buf_in;
    offline_entry_t in_entry;
    bool have_type = false;
    if (!read_from_thread_file(tidx, &in_entry, 1))
        return "Trace ends mid-block";
    if (in_entry.extended.type == OFFLINE_TYPE_EXTENDED &&
        in_entry.extended.ext == OFFLINE_EXT_TYPE_MEMINFO) {
        // For -L0_filter we have to store the type for multi-memref instrs where
        // we can't tell which memref it is (we'll still come here for the subsequent
        // memref operands but we'll exit early in the check below).
        have_type = true;
        buf->type = in_entry.extended.valueB;
        buf->size = in_entry.extended.valueA;
        VPRINT(4, "Found type entry type %d size %d\n", buf->type, buf->size);
        if (!read_from_thread_file(tidx, &in_entry, 1))
            return "Trace ends mid-block";
    }
    if (in_entry.addr.type != OFFLINE_TYPE_MEMREF &&
        in_entry.addr.type != OFFLINE_TYPE_MEMREF_HIGH) {
        // This happens when there are predicated memrefs in the bb, or for a
        // zero-iter rep string loop, or for a multi-memref instr with -L0_filter.
        // For predicated memrefs, they could be earlier, so "instr"
        // may not itself be predicated.
        // XXX i#2015: if there are multiple predicated memrefs, our instr vs
        // data stream may not be in the correct order here.
        VPRINT(4,
               "Missing memref from predication, 0-iter repstr, or filter "
               "(next type is 0x" ZHEX64_FORMAT_STRING ")\n",
               in_entry.combined_value);
        unread_from_thread_file(tidx, &in_entry, 1);
        return "";
    }
    if (!have_type) {
        if (instr->is_prefetch()) {
            buf->type = instr->prefetch_type();
            buf->size = 1;
        } else if (instr->is_flush()) {
            buf->type = TRACE_TYPE_DATA_FLUSH;
            buf->size = (ushort)opnd_size_in_bytes(opnd_get_size(ref));
        } else {
            if (write)
                buf->type = TRACE_TYPE_WRITE;
            else
                buf->type = TRACE_TYPE_READ;
            buf->size = (ushort)opnd_size_in_bytes(opnd_get_size(ref));
        }
    }
    // We take the full value, to handle low or high.
    buf->addr = (addr_t)in_entry.combined_value;
#ifdef X86
    if (opnd_is_near_base_disp(ref) && opnd_get_base(ref) != DR_REG_NULL &&
        opnd_get_index(ref) == DR_REG_NULL) {
        // We stored only the base reg, as an optimization.
        buf->addr += opnd_get_disp(ref);
    }
#endif
    VPRINT(4, "Appended memref type %d size %d to " PFX "\n", buf->type, buf->size,
           (ptr_uint_t)buf->addr);
    *buf_in = ++buf;
    // To avoid having to backtrack later, we read ahead to see whether this memref
    // faulted.  There's a footer so this should always succeed.
    if (!read_from_thread_file(tidx, &in_entry, 1))
        return "Trace ends mid-block";
    // Put it back.
    unread_from_thread_file(tidx, &in_entry, 1);
    if (in_entry.extended.type == OFFLINE_TYPE_EXTENDED &&
        in_entry.extended.ext == OFFLINE_EXT_TYPE_MARKER &&
        in_entry.extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT) {
        // A signal/exception interrupted the bb after the memref.
        VPRINT(4, "Signal/exception interrupted the bb\n");
        return FAULT_INTERRUPTED_BB;
    }
    return "";
}

std::string
raw2trace_t::append_bb_entries(uint tidx, offline_entry_t *in_entry, OUT bool *handled)
{
    uint instr_count = in_entry->pc.instr_count;
    const instr_summary_t *instr;
    trace_entry_t buf_start[MAX_COMBINED_ENTRIES];
    app_pc start_pc = modvec()[in_entry->pc.modidx].map_base + in_entry->pc.modoffs;
    app_pc pc, decode_pc = start_pc;
    if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
        modvec()[in_entry->pc.modidx].map_base == NULL) {
        // FIXME i#2062: add support for code not in a module (vsyscall, JIT, etc.).
        // Once that support is in we can remove the bool return value and handle
        // the memrefs up here.
        VPRINT(3, "Skipping ifetch for %u instrs not in a module\n", instr_count);
        *handled = false;
        return "";
    } else {
        VPRINT(3, "Appending %u instrs in bb " PFX " in mod %u +" PIFX " = %s\n",
               instr_count, (ptr_uint_t)start_pc, (uint)in_entry->pc.modidx,
               (ptr_uint_t)in_entry->pc.modoffs, modvec()[in_entry->pc.modidx].path);
    }
    bool skip_icache = false;
    bool truncated = false; // Whether a fault ended the bb early.
    if (instr_count == 0) {
        // L0 filtering adds a PC entry with a count of 0 prior to each memref.
        skip_icache = true;
        instr_count = 1;
        // We set a flag to avoid peeking forward on instr entries.
        if (!instrs_are_separate)
            instrs_are_separate = true;
    }
    CHECK(!instrs_are_separate || instr_count == 1, "cannot mix 0-count and >1-count");
    for (uint i = 0; !truncated && i < instr_count; ++i) {
        trace_entry_t *buf = buf_start;
        app_pc orig_pc = decode_pc - modvec()[in_entry->pc.modidx].map_base +
            modvec()[in_entry->pc.modidx].orig_base;
        // To avoid repeatedly decoding the same instruction on every one of its
        // dynamic executions, we cache the decoding in a hashtable.
        pc = decode_pc;
        instr =
            get_instr_summary(in_entry->pc.modidx, in_entry->pc.modoffs, &pc, orig_pc);
        if (instr == nullptr) {
            // We hit some error somewhere, and already reported it. Just exit the loop.
            break;
        }
        CHECK(!instr->is_cti() || i == instr_count - 1, "invalid cti");
        // FIXME i#1729: make bundles via lazy accum until hit memref/end.
        buf->type = instr->type();
        if (buf->type == TRACE_TYPE_INSTR_MAYBE_FETCH) {
            // We want it to look like the original rep string, with just one instr
            // fetch for the whole loop, instead of the drutil-expanded loop.
            // We fix up the maybe-fetch here so our offline file doesn't have to
            // rely on our own reader.
            if (!prev_instr_was_rep_string) {
                prev_instr_was_rep_string = true;
                buf->type = TRACE_TYPE_INSTR;
            } else {
                VPRINT(3, "Skipping instr fetch for " PFX "\n", (ptr_uint_t)decode_pc);
                // We still include the instr to make it easier for core simulators
                // (i#2051).
                buf->type = TRACE_TYPE_INSTR_NO_FETCH;
            }
        } else
            prev_instr_was_rep_string = false;
        buf->size = (ushort)(skip_icache ? 0 : instr->length());
        buf->addr = (addr_t)orig_pc;
        ++buf;
        decode_pc = pc;
        // We need to interleave instrs with memrefs.
        // There is no following memref for (instrs_are_separate && !skip_icache).
        if ((!instrs_are_separate || skip_icache) &&
            // Rule out OP_lea.
            (instr->reads_memory() || instr->writes_memory())) {
            for (uint j = 0; j < instr->num_mem_srcs(); j++) {
                std::string error =
                    append_memref(&buf, tidx, instr, instr->mem_src_at(j), false);
                if (error == FAULT_INTERRUPTED_BB) {
                    truncated = true;
                    break;
                } else if (!error.empty())
                    return error;
            }
            for (uint j = 0; !truncated && j < instr->num_mem_dests(); j++) {
                std::string error =
                    append_memref(&buf, tidx, instr, instr->mem_dest_at(j), true);
                if (error == FAULT_INTERRUPTED_BB) {
                    truncated = true;
                    break;
                } else if (!error.empty())
                    return error;
            }
        }
        CHECK((size_t)(buf - buf_start) < MAX_COMBINED_ENTRIES, "Too many entries");
        if (instr->is_cti()) {
            CHECK(delayed_branch[tidx].empty(), "Failed to flush delayed branch");
            // In case this is the last branch prior to a thread switch, buffer it.  We
            // avoid swapping threads immediately after a branch so that analyzers can
            // more easily find the branch target.  Doing this in the tracer would incur
            // extra overhead, and in the reader would be more complex and messy than
            // here (and we are ok bailing on doing this for online traces), so we
            // handle it in post-processing by delaying a thread-block-final branch (and
            // its memrefs) to that thread's next block.  This changes the timestamp
            // of the branch, which we live with.
            delayed_branch[tidx].insert(delayed_branch[tidx].begin(), (char *)buf_start,
                                        (char *)buf);
        } else {
            if (!out_file->write((char *)buf_start,
                                 (buf - buf_start) * sizeof(trace_entry_t)))
                return "Failed to write to output file";
        }
    }
    *handled = true;
    return "";
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
    uint tidx = (uint)thread_files.size();
    uint thread_count = (uint)thread_files.size();
    offline_entry_t in_entry;
    bool last_bb_handled = true;
    size_t size;
    std::vector<thread_id_t> tids(thread_files.size(), INVALID_THREAD_ID);
    std::vector<process_id_t> pids(thread_files.size(), (process_id_t)INVALID_PROCESS_ID);
    std::vector<bool> wrote_pid(thread_files.size(), false);
    std::vector<uint64> times(thread_files.size(), 0);
    byte buf_base[MAX_COMBINED_ENTRIES * sizeof(trace_entry_t)];

    // First read the tid and pid entries which precede any timestamps.
    // We append the tid to the output on every thread switch, and the pid
    // the very first time (using wrote_pid[]).
    for (uint i = 0; i < thread_files.size(); ++i) {
        if (!read_from_thread_file(i, &in_entry, 1))
            return "Failed to read header from input file";
        // Handle legacy traces which have the timestamp first.
        if (in_entry.tid.type == OFFLINE_TYPE_TIMESTAMP) {
            times[i] = in_entry.timestamp.usec;
            if (!read_from_thread_file(i, &in_entry, 1))
                return "Failed to read header from input file";
        }
        DR_ASSERT(in_entry.tid.type == OFFLINE_TYPE_THREAD);
        VPRINT(2, "File %u is thread %u\n", i, (uint)in_entry.tid.tid);
        tids[i] = in_entry.tid.tid;
        if (!read_from_thread_file(i, &in_entry, 1))
            return "Failed to read header from input file";
        DR_ASSERT(in_entry.pid.type == OFFLINE_TYPE_PID);
        VPRINT(2, "File %u is process %u\n", i, (uint)in_entry.pid.pid);
        pids[i] = in_entry.pid.pid;
    }

    // We read the thread files simultaneously in lockstep and merge them into
    // a single output file in timestamp order.
    // When a thread file runs out we leave its times[] entry as 0 and its file at eof.
    // We convert each offline entry into a trace_entry_t.
    // We fill in instr entries and memref type and size.
    do {
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
            buf += trace_metadata_writer_t::write_tid(buf, tids[tidx]);
            if (!wrote_pid[tidx]) {
                DR_ASSERT(pids[tidx] != (process_id_t)INVALID_PROCESS_ID);
                buf += trace_metadata_writer_t::write_pid(buf, pids[tidx]);
                wrote_pid[tidx] = true;
            }
            buf += trace_metadata_writer_t::write_timestamp(buf, (uintptr_t)times[tidx]);
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
        if (in_entry.extended.type == OFFLINE_TYPE_EXTENDED) {
            if (in_entry.extended.ext == OFFLINE_EXT_TYPE_FOOTER) {
                // Push forward to EOF.
                offline_entry_t entry;
                if (read_from_thread_file(tidx, &entry, 1) || !thread_file_at_eof(tidx))
                    return "Footer is not the final entry";
                CHECK(tids[tidx] != INVALID_THREAD_ID, "Missing thread id");
                VPRINT(2, "Thread %d exit\n", (uint)tids[tidx]);
                buf += trace_metadata_writer_t::write_thread_exit(buf, tids[tidx]);
                --thread_count;
                tidx = (uint)thread_files.size(); // Request thread scan.
            } else if (in_entry.extended.ext == OFFLINE_EXT_TYPE_MARKER) {
                buf += trace_metadata_writer_t::write_marker(
                    buf, (trace_marker_type_t)in_entry.extended.valueB,
                    (uintptr_t)in_entry.extended.valueA);
                VPRINT(3, "Appended marker type %u value %zu\n",
                       (trace_marker_type_t)in_entry.extended.valueB,
                       (uintptr_t)in_entry.extended.valueA);
            } else {
                std::stringstream ss;
                ss << "Invalid extension type " << (int)in_entry.extended.ext;
                return ss.str();
            }
        } else if (in_entry.addr.type == OFFLINE_TYPE_MEMREF ||
                   in_entry.addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
            if (!last_bb_handled) {
                // For currently-unhandled non-module code, memrefs are handled here
                // where we can easily handle the transition out of the bb.
                trace_entry_t *entry = (trace_entry_t *)buf;
                entry->type = TRACE_TYPE_READ; // Guess.
                entry->size = 1;               // Guess.
                entry->addr = (addr_t)in_entry.combined_value;
                VPRINT(4, "Appended non-module memref to " PFX "\n",
                       (ptr_uint_t)entry->addr);
                buf += sizeof(*entry);
            } else {
                // We should see an instr entry first
                return "memref entry found outside of bb";
            }
        } else if (in_entry.pc.type == OFFLINE_TYPE_PC) {
            result = append_bb_entries(tidx, &in_entry, &last_bb_handled);
            if (!result.empty())
                return result;
        } else if (in_entry.addr.type == OFFLINE_TYPE_IFLUSH) {
            offline_entry_t entry;
            if (!read_from_thread_file(tidx, &entry, 1) ||
                entry.addr.type != OFFLINE_TYPE_IFLUSH)
                return "Flush missing 2nd entry";
            VPRINT(2, "Flush " PFX "-" PFX "\n", (ptr_uint_t)in_entry.addr.addr,
                   (ptr_uint_t)entry.addr.addr);
            buf += trace_metadata_writer_t::write_iflush(
                buf, in_entry.addr.addr, (size_t)(entry.addr.addr - in_entry.addr.addr));
        } else {
            std::stringstream ss;
            ss << "Unknown trace type " << (int)in_entry.timestamp.type;
            return ss.str();
        }
        if (buf > buf_base) {
            size_t size = buf - buf_base;
            CHECK((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
            if (!out_file->write((char *)buf_base, size))
                return "Failed to write to output file";
        }
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
    return trace_metadata_reader_t::check_entry_thread_start(&ver_entry);
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

const instr_summary_t *
raw2trace_t::get_instr_summary(uint64 modidx, uint64 modoffs, INOUT app_pc *pc,
                               app_pc orig)
{
    const app_pc decode_pc = *pc;
    const instr_summary_t *ret =
        static_cast<const instr_summary_t *>(hashtable_lookup(&decode_cache, decode_pc));
    if (ret == nullptr) {
        instr_summary_t *desc = new instr_summary_t();
        if (!instr_summary_t::construct(dcontext, pc, orig, desc, verbosity)) {
            WARN("Encountered invalid/undecodable instr @ %s+" PFX,
                 modvec()[static_cast<size_t>(modidx)].path, (ptr_uint_t)modoffs);
            return nullptr;
        }
        hashtable_add(&decode_cache, decode_pc, desc);
        ret = desc;
    } else {
        /* XXX i#3129: Log some rendering of the instruction summary that will be
         * returned.
         */
        *pc = ret->next_pc();
    }
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

raw2trace_t::raw2trace_t(const char *module_map_in,
                         const std::vector<std::istream *> &thread_files_in,
                         std::ostream *out_file_in, void *dcontext_in,
                         unsigned int verbosity_in)
    : modmap(module_map_in)
    , modvec_ptr(nullptr)
    , thread_files(thread_files_in)
    , out_file(out_file_in)
    , dcontext(dcontext_in)
    , prev_instr_was_rep_string(false)
    , instrs_are_separate(false)
    , verbosity(verbosity_in)
    , user_process(nullptr)
    , user_process_data(nullptr)
{
    if (dcontext == NULL) {
        dcontext = dr_standalone_init();
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
