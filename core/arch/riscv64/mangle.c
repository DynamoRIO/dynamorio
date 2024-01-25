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

/* TODO i#3544: Think of a better way to represent these fields in the IR. */
/* Volume I: RISC-V Unprivileged ISA V20191213.
 * Page 26: */
#define FENCE_ORDERING_RW 0x3
#define FENCE_FM_NONE 0x0
/* Page 48: */
#define LRSC_ORDERING_RL_MASK 0x1
#define LRSC_ORDERING_AQ_MASK 0x2

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
    int dstack_middle_offs;
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
                          opnd_create_immed_int(-max_offs, OPSZ_12b)));

    /* Skip X0 slot. */
    dstack_offs += XSP_SZ;

    /* Push GPRs. */
    for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
        if (cci->reg_skip[i])
            continue;

        /* Uses c.sdsp to save space, see -max_bb_instrs option, same below. */
        PRE(ilist, instr,
            INSTR_CREATE_c_sdsp(dcontext,
                                opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                      dstack_offs + i * XSP_SZ, OPSZ_8),
                                opnd_create_reg(DR_REG_START_GPR + i)));
    }

    dstack_offs += DR_NUM_GPR_REGS * XSP_SZ;

    if (opnd_is_immed_int(push_pc)) {
        PRE(ilist, instr,
            XINST_CREATE_load_int(dcontext, opnd_create_reg(DR_REG_A0), push_pc));
        PRE(ilist, instr,
            INSTR_CREATE_c_sdsp(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                                opnd_create_reg(DR_REG_A0)));
    } else {
        ASSERT(opnd_is_reg(push_pc));
        /* push_pc is still holding the PC value. */
        PRE(ilist, instr,
            INSTR_CREATE_c_sdsp(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                                push_pc));
    }

    dstack_offs += XSP_SZ;
    /* XXX: c.sdsp/c.fsdsp has a zero-extended 9-bit offset, which is not enough for our
     * usage. We use dstack_middle_offs to mitigate this issue.
     */
    dstack_middle_offs = dstack_offs;
    dstack_offs = 0;
    PRE(ilist, instr,
        INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_SP),
                          opnd_create_reg(DR_REG_SP),
                          opnd_create_immed_int(dstack_middle_offs, OPSZ_12b)));

    /* Push FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_c_fsdsp(dcontext,
                                 opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                       dstack_offs + i * XSP_SZ, OPSZ_8),
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
        INSTR_CREATE_c_sdsp(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                            opnd_create_reg(DR_REG_A0)));

    dstack_offs += XSP_SZ;

    /* TODO i#3544: No support for SIMD on RISC-V so far, this is to keep the mcontext
     * shape. */
    dstack_offs += (proc_num_simd_registers() * sizeof(dr_simd_t));

    /* Restore sp. */
    PRE(ilist, instr,
        INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_SP),
                          opnd_create_reg(DR_REG_SP),
                          opnd_create_immed_int(-dstack_middle_offs, OPSZ_12b)));

    /* Restore the registers we used. */
    PRE(ilist, instr,
        INSTR_CREATE_c_ldsp(dcontext, opnd_create_reg(DR_REG_A0),
                            OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(DR_REG_A0))));

    return dstack_offs + dstack_middle_offs;
}

