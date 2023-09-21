/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "decode_private.h" */

#ifndef DECODE_PRIVATE_H
#define DECODE_PRIVATE_H

/* Non-public prefix constants.
 * These are used only in the decoding tables.  We decode the
 * information into the operands.
 * For encoding these properties are specified in the operands,
 * with our encoder auto-adding the appropriate prefixes.
 */
/* We start after PREFIX_SEG_GS which is 0x40 */
#define PREFIX_DATA 0x0080
#define PREFIX_ADDR 0x0100
#define PREFIX_REX_W 0x0200
#define PREFIX_REX_R 0x0400
#define PREFIX_REX_X 0x0800
#define PREFIX_REX_B 0x1000
#define PREFIX_REX_GENERAL 0x2000 /* 0x40: only matters for SPL...SDL vs AH..BH */
#define PREFIX_REX_ALL \
    (PREFIX_REX_W | PREFIX_REX_R | PREFIX_REX_X | PREFIX_REX_B | PREFIX_REX_GENERAL)
#define PREFIX_SIZE_SPECIFIERS (PREFIX_DATA | PREFIX_ADDR | PREFIX_REX_ALL)

/* Unused except in decode tables (we encode the prefix into the opcodes) */
#define PREFIX_REP 0x4000   /* 0xf3 */
#define PREFIX_REPNE 0x8000 /* 0xf2 */

/* First 2 are only used during initial decode so if running out of
 * space could replace w/ byte value compare.
 */
#define PREFIX_VEX_2B 0x000010000
#define PREFIX_VEX_3B 0x000020000
#define PREFIX_VEX_L 0x000040000
/* Also only used during initial decode */
#define PREFIX_XOP 0x000080000
/* Prefixes which are used for AVX-512 */
#define PREFIX_EVEX_RR 0x000200000
#define PREFIX_EVEX_LL 0x000400000
#define PREFIX_EVEX_z 0x000800000
#define PREFIX_EVEX_b 0x001000000
#define PREFIX_EVEX_VV 0x002000000

/* branch hints show up as segment modifiers */
#define SEG_JCC_NOT_TAKEN SEG_CS
#define SEG_JCC_TAKEN SEG_DS

/* XXX:
 *  o get rid of sI?  I sign-extend all the Ib's on decoding, many are
 *    actually sign-extended that aren't marked sIb
 *  o had to add size to immed_int and base_disp, don't have it for indir_reg!
 *    that's fine for encoding, aren't duplicate instrs differing on size of
 *    indir reg data, but should set it properly for later IR passes...
 *  o rewrite emit.c so does emit_inst only once (instr_length() does it!)
 */

/* bits used to encode info in instr_info_t's opcode field */
enum {
    OPCODE_TWOBYTES = 0x00000010,
    OPCODE_REG = 0x00000020,
    OPCODE_MODRM = 0x00000040,
    OPCODE_SUFFIX = 0x00000080,
    OPCODE_THREEBYTES = 0x00000008,
};

/* instr_info_t: each table entry is one of these
 * For reading all bytes of instruction, only need to know:
 * 1) prefixes + opcode boundary
 * 2) whether to read modrm byte, from modrm get sib and disp
 * 3) whether to read immed bytes (types A, I, sI, J, and O)
 * The rest of the types are for interpretation only
 *
 * We have room for 2 destinations and 3 sources
 *   Instrs with 2 dests include push, pop, edx:eax, xchg, rep lods
 * Exceptions:
 *   pusha = 8 sources
 *   popa = 8 destinations
 *   enter = 3 dests, 4 srcs (incl. 2 immeds)
 *   cpuid = 4 dests
 *   cmpxchg8b = 3 dests, 5 srcs
 *   movs = 3 dests, rest of string instrs w/ rep/repne have 3+ dests
 * Represent exceptions as sequence of instrs?
 *   All 5 exceptions have only one form, can use list field to
 *   point into separate table that holds types of extra operands.
 *
 * Separate immed field and only 2 srcs?
 * enter has 2 immeds, all other instrs have <=1
 * there are a number of instrs that need 3 srcs even ignoring immeds!
 *   (pusha,shld,shrd,cmpxchg,cmpxhchg8b,rep outs,rep cmps, rep scas)
 * => no separate immed field
 *
 * FIXMEs:
 * lea = computes addr, doesn't touch mem!  how encode?
 * in & out, ins & outs: are I/O ports in memory?!?
 * Should we model fp stack changes?!?
 * All i_eSP really read 0x4(esp) or some other offset, depending on
 *   if take esp value before or after instrs, etc.
 * punpck*, pshuf*: say "reads entire x-bits, only uses half of them"
 *   table is inconsistent in this: for punpck* it gives a memory size of x/2
 *   (b/c Intel's table gives that), for pshuf* (there are others too I think)
 *   gives size x (not present in Intel's table)
 */
