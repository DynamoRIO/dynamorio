/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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
#include "instr.h"
#include "decode.h"

#include "opcode_names.h"

bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode)
{
    return (mode == DR_ISA_ARM_A64);
}

dr_isa_mode_t
instr_get_isa_mode(instr_t *instr)
{
    return DR_ISA_ARM_A64;
}

int
instr_length_arch(dcontext_t *dcontext, instr_t *instr)
{
    if (instr_get_opcode(instr) == OP_LABEL)
        return 0;
    if (instr_get_opcode(instr) == OP_ldstex) {
        ASSERT(instr->length != 0);
        return instr->length;
    }
    ASSERT(instr_get_opcode(instr) != OP_ldstex);
    return AARCH64_INSTR_SIZE;
}

bool
opc_is_not_a_real_memory_load(int opc)
{
    return (opc == OP_adr || opc == OP_adrp);
}

uint
instr_branch_type(instr_t *cti_instr)
{
    int opcode = instr_get_opcode(cti_instr);
    switch (opcode) {
    case OP_b:
    case OP_bcond:
    case OP_cbnz:
    case OP_cbz:
    case OP_tbnz:
    case OP_tbz: return LINK_DIRECT | LINK_JMP;
    case OP_bl: return LINK_DIRECT | LINK_CALL;
    case OP_blraa:
    case OP_blrab:
    case OP_blraaz:
    case OP_blrabz:
    case OP_blr: return LINK_INDIRECT | LINK_CALL;
    case OP_br:
    case OP_braa:
    case OP_brab:
    case OP_braaz:
    case OP_brabz: return LINK_INDIRECT | LINK_JMP;
    case OP_ret:
    case OP_retaa:
    case OP_retab: return LINK_INDIRECT | LINK_RETURN;
    }
    CLIENT_ASSERT(false, "instr_branch_type: unknown opcode");
    return LINK_INDIRECT;
}

const char *
get_opcode_name(int opc)
{
    return opcode_names[opc];
}

bool
instr_is_mov(instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
instr_is_call_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    switch (opc) {
    case OP_bl:
    case OP_blr:
    case OP_blraa:
    case OP_blrab:
    case OP_blraaz:
    case OP_blrabz: return true;
    default: return false;
    }
}

bool
instr_is_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_bl);
}

bool
instr_is_near_call_direct(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_bl);
}

bool
instr_is_call_indirect(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    switch (opc) {
    case OP_blr:
    case OP_blraa:
    case OP_blrab:
    case OP_blraaz:
    case OP_blrabz: return true;
    default: return false;
    }
}

bool
instr_is_return(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    return (opc == OP_ret || opc == OP_retaa || opc == OP_retab);
}

bool
instr_is_cbr_arch(instr_t *instr)
{
    int opc = instr->opcode;                   /* caller ensures opcode is valid */
    return (opc == OP_bcond ||                 /* clang-format: keep */
            opc == OP_cbnz || opc == OP_cbz || /* clang-format: keep */
            opc == OP_tbnz || opc == OP_tbz);
}

bool
instr_is_mbr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    switch (opc) {
    case OP_blr:
    case OP_br:
    case OP_braa:
    case OP_brab:
    case OP_braaz:
    case OP_brabz:
    case OP_blraa:
    case OP_blrab:
    case OP_blraaz:
    case OP_blrabz:
    case OP_ret:
    case OP_retaa:
    case OP_retab: return true;
    default: return false;
    }
}

bool
instr_is_far_cti(instr_t *instr)
{
    return false;
}

bool
instr_is_ubr_arch(instr_t *instr)
{
    int opc = instr->opcode; /* caller ensures opcode is valid */
    return (opc == OP_b);
}

bool
instr_is_near_ubr(instr_t *instr)
{
    return instr_is_ubr(instr);
}

bool
instr_is_cti_short(instr_t *instr)
{
    /* The branch with smallest reach is TBNZ/TBZ, with range +/- 32 KiB.
     * We have restricted MAX_FRAGMENT_SIZE on AArch64 accordingly.
     */
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
    uint opc = instr_get_opcode(instr);

    /* We include several instructions that an assembler might generate for
     * "MOV reg, #imm", but not EOR or SUB or other instructions that could
     * in theory be used to generate a zero, nor "MOV reg, wzr/xzr" (for now).
     */

    /* movn/movz reg, imm */
    if (opc == OP_movn || opc == OP_movz) {
        opnd_t op = instr_get_src(instr, 0);
        if (opnd_is_immed_int(op)) {
            ptr_int_t imm = opnd_get_immed_int(op);
            *value = (opc == OP_movn ? ~imm : imm);
            return true;
        } else
            return false;
    }

    /* orr/add/sub reg, xwr/xzr, imm */
    if (opc == OP_orr || opc == OP_add || opc == OP_sub) {
        opnd_t reg = instr_get_src(instr, 0);
        opnd_t imm = instr_get_src(instr, 1);
        if (opnd_is_reg(reg) &&
            (opnd_get_reg(reg) == DR_REG_WZR || opnd_get_reg(reg) == DR_REG_XZR) &&
            opnd_is_immed_int(imm)) {
            *value = opnd_get_immed_int(imm);
            return true;
        } else
            return false;
    }

    return false;
}

