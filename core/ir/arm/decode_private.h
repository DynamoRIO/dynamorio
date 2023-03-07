/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
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
    /* Our EXT_ names are slightly different for A32 vs T32: we use "BITS" and only
     * specify the bottom for multi-bit sets for A32, but list which halfword (A vs
     * B) and the range for bitsets for T32.  We could normalize the two but having
     * them different is a feature.
     */
    EXT_OPC4,    /* Indexed by bits 7:4 */
    EXT_OPC4X,   /* Indexed by bits 7:4 in specific manner: see table */
    EXT_OPC4Y,   /* Indexed by bits 7:4 w/ 1st entry covering all evens */
    EXT_IMM1916, /* Indexed by whether imm4 in 19:16 is 0, 1, or other */
    EXT_IMM5,    /* Indexed by whether imm5 11:7 is zero or not */
    EXT_BITS0,   /* Indexed by bits 2:0 */
    EXT_BITS8,   /* Indexed by bits 9:8 */
    EXT_BIT4,    /* Indexed by bit 4 */
    EXT_BIT5,    /* Indexed by bit 5 */
    EXT_BIT9,    /* Indexed by bit 9 */
    EXT_FP,      /* Indexed by bits 11:8 but collapsed */
    EXT_FPA,     /* Indexed by bits 6,4 but invalid if ==3 */
    EXT_FPB,     /* Indexed by bits 6:4 */
    EXT_BITS16,  /* Indexed by bits 19:16 */
    EXT_RAPC,    /* Indexed by whether RA != PC */
    EXT_RBPC,    /* Indexed by whether RB != PC */
    EXT_RDPC,    /* Indexed by whether RD != PC */
    /* A32 unpred only */
    EXT_BIT6,    /* Indexed by bit 6 */
    EXT_BIT7,    /* Indexed by bit 7 */
    EXT_BIT19,   /* Indexed by bit 19 */
    EXT_BIT22,   /* Indexed by bit 22 */
    EXT_BITS20,  /* Indexed by bits 23:20 */
    EXT_IMM1816, /* Indexed by whether imm3 in 18:16 is zero or not */
    EXT_IMM2016, /* Indexed by whether imm5 in 20:16 is zero or not */
    EXT_SIMD6,   /* Indexed by 6 bits 11:8,6,4 */
    EXT_SIMD5,   /* Indexed by bits 11:8,5 */
    EXT_SIMD5B,  /* Indexed by bits 18:16,8:7 */
    EXT_SIMD8,   /* Indexed by bits 11:8,6:4, but 6:4 collapsed */
    EXT_SIMD6B,  /* Indexed by bits 10:8,7:6 + extra set of 7:6 for bit 11 being set */
    EXT_SIMD2,   /* Indexed by bits 11,6 */
    EXT_IMM6L,   /* Indexed by bits 10:8,6 */
    EXT_VLDA,    /* Indexed by bits (11:8,7:6)*3+X where X based on value of 3:0 */
    EXT_VLDB,    /* Indexed by bits (11:8,Y)*3+X (see table descr) */
    EXT_VLDC,    /* Indexed by bits (7:5)*3+X where X based on value of 3:0 */
    EXT_VLDD,    /* Indexed by bits (7:4)*3+X where X based on value of 3:0 */
    EXT_VTB,     /* Indexed by 11:10 and 9:8,6 in a special way */
    /* T32 32-bit only */
    EXT_A10_6_4,  /* Indexed by bits A10,6:4 */
    EXT_A9_7_eq1, /* Indexed by whether bits A9:7 == 0xf */
    EXT_B10_8,    /* Indexed by bits B10:8 */
    EXT_B2_0,     /* Indexed by bits B2:0 */
    EXT_B5_4,     /* Indexed by bits B5:4 */
    EXT_B6_4,     /* Indexed by bits B6:4 */
    EXT_B7_4,     /* Indexed by bits B7:4 */
    EXT_B7_4_eq1, /* Indexed by whether bits B7:4 == 0xf */
    EXT_B4,       /* Indexed by bit  B4 */
    EXT_B5,       /* Indexed by bit  B5 */
    EXT_B7,       /* Indexed by bit  B7 */
    EXT_B11,      /* Indexed by bit  B11 */
    EXT_B13,      /* Indexed by bit  B13 */
    EXT_FOPC8,    /* Indexed by bits A11:4 but stop at 0xfb */
    EXT_IMM126,   /* Indexed by whether imm5 in B12:12,7:6 is 0 or not */
    EXT_OPCBX,    /* Indexed by bits B11:8 but minus x1-x7 */
    EXT_RCPC,     /* Indexed by whether RC != PC */
    /* T32 16-bit only */
    EXT_11,    /* Indexed by bit  11 */
    EXT_11_10, /* Indexed by bits 11:10 */
    EXT_11_9,  /* Indexed by bits 11:9 */
    EXT_11_8,  /* Indexed by bits 11:8 */
    EXT_10_9,  /* Indexed by bits 10:9 */
    EXT_10_8,  /* Indexed by whether Rn(10:8) is also in the reglist(7:0) */
    EXT_10_6,  /* Indexed by whether imm 10:6 is zero or not */
    EXT_9_6,   /* Indexed by bits 9:6 */
    EXT_7_6,   /* Indexed by bits 7:6 */
    EXT_7,     /* Indexed by bit  7 */
    EXT_5_4,   /* Indexed by bits 5:4 */
    EXT_6_4,   /* Indexed by bits 6:4 */
    EXT_3_0,   /* Indexed by whether imm 3:0 is zero or not */
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
    DECODE_EXTRA_OPERANDS = 0x0001,   /* additional opnds in entry at code field */
    DECODE_EXTRA_SHIFT = 0x0002,      /* has 2 additional srcs @exop[0] */
    DECODE_EXTRA_WRITEBACK = 0x0004,  /* has 1 additional src @exop[1] */
    DECODE_EXTRA_WRITEBACK2 = 0x0008, /* has 2 additional src @exop[2] */
    DECODE_4_SRCS = 0x0010,           /* dst2==src1, src1==src2, etc. */
    DECODE_3_DSTS = 0x0020,           /* src1==dst3, src2==src1, etc. */
    DECODE_PREDICATE_28 = 0x0040,     /* has predicate in bits 31:28 */
    DECODE_PREDICATE_28_AL = 0x0080,  /* accepts only AL predicate in 31:28 */
    DECODE_PREDICATE_22 = 0x0100,     /* has predicate (not AL or OP) in bits 25:22 */
    DECODE_PREDICATE_8 = 0x0200,      /* has predicate (not AL or OP) in bits 11:8 */
    DECODE_UNPREDICTABLE = 0x0400,    /* unpredictable according to ISA spec */
    /* ARM versions we care about */
    DECODE_ARM_V8 = 0x0800,  /* added in v8: not present in v7 */
    DECODE_ARM_VFP = 0x1000, /* VFP instruction */
    /* XXX: running out of space here.  We could take the top of the eflags
     * bits as we're only using through 0x00000800 now.
     */
};

