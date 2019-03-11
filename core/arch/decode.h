/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/* Public prefix constants.  decode_private.h may define additional constants
 * only used during decoding.
 */
/* DR_API EXPORT TOFILE dr_ir_instr.h */
/* FIXME i#1551: add another attribute to ARM as PREFIX_ constants:
 *  + Add shift type for shifted source registers: 2-bit enum instead of
 *    6-entry bitfield, since not composable.
 */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * instr_t prefixes
 *
 * Note that prefixes that change the data or address size, or that
 * specify a different base segment, are not specified on a
 * whole-instruction level, but rather on individual operands (of
 * course with multiple operands they must all match).
 * The rep and repne prefixes are encoded directly into the opcodes.
 *
 */
#define PREFIX_LOCK 0x01          /**< Makes the instruction's memory accesses atomic. */
#define PREFIX_JCC_NOT_TAKEN 0x02 /**< Branch hint: conditional branch is taken. */
#define PREFIX_JCC_TAKEN 0x04     /**< Branch hint: conditional branch is not taken. */
#define PREFIX_XACQUIRE 0x08      /**< Transaction hint: start lock elision. */
#define PREFIX_XRELEASE 0x10      /**< Transaction hint: end lock elision. */

/* DR_API EXPORT END */

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
    ushort flags; /* encoding and extra operand flags */
    uint eflags;  /* combination of read & write flags from instr.h */
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

/* N.B.: if you change the size enum, change the string names for
 * them, kept in decode_shared.c
 */

/* DR_API EXPORT TOFILE dr_ir_opnd.h */
/* DR_API EXPORT BEGIN */

/* Memory operand sizes (with Intel's corresponding size names noted).
 * For register operands, the DR_REG_ constants are used, which implicitly
 * state a size (e.g., DR_REG_CX is 2 bytes).
 * Use the type opnd_size_t for these values (we avoid typedef-ing the
 * enum, as its storage size is compiler-specific).  opnd_size_t is a
 * byte, so the largest value here needs to be <= 255.
 */
