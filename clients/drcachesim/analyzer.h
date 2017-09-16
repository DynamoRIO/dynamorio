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

/* analyzer: represent a memory trace analysis tool that operates only
 * on a file.  We separate this from analyzer_multi, which can operate online
 * or on a raw trace file, to avoid needing to link in DR itself.
 */

#ifndef _ANALYZER_H_
#define _ANALYZER_H_ 1

#include <iterator>
#include <string>
#include "analysis_tool.h"
#include "reader.h"

class analyzer_t
{
 public:
    // Usage: errors encountered during a constructor will set a flag that should
    // be queried via operator!.
    analyzer_t();
    virtual ~analyzer_t();
    virtual bool operator!();

    // We have two usage models: one where there are multiple tools and the
    // trace iteration is performed by analyzer_t, and another where a single
    // tool controls the iteration.

    // The default, simpler, multiple-tool-supporting model uses this constructor.
    // The analyzer will reference the tools array passed in during its lifetime:
    // it does not make a copy.
    // The user must free them afterward.
    analyzer_t(const std::string &trace_file, analysis_tool_t **tools,
               int num_tools);
    virtual bool run();
    virtual bool print_stats();

    // The alternate usage model exposes the iterator to a single tool.
    analyzer_t(const std::string &trace_file);
    // As the iterator is more heavyweight than regular container iterators
    // we hold it internally and return a reference.  Copying will fail to compile
    // as reader_t is virtual, reminding the caller of begin() to use a reference.
    // This usage model supports only a single user of the iterator: the
    // multi-tool model above should be used if multiple tools are involved.
    virtual reader_t & begin();
    virtual reader_t & end();

 protected:
    bool init_file_reader(const std::string &trace_file);

    // This finalizes the trace_iter setup.  It can block and is meant to be
    // called at the top of run() or begin().
    bool start_reading();

    bool success;
    reader_t *trace_iter;
    reader_t *trace_end;
    int num_tools;
    analysis_tool_t **tools;
};

#endif /* _ANALYZER_H_ */