void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment)
{
    if (cci == NULL)
        cci = &default_clean_call_info;
    uint current_offs;
    current_offs = get_clean_call_switch_stack_size() -
        proc_num_simd_registers() * sizeof(dr_simd_t);

    /* sp is the stack pointer, which should not be poped. */
    cci->reg_skip[DR_REG_SP - DR_REG_START_GPR] = true;

    /* XXX: c.sdsp/c.fsdsp has a zero-extended 9-bit offset, which is not enough for our
     * usage.
     */
    ASSERT(current_offs >= DR_NUM_FPR_REGS * XSP_SZ);
    PRE(ilist, instr,
        INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_SP),
                          opnd_create_reg(DR_REG_SP),
                          opnd_create_immed_int(DR_NUM_FPR_REGS * XSP_SZ, OPSZ_12b)));

    current_offs -= XSP_SZ;
    /* Uses c.ldsp to save space, see -max_bb_instrs option, same below. */
    PRE(ilist, instr,
        INSTR_CREATE_c_ldsp(
            dcontext, opnd_create_reg(DR_REG_A0),
            OPND_CREATE_MEM64(DR_REG_SP, current_offs - DR_NUM_FPR_REGS * XSP_SZ)));
    /* csrw a0, fcsr */
    PRE(ilist, instr,
        INSTR_CREATE_csrrw(dcontext, opnd_create_reg(DR_REG_X0),
                           opnd_create_reg(DR_REG_A0),
                           opnd_create_immed_int(FCSR, OPSZ_12b)));

    current_offs -= DR_NUM_FPR_REGS * XSP_SZ;

    /* Pop FPRs. */
    for (int i = 0; i < DR_NUM_FPR_REGS; i++) {
        PRE(ilist, instr,
            INSTR_CREATE_c_fldsp(dcontext, opnd_create_reg(DR_REG_F0 + i),
                                 opnd_create_base_disp(
                                     DR_REG_SP, DR_REG_NULL, 0,
                                     current_offs - DR_NUM_FPR_REGS * XSP_SZ + i * XSP_SZ,
                                     OPSZ_8)));
    }

    /* Restore sp. */
    PRE(ilist, instr,
        INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_SP),
                          opnd_create_reg(DR_REG_SP),
                          opnd_create_immed_int(-DR_NUM_FPR_REGS * XSP_SZ, OPSZ_12b)));

    /* Skip pc field. */
    current_offs -= XSP_SZ;

    current_offs -= DR_NUM_GPR_REGS * XSP_SZ;

    /* Pop GPRs. */
    for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
        if (cci->reg_skip[i])
            continue;

        PRE(ilist, instr,
            INSTR_CREATE_c_ldsp(dcontext, opnd_create_reg(DR_REG_START_GPR + i),
                                opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                      current_offs + i * XSP_SZ,
                                                      OPSZ_8)));
    }
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return regular;
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
    /* XXX i#3544: JAL can target a ±1 MiB range, can we use it for a better performance?
     */
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
    /* Does not apply to RISC-V. */
    return false;
}

#ifdef UNIX
/* Inserts code to handle clone into ilist.
 * instr is the syscall instr itself.
 * Assumes that instructions exist beyond instr in ilist.
 *
 * After the clone syscall, check if a0 is 0, if not, jump to new_thread_dynamo_start() to
 * maintain control of the child.
 */
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /*    ecall
     *    bnez a0, parent
     *    jmp new_thread_dynamo_start
     *  parent:
     *    <post system call, etc.>
     */
    instr_t *in = instr_get_next(instr);
    instr_t *parent = INSTR_CREATE_label(dcontext);
    ASSERT(in != NULL);
    PRE(ilist, in,
        INSTR_CREATE_bne(dcontext, opnd_create_instr(parent), opnd_create_reg(DR_REG_A0),
                         opnd_create_reg(DR_REG_X0)));
    insert_reachable_cti(dcontext, ilist, in, vmcode_get_start(),
                         (byte *)get_new_thread_start(dcontext), true /*jmp*/,
                         false /*!returns*/, false /*!precise*/, DR_REG_A0 /*scratch*/,
                         NULL);
    instr_set_meta(instr_get_prev(in));
    PRE(ilist, in, parent);
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

static reg_id_t
pick_scratch_reg(dcontext_t *dcontext, instr_t *instr, reg_id_t do_not_pick,
                 ushort *scratch_slot DR_PARAM_OUT)
{
    reg_id_t reg = REG_NULL;
    ushort slot = 0;

    for (reg = SCRATCH_REG0, slot = TLS_REG0_SLOT; reg <= SCRATCH_REG_LAST;
         reg++, slot += sizeof(reg_t)) {
        if (!instr_uses_reg(instr, reg) && reg != do_not_pick)
            break;
    }

    if (scratch_slot != NULL)
        *scratch_slot = slot;

    ASSERT(reg != REG_NULL);
    return reg;
}

