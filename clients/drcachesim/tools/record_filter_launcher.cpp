/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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
#include "tools/filter/cache_filter.h"
#include "tools/filter/type_filter.h"
#include "tools/filter/record_filter.h"
#include "tests/test_helpers.h"

#include <limits>

using ::dynamorio::drmemtrace::disable_popups;
using ::dynamorio::drmemtrace::record_analysis_tool_t;
using ::dynamorio::drmemtrace::record_analyzer_t;
using ::dynamorio::drmemtrace::trace_marker_type_t;
using ::dynamorio::drmemtrace::trace_type_t;
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

namespace {

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

static droption_t<unsigned int> op_verbose(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64,
                                           "Verbosity level",
                                           "Verbosity level for notifications.");

static droption_t<uint64_t>
    op_stop_timestamp(DROPTION_SCOPE_ALL, "stop_timestamp", 0, 0,
                      std::numeric_limits<uint64_t>::max(),
                      "Timestamp (in us) in the trace when to stop filtering.",
                      "Record filtering will be disabled (everything will be output) "
                      "when the tool sees a TRACE_MARKER_TYPE_TIMESTAMP marker with "
                      "timestamp greater than the specified value.");

static droption_t<int> op_cache_filter_size(
    DROPTION_SCOPE_FRONTEND, "cache_filter_size", 0,
    "[Required] Enable data cache filter with given size (in bytes).",
    "Enable data cache filter with given size (in bytes), with 64 byte "
    "line size and a direct mapped LRU cache.");

static droption_t<std::string> op_remove_trace_types(
    DROPTION_SCOPE_FRONTEND, "remove_trace_types", "",
    "[Required] Comma-separated integers for trace types to remove.",
    "Comma-separated integers for trace types to remove. "
    "See trace_type_t for the list of trace entry types.");

static droption_t<std::string> op_remove_marker_types(
    DROPTION_SCOPE_FRONTEND, "remove_marker_types", "",
    "[Required] Comma-separated integers for marker types to remove.",
    "Comma-separated integers for marker types to remove. "
    "See trace_marker_type_t for the list of marker types.");

template <typename T>
std::vector<T>
parse_string(const std::string &s, char sep = ',')
{
    size_t pos, at = 0;
    if (s.empty())
        return {};
    std::vector<T> vec;
    do {
        pos = s.find(sep, at);
        vec.push_back(static_cast<T>(std::stoi(s.substr(at, pos))));
        at = pos + 1;
    } while (pos != std::string::npos);
    return vec;
}

} // namespace

int
_tmain(int argc, const TCHAR *targv[])
{
    disable_popups();

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

    std::vector<
        std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>>
        filter_funcs;
    if (op_cache_filter_size.specified()) {
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                // XXX: add more command-line options to allow the user to set these
                // parameters.
                new dynamorio::drmemtrace::cache_filter_t(
                    /*cache_associativity=*/1, /*cache_line_size=*/64,
                    op_cache_filter_size.get_value(),
                    /*filter_data=*/true, /*filter_instrs=*/false)));
    }
    if (op_remove_trace_types.specified() || op_remove_marker_types.specified()) {
        std::vector<trace_type_t> filter_trace_types =
            parse_string<trace_type_t>(op_remove_trace_types.get_value());
        std::vector<trace_marker_type_t> filter_marker_types =
            parse_string<trace_marker_type_t>(op_remove_marker_types.get_value());
        filter_funcs.emplace_back(
            std::unique_ptr<dynamorio::drmemtrace::record_filter_t::record_filter_func_t>(
                new dynamorio::drmemtrace::type_filter_t(filter_trace_types,
                                                         filter_marker_types)));
    }
    // TODO i#5675: Add other filters.

    auto record_filter = std::unique_ptr<record_analysis_tool_t>(
        new dynamorio::drmemtrace::record_filter_t(
            op_output_dir.get_value(), std::move(filter_funcs),
            op_stop_timestamp.get_value(), op_verbose.get_value()));
    std::vector<record_analysis_tool_t *> tools;
    tools.push_back(record_filter.get());

    record_analyzer_t record_analyzer(op_trace_dir.get_value(), &tools[0],
                                      (int)tools.size());
    if (!record_analyzer) {
        FATAL_ERROR("Failed to initialize trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.run()) {
        FATAL_ERROR("Failed to run trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    record_analyzer.print_stats();

    fprintf(stderr, "Done!\n");
    return 0;
}
