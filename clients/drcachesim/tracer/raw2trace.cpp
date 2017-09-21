/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

// XXX: DR should export this
#define INVALID_THREAD_ID 0

// Assumes we return an error string by convention.
#define CHECK(val, msg) do { \
    if (!(val)) return msg; \
} while (0)

#define WARN(msg, ...) do {                                  \
    fprintf(stderr, "WARNING: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
} while (0)

#define VPRINT(level, ...) do { \
    if (this->verbosity >= (level)) { \
        fprintf(stderr, "[drmemtrace]: "); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while (0)

#define DO_VERBOSE(level, x) do { \
    if (this->verbosity >= (level)) { \
        x \
    } \
} while (0)

/***************************************************************************
 * Module list
 */

std::string
raw2trace_t::read_and_map_modules(const char *module_map)
{
    // Read and load all of the modules.
    uint num_mods;
    VPRINT(1, "Reading module file from memory\n");
    if (drmodtrack_offline_read(INVALID_FILE, module_map, NULL, &modhandle, &num_mods) !=
        DRCOVLIB_SUCCESS)
        return "Failed to parse module file";
    for (uint i = 0; i < num_mods; i++) {
        drmodtrack_info_t info = {sizeof(info),};
        if (drmodtrack_offline_lookup(modhandle, i, &info) != DRCOVLIB_SUCCESS)
            return "Failed to query module file";
        if (strcmp(info.path, "<unknown>") == 0 ||
            // i#2062: VDSO is hard to decode so for now we treat is as non-module.
            // FIXME: currently we're dropping the ifetch data: we need the tracer
            // to identify it instead of us, which requires drmodtrack changes.
            strcmp(info.path, "[vdso]") == 0) {
            // We won't be able to decode.
            modvec.push_back(module_t(info.path, info.start, NULL, 0));
        } else if (info.containing_index != i) {
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
            byte *base_pc = dr_map_executable_file(info.path, DR_MAPEXE_SKIP_WRITABLE,
                                                   &map_size);
            if (base_pc == NULL) {
                // We expect to fail to map dynamorio.dll for x64 Windows as it
                // is built /fixed.  (We could try to have the map succeed w/o relocs,
                // but we expect to not care enough about code in DR).
                if (strstr(info.path, "dynamorio") != NULL)
                    modvec.push_back(module_t(info.path, info.start, NULL, 0));
                else
                    return "Failed to map module " + std::string(info.path);
            } else {
                VPRINT(1, "Mapped module %d @" PFX " = %s\n", (int)modvec.size(),
                       (ptr_uint_t)base_pc, info.path);
                modvec.push_back(module_t(info.path, info.start, base_pc, map_size));
            }
        }
    }
    VPRINT(1, "Successfully read %d modules\n", num_mods);
    return "";
}

std::string
raw2trace_t::unmap_modules(void)
{
    if (drmodtrack_offline_exit(modhandle) != DRCOVLIB_SUCCESS)
        return "Failed to clean up module table data";
    for (std::vector<module_t>::iterator mvi = modvec.begin();
         mvi != modvec.end(); ++mvi) {
        if (mvi->map_base != NULL && mvi->map_size != 0) {
            bool ok = dr_unmap_executable_file(mvi->map_base, mvi->map_size);
            if (!ok)
                WARN("Failed to unmap module %s", mvi->path);
        }
    }
    return "";
}

/***************************************************************************
 * Disassembly to fill in instr and memref entries
 */

static bool
instr_is_rep_string(instr_t *instr)
{
#ifdef X86
    uint opc = instr_get_opcode(instr);
    return (opc == OP_rep_ins || opc == OP_rep_outs || opc == OP_rep_movs ||
            opc == OP_rep_stos || opc == OP_rep_lods || opc == OP_rep_cmps ||
            opc == OP_repne_cmps || opc == OP_rep_scas || opc == OP_repne_scas);
#else
    return false;
#endif
}

