/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

/* file "decode.h" */

#ifndef DECODE_H
#define DECODE_H

#include "decode_api.h"

/* Public PREFIX_ constants are in instr_api.h.
 * decode_private.h may define additional constants only used during decoding.
 */

/* We encode some prefixes in the operands themselves, such that we shouldn't
 * consider the whole-instr_t flags when considering equality of instr_t
 */
#define PREFIX_SIGNIFICANT                                                 \
    (PREFIX_LOCK | PREFIX_JCC_TAKEN | PREFIX_JCC_TAKEN | PREFIX_XACQUIRE | \
     PREFIX_XRELEASE)

#ifdef X86
/* PREFIX_SEG_* is set by decode or decode_cti and is only a hint
 * to the caller.  Is ignored by encode in favor of the segment
 * reg specified in the applicable opnds.  We rely on it being set during
 * bb building and reference in in interp, and thus it is public.
 */
#    define PREFIX_SEG_FS 0x20
#    define PREFIX_SEG_GS 0x40
/* Prefix used for AVX-512 */
#    define PREFIX_EVEX 0x000100000
#endif

/* XXX: when adding prefixes, shift all the private values as they start
 * right after the last number here.  For private values, leave room for
 * PREFIX_PRED_BITS at the top.
 */

/* instr_info_t: each decoding table entry is one of these.
 * We use the same struct for all architectures, though the precise encodings
 * of the opcode and flags field vary (see the appropriate decode_private.h file).
 *
 * If we add a new arch that needs something different: we should make this a
 * black box data struct and add accessors for instr.c, mangle.c, and disassemble.c.
 */
typedef struct instr_info_t {
    int type; /* an OP_ constant or special type code below */
    /* opcode: indicates how to encode.  See decode_private.h for details as what's
     * stored here varies by arch.
     */
    uint opcode;
    const char *name;
    /* Operands: each has a type and a size.
     * The opnd_size_t will instead be reg_id_t for TYPE_*REG*.
     * We have room for 2 dsts and 3 srcs, which covers the vast majority of
     * instrs.  We use additional entries (presence indicated by bits in flags)
     * for instrs with extra operands.
     * We also use flags that shift which of these are considered dsts vs srcs.
     */
    byte dst1_type;
    opnd_size_t dst1_size;
    byte dst2_type;
    opnd_size_t dst2_size;
    byte src1_type;
    opnd_size_t src1_size;
    byte src2_type;
    opnd_size_t src2_size;
    byte src3_type;
    opnd_size_t src3_size;
    uint flags;  /* encoding and extra operand flags starting at lsb,
                  * AVX-512 tupletype attribute starting at msb.
                  */
    uint eflags; /* combination of read & write flags from instr.h */
    /* For normal entries, this points to the next entry in the encoding chain
     * for this opcode.
     * For special entries, this can point to the extra operand table,
     * or contain an index into an extension table, or hold a prefix value.
     * The type field indicates how to interpret it.
     */
    ptr_int_t code;
} instr_info_t;

/* instr_info_t is used for table entries, it holds info that is constant
 * for all instances of an instruction.
 * All variable information is kept in this struct, which is used for
 * decoding and encoding.
 */
struct _decode_info_t;
typedef struct _decode_info_t decode_info_t;

/* PR 225845: Our IR does not try to specify the format of the operands or the
 * addressing mode in opnd_t.size: only the size.  Our decode table uses the
 * Intel opcode table "type" fields, and we used to use them for opnd_t.size.
 * They do say more than just the size, but in core code we use the TYPE_ to
 * tell us any formatting we need to know about, and we've always treated
 * identical sizes with different formatting identically: we do not distinguish
 * 128-bit packed doubles from 128-bit packed floats, e.g.  Would any client
 * want that distinction?  There are enough subtleties in the ISA that
 * dispatching by opcode is probably going to be necessary for the client anyway
 * (e.g., maskmovq only writes selected bytes).  Furthermore, many of the
 * distinctions in the OPSZ_ constants apply only to registers, with such
 * distinctions having no way to be specified when constructing an operand as we
 * do not use the size field for register operand types (we only use it for
 * immediates and memory references): to be complete in supplying formatting
 * information we would want to use that field.  Decision: we're only going to
 * provide size information.
 */