static void
mangle_stolen_reg_and_tp_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr)
{
    ASSERT_NOT_TESTED();

    ushort slot;
    reg_id_t scratch_reg = DR_REG_NULL;
    opnd_t curop;
    int i;

    ASSERT(!instr_is_meta(instr) &&
           (instr_uses_reg(instr, dr_reg_stolen) || instr_uses_reg(instr, DR_REG_TP)));

    /* If instr uses tp register, we use the app's tp through a scratch register.
     *
     * TODO i#3544: If tp is only used for src, do not spill back into app's TLS;
     * likewise, if it's only used for dst, do not restore it from app's TLS.
     */
    if (instr_uses_reg(instr, DR_REG_TP)) {
        scratch_reg = pick_scratch_reg(dcontext, instr, DR_REG_NULL, &slot);
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg, slot));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch_reg,
                                          os_get_app_tls_base_offset(TLS_REG_LIB)));

        for (i = 0; i < instr_num_dsts(instr); i++) {
            curop = instr_get_dst(instr, i);
            if (opnd_is_reg(curop) && opnd_get_reg(curop) == DR_REG_TP)
                instr_set_dst(instr, i, opnd_create_reg(scratch_reg));
            else if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_TP) {
                instr_set_dst(instr, i,
                              opnd_create_base_disp(scratch_reg, DR_REG_NULL, 0,
                                                    opnd_get_disp(curop),
                                                    opnd_get_size(curop)));
            }
        }
        for (i = 0; i < instr_num_srcs(instr); i++) {
            curop = instr_get_src(instr, i);
            if (opnd_is_reg(curop) && opnd_get_reg(curop) == DR_REG_TP)
                instr_set_src(instr, i, opnd_create_reg(scratch_reg));
            else if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_TP) {
                instr_set_src(instr, i,
                              opnd_create_base_disp(scratch_reg, DR_REG_NULL, 0,
                                                    opnd_get_disp(curop),
                                                    opnd_get_size(curop)));
            }
        }
        instr_set_translation(instr, instrlist_get_translation_target(ilist));

        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, scratch_reg,
                                     os_get_app_tls_base_offset(TLS_REG_LIB)));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg, slot));
    }

    /* If instr uses stolen register, we use it from app's TLS through a scratch register.
     *
     * TODO i#3544: If stolen register is only used for src, do not spill back into app's
     * TLS; likewise, if it's only used for dst, do not restore it from app's TLS.
     */
    if (instr_uses_reg(instr, dr_reg_stolen)) {
        scratch_reg = pick_scratch_reg(dcontext, instr, scratch_reg, &slot);
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg, slot));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch_reg, TLS_REG_STOLEN_SLOT));

        for (i = 0; i < instr_num_dsts(instr); i++) {
            curop = instr_get_dst(instr, i);
            if (opnd_is_reg(curop) && opnd_get_reg(curop) == dr_reg_stolen)
                instr_set_dst(instr, i, opnd_create_reg(scratch_reg));
            else if (opnd_is_base_disp(curop) && opnd_get_base(curop) == dr_reg_stolen) {
                instr_set_dst(instr, i,
                              opnd_create_base_disp(scratch_reg, DR_REG_NULL, 0,
                                                    opnd_get_disp(curop),
                                                    opnd_get_size(curop)));
            }
        }
        for (i = 0; i < instr_num_srcs(instr); i++) {
            curop = instr_get_src(instr, i);
            if (opnd_is_reg(curop) && opnd_get_reg(curop) == dr_reg_stolen)
                instr_set_src(instr, i, opnd_create_reg(scratch_reg));
            else if (opnd_is_base_disp(curop) && opnd_get_base(curop) == dr_reg_stolen) {
                instr_set_src(instr, i,
                              opnd_create_base_disp(scratch_reg, DR_REG_NULL, 0,
                                                    opnd_get_disp(curop),
                                                    opnd_get_size(curop)));
            }
        }
        instr_set_translation(instr, instrlist_get_translation_target(ilist));

        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, scratch_reg, TLS_REG_STOLEN_SLOT));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg, slot));
    }
}