/* Because instructions in an IT block are correlated with the IT instruction,
 * we need a way to keep track of IT block state and avoid using stale state
 * on encode/decode.
 * For decoding, we added pc information in addition to it_block_info_t,
 * to only continue if the pc match.
 * For encoding, we store the instr pointer to ensure we're still encoding
 * in the same block.  We briefly tried requiring a call to enable tracking,
 * only used during full instrlist encoding, but too many cases perform
 * individual encoding (e.g., instr_length()) for that to easily work.
 */
/* it_block_info_t: keeps track of the IT block state */
typedef struct _it_block_info_t {
    byte num_instrs;
    byte firstcond;
    byte preds; /* bitmap for */
    /* XXX: gcc generates incorrect code for this signed bitfield.  We
     * work around it in encode_in_it_block(), where we assume 4 bits.
     */
    char cur_instr : 4; /* 0-3 */
} it_block_info_t;

typedef struct _encode_state_t {
    it_block_info_t itb_info;
    instr_t *instr;
} encode_state_t;

typedef struct _decode_state_t {
    it_block_info_t itb_info;
    app_pc pc;
} decode_state_t;

/* instr_info_t.code:
 * + for EXTENSION and *_EXT: index into extensions table
 * + for OP_: pointer to next entry of that opcode
 * + may also point to extra operand table
 */

struct _decode_info_t {
    dr_isa_mode_t isa_mode;

