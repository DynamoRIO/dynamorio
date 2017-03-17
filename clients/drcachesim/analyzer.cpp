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

#include "analysis_tool.h"
#include "analyzer.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
# include "reader/compressed_file_reader.h"
#endif
#include "common/utils.h"

analyzer_t::analyzer_t() :
    success(true), trace_iter(NULL), trace_end(NULL), num_tools(0), tools(NULL)
{
    /* Nothing else: child class needs to initialize. */
}

analyzer_t::analyzer_t(const std::string &trace_file, analysis_tool_t **tools_in,
                       int num_tools_in) :
    success(true), trace_iter(NULL), trace_end(NULL), num_tools(num_tools_in),
    tools(tools_in)
{
    for (int i = 0; i < num_tools; ++i) {
        if (tools[i] == NULL || !*tools[i]) {
            success = false;
            ERRMSG("Tool is not successfully initialized\n");
            return;
        }
    }
    if (trace_file.empty()) {
        success = false;
        ERRMSG("Trace file name is empty\n");
        return;
    }
#ifdef HAS_ZLIB
    // Even if the file is uncompressed, zlib's gzip interface is faster than
    // file_reader_t's fstream in our measurements, so we always use it when
    // available.
    trace_iter = new compressed_file_reader_t(trace_file.c_str());
    trace_end = new compressed_file_reader_t();
#else
    trace_iter = new file_reader_t(trace_file.c_str());
    trace_end = new file_reader_t();
#endif
}

analyzer_t::~analyzer_t()
{
    delete trace_iter;
    delete trace_end;
}

bool
analyzer_t::operator!()
{
    return !success;
}

bool
analyzer_t::start_reading()
{
    if (!trace_iter->init()) {
        ERRMSG("failed to read from trace\n");
        return false;
    }
    return true;
}

bool
analyzer_t::run()
{
    bool res = true;
    if (!start_reading())
        return false;

    for (; *trace_iter != *trace_end; ++(*trace_iter)) {
        for (int i = 0; i < num_tools; ++i) {
            memref_t memref = **trace_iter;
            res = tools[i]->process_memref(memref) && res;
        }
    }
    return res;
}

bool
analyzer_t::print_stats()
{
    bool res = true;
    for (int i = 0; i < num_tools; ++i)
        res = tools[i]->print_results() && res;
    return res;
}
