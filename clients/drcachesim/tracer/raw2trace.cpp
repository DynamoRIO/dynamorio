/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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

#define UNICODE

#include "dr_api.h"
#include "droption.h"
#include "drcovlib.h"
#include "dr_frontend.h"
#include "raw2trace.h"
#include "instru.h"
#include "../common/memref.h"
#include "../common/trace_entry.h"
#include <fstream>
#include <vector>

#ifdef UNIX
# include <dirent.h> /* opendir, readdir */
# include <unistd.h> /* getcwd */
#else
# include <windows.h>
# include <direct.h> /* _getcwd */
# pragma comment(lib, "User32.lib")
#endif

// XXX: currently the error handling and diagnostics are *not* modular:
// this class assumes an option op_verbose, and for errors it just
// prints directly and exits the process.
// The original plan was to have drcachesim launch this as a separate
// executable.
extern droption_t<unsigned int> op_verbose;

// XXX: DR should export this
#define INVALID_THREAD_ID 0

#define FATAL_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

#define CHECK(val, msg, ...) do { \
    if (!(val)) FATAL_ERROR(msg, ##__VA_ARGS__); \
} while (0)

#define WARN(msg, ...) do {                                  \
    fprintf(stderr, "WARNING: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
} while (0)

#define VPRINT(level, ...) do { \
    if (op_verbose.get_value() >= level) { \
        fprintf(stderr, "[drmemtrace]: "); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while (0)

#define DO_VERBOSE(level, x) do { \
    if (op_verbose.get_value() >= level) { \
        x \
    } \
} while (0)

/***************************************************************************
 * Module list
 */

void
raw2trace_t::read_and_map_modules(void)
{
    // Read and load all of the modules.
    std::string modfilename = indir + std::string(DIRSEP) + MODULE_LIST_FILENAME;
    uint num_mods;
    VPRINT(1, "Reading module file %s\n", modfilename.c_str());
    file_t modfile = dr_open_file(modfilename.c_str(), DR_FILE_READ);
    if (modfile == INVALID_FILE)
        FATAL_ERROR("Failed to open module file %s", modfilename.c_str());
    if (drmodtrack_offline_read(modfile, NULL, &modhandle, &num_mods) !=
        DRCOVLIB_SUCCESS)
        FATAL_ERROR("Failed to parse module file %s", modfilename.c_str());
    for (uint i = 0; i < num_mods; i++) {
        app_pc modbase;
        size_t modsize;
        const char *path;
        if (drmodtrack_offline_lookup(modhandle, i, &modbase, &modsize, &path) !=
            DRCOVLIB_SUCCESS)
            FATAL_ERROR("Failed to query module file");
        if (strcmp(path, "<unknown>") == 0) {
            // We won't be able to decode.
            modvec.push_back(module_t(path, modbase, NULL, 0));
        } else {
            size_t map_size;
            byte *base_pc = dr_map_executable_file(path, DR_MAPEXE_SKIP_WRITABLE,
                                                   &map_size);
            if (base_pc == NULL) {
                // We expect to fail to map dynamorio.dll for x64 Windows as it
                // is built /fixed.  (We could try to have the map succeed w/o relocs,
                // but we expect to not care enough about code in DR).
                if (strstr(path, "dynamorio") != NULL)
                    modvec.push_back(module_t(path, modbase, NULL, 0));
                else
                    FATAL_ERROR("Failed to map module %s", path);
            } else {
                VPRINT(1, "Mapped module %d @"PFX" = %s\n", (int)modvec.size(),
                       (ptr_uint_t)base_pc, path);
                modvec.push_back(module_t(path, modbase, base_pc, map_size));
            }
        }
    }
    VPRINT(1, "Successfully read %d modules\n", num_mods);
}