    /* We fill in instr_word for T32 too.  For T32.32, we put halfwordB up high
     * (to match our table opcodes, for easier human reading, and to enable
     * sharing the A32 bit position labels).  This does NOT match the little-endian
     * encoding of both halfwords as one doubleword: this matches big-endian.
     */
    uint instr_word;
    ushort halfwordA; /* T32 only */
    ushort halfwordB; /* T32.32 only */
    bool T32_16;      /* whether T32.16 as opposed to T32.32 or A32 */

    uint opcode;
    uint predicate;
    bool check_reachable; /* includes checking predication legality */

    /* For pc-relative references */
    byte *start_pc;
    byte *final_pc;
    byte *orig_pc;

    /* For decoding LSB=1 auto-Thumb addresses (i#1688) we keep the LSB=1 decoration */
    byte *decorated_pc;

    /* For instr_t* target encoding */
    ptr_int_t cur_offs;
    bool has_instr_opnds;

    /* For IT block */
    decode_state_t decode_state;
    encode_state_t encode_state;

    /***************************************************
     * The rest of the fields are zeroed when encoding each template
     */
    /* For encoding error messages */
    const char *errmsg; /* can contain one integer format parameter */
    int errmsg_param;

    /* For decoding reglists.  Max 1 reglist per template (we check this in
     * decode_debug_checks_arch()).
     */
    int reglist_sz;
    opnd_t *mem_needs_reglist_sz;
    bool mem_adjust_disp_for_reglist;
    /* For encoding reglists */
    uint reglist_start;
    uint reglist_stop;
    bool reglist_simd;
    opnd_size_t reglist_itemsz;
    uint reglist_min_num;
    int memop_sz;
    /* For decoding and encoding shift types.  We need to coordinate across two
     * adjacent immediates.  This is set to point at the first one.
     */
    bool shift_has_type;
    uint shift_type_idx;
    bool shift_uses_immed;
    dr_shift_type_t shift_type;
    bool shift_1bit; /* 1 bit in b6/b21; else, 2 bits in b4/b5 */
    /* Our IR and decode templates store the disp/index/shifted-index inside
     * the memory operand, but also have the same elements separate for writeback
     * or post-indexed addressing.  We need to make sure they match.
     * We assume that the writeback/postindex args are sources that are later
     * in the src array than memop, if memop is a source.
     */
    reg_id_t check_wb_base;
    reg_id_t check_wb_index;
    opnd_size_t check_wb_disp_sz;
    int check_wb_disp;
    bool check_wb_shift;
    uint check_wb_shift_type;   /* raw encoded value */
    uint check_wb_shift_amount; /* raw encoded value */
    /* For modified immed values */
    uint mod_imm_enc;
};

typedef struct _op_to_instr_info_t {
    const instr_info_t *const A32;
    const instr_info_t *const T32;
    const instr_info_t *const T32_it;
} op_to_instr_info_t;

/* N.B.: if you change the type enum, change the string names for
 * them, kept in encode.c
 */

/* Operand types have 2 parts, type and size.  The type tells us in which bits
 * the operand is encoded, and the type of operand.
 * For T32.32, we share the A32 bit labels by considering halfwordA to be
 * placed above halfwordB to form a big-endian doubleword.
 */
enum {
    /* operand types */
    TYPE_NONE, /* must be 0 for invalid_instr */

    /* We name the registers according to their encoded position: A, B, C, D.
     * XXX: Rd is T32-11:8; T32.16-2:0; A64-4:0 so not always "C"
     *
     * XXX: record which registers are "unpredictable" if PC (or SP, or LR) is
     * passed?  Many are, for many different opcodes.
     */
    TYPE_R_A, /* A/T32-19:16 = Rn: source register, often memory base */
    TYPE_R_B, /* A/T32-15:12 = Rd (A32 dest reg) or Rt (src reg) */
    TYPE_R_C, /* A/T32-11:8  = Rd (T32 dest reg) or Rs (A32, often shift value) */
    TYPE_R_D, /* A/T32-3:0   = Rm: source register, often used as offset */

    /* T32.16: 4-bit */
    TYPE_R_U, /* T32.16-6:3   = Rm: src reg */
    TYPE_R_V, /* T32.16-7,2:0 = DN:Rdn or DM:Rdm: src and dst reg */
    /* T32.16: 3-bit */
    TYPE_R_W,     /* T32.16-10:8  = Rd: dst reg, Rn: src reg, Rt */
    TYPE_R_X,     /* T32.16-8:6   = Rm: src reg */
    TYPE_R_Y,     /* T32.16-5:3   = Rm or Rn: src reg */
    TYPE_R_Z,     /* T32.16-2:0   = Rd: dst reg, Rn or Rm: src reg, Rt */
    TYPE_R_V_DUP, /* T32.16-7,2:0 = DN:Rdn or DM:Rdm, destructive TYPE_R_V */
    TYPE_R_W_DUP, /* T32.16-10:8  = Rdn, destructive TYPE_R_W */
    TYPE_R_Z_DUP, /* T32.16-2:0   = Rdn, destructive TYPE_R_Z */

