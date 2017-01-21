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

/* raw2trace.h: shared defines between the tracer and the converter.
 */

#ifndef _RAW2TRACE_H_
#define _RAW2TRACE_H_ 1

#include "dr_api.h"
#include "../common/trace_entry.h"
#include <fstream>
#include <vector>

#define OUTFILE_PREFIX "drmemtrace"
#define OUTFILE_SUFFIX "raw"
#define OUTFILE_SUBDIR "raw"
#define MODULE_LIST_FILENAME "modules.log"
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
    raw2trace_t(std::string indir, std::string outname);
    ~raw2trace_t();
    void do_conversion();

private:
    void read_and_map_modules(void);
    void unmap_modules(void);
    void open_thread_log_file(const char *basename);
    void open_thread_files();
    void merge_and_process_thread_files();
    bool append_bb_entries(uint tidx, offline_entry_t *in_entry);
    trace_entry_t *append_memref(trace_entry_t *buf_in, uint tidx, instr_t *instr,
                                 opnd_t ref, bool write);

    std::string indir;
    std::string outname;
    std::ofstream out_file;
    static const uint MAX_COMBINED_ENTRIES = 64;
    void *modhandle;
    std::vector<module_t> modvec;
    std::vector<std::ifstream*> thread_files;
    void *dcontext;
    bool prev_instr_was_rep_string;
};

#endif /* _RAW2TRACE_H_ */
