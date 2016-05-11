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
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "decode_fast.h" /* ensure we export decode_next_pc, decode_sizeof */
#include "instr_create.h"

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
    return (mode == DR_ISA_ARM_A64);
}

app_pc
canonicalize_pc_target(dcontext_t *dcontext, app_pc pc)
{
    return pc;
}

DR_API
app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

DR_API
app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

byte *
decode_eflags_usage(dcontext_t *dcontext, byte *pc, uint *usage,
                    dr_opnd_query_flags_t flags)
{
    *usage = 0; /* FIXME i#1569 */
    return pc + 4;
}

byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

/* FIXME i#1569: Very incomplete decoder: decode most instructions as OP_xx.
 * Temporary solution until a proper (table-driven) decoder is implemented.
 * SP (stack pointer) and ZR (zero register) may be confused.
 */
static byte *
decode_common(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    byte *next_pc = pc + 4;
    uint enc = *(uint *)pc;

    CLIENT_ASSERT(instr->opcode == OP_INVALID || instr->opcode == OP_UNDECODED,
                  "decode: instr is already decoded, may need to call instr_reset()");

    if ((enc & 0x7c000000) == 0x14000000) {
        instr_set_opcode(instr, TEST(1U << 31, enc) ? OP_bl : OP_b);
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr->src0 = opnd_create_pc(pc + ((enc & 0x1ffffff) << 2) -
                                     ((enc & 0x2000000) << 2));
    }
    else if ((enc & 0xff000010) == 0x54000000) {
        instr_set_opcode(instr, OP_bcond);
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr->src0 = opnd_create_pc(pc + ((enc >> 5 & 0x3ffff) << 2) -
                                     ((enc >> 5 & 0x40000) << 2));
        instr_set_predicate(instr, enc & 0xf);
    }
    else if ((enc & 0x7e000000) == 0x34000000) {
        instr_set_opcode(instr, TEST(1 << 24, enc) ? OP_cbnz : OP_cbz);
        instr_set_num_opnds(dcontext, instr, 0, 2);
        instr->src0 = opnd_create_pc(pc + ((enc >> 5 & 0x3ffff) << 2) -
                                     ((enc >> 5 & 0x40000) << 2));
        instr->srcs[0] = opnd_create_reg((TEST(1U << 31, enc) ?
                                          DR_REG_X0 : DR_REG_W0) +
                                         (enc & 31));
    }
    else if ((enc & 0x7e000000) == 0x36000000) {
        instr_set_opcode(instr, TEST(1 << 24, enc) ? OP_tbnz : OP_tbz);
        instr_set_num_opnds(dcontext, instr, 0, 3);
        instr->src0 = opnd_create_pc(pc + ((enc >> 5 & 0x1fff) << 2) -
                                     ((enc >> 5 & 0x2000) << 2));
        instr->srcs[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->srcs[1] = OPND_CREATE_INT8((enc >> 19 & 31) | (enc >> 26 & 32));
    }
    else if ((enc & 0xff9ffc1f) == 0xd61f0000 &&
             (enc & 0x00600000) != 0x00600000) {
        int op = enc >> 21 & 3;
        instr_set_opcode(instr, (op == 0 ? OP_br : op == 1 ? OP_blr : OP_ret));
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr->src0 = opnd_create_reg(DR_REG_X0 + (enc >> 5 & 31));
    }
    else if ((enc & 0x1f000000) == 0x10000000) {
        ptr_int_t off = (((enc >> 3 & 0xffffc) | (enc >> 29 & 3)) -
                         (ptr_int_t)(enc >> 3 & 0x100000));
        ptr_int_t x = TEST(1U << 31, enc) ?
            ((ptr_int_t)pc >> 12 << 12) + (off << 12) :
            (ptr_int_t)pc + off;
        instr_set_opcode(instr, TEST(1U << 31, enc) ? OP_adrp : OP_adr);
        instr_set_num_opnds(dcontext, instr, 1, 1);
        instr->dsts[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->src0 = opnd_create_rel_addr((void *)x, OPSZ_8);
    }
    else if ((enc & 0xbf000000) == 0x18000000) {
        int offs = (enc >> 3 & 0xffffc) - (enc >> 3 & 0x100000);
        instr_set_opcode(instr, OP_ldr);
        instr_set_num_opnds(dcontext, instr, 1, 1);
        instr->dsts[0] = opnd_create_reg((TEST(1 << 30, enc) ?
                                          DR_REG_X0 : DR_REG_W0) + (enc & 31));
        instr->src0 = opnd_create_rel_addr(pc + offs, OPSZ_8);
    }
    else if ((enc & 0x3f000000) == 0x1c000000 &&
             (enc & 0xc0000000) != 0xc0000000) {
        int opc = enc >> 30 & 3;
        int offs = (enc >> 3 & 0xffffc) - (enc >> 3 & 0x100000);
        instr_set_opcode(instr, OP_ldr);
        instr_set_num_opnds(dcontext, instr, 1, 1);
        instr->dsts[0] = opnd_create_reg((opc == 0 ? DR_REG_S0 :
                                          opc == 1 ? DR_REG_D0 : DR_REG_Q0) +
                                         (enc & 31));
        instr->src0 = opnd_create_rel_addr(pc + offs,
                                           opc == 0 ? OPSZ_4 :
                                           opc == 1 ? OPSZ_8 : OPSZ_16);
    }
    else if ((enc & 0xffffffe0) == 0xd53bd040) {
        instr_set_opcode(instr, OP_mrs);
        /* XXX DR_REG_TPIDR_EL0 as source operand */
        instr_set_num_opnds(dcontext, instr, 1, 0);
        instr->dsts[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
    }
    else if ((enc & 0xffffffe0) == 0xd51bd040) {
        instr_set_opcode(instr, OP_msr);
        /* XXX DR_REG_TPIDR_EL0 as destination operand */
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr->src0 = opnd_create_reg(DR_REG_X0 + (enc & 31));
    }
    else if ((enc & 0xffe0001f) == 0xd4000001) {
        instr_set_opcode(instr, OP_svc);
        instr_set_num_opnds(dcontext, instr, 0, 1);
        instr->src0 = OPND_CREATE_INT16(enc >> 5 & 0xffff);
    } else {
        /* We use OP_xx for instructions not yet handled by the decoder.
         * If an A64 instruction accesses a general-purpose register
         * (except X30) then the number of that register appears in one
         * of four possible places in the instruction word, so we can
         * pessimistically assume that an unrecognised instruction reads
         * and writes all four of those registers, and this is
         * sufficient to enable correct (though often excessive) mangling.
         */
        instr_set_opcode(instr, OP_xx);
        instr_set_num_opnds(dcontext, instr, 4, 5);
        instr->src0 = OPND_CREATE_INT32(enc);
        instr->srcs[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->dsts[0] = opnd_create_reg(DR_REG_X0 + (enc & 31));
        instr->srcs[1] = opnd_create_reg(DR_REG_X0 + (enc >> 5 & 31));
        instr->dsts[1] = opnd_create_reg(DR_REG_X0 + (enc >> 5 & 31));
        instr->srcs[2] = opnd_create_reg(DR_REG_X0 + (enc >> 10 & 31));
        instr->dsts[2] = opnd_create_reg(DR_REG_X0 + (enc >> 10 & 31));
        instr->srcs[3] = opnd_create_reg(DR_REG_X0 + (enc >> 16 & 31));
        instr->dsts[3] = opnd_create_reg(DR_REG_X0 + (enc >> 16 & 31));
    }

    instr_set_operands_valid(instr, true);

    if (orig_pc != pc) {
        /* We do not want to copy when encoding and condone an invalid
         * relative target.
         */
        instr_set_raw_bits_valid(instr, false);
        instr_set_translation(instr, orig_pc);
    } else {
        /* We set raw bits AFTER setting all srcs and dsts because setting
         * a src or dst marks instr as having invalid raw bits.
         */
        ASSERT(CHECK_TRUNCATE_TYPE_uint(next_pc - pc));
        instr_set_raw_bits(instr, pc, (uint)(next_pc - pc));
    }

    return next_pc;
}

byte *
decode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode_common(dcontext, pc, pc, instr);
}

byte *
decode_from_copy(dcontext_t *dcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

byte *
decode_cti(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode(dcontext, pc, instr);
}

byte *
decode_next_pc(dcontext_t *dcontext, byte *pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

int
decode_sizeof(dcontext_t *dcontext, byte *pc, int *num_prefixes)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

bool
decode_raw_is_jmp(dcontext_t *dcontext, byte *pc)
{
    uint enc = *(uint *)pc;
    return ((enc & 0xfc000000) == 0x14000000);
}

byte *
decode_raw_jmp_target(dcontext_t *dcontext, byte *pc)
{
    uint enc = *(uint *)pc;
    return pc + ((enc & 0x1ffffff) << 2) - ((enc & 0x2000000) << 2);
}

const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

const instr_info_t *
get_next_instr_info(const instr_info_t * info)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

byte
decode_first_opcode_byte(int opcode)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

opnd_size_t
resolve_variable_size(decode_info_t *di, opnd_size_t sz, bool is_reg)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

bool
optype_is_indir_reg(int optype)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
optype_is_reg(int optype)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
optype_is_gpr(int optype)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

#ifdef DEBUG
# ifndef STANDALONE_DECODER
void
check_encode_decode_consistency(dcontext_t *dcontext, instrlist_t *ilist)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}
# endif /* STANDALONE_DECODER */

void
decode_debug_checks_arch(void)
{
    /* FIXME i#1569: NYI */
}
#endif /* DEBUG */

#ifdef DECODE_UNIT_TEST

# include "instr_create.h"

int main()
{
    bool res = true;
    standalone_init();
    return res;
}

#endif /* DECODE_UNIT_TEST */