enum {
    /* OPSZ_ constants not exposed to the user so ok to be shifted
     * by additions in the main enum.
     */
    OPSZ_1_of_4 = OPSZ_LAST, /* 8 bits, but can be part of a GPR register */
    OPSZ_2_of_4,             /* 16 bits, but can be part of a GPR register */
    OPSZ_1_of_8,             /* 8 bits, but can be part of an MMX register */
    OPSZ_2_of_8,             /* 16 bits, but can be part of MMX register */
    OPSZ_4_of_8,             /* 32 bits, but can be half of MMX register */
    OPSZ_1_of_16,            /* 8 bits, but can be part of XMM register */
    OPSZ_2_of_16,            /* 16 bits, but can be part of XMM register */
    OPSZ_4_of_16,            /* 32 bits, but can be part of XMM register */
    OPSZ_4_rex8_of_16,       /* 32 bits, 64 with rex.w, but can be part of XMM register */
    OPSZ_8_of_16,            /* 64 bits, but can be half of XMM register */
    OPSZ_12_of_16,           /* 96 bits: 3/4 of XMM */
    OPSZ_12_rex8_of_16,      /* 96 bits, or 64 with rex.w: 3/4 of XMM */
    OPSZ_14_of_16,           /* 112 bits; all but one word of XMM */
    OPSZ_15_of_16,           /* 120 bits: all but one byte of XMM */
    OPSZ_16_of_32,           /* 128 bits: half of YMM */
    OPSZ_half_16_vex32,      /* half of 128 bits (XMM or memory);
                              * if vex.L then is half of 256 bits (YMM or memory).
                              */
    OPSZ_half_16_vex32_evex64,    /* 64 bits, but can be half of XMM register;
                                   * if evex.L then is 256 bits (YMM or memory);
                                   * if evex.L' then is 512 bits (ZMM or memory).
                                   */
    OPSZ_quarter_16_vex32,        /* quarter of 128 bits (XMM or memory);
                                   * if vex.L then is quarter of 256 bits (YMM or memory).
                                   */
    OPSZ_quarter_16_vex32_evex64, /* quarter of 128 bits (XMM or memory);
                                   * if evex.L then is quarter of 256 bits (YMM or
                                   * memory);
                                   * if evex.L' then is quarter of 512 bits (ZMM
                                   * or memory).
                                   */
    OPSZ_eighth_16_vex32,         /* eighth of 128 bits (XMM or memory);
                                   * if vex.L then is eighth of 256 bits (YMM or memory).
                                   */
    OPSZ_eighth_16_vex32_evex64,  /* eighth of 128 bits (XMM or memory);
                                   * if evex.L then is eighth of 256 bits (YMM or
                                   * memory);
                                   * if evex.L' then is eighth of 512 bits (ZMM
                                   * or memory).
                                   */
    OPSZ_SUBREG_START = OPSZ_1_of_4,
    OPSZ_SUBREG_END = OPSZ_eighth_16_vex32_evex64,
    OPSZ_LAST_ENUM, /* note last is NOT inclusive */
};

#ifdef X64
#    define OPSZ_STATS OPSZ_8
#else
#    define OPSZ_STATS OPSZ_4
#endif

#ifdef ARM
#    define IT_BLOCK_MAX_INSTRS 4
#endif

/* in encode.c, not exported to non-ir/ files */
/* This returns encoding information that may not be accurate as
 * it is not given the final PC and that may affect which encoding
 * template is used.  Callers should only use this when the
 * differences between templates with respect to reachability
 * do not matter.  One known difference is the absolute address
 * immediate templates on x86.
 */
const instr_info_t *
get_encoding_info(instr_t *instr);
const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info);
byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num);

/* in decode_shared.c */
extern const instr_info_t invalid_instr;

/* in decode.c */
const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode _IF_ARM(bool it_block));
bool
decode_raw_is_jmp(dcontext_t *dcontext, byte *pc);
byte *
decode_raw_jmp_target(dcontext_t *dcontext, byte *pc);
#ifdef AARCH64
bool
decode_raw_is_cond_branch_zero(dcontext_t *dcontext, byte *pc);
byte *
decode_raw_cond_branch_zero_target(dcontext_t *dcontext, byte *pc);
#endif

/* exported routines */

bool
is_isa_mode_legal(dr_isa_mode_t mode);

#ifdef X86
/* for dcontext_t */
#    define X64_MODE_DC(dc) IF_X64_ELSE(!get_x86_mode(dc), false)
/* Currently we assume that code caches are always 64-bit in x86_to_x64.
 * Later, if needed, we can introduce a new field in dcontext_t (xref i#862).
 */
#    define X64_CACHE_MODE_DC(dc) (X64_MODE_DC(dc) IF_X64(|| DYNAMO_OPTION(x86_to_x64)))
#elif defined(AARCHXX) || defined(RISCV64)
#    define X64_MODE_DC(dc) IF_X64_ELSE(true, false)
#    define X64_CACHE_MODE_DC(dc) IF_X64_ELSE(true, false)
#endif

DR_UNS_API
/**
 * Decodes the opcode and eflags usage of instruction at address \p pc
 * into \p instr.
 * If the eflags usage varies with operand values, the maximal value
 * will be set.
 * The instruction's raw bits are set to valid and pointed at \p pc
 * (xref instr_get_raw_bits()).
 * Assumes that \p instr is already initialized, and uses the x86/x64 mode
 * set for it rather than the current thread's mode!
 * If caller is re-using same instr_t struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr);

const struct instr_info_t *
get_next_instr_info(const instr_info_t *info);

#ifdef DEBUG
void
decode_debug_checks(void);
#endif

#ifdef ARM
/* The "current" pc has an offset in pc-relative computations that varies
 * by mode, opcode, and even operands.  Callers can pass NULL for instr
 * if their opcode is OP_b, OP_b_short, OP_bl, OP_cbnz, OP_cbz, or OP_blx.
 */
app_pc
decode_cur_pc(app_pc instr_pc, dr_isa_mode_t mode, uint opcode, instr_t *instr);

#    ifdef DEBUG
void
check_encode_decode_consistency(dcontext_t *dcontext, instrlist_t *ilist);
#    endif
#endif

/* for debugging: printing out types and sizes */
extern const char *const type_names[];
extern const char *const size_names[];

#endif /* DECODE_H */