enum {
/* For x86, register enum values are used for TYPE_*REG but we only use them
 * as opnd_size_t when we have the type available, so we can overlap
 * the two enums by adding new registers consecutively to the reg enum.
 * The reg_id_t type is now wider, but for x86 we ensure our values
 * all fit via an assert in d_r_arch_init().
 * To maintain backward compatibility we keep the OPSZ_ constants
 * starting at the same spot, now midway through the reg enum:
 */
#ifdef X86
    OPSZ_NA = DR_REG_INVALID + 1,
/**< Sentinel value: not a valid size. */ /* = 140 */
#else
    OPSZ_NA = 0, /**< Sentinel value: not a valid size. */
#endif
    OPSZ_FIRST = OPSZ_NA,
    OPSZ_0,   /**< 0 bytes, for "sizeless" operands (for Intel, code
               * 'm': used for both start addresses (lea, invlpg) and
               * implicit constants (rol, fldl2e, etc.) */
    OPSZ_1,   /**< 1 byte (for Intel, code 'b') */
    OPSZ_2,   /**< 2 bytes (for Intel, code 'w') */
    OPSZ_4,   /**< 4 bytes (for Intel, code 'd','si') */
    OPSZ_6,   /**< 6 bytes (for Intel, code 'p','s') */
    OPSZ_8,   /**< 8 bytes (for Intel, code 'q','pi') */
    OPSZ_10,  /**< Intel 's' 64-bit, or double extended precision floating point
               * (latter used by fld, fstp, fbld, fbstp) */
    OPSZ_16,  /**< 16 bytes (for Intel, code 'dq','ps','pd','ss','sd', or AMD 'o') */
    OPSZ_14,  /**< FPU operating environment with short data size (fldenv, fnstenv) */
    OPSZ_28,  /**< FPU operating environment with normal data size (fldenv, fnstenv) */
    OPSZ_94,  /**< FPU state with short data size (fnsave, frstor) */
    OPSZ_108, /**< FPU state with normal data size (fnsave, frstor) */
    OPSZ_512, /**< FPU, MMX, XMM state (fxsave, fxrstor) */
    /**
     * The following sizes (OPSZ_*_short*) vary according to the cs segment and the
     * operand size prefix.  This IR assumes that the cs segment is set to the
     * default operand size.  The operand size prefix then functions to shrink the
     * size.  The IR does not explicitly mark the prefix; rather, a shortened size is
     * requested in the operands themselves, with the IR adding the prefix at encode
     * time.  Normally the fixed sizes above should be used rather than these
     * variable sizes, which are used internally by the IR and should only be
     * externally specified when building an operand in order to be flexible and
     * allow other operands to decide the size for the instruction (the prefix
     * applies to the entire instruction).
     */
    OPSZ_2_short1,        /**< Intel 'c': 2/1 bytes ("2/1" means 2 bytes normally, but if
                           * another operand requests a short size then this size can
                           * accommodate by shifting to its short size, which is 1
                           * byte). */
    OPSZ_4_short2,        /**< Intel 'z': 4/2 bytes */
    OPSZ_4_rex8_short2,   /**< Intel 'v': 8/4/2 bytes */
    OPSZ_4_rex8,          /**< Intel 'd/q' (like 'v' but never 2 bytes) or 'y'. */
    OPSZ_6_irex10_short4, /**< Intel 'p': On Intel processors this is 10/6/4 bytes for
                           * segment selector + address.  On AMD processors this is
                           * 6/4 bytes for segment selector + address (rex is ignored). */
    OPSZ_8_short2,        /**< partially resolved 4x8_short2 */
    OPSZ_8_short4,        /**< Intel 'a': pair of 4_short2 (bound) */
    OPSZ_28_short14,      /**< FPU operating env variable data size (fldenv, fnstenv) */
    OPSZ_108_short94,     /**< FPU state with variable data size (fnsave, frstor) */
    /** Varies by 32-bit versus 64-bit processor mode. */
    OPSZ_4x8,  /**< Full register size with no variation by prefix.
                *   Used for control and debug register moves. */
    OPSZ_6x10, /**< Intel 's': 6-byte (10-byte for 64-bit mode) table base + limit */
    /**
     * Stack operands not only vary by operand size specifications but also
     * by 32-bit versus 64-bit processor mode.
     */
    OPSZ_4x8_short2,    /**< Intel 'v'/'d64' for stack operations.
                         * Also 64-bit address-size specified operands, which are
                         * short4 rather than short2 in 64-bit mode (but short2 in
                         * 32-bit mode).
                         * Note that this IR does not distinguish extra stack
                         * operations performed by OP_enter w/ non-zero immed.
                         */
    OPSZ_4x8_short2xi8, /**< Intel 'f64': 4_short2 for 32-bit, 8_short2 for 64-bit AMD,
                         *   always 8 for 64-bit Intel */
    OPSZ_4_short2xi4,   /**< Intel 'f64': 4_short2 for 32-bit or 64-bit AMD,
                         *   always 4 for 64-bit Intel */
    /**
     * The following 3 sizes differ based on whether the modrm chooses a
     * register or memory.
     */
    OPSZ_1_reg4,  /**< Intel Rd/Mb: zero-extends if reg; used by pextrb */
    OPSZ_2_reg4,  /**< Intel Rd/Mw: zero-extends if reg; used by pextrw */
    OPSZ_4_reg16, /**< Intel Udq/Md: 4 bytes of xmm or 4 bytes of memory;
                   *   used by insertps. */
    /* Sizes used by new instructions */
    OPSZ_xsave,           /**< Size is > 512 bytes: use cpuid to determine.
                           * Used for FPU, MMX, XMM, etc. state by xsave and xrstor. */
    OPSZ_12,              /**< 12 bytes: 32-bit iret */
    OPSZ_32,              /**< 32 bytes: pusha/popa
                           * Also Intel 'qq','pd','ps','x': 32 bytes (256 bits) */
    OPSZ_40,              /**< 40 bytes: 64-bit iret */
    OPSZ_32_short16,      /**< unresolved pusha/popa */
    OPSZ_8_rex16,         /**< cmpxcgh8b/cmpxchg16b */
    OPSZ_8_rex16_short4,  /**< Intel 'v' * 2 (far call/ret) */
    OPSZ_12_rex40_short6, /**< unresolved iret */
    OPSZ_16_vex32,        /**< 16 or 32 bytes depending on VEX.L (AMD/Intel 'x'). */
    OPSZ_15, /**< All but one byte of an xmm register (used by OP_vpinsrb). */

