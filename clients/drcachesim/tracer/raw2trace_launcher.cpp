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

/* Standalone raw2trace converter. */

#ifdef WINDOWS
# define UNICODE
# define _UNICODE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#include "droption.h"
#include "dr_frontend.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"

static droption_t<std::string> op_indir
(DROPTION_SCOPE_FRONTEND, "indir", "", "[Required] Directory with trace input files",
 "Specifies a directory within which all *.log files will be processed.");

static droption_t<std::string> op_out
(DROPTION_SCOPE_FRONTEND, "out", "", "[Required] Path to output file",
 "Specifies the path to the output file.");

static droption_t<unsigned int> op_verbose
(DROPTION_SCOPE_FRONTEND, "verbose", 0, "Verbosity level for diagnostic output",
 "Verbosity level for diagnostic output.");

#define FATAL_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

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
        op_indir.get_value().empty() ||
        op_out.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    raw2trace_directory_t dir(op_indir.get_value(), op_out.get_value(),
                              op_verbose.get_value());
    raw2trace_t raw2trace(dir.modfile_bytes, dir.thread_files, &dir.out_file, NULL,
                          op_verbose.get_value());
    std::string error = raw2trace.do_conversion();
    if (!error.empty())
        FATAL_ERROR("Conversion failed: %s", error.c_str());

    return 0;
}
