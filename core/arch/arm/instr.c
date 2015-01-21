/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

#include "../globals.h"
#include "instr.h"

/* FIXME i#1551: add A64 and Thumb support throughout */

bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode)
{
#ifdef X64
    return (mode == DR_ISA_ARM_A64);
#else
    if (mode == DR_ISA_ARM_THUMB)
        instr->flags |= INSTR_THUMB_MODE;
    else if (mode == DR_ISA_ARM_A32)
        instr->flags &= ~INSTR_THUMB_MODE;
    else
        return false;
    return true;
#endif
}

dr_isa_mode_t
instr_get_isa_mode(instr_t *instr)
{
#ifdef X64
    return DR_ISA_ARM_A64;
#else
    return TEST(INSTR_THUMB_MODE, instr->flags) ? DR_ISA_ARM_THUMB : DR_ISA_ARM_A32;
#endif
}

int
instr_length_arch(dcontext_t *dcontext, instr_t *instr)
{
    if (instr_get_opcode(instr) == OP_LABEL)
        return 0;
    if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB) {
        /* We have to encode to find the size */
        return -1;
    } else
        return ARM_INSTR_SIZE;
}

bool
opc_is_not_a_real_memory_load(int opc)
{
    return false;
}

/* return the branch type of the (branch) inst */
uint
instr_branch_type(instr_t *cti_instr)
{
    instr_get_opcode(cti_instr); /* ensure opcode is valid */
    if (instr_is_call_direct(cti_instr))
        return LINK_DIRECT|LINK_CALL;
    else if (instr_is_call_indirect(cti_instr))
        return LINK_INDIRECT|LINK_CALL;
    else if (instr_is_return(cti_instr))
        return LINK_INDIRECT|LINK_RETURN;
    else if (instr_is_mbr_arch(cti_instr))
        return LINK_INDIRECT|LINK_JMP;
    else if (instr_is_cbr_arch(cti_instr) || instr_is_ubr_arch(cti_instr))
        return LINK_DIRECT|LINK_JMP;
    else
        CLIENT_ASSERT(false, "instr_branch_type: unknown opcode");
    return LINK_INDIRECT;
}

bool
instr_is_mov(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_call_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return (opc == OP_bl || opc == OP_blx || opc == OP_blx_ind);
}

bool
instr_is_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_bl || opc == OP_blx);
}

bool
instr_is_near_call_direct(instr_t *instr)
{
    return instr_is_call_direct(instr);
}

bool
instr_is_call_indirect(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_blx_ind);
}

bool
instr_is_pop(instr_t *instr)
{
    opnd_t memop;
    if (instr_num_srcs(instr) == 0)
        return false;
    memop = instr_get_src(instr, 0);
    if (!opnd_is_base_disp(memop))
        return false;
    return opnd_get_base(memop) == DR_REG_SP;
}

bool
instr_reads_gpr_list(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    switch (opc) {
    case OP_stm:
    case OP_stmib:
    case OP_stmda:
    case OP_stmdb:
    case OP_stm_priv:
    case OP_stmib_priv:
    case OP_stmda_priv:
    case OP_stmdb_priv:
        return true;
    default:
        return false;
    }
}

bool
instr_writes_gpr_list(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    switch (opc) {
    case OP_ldm:
    case OP_ldmib:
    case OP_ldmda:
    case OP_ldmdb:
    case OP_ldm_priv:
    case OP_ldmib_priv:
    case OP_ldmda_priv:
    case OP_ldmdb_priv:
        return true;
    default:
        return false;
    }
}

bool
instr_is_return(instr_t *instr)
{
    /* There is no "return" opcode so we consider a return to be either:
     * A) An indirect branch through lr;
     * B) An instr that reads lr and writes pc;
     *    (XXX: should we limit to a move and rule out an add or shift or whatever?)
     * C) A pop into pc.
     */
    int opc = instr_get_opcode(instr);
    if ((opc == OP_bx || opc ==  OP_bxj) &&
        opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_LR)
        return true;
    if (!instr_writes_to_reg(instr, DR_REG_PC, DR_QUERY_INCLUDE_ALL))
        return false;
    return (instr_reads_from_reg(instr, DR_REG_LR, DR_QUERY_INCLUDE_ALL) ||
            instr_is_pop(instr));
}

bool
instr_is_cbr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    if (opc == OP_cbnz || opc ==  OP_cbz)
        return true;
    /* A predicated unconditional branch is a cbr */
    if (opc == OP_b || opc == OP_b_short || opc == OP_bx || opc == OP_bxj ||
        /* Yes, conditional calls are considered cbr */
        opc == OP_bl || opc == OP_blx || opc == OP_blx_ind) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        return (pred != DR_PRED_NONE && pred != DR_PRED_AL);
    }
    /* XXX: should OP_it be considered a cbr? */
    return false;
}

