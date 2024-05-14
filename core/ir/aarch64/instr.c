/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2016-2024 ARM Limited. All rights reserved.
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
#include "encode_api.h"
#include "opcode_names.h"

#include <stddef.h>

/* XXX i#6690: currently only A64 is supported for instruction encoding.
 * We want to add support for A64 decoding and synthetic ISA encoding as well.
 * XXX i#1684: move this function to core/ir/instr_shared.c once we can support
 * all architectures in the same build of DR.
 */
bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode)
{
    if (mode != DR_ISA_ARM_A64 && mode != DR_ISA_REGDEPS)
        return false;
    instr->isa_mode = mode;
    return true;
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
    /* These instructions have a memref operand, but do not read memory:
     * - adp/adrp do pc-relative address calculation.
     * - ldg loads the allocation tag for the referenced address.
     */
    return (opc == OP_adr || opc == OP_adrp || opc == OP_ldg);
}

bool
opc_is_not_a_real_memory_store(int opc)
{
    /* These instructions have a memref operand, but do not write memory:
     * - stg/st2g stores the allocation tag for the referenced address.
     *   note: other MTE tag storing instructions (stgp, stzg, etc) store memory as well
     *         as allocation tags so they are not checked for here. stg/st2g only store a
     *         tag.
     */
    return (opc == OP_stg || opc == OP_st2g);
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
instr_is_floating_type(instr_t *instr, dr_instr_category_t *type DR_PARAM_OUT)
{
    /* DR_FP_STATE instructions aren't available on AArch64.
     * Processor state is saved/restored with loads and stores.
     */
    uint cat = instr_get_category(instr);
    if (!TEST(DR_INSTR_CATEGORY_FP, cat))
        return false;
    if (type != NULL)
        *type = cat;
    return true;
}

bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type DR_PARAM_OUT)
{
    /* DR_FP_STATE instructions aren't available on AArch64.
     * Processor state is saved/restored with loads and stores.
     */
    uint cat = instr_get_category(instr);
    if (!TEST(DR_INSTR_CATEGORY_FP, cat))
        return false;
    else if (TEST(DR_INSTR_CATEGORY_MATH, cat)) {
        if (type != NULL)
            *type = DR_FP_MATH;
        return true;
    } else if (TEST(DR_INSTR_CATEGORY_CONVERT, cat)) {
        if (type != NULL)
            *type = DR_FP_CONVERT;
        return true;
    } else if (TEST(DR_INSTR_CATEGORY_MOVE, cat)) {
        if (type != NULL)
            *type = DR_FP_MOVE;
        return true;
    } else {
        CLIENT_ASSERT(false, "instr_is_floating_ex: FP instruction without subcategory");
        return false;
    }
}

bool
instr_is_floating(instr_t *instr)
{
    return instr_is_floating_type(instr, NULL);
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
    return reg_is_z(reg) || (DR_REG_Q0 <= reg && reg <= DR_REG_B31);
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
    switch (instr_get_opcode(instr)) {
    case OP_st1b:
    case OP_st1h:
    case OP_st1w:
    case OP_st1d:
    case OP_st2b:
    case OP_st2h:
    case OP_st2w:
    case OP_st2d:
    case OP_st3b:
    case OP_st3h:
    case OP_st3w:
    case OP_st3d:
    case OP_st4b:
    case OP_st4h:
    case OP_st4w:
    case OP_st4d:
    case OP_stnt1b:
    case OP_stnt1h:
    case OP_stnt1w:
    case OP_stnt1d: return true;
    }
    return false;
}

DR_API
bool
instr_is_gather(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_ld1b:
    case OP_ld1h:
    case OP_ld1w:
    case OP_ld1d:
    case OP_ld1sb:
    case OP_ld1sh:
    case OP_ld1sw:
    case OP_ld1rob:
    case OP_ld1rqb:
    case OP_ld1rqh:
    case OP_ld1rqw:
    case OP_ld1rqd:
    case OP_ldff1b:
    case OP_ldff1h:
    case OP_ldff1w:
    case OP_ldff1d:
    case OP_ldff1sb:
    case OP_ldff1sh:
    case OP_ldff1sw:
    case OP_ldnf1b:
    case OP_ldnf1h:
    case OP_ldnf1w:
    case OP_ldnf1d:
    case OP_ldnf1sb:
    case OP_ldnf1sh:
    case OP_ldnf1sw:
    case OP_ldnt1b:
    case OP_ldnt1h:
    case OP_ldnt1w:
    case OP_ldnt1d:
    case OP_ldnt1sb:
    case OP_ldnt1sh:
    case OP_ldnt1sw:
    case OP_ld2b:
    case OP_ld2h:
    case OP_ld2w:
    case OP_ld2d:
    case OP_ld3b:
    case OP_ld3h:
    case OP_ld3w:
    case OP_ld3d:
    case OP_ld4b:
    case OP_ld4h:
    case OP_ld4w:
    case OP_ld4d: return true;
    }
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