void
raw2trace_t::unmap_modules(void)
{
    if (drmodtrack_offline_exit(modhandle) != DRCOVLIB_SUCCESS)
        FATAL_ERROR("Failed to clean up module table data");
    for (std::vector<module_t>::iterator mvi = modvec.begin();
         mvi != modvec.end(); ++mvi) {
        if (mvi->map_base != NULL) {
            bool ok = dr_unmap_executable_file(mvi->map_base, mvi->map_size);
            if (!ok)
                WARN("Failed to unmap module %s", mvi->path);
        }
    }
}

/***************************************************************************
 * Directory iterator
 */

// We open each thread log file in a vector so we can read from them simultaneously.
void
raw2trace_t::open_thread_log_file(const char *basename)
{
    char path[MAXIMUM_PATH];
    CHECK(basename[0] != '/',
          "dir iterator entry %s should not be an absolute path\n", basename);
    // Skip the module list log.
    if (strcmp(basename, MODULE_LIST_FILENAME) == 0)
        return;
    // Skip any non-.raw in case someone put some other file in there.
    if (strstr(basename, OUTFILE_SUFFIX) == NULL)
        return;
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s",
                    indir.c_str(), DIRSEP, basename) <= 0) {
        FATAL_ERROR("Failed to get full path of file %s", basename);
    }
    NULL_TERMINATE_BUFFER(path);
    thread_files.push_back(new std::ifstream(path, std::ifstream::binary));
    if (!(*thread_files.back()))
        FATAL_ERROR("Failed to open thread log file %s", path);
    VPRINT(1, "Opened thread log file %s\n", path);
}