    /* Needed for ARM.  We share the same namespace for now */
    OPSZ_3, /**< 3 bytes */
    /* gpl_list_num_bits assumes OPSZ_ includes every value from 1b to 12b
     * (except 8b/OPSZ_1) in order
     */
    OPSZ_1b,  /**< 1 bit */
    OPSZ_2b,  /**< 2 bits */
    OPSZ_3b,  /**< 3 bits */
    OPSZ_4b,  /**< 4 bits */
    OPSZ_5b,  /**< 5 bits */
    OPSZ_6b,  /**< 6 bits */
    OPSZ_7b,  /**< 7 bits */
    OPSZ_9b,  /**< 9 bits */
    OPSZ_10b, /**< 10 bits */
    OPSZ_11b, /**< 11 bits */
    OPSZ_12b, /**< 12 bits */
    OPSZ_20b, /**< 20 bits */
    OPSZ_25b, /**< 25 bits */
    /**
     * At encode or decode time, the size will match the size of the
     * register list operand in the containing instruction's operands.
     */
    OPSZ_VAR_REGLIST,
    OPSZ_20,  /**< 20 bytes.  Needed for load/store of register lists. */
    OPSZ_24,  /**< 24 bytes.  Needed for load/store of register lists. */
    OPSZ_36,  /**< 36 bytes.  Needed for load/store of register lists. */
    OPSZ_44,  /**< 44 bytes.  Needed for load/store of register lists. */
    OPSZ_48,  /**< 48 bytes.  Needed for load/store of register lists. */
    OPSZ_52,  /**< 52 bytes.  Needed for load/store of register lists. */
    OPSZ_56,  /**< 56 bytes.  Needed for load/store of register lists. */
    OPSZ_60,  /**< 60 bytes.  Needed for load/store of register lists. */
    OPSZ_64,  /**< 64 bytes.  Needed for load/store of register lists. */
    OPSZ_68,  /**< 68 bytes.  Needed for load/store of register lists. */
    OPSZ_72,  /**< 72 bytes.  Needed for load/store of register lists. */
    OPSZ_76,  /**< 76 bytes.  Needed for load/store of register lists. */
    OPSZ_80,  /**< 80 bytes.  Needed for load/store of register lists. */
    OPSZ_84,  /**< 84 bytes.  Needed for load/store of register lists. */
    OPSZ_88,  /**< 88 bytes.  Needed for load/store of register lists. */
    OPSZ_92,  /**< 92 bytes.  Needed for load/store of register lists. */
    OPSZ_96,  /**< 96 bytes.  Needed for load/store of register lists. */
    OPSZ_100, /**< 100 bytes. Needed for load/store of register lists. */
    OPSZ_104, /**< 104 bytes. Needed for load/store of register lists. */
    /* OPSZ_108 already exists */
    OPSZ_112,           /**< 112 bytes. Needed for load/store of register lists. */
    OPSZ_116,           /**< 116 bytes. Needed for load/store of register lists. */
    OPSZ_120,           /**< 120 bytes. Needed for load/store of register lists. */
    OPSZ_124,           /**< 124 bytes. Needed for load/store of register lists. */
    OPSZ_128,           /**< 128 bytes. Needed for load/store of register lists. */
    OPSZ_SCALABLE,      /** Scalable size for SVE vector registers. */
    OPSZ_SCALABLE_PRED, /** Scalable size for SVE predicate registers. */
#ifdef AVOID_API_EXPORT
/* Add new size here.  Also update size_names[] in decode_shared.c along with
 * the size routines in opnd_shared.c.
 */
#endif
    OPSZ_LAST,
};

#ifdef X64
#    define OPSZ_PTR OPSZ_8      /**< Operand size for pointer values. */
#    define OPSZ_STACK OPSZ_8    /**< Operand size for stack push/pop operand sizes. */
#    define OPSZ_PTR_DBL OPSZ_16 /**< Double-pointer-sized. */
#    define OPSZ_PTR_HALF OPSZ_4 /**< Half-pointer-sized. */
#else
#    define OPSZ_PTR OPSZ_4      /**< Operand size for pointer values. */
#    define OPSZ_STACK OPSZ_4    /**< Operand size for stack push/pop operand sizes. */
#    define OPSZ_PTR_DBL OPSZ_8  /**< Double-pointer-sized. */
#    define OPSZ_PTR_HALF OPSZ_2 /**< Half-pointer-sized. */
#endif
#define OPSZ_VARSTACK                                          \
    OPSZ_4x8_short2 /**< Operand size for prefix-varying stack \
                     * push/pop operand sizes. */