bool
instr_is_prefetch(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_prfm:
    case OP_prfum:
    case OP_prfb:
    case OP_prfh:
    case OP_prfw:
    case OP_prfd: return true;
    default: return false;
    }
}

bool
instr_is_string_op(instr_t *instr)
{
    return false;
}

bool
instr_is_rep_string_op(instr_t *instr)
{
    return false;
}

bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type OUT)
{
    /* For now there is only support of FP arithmetic category type (DR_FP_MATH). */
    /* TODO i#6238: Add support for all FP types.
     */
    uint cat = instr_get_category(instr);
    if (TEST(DR_INSTR_CATEGORY_FP_MATH, cat)) {
        if (type != NULL)
            *type = DR_FP_MATH;
        return true;
    }

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

/* Is this an instruction that we must intercept in order to detect a
 * self-modifying program?
 */
bool
instr_is_icache_op(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (opc == OP_ic_ivau)
        return true; /* ic ivau, xT */
    if (opc == OP_isb)
        return true; /* isb */
    return false;
}

bool
instr_is_undefined(instr_t *instr)
{
    /* FIXME i#1569: Without a complete decoder we cannot recognise all
     * unallocated encodings, but for testing purposes we can recognise
     * some of them: blocks at the top and bottom of the encoding space.
     */
    if (instr_opcode_valid(instr) && instr_get_opcode(instr) == OP_xx) {
        uint enc = opnd_get_immed_int(instr_get_src(instr, 0));
        return ((enc & 0x18000000) == 0 || (~enc & 0xde000000) == 0);
    }
    return false;
}

void
instr_invert_cbr(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    dr_pred_type_t pred = instr_get_predicate(instr);
    CLIENT_ASSERT(instr_is_cbr(instr), "instr_invert_cbr: instr not a cbr");
    if (opc == OP_cbnz) {
        instr_set_opcode(instr, OP_cbz);
    } else if (opc == OP_cbz) {
        instr_set_opcode(instr, OP_cbnz);
    } else if (opc == OP_tbnz) {
        instr_set_opcode(instr, OP_tbz);
    } else if (opc == OP_tbz) {
        instr_set_opcode(instr, OP_tbnz);
    } else {
        instr_set_predicate(instr, instr_invert_predicate(pred));
    }
}

bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mc, bool pre)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
instr_predicate_reads_srcs(dr_pred_type_t pred)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
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
    return pred != DR_PRED_NONE && pred != DR_PRED_AL && pred != DR_PRED_NV;
}

bool
reg_is_gpr(reg_id_t reg)
{
    return (reg >= DR_REG_START_64 && reg <= DR_REG_STOP_64) ||
        (reg >= DR_REG_START_32 && reg <= DR_REG_STOP_32);
}

bool
reg_is_simd(reg_id_t reg)
{
    return (DR_REG_Q0 <= reg && reg <= DR_REG_B31);
}

bool
reg_is_vector_simd(reg_id_t reg)
{
    return false;
}

bool
reg_is_opmask(reg_id_t reg)
{
    return false;
}

bool
reg_is_bnd(reg_id_t reg)
{
    return false;
}

