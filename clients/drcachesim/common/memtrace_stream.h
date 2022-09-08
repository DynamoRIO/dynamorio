/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* memtrace_stream: an interface to access aspects of the full stream of memory
 * trace records.
 *
 * We had considered other avenues for analysis_tool_t to obtain things like
 * the record and instruction ordinals within the stream, in the presence of
 * skipping: we could add fields to memref but we'd either have to append
 * and have them at different offsets for each type or we'd have to break
 * compatibility to prepend every time we added more; or we could add parameters
 * to process_memref().  Passing an interface to the init routines seems
 * the simplest and most flexible.
 */

#ifndef _MEMTRACE_STREAM_H_
#define _MEMTRACE_STREAM_H_ 1

/**
 * @file drmemtrace/memtrace_stream.h
 * @brief DrMemtrace interface for obtaining information from analysis
 * tools on the full stream of memory reference records.
 */

/**
 * This is an interface for obtaining information from analysis tools
 * on the full stream of memory reference records.
 */
class memtrace_stream_t {
public:
    /** Destructor. */
    virtual ~memtrace_stream_t()
    {
    }
    /**
     * Returns the count of #memref_t records from the start of the trace to this point.
     * This includes records skipped over and not presented to any tool.
     */
    virtual uint64_t
    get_record_ordinal() = 0;
    /**
     * Returns the count of instructions from the start of the trace to this point.
     * This includes instructions skipped over and not presented to any tool.
     */
    virtual uint64_t
    get_instruction_ordinal() = 0;
};

#endif /* _MEMTRACE_STREAM_H_ */