/* Mangle a cbr that uses stolen register and tp register as follows:
 *
 *      beq  tp, t3, target         # t3 is the stolen register
 * =>
 *      sd   a0, a0_slot(t3)        # spill a0
 *      ld   a0, tp_slot(t3)        # load app's tp from memory
 *      sd   a1, a1_slot(t3)        # spill a1
 *      ld   a1, stolen_slot(t3)    # laod app's t3 from memory
 *      bne  a0, a1, fall
 *      ld   a0, a0_slot(t3)        # restore a0 (original branch taken)
 *      ld   a1, a1_slot(t3)        # restore a1
 *      j    target
 * fall:
 *      ld   a0, a0_slot(t3)        # restore a0 (original branch not taken)
 *      ld   a1, a1_slot(t3)        # restore a1
 */
static void
mangle_cbr_stolen_reg_and_tp_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                 instr_t *next_instr)
{
    ASSERT_NOT_TESTED();

    instr_t *fall = INSTR_CREATE_label(dcontext);
    reg_id_t scratch_reg1 = DR_REG_NULL, scratch_reg2 = DR_REG_NULL;
    int opcode = instr_get_opcode(instr);
    int i;
    ushort slot1, slot2;
    opnd_t opnd;
    bool instr_uses_tp = instr_uses_reg(instr, DR_REG_TP);
    bool instr_uses_reg_stolen = instr_uses_reg(instr, dr_reg_stolen);

    if (instr_uses_tp) {
        scratch_reg1 = pick_scratch_reg(dcontext, instr, DR_REG_NULL, &slot1);
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg1, slot1));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch_reg1,
                                          os_get_app_tls_base_offset(TLS_REG_LIB)));
    }

    if (instr_uses_reg_stolen) {
        scratch_reg2 = pick_scratch_reg(dcontext, instr, scratch_reg1, &slot2);
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg2, slot2));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch_reg2, TLS_REG_STOLEN_SLOT));
    }

    ASSERT(instr_num_dsts(instr) == 0 && instr_num_srcs(instr) == 3);
    instr_t *reversed_cbr =
        instr_create_0dst_3src(dcontext, opcode, opnd_create_instr(fall),
                               instr_get_src(instr, 1), instr_get_src(instr, 2));
    instr_invert_cbr(reversed_cbr);
    for (i = 0; i < instr_num_srcs(reversed_cbr); i++) {
        opnd = instr_get_src(reversed_cbr, i);
        if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == DR_REG_TP)
            instr_set_src(reversed_cbr, i, opnd_create_reg(scratch_reg1));
        else if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == dr_reg_stolen)
            instr_set_src(reversed_cbr, i, opnd_create_reg(scratch_reg2));
    }
    PRE(ilist, instr, reversed_cbr);

    /* Restore scratch regs on the fall-through (taken, pre-inversion) path. */
    if (instr_uses_tp)
        PRE(ilist, instr, instr_create_restore_from_tls(dcontext, scratch_reg1, slot1));
    if (instr_uses_reg_stolen)
        PRE(ilist, instr, instr_create_restore_from_tls(dcontext, scratch_reg2, slot2));

    /* Replace original cbr with an unconditional jump to its target. */
    opnd = instr_get_src(instr, 0);
    instr_reset(dcontext, instr);
    instr_set_opcode(instr, OP_jal);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_dst(instr, 0, opnd_create_reg(DR_REG_ZERO));
    instr_set_src(instr, 0, opnd);
    instr_set_translation(instr, instrlist_get_translation_target(ilist));

    /* Restore scratch regs on the taken (fall-through, pre-inversion) path. */
    PRE(ilist, next_instr, fall);
    if (instr_uses_tp) {
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg1, slot1));
    }
    if (instr_uses_reg_stolen) {
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg2, slot2));
    }
}