    TYPE_R_A_TOP, /* top half of register */
    TYPE_R_B_TOP, /* top half of register */
    TYPE_R_C_TOP, /* top half of register */
    TYPE_R_D_TOP, /* top half of register */

    TYPE_R_D_NEGATED, /* register's value is negated */

    TYPE_R_B_EVEN,  /* Must be an even-numbered reg */
    TYPE_R_B_PLUS1, /* Subsequent reg after prior TYPE_R_B_EVEN opnd */
    TYPE_R_D_EVEN,  /* Must be an even-numbered reg */
    TYPE_R_D_PLUS1, /* Subsequent reg after prior TYPE_R_D_EVEN opnd */

    /* An opnd with this type must come immediately after a TYPE_R_D opnd */
    TYPE_R_A_EQ_D, /* T32-19:16 = must be identical to Rm in 3:0 (OP_clz) */

    TYPE_CR_A, /* Coprocessor register in A slot */
    TYPE_CR_B, /* Coprocessor register in B slot */
    TYPE_CR_C, /* Coprocessor register in C slot */
    TYPE_CR_D, /* Coprocessor register in D slot */

    TYPE_V_A,       /* A32/T32 = 7,19:16, but for Q regs 7,19:17   = Vn src reg */
    TYPE_V_B,       /* A32/T32 = 22,15:12, but for Q regs 22,15:13 = Vd dst reg */
    TYPE_V_C,       /* A32/T32 = 5,3:0, but for Q regs 5,3:1       = Vm src reg */
    TYPE_V_C_3b,    /* A32-2:0      = Vm<2:0>: some (bottom) part of 128-bit src reg */
    TYPE_V_C_4b,    /* A32-3:0      = Vm<3:0>: some (bottom) part of 128-bit src reg */
    TYPE_W_A,       /* A32-19:16,7  = Vn VFP non-double: part of 128-bit src reg */
    TYPE_W_B,       /* A32-15:12,22 = Vd VFP non-double: part of 128-bit dst reg */
    TYPE_W_C,       /* A32-3:0,5    = Vm VFP non-double: part of 128-bit src reg */
    TYPE_W_C_PLUS1, /* Subsequent reg after TYPE_W_C */

    TYPE_SPSR,  /* Saved Program Status Register */
    TYPE_CPSR,  /* Current Program Status Register */
    TYPE_FPSCR, /* Floating Point Status and Control Register */
    TYPE_LR,    /* Link register */
    TYPE_SP,    /* Stack pointer */
    TYPE_PC,    /* PC register */

    /* FIXME i#1551: some immediates have built-in shifting or scaling: we
     * need to add handling for that.
     */
    /* Immediates are at several different bit positions and come in several
     * different sizes.  We considered storing a bitmask to cover any type
     * of immediate, but there are few enough that we are enumerating them.
     * For split types, our type + the size does not specify how many bits are at
     * each bit location: we rely on the decoder and encoder enumerating all the
     * possibilities.
     */
    TYPE_I_b0,
    TYPE_I_x4_b0,
    TYPE_I_SHIFTED_b0,
    TYPE_NI_b0,    /* negated immed */
    TYPE_NI_x4_b0, /* negated immed */
    TYPE_I_b3,
    TYPE_I_b4,
    TYPE_I_b5,
    TYPE_I_b6,
    TYPE_I_b7,
    TYPE_I_b8,
    TYPE_I_b9,
    TYPE_I_b10,
    TYPE_I_b16,
    TYPE_I_b17,
    TYPE_I_b18,
    TYPE_I_b19,
    TYPE_I_b20,
    TYPE_I_b21,    /* OP_vmov */
    TYPE_I_b0_b5,  /* OP_cvt: immed is either 32 or 16 minus [3:0,5] */
    TYPE_I_b4_b8,  /* OP_mrs T32 */
    TYPE_I_b4_b16, /* OP_mrs T32 */
    TYPE_I_b5_b3,  /* OP_vmla scalar: M:Vm<3> */
    TYPE_I_b8_b0,
    TYPE_NI_b8_b0, /* negated immed */
    TYPE_I_b8_b16,
    TYPE_I_b8_b24_b16_b0,  /* A32 OP_vbic, etc.: 11:8,24,18:16,3:0 AdvSIMDExpandImm */
    TYPE_I_b8_b28_b16_b0,  /* T32 OP_vbic, etc.: 11:8,28,18:16,3:0 AdvSIMDExpandImm */
    TYPE_I_b12_b6,         /* T32-14:12,7:6 */
    TYPE_I_b16_b0,         /* If 1 byte, then OP_vmov_f{32,64}: VFPExpandImm */
    TYPE_I_b16_b26_b12_b0, /* OP_movw T32-19:16,26,14:12,7:0 */
    TYPE_I_b21_b5,         /* OP_vmov: 21,6:5 */
    TYPE_I_b21_b6,         /* OP_vmov: 21,6 */
    TYPE_I_b26_b12_b0,     /* T32-26,14:12,7:0 + complex T32 "modified immed" encoding */
    TYPE_I_b26_b12_b0_z,   /* T32-26,14:12,7:0 + zero extend immed encoding */

