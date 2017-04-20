/* **********************************************************
 * Copyright (c) 2016-2017 ARM Limited. All rights reserved.
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

/* file "clean_call_opt.c" */

#include "../globals.h"
#include "arch.h"
#include "../clean_call_opt.h"

#ifdef CLIENT_INTERFACE

void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    uint i, num_regparm;

    /* XXX implement bitset for optimisation */
    memset(ci->reg_used, 0, sizeof(bool) * NUM_GP_REGS);
    /* Scratch registers used for setting up the jump to the clean callee. */
    ci->reg_used[SCRATCH_REG0] = true;
    ci->reg_used[SCRATCH_REG1] = true;
    ci->reg_used[DR_REG_X11 - DR_REG_START_GPR] = true;

    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {

        /* General purpose registers */
        for (i = 0; i < NUM_GP_REGS; i++) {
            reg_id_t reg = DR_REG_START_GPR + (reg_id_t)i;
            if (!ci->reg_used[i] &&
                instr_uses_reg(instr, reg)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses REG %s at "PFX"\n",
                    ci->start, reg_names[reg],
                    instr_get_app_pc(instr));
                ci->reg_used[i] = true;
                callee_info_reserve_slot(ci, SLOT_REG, reg);
            }
        }
    }

    num_regparm = MIN(ci->num_args, NUM_REGPARM);
    for (i = 0; i < num_regparm; i++) {
        reg_id_t reg = regparms[i];
        if (!ci->reg_used[reg - DR_REG_START_GPR]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" uses REG %s for arg passing\n",
                ci->start, reg_names[reg]);
            ci->reg_used[reg - DR_REG_START_GPR] = true;
            callee_info_reserve_slot(ci, SLOT_REG, reg);
        }
    }
    /* FIXME i#1621: the following checks are still missing:
     *    - analysis of SIMD registers
     *    - analysis of eflags (depends on i#2263)
     */
}

void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#1621: NYI on AArch64
     * Non-essential for cleancall_opt=1 optimizations.
     */
}

void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#1621: NYI on AArch64
     * Non-essential for cleancall_opt=1 optimizations.
     */
}

app_pc
check_callee_instr_level2(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc,
                          app_pc cur_pc, app_pc tgt_pc)
{
    /* FIXME i#1569: For opt level greater than 1, we abort. */
    return NULL;
}

bool
check_callee_ilist_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
    return false;
}

void
analyze_clean_call_aflags(dcontext_t *dcontext,
                          clean_call_info_t *cci, instr_t *where)
{
    /* FIXME i#1621: NYI on AArch64
     * Non-essential for cleancall_opt=1 optimizations.
     */
}

void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci,
                       instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci,
                        instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569: NYI on AArch64 */
}

#endif /* CLIENT_INTERFACE */