instr_t *
mangle_special_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr)
{
    if (!instr_uses_reg(instr, dr_reg_stolen) && !instr_uses_reg(instr, DR_REG_TP))
        return next_instr;

    if (instr_is_cbr(instr)) {
        mangle_cbr_stolen_reg_and_tp_reg(dcontext, ilist, instr, instr_get_next(instr));
    } else if (!instr_is_mbr(instr)) {
        mangle_stolen_reg_and_tp_reg(dcontext, ilist, instr, next_instr);
    }

    return next_instr;
}

/***************************************************************************
 * LR/SC sequence mangling.
 */

bool
instr_is_ldstex_mangling(dcontext_t *dcontext, instr_t *instr)
{
    /* This should be kept in sync with mangle_exclusive_monitor_op(). */
    if (!instr_is_our_mangling(instr))
        return false;

    opnd_t memop = opnd_create_null();
    if (instr_get_opcode(instr) == OP_sd)
        memop = instr_get_src(instr, 0);
    else if (instr_get_opcode(instr) == OP_ld)
        memop = instr_get_dst(instr, 0);
    if (opnd_is_base_disp(memop)) {
        ASSERT(opnd_get_index(memop) == DR_REG_NULL && opnd_get_scale(memop) == 0);
        uint offs = opnd_get_disp(memop);
        if (opnd_get_base(memop) == dr_reg_stolen && offs >= TLS_LRSC_ADDR_SLOT &&
            offs <= TLS_LRSC_SIZE_SLOT)
            return true;
    }

    ptr_int_t val;
    if (instr_get_opcode(instr) == OP_fence || instr_get_opcode(instr) == OP_bne ||
        /* Check for sc.w/d+bne+jal pattern.  */
        (instr_get_opcode(instr) == OP_jal && instr_get_prev(instr) != NULL &&
         instr_get_opcode(instr_get_prev(instr)) == OP_bne &&
         instr_get_prev(instr_get_prev(instr)) != NULL &&
         instr_is_exclusive_store(instr_get_prev(instr_get_prev(instr)))) ||
        instr_is_exclusive_load(instr) || instr_is_exclusive_store(instr) ||
        (instr_is_mov_constant(instr, &val) &&
         /* XXX: These are fragile, should we look backward a bit to check for more
            specific patterns? */
         (val == 1 /* cas fail */ || val == -1 /* reservation invalidation */ ||
          val == 4 /* lr.w/sc.w size */ || val == 8 /* lr.d/sc.d size */)))
        return true;

    return false;
}