bool
instr_is_mbr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    if (opc == OP_bx || opc ==  OP_bxj || opc == OP_blx_ind ||
        opc == OP_rfe || opc == OP_rfedb || opc == OP_rfeda || opc == OP_rfeib ||
        opc == OP_eret || opc == OP_tbb || opc == OP_tbh)
        return true;
    /* Any instr that writes to the pc, even conditionally (b/c consider that
     * OP_blx_ind when conditional is still an mbr) is an mbr.
     */
    return instr_writes_to_reg(instr, DR_REG_PC, DR_QUERY_INCLUDE_COND_DSTS);
}

bool
instr_is_far_cti(instr_t *instr) /* target address has a segment and offset */
{
    return false;
}

bool
instr_is_far_abs_cti(instr_t *instr)
{
    return false;
}

bool
instr_is_ubr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    if (opc == OP_b || opc == OP_b_short) {
        dr_pred_type_t pred = instr_get_predicate(instr);
        return (pred == DR_PRED_NONE || pred == DR_PRED_AL);
    }
    return false;
}

bool
instr_is_near_ubr(instr_t *instr)      /* unconditional branch */
{
    return instr_is_ubr(instr);
}

bool
instr_is_cti_short(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_b_short || opc == OP_cbz || opc == OP_cbnz);
}

bool
instr_is_cti_loop(instr_t *instr)
{
    return false;
}

bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc)
{
    /* FIXME i#1551: NYI: we need to mangle OP_cbz and OP_cbnz in a similar
     * manner to OP_jecxz on x86
     */
    return false;
}

bool
instr_is_interrupt(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_svc);
}

bool
instr_is_syscall(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_svc);
}

bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value)
{
    int opc = instr_get_opcode(instr);
    if (opc == OP_eor) {
        /* We include OP_eor for symmetry w/ x86, but on ARM "mov reg, #0" is
         * just as compact and there's no reason to use an xor.
         */
        if (opnd_same(instr_get_src(instr, 0), instr_get_dst(instr, 0)) &&
            opnd_same(instr_get_src(instr, 0), instr_get_src(instr, 1)) &&
            /* Must be the form with "sh2, i5_7" and no shift */
            instr_num_srcs(instr) == 4 &&
            opnd_get_immed_int(instr_get_src(instr, 2)) == DR_SHIFT_NONE &&
            opnd_get_immed_int(instr_get_src(instr, 3)) == 0) {
            *value = 0;
            return true;
        } else
            return false;
    } else if (opc == OP_mvn || opc == OP_mvns) {
        opnd_t op = instr_get_src(instr, 0);
        if (opnd_is_immed_int(op)) {
            *value = -opnd_get_immed_int(op);
            return true;
        } else
            return false;
    } else if (opc == OP_mov || opc == OP_movs || opc == OP_movw ||
               /* We include movt even though it only writes the top half */
               opc == OP_movt) {
        opnd_t op = instr_get_src(instr, 0);
        if (opnd_is_immed_int(op)) {
            *value = opnd_get_immed_int(op);
            return true;
        } else
            return false;
    }
    return false;
}

bool instr_is_prefetch(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type OUT)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_floating(instr_t *instr)
{
    return instr_is_floating_ex(instr, NULL);
}

bool
instr_saves_float_pc(instr_t *instr)
{
    return false;
}

bool
opcode_is_mmx(int op)
{
    /* XXX i#1551: add opcode_is_multimedia() (include packed data in GPR's?) */
    return false;
}

bool
opcode_is_sse_or_sse2(int op)
{
    return false;
}

bool
instr_is_mmx(instr_t *instr)
{
    return false;
}

bool
instr_is_sse_or_sse2(instr_t *instr)
{
    return false;
}

