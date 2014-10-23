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
 * * Neither the name of Goole, Inc. nor the names of its contributors may be
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

/* file "decode_private.h" */

#ifndef DECODE_PRIVATE_H
#define DECODE_PRIVATE_H


/* instr_info_t.type special codes: */
enum {
    /* not a valid opcode */
    INVALID = OP_LAST + 1,
    EXT_OPC4,    /* Indexed by bits 7:4 */
    EXT_OPC4X,   /* Indexed by bits 7:4 in specific manner: see table */
    EXT_OPC4Y,   /* Indexed by bits 7:4 w/ 1st entry covering all evens */
    EXT_IMM1916, /* Indexed by whether imm4 in 19:16 is zero or not */
    EXT_IMM5,    /* Indexed by whether imm5 11:7 is zero or not */
    EXT_BITS0,   /* Indexed by bits 2:0 */
    EXT_BITS8,   /* Indexed by bits 9:8 */
    EXT_BIT4,    /* Indexed by bit 4 */
    EXT_BIT9,    /* Indexed by bit 9 */
    EXT_RDPC,    /* Indexed by whether RD != PC */
    /* else, from OP_ enum */
};

/* instr_info_t.opcode: holds all the 1 bits for the opcode.
 * We set it first, so we don't need to store 0's explicitly.
 */

/* instr_info_t.name: we store lowercase, and the disassembler upcases it
 * for ARM-style disasm.
 */

/* instr_info.t operands: because the type tells us the encoding bit location,
 * we are free to reorder them.  We pick the asm order.
 */

/* instr_info_t.flags values: */
enum {
    DECODE_EXTRA_OPERANDS     = 0x0001, /* additional opnds in entry at code field */
    DECODE_EXTRA_SHIFT        = 0x0002, /* has 2 additional srcs @exop[0] */
    DECODE_EXTRA_WRITEBACK    = 0x0004, /* has 1 additional src @exop[1] */
    DECODE_4_SRCS             = 0x0008, /* dst2==src1, src1==src2, etc. */
    DECODE_3_DSTS             = 0x0010, /* src1==dst3, src2==src1, etc. */
    DECODE_PREDICATE          = 0x0020, /* takes a predicate */
    DECODE_PREDICATE_AL_ONLY  = 0x0040, /* takes AL predicate only */
    DECODE_UNPREDICTABLE      = 0x0080, /* unpredictable according to ISA spec */
    /* ARM versions we care about */
    DECODE_ARM_V8             = 0x0100, /* added in v8: not present in v7 */
};

/* instr_info_t.code:
 * + for EXTENSION and *_EXT: index into extensions table
 * + for OP_: pointer to next entry of that opcode
 * + may also point to extra operand table
 */

struct _decode_info_t {
    uint instr_word;

    uint opcode;

    /* For pc-relative references */
    byte *start_pc;
    byte *final_pc;
    byte *orig_pc;

    /* XXX i#1550: add dr_isa_mode_t isa_mode field to dcontext and
     * instr_t and cache it here for use when decoding.
     */

    /* For instr_t* target encoding */
    ptr_int_t cur_note;
    bool has_instr_opnds;
};


/* N.B.: if you change the type enum, change the string names for
 * them, kept in encode.c
 */

/* Operand types have 2 parts, type and size.  The type tells us in which bits
 * the operand is encoded, and the type of operand.
 */
enum {
    /* operand types */
    TYPE_NONE,  /* must be 0 for invalid_instr */

    /* We name the registers according to their encoded position: A, B, C, D.
     * XXX: Rd is T32-11:8; T16-2:0; A64-4:0 so not always "C"
     *
     * XXX: record which registers are "unpredictable" if PC (or SP, or LR) is
     * passed?  Many are, for many different opcodes.
     */
    TYPE_R_A,   /* A32-19:16 = Rn: source register, often memory base */
    TYPE_R_B,   /* A32-15:12 = Rd or Rt: dest reg */
    TYPE_R_C,   /* A32-11:8  = Rs: source register, often used as shift value */
    TYPE_R_D,   /* A32-3:0   = Rm: source register, often used as offset */

    TYPE_R_A_TOP, /* top half of register */
    TYPE_R_B_TOP, /* top half of register */
    TYPE_R_C_TOP, /* top half of register */
    TYPE_R_D_TOP, /* top half of register */

    TYPE_R_D_NEGATED, /* register's value is negated */

    TYPE_R_B_EVEN, /* Must be an even-numbered reg */
    TYPE_R_B_PLUS1, /* Subsequent reg after prior TYPE_R_B_EVEN opnd */
    TYPE_R_D_EVEN, /* Must be an even-numbered reg */
    TYPE_R_D_PLUS1, /* Subsequent reg after prior TYPE_R_D_EVEN opnd */

    TYPE_CR_A, /* Control register in A slot */
    TYPE_CR_B, /* Control register in B slot */
    TYPE_CR_C, /* Control register in C slot */
    TYPE_CR_D, /* Control register in D slot */

    TYPE_APSR, /* Application Program Status Register */
    TYPE_SPSR, /* Saved Program Status Register */
    TYPE_CPSR, /* Current Program Status Register */
    /* XXX: do we need these:
     * DSPSR = Debug Saved Program Status Register
     * FPCR  = Floating-Point Control Register
     * FPSR  = Floating-Point Status Register
     */

