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
 */

#ifndef _MEMREF_H_
#define _MEMREF_H_ 1

typedef void * addr_t;

enum {
    REF_TYPE_READ  = 0,
    REF_TYPE_WRITE = 1,
};

/* Each mem_ref_t is a <tid, type, size, addr> entry representing a memory reference
 * instruction or the reference information, e.g.:
 * - mem ref instr: { type = 42 (call), size = 5, addr = 0x7f59c2d002d3 }
 * - mem ref info:  { type = 1 (write), size = 8, addr = 0x7ffeacab0ec8 }
 */
typedef struct _memref_t {
    // XXX: for MacOS and Windows thread identifiers are 64-bit.
    // Plus, we probably want to know which process as well, so we may want
    // to switch to an internal id, maybe counting the processes and threads
    // (ideally with name strings somewhere?).  Or we can use the thread id
    // and store info on which process in a global file.
    //
    // XXX i#1703: we could get rid of the id from each entry and have a
    // separate special entry saying "all subsequent entries belong to this thread".
    unsigned int id;     // 4 bytes: thread identifier
    unsigned short type; // 2 bytes: r(0), w(1), or opcode (0/1 are invalid opcodes)
    unsigned short size; // 2 bytes: mem ref size or instr length
    addr_t addr;         // 4/8 bytes: mem ref addr or instr pc
} memref_t;

#endif /* _MEMREF_H_ */