bool
instr_is_mov_imm_to_tos(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_undefined(instr_t *instr)
{
    return instr_opcode_valid(instr);
}

DR_API
/* Given a cbr, change the opcode (and potentially branch hint
 * prefixes) to that of the inverted branch condition.
 */
void
instr_invert_cbr(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
}

DR_API
instr_t *
instr_convert_short_meta_jmp_to_long(dcontext_t *dcontext, instrlist_t *ilist,
                                     instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return NULL;
}

static dr_pred_trigger_t
instr_predicate_triggered_priv(instr_t *instr, priv_mcontext_t *mc)
{
    dr_pred_type_t pred = instr_get_predicate(instr);
    switch (pred) {
    case DR_PRED_NONE: return DR_PRED_TRIGGER_NOPRED;
    case DR_PRED_EQ: /* Z == 1 */
        return (TEST(EFLAGS_Z, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_NE: /* Z == 0 */
        return (!TEST(EFLAGS_Z, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_CS: /* C == 1 */
        return (TEST(EFLAGS_C, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_CC: /* C == 0 */
        return (!TEST(EFLAGS_C, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_MI: /* N == 1 */
        return (TEST(EFLAGS_N, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_PL: /* N == 0 */
        return (!TEST(EFLAGS_N, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_VS: /* V == 1 */
        return (TEST(EFLAGS_V, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_VC: /* V == 0 */
        return (!TEST(EFLAGS_V, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_HI: /* C == 1 and Z == 0 */
        return (TEST(EFLAGS_C, mc->apsr) && !TEST(EFLAGS_Z, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_LS: /* C == 0 or Z == 1 */
        return (!TEST(EFLAGS_C, mc->apsr) || TEST(EFLAGS_Z, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
   case DR_PRED_GE: /* N == V */
       return (TEST(EFLAGS_N, mc->apsr) == TEST(EFLAGS_V, mc->apsr)) ?
           DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_LT: /* N != V */
        return (TEST(EFLAGS_N, mc->apsr) == TEST(EFLAGS_V, mc->apsr)) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_GT /* Z == 0 and N == V */:
        return (!TEST(EFLAGS_Z, mc->apsr) &&
                (TEST(EFLAGS_N, mc->apsr) == TEST(EFLAGS_V, mc->apsr))) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_LE: /* Z == 1 or N != V */
        return (TEST(EFLAGS_Z, mc->apsr) ||
                (TEST(EFLAGS_N, mc->apsr) != TEST(EFLAGS_V, mc->apsr))) ?
            DR_PRED_TRIGGER_MATCH : DR_PRED_TRIGGER_MISMATCH;
    case DR_PRED_AL: return DR_PRED_TRIGGER_MATCH;
    case DR_PRED_OP: return DR_PRED_TRIGGER_NOPRED;
    default:
        CLIENT_ASSERT(false, "invalid predicate");
        return DR_PRED_TRIGGER_INVALID;
    }
}

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mc, bool pre)
{
    int opc = instr_get_opcode(instr);
    dr_pred_trigger_t trigger = instr_predicate_triggered_priv(instr, mc);
    CLIENT_ASSERT(instr_is_cbr(instr), "instr_cbr_taken: instr not a cbr");
    if (trigger == DR_PRED_TRIGGER_MISMATCH)
        return false;
    if (opc == OP_cbnz || opc ==  OP_cbz) {
        reg_id_t reg;
        reg_t val;
        CLIENT_ASSERT(opnd_is_reg(instr_get_src(instr, 1)), "invalid OP_cb{,n}z");
        reg = opnd_get_reg(instr_get_src(instr, 1));
        val = reg_get_value_priv(reg, mc);
        if (opc == OP_cbnz)
            return (val != 0);
        else
            return (val == 0);
    } else {
        CLIENT_ASSERT(instr_get_predicate(instr) != DR_PRED_NONE &&
                      instr_get_predicate(instr) != DR_PRED_AL, "invalid cbr type");
        return (trigger == DR_PRED_TRIGGER_MATCH);
    }
}

/* Given eflags, returns whether or not the conditional branch opc would be taken */
static bool
opc_jcc_taken(int opc, reg_t eflags)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return NULL;
}

/* Given eflags, returns whether or not the conditional branch instr would be taken */
bool
instr_jcc_taken(instr_t *instr, reg_t eflags)
{
    /* FIXME i#1551: NYI */
    return opc_jcc_taken(instr_get_opcode(instr), eflags);
}

DR_API
/* Converts a cmovcc opcode \p cmovcc_opcode to the OP_jcc opcode that
 * tests the same bits in eflags.
 */
int
instr_cmovcc_to_jcc(int cmovcc_opcode)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return OP_INVALID;
}

DR_API
bool
instr_cmovcc_triggered(instr_t *instr, reg_t eflags)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

DR_API
dr_pred_trigger_t
instr_predicate_triggered(instr_t *instr, dr_mcontext_t *mc)
{
    return instr_predicate_triggered_priv(instr, dr_mcontext_as_priv_mcontext(mc));
}

bool
instr_predicate_reads_srcs(dr_pred_type_t pred)
{
    return false;
}

bool
instr_predicate_writes_eflags(dr_pred_type_t pred)
{
    return false;
}

bool
instr_predicate_is_cond(dr_pred_type_t pred)
{
    return pred != DR_PRED_NONE && pred != DR_PRED_AL;
}

bool
reg_is_gpr(reg_id_t reg)
{
    return (reg >= DR_REG_X0 && reg < DR_REG_Q0);
}

bool
reg_is_segment(reg_id_t reg)
{
    return false;
}

bool
reg_is_simd(reg_id_t reg)
{
    return (reg >= DR_REG_Q0 && reg <= DR_REG_B31);
}

bool
reg_is_ymm(reg_id_t reg)
{
    return false;
}

bool
reg_is_xmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_mmx(reg_id_t reg)
{
    return false;
}

bool
reg_is_fp(reg_id_t reg)
{
    return false;
}

bool
instr_is_nop(instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    return (opcode == OP_nop);
}

bool
opnd_same_sizes_ok(opnd_size_t s1, opnd_size_t s2, bool is_reg)
{
    /* We don't have the same varying sizes that x86 has */
    return (s1 == s2);
}

instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}
