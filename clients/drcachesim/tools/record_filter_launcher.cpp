/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* Standalone record filter tool launcher for file traces. */

#include "analyzer.h"
#include "droption.h"
#include "dr_frontend.h"
#include "tools/filter/null_filter.h"
#include "tools/filter/record_filter.h"

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

static droption_t<std::string>
    op_trace_dir(DROPTION_SCOPE_FRONTEND, "trace_dir", "",
                 "[Required] Trace input directory",
                 "Specifies the directory containing the trace files to be filtered.");

static droption_t<std::string>
    op_output_dir(DROPTION_SCOPE_FRONTEND, "output_dir", "",
                  "[Required] Output directory for the filtered trace",
                  "Specifies the directory where the filtered trace will be written.");

droption_t<unsigned int> op_verbose(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64,
                                    "Verbosity level",
                                    "Verbosity level for notifications.");

int
_tmain(int argc, const TCHAR *targv[])
{
    char **argv;
    drfront_status_t sc = drfront_convert_args(targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to process args: %d", sc);

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_dir.get_value().empty() || op_output_dir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    record_filter_t::record_filter_func_t *null_filter = new null_filter_t();
    std::vector<record_filter_t::record_filter_func_t *> filter_funcs;
    filter_funcs.push_back(null_filter);
    // TODO i#5675: Add other filters.

    record_analysis_tool_t *record_filter = new record_filter_t(
        op_output_dir.get_value(), filter_funcs, op_verbose.get_value());
    std::vector<record_analysis_tool_t *> tools;
    tools.push_back(record_filter);

    record_analyzer_t record_analyzer(op_trace_dir.get_value(), &tools[0],
                                      (int)tools.size());
    if (!record_analyzer) {
        FATAL_ERROR("failed to initialize trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.run()) {
        FATAL_ERROR("failed to run trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    record_analyzer.print_stats();
    delete record_filter;
    delete null_filter;

    fprintf(stderr, "Done!\n");
    return 0;
}
