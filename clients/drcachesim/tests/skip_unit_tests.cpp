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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for the skip feature. */

#include "droption.h"
#include "zipfile_file_reader.h"
#include "tools/view_create.h"

#include <iostream>
#include <memory>

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

#ifndef HAS_ZIP
#    error zipfile reading is required for this test
#endif

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

#define CHECK(cond, msg, ...)             \
    do {                                  \
        if (!(cond)) {                    \
            fprintf(stderr, "%s\n", msg); \
            return false;                 \
        }                                 \
    } while (0)

static droption_t<std::string> op_trace_file(DROPTION_SCOPE_FRONTEND, "trace_file", "",
                                             "[Required] Trace input .zip file",
                                             "Specifies the input .zip trace file.");

static droption_t<bool> op_verbose(DROPTION_SCOPE_FRONTEND, "verbose", false,
                                   "Whether to print diagnostics",
                                   "Whether to print diagnostics");

bool
test_skip_initial()
{
    int view_count = 10;
    // Our checked-in trace has a chunk size of 20, letting us test cross-chunk
    // skips.  We verify the chunk size below to ensure updates to that file
    // remember to set that value.
    // We check each skip value to ensure the view tool is outputting the
    // expected instruction count.
    for (int skip_instrs = 0; skip_instrs < 50; skip_instrs++) {
        if (op_verbose.get_value())
            std::cout << "Testing -skip_instrs " << skip_instrs << "\n";
        // Capture cerr.
        std::stringstream capture;
        std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
        // Open the trace.
        std::unique_ptr<reader_t> iter = std::unique_ptr<reader_t>(
            new zipfile_file_reader_t(op_trace_file.get_value()));
        CHECK(!!iter, "failed to open zipfile");
        CHECK(iter->init(), "failed to initialize reader");
        std::unique_ptr<reader_t> iter_end =
            std::unique_ptr<reader_t>(new zipfile_file_reader_t());
        // Run the tool.
        std::unique_ptr<analysis_tool_t> tool = std::unique_ptr<analysis_tool_t>(
            view_tool_create("", /*skip_refs=*/0, /*sim_refs=*/view_count, "att"));
        std::string error = tool->initialize_stream(iter.get());
        CHECK(error.empty(), error.c_str());
        iter->skip_instructions(skip_instrs);
        for (; *iter != *iter_end; ++(*iter)) {
            const memref_t &memref = **iter;
            CHECK(tool->process_memref(memref), tool->get_error_string().c_str());
        }
        // Check the result.
        std::string res = capture.str();
        if (op_verbose.get_value())
            std::cout << "Got: |" << res << "|\n";
        CHECK(skip_instrs != 0 ||
                  res.find("chunk instruction count 20") != std::string::npos,
              "expecting chunk size of 20 in test trace");
        std::stringstream res_stream(res);
        // Example output for -skip_instrs 49:
        //    Output format:
        //    <--record#-> <--instr#->: <---tid---> <record details>
        //    ------------------------------------------------------------
        //              69          49:     3854659 <marker: timestamp 13312570674112282>
        //              70          49:     3854659 <marker: tid 3854659 on core 3>
        //              71          50:     3854659 ifetch    2 byte(s) @ 0x0000000401 ...
        //                                   d9                jnz    $0x000000000040100b
        std::string line;
        // First we expect "Output format:"
        std::getline(res_stream, line, '\n');
        CHECK(starts_with(line, "Output format"), "missing header");
        // Next we expect "<--record#-> <--instr#->: <---tid---> <record details>"
        std::getline(res_stream, line, '\n');
        CHECK(starts_with(line, "<--record#-> <--instr#->"), "missing 2nd header");
        // Next we expect "------------------------------------------------------------"
        std::getline(res_stream, line, '\n');
        CHECK(starts_with(line, "------"), "missing divider line");
        // Next we expect the timestamp entry with the instruction count before
        // a colon: "       69       49: T3854659 <marker: timestamp 13312570674112282>"
        // We expect the count to equal the -skip_instrs value.
        std::getline(res_stream, line, '\n');
        std::stringstream expect_stream;
        expect_stream << skip_instrs << ":";
        CHECK(line.find(expect_stream.str()) != std::string::npos, "bad instr ordinal");
        CHECK(skip_instrs == 0 || line.find("timestamp") != std::string::npos,
              "missing timestamp");
        // Next we expect the cpuid entry.
        std::getline(res_stream, line, '\n');
        CHECK(skip_instrs == 0 || line.find("on core") != std::string::npos,
              "missing cpuid");
        // Next we expect the target instruction fetch.
        std::getline(res_stream, line, '\n');
        CHECK(skip_instrs == 0 || line.find("ifetch") != std::string::npos,
              "missing ifetch");
        // We don't know the precise record count but ensure it's > instr count.
        int64_t ref_count;
        std::istringstream ref_in(line);
        ref_in >> ref_count;
        CHECK(ref_count > skip_instrs, "invalid ref count");
        // Reset cerr.
        std::cerr.rdbuf(prior);
    }
    return true;
}

int
test_main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace_file.get_value().empty()) {
        FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                    droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }
    if (!test_skip_initial())
        return 1;
    // TODO i#5538: Add tests that skip from the middle once we have full support
    // for duplicating the timestamp,cpu in that scenario.
    fprintf(stderr, "Success\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