    /* PC-relative jump targets.  All are x2 unless specified. */
    TYPE_J_b0,                 /* T16-OP_b: signed immed is stored as value/2 */
    TYPE_J_x4_b0,              /* OP_b, OP_bl: signed immed is stored as value/4 */
    TYPE_J_b0_b24,             /* OP_blx imm24:H:0 */
    TYPE_J_b9_b3,              /* OP_cb{n}z: ZeroExtend(i:imm5:0) [9,7:3]:0 */
    TYPE_J_b26_b11_b13_b16_b0, /* OP_b T32-26,11,13,21:16,10:0 x2 */
    /* OP_b T32-26,13,11,25:16,10:0 x2, but bits 13 and 11 are flipped if bit 26 is 0 */
    TYPE_J_b26_b13_b11_b16_b0,

    TYPE_SHIFT_b4,  /* T32-5:4 */
    TYPE_SHIFT_b5,  /* A32-6:5 */
    TYPE_SHIFT_b6,  /* value is :0 */
    TYPE_SHIFT_b21, /* value is :0 */
    TYPE_SHIFT_LSL, /* shift logical left */
    TYPE_SHIFT_ASR, /* shift arithmetic right */

    TYPE_L_8b,           /* 8-bit register list */
    TYPE_L_9b_LR,        /* T32.16-push 9-bit register list 0:M:000000:reg_list */
    TYPE_L_9b_PC,        /* T32.16-pop  9-bit register list P:0000000:reg_list  */
    TYPE_L_16b,          /* 16-bit register list */
    TYPE_L_16b_NO_SP,    /* 16-bit register list but no SP */
    TYPE_L_16b_NO_SP_PC, /* 16-bit register list but no SP or PC */
    TYPE_L_CONSEC,       /* Consecutive multimedia regs starting at prior opnd, w/ dword
                          * count in immed 7:0
                          */
    TYPE_L_VBx2,         /* 2 consecutive multimedia regs starting at TYPE_V_B */
    TYPE_L_VBx3,         /* 3 consecutive multimedia regs starting at TYPE_V_B */
    TYPE_L_VBx4,         /* 4 consecutive multimedia regs starting at TYPE_V_B */
    TYPE_L_VBx2D,        /* 2 doubly-spaced multimedia regs starting at TYPE_V_B */
    TYPE_L_VBx3D,        /* 3 doubly-spaced multimedia regs starting at TYPE_V_B */
    TYPE_L_VBx4D,        /* 4 doubly-spaced multimedia regs starting at TYPE_V_B */
    TYPE_L_VAx2,         /* 2 consecutive multimedia regs starting at TYPE_V_A */
    TYPE_L_VAx3,         /* 3 consecutive multimedia regs starting at TYPE_V_A */
    TYPE_L_VAx4,         /* 4 consecutive multimedia regs starting at TYPE_V_A */

