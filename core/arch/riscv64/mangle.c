/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "arch.h"
#include "instr_create_shared.h"
#include "instrument.h" /* instrlist_meta_preinsert */

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define PRE instrlist_meta_preinsert

void
mangle_arch_init(void)
{
    /* Nothing. */
}

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *instr)
{
    /* Nothing. */
}

uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr, uint alignment,
                          opnd_t push_pc, reg_id_t scratch)
{
    uint dstack_offs = 0;
    int max_offs;

    if (cci == NULL)
        cci = &default_clean_call_info;
    ASSERT(proc_num_simd_registers() == MCXT_NUM_SIMD_SLOTS);

    /* a0 is used to save and restore the pc and csr registers. */
    cci->reg_skip[DR_REG_A0 - DR_REG_START_GPR] = false;

    max_offs = get_clean_call_switch_stack_size();

    PRE(ilist, instr,
        INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_SP),
                          opnd_create_reg(DR_REG_SP),
                          opnd_add_flags(opnd_create_immed_int(-max_offs, OPSZ_12b),
                                         DR_OPND_IMM_PRINT_DECIMAL)));

    /* Push GPRs. */
    for (int i = 1; i < DR_NUM_GPR_REGS; i += 1) {
        if (cci->reg_skip[i])
            continue;

        PRE(ilist, instr,
            INSTR_CREATE_sd(dcontext,
                            opnd_add_flags(opnd_create_base_disp(
                                               DR_REG_SP, DR_REG_NULL, 0,
                                               dstack_offs + i * sizeof(reg_t), OPSZ_8),
                                           DR_OPND_IMM_PRINT_DECIMAL),
                            opnd_create_reg(DR_REG_ZERO + i)));
    }

    dstack_offs += DR_NUM_GPR_REGS * XSP_SZ;

    if (opnd_is_immed_int(push_pc)) {
        PRE(ilist, instr,
            XINST_CREATE_load_int(dcontext, opnd_create_reg(DR_REG_A0), push_pc));
    } else {
        ASSERT(opnd_is_reg(push_pc));
        reg_id_t push_pc_reg = opnd_get_reg(push_pc);
        /* push_pc opnd is already pushed on the stack. */
        PRE(ilist, instr,
            INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_A0),
                            OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(push_pc_reg))));
    }

    PRE(ilist, instr,
        INSTR_CREATE_sd(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                        opnd_create_reg(DR_REG_A0)));

    dstack_offs += XSP_SZ;

    /* Push FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i += 1) {
        PRE(ilist, instr,
            INSTR_CREATE_fsd(dcontext,
                             opnd_add_flags(opnd_create_base_disp(
                                                DR_REG_SP, DR_REG_NULL, 0,
                                                dstack_offs + i * sizeof(reg_t), OPSZ_8),
                                            DR_OPND_IMM_PRINT_DECIMAL),
                             opnd_create_reg(DR_REG_F0 + i)));
    }

    dstack_offs += DR_NUM_FPR_REGS * XSP_SZ;

    /* csrr a0, fcsr */
    PRE(ilist, instr,
        INSTR_CREATE_csrrs(dcontext, opnd_create_reg(DR_REG_A0),
                           opnd_create_reg(DR_REG_X0),
                           /* FIXME i#3544: Use register. */
                           opnd_create_immed_int(0x003 /* fcsr */, OPSZ_12b)));

    PRE(ilist, instr,
        INSTR_CREATE_sd(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                        opnd_create_reg(DR_REG_A0)));

    dstack_offs += XSP_SZ;

    /* Keep mcontext shape. */
    dstack_offs += (proc_num_simd_registers() * sizeof(dr_simd_t));

    /* Restore the registers we used. */
    PRE(ilist, instr,
        INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_A0),
                        OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(DR_REG_A0))));

    return dstack_offs;
}

void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment)
{
    if (cci == NULL)
        cci = &default_clean_call_info;
    uint current_offs;
    current_offs =
        get_clean_call_switch_stack_size() - proc_num_simd_registers() * sizeof(reg_t);

    PRE(ilist, instr,
        INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_A0),
                        OPND_CREATE_MEM64(DR_REG_SP, current_offs)));

    /* csrw a0, fcsr */
    PRE(ilist, instr,
        INSTR_CREATE_csrrw(dcontext, opnd_create_reg(DR_REG_X0),
                           opnd_create_reg(DR_REG_A0),
                           opnd_create_immed_int(/*fcsr*/ 0x003, OPSZ_12b)));

    current_offs -= DR_NUM_FPR_REGS * XSP_SZ;

    /* Pop FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i += 1) {
        PRE(ilist, instr,
            INSTR_CREATE_fld(dcontext, opnd_create_reg(DR_REG_F0 + i),
                             opnd_add_flags(opnd_create_base_disp(
                                                DR_REG_SP, DR_REG_NULL, 0,
                                                current_offs + i * sizeof(reg_t), OPSZ_8),
                                            DR_OPND_IMM_PRINT_DECIMAL)));
    }

    /* Skip pc field. */
    current_offs -= XSP_SZ;

    current_offs -= DR_NUM_GPR_REGS * XSP_SZ;

    /* Pop GPRs. */
    for (int i = 1; i < DR_NUM_GPR_REGS; i += 1) {
        if (cci->reg_skip[i])
            continue;

        PRE(ilist, instr,
            INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_X0 + i),
                            opnd_add_flags(opnd_create_base_disp(
                                               DR_REG_SP, DR_REG_NULL, 0,
                                               current_offs + i * sizeof(reg_t), OPSZ_8),
                                           DR_OPND_IMM_PRINT_DECIMAL)));
    }
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return regular;
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save, byte *encode_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/*###########################################################################
 *   MANGLING ROUTINES
 */

/* This is *not* a hot-patchable patch: i.e., it is subject to races. */
void
patch_mov_immed_arch(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                     instr_t *last)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
                    instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

#ifdef UNIX
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
float_pc_update(dcontext_t *dcontext)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* END OF MANGLING ROUTINES
 *###########################################################################
 */
