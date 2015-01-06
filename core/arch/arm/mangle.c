/* ******************************************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
 * ******************************************************************************/

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
 * ARE DISCLAIMED. IN NO EVENT SHALL Google, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "mangle.c" */

#include "../globals.h"
#include "arch.h"
#include "instr_create.h"
#include "instrument.h" /* instrlist_meta_preinsert */

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert

/* For ARM, we always use TLS and never use hardcoded dcontext
 * (xref USE_SHARED_GENCODE_ALWAYS() and -private_ib_in_tls).
 * Thus we use instr_create_{save_to,restore_from}_tls() directly.
 */

byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    /* FIXME i#1551: refactor the caller and make this routine x86-only. */
    ASSERT_NOT_REACHED();
    return NULL;
}

void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}
/***************************************************************************/
#if !defined(STANDALONE_DECODER)

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci,
                    instrlist_t *ilist, instr_t *instr)
{
    /* clear eflags for callee's usage */
    if (cci == NULL || !cci->skip_clear_eflags) {
        if (!dynamo_options.cleancall_ignore_eflags) {
            /* FIXME i#1551: NYI on ARM */
            ASSERT_NOT_IMPLEMENTED(false);
        }
    }
}

/* Pushes not only the GPRs but also simd regs, xip, and xflags, in
 * priv_mcontext_t order.
 * The current stack pointer alignment should be passed.  Use 1 if
 * unknown (NOT 0).
 * Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a priv_mcontext_t and have to do further xsp
 * fixups anyway).
 */
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr,
                          uint alignment, instr_t *push_pc)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

/* User should pass the alignment from insert_push_all_registers: i.e., the
 * alignment at the end of all the popping, not the alignment prior to
 * the popping.
 */
void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *instr,
                         uint alignment)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
#ifdef X64
    /* FIXME i#1551: NYI on AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return regular;
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    uint i;
    instr_t *mark = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, mark);

    ASSERT(num_args == 0 || args != NULL);
    /* FIXME i#1551: we only support limited number of args for now. */
    ASSERT_NOT_IMPLEMENTED(num_args <= NUM_REGPARM);
    for (i = 0; i < num_args; i++) {
        /* FIXME i#1551: we only implement naive parameter preparation,
         * where args are all regs and do not conflict with param regs.
         */
        ASSERT_NOT_IMPLEMENTED(opnd_is_reg(args[i]) &&
                               opnd_get_size(args[i]) == OPSZ_PTR);
        DODEBUG({
            uint j;
            /* assume no reg used by arg conflicts with regparms */
            for (j = 0; j < i; j++)
                ASSERT_NOT_IMPLEMENTED(!opnd_uses_reg(args[j], regparms[i]));
        });
        if (regparms[i] != opnd_get_reg(args[i])) {
            POST(ilist, mark, XINST_CREATE_move(dcontext,
                                                opnd_create_reg(regparms[i]),
                                                args[i]));
        }
    }
    return 0;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    /* load target into scratch register */
    insert_mov_immed_ptrsz(dcontext, (ptr_int_t)target,
                           opnd_create_reg(scratch), ilist, where, NULL, NULL);
    /* mov target from scratch register to pc */
    PRE(ilist, where, INSTR_CREATE_mov(dcontext,
                                       opnd_create_reg(DR_REG_PC),
                                       opnd_create_reg(scratch)));
    return true /* an ind branch */;
}

/*###########################################################################
 *###########################################################################
 *
 *   M A N G L I N G   R O U T I N E S
 */

