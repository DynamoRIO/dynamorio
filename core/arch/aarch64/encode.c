/* **********************************************************
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
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_private.h"

/* Extra logging for encoding */
#define ENC_LEVEL 6

/* Order corresponds to DR_REG_ enum. */
const char * const reg_names[] = {
    "<NULL>",
    "<invalid>",
};

/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = {
    REG_NULL,
    REG_NULL,

#define XREGS \
    DR_REG_X0,  DR_REG_X1,   DR_REG_X2,   DR_REG_X3,  \
    DR_REG_X4,  DR_REG_X5,   DR_REG_X6,   DR_REG_X7,  \
    DR_REG_X8,  DR_REG_X9,   DR_REG_X10,  DR_REG_X11, \
    DR_REG_X12, DR_REG_X13,  DR_REG_X14,  DR_REG_X15, \
    DR_REG_X16, DR_REG_X17,  DR_REG_X18,  DR_REG_X19, \
    DR_REG_X20, DR_REG_X21,  DR_REG_X22,  DR_REG_X23, \
    DR_REG_X24, DR_REG_X25,  DR_REG_X26,  DR_REG_X27, \
    DR_REG_X28, DR_REG_X29,  DR_REG_X30,  DR_REG_X31,
XREGS /* X0-X31 */
XREGS /* W0-W31 */
#undef XREGS

#define QREGS \
    DR_REG_Q0,  DR_REG_Q1,   DR_REG_Q2,   DR_REG_Q3,  \
    DR_REG_Q4,  DR_REG_Q5,   DR_REG_Q6,   DR_REG_Q7,  \
    DR_REG_Q8,  DR_REG_Q9,   DR_REG_Q10,  DR_REG_Q11, \
    DR_REG_Q12, DR_REG_Q13,  DR_REG_Q14,  DR_REG_Q15, \
    DR_REG_Q16, DR_REG_Q17,  DR_REG_Q18,  DR_REG_Q19, \
    DR_REG_Q20, DR_REG_Q21,  DR_REG_Q22,  DR_REG_Q23, \
    DR_REG_Q24, DR_REG_Q25,  DR_REG_Q26,  DR_REG_Q27, \
    DR_REG_Q28, DR_REG_Q29,  DR_REG_Q30,  DR_REG_Q31,
QREGS /* Q0-Q31*/
QREGS /* D0-D31 */
QREGS /* S0-S31 */
QREGS /* H0-H31 */
QREGS /* B0-B31 */
#undef QREGS

    DR_REG_NZCV, DR_REG_FPCR, DR_REG_FPSR,
    DR_REG_TPIDR_EL0, DR_REG_TPIDRRO_EL0
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    /* FIXME i#1569: NYI */
}
#endif

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t * ii)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

/* FIXME i#1569: Very incomplete encoder.
 * Temporary solution until a proper (table-driven) encoder is implemented.
 * SP (stack pointer) and ZR (zero register) may be confused.
 */