static instr_t *
mangle_exclusive_load(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                      instr_t *next_instr)
{
    /* TODO i#3544: Not implemented.  */
    ASSERT_NOT_IMPLEMENTED(!instr_uses_reg(instr, DR_REG_TP));

    reg_id_t scratch_reg1 = DR_REG_NULL, scratch_reg2 = DR_REG_NULL;
    ushort slot1, slot2;
    int aqrl, opcode;
    opnd_t dst, src0;
    opnd_size_t opsz;
    bool uses_reg_stolen;
    ASSERT(instr_is_exclusive_load(instr));
    ASSERT(instr_num_dsts(instr) == 1 && instr_num_srcs(instr) == 2 &&
           opnd_is_immed_int(instr_get_src(instr, 1)));

    aqrl = opnd_get_immed_int(instr_get_src(instr, 1));
    uses_reg_stolen = instr_uses_reg(instr, dr_reg_stolen);

    /* Pick and spill scratch register(s). */
    scratch_reg1 = pick_scratch_reg(dcontext, instr, DR_REG_NULL, &slot1);
    PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg1, slot1));

    if (uses_reg_stolen) {
        scratch_reg2 = pick_scratch_reg(dcontext, instr, scratch_reg1, &slot2);
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg2, slot2));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch_reg2, TLS_REG_STOLEN_SLOT));
    }

    /* Keep the release semantics if needed. */
    if (TESTALL(LRSC_ORDERING_RL_MASK, aqrl)) {
        /* fence rw, rw */
        PRE(ilist, instr,
            INSTR_CREATE_fence(dcontext,
                               opnd_create_immed_int(FENCE_ORDERING_RW, OPSZ_4b),
                               opnd_create_immed_int(FENCE_ORDERING_RW, OPSZ_4b),
                               opnd_create_immed_int(FENCE_FM_NONE, OPSZ_4b)));
    }

    /* Replace exclusive load to a normal one. */
    dst = instr_get_dst(instr, 0);
    src0 = instr_get_src(instr, 0);
    opcode = instr_get_opcode(instr) == OP_lr_d ? OP_ld : OP_lw;
    opsz = opcode == OP_ld ? OPSZ_8 : OPSZ_4;
    ASSERT(opnd_is_reg(dst) && opnd_is_base_disp(src0));
    if (opnd_get_reg(dst) == dr_reg_stolen) {
        opnd_replace_reg(&dst, dr_reg_stolen, scratch_reg2);
    }
    if (opnd_get_base(src0) == dr_reg_stolen) {
        opnd_replace_reg(&src0, dr_reg_stolen, scratch_reg2);
    }
    instr_reset(dcontext, instr);
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_dst(instr, 0, dst);
    instr_set_src(instr, 0, src0);
    instr_set_translation(instr, instrlist_get_translation_target(ilist));

    /* Keep the acquire semantics if needed. */
    if (TESTALL(LRSC_ORDERING_AQ_MASK, aqrl)) {
        /* fence rw, rw */
        PRE(ilist, next_instr,
            INSTR_CREATE_fence(dcontext,
                               opnd_create_immed_int(FENCE_ORDERING_RW, OPSZ_4b),
                               opnd_create_immed_int(FENCE_ORDERING_RW, OPSZ_4b),
                               opnd_create_immed_int(FENCE_FM_NONE, OPSZ_4b)));
    }

    /* Save address, value and size to TLS slot. */
    PRE(ilist, next_instr,
        instr_create_save_to_tls(dcontext, opnd_get_base(src0), TLS_LRSC_ADDR_SLOT));
    PRE(ilist, next_instr,
        instr_create_save_to_tls(dcontext, opnd_get_reg(dst), TLS_LRSC_VALUE_SLOT));
    PRE(ilist, next_instr,
        XINST_CREATE_load_int(
            dcontext, opnd_create_reg(scratch_reg1),
            opnd_create_immed_int(opnd_get_size(instr_get_src(instr, 0)), OPSZ_12b)));
    PRE(ilist, next_instr,
        instr_create_save_to_tls(dcontext, scratch_reg1, TLS_LRSC_SIZE_SLOT));

    /* Restore scratch register(s). */
    PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, scratch_reg1, slot1));
    if (uses_reg_stolen) {
        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, scratch_reg2, TLS_REG_STOLEN_SLOT));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg2, slot2));
    }
    return next_instr;
}