/* instr_info_t.opcode: split into bytes
 * 0th (ms) = prefix byte, if byte 3's 1st nibble's bit 3 and bit 4 are both NOT set;
 *            modrm byte, if  byte 3's 1st nibble's bit 3 IS set.
 *            suffix byte, if  byte 3's 1st nibble's bit 4 IS set.
 * 1st = 1st byte of opcode
 * 2nd = 2nd byte of opcode (if there are 2)
 * 3rd (ls) = split into nibbles
 *   1st nibble (ms) = if bit 1 (OPCODE_TWOBYTES) set, opcode has 2 bytes
 *                       if REQUIRES_EVEX then this bit instead means that this
 *                       instruction must have evex.b set
 *                     if bit 2 (OPCODE_REG) set, opcode has /n
 *                     if bit 3 (OPCODE_MODRM) set, opcode based on entire modrm
 *                       that modrm is stored as the byte 0.
 *                       if REQUIRES_VEX or REQUIRES_EVEX then this bit instead means
 *                       that this instruction must have vex.W or evex.W set.
 *                     if bit 4 (OPCODE_SUFFIX) set, opcode based on suffix byte
 *                       that byte is stored as the byte 0
 *                       if REQUIRES_VEX or REQUIRES_EVEX then this bit instead means
 *                       that this instruction must have vex.L or evex.L set.
 *                     XXX i#1312: Possibly a case for EVEX_LL (L') needs to be
 *                     supported at some point.
 *                     XXX: so we do not support an instr that has an opcode
 *                     dependent on both a prefix and the entire modrm or suffix!
 *                     XXX: perhaps we should use the flags rather than cramming all
 *                     of this information into the opcode byte, especially with the
 *                     VEX/EVEX dependent behavior.
 *   2nd nibble (ls) = bits 1-3 hold /n for OPCODE_REG
 *                     if bit 4 (OPCODE_THREEBYTES) is set, the opcode has
 *                       3 bytes, with the first being an implied 0x0f (so
 *                       the 2nd byte is stored as "1st" and 3rd as "2nd").
 */
/* instr_info_t.code:
 * + for PREFIX: one of the PREFIX_ constants, or SEG_ constant
 * + for EXTENSION and *_EXT: index into extensions table
 * + for OP_: pointer to next entry of that opcode
 * + may also point to extra operand table
 */

/* classification of instruction bytes up to modrm/disp/immed
 * these constants are used for instr_info_t.type field
 */
enum {
    /* not a valid opcode */
    INVALID = OP_LAST + 1,
    /* prefix byte */
    PREFIX,
    /* 0x0f = two-byte escape code */
    ESCAPE,
    /* floating point instruction escape code */
    FLOAT_EXT,
    /* opcode extension via reg field of modrm */
    EXTENSION,
    /* 2-byte instructions differing by presence of 0xf3/0x66/0xf2 prefixes */
    PREFIX_EXT,
    /* (rep prefix +) 1-byte-opcode string instruction */
    REP_EXT,
    /* (repne prefix +) 1-byte-opcode string instruction */
    REPNE_EXT,
    /* 2-byte instructions differing by mod bits of modrm */
    MOD_EXT,
    /* 2-byte instructions differing by rm bits of modrm */
    RM_EXT,
    /* 2-byte instructions whose opcode also depends on a suffix byte */
    SUFFIX_EXT,
    /* instructions that vary based on whether in 64-bit mode or not */
    X64_EXT,
    /* 3-byte opcodes beginning 0x0f 0x38 (SSSE3 and SSE4) */
    ESCAPE_3BYTE_38,
    /* 3-byte opcodes beginning 0x0f 0x3a (SSE4) */
    ESCAPE_3BYTE_3a,
    /* instructions differing if a rex.b prefix is present */
    REX_B_EXT,
    /* instructions differing if a rex.w prefix is present */
    REX_W_EXT,
    /* instructions differing based on whether part of a vex prefix */
    VEX_PREFIX_EXT,
    /* instructions differing based on whether (e)vex-encoded */
    E_VEX_EXT,
    /* instructions differing based on whether vex-encoded and vex.L */
    VEX_L_EXT,
    /* instructions differing based on vex.W */
    VEX_W_EXT,
    /* instructions differing based on whether part of an xop prefix */
    XOP_PREFIX_EXT,
    /* xop opcode map 8 */
    XOP_8_EXT,
    /* xop opcode map 9 */
    XOP_9_EXT,
    /* xop opcode map 10 */
    XOP_A_EXT,
    /* instructions differing based on evex */
    EVEX_PREFIX_EXT,
    /* instructions differing based on evex.W and evex.b */
    EVEX_Wb_EXT,
    /* XXX i#1312: We probably do not need EVEX_LL_EXT. L' does not seem to be part
     * of any instruction's opcode. Remove this comment when this has been finalized.
     */
    /* else, from OP_ enum */
};