ptr_int_t
d_r_compute_scaled_index_aarch64(opnd_t opnd, reg_t index_val)
{
    bool scaled = false;
    uint amount = 0;
    dr_extend_type_t type = opnd_get_index_extend(opnd, &scaled, &amount);
    reg_t extended = 0;
    uint msb = 0;
    switch (type) {
    default: CLIENT_ASSERT(false, "Unsupported extend type"); return 0;
    case DR_EXTEND_UXTW: extended = index_val & 0x00000000ffffffffULL; break;
    case DR_EXTEND_SXTW:
        extended = index_val & 0x00000000ffffffffULL;
        msb = extended >> 31u;
        if (msb == 1) {
            extended = ((~0ull) << 32u) | extended;
        }
        break;
    case DR_EXTEND_UXTX:
    case DR_EXTEND_SXTX: extended = index_val; break;
    }
    if (scaled) {
        return extended << amount;
    } else {
        return extended;
    }
}

static bool
is_active_in_mask(size_t element, uint64 mask, size_t element_size_bytes)
{
    const uint64 element_flag = 1ull << (element_size_bytes * element);
    return TESTALL(element_flag, mask);
}

bool
instr_compute_vector_address(instr_t *instr, priv_mcontext_t *mc, size_t mc_size,
                             dr_mcontext_flags_t mc_flags, opnd_t curop, uint addr_index,
                             DR_PARAM_OUT bool *have_addr, DR_PARAM_OUT app_pc *addr,
                             DR_PARAM_OUT bool *write)
{
    CLIENT_ASSERT(have_addr != NULL && addr != NULL && mc != NULL && write != NULL,
                  "SVE address computation: invalid args");
    CLIENT_ASSERT(TEST(DR_MC_MULTIMEDIA, mc_flags),
                  "dr_mcontext_t.flags must include DR_MC_MULTIMEDIA");
    CLIENT_ASSERT(mc_size >= offsetof(dr_mcontext_t, svep) + sizeof(mc->svep),
                  "Incompatible client, invalid dr_mcontext_t.size.");

    *write = instr_is_scatter(instr);
    ASSERT(*write || instr_is_gather(instr));

    const size_t vl_bytes = opnd_size_in_bytes(OPSZ_SVE_VECLEN_BYTES);
    /* DynamoRIO currently supports up to 512-bit vector registers so a predicate register
     * value should be <= 64-bits.
     * If DynamoRIO is extended in the future to support large vector lengths this
     * function will need to be updated to cope with larger predicate mask values.
     */
    ASSERT(vl_bytes / 8 < sizeof(uint64));

    const reg_t governing_pred = opnd_get_reg(instr_get_src(instr, 1));
    ASSERT(governing_pred >= DR_REG_START_P && governing_pred <= DR_REG_STOP_P);
    uint64 mask = mc->svep[governing_pred - DR_REG_START_P].u64[0];

    if (mask == 0) {
        return false;
    }

    const size_t element_size_bytes =
        opnd_size_in_bytes(opnd_get_vector_element_size(curop));
    const size_t num_elements = vl_bytes / element_size_bytes;

    size_t active_elements_found = 0;
    for (size_t element = 0; element < num_elements; element++) {
        if (is_active_in_mask(element, mask, element_size_bytes)) {
            active_elements_found++;
            if (active_elements_found - 1 == addr_index) {
                const reg_t base_reg = opnd_get_base(curop);
                if (reg_is_z(base_reg)) {
                    /* Vector base: extract the current element. */
                    size_t base_reg_num = base_reg - DR_REG_START_Z;
                    if (element_size_bytes == 4) {
                        *addr = (app_pc)(reg_t)mc->simd[base_reg_num].u32[element];
                    } else {
                        ASSERT(element_size_bytes == 8);
                        *addr = (app_pc)mc->simd[base_reg_num].u64[element];
                    }
                } else {
                    /* Scalar base. */
                    *addr = (app_pc)reg_get_value_priv(base_reg, mc);
                }

                const reg_t index_reg = opnd_get_index(curop);
                reg_t unscaled_index_val = 0;
                if (reg_is_z(index_reg)) {
                    /* Vector index: extract the current element. */
                    size_t index_reg_num = index_reg - DR_REG_START_Z;
                    if (element_size_bytes == 4) {
                        unscaled_index_val = mc->simd[index_reg_num].u32[element];
                    } else {
                        ASSERT(element_size_bytes == 8);
                        unscaled_index_val = mc->simd[index_reg_num].u64[element];
                    }
                } else {
                    /* Scalar index or no index. */
                    unscaled_index_val = reg_get_value_priv(index_reg, mc);
                }

                *have_addr = true;
                *addr += d_r_compute_scaled_index_aarch64(curop, unscaled_index_val);
                *addr += opnd_get_disp(curop);

                return addr_index < num_elements;
            }
        }
    }

    return false;
}