static uint encode_common(byte *pc, instr_t *i)
{
    ASSERT(((ptr_int_t)pc & 3) == 0);
    switch (i->opcode) {
    case OP_b:
    case OP_bl:
        ASSERT(i->num_dsts == 0 && i->num_srcs == 1 &&
               i->src0.kind == PC_kind);
        return (0x14000000 | (uint)(i->opcode == OP_bl) << 31 |
                (0x3ffffff & (uint)(i->src0.value.pc - pc) >> 2));
    case OP_bcond:
        ASSERT(i->num_dsts == 0 && i->num_srcs == 1 &&
               i->src0.kind == PC_kind);
        return (0x54000000 |
                (0x001fffff & (uint)(i->src0.value.pc - pc)) >> 2 << 5 |
                (instr_get_predicate(i) & 0xf));
    case OP_cbnz:
    case OP_cbz:
        ASSERT(i->num_dsts == 0 && i->num_srcs == 2 &&
               i->src0.kind == PC_kind &&
               i->srcs[0].kind == REG_kind && i->srcs[0].size == 0 &&
               ((uint)(i->srcs[0].value.reg - DR_REG_W0) < 32 ||
                (uint)(i->srcs[0].value.reg - DR_REG_X0) < 32));
        return (0x34000000 | (i->opcode == OP_cbnz) << 24 |
                (uint)((uint)(i->srcs[0].value.reg - DR_REG_X0) < 32) << 31 |
                (0x001fffff & (uint)(i->src0.value.pc - pc)) >> 2 << 5 |
                ((i->srcs[0].value.reg - DR_REG_X0) < 32 ?
                 (i->srcs[0].value.reg - DR_REG_X0) :
                 (i->srcs[0].value.reg - DR_REG_W0)));
    case OP_load:
        ASSERT(i->num_dsts == 1 && i->num_srcs == 1 &&
               i->dsts[0].kind == REG_kind && i->dsts[0].size == 0 &&
               i->src0.kind == BASE_DISP_kind &&
               i->src0.value.base_disp.index_reg == DR_REG_NULL);
        if (reg_is_gpr(i->dsts[0].value.reg)) {
            uint rt = (i->dsts[0].value.reg -
                       (i->src0.size == OPSZ_8 ? DR_REG_X0 : DR_REG_W0));
            ASSERT(i->src0.size == OPSZ_4 || i->src0.size == OPSZ_8);
            ASSERT(rt < 31);
            return ((i->src0.size == OPSZ_8 ? 0xf9400000 : 0xb9400000 ) |
                    rt |
                    (i->src0.value.base_disp.base_reg - DR_REG_X0) << 5 |
                    i->src0.value.base_disp.disp >>
                    (i->src0.size == OPSZ_8 ? 3 : 2 ) << 10);
        } else {
            uint rt = (i->dsts[0].value.reg -
                       (i->src0.size == OPSZ_4 ? DR_REG_S0 :
                        i->src0.size == OPSZ_8 ? DR_REG_D0 : DR_REG_Q0));
            ASSERT(i->src0.size == OPSZ_4 || i->src0.size == OPSZ_8 ||
                   i->src0.size == OPSZ_16);
            ASSERT(rt < 32);
            return ((i->src0.size == OPSZ_4 ? 0xbd400000 :
                     i->src0.size == OPSZ_8 ? 0xfd400000 : 0x3dc00000) |
                    rt |
                    (i->src0.value.base_disp.base_reg - DR_REG_X0) << 5 |
                    i->src0.value.base_disp.disp >>
                    (i->src0.size == OPSZ_4 ? 2 :
                     i->src0.size == OPSZ_8 ? 3 : 4 ) << 10);
         }
    case OP_mov:
        ASSERT(i->num_dsts == 1 && i->num_srcs == 1 &&
               i->dsts[0].kind == REG_kind && i->dsts[0].size == 0 &&
               i->src0.kind == REG_kind && i->src0.size == 0);
        return (0xaa0003e0 |
                (i->dsts[0].value.reg - DR_REG_X0) |
                (i->src0.value.reg - DR_REG_X0) << 16);
    case OP_store:
        ASSERT(i->num_dsts == 1 && i->num_srcs == 1 &&
               i->src0.kind == REG_kind && i->src0.size == 0 &&
               i->dsts[0].kind == BASE_DISP_kind &&
               (i->dsts[0].size == OPSZ_4 || i->dsts[0].size == OPSZ_8) &&
               i->dsts[0].value.base_disp.index_reg == DR_REG_NULL);
        return ((i->dsts[0].size == OPSZ_8 ? 0xf9000000 : 0xb9000000 ) |
                (i->src0.value.reg - DR_REG_X0) |
                (i->dsts[0].value.base_disp.base_reg - DR_REG_X0) << 5 |
                i->dsts[0].value.base_disp.disp >>
                (i->dsts[0].size == OPSZ_8 ? 3 : 2 ) << 10);
    case OP_tbnz:
    case OP_tbz:
        ASSERT(i->num_dsts == 0 && i->num_srcs == 3 &&
               i->src0.kind == PC_kind &&
               i->srcs[0].kind == REG_kind && i->srcs[0].size == 0 &&
               (uint)(i->srcs[0].value.reg - DR_REG_X0) < 32 &&
               i->srcs[1].kind == IMMED_INTEGER_kind);
        return (0x36000000 | (i->opcode == OP_tbnz) << 24 |
                (0xffff & (uint)(i->src0.value.pc - pc)) >> 2 << 5 |
                (i->srcs[0].value.reg - DR_REG_X0) |
                (i->srcs[1].value.immed_int & 31) << 19 |
                (i->srcs[1].value.immed_int & 32) << 26);
    case OP_xx:
        return i->src0.value.immed_int;

    default:
        ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    case OP_add:
    case OP_svc:
        /* FIXME i#1569: These are encoded but never executed. */
        return i->opcode;
    }
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable, bool *has_instr_opnds/*OUT OPTIONAL*/
                  _IF_DEBUG(bool assert_reachable))
{
    if (has_instr_opnds != NULL)
        *has_instr_opnds = false;

    if (instr_is_label(instr))
        return copy_pc;

    /* First, handle the already-encoded instructions */
    if (instr_raw_bits_valid(instr)) {
        CLIENT_ASSERT(check_reachable, "internal encode error: cannot encode raw "
                      "bits and ignore reachability");
        /* Copy raw bits, possibly re-relativizing */
        return copy_and_re_relativize_raw_instr(dcontext, instr, copy_pc, final_pc);
    }
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_encode error: operands invalid");

    *(uint *)copy_pc = encode_common(final_pc, instr);
    return copy_pc + 4;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr,
                                 byte *dst_pc, byte *final_pc)
{
    /* FIXME i#1569: re-relativizing is NYI */
    ASSERT(instr_raw_bits_valid(instr));
    memcpy(dst_pc, instr->bytes, instr->length);
    return dst_pc + instr->length;
}