#ifdef UNIX
void
raw2trace_t::open_thread_files()
{
    struct dirent *ent;
    DIR *dir = opendir(indir.c_str());
    VPRINT(1, "Iterating dir %s\n", indir.c_str());
    if (dir == NULL)
        FATAL_ERROR("Failed to list directory %s", indir.c_str());
    while ((ent = readdir(dir)) != NULL)
        open_thread_log_file(ent->d_name);
    closedir (dir);
}
#else
void
raw2trace_t::open_thread_files()
{
    HANDLE find = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;
    char path[MAXIMUM_PATH];
    TCHAR wpath[MAXIMUM_PATH];
    VPRINT(1, "Iterating dir %s\n", indir.c_str());
    // Append \*
    dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s\\*", indir.c_str());
    NULL_TERMINATE_BUFFER(path);
    if (drfront_char_to_tchar(path, wpath, BUFFER_SIZE_ELEMENTS(wpath)) !=
        DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to convert from utf-8 to utf-16");
    find = FindFirstFileW(wpath, &data);
    if (find == INVALID_HANDLE_VALUE)
        FATAL_ERROR("Failed to list directory %s\n", indir.c_str());
    do {
        if (!TESTANY(data.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)) {
            if (drfront_tchar_to_char(data.cFileName, path, BUFFER_SIZE_ELEMENTS(path)) !=
                DRFRONT_SUCCESS)
                FATAL_ERROR("Failed to convert from utf-16 to utf-8");
            open_thread_log_file(path);
        }
    } while (FindNextFile(find, &data) != 0);
    FindClose(find);
}
#endif

/***************************************************************************
 * Disassembly to fill in instr and memref entries
 */

trace_entry_t *
raw2trace_t::append_memref(trace_entry_t *buf_in, uint tidx, instr_t *instr,
                           opnd_t ref, bool write)
{
    trace_entry_t *buf = buf_in;
    offline_entry_t in_entry;
    if (!thread_files[tidx]->read((char*)&in_entry, sizeof(in_entry)))
        FATAL_ERROR("Trace ends mid-block");
    if (in_entry.addr.type != OFFLINE_TYPE_MEMREF &&
        in_entry.addr.type != OFFLINE_TYPE_MEMREF_HIGH) {
        // XXX: if there are multiple predicated memrefs, we may not be able to tell
        // which one(s) executed.
        VPRINT(3, "Missing memref (next type is 0x"ZHEX64_FORMAT_STRING")\n",
               in_entry.combined_value);
        CHECK(instr_get_predicate(instr) != DR_PRED_NONE, "missing memref entry");
        return buf;
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
    VPRINT(3, "Appended memref to "PFX"\n", (ptr_uint_t)buf->addr);
    ++buf;
    return buf;
}

bool
raw2trace_t::append_bb_entries(uint tidx, offline_entry_t *in_entry)
{
    uint instr_count = in_entry->pc.instr_count;
    instr_t instr;
    trace_entry_t buf_start[MAX_COMBINED_ENTRIES];
    app_pc start_pc = modvec[in_entry->pc.modidx].map_base + in_entry->pc.modoffs;
    app_pc pc, decode_pc = start_pc;
    if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
        modvec[in_entry->pc.modidx].map_base == NULL) {
        // FIXME i#1729: add support for code not in a module (vsyscall, JIT, etc.).
        // Once that support is in we can remove the bool return value and handle
        // the memrefs up here.
        VPRINT(2, "Skipping ifetch for %u instrs not in a module\n", instr_count);
        return false;
    } else {
        VPRINT(2, "Appending %u instrs in bb "PFX" in mod %u +"PIFX" = %s\n",
               instr_count, (ptr_uint_t)start_pc, (uint)in_entry->pc.modidx,
               (ptr_uint_t)in_entry->pc.modoffs, modvec[in_entry->pc.modidx].path);
    }
    instr_init(dcontext, &instr);
    for (uint i = 0; i < instr_count; ++i) {
        trace_entry_t *buf = buf_start;
        app_pc orig_pc = decode_pc - modvec[in_entry->pc.modidx].map_base +
            modvec[in_entry->pc.modidx].orig_base;
        instr_reset(dcontext, &instr);
        // We assume the default ISA mode and currently require the 32-bit
        // postprocessor for 32-bit applications.
        pc = decode(dcontext, decode_pc, &instr);
        DO_VERBOSE(3, {
            instr_set_translation(&instr, orig_pc);
            dr_print_instr(dcontext, STDOUT, &instr, "");
        });
        if (pc == NULL || !instr_valid(&instr)) {
            WARN("Encountered invalid/undecodable instr @ %s+"PFX,
                 modvec[in_entry->pc.modidx].path, (ptr_uint_t)in_entry->pc.modoffs);
            break;
        }
        CHECK(!instr_is_cti(&instr) || i == instr_count - 1, "invalid cti");
        // FIXME i#1729: make bundles via lazy accum until hit memref/end.
        buf->type = TRACE_TYPE_INSTR;
        buf->size = (ushort) instr_length(dcontext, &instr);
        buf->addr = (addr_t) orig_pc;
        ++buf;
        decode_pc = pc;
        // We need to interleave instrs with memrefs.
        if (instr_reads_memory(&instr) || instr_writes_memory(&instr)) { // Check OP_lea.
            for (int i = 0; i < instr_num_srcs(&instr); i++) {
                if (opnd_is_memory_reference(instr_get_src(&instr, i))) {
                    buf = append_memref(buf, tidx, &instr, instr_get_src(&instr, i),
                                        false);
                }
            }
            for (int i = 0; i < instr_num_dsts(&instr); i++) {
                if (opnd_is_memory_reference(instr_get_dst(&instr, i))) {
                    buf = append_memref(buf, tidx, &instr, instr_get_dst(&instr, i),
                                        true);
                }
            }
        }
        CHECK((size_t)(buf - buf_start) < MAX_COMBINED_ENTRIES, "Too many entries");
        if (!out_file.write((char*)buf_start, (buf - buf_start)*sizeof(trace_entry_t)))
            FATAL_ERROR("Failed to write to output file");
    }
    instr_free(dcontext, &instr);
    return true;
}

/***************************************************************************
 * Top-level
 */

void
raw2trace_t::merge_and_process_thread_files()
{
    uint tidx = 0;
    uint thread_count = (uint)thread_files.size();
    offline_entry_t in_entry;
    online_instru_t instru(NULL);
    bool last_bb_handled = true;
    std::vector<thread_id_t> tids(thread_files.size(), INVALID_THREAD_ID);
    byte buf[MAX_COMBINED_ENTRIES * sizeof(trace_entry_t)];

    // We read the thread files simultaneously in lockstep and merge them into
    // a single output file in timestamp order.
    // We convert each offline entry into a trace_entry_t.
    // We fill in instr entries and memref type and size.
    do {
        int size = 0;
        if (!thread_files[tidx]->read((char*)&in_entry, sizeof(in_entry))) {
            if (thread_files[tidx]->eof()) {
                CHECK(tids[tidx] != INVALID_THREAD_ID, "Missing thread id");
                VPRINT(2, "Thread %d exit\n", (int)tids[tidx]);
                size = instru.append_thread_exit(buf, tids[tidx]);
                --thread_count;
                if (thread_count == 0)
                    break;
                // FIXME i#1729: pick new thread based on timestamps
            } else
                FATAL_ERROR("Failed to read from input file");
        }
        if (in_entry.timestamp.type == OFFLINE_TYPE_TIMESTAMP) {
            // FIXME i#1729: pick new thread based on timestamps
        } else if (in_entry.addr.type == OFFLINE_TYPE_MEMREF ||
                   in_entry.addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
            if (!last_bb_handled) {
                // For currently-unhandled non-module code, memrefs are handled here
                // where we can easily handle the transition out of the bb.
                trace_entry_t *entry = (trace_entry_t *) buf;
                entry->type = TRACE_TYPE_READ; // Guess.
                entry->size = 1; // Guess.
                entry->addr = (addr_t) in_entry.combined_value;
                VPRINT(3, "Appended non-module memref to "PFX"\n",
                       (ptr_uint_t)entry->addr);
                size = sizeof(*entry);
            } else {
                // We should see an instr entry first
                CHECK(false, "memref entry found outside of bb");
            }
        } else if (in_entry.pc.type == OFFLINE_TYPE_PC) {
            last_bb_handled = append_bb_entries(tidx, &in_entry);
        } else if (in_entry.tid.type == OFFLINE_TYPE_THREAD) {
            VPRINT(2, "Thread %u entry\n", (uint)in_entry.tid.tid);
            if (tids[tidx] == INVALID_THREAD_ID)
                tids[tidx] = in_entry.tid.tid;
            size = instru.append_tid(buf, in_entry.tid.tid);
        } else if (in_entry.pid.type == OFFLINE_TYPE_PID) {
            VPRINT(2, "Process %u entry\n", (uint)in_entry.pid.pid);
            size = instru.append_pid(buf, in_entry.pid.pid);
        } else
            FATAL_ERROR("Unknown trace type %d", in_entry.timestamp.type);
        if (size > 0) {
            CHECK((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
            if (!out_file.write((char*)buf, size))
                FATAL_ERROR("Failed to write to output file");
        }
    } while (thread_count > 0);
}

void
raw2trace_t::do_conversion()
{
    read_and_map_modules();
    open_thread_files();
    merge_and_process_thread_files();
}

raw2trace_t::raw2trace_t(std::string indir_in, std::string outname_in)
    : indir(indir_in), outname(outname_in)
{
    // Support passing both base dir and raw/ subdir.
    if (indir.find(OUTFILE_SUBDIR) == std::string::npos)
        indir += std::string(DIRSEP) + OUTFILE_SUBDIR;
    out_file.open(outname.c_str(), std::ofstream::binary);
    if (!out_file)
        FATAL_ERROR("Failed to open output file %s", outname.c_str());
    VPRINT(1, "Writing to %s\n", outname.c_str());

    dcontext = dr_standalone_init();
}

raw2trace_t::~raw2trace_t()
{
    out_file.close();
    for (std::vector<std::ifstream*>::iterator fi = thread_files.begin();
         fi != thread_files.end(); ++fi)
        (*fi)->close();
    unmap_modules();
}