/* instr_info_t modrm/extra operands flags up to DR_TUPLE_TYPE_BITPOS bits only! */
#define HAS_MODRM 0x01          /* else, no modrm */
#define HAS_EXTRA_OPERANDS 0x02 /* else, <= 2 dsts, <= 3 srcs */
/* if HAS_EXTRA_OPERANDS: */
#define EXTRAS_IN_CODE_FIELD 0x04 /* next instr_info_t pointed to by code field */
/* rather than split out into little tables of 32-bit vs OP_INVALID, we use a
 * flag to indicate opcodes that are invalid in particular modes:
 */
#define X86_INVALID 0x08
#define X64_INVALID 0x10
/* We use this to avoid needing a single-valid-entry subtable in prefix_extensions
 * when decoding.  This is never needed for encoding.
 */
#define REQUIRES_PREFIX 0x20
/* Instr must be encoded using vex. If this flag is not present, this instruction
 * is invalid if encoded using vex.
 */
#define REQUIRES_VEX 0x40
/* Instr must be encoded using a rex.w prefix.  We could expand this to
 * include other rex flags by combining with OPCODE_* flags, like REQUIRES_VEX
 * does today.
 */
#define REQUIRES_REX 0x80
/* Instr must be encoded with VEX.L=0.  If VEX.L=1 this is an invalid instr.
 * This helps us avoid creating a ton of vex_L_extensions entries.
 */
#define REQUIRES_VEX_L_0 0x0100
/* Instr must be encoded with VEX.L=1.  If VEX.L=0 this is an invalid instr.
 * This helps us avoid creating a ton of vex_L_extensions entries.
 * OPCODE_SUFFIX for REQUIRES_VEX means the same thing for encoding.
 */
#define REQUIRES_VEX_L_1 0x0200
/* Predicated via a jcc condition code */
#define HAS_PRED_CC 0x0400
/* Predicated via something complex */
#define HAS_PRED_COMPLEX 0x0800
/* Instr must be encoded using evex. If this flag is not present, this instruction
 * is invalid if encoded using evex.
 */
#define REQUIRES_EVEX 0x01000
/* Instr must be encoded with EVEX.LL=0.  If EVEX.LL=1 this is an invalid instr.
 */
#define REQUIRES_EVEX_LL_0 0x02000
/* Instruction's VSIB's index reg must be ymm. We are using this and the next flag
 * to constrain the VSIB's index register's size.
 */
#define REQUIRES_VSIB_YMM 0x04000
/* Instruction's VSIB's index reg must be zmm. */
#define REQUIRES_VSIB_ZMM 0x08000
/* EVEX default write mask not allowed. */
#define REQUIRES_NOT_K0 0x10000
/* 8-bit input size in the context of Intel's AVX-512 compressed disp8. */
#define DR_EVEX_INPUT_OPSZ_1 0x20000
/* 16-bit input size in the context of Intel's AVX-512 compressed disp8. */
#define DR_EVEX_INPUT_OPSZ_2 0x40000
/* 32-bit input size in the context of Intel's AVX-512 compressed disp8. */
#define DR_EVEX_INPUT_OPSZ_4 0x80000
/* 64-bit input size in the context of Intel's AVX-512 compressed disp8. */
#define DR_EVEX_INPUT_OPSZ_8 0x100000
/* The EVEX.b bit indicates all exceptions are suppresed. {sae} */
#define EVEX_b_IS_SAE 0x200000
/* The EVEX.L/EVEX.LL bits are used for rounding control, not size. {er} */
#define EVEX_L_LL_IS_ER 0x400000

