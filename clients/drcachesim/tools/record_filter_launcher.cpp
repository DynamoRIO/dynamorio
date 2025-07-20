/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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

#ifdef WINDOWS
#    define NOMINMAX // Avoid windows.h messing up std::max.
#    define UNICODE  // For Windows headers.
#    define _UNICODE // For C headers.
#endif

#include "analyzer.h"
#include "droption.h"
#include "dr_frontend.h"
#include "tools/filter/record_filter_create.h"
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
    // Wrap max in parens to work around Visual Studio compiler issues with the
    // max macro (even despite NOMINMAX defined above).
    op_stop_timestamp(DROPTION_SCOPE_ALL, "stop_timestamp", 0, 0,
                      (std::numeric_limits<uint64_t>::max)(),
                      "Timestamp (in us) in the trace when to stop filtering.",
                      "Record filtering will be disabled (everything will be output) "
                      "when the tool sees a TRACE_MARKER_TYPE_TIMESTAMP marker with "
                      "timestamp greater than the specified value.");

static droption_t<int> op_cache_filter_size(
    DROPTION_SCOPE_FRONTEND, "cache_filter_size", 0,
    "Enable data cache filter with given size (in bytes).",
    "Enable data cache filter with given size (in bytes), with 64 byte "
    "line size and a direct mapped LRU cache.");

static droption_t<std::string>
    op_remove_trace_types(DROPTION_SCOPE_FRONTEND, "remove_trace_types", "",
                          "Comma-separated integers for trace types to remove.",
                          "Comma-separated integers for trace types to remove. "
                          "See trace_type_t for the list of trace entry types.");

static droption_t<std::string>
    op_remove_marker_types(DROPTION_SCOPE_FRONTEND, "remove_marker_types", "",
                           "Comma-separated integers for marker types to remove.",
                           "Comma-separated integers for marker types to remove. "
                           "See trace_marker_type_t for the list of marker types.");

static droption_t<uint64_t> op_trim_before_timestamp(
    DROPTION_SCOPE_ALL, "trim_before_timestamp", 0, 0,
    (std::numeric_limits<uint64_t>::max)(),
    "Trim records until this timestamp (in us) in the trace.",
    "Removes all records (after headers) before the first TRACE_MARKER_TYPE_TIMESTAMP "
    "marker in the trace with timestamp greater than or equal to the specified value.");

static droption_t<uint64_t> op_trim_after_timestamp(
    DROPTION_SCOPE_ALL, "trim_after_timestamp", 0, 0,
    (std::numeric_limits<uint64_t>::max)(),
    "Trim records after this timestamp (in us) in the trace.",
    "Removes all records from the first TRACE_MARKER_TYPE_TIMESTAMP marker with "
    "timestamp larger than the specified value.");

static droption_t<uint64_t> op_trim_before_instr(
    DROPTION_SCOPE_ALL, "trim_before_instr", 0, 0, (std::numeric_limits<uint64_t>::max)(),
    "Trim records approximately until this instruction ordinal in the trace.",
    "Removes all records (after headers) before the first TRACE_MARKER_TYPE_TIMESTAMP "
    "marker in the trace that comes after the specified instruction ordinal.");

static droption_t<uint64_t> op_trim_after_instr(
    DROPTION_SCOPE_ALL, "trim_after_instr", 0, 0, (std::numeric_limits<uint64_t>::max)(),
    "Trim records approximately after this instruction ordinal in the trace.",
    "Removes all records from the first TRACE_MARKER_TYPE_TIMESTAMP marker in the trace "
    "that comes after the specified instruction ordinal.");

/* XXX i#6369: we should partition our options by tool. This one should belong to the
 * record_filter partition. For now we add the filter_ prefix to options that should be
 * used in conjunction with record_filter.
 */
droption_t<bool> op_encodings2regdeps(
    DROPTION_SCOPE_FRONTEND, "filter_encodings2regdeps", false,
    "Enable converting the encoding of instructions to synthetic ISA DR_ISA_REGDEPS.",
    "This option is for -tool record_filter. When present, it converts "
    "the encoding of instructions from a real ISA to the DR_ISA_REGDEPS synthetic ISA.");

droption_t<std::string>
    op_filter_func_ids(DROPTION_SCOPE_FRONTEND, "filter_keep_func_ids", "",
                       "Comma-separated integers of function IDs to keep.",
                       "This option is for -tool record_filter. It preserves "
                       "TRACE_MARKER_TYPE_FUNC_[ID | ARG | RETVAL | RETADDR] "
                       "markers for the listed function IDs and removed those "
                       "belonging to unlisted function IDs.");

droption_t<std::string> op_modify_marker_value(
    DROPTION_SCOPE_FRONTEND, "filter_modify_marker_value", "",
    "Comma-separated pairs of integers representing <TRACE_MARKER_TYPE_, new_value>.",
    "This option is for -tool record_filter. It modifies the value of all listed "
    "TRACE_MARKER_TYPE_ markers in the trace with their corresponding new_value. "
    "The list must have an even size. Example: -filter_modify_marker_value 3,24,18,2048 "
    "sets all TRACE_MARKER_TYPE_CPU_ID == 3 in the trace to core 24 and "
    "TRACE_MARKER_TYPE_PAGE_SIZE == 18 to 2k.");

} // namespace

int
_tmain(int argc, const TCHAR *targv[])
{
    disable_popups();

#if defined(WINDOWS) && !defined(_UNICODE)
#    error _UNICODE must be defined
#endif

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

    auto record_filter = std::unique_ptr<record_analysis_tool_t>(
        dynamorio::drmemtrace::record_filter_tool_create(
            op_output_dir.get_value(), op_stop_timestamp.get_value(),
            op_cache_filter_size.get_value(), op_remove_trace_types.get_value(),
            op_remove_marker_types.get_value(), op_trim_before_timestamp.get_value(),
            op_trim_after_timestamp.get_value(), op_trim_before_instr.get_value(),
            op_trim_after_instr.get_value(), op_encodings2regdeps.get_value(),
            op_filter_func_ids.get_value(), op_modify_marker_value.get_value(),
            op_verbose.get_value()));
    std::vector<record_analysis_tool_t *> tools;
    tools.push_back(record_filter.get());

    record_analyzer_t record_analyzer(
        op_trace_dir.get_value(), &tools[0], (int)tools.size(), /*worker_count=*/0,
        /*skip_instrs=*/0, /*interval_microseconds=*/0, op_verbose.get_value());
    if (!record_analyzer) {
        FATAL_ERROR("Failed to initialize trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.run()) {
        FATAL_ERROR("Failed to run trace filter: %s",
                    record_analyzer.get_error_string().c_str());
    }
    if (!record_analyzer.print_stats()) {
        FATAL_ERROR("Failed to print stats: %s",
                    record_analyzer.get_error_string().c_str());
    }

    fprintf(stderr, "Done!\n");
    return 0;
}
