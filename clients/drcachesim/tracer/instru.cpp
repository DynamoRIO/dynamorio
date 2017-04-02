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

/* instru: instrumentation utilities.
 */

#include "dr_api.h"
#include "instru.h"
#include "../common/trace_entry.h"

unsigned short
instru_t::instr_to_instr_type(instr_t *instr)
{
    if (instr_is_call_direct(instr))
        return TRACE_TYPE_INSTR_DIRECT_CALL;
    if (instr_is_call_indirect(instr))
        return TRACE_TYPE_INSTR_INDIRECT_CALL;
    if (instr_is_return(instr))
        return TRACE_TYPE_INSTR_RETURN;
    if (instr_is_ubr(instr))
        return TRACE_TYPE_INSTR_DIRECT_JUMP;
    if (instr_is_mbr(instr)) // But not a return or call.
        return TRACE_TYPE_INSTR_INDIRECT_JUMP;
    if (instr_is_cbr(instr))
        return TRACE_TYPE_INSTR_CONDITIONAL_JUMP;
    return TRACE_TYPE_INSTR;
}

unsigned short
instru_t::instr_to_prefetch_type(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    DR_ASSERT(instr_is_prefetch(instr));
    switch (opcode) {
#ifdef X86
    case OP_prefetcht0:
        return TRACE_TYPE_PREFETCHT0;
    case OP_prefetcht1:
        return TRACE_TYPE_PREFETCHT1;
    case OP_prefetcht2:
        return TRACE_TYPE_PREFETCHT2;
    case OP_prefetchnta:
        return TRACE_TYPE_PREFETCHNTA;
#endif
#ifdef ARM
    case OP_pld:
        return TRACE_TYPE_PREFETCH_READ;
    case OP_pldw:
        return TRACE_TYPE_PREFETCH_WRITE;
    case OP_pli:
        return TRACE_TYPE_PREFETCH_INSTR;
#endif
    default:
        return TRACE_TYPE_PREFETCH;
    }
}

bool
instru_t::instr_is_flush(instr_t *instr)
{
    // Assuming we won't see any privileged instructions.
#ifdef X86
    if (instr_get_opcode(instr) == OP_clflush)
        return true;
#endif
    return false;
}
