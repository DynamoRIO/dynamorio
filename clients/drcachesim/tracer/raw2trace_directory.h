/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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
#include <unordered_map>
#include <vector>

#include "archive_ostream.h"
#include "dr_api.h"

#if !defined DEFAULT_TRACE_COMPRESSION_TYPE
#    ifdef HAS_ZIP
#        define DEFAULT_TRACE_COMPRESSION_TYPE "zip"
#    elif defined(HAS_LZ4)
#        define DEFAULT_TRACE_COMPRESSION_TYPE "lz4"
#    elif defined(HAS_ZLIB)
#        define DEFAULT_TRACE_COMPRESSION_TYPE "gzip"
#    else
#        define DEFAULT_TRACE_COMPRESSION_TYPE "none"
#    endif
#endif

namespace dynamorio {
namespace drmemtrace {

class raw2trace_directory_t {
public:
    raw2trace_directory_t(unsigned int verbosity = 0)
        : modfile_bytes_(nullptr)
        , encoding_file_(INVALID_FILE)
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
    initialize(const std::string &indir, const std::string &outdir,
               const std::string &compress = DEFAULT_TRACE_COMPRESSION_TYPE);
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

    static std::string
    window_subdir_if_present(const std::string &dir);

    static bool
    is_window_subdir(const std::string &dir);

    char *modfile_bytes_;
    file_t encoding_file_;
    std::vector<std::istream *> in_files_;
    std::vector<std::ostream *> out_files_;
    std::vector<archive_ostream_t *> out_archives_;
    std::ostream *serial_schedule_file_ = nullptr;
    archive_ostream_t *cpu_schedule_file_ = nullptr;
    std::unordered_map<thread_id_t, std::istream *> in_kfiles_map_;
    std::string kcoredir_;
    std::string kallsymsdir_;

private:
    std::string
    trace_suffix();
    std::string
    read_module_file(const std::string &modfilename);
    std::string
    open_thread_files();
    std::string
    open_thread_log_file(const char *basename);
    std::string
    open_serial_schedule_file();
    std::string
    open_cpu_schedule_file();
#ifdef BUILD_PT_POST_PROCESSOR
    std::string
    open_kthread_files();
#endif
    file_t modfile_;
    std::string kernel_indir_;
    std::string indir_;
    std::string outdir_;
    unsigned int verbosity_;
    std::string compress_type_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RAW2TRACE_DIRECTORY_H_ */
