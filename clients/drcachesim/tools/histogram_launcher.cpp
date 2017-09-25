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

/* Standalone histogram analysis tool launcher for file traces. */

#ifdef WINDOWS
# define UNICODE
# define _UNICODE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#include "droption.h"
#include "dr_frontend.h"
#include "../analyzer.h"
#include "histogram_create.h"

#define FATAL_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

static droption_t<std::string> op_trace
(DROPTION_SCOPE_FRONTEND, "trace", "", "[Required] Trace input file",
 "Specifies the file containing the trace to be analyzed.");

// XXX i#2006: these are duplicated from drcachesim's options.
// Once we decide on the final tool generalization approach we should
// either share these options in a single location or split them.

droption_t<unsigned int> op_line_size
(DROPTION_SCOPE_FRONTEND, "line_size", 64, "Cache line size",
 "Specifies the cache line size, which is assumed to be identical for L1 and L2 "
 "caches.");

droption_t<unsigned int> op_report_top
(DROPTION_SCOPE_FRONTEND, "report_top", 10,
 "Number of top results to be reported",
 "Specifies the number of top results to be reported.");

droption_t<unsigned int> op_verbose
(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64, "Verbosity level",
 "Verbosity level for notifications.");

// For test simplicity we use this same launcher to run some extra tests.
droption_t<bool> op_test_mode
(DROPTION_SCOPE_ALL, "test_mode", false, "Run tests",
 "Run extra analyses for testing.");

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
        op_trace.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    analysis_tool_t *tool =
        histogram_tool_create(op_line_size.get_value(),
                              op_report_top.get_value(),
                              op_verbose.get_value());

    analyzer_t analyzer(op_trace.get_value(), &tool, 1);
    if (!analyzer)
        FATAL_ERROR("failed to initialize analyzer");
    if (!analyzer.run())
        FATAL_ERROR("failed to run analyzer");
    analyzer.print_stats();
    delete tool;

    if (op_test_mode.get_value()) {
        // Test the external-iterator interface.
        tool = histogram_tool_create(op_line_size.get_value(),
                                     op_report_top.get_value(),
                                     op_verbose.get_value());
        analyzer_t external(op_trace.get_value());
        if (!external)
            FATAL_ERROR("failed to initialize analyzer");
        for (reader_t &iter = external.begin(); iter != external.end(); ++iter) {
            if (!tool->process_memref(*iter))
                FATAL_ERROR("tool failed to process entire trace");
        }
        if (!tool->print_results())
            FATAL_ERROR("tool failed to print results");
        delete tool;
    }

    return 0;
}
