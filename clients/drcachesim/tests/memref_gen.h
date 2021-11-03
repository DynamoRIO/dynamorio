/* **********************************************************
 * Copyright (c) 2021 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _MEMREF_GEN_
#define _MEMREF_GEN_ 1

#include "../common/memref.h"

namespace {

inline memref_t
gen_data(memref_tid_t tid, bool load, addr_t addr, size_t size)
{
    memref_t memref = {};
    memref.instr.type = load ? TRACE_TYPE_READ : TRACE_TYPE_WRITE;
    memref.instr.tid = tid;
    memref.instr.addr = addr;
    memref.instr.size = size;
    return memref;
}

inline memref_t
gen_instr_type(trace_type_t type, memref_tid_t tid, addr_t pc)
{
    memref_t memref = {};
    memref.instr.type = type;
    memref.instr.tid = tid;
    memref.instr.addr = pc;
    memref.instr.size = 1;
    return memref;
}

inline memref_t
gen_instr(memref_tid_t tid, addr_t pc)
{
    return gen_instr_type(TRACE_TYPE_INSTR, tid, pc);
}

inline memref_t
gen_branch(memref_tid_t tid, addr_t pc)
{
    return gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
}

inline memref_t
gen_marker(memref_tid_t tid, trace_marker_type_t type, uintptr_t val)
{
    memref_t memref = {};
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.tid = tid;
    memref.marker.marker_type = type;
    memref.marker.marker_value = val;
    return memref;
}

inline memref_t
gen_exit(memref_tid_t tid)
{
    memref_t memref = {};
    memref.instr.type = TRACE_TYPE_THREAD_EXIT;
    memref.instr.tid = tid;
    return memref;
}

} // namespace

#endif /* _MEMREF_GEN_ */
