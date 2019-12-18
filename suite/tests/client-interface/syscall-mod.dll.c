/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#include "dr_api.h"
#ifdef MACOS
#    include <sys/syscall.h>
#else
#    include <syscall.h>
#endif

/* Tests removing a system call. */

#define MINSERT instrlist_meta_preinsert

#ifdef AARCH64
#    define SYSCALL_ARG_REG DR_REG_X8
#    define SYSCALL_RES_REG DR_REG_X0
#else
#    define SYSCALL_ARG_REG REG_EAX
#    define SYSCALL_RES_REG REG_EAX
#endif

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;
    instr_t *next_instr;
    ptr_int_t value;
    reg_t in_reg = -1;

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (instr_is_mov_constant(instr, &value) &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == SYSCALL_ARG_REG &&
            opnd_is_immed_int(instr_get_src(instr, 0)))
            in_reg = opnd_get_immed_int(instr_get_src(instr, 0));
        if (instr_is_syscall(instr) && in_reg == SYS_getpid) {
            instr_t *myval = XINST_CREATE_load_int(
                drcontext, opnd_create_reg(SYSCALL_RES_REG), OPND_CREATE_INT32(-7));
            instr_set_translation(myval, instr_get_app_pc(instr));
            instrlist_preinsert(bb, instr, myval);
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        }
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}
