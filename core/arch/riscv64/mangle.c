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

/* TODO i#3544: Think of a better way to represent CSR in the IR, maybe as registers? */
/* Number of the fcsr register. */
#define FCSR 0x003

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
    for (int i = 1; i < DR_NUM_GPR_REGS; i++) {
        if (cci->reg_skip[i])
            continue;

        PRE(ilist, instr,
            INSTR_CREATE_sd(
                dcontext,
                opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                     dstack_offs + i * XSP_SZ, OPSZ_8),
                               DR_OPND_IMM_PRINT_DECIMAL),
                opnd_create_reg(DR_REG_ZERO + i)));
    }

    dstack_offs += DR_NUM_GPR_REGS * XSP_SZ;

    if (opnd_is_immed_int(push_pc)) {
        PRE(ilist, instr,
            XINST_CREATE_load_int(dcontext, opnd_create_reg(DR_REG_A0), push_pc));
        PRE(ilist, instr,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                            opnd_create_reg(DR_REG_A0)));
    } else {
        ASSERT(opnd_is_reg(push_pc));
        /* push_pc is still holding the PC value. */
        PRE(ilist, instr,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                            push_pc));
    }

    dstack_offs += XSP_SZ;

    /* Push FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_fsd(
                dcontext,
                opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                     dstack_offs + i * XSP_SZ, OPSZ_8),
                               DR_OPND_IMM_PRINT_DECIMAL),
                opnd_create_reg(DR_REG_F0 + i)));
    }

    dstack_offs += DR_NUM_FPR_REGS * XSP_SZ;

    /* csrr a0, fcsr */
    PRE(ilist, instr,
        INSTR_CREATE_csrrs(dcontext, opnd_create_reg(DR_REG_A0),
                           opnd_create_reg(DR_REG_X0),
                           /* FIXME i#3544: Use register. */
                           opnd_create_immed_int(FCSR, OPSZ_12b)));

    PRE(ilist, instr,
        INSTR_CREATE_sd(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                        opnd_create_reg(DR_REG_A0)));

    dstack_offs += XSP_SZ;

    /* TODO i#3544: No support for SIMD on RISC-V so far, this is to keep the mcontext
     * shape. */
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
        get_clean_call_switch_stack_size() - proc_num_simd_registers() * XSP_SZ;

    PRE(ilist, instr,
        INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_A0),
                        OPND_CREATE_MEM64(DR_REG_SP, current_offs)));

    /* csrw a0, fcsr */
    PRE(ilist, instr,
        INSTR_CREATE_csrrw(dcontext, opnd_create_reg(DR_REG_X0),
                           opnd_create_reg(DR_REG_A0),
                           opnd_create_immed_int(FCSR, OPSZ_12b)));

    current_offs -= DR_NUM_FPR_REGS * XSP_SZ;

    /* Pop FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_fld(
                dcontext, opnd_create_reg(DR_REG_F0 + i),
                opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                     current_offs + i * XSP_SZ, OPSZ_8),
                               DR_OPND_IMM_PRINT_DECIMAL)));
    }

    /* Skip pc field. */
    current_offs -= XSP_SZ;

    current_offs -= DR_NUM_GPR_REGS * XSP_SZ;

    /* Pop GPRs. */
    for (int i = 1; i < DR_NUM_GPR_REGS; i++) {
        if (cci->reg_skip[i])
            continue;

        PRE(ilist, instr,
            INSTR_CREATE_ld(
                dcontext, opnd_create_reg(DR_REG_X0 + i),
                opnd_add_flags(opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                     current_offs + i * XSP_SZ, OPSZ_8),
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
    /* A scratch register is required for holding the jump target. */
    ASSERT(scratch != REG_NULL);

    /* Load target into scratch register. */
    insert_mov_immed_ptrsz(dcontext,
                           (ptr_int_t)PC_AS_JMP_TGT(dr_get_isa_mode(dcontext), target),
                           opnd_create_reg(scratch), ilist, where, NULL, NULL);

    /* Even if it's a call, if it doesn't return, we use jump. */
    if (!jmp && returns) {
        /* jalr ra, 0(scratch) */
        PRE(ilist, where, XINST_CREATE_call_reg(dcontext, opnd_create_reg(scratch)));
    } else {
        /* jalr zero, 0(scratch) */
        PRE(ilist, where, XINST_CREATE_jump_reg(dcontext, opnd_create_reg(scratch)));
    }

    /* Always use an indirect branch for RISC-V. */
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
    ASSERT(instr_get_opcode(instr) == OP_jal);
    ASSERT(opnd_is_pc(instr_get_target(instr)));
    insert_mov_immed_ptrsz(dcontext, get_call_return_address(dcontext, ilist, instr),
                           instr_get_dst(instr, 0), ilist, instr, NULL, NULL);
    instrlist_remove(ilist, instr); /* remove OP_jal */
    instr_destroy(dcontext, instr);
    return next_instr;
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* The mangling is identical. */
    return mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    /* The mangling is identical. */
    mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
}

instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    ASSERT(instr_is_mbr(instr));
    opnd_t dst = instr_get_dst(instr, 0), target = instr_get_target(instr);
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    ASSERT(opnd_is_reg(target));

    ASSERT_NOT_IMPLEMENTED(!opnd_same(target, opnd_create_reg(DR_REG_TP)));
    ASSERT_NOT_IMPLEMENTED(!opnd_same(dst, opnd_create_reg(DR_REG_TP)));
    ASSERT_NOT_IMPLEMENTED(!opnd_same(dst, opnd_create_reg(dr_reg_stolen)));

    if (opnd_same(target, opnd_create_reg(dr_reg_stolen))) {
        /* If the target reg is dr_reg_stolen, the app value is in TLS. */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, IBL_TARGET_REG, TLS_REG_STOLEN_SLOT));
        if (opnd_get_immed_int(instr_get_src(instr, 1)) != 0) {
            PRE(ilist, instr,
                XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                 instr_get_src(instr, 1)));
        }
    } else
        PRE(ilist, instr,
            XINST_CREATE_add_2src(dcontext, opnd_create_reg(IBL_TARGET_REG), target,
                                  instr_get_src(instr, 1)));

    if (opnd_get_reg(dst) != DR_REG_ZERO) {
        insert_mov_immed_ptrsz(dcontext, get_call_return_address(dcontext, ilist, instr),
                               dst, ilist, next_instr, NULL, NULL);
    }

    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}

instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    opnd_t dst = instr_get_dst(instr, 0);
    app_pc tgt;
    ASSERT(instr_get_opcode(instr) == OP_auipc);
    ASSERT(instr_has_rel_addr_reference(instr));
    instr_get_rel_data_or_instr_target(instr, &tgt);
    ASSERT(opnd_is_reg(dst));
    ASSERT(opnd_is_rel_addr(instr_get_src(instr, 0)));

    ASSERT_NOT_IMPLEMENTED(!instr_uses_reg(instr, DR_REG_TP));

    if (instr_uses_reg(instr, dr_reg_stolen)) {
        dst = opnd_create_reg(DR_REG_A0);
        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, DR_REG_A0, TLS_REG0_SLOT));
    }

    insert_mov_immed_ptrsz(dcontext, (ptr_int_t)tgt, dst, ilist, next_instr, NULL, NULL);

    if (instr_uses_reg(instr, dr_reg_stolen)) {
        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, DR_REG_A0, TLS_REG_STOLEN_SLOT));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, DR_REG_A0, TLS_REG0_SLOT));
    }

    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
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