void
insert_mov_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                      ptr_int_t val, opnd_t dst,
                      instrlist_t *ilist, instr_t *instr,
                      instr_t **first, instr_t **second)
{
    instr_t *mov1, *mov2;
    ASSERT(opnd_is_reg(dst));
    /* FIXME i#1551: we may handle Thumb and ARM mode differently.
     * Now we assume ARM mode only.
     */
    if (val <= 0xfff) {
        mov1 = INSTR_CREATE_mov(dcontext, dst, OPND_CREATE_INT(val));
        PRE(ilist, instr, mov1);
        mov2 = NULL;
    } else {
        /* To use INT16 here and pass the size checks in opnd_create_immed_int
         * we'd have to add UINT16 (or sign-extend the bottom half again):
         * simpler to use INT, and our general ARM philosophy is to use INT and
         * ignore immed sizes at instr creation time (only at encode time do we
         * check them).
         */
        mov1 = INSTR_CREATE_movw(dcontext, dst, OPND_CREATE_INT(val & 0xffff));
        PRE(ilist, instr, mov1);
        val = (val >> 16) & 0xffff;
        if (val == 0) {
            /* movw zero-extends so we're done */
            mov2 = NULL;
        } else {
            /* XXX: movw expects reg size to be OPSZ_PTR but
             * movt expects reg size to be OPSZ_PTR_HALF.
             */
            opnd_set_size(&dst, OPSZ_PTR_HALF);
            mov2 = INSTR_CREATE_movt(dcontext, dst, OPND_CREATE_INT(val));
            PRE(ilist, instr, mov2);
        }
    }
    if (first != NULL)
        *first = mov1;
    if (second != NULL)
        *second = mov2;
}

void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       instr_t **first, instr_t **second)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                    instr_t *instr, instr_t *next_instr)
{
    /* Shared routine already checked method, handled INSTR_NI_SYSCALL*,
     * and inserted the signal barrier and non-auto-restart nop.
     * If we get here, we're dealing with an ignorable syscall.
     */

    /* We assume we do not have to restore the stolen reg value, as it's
     * r8+ and so there will be no syscall arg or number stored in it.
     * We assume the kernel won't read it.
     */
    ASSERT(DR_REG_STOLEN_MIN > DR_REG_SYSNUM);

    /* We do need to save the stolen reg if it is caller-saved.
     * For now we assume that the kernel honors the calling convention
     * and won't clobber callee-saved regs.
     */
    if (dr_reg_stolen != DR_REG_R10 && dr_reg_stolen != DR_REG_R11) {
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, DR_REG_R10, TLS_REG0_SLOT));
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_R10),
                              opnd_create_reg(dr_reg_stolen)));
        /* Post-syscall: */
        PRE(ilist, next_instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(dr_reg_stolen),
                              opnd_create_reg(DR_REG_R10)));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, DR_REG_R10, TLS_REG0_SLOT));
    }
}

#ifdef UNIX
void
mangle_clone_code(dcontext_t *dcontext, byte *pc, bool skip)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool skip _IF_X64(gencode_mode_t mode))
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* Strategy: replace OP_bl with 2-step mov immed into lr + OP_b */
    ptr_uint_t retaddr;
    uint opc = instr_get_opcode(instr);
    ASSERT(opc == OP_bl || opc == OP_blx);
    retaddr = get_call_return_address(dcontext, ilist, instr);
    insert_mov_immed_ptrsz(dcontext, (ptr_int_t)retaddr, opnd_create_reg(DR_REG_LR),
                           ilist, instr, NULL, NULL);
    if (opc == OP_bl) {
        /* remove OP_bl (final added jmp already targets the callee) */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    } else {
        /* Unfortunately while there is OP_blx with an immed, OP_bx requires
         * indirection through a register.  We thus need to swap modes separately,
         * but our ISA doesn't support mixing modes in one fragment, making
         * a local "blx next_instr" not easy.
         */
        /* FIXME i#1551: handling OP_blx NYI on ARM */
        ASSERT_NOT_IMPLEMENTED(false);
    }
    return next_instr;
}