    /* All memory addressing modes use fixed base and index registers in A32 and T32.32:
     * A32: base  = RA 19:16 ("Rn" in manual)
     *      index = RD  3:0  ("Rm" in manual)
     * T32.32: base  = RA 19:16 ("Rn" in manual)
     *         index = RD  3:0  ("Rm" in manual)
     *
     * T32.16 may use fixed register for index but different register for base.
     * T32.16: base  = RY  5:3  ("Rn" in manual)
     *                 RW 10:8  ("Rn" in manual) for ldm/stm
     *         index = RX  8:6  ("Rm" in manual)
     *
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
    TYPE_M,             /* mem w/ just base */
    TYPE_M_SP,          /* mem w/ just SP as base */
    TYPE_M_POS_REG,     /* mem offs + reg index */
    TYPE_M_NEG_REG,     /* mem offs - reg index */
    TYPE_M_POS_SHREG,   /* mem offs + reg-shifted (or extended for A64) index */
    TYPE_M_NEG_SHREG,   /* mem offs - reg-shifted (or extended for A64) index */
    TYPE_M_POS_LSHREG,  /* mem offs + LSL reg-shifted (T32: by 5:4) index */
    TYPE_M_POS_LSH1REG, /* mem offs + LSL reg-shifted by 1 index */
    TYPE_M_POS_I12,     /* mem offs + 12-bit immed @ 11:0 (A64: 21:10 + scaled) */
    TYPE_M_NEG_I12,     /* mem offs - 12-bit immed @ 11:0 (A64: 21:10 + scaled) */
    TYPE_M_SI9,         /* mem offs + signed 9-bit immed @ 20:12 */
    TYPE_M_POS_I8,      /* mem offs + 8-bit immed @ 7:0 */
    TYPE_M_NEG_I8,      /* mem offs - 8-bit immed @ 7:0 */
    TYPE_M_POS_I8x4,    /* mem offs + 4 * 8-bit immed @ 7:0 */
    TYPE_M_NEG_I8x4,    /* mem offs - 4 * 8-bit immed @ 7:0 */
    TYPE_M_SP_POS_I8x4, /* mem offs + 4 * 8-bit immed @ 7:0 with SP as base */
    TYPE_M_POS_I4_4,    /* mem offs + 8-bit immed split @ 11:8|3:0 */
    TYPE_M_NEG_I4_4,    /* mem offs - 8-bit immed split @ 11:8|3:0 */
    TYPE_M_SI7,         /* mem offs + signed 7-bit immed @ 6:0 */
    TYPE_M_POS_I5,      /* mem offs + 5-bit immed @ 10:6 */
    TYPE_M_POS_I5x2,    /* mem offs + 2 * 5-bit immed @ 10:6 */
    TYPE_M_POS_I5x4,    /* mem offs + 4 * 5-bit immed @ 10:6 */

    TYPE_M_PCREL_POS_I8x4, /* mem offs pc-relative + 4 * 8-bit immed @  7:0 */
    TYPE_M_PCREL_POS_I12,  /* mem offs pc-relative + 12-bit immed @ 11:0 */
    TYPE_M_PCREL_NEG_I12,  /* mem offs pc-relative - 12-bit immed @ 11:0 */
    TYPE_M_PCREL_S9,       /* mem offs pc-relative w/ signed 9-bit immed 23:5 scaled */
    TYPE_M_PCREL_U9,       /* mem offs pc-relative w/ unsigned 9-bit immed 23:5 scaled */

    TYPE_M_UP_OFFS,      /* mem w/ base plus ptr-sized disp */
    TYPE_M_DOWN,         /* mem w/ base pointing at start of last ptr-sized slot */
    TYPE_M_DOWN_OFFS,    /* mem w/ base minus ptr-sized disp pointing at last slot */
    TYPE_M_SP_DOWN_OFFS, /* mem w/ base minus ptr-sized (SP) disp pointing at last slot */

    TYPE_K, /* integer constant, size ignored, value stored in size */

    /* when adding new types, update type_names[] in encode.c */
    TYPE_BEYOND_LAST_ENUM,

    /* Non-incremental-enum valus */
    DECODE_INDEX_SHIFT_TYPE_BITPOS_A32 = 5,
    DECODE_INDEX_SHIFT_TYPE_SIZE = OPSZ_2b,
    DECODE_INDEX_SHIFT_AMOUNT_BITPOS_A32 = 7,
    DECODE_INDEX_SHIFT_AMOUNT_SIZE_A32 = OPSZ_5b,
    DECODE_INDEX_SHIFT_AMOUNT_BITPOS_T32 = 4,
    DECODE_INDEX_SHIFT_AMOUNT_SIZE_T32 = OPSZ_2b,