struct _decode_info_t {
    uint opcode;
    /* Holds address and data size prefixes, as well as the prefixes
     * that are shared as-is with instr_t (PREFIX_SIGNIFICANT).
     * We assume we're in the default mode (32-bit or 64-bit,
     * depending on our build) and that the address and data size
     * prefixes can be treated as absolute.
     */
    uint prefixes;
    reg_id_t seg_override; /* REG enum of seg, REG_NULL if none */
    /* modrm info */
    byte modrm;
    byte mod;
    byte reg;
    byte rm;
    bool has_sib;
    byte scale;
    byte index;
    byte base;
    bool has_disp;
    int disp;
    /* immed info */
    opnd_size_t size_immed;
    opnd_size_t size_immed2;
    bool immed_pc_relativize : 1;
    bool immed_subtract_length : 1;
    bool immed_pc_rel_offs : 1;
    ushort immed_shift;
    ptr_int_t immed;
    ptr_int_t immed2; /* this additional field could be 32-bit on all platforms */
    /* These fields are used for decoding/encoding rip-relative data refs. */
    byte *start_pc;
    byte *final_pc;
    uint len;
    /* This field is only used when encoding rip-relative data refs, and for
     * re-relativizing level 1-3 relative jumps.  To save space we could make it a
     * union with disp.
     */
    byte *disp_abs;
#ifdef X64
    /* Since the mode when an instr_t is involved is per-instr rather than
     * per-dcontext we have our own field here instead of passing dcontext around.
     * It's up to the caller to set this field to match either the instr_t
     * or the dcontext_t field.
     */
    bool x86_mode;
#endif
    /* PR 302353: support decoding as though somewhere else */
    byte *orig_pc;
    /* these 3 prefixes may be part of opcode */
    bool data_prefix;
    bool rep_prefix;
    bool repne_prefix;
    /* vvvv bits for extra operand */
    union {
        byte vex_vvvv;
        byte evex_vvvv;
    };
    bool vex_encoded;
    bool evex_encoded;
    byte evex_aaa; /* aaa bits for opmask */
    /* for instr_t* target encoding */
    ptr_int_t cur_offs;
    bool has_instr_opnds;
    dr_tuple_type_t tuple_type;
    opnd_size_t input_size;
};

/* N.B.: if you change the type enum, change the string names for
 * them, kept in encode.c
 *
 * The TYPE_x enums are listed in 'Appendix A Opcode Map (Intel SDM Volume 2)'
 * specifically A.2.1 Codes for Addressing Method
 */

