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

/* raw2trace.h: shared defines between the tracer and the converter.
 */

#ifndef _RAW2TRACE_H_
#define _RAW2TRACE_H_ 1

#include "dr_api.h"
#include "drmemtrace.h"
#include "trace_entry.h"
#include <fstream>
#include "hashtable.h"
#include <vector>

#define OUTFILE_PREFIX "drmemtrace"
#define OUTFILE_SUFFIX "raw"
#define OUTFILE_SUBDIR "raw"
#define TRACE_FILENAME "drmemtrace.trace"

struct module_t {
    module_t(const char *path, app_pc orig, byte *map, size_t size) :
        path(path), orig_base(orig), map_base(map), map_size(size) {}
    const char *path;
    app_pc orig_base;
    byte *map_base;
    size_t map_size;
};

class raw2trace_t {
public:
    // module_map, thread_files and out_file are all owned and opened/closed by the
    // caller.
    raw2trace_t(const char *module_map, const std::vector<std::istream*> &thread_files,
                std::ostream *out_file, void *dcontext = NULL,
                unsigned int verbosity = 0);
    ~raw2trace_t();
    // Returns non-empty error message on failure.
    std::string do_conversion();
    static std::string check_thread_file(std::istream *f);

private:
    std::string read_and_map_modules(const char *module_map);
    std::string unmap_modules(void);
    std::string merge_and_process_thread_files();
    std::string append_bb_entries(uint tidx, offline_entry_t *in_entry,
                                  OUT bool *handled);
    std::string append_memref(INOUT trace_entry_t **buf_in, uint tidx, instr_t *instr,
                              opnd_t ref, bool write);

    static const uint MAX_COMBINED_ENTRIES = 64;
    const char *modmap;
    void *modhandle;
    std::vector<module_t> modvec;
    std::vector<std::istream*> thread_files;
    std::ostream *out_file;
    void *dcontext;
    bool prev_instr_was_rep_string;
    // This indicates that each memref has its own PC entry and that each
    // icache entry does not need to be considered a memref PC entry as well.
    bool instrs_are_separate;
    unsigned int verbosity;
    // We use a hashtable to cache decodings.  We compared the performance of
    // hashtable_t to std::map.find, std::map.lower_bound, std::tr1::unordered_map,
    // and c++11 std::unordered_map (including tuning its load factor, initial size,
    // and hash function), and hashtable_t outperformed the others (i#2056).
    hashtable_t decode_cache;
};

#endif /* _RAW2TRACE_H_ */
