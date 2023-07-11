/* **********************************************************
 * Copyright (c) 2014-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "instrument.h"

#define APP instrlist_meta_append
#define PRE instrlist_meta_preinsert

#define OPND_ARG1 opnd_create_reg(DR_REG_R0)

/* Load DR's TLS base to dr_reg_stolen via reg_base */
static void
insert_load_dr_tls_base(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t reg_base)
{
#ifdef AARCH64
    /* Load TLS base from user-mode thread pointer/ID register:
     * mrs reg_base, tpidr_el0
     */
    PRE(ilist, where,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(reg_base),
                         opnd_create_reg(LIB_SEG_TLS)));
#else // ARM
    /* load TLS base from user-read-only-thread-ID register
     * mrc p15, 0, reg_base, c13, c0, 3
     */
    PRE(ilist, where,
        INSTR_CREATE_mrc(dcontext, opnd_create_reg(reg_base),
                         OPND_CREATE_INT(USR_TLS_COPROC_15), OPND_CREATE_INT(0),
                         opnd_create_reg(DR_REG_CR13), opnd_create_reg(DR_REG_CR0),
                         OPND_CREATE_INT(USR_TLS_REG_OPCODE)));
#endif
    /* ldr dr_reg_stolen, [reg_base, DR_TLS_BASE_OFFSET] */
    PRE(ilist, where,
        XINST_CREATE_load(dcontext, opnd_create_reg(dr_reg_stolen),
                          OPND_CREATE_MEMPTR(reg_base, DR_TLS_BASE_OFFSET)));
}

/* Having only one thread register (TPIDRURO for ARM, TPIDR_EL0 for AARCH64) shared
 * between app and DR, we steal a register for DR's TLS base in the code cache,
 * and store DR's TLS base into an private lib's TLS slot for accessing in C code.
 * On entering the code cache (fcache_enter):
 * - grab gen routine's parameter dcontext and put it into REG_DCXT
 * - load DR's TLS base into dr_reg_stolen from privlib's TLS
 */
void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
#ifdef UNIX
    instr_t *no_signals = INSTR_CREATE_label(dcontext);
    /* save callee-saved reg in case we return for a signal */
    APP(ilist,
        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_R1),
                          opnd_create_reg(REG_DCXT)));
#endif
    ASSERT_NOT_IMPLEMENTED(!absolute &&
                           !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
    /* grab gen routine's parameter dcontext and put it into REG_DCXT */
    APP(ilist, XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT), OPND_ARG1));
#ifdef UNIX
    APP(ilist,
        INSTR_CREATE_ldrsb(dcontext, opnd_create_reg(DR_REG_R2),
                           OPND_DC_FIELD(absolute, dcontext, OPSZ_1, SIGPENDING_OFFSET)));
    APP(ilist,
        XINST_CREATE_cmp(dcontext, opnd_create_reg(DR_REG_R2), OPND_CREATE_INT8(0)));
    APP(ilist,
        XINST_CREATE_jump_cond(dcontext, DR_PRED_LE, opnd_create_instr(no_signals)));
    /* restore callee-saved reg */
    APP(ilist,
        XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT),
                          opnd_create_reg(DR_REG_R1)));
    APP(ilist,
        IF_AARCH64_ELSE(INSTR_CREATE_br, INSTR_CREATE_bx)(dcontext,
                                                          opnd_create_reg(DR_REG_LR)));
    APP(ilist, no_signals);
#endif
    /* set up stolen reg */
    insert_load_dr_tls_base(dcontext, ilist, NULL /*append*/, SCRATCH_REG0);
}