    SHIFT_ENCODING_DECODE = -1,
    SHIFT_ENCODING_LSL = 0,
    SHIFT_ENCODING_LSR = 1,
    SHIFT_ENCODING_ASR = 2,
    SHIFT_ENCODING_ROR = 3,
    SHIFT_ENCODING_RRX = 3,
};

/* exported tables */
extern const instr_info_t A32_pred_opc8[];
extern const instr_info_t A32_ext_opc4x[][6];
extern const instr_info_t A32_ext_opc4y[][9];
extern const instr_info_t A32_ext_opc4[][16];
extern const instr_info_t A32_ext_imm1916[][3];
extern const instr_info_t A32_ext_bits0[][8];
extern const instr_info_t A32_ext_bits8[][4];
extern const instr_info_t A32_ext_bit9[][2];
extern const instr_info_t A32_ext_bit5[][2];
extern const instr_info_t A32_ext_bit4[][2];
extern const instr_info_t A32_ext_fp[][3];
extern const instr_info_t A32_ext_opc4fpA[][3];
extern const instr_info_t A32_ext_opc4fpB[][8];
extern const instr_info_t A32_ext_bits16[][16];
extern const instr_info_t A32_ext_RAPC[][2];
extern const instr_info_t A32_ext_RBPC[][2];
extern const instr_info_t A32_ext_RDPC[][2];
extern const instr_info_t A32_ext_imm5[][2];
extern const instr_info_t A32_extra_operands[];
extern const instr_info_t A32_unpred_opc7[];
extern const instr_info_t A32_ext_bits20[][16];
extern const instr_info_t A32_ext_imm2016[][2];
extern const instr_info_t A32_ext_imm1816[][2];
extern const instr_info_t A32_ext_bit7[][2];
extern const instr_info_t A32_ext_bit6[][2];
extern const instr_info_t A32_ext_bit19[][2];
extern const instr_info_t A32_ext_bit22[][2];
extern const instr_info_t A32_ext_simd6[][64];
extern const instr_info_t A32_ext_simd5[][32];
extern const instr_info_t A32_ext_simd5b[][32];
extern const instr_info_t A32_ext_simd8[][80];
extern const instr_info_t A32_ext_simd6b[][36];
extern const instr_info_t A32_ext_simd2[][4];
extern const instr_info_t A32_ext_imm6L[][16];
extern const instr_info_t A32_ext_vldA[][132];
extern const instr_info_t A32_ext_vldB[][96];
extern const instr_info_t A32_ext_vldC[][24];
extern const instr_info_t A32_ext_vldD[][48];
extern const instr_info_t A32_ext_vtb[][9];

extern const instr_info_t T32_base_e[];
extern const instr_info_t T32_base_f[];
extern const instr_info_t T32_ext_fopc8[][192];
extern const instr_info_t T32_ext_A9_7_eq1[][2];
extern const instr_info_t T32_ext_bits_A10_6_4[][16];
extern const instr_info_t T32_ext_opcBX[][9];
extern const instr_info_t T32_ext_bits_B10_8[][8];
extern const instr_info_t T32_ext_bits_B7_4[][16];
extern const instr_info_t T32_ext_B7_4_eq1[][8];
extern const instr_info_t T32_ext_bits_B6_4[][8];
extern const instr_info_t T32_ext_bits_B5_4[][4];
extern const instr_info_t T32_ext_bits_B2_0[][8];
extern const instr_info_t T32_ext_bit_B4[][2];
extern const instr_info_t T32_ext_bit_B5[][2];
extern const instr_info_t T32_ext_bit_B7[][2];
extern const instr_info_t T32_ext_bit_B11[][2];
extern const instr_info_t T32_ext_bit_B13[][2];
extern const instr_info_t T32_ext_RAPC[][2];
extern const instr_info_t T32_ext_RBPC[][2];
extern const instr_info_t T32_ext_RCPC[][2];
extern const instr_info_t T32_ext_imm126[][2];
extern const instr_info_t T32_extra_operands[];

