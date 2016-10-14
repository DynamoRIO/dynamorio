/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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
#include "common/options.h"
#include "common/utils.h"
#include "reader/file_reader.h"
#include "reader/ipc_reader.h"
#include "simulator/cache_simulator.h"
#include "simulator/tlb_simulator.h"
#include "tools/histogram.h"
#include "tracer/raw2trace.h"
#include <fstream>

analyzer_t::analyzer_t() :
    success(true), trace_iter(NULL), trace_end(NULL), num_tools(0)
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
        std::ifstream exists(tracefile.c_str());
        if (!exists.good()) {
            raw2trace_t raw2trace(op_indir.get_value(), tracefile);
            raw2trace.do_conversion();
        }
        exists.close();
        trace_iter = new file_reader_t(tracefile.c_str());
        trace_end = new file_reader_t();
    } else if (op_infile.get_value().empty()) {
        trace_iter = new ipc_reader_t(op_ipc_name.get_value().c_str());
        trace_end = new ipc_reader_t();
    } else {
        trace_iter = new file_reader_t(op_infile.get_value().c_str());
        trace_end = new file_reader_t();
    }
    // We can't call trace_iter->init() here as it blocks for ipc_reader_t.
}

analyzer_t::~analyzer_t()
{
    delete trace_iter;
    delete trace_end;
    destroy_analysis_tools();
}

bool
analyzer_t::operator!()
{
    return !success;
}

bool
analyzer_t::run()
{
    bool res = true;
    if (!start_reading())
        return false;

    for (; *trace_iter != *trace_end; ++(*trace_iter)) {
        for (int i = 0; i < num_tools; i++) {
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
    for (int i = 0; i < num_tools; i++)
        res = tools[i]->print_results() && res;
    return res;
}

bool
analyzer_t::start_reading()
{
    if (!trace_iter->init()) {
        ERRMSG("failed to read from %s\n", op_infile.get_value().empty() ?
               op_ipc_name.get_value().c_str() : op_infile.get_value().c_str());
        return false;
    }
    return true;
}

bool
analyzer_t::create_analysis_tools()
{
    /* FIXME i#2006: add multiple tool support. */
    /* FIXME i#2006: create a single top-level tool for multi-component
     * tools.
     */
    if (op_simulator_type.get_value() == CPU_CACHE)
        tools[0] = new cache_simulator_t;
    else if (op_simulator_type.get_value() == TLB)
        tools[0] = new tlb_simulator_t;
    else if (op_simulator_type.get_value() == HISTOGRAM)
        tools[0] = new histogram_t;
    else {
        ERRMSG("Usage error: unsupported analyzer type. "
               "Please choose " CPU_CACHE ", " TLB ", or " HISTOGRAM ".\n");
        return false;
    }
    if (!tools[0])
        return false;
    num_tools = 1;
    return true;
}

void
analyzer_t::destroy_analysis_tools()
{
    if (!success)
        return;
    for (int i = 0; i < num_tools; i++)
        delete tools[i];
}
