/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
    /* FIXME i#1551: add Thumb support */
    return 2;
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
    switch (instr_get_opcode(cti_instr)) {
    case OP_bl:
        return LINK_DIRECT|LINK_CALL;     /* unconditional */
    default:
        /* FIXME i#1551: fill in the rest */
        CLIENT_ASSERT(false, "instr_branch_type: unknown opcode: NYI");
    }

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
opcode_is_call(int opc)
{
    return (opc == OP_bl || opc == OP_blx);
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
    /* FIXME i#1551: NYI -- we should split OP_blx into reg and immed forms */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_return(instr_t *instr)
{
    /* FIXME i#1551: NYI -- have to look for read of lr and dst of pc */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
opcode_is_cbr(int opc)
{
    /* FIXME i#1551: NYI -- what about predicate?  We have to have instr? */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_cbr(instr_t *instr)      /* conditional branch */
{
    /* FIXME i#1551: NYI: look at predicate */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
opcode_is_mbr(int opc)
{
    /* FIXME i#1551: add OP_blx_ind too */
    return (opc == OP_bx || opc == OP_bxj);
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
opcode_is_ubr(int opc)
{
    /* FIXME i#1551: NYI -- what about predicate?  We have to have instr? */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_ubr(instr_t *instr)      /* unconditional branch */
{
    int opc = instr_get_opcode(instr);
    return opcode_is_ubr(opc);
}

bool
instr_is_near_ubr(instr_t *instr)      /* unconditional branch */
{
    return instr_is_ubr(instr);
}

bool
instr_is_cti_short(instr_t *instr)
{
    /* FIXME i#1551: NYI -- use for diff immed sizes on ARM? */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_cti_loop(instr_t *instr)
{
    return false;
}

bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc)
{
    return false;
}

bool
instr_is_interrupt(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

bool
instr_is_syscall(instr_t *instr)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return false;
}

/* looks for mov_imm and mov_st and xor w/ src==dst,
 * returns the constant they set their dst to
 */
bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
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

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mcontext, bool pre)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    return NULL;
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
    return (reg >= DR_REG_X0 && reg < DR_REG_Q0) ||
        (reg >= DR_REG_R0_TH && reg <= DR_REG_R31_BB);
}

bool
reg_is_segment(reg_id_t reg)
{
    return false;
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
    return opcode == OP_nop;
}