#define OPSZ_REXVARSTACK                                      \
    OPSZ_4_rex8_short2 /* Operand size for prefix/rex-varying \
                        * stack push/pop like operand sizes. */

#define OPSZ_ret OPSZ_4x8_short2xi8 /**< Operand size for ret instruction. */
#define OPSZ_call OPSZ_ret          /**< Operand size for push portion of call. */

/* Convenience defines for specific opcodes */
#define OPSZ_lea OPSZ_0              /**< Operand size for lea memory reference. */
#define OPSZ_invlpg OPSZ_0           /**< Operand size for invlpg memory reference. */
#define OPSZ_xlat OPSZ_1             /**< Operand size for xlat memory reference. */
#define OPSZ_clflush OPSZ_1          /**< Operand size for clflush memory reference. */
#define OPSZ_prefetch OPSZ_1         /**< Operand size for prefetch memory references. */
#define OPSZ_lgdt OPSZ_6x10          /**< Operand size for lgdt memory reference. */
#define OPSZ_sgdt OPSZ_6x10          /**< Operand size for sgdt memory reference. */
#define OPSZ_lidt OPSZ_6x10          /**< Operand size for lidt memory reference. */
#define OPSZ_sidt OPSZ_6x10          /**< Operand size for sidt memory reference. */
#define OPSZ_bound OPSZ_8_short4     /**< Operand size for bound memory reference. */
#define OPSZ_maskmovq OPSZ_8         /**< Operand size for maskmovq memory reference. */
#define OPSZ_maskmovdqu OPSZ_16      /**< Operand size for maskmovdqu memory reference. */
#define OPSZ_fldenv OPSZ_28_short14  /**< Operand size for fldenv memory reference. */
#define OPSZ_fnstenv OPSZ_28_short14 /**< Operand size for fnstenv memory reference. */
#define OPSZ_fnsave OPSZ_108_short94 /**< Operand size for fnsave memory reference. */
#define OPSZ_frstor OPSZ_108_short94 /**< Operand size for frstor memory reference. */
#define OPSZ_fxsave OPSZ_512         /**< Operand size for fxsave memory reference. */
#define OPSZ_fxrstor OPSZ_512        /**< Operand size for fxrstor memory reference. */
/* DR_API EXPORT END */