static instr_t *
mangle_exclusive_store(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       instr_t *next_instr)
{
    reg_id_t scratch_reg1, scratch_reg2;
    ushort slot1, slot2;
    int opcode;
    opnd_t dst0, dst1;
    opnd_size_t opsz;
    instr_t *fail = INSTR_CREATE_label(dcontext), *final = INSTR_CREATE_label(dcontext),
            *loop = INSTR_CREATE_label(dcontext);
    ASSERT(instr_is_exclusive_store(instr));
    ASSERT(instr_num_dsts(instr) == 2 && instr_num_srcs(instr) == 2);

    /* TODO i#3544: Not implemented.  */
    ASSERT_NOT_IMPLEMENTED(!instr_uses_reg(instr, dr_reg_stolen));
    ASSERT_NOT_IMPLEMENTED(!instr_uses_reg(instr, DR_REG_TP));

    dst0 = instr_get_dst(instr, 0);
    dst1 = instr_get_dst(instr, 1);
    ASSERT(opnd_is_base_disp(dst0));
    opsz = instr_get_opcode(instr) == OP_sc_d ? OPSZ_8 : OPSZ_4;
    scratch_reg1 = pick_scratch_reg(dcontext, instr, DR_REG_NULL, &slot1);
    scratch_reg2 = pick_scratch_reg(dcontext, instr, scratch_reg1, &slot2);

    /* Spill scratch registers. */
    PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg1, slot1));
    PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch_reg2, slot2));

    /* Restore address saved by exclusive load and check if it's equal to dst0. */
    PRE(ilist, instr,
        instr_create_restore_from_tls(dcontext, scratch_reg1, TLS_LRSC_ADDR_SLOT));
    PRE(ilist, instr,
        INSTR_CREATE_bne(dcontext, opnd_create_instr(fail), opnd_create_reg(scratch_reg1),
                         opnd_create_reg(opnd_get_base(dst0))));

    /* Restore size saved by exclusive load and check if it's equal to current size. */
    PRE(ilist, instr,
        instr_create_restore_from_tls(dcontext, scratch_reg1, TLS_LRSC_SIZE_SLOT));
    PRE(ilist, instr,
        XINST_CREATE_load_int(dcontext, opnd_create_reg(scratch_reg2),
                              opnd_create_immed_int(opsz, OPSZ_12b)));
    PRE(ilist, instr,
        INSTR_CREATE_bne(dcontext, opnd_create_instr(fail), opnd_create_reg(scratch_reg1),
                         opnd_create_reg(scratch_reg2)));

    PRE(ilist, instr,
        instr_create_restore_from_tls(dcontext, scratch_reg1, TLS_LRSC_VALUE_SLOT));
    PRE(ilist, instr, loop);

    /* Convert exclusive store to a compare-and-swap.
     * Begin of the LR/SC sequence.
     */
    opcode = instr_get_opcode(instr) == OP_sc_d ? OP_lr_d : OP_lr_w;
    PRE(ilist, instr,
        instr_create_1dst_2src(dcontext, opcode, opnd_create_reg(scratch_reg2), dst0,
                               opnd_create_immed_int(0b11, OPSZ_2b)));
    PRE(ilist, instr,
        INSTR_CREATE_bne(dcontext, opnd_create_instr(final),
                         opnd_create_reg(scratch_reg1), opnd_create_reg(scratch_reg2)));

    /* instr is here. */

    PRE(ilist, next_instr,
        INSTR_CREATE_bne(dcontext, opnd_create_instr(loop), dst1,
                         opnd_create_reg(DR_REG_ZERO)));
    /* End of the LR/SC sequence. */

    PRE(ilist, next_instr, XINST_CREATE_jump(dcontext, opnd_create_instr(final)));

    /* Write a non-zero value to dst on fail. */
    PRE(ilist, next_instr, fail);
    PRE(ilist, next_instr,
        XINST_CREATE_load_int(dcontext, dst1, opnd_create_immed_int(1, OPSZ_12b)));

    PRE(ilist, next_instr, final);

    /* Invalidate reservation regardless of success or failure. Doing that by writing
     * -1 to the lrsc address slot since -1 is never a valid address.
     */
    PRE(ilist, next_instr,
        XINST_CREATE_load_int(dcontext, opnd_create_reg(scratch_reg1),
                              opnd_create_immed_int(-1, OPSZ_12b)));
    PRE(ilist, next_instr,
        instr_create_save_to_tls(dcontext, scratch_reg1, TLS_LRSC_ADDR_SLOT));

    /* Restore scratch registers. */
    PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, scratch_reg1, slot1));
    PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, scratch_reg2, slot2));
    return next_instr;
}

