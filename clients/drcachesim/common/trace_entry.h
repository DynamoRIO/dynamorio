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

/* This is the binary data format for what we send through IPC between the
 * memory tracing clients running inside the application(s) and the simulator
 * process.
 * We aren't bothering to pack it as it won't be over the network or persisted.
 * It's already arranged to minimize padding.
 * We do save space using heterogenous data via the type field to send
 * thread id data only periodically rather than paying for the cost of a
 * thread id field in every entry.
 */

#ifndef _TRACE_ENTRY_H_
#define _TRACE_ENTRY_H_ 1

#include <stdint.h>

typedef uintptr_t addr_t;

// XXX: if we want to rely on a recent C++ standard we could try to get
// this enum to be just 2 bytes instead of an int and give it qualified names.
typedef enum {
    // XXX: if we want to include OP_ opcode values, we should have them occupy
    // the first 1,000 or so values and use OP_AFTER_LAST for the first of the
    // values below.  That would require including dr_api.h though.  For now we
    // assume we don't need the opcode.

    // These entries describe a memory reference as data:
    TRACE_TYPE_READ,
    TRACE_TYPE_WRITE,
    TRACE_TYPE_PREFETCH,

    // These entries describe an instruction fetch memory reference.
    // The trace stream always has the instr fetch prior to data refs,
    // which the reader can use to obtain the PC for data references.
    TRACE_TYPE_INSTR,

    // A cache flush:
    TRACE_TYPE_FLUSH,

    // These entries indicate that all subsequent memory references (until the
    // next entry of this type) came from the thread whose id is in the addr
    // field:
    TRACE_TYPE_THREAD,

    // These entries indicate which process the current thread belongs to.
    // The process id is in the addr field.
    TRACE_TYPE_PID,
} trace_type_t;

// Each trace entry is a <type, size, addr> tuple representing a memory reference
// or some other data according to the type field.
typedef struct _trace_entry_t {
    unsigned short type; // 2 bytes: trace_type_t
    unsigned short size; // 2 bytes: mem ref size or instr length
    addr_t addr;         // 4/8 bytes: mem ref addr, instr pc, tid, pid
} trace_entry_t;

#endif /* _TRACE_ENTRY_H_ */
