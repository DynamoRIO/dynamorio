/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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

#ifndef _RAW2TRACE_DIRECTORY_H_
#define _RAW2TRACE_DIRECTORY_H_ 1

#include <fstream>
#include <string>
#include <vector>

#include "dr_api.h"

class raw2trace_directory_t {
public:
    raw2trace_directory_t(unsigned int verbosity = 0)
        : modfile_bytes_(nullptr)
        , modfile_(INVALID_FILE)
        , indir_("")
        , outdir_("")
        , verbosity_(verbosity)
    {
        // We use DR API routines so we need to initialize.
        dr_standalone_init();
    }
    ~raw2trace_directory_t();

    // If outdir.empty() then a peer of indir's OUTFILE_SUBDIR named TRACE_SUBDIR
    // is used by default.  Returns "" on success or an error message on failure.
    std::string
    initialize(const std::string &indir, const std::string &outdir);
    // Use this instead of initialize() to only fill in modfile_bytes, for
    // constructing a module_mapper_t.  Returns "" on success or an error message on
    // failure.
    std::string
    initialize_module_file(const std::string &module_file_path);
    // Use this instead of initialize() to only read the funcion map file.
    // Returns "" on success or an error message on failure.
    // On success, pushes the parsed entries from the file into "entries".
    std::string
    initialize_funclist_file(const std::string &funclist_file_path,
                             OUT std::vector<std::vector<std::string>> *entries);

    static std::string
    tracedir_from_rawdir(const std::string &rawdir);

    char *modfile_bytes_;
    std::vector<std::istream *> in_files_;
    std::vector<std::ostream *> out_files_;

private:
    std::string
    read_module_file(const std::string &modfilename);
    std::string
    open_thread_files();
    std::string
    open_thread_log_file(const char *basename);
    file_t modfile_;
    std::string indir_;
    std::string outdir_;
    unsigned int verbosity_;
};

#endif /* _RAW2TRACE_DIRECTORY_H_ */