/* operand types have 2 parts, type and size */
enum {
    /* operand types */
    TYPE_NONE, /* must be 0 for invalid_instr */
    TYPE_A,    /* immediate that is absolute address */
    TYPE_B,    /* vex.vvvv field selects general-purpose register */
    TYPE_C,    /* reg of modrm selects control reg */
    TYPE_D,    /* reg of modrm selects debug reg */
    TYPE_E,    /* modrm selects reg or mem addr */
    /* I don't use type F, I have eflags info in separate field */
    TYPE_G,       /* reg of modrm selects register */
    TYPE_H,       /* vex.vvvv field selects xmm/ymm register */
    TYPE_I,       /* immediate */
    TYPE_J,       /* immediate that is relative offset of EIP */
    TYPE_L,       /* top 4 bits of 8-bit immed select xmm/ymm register */
    TYPE_M,       /* modrm selects mem addr */
    TYPE_O,       /* immediate that is memory offset */
    TYPE_P,       /* reg of modrm selects MMX */
    TYPE_Q,       /* modrm selects MMX or mem addr */
    TYPE_R,       /* modrm selects register */
    TYPE_S,       /* reg of modrm selects segment register */
    TYPE_V,       /* reg of modrm selects XMM */
    TYPE_W,       /* modrm selects XMM or mem addr */
    TYPE_X,       /* DS:(RE)(E)SI */
    TYPE_Y,       /* ES:(RE)(E)SDI */
    TYPE_P_MODRM, /* == Intel 'N': modrm selects MMX */
    TYPE_V_MODRM, /* == Intel 'U': modrm selects XMM */
    TYPE_1,
    TYPE_FLOATCONST,
    TYPE_XLAT,     /* DS:(RE)(E)BX+AL */
    TYPE_MASKMOVQ, /* DS:(RE)(E)DI */
    TYPE_FLOATMEM,
    TYPE_VSIB,          /* modrm selects mem addr with required VSIB */
    TYPE_REG,           /* hardcoded register */
    TYPE_XREG,          /* hardcoded register, default 32/64 bits depending on mode */
    TYPE_VAR_REG,       /* hardcoded register, default 32 bits, but can be
                         * 16 w/ data prefix or 64 w/ rex.w: equivalent of Intel 'v'
                         * == like OPSZ_4_rex8_short2 */
    TYPE_VARZ_REG,      /* hardcoded register, default 32 bits, but can be
                         * 16 w/ data prefix: equivalent of Intel 'z'
                         * == like OPSZ_4_short2 */
    TYPE_VAR_XREG,      /* hardcoded register, default 32/64 bits depending on mode,
                         * but can be 16 w/ data prefix: equivalent of Intel 'd64'
                         * == like OPSZ_4x8_short2 */
    TYPE_VAR_REGX,      /* hardcoded register, default 32 bits, but can be 64 w/ rex.w:
                         * equivalent of Intel 'y' == like OPSZ_4_rex8 */
    TYPE_VAR_ADDR_XREG, /* hardcoded register, default 32/64 bits depending on mode,
                         * but can be 16/32 w/ addr prefix: equivalent of Intel 'd64' */
    /* For x64 extensions (Intel '+r.') where rex.r can select an extended
     * register (r8-r15): we could try to add a flag that modifies the above
     * register types, but we'd have to stick it inside some stolen bits.  For
     * simplicity, we just make each combination a separate type:
     */
    TYPE_REG_EX,      /* like TYPE_REG but extendable. used for mov_imm 8-bit immed */
    TYPE_VAR_REG_EX,  /* like TYPE_VAR_REG (OPSZ_4_rex8_short2) but extendable.
                       * used for xchg and mov_imm 'v' immed. */
    TYPE_VAR_XREG_EX, /* like TYPE_VAR_XREG (OPSZ_4x8_short2) but extendable.
                       * used for pop and push. */
    TYPE_VAR_REGX_EX, /* like TYPE_VAR_REGX but extendable.  used for bswap. */
    TYPE_INDIR_E,
    TYPE_INDIR_REG,
    TYPE_INDIR_VAR_XREG,         /* indirected register that only varies by stack segment,
                                  * with a base of 32/64 depending on the mode;
                                  * indirected size varies with data prefix */
    TYPE_INDIR_VAR_REG,          /* indirected register that only varies by stack segment,
                                  * with a base of 32/64;
                                  * indirected size varies with data and rex prefixes */
    TYPE_INDIR_VAR_XIREG,        /* indirected register that only varies by stack segment,
                                  * with a base of 32/64 depending on the mode;
                                  * indirected size varies w/ data prefix, except
                                  * 64-bit Intel */
    TYPE_INDIR_VAR_XREG_OFFS_1,  /* TYPE_INDIR_VAR_XREG but with an offset of
                                  * -1 * size */
    TYPE_INDIR_VAR_XREG_OFFS_8,  /* TYPE_INDIR_VAR_XREG but with an offset of
                                  * -8 * size and a size of 8 stack slots */
    TYPE_INDIR_VAR_XREG_OFFS_N,  /* TYPE_INDIR_VAR_XREG but with an offset of
                                  * -N * size and a size to match: i.e., it
                                  * varies based on other operands */
    TYPE_INDIR_VAR_XIREG_OFFS_1, /* TYPE_INDIR_VAR_XIREG but with an offset of
                                  * -1 * size */
    TYPE_INDIR_VAR_REG_OFFS_2,   /* TYPE_INDIR_VAR_REG but with an offset of
                                  * -2 * size and a size of 2 stack slots */
    /* we have to encode the memory size into the type b/c we use the size
     * to store the base reg: but since most base regs are xsp we could
     * encode that into the type and store the size in the size field
     */
    TYPE_INDIR_VAR_XREG_SIZEx8,  /* TYPE_INDIR_VAR_XREG but with a size of
                                  * 8 * regular size */
    TYPE_INDIR_VAR_REG_SIZEx2,   /* TYPE_INDIR_VAR_REG but with a size of
                                  * 2 * regular size */
    TYPE_INDIR_VAR_REG_SIZEx3x5, /* TYPE_INDIR_VAR_REG but with a size of
                                  * 3 * regular size for 32-bit, 5 * regular
                                  * size for 64-bit */
    TYPE_K_MODRM,                /* modrm.rm selects k0-k7 or mem addr */
    TYPE_K_MODRM_R,              /* modrm.rm selects k0-k7 */
    TYPE_K_REG,                  /* modrm.reg selects k0-k7 */
    TYPE_K_VEX,                  /* vex.vvvv field selects k0-k7 */
    TYPE_K_EVEX,                 /* evex.aaa field selects k0-k7 */
    TYPE_T_REG,                  /* modrm.reg selects bnd0-bnd3 */
    TYPE_T_MODRM,                /* modrm.rm selects bnd0-bnd3 register or 8 bytes
                                  * memory in 32-bit mode, or 16 bytes memory in 64-bit
                                  * mode.
                                  */
    /* when adding new types, update type_names[] in encode.c */
    TYPE_BEYOND_LAST_ENUM,
};