/* The coproc names more closely match A32 as the table split is similar */
extern const instr_info_t T32_coproc_e[];
extern const instr_info_t T32_coproc_f[];
extern const instr_info_t T32_ext_fp[][3];
extern const instr_info_t T32_ext_opc4[][16];
extern const instr_info_t T32_ext_imm1916[][3];
extern const instr_info_t T32_ext_opc4fpA[][3];
extern const instr_info_t T32_ext_opc4fpB[][8];
extern const instr_info_t T32_ext_bits16[][16];
extern const instr_info_t T32_ext_bits20[][16];
extern const instr_info_t T32_ext_imm2016[][2];
extern const instr_info_t T32_ext_imm1816[][2];
extern const instr_info_t T32_ext_bit6[][2];
extern const instr_info_t T32_ext_bit19[][2];
extern const instr_info_t T32_ext_simd6[][64];
extern const instr_info_t T32_ext_simd5[][32];
extern const instr_info_t T32_ext_simd5b[][32];
extern const instr_info_t T32_ext_simd8[][80];
extern const instr_info_t T32_ext_simd6b[][36];
extern const instr_info_t T32_ext_simd2[][4];
extern const instr_info_t T32_ext_imm6L[][16];
extern const instr_info_t T32_ext_vldA[][132];
extern const instr_info_t T32_ext_vldB[][96];
extern const instr_info_t T32_ext_vldC[][24];
extern const instr_info_t T32_ext_vldD[][48];
extern const instr_info_t T32_ext_vtb[][9];

extern const instr_info_t T32_16_opc4[];
extern const instr_info_t T32_16_ext_bit_11[][2];
extern const instr_info_t T32_16_ext_bits_11_10[][4];
extern const instr_info_t T32_16_ext_bits_11_9[][8];
extern const instr_info_t T32_16_ext_bits_11_8[][16];
extern const instr_info_t T32_16_ext_bits_9_6[][16];
extern const instr_info_t T32_16_ext_bit_7[][2];
extern const instr_info_t T32_16_ext_bits_5_4[][4];
extern const instr_info_t T32_16_ext_bits_10_9[][4];
extern const instr_info_t T32_16_ext_bits_10_8[][2];
extern const instr_info_t T32_16_ext_bits_7_6[][4];
extern const instr_info_t T32_16_ext_bits_6_4[][8];
extern const instr_info_t T32_16_ext_imm_10_6[][2];
extern const instr_info_t T32_16_ext_imm_3_0[][2];

extern const instr_info_t T32_16_it_opc4[];
extern const instr_info_t T32_16_it_ext_bit_11[][2];
extern const instr_info_t T32_16_it_ext_bits_11_10[][4];
extern const instr_info_t T32_16_it_ext_bits_11_9[][8];
extern const instr_info_t T32_16_it_ext_bits_11_8[][16];
extern const instr_info_t T32_16_it_ext_bits_9_6[][16];
extern const instr_info_t T32_16_it_ext_bit_7[][2];
extern const instr_info_t T32_16_it_ext_bits_10_9[][4];
extern const instr_info_t T32_16_it_ext_bits_10_8[][2];
extern const instr_info_t T32_16_it_ext_bits_7_6[][4];
extern const instr_info_t T32_16_it_ext_bits_6_4[][8];
extern const instr_info_t T32_16_it_ext_imm_10_6[][2];

/* table that translates opcode enums into pointers into decoding tables */
extern const op_to_instr_info_t op_instr[];

opnd_size_t
resolve_size_upward(opnd_size_t size);

opnd_size_t
resolve_size_downward(opnd_size_t size);

bool
optype_is_reg(int optype);

bool
optype_is_gpr(int optype);

uint
gpr_list_num_bits(byte optype);

void
it_block_info_init_immeds(it_block_info_t *info, byte mask, byte firstcond);

void
it_block_info_init(it_block_info_t *info, decode_info_t *di);

static inline void
it_block_info_reset(it_block_info_t *info)
{
    *(uint *)info = 0;
}

/* move to the next instr,
 * - return true if still in the IT block,
 * - return false if finish current IT block
 */
static inline bool
it_block_info_advance(it_block_info_t *info)
{
    ASSERT(info->num_instrs != 0);
    if (++info->cur_instr == info->num_instrs)
        return false; /* reach the end */
    return true;
}

static inline dr_pred_type_t
it_block_instr_predicate(it_block_info_t info, uint index)
{
    return (
        DR_PRED_EQ +
        (TEST(BITMAP_MASK(index), info.preds) ? info.firstcond : (info.firstcond ^ 0x1)));
}

#endif /* DECODE_PRIVATE_H */
