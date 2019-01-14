/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

#include "analyzer.h"
#include "analyzer_multi.h"
#include "analysis_tool_interface.h"
#include "common/options.h"
#include "common/utils.h"
#include "common/directory_iterator.h"
#include "tracer/raw2trace_directory.h"
#include "tracer/raw2trace.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#endif
#include "reader/ipc_reader.h"
#ifdef DEBUG
#    include "tests/trace_invariants.h"
#endif

analyzer_multi_t::analyzer_multi_t()
{
    worker_count = op_jobs.get_value();
    // Initial measurements show it's sometimes faster to keep the parallel model
    // of using single-file readers but use them sequentially, as opposed to
    // the every-file interleaving reader, but the user can specify -jobs 1, so
    // we still keep the serial vs parallel split for 0.
    if (worker_count == 0)
        parallel = false;
    if (!create_analysis_tools()) {
        success = false;
        error_string = "Failed to create analysis tool: " + error_string;
        return;
    }
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_indir.get_value().empty() && op_infile.get_value().empty() &&
        op_ipc_name.get_value().empty()) {
        error_string =
            "Usage error: -ipc_name or -indir or -infile is required\nUsage:\n" +
            droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        success = false;
        return;
    }
    if (!op_indir.get_value().empty()) {
        std::string tracedir =
            raw2trace_directory_t::tracedir_from_rawdir(op_indir.get_value());
        // We support the trace dir being empty if we haven't post-processed
        // the raw files yet.
        bool needs_processing = false;
        if (!directory_iterator_t::is_directory(tracedir))
            needs_processing = true;
        else {
            directory_iterator_t end;
            directory_iterator_t iter(tracedir);
            if (!iter) {
                needs_processing = true;
            } else {
                int count = 0;
                for (; iter != end; ++iter) {
                    if ((*iter) == "." || (*iter) == "..")
                        continue;
                    ++count;
                    // XXX: It would be nice to call file_reader_t::is_complete()
                    // but we don't have support for that for compressed files.
                    // Thus it's up to the user to delete incomplete processed files.
                }
                if (count == 0)
                    needs_processing = true;
            }
        }
        if (needs_processing) {
            raw2trace_directory_t dir(op_verbose.get_value());
            std::string dir_err = dir.initialize(op_indir.get_value(), "");
            if (!dir_err.empty()) {
                success = false;
                error_string = "Directory setup failed: " + dir_err;
            }
            raw2trace_t raw2trace(dir.modfile_bytes, dir.in_files, dir.out_files, nullptr,
                                  op_verbose.get_value(), op_jobs.get_value());
            std::string error = raw2trace.do_conversion();
            if (!error.empty()) {
                success = false;
                error_string = "raw2trace failed: " + error;
            }
        }
        if (!init_file_reader(tracedir, op_verbose.get_value()))
            success = false;
    } else if (op_infile.get_value().empty()) {
        // XXX i#3323: Add parallel analysis support for online tools.
        parallel = false;
        serial_trace_iter =
            std::unique_ptr<reader_t>(new ipc_reader_t(op_ipc_name.get_value().c_str()));
        trace_end = std::unique_ptr<reader_t>(new ipc_reader_t());
        if (!*serial_trace_iter) {
            success = false;
#ifdef UNIX
            // This is the most likely cause of the error.
            // XXX: Even better would be to propagate the mkfifo errno here.
            error_string = "try removing stale pipe file " +
                reinterpret_cast<ipc_reader_t *>(serial_trace_iter.get())
                    ->get_pipe_name();
#endif
        }
    } else {
        // Legacy file.
        if (!init_file_reader(op_infile.get_value(), op_verbose.get_value()))
            success = false;
    }
    // We can't call serial_trace_iter->init() here as it blocks for ipc_reader_t.
}

analyzer_multi_t::~analyzer_multi_t()
{
    destroy_analysis_tools();
}

bool
analyzer_multi_t::create_analysis_tools()
{
    /* FIXME i#2006: add multiple tool support. */
    /* FIXME i#2006: create a single top-level tool for multi-component
     * tools.
     */
    tools = new analysis_tool_t *[max_num_tools];
    tools[0] = drmemtrace_analysis_tool_create();
    if (tools[0] == NULL)
        return false;
    std::string tool_error;
    if (!*tools[0]) {
        tool_error = tools[0]->get_error_string();
        if (tool_error.empty())
            tool_error = "no error message provided.";
    } else
        tool_error = tools[0]->initialize();
    if (!tool_error.empty()) {
        error_string = "Tool failed to initialize: " + tool_error;
        delete tools[0];
        tools[0] = NULL;
        return false;
    }
    num_tools = 1;
#ifdef DEBUG
    if (op_test_mode.get_value()) {
        tools[1] = new trace_invariants_t(op_offline.get_value(), op_verbose.get_value());
        if (tools[1] == NULL)
            return false;
        if (!!*tools[1])
            tools[1]->initialize();
        if (!*tools[1]) {
            error_string = tools[1]->get_error_string();
            delete tools[1];
            tools[1] = NULL;
            return false;
        }
        num_tools = 2;
    }
#endif
    return true;
}

void
analyzer_multi_t::destroy_analysis_tools()
{
    if (!success)
        return;
    for (int i = 0; i < num_tools; i++)
        delete tools[i];
    delete[] tools;
}
