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

#include "analyzer.h"
#include "analyzer_multi.h"
#include "analysis_tool_interface.h"
#include "common/options.h"
#include "common/utils.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
# include "reader/compressed_file_reader.h"
#endif
#include "reader/ipc_reader.h"
#include "tracer/raw2trace_directory.h"
#include "tracer/raw2trace.h"

analyzer_multi_t::analyzer_multi_t()
{
    if (!create_analysis_tools()) {
        success = false;
        ERRMSG("Failed to create analysis tool\n");
        return;
    }
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_infile.get_value().empty() && op_ipc_name.get_value().empty()) {
        ERRMSG("Usage error: -ipc_name or -infile is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        success = false;
        return;
    }
    if (!op_indir.get_value().empty()) {
        // XXX: better to put in app name + pid, or rely on staying inside subdir?
        std::string tracefile = op_indir.get_value() + std::string(DIRSEP) +
            TRACE_FILENAME;
        file_reader_t *existing = new file_reader_t(tracefile.c_str());
        if (existing->is_complete())
            trace_iter = existing;
        else {
            delete existing;
            raw2trace_directory_t dir(op_indir.get_value(), tracefile);
            raw2trace_t raw2trace(dir.modfile_bytes, dir.thread_files, &dir.out_file);
            std::string error = raw2trace.do_conversion();
            if (!error.empty())
                ERRMSG("raw2trace failed: %s\n", error.c_str());
            trace_iter = new file_reader_t(tracefile.c_str());
        }
        // We don't support a compressed file here (is_complete() is too hard
        // to implement).
        trace_end = new file_reader_t();
    } else if (op_infile.get_value().empty()) {
        trace_iter = new ipc_reader_t(op_ipc_name.get_value().c_str());
        trace_end = new ipc_reader_t();
    } else {
#ifdef HAS_ZLIB
        // Even for uncompressed files, zlib's gzip interface is faster than fstream.
        trace_iter = new compressed_file_reader_t(op_infile.get_value().c_str());
        trace_end = new compressed_file_reader_t();
#else
        trace_iter = new file_reader_t(op_infile.get_value().c_str());
        trace_end = new file_reader_t();
#endif
    }
    // We can't call trace_iter->init() here as it blocks for ipc_reader_t.
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
    tools = new analysis_tool_t*[max_num_tools];
    tools[0] = drmemtrace_analysis_tool_create();
    if (tools[0] != NULL && !*tools[0]) {
        delete tools[0];
        tools[0] = NULL;
    }
    if (tools[0] == NULL)
        return false;
    num_tools = 1;
    return true;
}

void
analyzer_multi_t::destroy_analysis_tools()
{
    if (!success)
        return;
    for (int i = 0; i < num_tools; i++)
        delete tools[i];
    delete [] tools;
}