void
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    ptr_uint_t retaddr;
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, DR_REG_R2, TLS_REG2_SLOT));
    if (!opnd_same(instr_get_target(instr), opnd_create_reg(DR_REG_R2))) {
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_R2),
                              instr_get_target(instr)));
    }
    retaddr = get_call_return_address(dcontext, ilist, instr);
    insert_mov_immed_ptrsz(dcontext, (ptr_int_t)retaddr, opnd_create_reg(DR_REG_LR),
                           ilist, instr, NULL, NULL);
    /* remove OP_blx_ind (final added jmp already targets the callee) */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    /* FIXME i#1551: handle mode switch */
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    int opc = instr_get_opcode(instr);
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, DR_REG_R2, TLS_REG2_SLOT));
    if (opc == OP_pop) {
        /* The pop into pc will always be last (r15) so we remove it and add
         * a single-pop instr into r2.
         */
        uint i;
        bool found_pc;
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_reg(instr_get_dst(instr, i)) &&
                opnd_get_reg(instr_get_dst(instr, i)) == DR_REG_PC) {
                found_pc = true;
                instr_remove_dst(dcontext, instr, i);
                break;
            }
        }
        ASSERT(found_pc);
        PRE(ilist, next_instr,
            INSTR_CREATE_pop(dcontext, opnd_create_reg(DR_REG_R2)));
    } else if (opc == OP_bx || opc ==  OP_bxj) {
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_R2),
                              opnd_create_reg(DR_REG_LR)));
        /* remove the bx */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    } else {
        /* Reads lr and writes pc */
        uint i;
        bool found_pc;
        /* XXX: can anything (non-OP_ldm) have r2 as an additional dst? */
        ASSERT_NOT_IMPLEMENTED(instr_writes_to_reg(instr, DR_REG_R2,
                                                   DR_QUERY_INCLUDE_ALL));
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_reg(instr_get_dst(instr, i)) &&
                opnd_get_reg(instr_get_dst(instr, i)) == DR_REG_PC) {
                found_pc = true;
                instr_set_dst(instr, i, opnd_create_reg(DR_REG_R2));
                break;
            }
        }
        ASSERT(found_pc);
    }
}

void
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    uint opc = instr_get_opcode(instr);
    /* Compute the value of r15==pc for orig app instr */
    ptr_int_t r15 = (ptr_int_t)instr_get_raw_bits(instr) + ARM_CUR_PC_OFFS;
    opnd_t reg_op, mem_op;
    ASSERT(instr_has_rel_addr_reference(instr));

    if (opc == OP_ldr || opc == OP_str) {
        reg_id_t reg;
        ushort slot;
        if (opc == OP_ldr) {
            reg_op = instr_get_dst(instr, 0);
            mem_op = instr_get_src(instr, 0);
        } else {
            reg_op = instr_get_src(instr, 0);
            mem_op = instr_get_dst(instr, 0);
        }
        ASSERT(opnd_is_reg(reg_op) && opnd_is_base_disp(mem_op));
        ASSERT_NOT_IMPLEMENTED(!instr_is_cti(instr));
        for (reg  = SCRATCH_REG0, slot = TLS_REG0_SLOT;
             reg <= SCRATCH_REG5; reg++, slot++) {
            if (!instr_uses_reg(instr, reg))
                break;
        }
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, reg, slot));
        insert_mov_immed_ptrsz(dcontext, r15, opnd_create_reg(reg),
                               ilist, instr, NULL, NULL);
        if (opc == OP_ldr) {
            instr_set_src(instr, 0,
                          opnd_create_base_disp(reg, REG_NULL, 0,
                                                opnd_get_disp(mem_op),
                                                opnd_get_size(mem_op)));
        } else {
            instr_set_dst(instr, 0,
                          opnd_create_base_disp(reg, REG_NULL, 0,
                                                opnd_get_disp(mem_op),
                                                opnd_get_size(mem_op)));
        }
        PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, reg, slot));
    } else {
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_IMPLEMENTED(false);
    }
    return false;
}

void
float_pc_update(dcontext_t *dcontext)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
}

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

#endif /* !STANDALONE_DECODER */
/***************************************************************************/
