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

/* analysis_tool: represent a memory trace analysis tool.
 */

#ifndef _ANALYSIS_TOOL_H_
#define _ANALYSIS_TOOL_H_ 1

/**
 * @file drmemtrace/analysis_tool.h
 * @brief DrMemtrace analysis tool base class.
 */

// To support installation of headers for analysis tools into a single
// separate directory we omit common/ here and rely on -I.
#include "memref.h"

/**
 * The base class for a tool that analyzes a trace.  A new tool should subclass this
 * class.  Two key functions should be overridden: process_memref(), which operates
 * on a single trace entry, and print_results(), which is called just once after
 * processing the entire trace and should present the results of analysis.  In the
 * default mode of operation, the #analyzer_t class iterates over the trace and calls
 * the process_memref() function of each tool.  An alternative mode is supported
 * which exposes the iterator and allows a separate control infrastructure to be
 * built.
 */
class analysis_tool_t
{
 public:
    /**
     * Errors encountered during the constructor will set the success flag, which should
     * be queried via operator!.
     */
    analysis_tool_t() : success(true) {};
    virtual ~analysis_tool_t() {}; /**< Destructor. */
    /** Returns whether the tool was created successfully. */
    virtual bool operator!() { return !success; }
    /**
     * The heart of an analysis tool, this routine operates on a single trace entry and
     * takes whatever actions the tool needs to perform its analysis.
     * The return value indicates whether it was successful or there was an error.
     */
    virtual bool process_memref(const memref_t &memref) = 0;
    /**
     * This routine reports the results of the trace analysis.
     * The return value indicates whether it was successful or there was an error.
     */
    virtual bool print_results() = 0;
 protected:
    bool success;
};

#endif /* _ANALYSIS_TOOL_H_ */
