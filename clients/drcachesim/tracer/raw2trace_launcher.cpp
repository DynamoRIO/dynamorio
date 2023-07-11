/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* Standalone raw2trace converter. */

#ifdef WINDOWS
#    define UNICODE
#    define _UNICODE
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

#include "droption.h"
#include "dr_frontend.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"

using ::dynamorio::drmemtrace::raw2trace_directory_t;
using ::dynamorio::drmemtrace::raw2trace_t;
using ::dynamorio::droption::bytesize_t;
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

namespace {

// XXX: We're duplicating some options from common/options.cpp: we should be
// able to share?!

static droption_t<std::string>
    op_indir(DROPTION_SCOPE_FRONTEND, "indir", "",
             "[Required] Directory with trace input files",
             "Specifies a directory within which all *.raw files will be processed.");

static droption_t<std::string>
    op_outdir(DROPTION_SCOPE_FRONTEND, "out", "", "Path to output directory",
              "Specifies the path to the output directory where per-thread output files "
              "will be written.  If unspecified, -indir/trace/ is used.");

static droption_t<std::string> op_alt_module_dir(
    DROPTION_SCOPE_FRONTEND, "alt_module_dir", "", "Alternate module search directory",
    "Specifies a directory to look for binaries needed to post-process "
    "the trace.  This directory takes precedence over the recorded path.");

static droption_t<bytesize_t> op_chunk_instr_count(
    DROPTION_SCOPE_FRONTEND, "chunk_instr_count", 10 * 1000 * 1000U,
    "Chunk instruction count",
    "Specifies the size in instructions of the chunks into which a trace output file "
    "is split inside a zipfile.  This is the granularity of a fast seek. "
    "For 32-bit this cannot exceed 4G.");

static droption_t<unsigned int> op_verbose(DROPTION_SCOPE_FRONTEND, "verbose", 0,
                                           "Verbosity level for diagnostic output",
                                           "Verbosity level for diagnostic output.");

static droption_t<int>
    op_jobs(DROPTION_SCOPE_ALL, "jobs", -1, "Number of parallel jobs",
            "By default, post-processing is parallelized.  This option controls the "
            "number of concurrent jobs.  0 "
            "disables concurrency and uses  single thread to perform all operations.  A "
            "negative value sets the job count to the number of hardware threads.");

static droption_t<std::string> op_trace_compress(
    DROPTION_SCOPE_FRONTEND, "compress", DEFAULT_TRACE_COMPRESSION_TYPE,
    "Trace compression: \"zip\",\"gzip\",\"zlib\",\"lz4\",\"none\"",
    "Specifies the compression type to use for trace files: \"zip\", "
    "\"gzip\", \"zlib\", \"lz4\", or \"none\". "
    "In most cases where fast skipping by instruction count is not needed "
    "lz4 compression generally improves performance and is recommended. "
    "When it comes to storage types, the impact on overhead varies: "
    "for SSDs, zip and gzip often increase overhead and should only be chosen "
    "if space is limited.");

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

} // namespace

int
_tmain(int argc, const TCHAR *targv[])
{
    // Convert to UTF-8 if necessary
    char **argv;
    drfront_status_t sc = drfront_convert_args(targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to process args: %d", sc);

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_indir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    raw2trace_directory_t dir(op_verbose.get_value());
    std::string dir_err = dir.initialize(op_indir.get_value(), op_outdir.get_value(),
                                         op_trace_compress.get_value());
    if (!dir_err.empty())
        FATAL_ERROR("Directory parsing failed: %s", dir_err.c_str());
    raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                          dir.out_archives_, dir.encoding_file_,
                          dir.serial_schedule_file_, dir.cpu_schedule_file_, nullptr,
                          op_verbose.get_value(), op_jobs.get_value(),
                          op_alt_module_dir.get_value(), op_chunk_instr_count.get_value(),
                          dir.in_kfiles_map_, dir.kcoredir_, dir.kallsymsdir_);
    std::string error = raw2trace.do_conversion();
    if (!error.empty())
        FATAL_ERROR("Conversion failed: %s", error.c_str());

    return 0;
}
