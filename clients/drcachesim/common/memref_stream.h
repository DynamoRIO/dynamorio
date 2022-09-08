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

/* memref_stream: represent a memory trace analysis tool.
 */

#ifndef _MEMREF_STREAM_H_
#define _MEMREF_STREAM_H_ 1

/**
 * @file drmemtrace/memref_stream.h
 * @brief DrMemtrace interface for obtaining information from analysis
 * tools on the full stream of memory reference records.
 */

/**
 * This is an interface for obtaining information from analysis tools
 * on the full stream of memory reference records.
 */
class memref_stream_t {
public:
    /** Destructor. */
    virtual ~memref_stream_t()
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

#endif /* _MEMREF_STREAM_H_ */