    /* XXX: Register types we'll want to add:
     *   Wn = 32-bit GPR
     *   Xn = 64-bit GPR
     *   Vn = general name for 128-bit SIMD register
     *   Qn = 128-bit SIMD
     *   Dn = bottom 64-bit SIMD
     *   Sn = bottom 32-bit SIMD
     *   Hn = bottom 16-bit SIMD
     *   Bn = bottom 8-bit SIMD
     */

    /* Immediates are at several different bit positions and come in several
     * different sizes.  We considered storing a bitmask to cover any type
     * of immediate, but there are few enough that we are enumerating them:
     */
    TYPE_I_b0,
    TYPE_NI_b0, /* negated immed */
    TYPE_I_b3,
    TYPE_I_b5,
    TYPE_I_b7,
    TYPE_I_b8,
    TYPE_I_b10,
    TYPE_I_b16,
    TYPE_I_b20,
    TYPE_I_b0_b8,
    TYPE_NI_b0_b8, /* negated immed */
    TYPE_I_b0_b16,
    TYPE_I_b16_b9,
    TYPE_I_b16_b8,

    TYPE_SHIFT_b5,
    TYPE_SHIFT_b6, /* value is :0 */
    TYPE_SHIFT_LSL,   /* shift logical left */
    TYPE_SHIFT_ASR,   /* shift arithmetic right */

    TYPE_L_8,   /* 8-bit register list */
    TYPE_L_13,  /* 13-bit register list */
    TYPE_L_16,  /* 16-bit register list */

    /* All memory addressing modes use fixed base and index registers:
     * A32: base  = RD 19:16 ("Rn" in manual)
     *      index = RA  3:0  ("Rm" in manual)
     * T16: base  =
     *      index =
     * T32: base  =
     *      index =
     * A64: base  =
     *      index =
     *
     * Shifted registers always use sh2, i5.
     *
     * To be compatible w/ x86, we don't want to list the index, offset, or shift
     * operands separately for regular offset addressing: we want to hide them
     * inside the memref.  So we have to record exactly how to decode and encode
     * each piece.
     *
     * We don't encode in the memref whether it has writeback ("[Rn + Rm]!") or
     * is post-indexed ("[Rn], Rm"): the disassembler has to look at the other
     * opnds to figure out how to write down the memref, and single-memref-opnd
     * disasm will NOT contain writeback or post-index info.
     */
    TYPE_M,           /* mem w/ just base */
    TYPE_M_POS_REG,   /* mem offs + reg index */
    TYPE_M_NEG_REG,   /* mem offs - reg index */
    TYPE_M_POS_SHREG, /* mem offs + reg-shifted (or extended for A64) index */
    TYPE_M_NEG_SHREG, /* mem offs - reg-shifted (or extended for A64) index */
    TYPE_M_POS_I12,   /* mem offs + 12-bit immed @ 11:0 (A64: 21:10 + scaled) */
    TYPE_M_NEG_I12,   /* mem offs - 12-bit immed @ 11:0 (A64: 21:10 + scaled) */
    TYPE_M_SI9,       /* mem offs + signed 9-bit immed @ 20:12 */
    TYPE_M_POS_I8,    /* mem offs + 8-bit immed @ 7:0 */
    TYPE_M_NEG_I8,    /* mem offs - 8-bit immed @ 7:0 */
    TYPE_M_POS_I4_4,  /* mem offs + 8-bit immed split @ 11:8|3:0 */
    TYPE_M_NEG_I4_4,  /* mem offs - 8-bit immed split @ 11:8|3:0 */
    TYPE_M_SI7,       /* mem offs + signed 7-bit immed @ 6:0 */
    TYPE_M_POS_I5,    /* mem offs + 5-bit immed @ 5:0 */

    TYPE_M_PCREL_S9,  /* mem offs pc-relative w/ signed 9-bit immed 23:5 scaled */
    TYPE_M_PCREL_U9,  /* mem offs pc-relative w/ unsigned 9-bit immed 23:5 scaled */

    TYPE_M_UP_OFFS,   /* mem w/ base plus ptr-sized disp */
    TYPE_M_DOWN,      /* mem w/ base pointing at endpoint */
    TYPE_M_DOWN_OFFS, /* mem w/ base minus ptr-sized disp pointing at endpoint */

   /* when adding new types, update type_names[] in encode.c */
};

/* exported tables */
extern const instr_info_t A32_pred_opc8[];
extern const instr_info_t A32_nopred_opc8[];
extern const instr_info_t A32_ext_opc4x[][6];
extern const instr_info_t A32_ext_opc4y[][9];
extern const instr_info_t A32_ext_opc4[][16];
extern const instr_info_t A32_ext_imm1916[][2];
extern const instr_info_t A32_ext_bits0[][8];
extern const instr_info_t A32_ext_bits8[][4];
extern const instr_info_t A32_ext_bit9[][2];
extern const instr_info_t A32_ext_bit4[][2];
extern const instr_info_t A32_ext_RDPC[][2];
extern const instr_info_t A32_ext_imm5[][2];
extern const instr_info_t A32_extra_operands[];

/* tables that translate opcode enums into pointers into decoding tables */
extern const instr_info_t * const op_instr_A32[];

#endif /* DECODE_PRIVATE_H */
