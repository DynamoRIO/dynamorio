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

#include "analyzer.h"
#include "common/options.h"
#include "common/utils.h"
#include "reader/file_reader.h"
#include "reader/ipc_reader.h"

analyzer_t::analyzer_t() :
    success(true)
{
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_infile.get_value().empty() && op_ipc_name.get_value().empty()) {
        ERRMSG("Usage error: -ipc_name or -infile is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        success = false;
        return;
    }
    if (op_infile.get_value().empty()) {
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
        ERRMSG("failed to read from %s\n", op_infile.get_value().empty() ?
               op_ipc_name.get_value().c_str() : op_infile.get_value().c_str());
        return false;
    }
    return true;
}