enum {
    /* OPSZ_ constants not exposed to the user so ok to be shifted
     * by additions above
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
    OPSZ_8_of_16_vex32,      /* 64 bits, but can be half of XMM register; if
                              * vex.L then is 256 bits (YMM or memory)
                              */
    OPSZ_16_of_32,           /* 128 bits: half of YMM */
    OPSZ_SUBREG_START = OPSZ_1_of_4,
    OPSZ_SUBREG_END = OPSZ_16_of_32,
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

/* in encode.c, not exported to non-arch/ files */
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

/* DR_API EXPORT TOFILE dr_ir_utils.h */

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
#elif defined(AARCHXX)
#    define X64_MODE_DC(dc) IF_X64_ELSE(true, false)
#    define X64_CACHE_MODE_DC(dc) IF_X64_ELSE(true, false)
#endif

DR_API
/**
 * Decodes only enough of the instruction at address \p pc to determine
 * its eflags usage, which is returned in \p usage as EFLAGS_ constants
 * or'ed together.
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instruction.
 */
#ifdef UNSUPPORTED_API
/**
 * This corresponds to halfway between Level 1 and Level 2: a Level 1 decoding
 * plus eflags information (usually only at Level 2).
 */
#endif
byte *
decode_eflags_usage(dcontext_t *dcontext, byte *pc, uint *usage,
                    dr_opnd_query_flags_t flags);

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

DR_API
/**
 * Decodes the instruction at address \p pc into \p instr, filling in the
 * instruction's opcode, eflags usage, prefixes, and operands.
 * The instruction's raw bits are set to valid and pointed at \p pc
 * (xref instr_get_raw_bits()).
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr_t struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
#ifdef UNSUPPORTED_API
/**
 * This corresponds to a Level 3 decoding.
 */
#endif
byte *
decode(dcontext_t *dcontext, byte *pc, instr_t *instr);

DR_API
/**
 * Decodes the instruction at address \p copy_pc into \p instr as though
 * it were located at address \p orig_pc.  Any pc-relative operands have
 * their values calculated as though the instruction were actually at
 * \p orig_pc, though that address is never de-referenced.
 * The instruction's raw bits are not valid, but its application address field
 * (see instr_get_app_pc()) is set to \p orig_pc.
 * The instruction's opcode, eflags usage, prefixes, and operands are
 * all filled in.
 * Assumes that \p instr is already initialized, but uses the x86/x64 mode
 * for the thread \p dcontext rather than that set in instr.
 * If caller is re-using same instr_t struct over multiple decodings,
 * caller should call instr_reset() or instr_reuse().
 * Returns the address of the next byte after the decoded instruction
 * copy at \p copy_pc.
 * Returns NULL on decoding an invalid instr and sets opcode to OP_INVALID.
 */
#ifdef UNSUPPORTED_API
/**
 * This corresponds to a Level 3 decoding.
 */
#endif
byte *
decode_from_copy(dcontext_t *dcontext, byte *copy_pc, byte *orig_pc, instr_t *instr);

#ifdef CLIENT_INTERFACE
/* decode_as_bb() is defined in interp.c, but declared here so it will
 * be listed next to the other decode routines in the API headers.
 */
DR_API
/**
 * Client routine to decode instructions at an arbitrary app address,
 * following all the rules that DynamoRIO follows internally for
 * terminating basic blocks.  Note that DynamoRIO does not validate
 * that \p start_pc is actually the first instruction of a basic block.
 * \note Caller is reponsible for freeing the list and its instrs!
 */
instrlist_t *
decode_as_bb(void *drcontext, byte *start_pc);

/* decode_trace() is also in interp.c */
DR_API
/**
 * Decodes the trace with tag \p tag, and returns an instrlist_t of
 * the instructions comprising that fragment.  If \p tag is not a
 * valid tag for an existing trace, the routine returns NULL.  Clients
 * can use dr_trace_exists_at() to determine whether the trace exists.
 * \note Unlike the instruction list presented by the trace event, the
 * list here does not include any existing client modifications.  If
 * client-modified instructions are needed, it is the responsibility
 * of the client to record or recreate that list itself.
 * \note This routine does not support decoding thread-private traces
 * created by other than the calling thread.
 */
instrlist_t *
decode_trace(void *drcontext, void *tag);
#endif

const struct instr_info_t *
get_next_instr_info(const instr_info_t *info);

DR_API
/**
 * Given an OP_ constant, returns the first byte of its opcode when
 * encoded as an IA-32 instruction.
 */
byte
decode_first_opcode_byte(int opcode);

DR_API
/** Given an OP_ constant, returns the string name of its opcode. */
const char *
decode_opcode_name(int opcode);

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates whether to treat code as 32-bit (x86) or 64-bit (x64).  This
 * routine sets that flag to the indicated value and returns the old value.  Be
 * sure to restore the old value prior to any further application execution to
 * avoid problems in mis-interpreting application code.
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by dr_set_isa_mode().
 */
bool
set_x86_mode(dcontext_t *dcontext, bool x86);

DR_API
/**
 * The decode and encode routines use a per-thread persistent flag that
 * indicates whether to treat code as 32-bit (x86) or 64-bit (x64).  This
 * routine returns the value of that flag.
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by dr_get_isa_mode().
 */
bool
get_x86_mode(dcontext_t *dcontext);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

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

DR_API
/**
 * Given an application program counter value, returns the
 * corresponding value to use as an indirect branch target for the
 * given \p isa_mode.  For ARM's Thumb mode (#DR_ISA_ARM_THUMB), this
 * involves setting the least significant bit of the address.
 */
app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc);

DR_API
/**
 * Given an application program counter value, returns the
 * corresponding value to use as a memory load target for the given \p
 * isa_mode, or for comparing to the application address inside a
 * basic block or trace.  For ARM's Thumb mode (#DR_ISA_ARM_THUMB),
 * this involves clearing the least significant bit of the address.
 */
app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc);

/* for debugging: printing out types and sizes */
extern const char *const type_names[];
extern const char *const size_names[];

#endif /* DECODE_H */
