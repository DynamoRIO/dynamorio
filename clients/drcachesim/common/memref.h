/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* This is the data format that the simulator takes as input */

#ifndef _MEMREF_H_
#define _MEMREF_H_ 1

#include <stdint.h>
#include <stddef.h> // for size_t
#include "trace_entry.h"

// On some platforms, like MacOS, a thread id is 64 bits.
// We just make both 64 bits to cover all our bases.
typedef int_least64_t memref_pid_t;
typedef int_least64_t memref_tid_t;

typedef struct _memref_t {
    memref_pid_t pid;
    memref_tid_t tid;
    unsigned short type; // trace_type_t

    // Fields below are here at not valid for TRACE_TYPE_THREAD_EXIT.

    size_t size;
    addr_t addr;

    // The pc field is only used for read, write, and prefetch entries.
    // XXX: should we remove it from here and have the simulator compute it
    // from instr entries?  Though if the user turns off icache simulation
    // it may be better to keep it as a field here and have the reader
    // fill it in for us.
    addr_t pc;
} memref_t;

#endif /* _MEMREF_H_ */
