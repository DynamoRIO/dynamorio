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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for trace_filter. */

#include <cstring>
#include "analyzer.h"
#include "dr_api.h"
#include "droption.h"
#include "dr_frontend.h"
#include "tools/basic_counts.h"
#include "tools/trace_filter.h"
#include <iostream>

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

#define CHECK(cond, msg, ...)         \
    do {                              \
        if (!(cond)) {                \
            std::cerr << msg << "\n"; \
            return false;             \
        }                             \
    } while (0)

static droption_t<std::string>
    op_trace_dir(DROPTION_SCOPE_FRONTEND, "trace_dir", "",
                 "[Required] Trace input directory",
                 "Specifies the directory containing the trace files to be filtered.");

static droption_t<std::string> op_tmp_output_dir(
    DROPTION_SCOPE_FRONTEND, "tmp_output_dir", "",
    "[Required] Output directory for the filtered trace",
    "Specifies the directory where the filtered trace will be written.");

static bool
local_create_dir(const char *dir)
{
    if (!dr_directory_exists(dir))
        return dr_create_dir(dir);
    return true;
}

basic_counts_t::counters_t
get_basic_counts(const std::string &trace_dir)
{
    analysis_tool_t *basic_counts_tool = new basic_counts_t(/*verbose=*/0);
    std::vector<analysis_tool_t *> tools;
    tools.push_back(basic_counts_tool);
    analyzer_t analyzer(trace_dir, &tools[0], (int)tools.size());
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    basic_counts_t::counters_t counts =
        ((basic_counts_t *)basic_counts_tool)->get_total_counts();
    delete basic_counts_tool;
    return counts;
}

bool
test_nop_filter()
{
    std::string output_dir = op_tmp_output_dir.get_value() + "/nop_filter";
    if (!local_create_dir(output_dir.c_str())) {
        FATAL_ERROR("Failed to create filtered trace output dir %s", output_dir.c_str());
    }
    {
        auto trace_filter = std::unique_ptr<trace_filter_t>(
            new trace_filter_t(op_trace_dir.get_value(), output_dir));
        if (!trace_filter->run()) {
            FATAL_ERROR("Failed to run trace_filter %s",
                        trace_filter->get_error_string().c_str());
        }
        // Need to destroy trace_filter to flush the output.
    }
    basic_counts_t::counters_t c1 = get_basic_counts(op_trace_dir.get_value());
    basic_counts_t::counters_t c2 = get_basic_counts(output_dir);
    if (c1 == c2) {
        fprintf(stderr, "test_nop_filter passed\n");
        return true;
    }
    fprintf(stderr, "Nop filter returned different counts\n");
    return false;
}

int
main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_dir.get_value().empty() || op_tmp_output_dir.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }

    if (!test_nop_filter())
        return 1;
    fprintf(stderr, "All done!\n");
    return 0;
}
