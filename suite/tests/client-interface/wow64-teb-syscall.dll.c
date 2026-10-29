/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "dr_api.h"
#include "client_tools.h"

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    if (!dr_is_wow64()) {
        dr_fprintf(STDERR, "WOW64 unavailable\n");
        return;
    }
    void *dc = dr_get_current_drcontext();
    /* Decode private wrappers using the TEB gate, including non-syscall controls. */
    byte gate[] = { 0x64, 0xff, 0x15, 0xc0, 0x00, 0x00, 0x00 };
    instr_t *inst = instr_create(dc);
    ASSERT(decode(dc, gate, inst) == gate + sizeof(gate));
    ASSERT(instr_is_call_indirect(inst));
    ASSERT(instr_is_syscall(inst));
    instr_reset(dc, inst);
    gate[3] = 0xc4;
    ASSERT(decode(dc, gate, inst) == gate + sizeof(gate));
    ASSERT(!instr_is_syscall(inst));
    instr_reset(dc, inst);
    gate[0] = 0x65;
    gate[3] = 0xc0;
    ASSERT(decode(dc, gate, inst) == gate + sizeof(gate));
    ASSERT(!instr_is_syscall(inst));
    instr_destroy(dc, inst);
    dr_fprintf(STDERR, "TEB syscall gate decoded\n");
}