std::string
raw2trace_t::append_memref(INOUT trace_entry_t **buf_in, uint tidx, instr_t *instr,
                           opnd_t ref, bool write)
{
    trace_entry_t *buf = *buf_in;
    offline_entry_t in_entry;
    if (!thread_files[tidx]->read((char*)&in_entry, sizeof(in_entry)))
        return "Trace ends mid-block";
    if (in_entry.addr.type != OFFLINE_TYPE_MEMREF &&
        in_entry.addr.type != OFFLINE_TYPE_MEMREF_HIGH) {
        // This happens when there are predicated memrefs in the bb.
        // They could be earlier, so "instr" may not itself be predicated.
        // XXX i#2015: if there are multiple predicated memrefs, our instr vs
        // data stream may not be in the correct order here.
        VPRINT(4, "Missing memref (next type is 0x" ZHEX64_FORMAT_STRING ")\n",
               in_entry.combined_value);
        // Put back the entry.
        thread_files[tidx]->seekg(-(std::streamoff)sizeof(in_entry),
                                  thread_files[tidx]->cur);
        return "";
    }
    if (instr_is_prefetch(instr)) {
        buf->type = instru_t::instr_to_prefetch_type(instr);
        buf->size = 1;
    } else if (instru_t::instr_is_flush(instr)) {
        buf->type = TRACE_TYPE_DATA_FLUSH;
        buf->size = (ushort) opnd_size_in_bytes(opnd_get_size(ref));
    } else {
        if (write)
            buf->type = TRACE_TYPE_WRITE;
        else
            buf->type = TRACE_TYPE_READ;
        buf->size = (ushort) opnd_size_in_bytes(opnd_get_size(ref));
    }
    // We take the full value, to handle low or high.
    buf->addr = (addr_t) in_entry.combined_value;
    VPRINT(4, "Appended memref to " PFX "\n", (ptr_uint_t)buf->addr);
    *buf_in = ++buf;
    return "";
}