/* RISC-V provides LR/SC (load-reserved / store-conditional) pair to perform complex
 * atomic memory operations. The way it works is that while LR is doing memory load, it
 * will register a reservation set -- a set of bytes that subsumes the bytes in the
 * addressed word. SC conditionally writes a word to the address only if the reservation
 * is still valid and the reservation set contains the bytes being written.
 *
 * Under cache consistency protocol, LR/SC can be implemented by simply adding a mark to
 * the corresponding cache line. But this also puts many restrictions for instructions
 * between LR/SC. For example, memory access instructions are not allowed.
 *
 * (Read more on the Volume I: RISC-V Unprivileged ISA V20191213 at page 51.)
 *
 * This is essentially the same situation as ARM/AArch64's exclusive monitors, quote from
 * ldstex.dox: "Since dynamic instrumentation routinely adds additional memory loads and
 * stores in between application instructions, it is in danger of breaking every monitor
 * in the application."
 *
 * On an Unmatched RISC-V SBC, without this mangling, any application linked with libc
 * would hang on startup.
 *
 * So for the LR/SC sequence, a similar approach to AArch64's exclusive monitors is
 * adopted: mangling LR to a normal load, and SC to a compare-and-swap.
 *
 * While this introduces ABA problems, quote again from ldstex.dox: "the difference almost
 * never matters for real programs".
 *
 * Here is an example of how we do the transformation:
 *
 * # Original code sequence
 * 1:
 *      lr.w.aqrl   a5, (a3)
 *      bne         a5, a4, 1f
 *      sc.w.rl     a1, a2, (a3)
 *      bnez        a1, 1b
 * 1:
 *
 * # After mangling
 * <block 1>
 * 1:
 *      sd          a0, a0_slot(t3)         # save scratch register
 *      fence       rw, rw                  # keep release semantics
 *      lw          a5, 0(a3)               # replace lr with a normal load
 *      fence       rw, rw                  # keep acquire semantics
 *      sd          a3, lrsc_addr_slot(t3)  # save address
 *      sd          a5, lrsc_val_slot(t3)   # save value
 *      li          a0, 4
 *      sd          a0, lrsc_size_slot(t3)  # save size (4 bytes)
 *      ld          a0, a0_slot(t3)         # restore scratch register
 *      bne         a5, a4, 1f
 *
 * <block 2>
 * 1:
 *      sd          a0, a0_slot(t3)         # save scratch register 1
 *      sd          a4, a4_slot(t3)         # save scratch register 2
 *      ld          a0, lrsc_addr_slot(t3)  # load saved address
 *      bne         a0, a3, fail            # check address
 *      ld          a0, lrsc_size_slot(t3)  # load saved size
 *      li          a4, 4
 *      bne         a0, a4, fail            # check size
 *      ld          a0, lrsc_val_slot(t3)   # load saved value
 * loop:
 *      lr.w.aqrl   a4, (a3)                # begin of the CAS sequence
 *      bne         a0, a4, final
 *      sc.w.rl     a1, a2, (a3)
 *      bne         a1, zero, loop          # retry on failure, end of the sequence
 *      j           final
 * fail:
 *      li          a1, 1                   # sets non-zero value to dst on failure
 * final:
 *      li          a0, -1
 *      sd          a0, lrsc_addr_slot(t3)  # invalidate reservation
 *      ld          a0, a0_slot(t3)         # restore scratch register 1
 *      ld          a4, a4_slot(t3)         # restore scratch register 2
 *      bnez        a1, 1b
 */
instr_t *
mangle_exclusive_monitor_op(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                            instr_t *next_instr)
{
    ASSERT_NOT_TESTED();

    if (!INTERNAL_OPTION(ldstex2cas))
        return NULL;

    if (instr_is_exclusive_load(instr)) {
        return mangle_exclusive_load(dcontext, ilist, instr, next_instr);
    } else if (instr_is_exclusive_store(instr)) {
        return mangle_exclusive_store(dcontext, ilist, instr, next_instr);
    }
    return next_instr;
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