#define MODRM_BYTE(mod, reg, rm) ((byte)(((mod) << 6) | ((reg) << 3) | (rm)))

/* for decode_info_t */
#define X64_MODE(di) IF_X64_ELSE(!(di)->x86_mode, false)

/* in decode.c, not exported to non-x86 files */
bool
optype_is_indir_reg(int optype);
opnd_size_t
resolve_var_reg_size(opnd_size_t sz, bool is_reg);
opnd_size_t
resolve_variable_size(decode_info_t *di /*IN: x86_mode, prefixes*/, opnd_size_t sz,
                      bool is_reg);
opnd_size_t
resolve_variable_size_dc(dcontext_t *dcontext, uint prefixes, opnd_size_t sz,
                         bool is_reg);
/* Also takes in reg8 for TYPE_REG_EX mov_imm */
reg_id_t
resolve_var_reg(decode_info_t *di /*IN: x86_mode, prefixes*/, reg_id_t reg32, bool addr,
                bool can_shrink _IF_X64(bool default_64) _IF_X64(bool can_grow)
                    _IF_X64(bool extendable));
opnd_size_t
resolve_addr_size(decode_info_t *di /*IN: x86_mode, prefixes*/);
opnd_size_t
indir_var_reg_size(decode_info_t *di, int optype);
int
indir_var_reg_offs_factor(int optype);
opnd_size_t
expand_subreg_size(opnd_size_t sz);
dr_pred_type_t
decode_predicate_from_instr_info(uint opcode, const instr_info_t *info);

/* Helper function to determine the compressed displacement factor, as specified in
 * Intel's Vol.2A 2.6.5 "Compressed Displacement (disp8*N) Support in EVEX".
 */
int
decode_get_compressed_disp_scale(decode_info_t *di);

/* Retrieves the tuple_type and input_size from info and saves it in di. */
void
decode_get_tuple_type_input_size(const instr_info_t *info, decode_info_t *di);

/* in instr.c, not exported to non-x86 files */
bool
opc_is_cbr_arch(int opc);

/* exported tables */
extern const instr_info_t first_byte[];
extern const instr_info_t second_byte[];
extern const instr_info_t base_extensions[][8];
extern const instr_info_t prefix_extensions[][12];
extern const instr_info_t mod_extensions[][2];
extern const instr_info_t rm_extensions[][8];
extern const instr_info_t x64_extensions[][2];
extern const instr_info_t rex_b_extensions[][2];
extern const instr_info_t rex_w_extensions[][2];
extern const instr_info_t vex_prefix_extensions[][2];
extern const instr_info_t e_vex_extensions[][3];
extern const instr_info_t vex_L_extensions[][3];
extern const instr_info_t vex_W_extensions[][2];
extern const byte third_byte_38_index[256];
extern const byte third_byte_3a_index[256];
extern const instr_info_t third_byte_38[];
extern const instr_info_t third_byte_3a[];
extern const instr_info_t rep_extensions[][4];
extern const instr_info_t repne_extensions[][6];
extern const instr_info_t float_low_modrm[];
extern const instr_info_t float_high_modrm[][64];
extern const byte suffix_index[256];
extern const instr_info_t suffix_extensions[];
extern const instr_info_t extra_operands[];
extern const byte xop_8_index[256];
extern const byte xop_9_index[256];
extern const byte xop_a_index[256];
extern const instr_info_t xop_prefix_extensions[][2];
extern const instr_info_t xop_extensions[];
extern const instr_info_t evex_prefix_extensions[][2];
extern const instr_info_t evex_Wb_extensions[][4];

/* table that translates opcode enums into pointers into decoding tables */
extern const instr_info_t *const op_instr[];

#endif /* DECODE_PRIVATE_H */