std::string
raw2trace_t::append_bb_entries(uint tidx, offline_entry_t *in_entry, OUT bool *handled)
{
    uint instr_count = in_entry->pc.instr_count;
    instr_t *instr;
    trace_entry_t buf_start[MAX_COMBINED_ENTRIES];
    app_pc start_pc = modvec[in_entry->pc.modidx].map_base + in_entry->pc.modoffs;
    app_pc pc, decode_pc = start_pc;
    if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
        modvec[in_entry->pc.modidx].map_base == NULL) {
        // FIXME i#2062: add support for code not in a module (vsyscall, JIT, etc.).
        // Once that support is in we can remove the bool return value and handle
        // the memrefs up here.
        VPRINT(3, "Skipping ifetch for %u instrs not in a module\n", instr_count);
        *handled = false;
        return "";
    } else {
        VPRINT(3, "Appending %u instrs in bb " PFX " in mod %u +" PIFX " = %s\n",
               instr_count, (ptr_uint_t)start_pc, (uint)in_entry->pc.modidx,
               (ptr_uint_t)in_entry->pc.modoffs, modvec[in_entry->pc.modidx].path);
    }
    bool skip_icache = false;
    if (instr_count == 0) {
        // L0 filtering adds a PC entry with a count of 0 prior to each memref.
        skip_icache = true;
        instr_count = 1;
        // We set a flag to avoid peeking forward on instr entries.
        if (!instrs_are_separate)
            instrs_are_separate = true;
    }
    CHECK(!instrs_are_separate || instr_count == 1, "cannot mix 0-count and >1-count");
    for (uint i = 0; i < instr_count; ++i) {
        trace_entry_t *buf = buf_start;
        app_pc orig_pc = decode_pc - modvec[in_entry->pc.modidx].map_base +
            modvec[in_entry->pc.modidx].orig_base;
        bool skip_instr = false;
        // To avoid repeatedly decoding the same instruction on every one of its
        // dynamic executions, we cache the decoding in a hashtable.
        instr = (instr_t *) hashtable_lookup(&decode_cache, decode_pc);
        if (instr == NULL) {
            instr = instr_create(dcontext);
            // We assume the default ISA mode and currently require the 32-bit
            // postprocessor for 32-bit applications.
            pc = decode(dcontext, decode_pc, instr);
            if (pc == NULL || !instr_valid(instr)) {
                WARN("Encountered invalid/undecodable instr @ %s+" PFX,
                     modvec[in_entry->pc.modidx].path, (ptr_uint_t)in_entry->pc.modoffs);
                break;
            }
            hashtable_add(&decode_cache, decode_pc, instr);
        } else {
            pc = instr_get_raw_bits(instr) + instr_length(dcontext, instr);
        }
        CHECK(!instr_is_cti(instr) || i == instr_count - 1, "invalid cti");
        if (instr_is_rep_string(instr)) {
            // We want it to look like the original rep string instead of the
            // drutil-expanded loop.
            if (!prev_instr_was_rep_string)
                prev_instr_was_rep_string = true;
            else
                skip_instr = true;
        } else
            prev_instr_was_rep_string = false;
        // FIXME i#1729: make bundles via lazy accum until hit memref/end.
        if (!skip_instr) {
            DO_VERBOSE(3, {
                instr_set_translation(instr, orig_pc);
                dr_print_instr(dcontext, STDOUT, instr, "");
            });
            buf->type = instru_t::instr_to_instr_type(instr);
            buf->size = (ushort) (skip_icache ? 0 : instr_length(dcontext, instr));
            buf->addr = (addr_t) orig_pc;
            ++buf;
        } else
            VPRINT(3, "Skipping instr fetch for " PFX "\n", (ptr_uint_t)decode_pc);
        decode_pc = pc;
        // We need to interleave instrs with memrefs.
        // There is no following memref for (instrs_are_separate && !skip_icache).
        if ((!instrs_are_separate || skip_icache) &&
            // Rule out OP_lea.
            (instr_reads_memory(instr) || instr_writes_memory(instr))) {
            for (int i = 0; i < instr_num_srcs(instr); i++) {
                if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                    std::string error = append_memref(&buf, tidx, instr,
                                                      instr_get_src(instr, i), false);
                    if (!error.empty())
                        return error;
                }
            }
            for (int i = 0; i < instr_num_dsts(instr); i++) {
                if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                    std::string error = append_memref(&buf, tidx, instr,
                                                      instr_get_dst(instr, i), true);
                    if (!error.empty())
                        return error;
                }
            }
        }
        CHECK((size_t)(buf - buf_start) < MAX_COMBINED_ENTRIES, "Too many entries");
        if (!out_file->write((char*)buf_start, (buf - buf_start)*sizeof(trace_entry_t)))
            return "Failed to write to output file";
    }
    *handled = true;
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
    uint tidx = (uint)thread_files.size();
    uint thread_count = (uint)thread_files.size();
    offline_entry_t in_entry;
    online_instru_t instru(NULL, false);
    bool last_bb_handled = true;
    std::vector<thread_id_t> tids(thread_files.size(), INVALID_THREAD_ID);
    std::vector<uint64> times(thread_files.size(), 0);
    byte buf_base[MAX_COMBINED_ENTRIES * sizeof(trace_entry_t)];

    // We read the thread files simultaneously in lockstep and merge them into
    // a single output file in timestamp order.
    // When a thread file runs out we leave its times[] entry as 0 and its file at eof.
    // We convert each offline entry into a trace_entry_t.
    // We fill in instr entries and memref type and size.
    do {
        int size = 0;
        byte *buf = buf_base;
        if (tidx >= thread_files.size()) {
            // Pick the next thread by looking for the smallest timestamp.
            uint64 min_time = 0xffffffffffffffff;
            uint next_tidx = 0;
            for (uint i=0; i<times.size(); ++i) {
                if (times[i] == 0 && !thread_files[i]->eof()) {
                    offline_entry_t entry;
                    if (!thread_files[i]->read((char*)&entry, sizeof(entry)))
                        return "Failed to read from input file";
                    if (entry.timestamp.type != OFFLINE_TYPE_TIMESTAMP)
                        return "Missing timestamp entry";
                    times[i] = entry.timestamp.usec;
                    VPRINT(3, "Thread %u timestamp is @0x" ZHEX64_FORMAT_STRING
                           "\n", (uint)tids[i], times[i]);
                }
                if (times[i] != 0 && times[i] < min_time) {
                    min_time = times[i];
                    next_tidx = i;
                }
            }
            VPRINT(2, "Next thread in timestamp order is %u @0x" ZHEX64_FORMAT_STRING
                   "\n", (uint)tids[next_tidx], times[next_tidx]);
            tidx = next_tidx;
            times[tidx] = 0; // Read from file for this thread's next timestamp.
            if (tids[tidx] != INVALID_THREAD_ID) {
                // The initial read from a file may not have seen its tid entry
                // yet.  We expect to hit that entry next.
                size += instru.append_tid(buf, tids[tidx]);
            }
            if (size > 0) {
                // We have to write this now before we append any bb entries.
                CHECK((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
                if (!out_file->write((char*)buf_base, size))
                    return "Failed to write to output file";
                buf = buf_base;
            }
            size = 0;
        }
        VPRINT(4, "About to read thread %d at pos %d\n",
               (uint)tids[tidx], (int)thread_files[tidx]->tellg());
        if (!thread_files[tidx]->read((char*)&in_entry, sizeof(in_entry))) {
            if (thread_files[tidx]->eof()) {
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
        if (in_entry.extended.type == OFFLINE_TYPE_EXTENDED) {
            if (in_entry.extended.ext == OFFLINE_EXT_TYPE_FOOTER) {
                // Push forward to EOF.
                offline_entry_t entry;
                if (thread_files[tidx]->read((char*)&entry, sizeof(entry)) ||
                    !thread_files[tidx]->eof())
                    return "Footer is not the final entry";
                CHECK(tids[tidx] != INVALID_THREAD_ID, "Missing thread id");
                VPRINT(2, "Thread %d exit\n", (uint)tids[tidx]);
                size += instru.append_thread_exit(buf, tids[tidx]);
                buf += size;
                --thread_count;
                tidx = (uint)thread_files.size(); // Request thread scan.
            } else {
                std::stringstream ss;
                ss << "Invalid extension type " << (int)in_entry.extended.ext;
                return ss.str();
            }
        } else if (in_entry.timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
            VPRINT(2, "Thread %u timestamp 0x" ZHEX64_FORMAT_STRING "\n",
                   (uint)tids[tidx], in_entry.timestamp.usec);
            times[tidx] = in_entry.timestamp.usec;
            tidx = (uint)thread_files.size(); // Request thread scan.
        } else if (in_entry.addr.type == OFFLINE_TYPE_MEMREF ||
                   in_entry.addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
            if (!last_bb_handled) {
                // For currently-unhandled non-module code, memrefs are handled here
                // where we can easily handle the transition out of the bb.
                trace_entry_t *entry = (trace_entry_t *) buf;
                entry->type = TRACE_TYPE_READ; // Guess.
                entry->size = 1; // Guess.
                entry->addr = (addr_t) in_entry.combined_value;
                VPRINT(4, "Appended non-module memref to " PFX "\n",
                       (ptr_uint_t)entry->addr);
                size += sizeof(*entry);
                buf += size;
            } else {
                // We should see an instr entry first
                return "memref entry found outside of bb";
            }
        } else if (in_entry.pc.type == OFFLINE_TYPE_PC) {
          std::string result = append_bb_entries(tidx, &in_entry, &last_bb_handled);
          if (!result.empty())
              return result;
        } else if (in_entry.tid.type == OFFLINE_TYPE_THREAD) {
            VPRINT(2, "Thread %u entry\n", (uint)in_entry.tid.tid);
            if (tids[tidx] == INVALID_THREAD_ID)
                tids[tidx] = in_entry.tid.tid;
            size += instru.append_tid(buf, in_entry.tid.tid);
            buf += size;
        } else if (in_entry.pid.type == OFFLINE_TYPE_PID) {
            VPRINT(2, "Process %u entry\n", (uint)in_entry.pid.pid);
            size += instru.append_pid(buf, in_entry.pid.pid);
            buf += size;
        } else if (in_entry.addr.type == OFFLINE_TYPE_IFLUSH) {
            offline_entry_t entry;
            if (!thread_files[tidx]->read((char*)&entry, sizeof(entry)) ||
                entry.addr.type != OFFLINE_TYPE_IFLUSH)
                return "Flush missing 2nd entry";
            VPRINT(2, "Flush " PFX"-" PFX"\n", (ptr_uint_t)in_entry.addr.addr,
                   (ptr_uint_t)entry.addr.addr);
            size += instru.append_iflush(buf, in_entry.addr.addr,
                                         (size_t)(entry.addr.addr - in_entry.addr.addr));
            buf += size;
        } else {
            std::stringstream ss;
            ss << "Unknown trace type " << (int)in_entry.timestamp.type;
            return ss.str();
        }
        if (size > 0) {
            CHECK((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
            if (!out_file->write((char*)buf_base, size))
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
    if (!f->read((char*)&ver_entry, sizeof(ver_entry))) {
        return "Unable to read thread log file";
    }
    if (ver_entry.extended.type != OFFLINE_TYPE_EXTENDED ||
        ver_entry.extended.ext != OFFLINE_EXT_TYPE_HEADER) {
        return "Thread log file is corrupted: missing version entry";
    }
    if (ver_entry.extended.value != OFFLINE_FILE_VERSION) {
        std::stringstream ss;
        ss << "Version mismatch: expect " << OFFLINE_FILE_VERSION << " vs "
           << (int)ver_entry.extended.value;
        return ss.str();
    }
    return "";
}

std::string
raw2trace_t::do_conversion()
{
    std::string error = read_and_map_modules(modmap);
    if (!error.empty())
        return error;
    trace_entry_t entry;
    entry.type = TRACE_TYPE_HEADER;
    entry.size = 0;
    entry.addr = TRACE_ENTRY_VERSION;
    if (!out_file->write((char*)&entry, sizeof(entry)))
        return "Failed to write header to output file";

    error = merge_and_process_thread_files();
    if (!error.empty())
        return error;

    entry.type = TRACE_TYPE_FOOTER;
    entry.size = 0;
    entry.addr = 0;
    if (!out_file->write((char*)&entry, sizeof(entry)))
        return "Failed to write footer to output file";
    VPRINT(1, "Successfully converted %zu thread files\n", thread_files.size());
    return "";
}

raw2trace_t::raw2trace_t(const char *module_map_in,
                         const std::vector<std::istream*> &thread_files_in,
                         std::ostream *out_file_in,
                         void *dcontext_in,
                         unsigned int verbosity_in)
    : modmap(module_map_in), thread_files(thread_files_in), out_file(out_file_in),
      dcontext(dcontext_in), prev_instr_was_rep_string(false), instrs_are_separate(false),
      verbosity(verbosity_in)
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
    hashtable_config_t config = {sizeof(config), true, 40};
    hashtable_configure(&decode_cache, &config);
}

raw2trace_t::~raw2trace_t()
{
    unmap_modules();
    // XXX: We can't use a free-payload function b/c we can't get the dcontext there,
    // so we have to explicitly free the payloads.
    for (uint i = 0; i < HASHTABLE_SIZE(decode_cache.table_bits); i++) {
        for (hash_entry_t *e = decode_cache.table[i]; e != NULL; e = e->next) {
            instr_destroy(dcontext, (instr_t *)e->payload);
        }
    }
    hashtable_delete(&decode_cache);
}