bool
reg_is_strictly_zmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_ymm(reg_id_t reg)
{
    /* i#1312: check why this assertion is here and not
     * in the other x86 related reg_is_ functions.
     */
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
reg_is_strictly_ymm(reg_id_t reg)
{
    return false;
}

bool
reg_is_xmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_strictly_xmm(reg_id_t reg)
{
    return false;
}

bool
reg_is_mmx(reg_id_t reg)
{
    return false;
}

bool
instr_is_opmask(instr_t *instr)
{
    return false;
}

bool
reg_is_fp(reg_id_t reg)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
reg_is_z(reg_id_t reg)
{
    return DR_REG_Z0 <= reg && reg <= DR_REG_Z31;
}

bool
instr_is_nop(instr_t *instr)
{
    uint opc = instr_get_opcode(instr);
    return (opc == OP_nop);
}

bool
opnd_same_sizes_ok(opnd_size_t s1, opnd_size_t s2, bool is_reg)
{
    return (s1 == s2);
}

instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

bool
instr_reads_thread_register(instr_t *instr)
{
    return (instr_get_opcode(instr) == OP_mrs && opnd_is_reg(instr_get_src(instr, 0)) &&
            opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_TPIDR_EL0);
}

bool
instr_writes_thread_register(instr_t *instr)
{
    return (instr_get_opcode(instr) == OP_msr && instr_num_dsts(instr) == 1 &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_TPIDR_EL0);
}

/* Identify one of the reg-reg moves inserted as part of stolen reg mangling:
 *   +0    m4  f9000380   str    %x0 -> (%x28)[8byte]
 * Move stolen reg to x0:
 *   +4    m4  aa1c03e0   orr    %xzr %x28 lsl $0x0000000000000000 -> %x0
 *   +8    m4  f9401b9c   ldr    +0x30(%x28)[8byte] -> %x28
 *   +12   L3  f81e0ffc   str    %x28 %sp $0xffffffffffffffe0 -> -0x20(%sp)[8byte] %sp
 * Move x0 back to stolenr eg:
 *   +16   m4  aa0003fc   orr    %xzr %x0 lsl $0x0000000000000000 -> %x28
 *   +20   m4  f9400380   ldr    (%x28)[8byte] -> %x0
 */
bool
instr_is_stolen_reg_move(instr_t *instr, bool *save, reg_id_t *reg)
{
    CLIENT_ASSERT(instr != NULL, "internal error: NULL argument");
    if (instr_is_app(instr) || instr_get_opcode(instr) != OP_orr)
        return false;
    ASSERT(instr_num_srcs(instr) == 4 && instr_num_dsts(instr) == 1 &&
           opnd_is_reg(instr_get_src(instr, 1)) && opnd_is_reg(instr_get_dst(instr, 0)));
    if (opnd_get_reg(instr_get_src(instr, 1)) == dr_reg_stolen) {
        if (save != NULL)
            *save = true;
        if (reg != NULL) {
            *reg = opnd_get_reg(instr_get_dst(instr, 0));
            ASSERT(*reg != dr_reg_stolen);
        }
        return true;
    }
    if (opnd_get_reg(instr_get_dst(instr, 0)) == dr_reg_stolen) {
        if (save != NULL)
            *save = false;
        if (reg != NULL)
            *reg = opnd_get_reg(instr_get_src(instr, 0));
        return true;
    }
    return false;
}

DR_API
bool
instr_is_exclusive_load(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_ldaxp:
    case OP_ldaxr:
    case OP_ldaxrb:
    case OP_ldaxrh:
    case OP_ldxp:
    case OP_ldxr:
    case OP_ldxrb:
    case OP_ldxrh: return true;
    }
    return false;
}

DR_API
bool
instr_is_exclusive_store(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_stlxp:
    case OP_stlxr:
    case OP_stlxrb:
    case OP_stlxrh:
    case OP_stxp:
    case OP_stxr:
    case OP_stxrb:
    case OP_stxrh: return true;
    }
    return false;
}

DR_API
bool
instr_is_scatter(instr_t *instr)
{
    /* FIXME i#3837: add support. */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

DR_API
bool
instr_is_gather(instr_t *instr)
{
    /* FIXME i#3837: add support. */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

dr_pred_type_t
instr_invert_predicate(dr_pred_type_t pred)
{
    switch (pred) {
    case DR_PRED_EQ: return DR_PRED_NE;
    case DR_PRED_NE: return DR_PRED_EQ;
    case DR_PRED_CS: return DR_PRED_CC;
    case DR_PRED_CC: return DR_PRED_CS;
    case DR_PRED_MI: return DR_PRED_PL;
    case DR_PRED_PL: return DR_PRED_MI;
    case DR_PRED_VS: return DR_PRED_VC;
    case DR_PRED_VC: return DR_PRED_VS;
    case DR_PRED_HI: return DR_PRED_LS;
    case DR_PRED_LS: return DR_PRED_HI;
    case DR_PRED_GE: return DR_PRED_LT;
    case DR_PRED_LT: return DR_PRED_GE;
    case DR_PRED_GT: return DR_PRED_LE;
    case DR_PRED_LE: return DR_PRED_GT;
    default: CLIENT_ASSERT(false, "Incorrect predicate value"); return DR_PRED_NONE;
    }
}
