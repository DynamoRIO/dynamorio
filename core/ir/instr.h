/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* The machine-specific IR consists of instruction lists, instructions,
 * operands, and opcodes.  You can find related declarations and interface
 * functions in corresponding headers.
 */
/* file "instr.h" -- instr_t specific definitions and utilities */

#ifndef _INSTR_H_
#define _INSTR_H_ 1

#include "opcode.h"
#include "opnd.h"

/* To avoid duplicating code we use our own exported macros, unless an includer
 * needs to avoid it.
 */
#ifdef DR_NO_FAST_IR
#    undef DR_FAST_IR
#    undef INSTR_INLINE
#else
#    define DR_FAST_IR 1
#endif

/* Avoid clang-format bug in i#3158 where having ifdef AVOID_API_EXPORT before
 * a declaration causes the declaration to be indented and skipped by genapi.pl.
 */
#ifdef AVOID_API_EXPORT
#    define INSTR_INLINE_INTERNALLY INSTR_INLINE
#else
#    define INSTR_INLINE_INTERNALLY /* nothing */
#endif

/* can't include decode.h, it includes us, just declare struct */
struct instr_info_t;

/* DR_API EXPORT TOFILE dr_ir_opcodes.h */
/* DR_API EXPORT BEGIN */
#ifdef API_EXPORT_ONLY
#    ifdef X86
#        include "dr_ir_opcodes_x86.h"
#    elif defined(AARCH64)
#        include "dr_ir_opcodes_aarch64.h"
#    elif defined(ARM)
#        include "dr_ir_opcodes_arm.h"
#    endif
#endif
/* DR_API EXPORT END */

/* If INSTR_INLINE is already defined, that means we've been included by
 * instr_shared.c, which wants to use C99 extern inline.  Otherwise, DR_FAST_IR
 * determines whether our instr routines are inlined.
 */
/* Inlining macro controls. */
#ifndef INSTR_INLINE
#    ifdef DR_FAST_IR
#        define INSTR_INLINE inline
#    else
#        define INSTR_INLINE
#    endif
#endif

/*************************
 ***      instr_t      ***
 *************************/

/* instr_t structure
 * An instruction represented by instr_t can be in a number of states,
 * depending on whether it points to raw bits that are valid,
 * whether its operand and opcode fields are up to date, and whether
 * its eflags field is up to date.
 * Invariant: if opcode == OP_UNDECODED, raw bits should be valid.
 *            if opcode == OP_INVALID, raw bits may point to real bits,
 *              but they are not a valid instruction stream.
 *
 * CORRESPONDENCE WITH CGO LEVELS
 * Level 0 = raw bits valid, !opcode_valid, decode_sizeof(instr) != instr->len
 *   opcode_valid is equivalent to opcode != OP_INVALID && opcode != OP_UNDECODED
 * Level 1 = raw bits valid, !opcode_valid, decode_sizeof(instr) == instr->len
 * Level 2 = raw bits valid, opcode_valid, !operands_valid
 *   (eflags info is auto-derived on demand so not an issue)
 * Level 3 = raw bits valid, operands valid
 *   (we assume that if operands_valid then opcode_valid)
 * Level 4 = !raw bits valid, operands valid
 *
 * Independent of these is whether its raw bits were allocated for
 * the instr or not.
 */

/* DR_API EXPORT TOFILE dr_ir_instr.h */
/* For inlining, we need to expose some of these flags.  We bracket the ones we
 * want in export begin/end.  AVOID_API_EXPORT does not work because there are
 * nested ifdefs.
 */
/* DR_API EXPORT BEGIN */
#ifdef DR_FAST_IR
/* flags */
enum {
    /* DR_API EXPORT END */
    /* these first flags are shared with the LINK_ flags and are
     * used to pass on info to link stubs
     */
    /* used to determine type of indirect branch for exits */
    INSTR_DIRECT_EXIT = LINK_DIRECT,
    INSTR_INDIRECT_EXIT = LINK_INDIRECT,
    INSTR_RETURN_EXIT = LINK_RETURN,
    /* JMP|CALL marks an indirect jmp preceded by a call (== a PLT-style ind call)
     * so use EXIT_IS_{JMP,CALL} rather than these raw bits
     */
    INSTR_CALL_EXIT = LINK_CALL,
    INSTR_JMP_EXIT = LINK_JMP,
    INSTR_IND_JMP_PLT_EXIT = (INSTR_JMP_EXIT | INSTR_CALL_EXIT),
    INSTR_FAR_EXIT = LINK_FAR,
    INSTR_BRANCH_SPECIAL_EXIT = LINK_SPECIAL_EXIT,
    INSTR_BRANCH_PADDED = LINK_PADDED,
#    ifdef X64
    /* PR 257963: since we don't store targets of ind branches, we need a flag
     * so we know whether this is a trace cmp exit, which has its own ibl entry
     */
    INSTR_TRACE_CMP_EXIT = LINK_TRACE_CMP,
#    endif
#    ifdef WINDOWS
    INSTR_CALLBACK_RETURN = LINK_CALLBACK_RETURN,
#    else
    INSTR_NI_SYSCALL_INT = LINK_NI_SYSCALL_INT,
#    endif
    INSTR_NI_SYSCALL = LINK_NI_SYSCALL,
    INSTR_NI_SYSCALL_ALL = LINK_NI_SYSCALL_ALL,
    /* meta-flag */
    EXIT_CTI_TYPES = (INSTR_DIRECT_EXIT | INSTR_INDIRECT_EXIT | INSTR_RETURN_EXIT |
                      INSTR_CALL_EXIT | INSTR_JMP_EXIT | INSTR_FAR_EXIT |
                      INSTR_BRANCH_SPECIAL_EXIT | INSTR_BRANCH_PADDED |
#    ifdef X64
                      INSTR_TRACE_CMP_EXIT |
#    endif
#    ifdef WINDOWS
                      INSTR_CALLBACK_RETURN |
#    else
                      INSTR_NI_SYSCALL_INT |
#    endif
                      INSTR_NI_SYSCALL),

    /* instr_t-internal flags (not shared with LINK_) */
    INSTR_OPERANDS_VALID = 0x00010000,
    /* meta-flag */
    INSTR_FIRST_NON_LINK_SHARED_FLAG = INSTR_OPERANDS_VALID,
    INSTR_EFLAGS_VALID = 0x00020000,
    INSTR_EFLAGS_6_VALID = 0x00040000,
    INSTR_RAW_BITS_VALID = 0x00080000,
    INSTR_RAW_BITS_ALLOCATED = 0x00100000,
    /* DR_API EXPORT BEGIN */
    INSTR_DO_NOT_MANGLE = 0x00200000,
    /* DR_API EXPORT END */
    /* This flag is set by instr_noalloc_init() and used to identify the
     * instr_noalloc_t "subclass" of instr_t.  It should not be otherwise used.
     */
    INSTR_IS_NOALLOC_STRUCT = 0x00400000,
    /* used to indicate that an indirect call can be treated as a direct call */
    INSTR_IND_CALL_DIRECT = 0x00800000,
#    ifdef WINDOWS
    /* used to indicate that a syscall should be executed via shared syscall */
    INSTR_SHARED_SYSCALL = 0x01000000,
#    else
    /* Indicates an instruction that's part of the rseq endpoint.  We use this in
     * instrlist_t.flags (sort of the same namespace: INSTR_OUR_MANGLING is used there,
     * but also EDI_VAL_*) and as a version of DR_NOTE_RSEQ that survives encoding
     * (seems like we could store notes for labels in another field so they do
     * in fact survive: a union with instr_t.translation?).
     */
    INSTR_RSEQ_ENDPOINT = 0x01000000,
#    endif

#    ifdef CLIENT_INTERFACE
    /* This enum is also used for INSTR_OUR_MANGLING_EPILOGUE. Its semantics are
     * orthogonal to this and must not overlap.
     */
    INSTR_CLOBBER_RETADDR = 0x02000000,
#    endif

    /* Indicates that the instruction is part of an own mangling region's
     * epilogue (xref i#3307). Currently, instructions with the
     * INSTR_CLOBBER_RETADDR property are never in a mangling epilogue, which
     * is why we are reusing its enum value here.
     * */
    INSTR_OUR_MANGLING_EPILOGUE = 0x02000000,
    /* Signifies that this instruction may need to be hot patched and should
     * therefore not cross a cache line. It is not necessary to set this for
     * exit cti's or linkstubs since it is mainly intended for clients etc.
     * Handling of this flag is not yet implemented */
    INSTR_HOT_PATCHABLE = 0x04000000,
#    ifdef DEBUG
    /* case 9151: only report invalid instrs for normal code decoding */
    INSTR_IGNORE_INVALID = 0x08000000,
#    endif
    /* Currently used for frozen coarse fragments with final jmps and
     * jmps to ib stubs that are elided: we need the jmp instr there
     * to build the linkstub_t but we do not want to emit it. */
    INSTR_DO_NOT_EMIT = 0x10000000,
    /* PR 251479: re-relativization support: is instr->rip_rel_pos valid? */
    INSTR_RIP_REL_VALID = 0x20000000,
#    ifdef X86
    /* PR 278329: each instr stores its own mode */
    INSTR_X86_MODE = 0x40000000,
#    elif defined(ARM)
    /* We assume we don't need to distinguish A64 from A32 as you cannot swap
     * between them in user mode.  Thus we only need one flag.
     * XXX: we might want more power for drdecode, though the global isa_mode
     * should be sufficient there.
     */
    INSTR_THUMB_MODE = 0x40000000,
#    endif
    /* PR 267260: distinguish our own mangling from client-added instrs */
    INSTR_OUR_MANGLING = 0x80000000,
    /* DR_API EXPORT BEGIN */
};
#endif /* DR_FAST_IR */

/** Triggers used for conditionally executed instructions. */
typedef enum _dr_pred_type_t {
#ifdef AVOID_API_EXPORT
/* We resist using #elif here because otherwise doxygen will be unable to
 * document both defines, for X86 and for AARCHXX.
 */
#endif
    DR_PRED_NONE, /**< No predicate is present. */
#ifdef X86
    DR_PRED_O,   /**< x86 condition: overflow (OF=1). */
    DR_PRED_NO,  /**< x86 condition: no overflow (OF=0). */
    DR_PRED_B,   /**< x86 condition: below (CF=1). */
    DR_PRED_NB,  /**< x86 condition: not below (CF=0). */
    DR_PRED_Z,   /**< x86 condition: zero (ZF=1). */
    DR_PRED_NZ,  /**< x86 condition: not zero (ZF=0). */
    DR_PRED_BE,  /**< x86 condition: below or equal (CF=1 or ZF=1). */
    DR_PRED_NBE, /**< x86 condition: not below or equal (CF=0 and ZF=0). */
    DR_PRED_S,   /**< x86 condition: sign (SF=1). */
    DR_PRED_NS,  /**< x86 condition: not sign (SF=0). */
    DR_PRED_P,   /**< x86 condition: parity (PF=1). */
    DR_PRED_NP,  /**< x86 condition: not parity (PF=0). */
    DR_PRED_L,   /**< x86 condition: less (SF != OF). */
    DR_PRED_NL,  /**< x86 condition: not less (SF=OF). */
    DR_PRED_LE,  /**< x86 condition: less or equal (ZF=1 or SF != OF). */
    DR_PRED_NLE, /**< x86 condition: not less or equal (ZF=0 and SF=OF). */
    /**
     * x86 condition: special opcode-specific condition that depends on the
     * values of the source operands.  Thus, unlike all of the other conditions,
     * the source operands will be accessed even if the condition then fails
     * and the destinations are not touched.  Any written eflags are
     * unconditionally written, unlike regular destination operands.
     */
    DR_PRED_COMPLEX,
    /* Aliases for XINST_CREATE_jump_cond() and other cross-platform routines. */
    DR_PRED_EQ = DR_PRED_Z,  /**< Condition code: equal. */
    DR_PRED_NE = DR_PRED_NZ, /**< Condition code: not equal. */
    DR_PRED_LT = DR_PRED_L,  /**< Condition code: signed less than. */
    /* DR_PRED_LE already matches aarchxx */
    DR_PRED_GT = DR_PRED_NLE, /**< Condition code: signed greater than. */
    DR_PRED_GE = DR_PRED_NL,  /**< Condition code: signed greater than or equal. */
#endif
#ifdef AARCHXX
    DR_PRED_EQ, /**< ARM condition: 0000 Equal                   (Z == 1)           */
    DR_PRED_NE, /**< ARM condition: 0001 Not equal               (Z == 0)           */
    DR_PRED_CS, /**< ARM condition: 0010 Carry set               (C == 1)           */
    DR_PRED_CC, /**< ARM condition: 0011 Carry clear             (C == 0)           */
    DR_PRED_MI, /**< ARM condition: 0100 Minus, negative         (N == 1)           */
    DR_PRED_PL, /**< ARM condition: 0101 Plus, positive or zero  (N == 0)           */
    DR_PRED_VS, /**< ARM condition: 0110 Overflow                (V == 1)           */
    DR_PRED_VC, /**< ARM condition: 0111 No overflow             (V == 0)           */
    DR_PRED_HI, /**< ARM condition: 1000 Unsigned higher         (C == 1 and Z == 0)*/
    DR_PRED_LS, /**< ARM condition: 1001 Unsigned lower or same  (C == 0 or Z == 1) */
    DR_PRED_GE, /**< ARM condition: 1010 Signed >=               (N == V)           */
    DR_PRED_LT, /**< ARM condition: 1011 Signed less than        (N != V)           */
    DR_PRED_GT, /**< ARM condition: 1100 Signed greater than     (Z == 0 and N == V)*/
    DR_PRED_LE, /**< ARM condition: 1101 Signed <=               (Z == 1 or N != V) */
    DR_PRED_AL, /**< ARM condition: 1110 Always (unconditional)                    */
#    ifdef AARCH64
    DR_PRED_NV, /**< ARM condition: 1111 Never, meaning always                     */
#    endif
#    ifdef ARM
    DR_PRED_OP, /**< ARM condition: 1111 Part of opcode                            */
#    endif
    /* Aliases */
    DR_PRED_HS = DR_PRED_CS, /**< ARM condition: alias for DR_PRED_CS. */
    DR_PRED_LO = DR_PRED_CC, /**< ARM condition: alias for DR_PRED_CC. */
#endif
} dr_pred_type_t;

/**
 * Specifies hints for how an instruction should be encoded if redundant encodings are
 * available. Currently, we provide a hint for x86 evex encoded instructions. It can be
 * used to encode an instruction in its evex form instead of its vex format (xref #3339).
 */
typedef enum _dr_encoding_hint_type_t {
    DR_ENCODING_HINT_NONE = 0x0, /**< No encoding hint is present. */
#ifdef X86
    DR_ENCODING_HINT_X86_EVEX = 0x1, /**< x86: Encode in EVEX form if available. */
#endif
} dr_encoding_hint_type_t;
/* DR_API EXPORT END */

#define DR_TUPLE_TYPE_BITS 4
#define DR_TUPLE_TYPE_BITPOS (32 - DR_TUPLE_TYPE_BITS)

/* AVX-512 tuple type attributes as specified in Intel's tables. */
typedef enum _dr_tuple_type_t {
    DR_TUPLE_TYPE_NONE = 0,
#ifdef X86
    DR_TUPLE_TYPE_FV = 1,
    DR_TUPLE_TYPE_HV = 2,
    DR_TUPLE_TYPE_FVM = 3,
    DR_TUPLE_TYPE_T1S = 4,
    DR_TUPLE_TYPE_T1F = 5,
    DR_TUPLE_TYPE_T2 = 6,
    DR_TUPLE_TYPE_T4 = 7,
    DR_TUPLE_TYPE_T8 = 8,
    DR_TUPLE_TYPE_HVM = 9,
    DR_TUPLE_TYPE_QVM = 10,
    DR_TUPLE_TYPE_OVM = 11,
    DR_TUPLE_TYPE_M128 = 12,
    DR_TUPLE_TYPE_DUP = 13,
#endif
} dr_tuple_type_t;

/* These aren't composable, so we store them in as few bits as possible.
 * The top 5 prefix bits hold the value (x86 needs 17 values).
 * XXX: if we need more space we could compress the x86 values: they're
 * all pos/neg pairs so we could store the pos/neg bit just once.
 * XXX: if we want a slightly faster predication check we could take
 * a dedicated PREFIX_PREDICATED bit.
 */
#define PREFIX_PRED_BITS 5
#define PREFIX_PRED_BITPOS (32 - PREFIX_PRED_BITS)
#define PREFIX_PRED_MASK \
    (((1 << PREFIX_PRED_BITS) - 1) << PREFIX_PRED_BITPOS) /*0xf8000000 */
/* DR_API EXPORT BEGIN */

/**
 * Data slots available in a label (instr_create_label()) instruction
 * for storing client-controlled data.  Accessible via
 * instr_get_label_data_area().
 */
typedef struct _dr_instr_label_data_t {
    ptr_uint_t data[4]; /**< Generic fields for storing user-controlled data */
} dr_instr_label_data_t;

/**
 * Label instruction callback function. Set by instr_set_label_callback() and
 * called when the label is freed. \p instr is the label instruction allowing
 * the caller to free the label's auxiliary data.
 */
typedef void (*instr_label_callback_t)(void *drcontext, instr_t *instr);

/**
 * Bitmask values passed as flags to routines that ask about whether operands
 * and condition codes are read or written.  These flags determine how to treat
 * conditionally executed instructions.
 * As a special case, the addressing registers inside a destination memory
 * operand are covered by DR_QUERY_INCLUDE_COND_SRCS rather than
 * DR_QUERY_INCLUDE_COND_DSTS.
 */
typedef enum _dr_opnd_query_flags_t {
    /**
     * By default, routines that take in these flags will only consider
     * destinations that are always written.  Thus, all destinations are skipped
     * for an instruction that is predicated and executes conditionally (see
     * instr_is_predicated()).  If this flag is set, a conditionally executed
     * instruction's destinations are included just like any other
     * instruction's.  As a special case, the addressing registers inside a
     * destination memory operand are covered by DR_QUERY_INCLUDE_COND_SRCS
     * rather than this flag.
     */
    DR_QUERY_INCLUDE_COND_DSTS = 0x01,
    /**
     * By default, routines that take in these flags will only consider sources
     * that are always read.  Thus, all sources are skipped for an instruction
     * that is predicated and executes conditionally (see
     * instr_is_predicated()), except for predication conditions that involve
     * the source operand values.  If this flag is set, a conditionally executed
     * instruction's sources are included just like any other instruction's.
     * As a special case, the addressing registers inside a destination memory
     * operand are covered by this flag rather than DR_QUERY_INCLUDE_COND_DSTS.
     */
    DR_QUERY_INCLUDE_COND_SRCS = 0x02,
    /** The default value that typical liveness analysis would want to use. */
    DR_QUERY_DEFAULT = DR_QUERY_INCLUDE_COND_SRCS,
    /** Includes all operands whether conditional or not. */
    DR_QUERY_INCLUDE_ALL = (DR_QUERY_INCLUDE_COND_DSTS | DR_QUERY_INCLUDE_COND_SRCS),
} dr_opnd_query_flags_t;

#ifdef DR_FAST_IR
/* DR_API EXPORT END */
/* FIXME: could shrink prefixes, eflags, opcode, and flags fields
 * this struct isn't a memory bottleneck though b/c it isn't persistent
 */
/* DR_API EXPORT BEGIN */

/**
 * instr_t type exposed for optional "fast IR" access.  Note that DynamoRIO
 * reserves the right to change this structure across releases and does
 * not guarantee binary or source compatibility when this structure's fields
 * are directly accessed.  If the instr_ accessor routines are used, DynamoRIO does
 * guarantee source compatibility, but not binary compatibility.  If binary
 * compatibility is desired, do not use the fast IR feature.
 */
struct _instr_t {
    /* flags contains the constants defined above */
    uint flags;

    /* hints for encoding this instr in a specific way, holds dr_encoding_hint_type_t */
    uint encoding_hints;

    /* Raw bits of length length are pointed to by the bytes field.
     * label_cb stores a callback function pointer used by label instructions
     * and called when the label is freed.
     */
    uint length;
    union {
        byte *bytes;
        instr_label_callback_t label_cb;
    };

    /* translation target for this instr */
    app_pc translation;

    uint opcode;

#    ifdef X86
    /* PR 251479: offset into instr's raw bytes of rip-relative 4-byte displacement */
    byte rip_rel_pos;
#    endif

    /* we dynamically allocate dst and src arrays b/c x86 instrs can have
     * up to 8 of each of them, but most have <=2 dsts and <=3 srcs, and we
     * use this struct for un-decoded instrs too
     */
    byte num_dsts;
    byte num_srcs;

    union {
        struct {
            /* for efficiency everyone has a 1st src opnd, since we often just
             * decode jumps, which all have a single source (==target)
             * yes this is an extra 10 bytes, but the whole struct is still < 64 bytes!
             */
            opnd_t src0;
            opnd_t *srcs; /* this array has 2nd src and beyond */
            opnd_t *dsts;
        };
        dr_instr_label_data_t label_data;
    };

    uint prefixes; /* data size, addr size, or lock prefix info */
    uint eflags;   /* contains EFLAGS_ bits, but amount of info varies
                    * depending on how instr was decoded/built */

    /* this field is for the use of passes as an annotation.
     * it is also used to hold the offset of an instruction when encoding
     * pc-relative instructions. A small range of values is reserved for internal use
     * by DR and cannot be used by clients; see DR_NOTE_FIRST_RESERVED in globals.h.
     */
    void *note;

    /* fields for building instructions into instruction lists */
    instr_t *prev;
    instr_t *next;

};     /* instr_t */
#endif /* DR_FAST_IR */

/**
 * A version of #instr_t which guarantees to not use heap allocation for regular
 * decoding and encoding.  It inlines all the possible operands and encoding space
 * inside the structure.  Some operations could still use heap if custom label data is
 * used to point at heap-allocated structures through extension libraries or custom
 * code.
 *
 * The instr_from_noalloc() function should be used to obtain an #instr_t pointer for
 * passing to API functions:
 *
 * \code
 *    instr_noalloc_t noalloc;
 *    instr_noalloc_init(dcontext, &noalloc);
 *    instr_t *instr = instr_from_noalloc(&noalloc);
 *    pc = decode(dcontext, ptr, instr);
 * \endcode
 *
 * No freeing is required.  To re-use the same structure, instr_reset() can be called.
 *
 * Some operations are not supported on this instruction format:
 * + instr_clone()
 * + instr_remove_srcs()
 * + instr_remove_dsts()
 * + Automated re-relativization when encoding.
 *
 * This format does not support caching encodings, so it is less efficient for encoding.
 * It is intended for use when decoding in a signal handler or other locations where
 * heap allocation is unsafe.
 */
typedef struct instr_noalloc_t {
    instr_t instr; /**< The base instruction, valid for passing to API functions. */
    opnd_t srcs[MAX_SRC_OPNDS - 1];    /**< Built-in storage for source operands. */
    opnd_t dsts[MAX_DST_OPNDS];        /**< Built-in storage for destination operands. */
    byte encode_buf[MAX_INSTR_LENGTH]; /**< Encoding space for instr_length(), etc. */
} instr_noalloc_t;

/****************************************************************************
 * INSTR ROUTINES
 */
/**
 * @file dr_ir_instr.h
 * @brief Functions to create and manipulate instructions.
 */

/* DR_API EXPORT END */

DR_API
/**
 * Returns an initialized instr_t allocated on the thread-local heap.
 * Sets the x86/x64 mode of the returned instr_t to the mode of dcontext.
 * The instruction should be de-allocated with instr_destroy(), which
 * will be called automatically if this instruction is added to the instruction
 * list passed to the basic block or trace events.
 */
/* For -x86_to_x64, sets the mode of the instr to the code cache mode instead of
the app mode. */
instr_t *
instr_create(dcontext_t *dcontext);

DR_API
/**
 * Initializes \p instr.
 * Sets the x86/x64 mode of \p instr to the mode of dcontext.
 * When finished with it, the instruction's internal memory should be freed
 * with instr_free(), or instr_reset() for reuse.
 */
void
instr_init(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Initializes the no-heap-allocation structure \p instr.
 * Sets the x86/x64 mode of \p instr to the mode of dcontext.
 */
void
instr_noalloc_init(dcontext_t *dcontext, instr_noalloc_t *instr);

DR_API
INSTR_INLINE
/**
 * Given an #instr_noalloc_t where all operands are included, returns
 * an #instr_t pointer corresponding to that no-alloc structure suitable for
 * passing to instruction API functions.
 */
instr_t *
instr_from_noalloc(instr_noalloc_t *noalloc);

DR_API
/**
 * Deallocates all memory that was allocated by \p instr.  This
 * includes raw bytes allocated by instr_allocate_raw_bits() and
 * operands allocated by instr_set_num_opnds().  Does not deallocate
 * the storage for \p instr itself (use instr_destroy() instead if
 * \p instr was created with instr_create()).
 */
void
instr_free(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Performs both instr_free() and instr_init().
 * \p instr must have been initialized.
 */
void
instr_reset(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Frees all dynamically allocated storage that was allocated by \p instr,
 * except for allocated bits.
 * Also zeroes out \p instr's fields, except for raw bit fields,
 * whether \p instr is instr_is_meta(), and the x86 mode of \p instr.
 * \p instr must have been initialized.
 */
void
instr_reuse(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Performs instr_free() and then deallocates the thread-local heap
 * storage for \p instr that was performed by instr_create().
 */
void
instr_destroy(dcontext_t *dcontext, instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the next instr_t in the instrlist_t that contains \p instr.
 * \note The next pointer for an instr_t is inside the instr_t data
 * structure itself, making it impossible to have on instr_t in
 * two different InstrLists (but removing the need for an extra data
 * structure for each element of the instrlist_t).
 */
instr_t *
instr_get_next(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the next application (non-meta) instruction in the instruction list
 * that contains \p instr.
 *
 * \note As opposed to instr_get_next(), this routine skips all meta
 * instructions inserted by either DynamoRIO or its clients.
 *
 * \note We recommend using this routine during the phase of application
 * code analysis, as any meta instructions present are guaranteed to be ok
 * to skip.
 * However, caution should be exercised if using this routine after any
 * instrumentation insertion has already happened, as instrumentation might
 * affect register usage or other factors being analyzed.
 */
instr_t *
instr_get_next_app(instr_t *instr);

DR_API
INSTR_INLINE
/** Returns the previous instr_t in the instrlist_t that contains \p instr. */
instr_t *
instr_get_prev(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the previous application (non-meta) instruction in the instruction
 * list that contains \p instr.
 *
 * \note As opposed to instr_get_prev(), this routine skips all meta
 * instructions inserted by either DynamoRIO or its clients.
 *
 * \note We recommend using this routine during the phase of application
 * code analysis, as any meta instructions present are guaranteed to be ok
 * to skip.
 * However, caution should be exercised if using this routine after any
 * instrumentation insertion has already happened, as instrumentation might
 * affect register usage or other factors being analyzed.
 */
instr_t *
instr_get_prev_app(instr_t *instr);

DR_API
INSTR_INLINE
/** Sets the next field of \p instr to point to \p next. */
void
instr_set_next(instr_t *instr, instr_t *next);

DR_API
INSTR_INLINE
/** Sets the prev field of \p instr to point to \p prev. */
void
instr_set_prev(instr_t *instr, instr_t *prev);

DR_API
INSTR_INLINE
/**
 * Gets the value of the user-controlled note field in \p instr.
 * \note Important: is also used when emitting for targets that are other
 * instructions.  Thus it will be overwritten when calling instrlist_encode()
 * or instrlist_encode_to_copy() with \p has_instr_jmp_targets set to true.
 * \note The note field is copied (shallowly) by instr_clone().
 */
void *
instr_get_note(instr_t *instr);

DR_API
INSTR_INLINE
/** Sets the user-controlled note field in \p instr to \p value. */
void
instr_set_note(instr_t *instr, void *value);

DR_API
/** Return the taken target pc of the (direct branch) instruction. */
app_pc
instr_get_branch_target_pc(instr_t *cti_instr);

DR_API
/** Set the taken target pc of the (direct branch) instruction. */
void
instr_set_branch_target_pc(instr_t *cti_instr, app_pc pc);

DR_API
/**
 * Returns true iff \p instr is a conditional branch, unconditional branch,
 * or indirect branch with a program address target (NOT an instr_t address target)
 * and \p instr is ok to mangle.
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_exit_cti(instr_t *instr);

DR_API
/** Return true iff \p instr's opcode is OP_int, OP_into, or OP_int3. */
bool
instr_is_interrupt(instr_t *instr);

bool
instr_branch_is_padded(instr_t *instr);

void
instr_branch_set_padded(instr_t *instr, bool val);

/* Returns true iff \p instr has been marked as a special fragment
 * exit cti.
 */
bool
instr_branch_special_exit(instr_t *instr);

/* If \p val is true, indicates that \p instr is a special exit cti.
 * If \p val is false, indicates otherwise.
 */
void
instr_branch_set_special_exit(instr_t *instr, bool val);

DR_API
INSTR_INLINE
/**
 * Return true iff \p instr is an application (non-meta) instruction
 * (see instr_set_app() for more information).
 */
bool
instr_is_app(instr_t *instr);

DR_API
/**
 * Sets \p instr as an application (non-meta) instruction.
 * An application instruction might be mangled by DR if necessary,
 * e.g., to create an exit stub for a branch instruction.
 * All application instructions that are added to basic blocks or
 * traces should have their translation fields set (via
 * #instr_set_translation()).
 */
void
instr_set_app(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Return true iff \p instr is a meta instruction
 * (see instr_set_meta() for more information).
 */
bool
instr_is_meta(instr_t *instr);

DR_API
/**
 * Sets \p instr as a meta instruction.
 * A meta instruction will not be mangled by DR in any way, which is necessary
 * to have DR not create an exit stub for a branch instruction.
 * Meta instructions should not fault (unless such faults are handled
 * by the client) and are not considered
 * application instructions but rather added instrumentation code (see
 * #dr_register_bb_event() for further information).
 */
void
instr_set_meta(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Return true iff \p instr is not a meta-instruction
 * (see instr_set_app() for more information).
 *
 * \deprecated instr_is_app()/instr_is_meta() should be used instead.
 */
bool
instr_ok_to_mangle(instr_t *instr);

DR_API
/**
 * Sets \p instr to "ok to mangle" if \p val is true and "not ok to
 * mangle" if \p val is false.
 *
 * \deprecated instr_set_app()/instr_set_meta() should be used instead.
 */
void
instr_set_ok_to_mangle(instr_t *instr, bool val);

DR_API
/**
 * A convenience routine that calls both
 * #instr_set_meta (instr) and
 * #instr_set_translation (instr, NULL).
 */
void
instr_set_meta_no_translation(instr_t *instr);

DR_API
#ifdef AVOID_API_EXPORT
/* This is hot internally, but unlikely to be used by clients. */
INSTR_INLINE
#endif
/** Return true iff \p instr is to be emitted into the cache. */
bool
instr_ok_to_emit(instr_t *instr);

DR_API
/**
 * Set \p instr to "ok to emit" if \p val is true and "not ok to emit"
 * if \p val is false.  An instruction that should not be emitted is
 * treated normally by DR for purposes of exits but is not placed into
 * the cache.  It is used for final jumps that are to be elided.
 */
void
instr_set_ok_to_emit(instr_t *instr, bool val);

DR_API
/**
 * Returns the length of \p instr.
 * As a side effect, if instr_is_app(instr) and \p instr's raw bits
 * are invalid, encodes \p instr into bytes allocated with
 * instr_allocate_raw_bits(), after which instr is marked as having
 * valid raw bits.
 */
int
instr_length(dcontext_t *dcontext, instr_t *instr);

/* not exported */
void
instr_shift_raw_bits(instr_t *instr, ssize_t offs);
int
instr_exit_branch_type(instr_t *instr);
void
instr_exit_branch_set_type(instr_t *instr, uint type);

DR_API
/** Returns the total number of bytes of memory used by \p instr. */
int
instr_mem_usage(instr_t *instr);

DR_API
/**
 * Returns a copy of \p orig with separately allocated memory for
 * operands and raw bytes if they were present in \p orig.
 * Only a shallow copy of the \p note field is made. The \p label_cb
 * field will not be copied at all if \p orig is a label instruction.
 */
instr_t *
instr_clone(dcontext_t *dcontext, instr_t *orig);

DR_API
/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(opcode)
 * - instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build(dcontext_t *dcontext, int opcode, int num_dsts, int num_srcs);

DR_API
/**
 * Convenience routine: calls
 * - instr_create(dcontext)
 * - instr_set_opcode(instr, opcode)
 * - instr_allocate_raw_bits(dcontext, instr, num_bytes)
 *
 * and returns the resulting instr_t.
 */
instr_t *
instr_build_bits(dcontext_t *dcontext, int opcode, uint num_bytes);

DR_API
/**
 * Returns true iff \p instr's opcode is NOT OP_INVALID.
 * Not to be confused with an invalid opcode, which can be OP_INVALID or
 * OP_UNDECODED.  OP_INVALID means an instruction with no valid fields:
 * raw bits (may exist but do not correspond to a valid instr), opcode,
 * eflags, or operands.  It could be an uninitialized
 * instruction or the result of decoding an invalid sequence of bytes.
 */
bool
instr_valid(instr_t *instr);

DR_API
/** Get the original application PC of \p instr if it exists. */
app_pc
instr_get_app_pc(instr_t *instr);

DR_API
/** Returns \p instr's opcode (an OP_ constant). */
int
instr_get_opcode(instr_t *instr);

DR_API
/** Assumes \p opcode is an OP_ constant and sets it to be instr's opcode. */
void
instr_set_opcode(instr_t *instr, int opcode);

const struct instr_info_t *
instr_get_instr_info(instr_t *instr);

const struct instr_info_t *
get_instr_info(int opcode);

DR_API
INSTR_INLINE
/**
 * Returns the number of source operands of \p instr.
 *
 * \note Addressing registers used in destination memory references
 * (i.e., base, index, or segment registers) are not separately listed
 * as source operands.
 */
int
instr_num_srcs(instr_t *instr);

DR_API
INSTR_INLINE
/**
 * Returns the number of destination operands of \p instr.
 */
int
instr_num_dsts(instr_t *instr);

DR_API
/**
 * Assumes that \p instr has been initialized but does not have any
 * operands yet.  Allocates storage for \p num_srcs source operands
 * and \p num_dsts destination operands.
 */
void
instr_set_num_opnds(dcontext_t *dcontext, instr_t *instr, int num_dsts, int num_srcs);

DR_API
/**
 * Returns \p instr's source operand at position \p pos (0-based).
 */
opnd_t
instr_get_src(instr_t *instr, uint pos);

DR_API
/**
 * Returns \p instr's destination operand at position \p pos (0-based).
 */
opnd_t
instr_get_dst(instr_t *instr, uint pos);

DR_API
/**
 * Sets \p instr's source operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_src(instr_t *instr, uint pos, opnd_t opnd);

DR_API
/**
 * Sets \p instr's destination operand at position \p pos to be \p opnd.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_dst(instr_t *instr, uint pos, opnd_t opnd);

DR_API
/**
 * Removes \p instr's source operands from position \p start up to
 * but not including position \p end (so pass n,n+1 to remove just position n).
 * Shifts all subsequent source operands (if any) down in the operand array.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_remove_srcs(dcontext_t *dcontext, instr_t *instr, uint start, uint end);

DR_API
/**
 * Removes \p instr's destination operands from position \p start up to
 * but not including position \p end (so pass n,n+1 to remove just position n).
 * Shifts all subsequent destination operands (if any) down in the operand array.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_remove_dsts(dcontext_t *dcontext, instr_t *instr, uint start, uint end);

DR_API
/**
 * Assumes that \p cti_instr is a control transfer instruction
 * Returns the first source operand of \p cti_instr (its target).
 */
opnd_t
instr_get_target(instr_t *cti_instr);

DR_API
/**
 * Assumes that \p cti_instr is a control transfer instruction.
 * Sets the first source operand of \p cti_instr to be \p target.
 * Also calls instr_set_raw_bits_valid(\p instr, false) and
 * instr_set_operands_valid(\p instr, true).
 */
void
instr_set_target(instr_t *cti_instr, opnd_t target);

INSTR_INLINE_INTERNALLY
DR_API
/** Returns true iff \p instr's operands are up to date. */
bool
instr_operands_valid(instr_t *instr);

DR_API
/** Sets \p instr's operands to be valid if \p valid is true, invalid otherwise. */
void
instr_set_operands_valid(instr_t *instr, bool valid);

DR_API
/**
 * Returns true iff \p instr's opcode is valid.
 * If the opcode is ever set to other than OP_INVALID or OP_UNDECODED it is assumed
 * to be valid.  However, calling instr_get_opcode() will attempt to
 * decode a valid opcode, hence the purpose of this routine.
 */
bool
instr_opcode_valid(instr_t *instr);

/******************************************************************
 * Eflags validity is not exported!  It's hidden.  Calling get_eflags or
 * get_arith_flags will make them valid if they're not.
 */

bool
instr_arith_flags_valid(instr_t *instr);

/* Sets instr's arithmetic flags (the 6 bottom eflags) to be valid if
 * valid is true, invalid otherwise. */
void
instr_set_arith_flags_valid(instr_t *instr, bool valid);

/* Returns true iff instr's eflags are up to date. */
bool
instr_eflags_valid(instr_t *instr);

/* Sets instr's eflags to be valid if valid is true, invalid otherwise. */
void
instr_set_eflags_valid(instr_t *instr, bool valid);

uint
instr_eflags_conditionally(uint full_eflags, dr_pred_type_t pred,
                           dr_opnd_query_flags_t flags);

DR_API
/**
 * Returns \p instr's eflags use as EFLAGS_ constants (e.g., EFLAGS_READ_CF,
 * EFLAGS_WRITE_OF, etc.) or'ed together.
 * Which eflags are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 */
uint
instr_get_eflags(instr_t *instr, dr_opnd_query_flags_t flags);

DR_API
/**
 * Returns the eflags usage of instructions with opcode \p opcode,
 * as EFLAGS_ constants (e.g., EFLAGS_READ_CF, EFLAGS_WRITE_OF, etc.) or'ed
 * together.
 * If \p opcode is predicated (see instr_is_predicated()) or if the set
 * of flags read or written varies with an operand value, this routine
 * returns the maximal set that might be accessed or written.
 */
uint
instr_get_opcode_eflags(int opcode);

DR_API
/**
 * Returns \p instr's arithmetic flags (bottom 6 eflags) use as EFLAGS_
 * constants (e.g., EFLAGS_READ_CF, EFLAGS_WRITE_OF, etc.) or'ed together.
 * If \p instr's eflags behavior has not been calculated yet or is
 * invalid, the entire eflags use is calculated and returned (not
 * just the arithmetic flags).
 * Which eflags are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 */
uint
instr_get_arith_flags(instr_t *instr, dr_opnd_query_flags_t flags);

/*
 ******************************************************************/

DR_API
/**
 * Assumes that \p instr does not currently have any raw bits allocated.
 * Sets \p instr's raw bits to be \p length bytes starting at \p addr.
 * Does not set the operands invalid.
 */
void
instr_set_raw_bits(instr_t *instr, byte *addr, uint length);

DR_API
/** Sets \p instr's raw bits to be valid if \p valid is true, invalid otherwise. */
void
instr_set_raw_bits_valid(instr_t *instr, bool valid);

INSTR_INLINE_INTERNALLY
DR_API
/** Returns true iff \p instr's raw bits are a valid encoding of instr. */
bool
instr_raw_bits_valid(instr_t *instr);

INSTR_INLINE_INTERNALLY
DR_API
/** Returns true iff \p instr has its own allocated memory for raw bits. */
bool
instr_has_allocated_bits(instr_t *instr);

INSTR_INLINE_INTERNALLY
DR_API
/** Returns true iff \p instr's raw bits are not a valid encoding of \p instr. */
bool
instr_needs_encoding(instr_t *instr);

DR_API
/**
 * Return true iff \p instr is not a meta-instruction that can fault
 * (see instr_set_meta_may_fault() for more information).
 *
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
bool
instr_is_meta_may_fault(instr_t *instr);

DR_API
/**
 * \deprecated Any meta instruction can fault if it has a non-NULL
 * translation field and the client fully handles all of its faults,
 * so this routine is no longer needed.
 */
void
instr_set_meta_may_fault(instr_t *instr, bool val);

DR_API
/**
 * Allocates \p num_bytes of memory for \p instr's raw bits.
 * If \p instr currently points to raw bits, the allocated memory is
 * initialized with the bytes pointed to.
 * \p instr is then set to point to the allocated memory.
 */
void
instr_allocate_raw_bits(dcontext_t *dcontext, instr_t *instr, uint num_bytes);

DR_API
/**
 * Sets the translation pointer for \p instr, used to recreate the
 * application address corresponding to this instruction.  When adding
 * or modifying instructions that are to be considered application
 * instructions (i.e., non meta-instructions: see #instr_is_app),
 * the translation should always be set.  Pick
 * the application address that if executed will be equivalent to
 * restarting \p instr.  Currently the translation address must lie
 * within the existing bounds of the containing code block.
 * Returns the supplied \p instr (for easy chaining).  Use
 * #instr_get_app_pc to see the current value of the translation.
 */
instr_t *
instr_set_translation(instr_t *instr, app_pc addr);

DR_UNS_API
/**
 * If the translation pointer is set for \p instr, returns that
 * else returns NULL.
 * \note The translation pointer is not automatically set when
 * decoding instructions from raw bytes (via decode(), e.g.); it is
 * set for instructions in instruction lists generated by DR (see
 * dr_register_bb_event()).
 *
 */
app_pc
instr_get_translation(instr_t *instr);

DR_API
/**
 * Calling this function with \p instr makes it safe to keep the
 * instruction around indefinitely when its raw bits point into the
 * cache.  The function allocates memory local to \p instr to hold a
 * copy of the raw bits. If this was not done, the original raw bits
 * could be deleted at some point.  Making an instruction persistent
 * is necessary if you want to keep it beyond returning from the call
 * that produced the instruction.
 */
void
instr_make_persistent(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Assumes that \p instr's raw bits are valid.
 * Returns a pointer to \p instr's raw bits.
 * \note A freshly-decoded instruction has valid raw bits that point to the
 * address from which it was decoded.  However, for instructions presented
 * in the basic block or trace events, use instr_get_app_pc() to retrieve
 * the corresponding application address, as the raw bits will not be set
 * for instructions added after decoding, and may point to a different location
 * for insructions that have been modified.
 */
byte *
instr_get_raw_bits(instr_t *instr);

DR_API
/** If \p instr has raw bits allocated, frees them. */
void
instr_free_raw_bits(dcontext_t *dcontext, instr_t *instr);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and have > \p pos bytes.
 * Returns a pointer to \p instr's raw byte at position \p pos (beginning with 0).
 */
byte
instr_get_raw_byte(instr_t *instr, uint pos);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > \p pos bytes.
 * Sets instr's raw byte at position \p pos (beginning with 0) to the value \p byte.
 */
void
instr_set_raw_byte(instr_t *instr, uint pos, byte byte);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have >= num_bytes bytes.
 * Copies the \p num_bytes beginning at start to \p instr's raw bits.
 */
void
instr_set_raw_bytes(instr_t *instr, byte *start, uint num_bytes);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and allocated by \p instr
 * and have > pos+3 bytes.
 * Sets the 4 bytes beginning at position \p pos (0-based) to the value word.
 */
void
instr_set_raw_word(instr_t *instr, uint pos, uint word);

DR_API
/**
 * Assumes that \p instr's raw bits are valid and have > \p pos + 3 bytes.
 * Returns the 4 bytes beginning at position \p pos (0-based).
 */
uint
instr_get_raw_word(instr_t *instr, uint pos);

DR_API
/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Ors \p instr's prefixes with \p prefix.
 * Returns the supplied instr (for easy chaining).
 */
instr_t *
instr_set_prefix_flag(instr_t *instr, uint prefix);

DR_API
/**
 * Assumes that \p prefix is a PREFIX_ constant.
 * Returns true if \p instr's prefixes contain the flag \p prefix.
 */
bool
instr_get_prefix_flag(instr_t *instr, uint prefix);

/* NOT EXPORTED because we want to limit a client to seeing only the
 * handful of PREFIX_ flags we're exporting.
 * Assumes that prefix is a group of PREFIX_ constants or-ed together.
 * Sets instr's prefixes to be exactly those flags in prefixes.
 */
void
instr_set_prefixes(instr_t *instr, uint prefixes);

/* NOT EXPORTED because we want to limit a client to seeing only the
 * handful of PREFIX_ flags we're exporting.
 * Returns instr's prefixes as PREFIX_ constants or-ed together.
 */
uint
instr_get_prefixes(instr_t *instr);

DR_API
/**
 * Returns whether \p instr is predicated: i.e., whether its operation
 * is conditional.
 */
bool
instr_is_predicated(instr_t *instr);

DR_API
/**
 * Returns the DR_PRED_ constant for \p instr that describes what its
 * conditional execution is dependent on.
 */
dr_pred_type_t
instr_get_predicate(instr_t *instr);

#ifdef ARM
DR_API
/**
 * Returns the string name corresponding to the given DR_PRED_ constant.
 * \note ARM-only.
 */
const char *
instr_predicate_name(dr_pred_type_t pred);
#endif

#ifdef AARCHXX
DR_API
/**
 * Returns the DR_PRED_ constant that represents the opposite condition
 * from \p pred.  A valid conditional branch predicate must be passed (i.e.,
 * not #DR_PRED_NONE, #DR_PRED_AL, or #DR_PRED_OP for ARM and not #DR_PRED_NONE,
 * #DR_PRED_AL, or #DR_PRED_NV for AArch64).
 * \note ARM and AArch64-only.
 */
dr_pred_type_t
instr_invert_predicate(dr_pred_type_t pred);
#endif

#ifdef ARM
DR_API
/**
 * Assumes that \p it_instr's opcode is #OP_it.  Returns the number of instructions
 * in the IT block that \p it_instr heads.
 * \note ARM-only.
 */
uint
instr_it_block_get_count(instr_t *it_instr);

DR_API
/**
 * Assumes that \p it_instr's opcode is #OP_it.  Returns the predicate for the
 * instruction with ordinal \p index in IT block that \p it_instr heads.
 * \note ARM-only.
 */
dr_pred_type_t
instr_it_block_get_pred(instr_t *it_instr, uint index);

DR_API
/**
 * Computes immediates (firstcond and mask) for creating a new instruction with
 * opcode #OP_it with the given predicates.  Up to four instructions can exist
 * in a single IT block.  Pass #DR_PRED_NONE for all predicates beyond the
 * desired instruction count in the newly created IT block.
 * Returns whether the given predicates are valid for creating an IT block.
 * \note ARM-only.
 */
bool
instr_it_block_compute_immediates(dr_pred_type_t pred0, dr_pred_type_t pred1,
                                  dr_pred_type_t pred2, dr_pred_type_t pred3,
                                  byte *firstcond_out, byte *mask_out);

DR_API
/**
 * Creates a new instruction with opcode #OP_it and immediates set to encode
 * an IT block with the given predicates.  Up to four instructions can exist
 * in a single IT block.  Pass #DR_PRED_NONE for all predicates beyond the
 * desired instruction count in the newly created IT block.
 * \note ARM-only.
 */
instr_t *
instr_it_block_create(dcontext_t *dcontext, dr_pred_type_t pred0, dr_pred_type_t pred1,
                      dr_pred_type_t pred2, dr_pred_type_t pred3);
#endif /* ARM */

DR_API
/**
 * Returns true iff \p instr is an exclusive load instruction,
 * e.g., #OP_ldrex on ARM.
 */
bool
instr_is_exclusive_load(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is an exclusive store instruction,
 * e.g., #OP_strex on ARM.
 */
bool
instr_is_exclusive_store(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a scatter-store instruction.
 */
bool
instr_is_scatter(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a gather-load instruction.
 */
bool
instr_is_gather(instr_t *instr);

bool
instr_predicate_reads_srcs(dr_pred_type_t pred);

bool
instr_predicate_writes_eflags(dr_pred_type_t pred);

DR_API
/**
 * Returns true iff \p pred denotes a truly conditional predicate: on all
 * architectures, this excludes #DR_PRED_NONE. On ARM it also excludes
 * #DR_PRED_AL and #DR_PRED_OP; on AArch64, it also excludes #DR_PRED_AL
 * and #DR_PRED_NV.
 */
bool
instr_predicate_is_cond(dr_pred_type_t pred);

DR_API
/**
 * Sets the predication for \p instr to the given DR_PRED_ constant.
 * Returns \p instr if successful, or NULL if unsuccessful.
 */
instr_t *
instr_set_predicate(instr_t *instr, dr_pred_type_t pred);

/* DR_API EXPORT BEGIN */
/** This type holds the return values for instr_predicate_triggered(). */
typedef enum _dr_pred_trigger_t {
    /** This instruction is not predicated. */
    DR_PRED_TRIGGER_NOPRED,
    /** The predicate matches and the instruction will execute. */
    DR_PRED_TRIGGER_MATCH,
    /** The predicate does not match and the instruction will not execute. */
    DR_PRED_TRIGGER_MISMATCH,
    /** It is unknown whether the predicate matches. */
    DR_PRED_TRIGGER_UNKNOWN,
    /** An invalid parameter was passed. */
    DR_PRED_TRIGGER_INVALID,
} dr_pred_trigger_t;
/* DR_API EXPORT END */

DR_API
/**
 * Given the machine context \p mc, returns whether or not the predicated
 * instruction \p instr will execute.
 * Currently condition-code predicates are supported and OP_bsf and
 * OP_bsr from #DR_PRED_COMPLEX; other instances of #DR_PRED_COMPLEX
 * are not supported.
 * \p mc->flags must include #DR_MC_CONTROL for condition-code predicates,
 * and additionally #DR_MC_INTEGER for OP_bsf and OP_bsr.
 *
 * \note More complex predicates will be added in the future and they may require
 * additional state in \p mc.
 */
dr_pred_trigger_t
instr_predicate_triggered(instr_t *instr, dr_mcontext_t *mc);

/* DR_API EXPORT BEGIN */
#if defined(X86) && defined(X64)
/* DR_API EXPORT END */
DR_API
/**
 * Each instruction stores whether it should be interpreted in 32-bit
 * (x86) or 64-bit (x64) mode.  This routine sets the mode for \p instr.
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by instr_set_isa_mode().
 */
void
instr_set_x86_mode(instr_t *instr, bool x86);

DR_API
/**
 * Returns true if \p instr is an x86 instruction (32-bit) and false
 * if \p instr is an x64 instruction (64-bit).
 *
 * \note For 64-bit DR builds only.
 *
 * \deprecated Replaced by instr_get_isa_mode().
 */
bool
instr_get_x86_mode(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Each instruction stores the processor mode under which it should be
 * interpreted.  This routine sets the mode for \p instr.
 */
bool
instr_set_isa_mode(instr_t *instr, dr_isa_mode_t mode);

DR_API
/**
 * Each instruction stores the processor mode under which it should be
 * interpreted.  This routine returns the mode for \p instr.
 */
dr_isa_mode_t
instr_get_isa_mode(instr_t *instr);

DR_API
/**
 * Each instruction may store a hint for how the instruction should be encoded if
 * redundant encodings are available. This presumes that the user knows that a
 * redundant encoding is available. This routine sets the \p hint for \p instr.
 * Returns \p instr (for easy chaining).
 */
instr_t *
instr_set_encoding_hint(instr_t *instr, dr_encoding_hint_type_t hint);

DR_API
/**
 * Each instruction may store a hint for how the instruction should be encoded if
 * redundant encodings are available. This presumes that the user knows that a
 * redundant encoding is available. This routine returns whether the \p hint is set
 * for \p instr.
 */
bool
instr_has_encoding_hint(instr_t *instr, dr_encoding_hint_type_t hint);

/***********************************************************************/
/* decoding routines */

DR_UNS_API
/**
 * If instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands instr into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.
 * Returns the replacement of instr, if any expansion is performed
 * (in which case the old instr is destroyed); otherwise returns
 * instr unchanged.
 */
instr_t *
instr_expand(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * Returns true if instr is at Level 0 (i.e. a bundled group of instrs
 * as raw bits).
 */
bool
instr_is_level_0(instr_t *inst);

DR_UNS_API
/**
 * If the next instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new next instr.
 */
instr_t *
instr_get_next_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * If the prev instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new prev instr.
 */
instr_t *
instr_get_prev_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already at the level of decode_cti, decodes enough
 * from the raw bits pointed to by instr to bring it to that level.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 *
 * decode_cti decodes only enough of instr to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * instr are filled in.  If not, only the raw bits fields of instr are
 * filled in.  This corresponds to a Level 3 decoding for control
 * transfer instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 */
void
instr_decode_cti(dcontext_t *dcontext, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already at the level of decode_opcode, decodes enough
 * from the raw bits pointed to by instr to bring it to that level.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 *
 * decode_opcode decodes the opcode and eflags usage of the instruction
 * (if the eflags usage varies with operand values, the maximal value
 * will be set).
 * This corresponds to a Level 2 decoding.
 */
void
instr_decode_opcode(dcontext_t *dcontext, instr_t *instr);

DR_UNS_API
/**
 * If instr is not already fully decoded, decodes enough
 * from the raw bits pointed to by instr to bring it Level 3.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 */
void
instr_decode(dcontext_t *dcontext, instr_t *instr);

/* Calls instr_decode() with the current dcontext.  *Not* exported.  Mostly
 * useful as the slow path for IR routines that get inlined.
 */
instr_t *
instr_decode_with_current_dcontext(instr_t *instr);

/* DR_API EXPORT TOFILE dr_ir_instrlist.h */
DR_UNS_API
/**
 * If the first instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new first instr.
 */
instr_t *
instrlist_first_expanded(dcontext_t *dcontext, instrlist_t *ilist);

DR_UNS_API
/**
 * If the last instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new last instr.
 */
instr_t *
instrlist_last_expanded(dcontext_t *dcontext, instrlist_t *ilist);

DR_UNS_API
/**
 * Brings all instrs in ilist up to the decode_cti level, and
 * hooks up intra-ilist cti targets to use instr_t targets, by
 * matching pc targets to each instruction's raw bits.
 *
 * decode_cti decodes only enough of instr to determine
 * its size, its effects on the 6 arithmetic eflags, and whether it is
 * a control-transfer instruction.  If it is, the operands fields of
 * instr are filled in.  If not, only the raw bits fields of instr are
 * filled in.  This corresponds to a Level 3 decoding for control
 * transfer instructions but a Level 1 decoding plus arithmetic eflags
 * information for all other instructions.
 */
void
instrlist_decode_cti(dcontext_t *dcontext, instrlist_t *ilist);
/* DR_API EXPORT TOFILE dr_ir_instr.h */

/***********************************************************************/
/* utility functions */

DR_API
/**
 * Shrinks all registers not used as addresses, and all immed integer and
 * address sizes, to 16 bits.
 * Does not shrink DR_REG_ESI or DR_REG_EDI used in string instructions.
 */
void
instr_shrink_to_16_bits(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef X64
/* DR_API EXPORT END */
DR_API
/**
 * Shrinks all registers, including addresses, and all immed integer and
 * address sizes, to 32 bits.
 *
 * \note For 64-bit DR builds only.
 */
void
instr_shrink_to_32_bits(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's operands references a
 * register that overlaps \p reg.
 *
 * Returns false for multi-byte nops with an operand using reg.
 */
bool
instr_uses_reg(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Returns true iff at least one of \p instr's operands references a floating
 * point register.
 */
bool
instr_uses_fp_reg(instr_t *instr);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's source operands references \p reg.
 *
 * Returns false for multi-byte nops with a source operand using reg.
 *
 * \note Use instr_reads_from_reg() to also consider addressing
 * registers in destination operands.
 */
bool
instr_reg_in_src(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands references \p reg.
 */
bool
instr_reg_in_dst(instr_t *instr, reg_id_t reg);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * a register operand for a register that overlaps \p reg.
 * Which operands are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 */
bool
instr_writes_to_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags);

DR_API
/**
 * Assumes that reg is a DR_REG_ constant.
 * Returns true iff at least one of instr's operands reads
 * from a register that overlaps reg (checks both source operands
 * and addressing registers used in destination operands).
 *
 * Returns false for multi-byte nops with an operand using reg.
 *
 * Which operands are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 * As a special case, the addressing registers inside a destination memory
 * operand are covered by DR_QUERY_INCLUDE_COND_SRCS rather than
 * DR_QUERY_INCLUDE_COND_DSTS.
 */
bool
instr_reads_from_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's destination operands is
 * the same register (not enough to just overlap) as \p reg.
 * Which operands are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 */
bool
instr_writes_to_exact_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags);

DR_API
/**
 * Assumes that \p reg is a DR_REG_ constant.
 * Returns true iff at least one of \p instr's source operands is
 * the same register (not enough to just overlap) as \p reg.
 *
 * For example, false is returned if the instruction \p instr is vmov [m], zmm0
 * and the register being tested \p reg is \p DR_REG_XMM0.
 *
 * Registers used in memory operands, namely base, index and segmentation registers,
 * are checked also by this routine. This also includes destination operands.
 *
 * Which operands are considered to be accessed for conditionally executed
 * instructions are controlled by \p flags.
 */
bool
instr_reads_from_exact_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags);

DR_API
/**
 * Replaces all instances of \p old_opnd in \p instr's source operands with
 * \p new_opnd (uses opnd_same() to detect sameness).
 * Returns whether it replaced anything.
 */
bool
instr_replace_src_opnd(instr_t *instr, opnd_t old_opnd, opnd_t new_opnd);

DR_API
/**
 * Replaces all instances of \p old_reg (or any size variant) in \p instr's operands
 * with \p new_reg.  Resizes \p new_reg to match sub-full-size uses of \p old_reg.
 * Returns whether it replaced anything.
 */
bool
instr_replace_reg_resize(instr_t *instr, reg_id_t old_reg, reg_id_t new_reg);

DR_API
/**
 * Returns true iff \p instr1 and \p instr2 have the same opcode, prefixes,
 * and source and destination operands (uses opnd_same() to compare the operands).
 */
bool
instr_same(instr_t *instr1, instr_t *instr2);

DR_API
/**
 * Returns true iff any of \p instr's source operands is a memory reference.
 *
 * Unlike opnd_is_memory_reference(), this routine conisders the
 * semantics of the instruction and returns false for both multi-byte
 * nops with a memory operand and for the #OP_lea instruction, as they
 * do not really reference the memory.  It does return true for
 * prefetch instructions.
 *
 * If \p instr is predicated (see instr_is_predicated()), the memory reference
 * may not always be accessed.
 */
bool
instr_reads_memory(instr_t *instr);

DR_API
/**
 * Returns true iff any of \p instr's destination operands is a memory
 * reference.  If \p instr is predicated (see instr_is_predicated()), the
 * destination may not always be written.
 */
bool
instr_writes_memory(instr_t *instr);

#ifdef X86
DR_API
/**
 * Returns true iff \p instr writes to an xmm register and zeroes the top half
 * of the corresponding ymm register as a result (some instructions preserve
 * the top half while others zero it when writing to the bottom half).
 * This zeroing will occur even if \p instr is predicated (see instr_is_predicated()).
 */
/* XXX i#1312: For AVX-512, we will want a instr_zeroes_zmmh function as well that also
 * includes the vzeroupper instruction.
 */
bool
instr_zeroes_ymmh(instr_t *instr);

DR_API
/** Returns true if \p instr's opcode is #OP_xsave32, #OP_xsaveopt32, #OP_xsave64,
 * #OP_xsaveopt64, #OP_xsavec32 or #OP_xsavec64.
 */
bool
instr_is_xsave(instr_t *instr);
#endif

DR_API
/**
 * If any of \p instr's operands is a rip-relative data or instruction
 * memory reference, returns the address that reference targets.  Else
 * returns false.  For instruction references, only PC operands are
 * considered: not instruction pointer operands.
 *
 * \note Currently this is only implemented for x86.
 */
bool
instr_get_rel_data_or_instr_target(instr_t *instr, /*OUT*/ app_pc *target);

/* DR_API EXPORT BEGIN */
#if defined(X64) || defined(ARM)
/* DR_API EXPORT END */
DR_API
/**
 * Returns true iff any of \p instr's operands is a rip-relative data memory reference.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_has_rel_addr_reference(instr_t *instr);

DR_API
/**
 * If any of \p instr's operands is a rip-relative data memory reference, returns the
 * address that reference targets.  Else returns false.
 *
 * \note For 64-bit DR builds only.
 */
bool
instr_get_rel_addr_target(instr_t *instr, /*OUT*/ app_pc *target);

DR_API
/**
 * If any of \p instr's destination operands is a rip-relative data memory
 * reference, returns the operand position.  If there is no such
 * destination operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_dst_idx(instr_t *instr);

DR_API
/**
 * If any of \p instr's source operands is a rip-relative memory
 * reference, returns the operand position.  If there is no such
 * source operand, returns -1.
 *
 * \note For 64-bit DR builds only.
 */
int
instr_get_rel_addr_src_idx(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif /* X64 || ARM */
/* DR_API EXPORT END */

#ifdef X86
/* We're not exposing the low-level rip_rel_pos routines directly to clients,
 * who should only use this level 1-3 feature via decode_cti + encode.
 */

/* Returns true iff instr's raw bits are valid and the offset within
 * those raw bits of a rip-relative displacement is set (to 0 if there
 * is no such displacement).
 */
bool
instr_rip_rel_valid(instr_t *instr);

/* Sets whether instr's rip-relative displacement offset is valid. */
void
instr_set_rip_rel_valid(instr_t *instr, bool valid);

/* Assumes that instr_rip_rel_valid() is true.
 * Returns the offset within the encoded bytes of instr of the
 * displacement used for rip-relative addressing; returns 0
 * if instr has no rip-relative operand.
 * There can be at most one rip-relative operand in one instruction.
 */
uint
instr_get_rip_rel_pos(instr_t *instr);

/* Sets the offset within instr's encoded bytes of instr's
 * rip-relative displacement (the offset should be 0 if there is no
 * rip-relative operand) and marks it valid.  \p pos must be less
 * than 256.
 */
void
instr_set_rip_rel_pos(instr_t *instr, uint pos);
#endif /* X86 */

/* not exported: for PR 267260 */
bool
instr_is_our_mangling(instr_t *instr);

/* Sets whether instr came from our mangling. */
void
instr_set_our_mangling(instr_t *instr, bool ours);

/* Returns whether instr came from our mangling and is in epilogue. This routine
 * requires the caller to already know that instr is our_mangling.
 */
bool
instr_is_our_mangling_epilogue(instr_t *instr);

/* Sets whether instr came from our mangling and is in epilogue. */
void
instr_set_our_mangling_epilogue(instr_t *instr, bool epilogue);

/* Sets that instr is in our mangling epilogue as well as the translation pointer
 * for instr, by adding the translation pointer of mangle_instr to its length.
 * Returns the instr.
 */
instr_t *
instr_set_translation_mangling_epilogue(dcontext_t *dcontext, instrlist_t *ilist,
                                        instr_t *instr);

DR_API
/**
 * Returns NULL if none of \p instr's operands is a memory reference.
 * Otherwise, returns the effective address of the first memory operand
 * when the operands are considered in this order: destinations and then
 * sources.  The address is computed using the passed-in registers.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
app_pc
instr_compute_address(instr_t *instr, dr_mcontext_t *mc);

app_pc
instr_compute_address_priv(instr_t *instr, priv_mcontext_t *mc);

DR_API
/**
 * Performs address calculation in the same manner as
 * instr_compute_address() but handles multiple memory operands.  The
 * \p index parameter should be initially set to 0 and then
 * incremented with each successive call until this routine returns
 * false, which indicates that there are no more memory operands.  The
 * address of each is computed in the same manner as
 * instr_compute_address() and returned in \p addr; whether it is a
 * write is returned in \p is_write.  Either or both OUT variables can
 * be NULL.
 * \p mc->flags must include DR_MC_CONTROL and DR_MC_INTEGER.
 * For instructions that use vector addressing (VSIB, introduced in AVX2),
 * mc->flags must additionally include DR_MC_MULTIMEDIA.
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex(instr_t *instr, dr_mcontext_t *mc, uint index, OUT app_pc *addr,
                         OUT bool *write);

DR_API
/**
 * Performs address calculation in the same manner as
 * instr_compute_address_ex() with additional information
 * of which opnd is used for address computation returned
 * in \p pos. If \p pos is NULL, it is the same as
 * instr_compute_address_ex().
 *
 * Like instr_reads_memory(), this routine does not consider
 * multi-byte nops that use addressing operands, or the #OP_lea
 * instruction's source operand, to be memory references.
 */
bool
instr_compute_address_ex_pos(instr_t *instr, dr_mcontext_t *mc, uint index,
                             OUT app_pc *addr, OUT bool *is_write, OUT uint *pos);

bool
instr_compute_address_ex_priv(instr_t *instr, priv_mcontext_t *mc, uint index,
                              OUT app_pc *addr, OUT bool *write, OUT uint *pos);

DR_API
/**
 * Calculates the size, in bytes, of the memory read or write of \p instr.
 * If \p instr does not reference memory, or is invalid, returns 0.
 * If \p instr is a repeated string instruction, considers only one iteration.
 * If \p instr uses vector addressing (VSIB, introduced in AVX2), considers
 * only the size of each separate memory access.
 */
uint
instr_memory_reference_size(instr_t *instr);

DR_API
/**
 * \return a pointer to user-controlled data fields in a label instruction.
 * These fields are available for use by clients for their own purposes.
 * Returns NULL if \p instr is not a label instruction.
 * \note These data fields are copied (shallowly) across instr_clone().
 */
dr_instr_label_data_t *
instr_get_label_data_area(instr_t *instr);

DR_API
/**
 * Set a function \p func which is called when the label instruction is freed.
 * \p instr is the label instruction allowing \p func to free the label's
 * auxiliary data.
 * \note This data field is not copied across instr_clone(). Instead, the
 * clone's field will be NULL (xref i#3962).
 */
void
instr_set_label_callback(instr_t *instr, instr_label_callback_t func);

/* Get a label instructions callback function */
instr_label_callback_t
instr_get_label_callback(instr_t *instr);

/* DR_API EXPORT TOFILE dr_ir_utils.h */
/* DR_API EXPORT BEGIN */

/***************************************************************************
 * DECODE / DISASSEMBLY ROUTINES
 */
/* DR_API EXPORT END */

DR_API
/**
 * Calculates the size, in bytes, of the memory read or write of
 * the instr at \p pc.  If the instruction is a repeating string instruction,
 * considers only one iteration.
 * Returns the pc of the following instruction.
 * If the instruction at \p pc does not reference memory, or is invalid,
 * returns NULL.
 */
app_pc
decode_memory_reference_size(dcontext_t *dcontext, app_pc pc, uint *size_in_bytes);

/* DR_API EXPORT TOFILE dr_ir_instr.h */
DR_API
/**
 * Returns true iff \p instr is an IA-32/AMD64 "mov" instruction: either OP_mov_st,
 * OP_mov_ld, OP_mov_imm, OP_mov_seg, or OP_mov_priv.
 */
bool
instr_is_mov(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_call, OP_call_far, OP_call_ind,
 * or OP_call_far_ind on x86; OP_bl, OP_blx, or OP_blx_ind on ARM.
 */
bool
instr_is_call(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_call or OP_call_far. */
bool
instr_is_call_direct(instr_t *instr);

DR_API
/** Returns true iff \p instr's opcode is OP_call on x86; OP_bl or OP_blx on ARM. */
bool
instr_is_near_call_direct(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_call_ind or OP_call_far_ind on x86;
 * OP_blx_ind on ARM.
 */
bool
instr_is_call_indirect(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_ret, OP_ret_far, or OP_iret on x86.
 * On ARM, returns true iff \p instr reads DR_REG_LR and writes DR_REG_PC.
 */
bool
instr_is_return(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a control transfer instruction of any
 * kind, whether direct, indirect, conditional, or unconditional.
 */
bool
instr_is_cti(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a control transfer instruction that takes an
 * 8-bit offset on x86 (OP_loop*, OP_jecxz, OP_jmp_short, or OP_jcc_short) or
 * a small offset on ARM (OP_cbz, OP_cbnz, OP_b_short).
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_cti_short(instr_t *instr);

DR_API
/** Returns true iff \p instr is one of OP_loop* or OP_jecxz on x86. */
bool
instr_is_cti_loop(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr's opcode is OP_loop* or OP_jecxz on x86
 * or OP_cbz or OP_cbnz on ARM and instr has
 * been transformed to a sequence of instruction that will allow a larger
 * offset.
 * If \p pc != NULL, \p pc is expected to point to the beginning of the encoding of
 * \p instr, and the following instructions are assumed to be encoded in sequence
 * after \p instr.
 * Otherwise, the encoding is expected to be found in \p instr's allocated bits.
 */
#ifdef UNSUPPORTED_API
/**
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
#endif
bool
instr_is_cti_short_rewrite(instr_t *instr, byte *pc);

byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target);

DR_API
/**
 * Returns true iff \p instr is a conditional branch: OP_jcc, OP_jcc_short,
 * OP_loop*, or OP_jecxz on x86; OP_cbnz, OP_cbz, or when a predicate is present
 * any of OP_b, OP_b_short, OP_bx, OP_bxj, OP_bl, OP_blx, OP_blx_ind on ARM.
 */
bool
instr_is_cbr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a multi-way (indirect) branch: OP_jmp_ind,
 * OP_call_ind, OP_ret, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or
 * OP_iret on x86; OP_bx, OP_bxj, OP_blx_ind, or any instruction with a
 * destination register operand of DR_REG_PC on ARM.
 */
bool
instr_is_mbr(instr_t *instr);

bool
instr_is_jump_mem(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is an unconditional direct branch: OP_jmp,
 * OP_jmp_short, or OP_jmp_far on x86; OP_b or OP_b_short with no predicate on ARM.
 */
bool
instr_is_ubr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a near unconditional direct branch: OP_jmp,
 * or OP_jmp_short on x86; OP_b with no predicate on ARM.
 */
bool
instr_is_near_ubr(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is a far control transfer instruction: OP_jmp_far,
 * OP_call_far, OP_jmp_far_ind, OP_call_far_ind, OP_ret_far, or OP_iret, on x86.
 */
bool
instr_is_far_cti(instr_t *instr);

DR_API
/** Returns true if \p instr is an absolute call or jmp that is far. */
bool
instr_is_far_abs_cti(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is used to implement system calls: OP_int with a
 * source operand of 0x80 on linux or 0x2e on windows, or OP_sysenter,
 * or OP_syscall, or #instr_is_wow64_syscall() for WOW64.
 */
bool
instr_is_syscall(instr_t *instr);

/* DR_API EXPORT BEGIN */
#ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Returns true iff \p instr is the indirect transfer from the 32-bit
 * ntdll.dll to the wow64 system call emulation layer.  This
 * instruction will also return true for instr_is_syscall, as well as
 * appear as an indirect call, so clients modifying indirect calls may
 * want to avoid modifying this type.
 *
 * \note Windows-only
 */
bool
instr_is_wow64_syscall(instr_t *instr);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Returns true iff \p instr is a prefetch instruction.
 */
bool
instr_is_prefetch(instr_t *instr);

DR_API
/**
 * Tries to identify common cases of moving a constant into either a
 * register or a memory address.
 * Returns true and sets \p *value to the constant being moved for the following
 * cases: OP_mov_imm, OP_mov_st, and OP_xor where the source equals the
 * destination, for x86; OP_mov, OP_movs, OP_movw, OP_mvn, OP_mvns, or OP_eor
 * where the sources equal the destination and there is no shift, for ARM.
 */
bool
instr_is_mov_constant(instr_t *instr, ptr_int_t *value);

DR_API
/** Returns true iff \p instr is a floating point instruction. */
bool
instr_is_floating(instr_t *instr);

bool
instr_saves_float_pc(instr_t *instr);

#ifdef AARCH64
bool
instr_is_icache_op(instr_t *instr);
#endif

DR_API
/** Returns true iff \p instr is an Intel string operation instruction. */
bool
instr_is_string_op(instr_t *instr);

DR_API
/** Returns true iff \p instr is an Intel repeated-loop string operation instruction. */
bool
instr_is_rep_string_op(instr_t *instr);

/* DR_API EXPORT BEGIN */
/**
 * Indicates which type of floating-point operation and instruction performs.
 */
typedef enum {
    DR_FP_STATE,   /**< Loads, stores, or queries general floating point state. */
    DR_FP_MOVE,    /**< Moves floating point values from one location to another. */
    DR_FP_CONVERT, /**< Converts to or from floating point values. */
    DR_FP_MATH,    /**< Performs arithmetic or conditional operations. */
} dr_fp_type_t;
/* DR_API EXPORT END */

DR_API
/**
 * Returns true iff \p instr is a floating point instruction.
 * @param[in] instr  The instruction to query
 * @param[out] type  If the return value is true and \p type is
 *   non-NULL, the type of the floating point operation is written to \p type.
 */
bool
instr_is_floating_ex(instr_t *instr, dr_fp_type_t *type);

DR_API
/** Returns true iff \p instr is part of Intel's MMX instructions. */
bool
instr_is_mmx(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's AVX-512 scalar opmask instructions. */
bool
instr_is_opmask(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE instructions. */
bool
instr_is_sse(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE2 instructions. */
bool
instr_is_sse2(instr_t *instr);

DR_API
/**
 * Returns true iff \p instr is part of Intel's SSE or SSE2 instructions.
 * \deprecated Use instr_is_sse() combined with instr_is_sse2() instead.
 */
bool
instr_is_sse_or_sse2(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of AMD's 3D-Now! instructions. */
bool
instr_is_3DNow(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE3 instructions. */
bool
instr_is_sse3(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSSE3 instructions. */
bool
instr_is_ssse3(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE4.1 instructions. */
bool
instr_is_sse41(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of Intel's SSE4.2 instructions. */
bool
instr_is_sse42(instr_t *instr);

DR_API
/** Returns true iff \p instr is part of AMD's SSE4A instructions. */
bool
instr_is_sse4A(instr_t *instr);

DR_API
/** Returns true iff \p instr is a "mov $imm -> (%esp)". */
bool
instr_is_mov_imm_to_tos(instr_t *instr);

DR_API
/** Returns true iff \p instr is a label meta-instruction. */
bool
instr_is_label(instr_t *instr);

DR_API
/** Returns true iff \p instr is an "undefined" instruction (ud2) */
bool
instr_is_undefined(instr_t *instr);

#ifdef ARM
bool
instr_is_pop(instr_t *instr);

bool
instr_reads_gpr_list(instr_t *instr);

bool
instr_writes_gpr_list(instr_t *instr);

bool
instr_reads_reg_list(instr_t *instr);

bool
instr_writes_reg_list(instr_t *instr);
#endif

#ifdef X86
bool
instr_can_set_single_step(instr_t *instr);

/* Returns true if \p instr is part of Intel's AVX-512 instructions that may write to a
 * zmm or opmask register. It approximates this by checking whether PREFIX_EVEX is set. If
 * not set, it is looking at whether the instruction's raw bytes are valid, and if they
 * are, whether the instruction is evex-encoded. The function assumes that the
 * instruction's isa mode is set correctly. If the instruction's raw bytes are not valid,
 * it checks the destinations of \p instr.
 */
bool
instr_may_write_zmm_or_opmask_register(instr_t *instr);
#endif

DR_API
/**
 * Assumes that \p instr's opcode is OP_int and that either \p instr's
 * operands or its raw bits are valid.
 * Returns the first source operand if \p instr's operands are valid,
 * else if \p instr's raw bits are valid returns the first raw byte.
 */
int
instr_get_interrupt_number(instr_t *instr);

DR_API
/**
 * Assumes that \p instr is a conditional branch instruction
 * Reverses the logic of \p instr's conditional
 * e.g., changes OP_jb to OP_jnb.
 * Works on cti_short_rewrite as well.
 */
void
instr_invert_cbr(instr_t *instr);

/* PR 266292 */
DR_API
/**
 * Assumes that instr is a meta instruction (instr_is_meta())
 * and an instr_is_cti_short() (<=8-bit reach). Converts instr's opcode
 * to a long form (32-bit reach for x86).  If instr's opcode is OP_loop* or
 * OP_jecxz for x86 or OP_cbnz or OP_cbz for ARM, converts it to a sequence of
 * multiple instructions (which is different from instr_is_cti_short_rewrite()).
 * Each added instruction is marked instr_is_meta().
 * Returns the long form of the instruction, which is identical to \p instr
 * unless \p instr is OP_{loop*,jecxz,cbnz,cbz}, in which case the return value
 * is the final instruction in the sequence, the one that has long reach.
 * \note DR automatically converts app short ctis to long form.
 */
instr_t *
instr_convert_short_meta_jmp_to_long(dcontext_t *dcontext, instrlist_t *ilist,
                                     instr_t *instr);

DR_API
/**
 * Given \p eflags, returns whether or not the conditional branch, \p
 * instr, would be taken.
 */
bool
instr_jcc_taken(instr_t *instr, reg_t eflags);

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 * (not exported since machine state isn't)
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mcontext, bool pre);

DR_API
/**
 * Converts a cmovcc opcode \p cmovcc_opcode to the OP_jcc opcode that
 * tests the same bits in eflags.
 */
int
instr_cmovcc_to_jcc(int cmovcc_opcode);

DR_API
/**
 * Given \p eflags, returns whether or not the conditional move
 * instruction \p instr would execute the move.  The conditional move
 * can be an OP_cmovcc or an OP_fcmovcc instruction.
 */
bool
instr_cmovcc_triggered(instr_t *instr, reg_t eflags);

/* utility routines that are in optimize.c */
opnd_t
instr_get_src_mem_access(instr_t *instr);

void
d_r_loginst(dcontext_t *dcontext, uint level, instr_t *instr, const char *string);

void
d_r_logopnd(dcontext_t *dcontext, uint level, opnd_t opnd, const char *string);

DR_API
/**
 * Returns true if \p instr is one of a class of common nops.
 * currently checks:
 * - nop
 * - nop reg/mem
 * - xchg reg, reg
 * - mov reg, reg
 * - lea reg, (reg)
 */
bool
instr_is_nop(instr_t *instr);

DR_UNS_API
/**
 * Convenience routine to create a nop of a certain size.  If \p raw
 * is true, sets raw bytes rather than filling in the operands or opcode.
 */
instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and no sources or destinations.
 */
instr_t *
instr_create_0dst_0src(dcontext_t *dcontext, int opcode);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and a single source (\p src).
 */
instr_t *
instr_create_0dst_1src(dcontext_t *dcontext, int opcode, opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_0dst_2src(dcontext_t *dcontext, int opcode, opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_0dst_3src(dcontext_t *dcontext, int opcode, opnd_t src1, opnd_t src2,
                       opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_0dst_4src(dcontext_t *dcontext, int opcode, opnd_t src1, opnd_t src2,
                       opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode and one destination (\p dst).
 */
instr_t *
instr_create_1dst_0src(dcontext_t *dcontext, int opcode, opnd_t dst);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination(\p dst),
 * and one source (\p src).
 */
instr_t *
instr_create_1dst_1src(dcontext_t *dcontext, int opcode, opnd_t dst, opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_1dst_2src(dcontext_t *dcontext, int opcode, opnd_t dst, opnd_t src1,
                       opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_1dst_3src(dcontext_t *dcontext, int opcode, opnd_t dst, opnd_t src1,
                       opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and four sources (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_1dst_4src(dcontext_t *dcontext, int opcode, opnd_t dst, opnd_t src1,
                       opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, one destination (\p dst),
 * and five sources (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_1dst_5src(dcontext_t *dcontext, int opcode, opnd_t dst, opnd_t src1,
                       opnd_t src2, opnd_t src3, opnd_t src4, opnd_t src5);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and no sources.
 */
instr_t *
instr_create_2dst_0src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and one source (\p src).
 */
instr_t *
instr_create_2dst_1src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and two sources (\p src1, \p src2).
 */
instr_t *
instr_create_2dst_2src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and three sources (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_2dst_3src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and four sources (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_2dst_4src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, two destinations (\p dst1, \p dst2)
 * and five sources (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_2dst_5src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4, opnd_t src5);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and no sources.
 */
instr_t *
instr_create_3dst_0src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and two sources
 * (\p src1, \p src2).
 */
instr_t *
instr_create_3dst_2src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and three sources
 * (\p src1, \p src2, \p src3).
 */
instr_t *
instr_create_3dst_3src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t src1, opnd_t src2, opnd_t src3);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_3dst_4src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, three destinations
 * (\p dst1, \p dst2, \p dst3) and five sources
 * (\p src1, \p src2, \p src3, \p src4, \p src5).
 */
instr_t *
instr_create_3dst_5src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4,
                       opnd_t src5);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and 1 source (\p src).
 */
instr_t *
instr_create_4dst_1src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t dst4, opnd_t src);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and 2 sources (\p src1 and \p src2).
 */
instr_t *
instr_create_4dst_2src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t dst4, opnd_t src1, opnd_t src2);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated
 * on the thread-local heap with opcode \p opcode, four destinations
 * (\p dst1, \p dst2, \p dst3, \p dst4) and four sources
 * (\p src1, \p src2, \p src3, \p src4).
 */
instr_t *
instr_create_4dst_4src(dcontext_t *dcontext, int opcode, opnd_t dst1, opnd_t dst2,
                       opnd_t dst3, opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, \p fixed_dsts destination operands,
 * and \p fixed_srcs plus \p var_srcs source operands.  The variable arguments
 * must start with the (fixed) destinations, followed by the fixed sources,
 * followed by the variable sources.  The \p var_ord parameter specifies the
 * (0-based) ordinal position within the resulting instruction's source array
 * at which the variable sources should be placed, allowing them to be inserted
 * in the middle of the fixed sources.
 */
instr_t *
instr_create_Ndst_Msrc_varsrc(dcontext_t *dcontext, int opcode, uint fixed_dsts,
                              uint fixed_srcs, uint var_srcs, uint var_ord, ...);

DR_API
/**
 * Convenience routine that returns an initialized instr_t allocated on the
 * thread-local heap with opcode \p opcode, \p fixed_dsts plus \p var_dsts
 * destination operands, and \p fixed_srcs source operands.  The variable
 * arguments must start with the fixed destinations, followed by the (fixed)
 * sources, followed by the variable destinations.  The \p var_ord parameter
 * specifies the (0-based) ordinal position within the resulting instruction's
 * destination array at which the variable destinations should be placed,
 * allowing them to be inserted in the middle of the fixed destinations.
 */
instr_t *
instr_create_Ndst_Msrc_vardst(dcontext_t *dcontext, int opcode, uint fixed_dsts,
                              uint fixed_srcs, uint var_dsts, uint var_ord, ...);

DR_API
/** Convenience routine that returns an initialized instr_t for OP_popa. */
instr_t *
instr_create_popa(dcontext_t *dcontext);

DR_API
/** Convenience routine that returns an initialized instr_t for OP_pusha. */
instr_t *
instr_create_pusha(dcontext_t *dcontext);

/* build instructions from raw bits */

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 1 byte (byte).
 */
instr_t *
instr_create_raw_1byte(dcontext_t *dcontext, byte byte);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 2 bytes (byte1, byte2).
 */
instr_t *
instr_create_raw_2bytes(dcontext_t *dcontext, byte byte1, byte byte2);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 3 bytes (byte1, byte2, byte3).
 */
instr_t *
instr_create_raw_3bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 4 bytes (byte1, byte2, byte3, byte4).
 */
instr_t *
instr_create_raw_4bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 5 bytes (byte1, byte2, byte3, byte4, byte5).
 */
instr_t *
instr_create_raw_5bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 6 bytes (byte1, byte2, byte3, byte4, byte5, byte6).
 */
instr_t *
instr_create_raw_6bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 7 bytes (byte1, byte2, byte3, byte4, byte5, byte6,
 * byte7).
 */
instr_t *
instr_create_raw_7bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6, byte byte7);

DR_UNS_API
/**
 * Convenience routine that returns an initialized instr_t with invalid operands
 * and allocated raw bits with 7 bytes (byte1, byte2, byte3, byte4, byte5, byte6,
 * byte7, byte8).
 */
instr_t *
instr_create_raw_8bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6, byte byte7, byte byte8);

/* arch-specific routines */
int
instr_length_arch(dcontext_t *dcontext, instr_t *instr);
bool
opc_is_not_a_real_memory_load(int opc);
bool
instr_compute_address_VSIB(instr_t *instr, priv_mcontext_t *mc, size_t mc_size,
                           dr_mcontext_flags_t mc_flags, opnd_t curop, uint index,
                           OUT bool *have_addr, OUT app_pc *addr, OUT bool *write);
uint
instr_branch_type(instr_t *cti_instr);
#ifdef AARCH64
const char *
get_opcode_name(int opc);
#endif
/* these routines can assume that instr's opcode is valid */
bool
instr_is_call_arch(instr_t *instr);
bool
instr_is_cbr_arch(instr_t *instr);
bool
instr_is_mbr_arch(instr_t *instr);
bool
instr_is_ubr_arch(instr_t *instr);

/* private routines for spill code */
instr_t *
instr_create_save_to_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs);
instr_t *
instr_create_save_immed32_to_dcontext(dcontext_t *dcontext, int immed, int offs);
instr_t *
instr_create_save_immed16_to_dcontext(dcontext_t *dcontext, int immed, int offs);
instr_t *
instr_create_save_immed8_to_dcontext(dcontext_t *dcontext, int immed, int offs);
instr_t *
instr_create_save_immed_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, int offs,
                                      ptr_int_t immed, opnd_size_t sz);
instr_t *
instr_create_restore_from_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs);

instr_t *
instr_create_save_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, reg_id_t reg,
                                int offs);
instr_t *
instr_create_restore_from_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, reg_id_t reg,
                                     int offs);

instr_t *
instr_create_jump_via_dcontext(dcontext_t *dcontext, int offs);
instr_t *
instr_create_save_dynamo_stack(dcontext_t *dcontext);
instr_t *
instr_create_restore_dynamo_stack(dcontext_t *dcontext);
bool
instr_raw_is_tls_spill(byte *pc, reg_id_t reg, ushort offs);
bool
instr_is_tls_spill(instr_t *instr, reg_id_t reg, ushort offs);
bool
instr_is_tls_xcx_spill(instr_t *instr);
/* Pass REG_NULL to not care about the reg */
bool
instr_is_tls_restore(instr_t *instr, reg_id_t reg, ushort offs);

DR_API
/**
 * Returns whether \p instr is a register spill or restore, whether it was
 * created by dr_save_reg(), dr_restore_reg(), dr_insert_read_raw_tls(),
 * dr_insert_write_raw_tls(), routines that call the aforementioned routines
 * (e.g., dr_save_arith_flags()), or DR's own internal spills and restores.
 * Returns information about the spill/restore in the OUT parameters.
 * The returned \p offs is the raw offset in bytes from the TLS segment base,
 * the stolen register base, or the thread-private context area.
 */
bool
instr_is_reg_spill_or_restore(void *drcontext, instr_t *instr, bool *tls OUT,
                              bool *spill OUT, reg_id_t *reg OUT, uint *offs OUT);

bool
instr_is_DR_reg_spill_or_restore(void *drcontext, instr_t *instr, bool *tls OUT,
                                 bool *spill OUT, reg_id_t *reg OUT, uint *offs OUT);

#ifdef AARCHXX
bool
instr_reads_thread_register(instr_t *instr);
bool
instr_is_stolen_reg_move(instr_t *instr, bool *save, reg_id_t *reg);
#endif

#ifdef AARCH64
bool
instr_writes_thread_register(instr_t *instr);
#endif

/* N.B. : client meta routines (dr_insert_* etc.) should never use anything other
 * then TLS_XAX_SLOT unless the client has specified a slot to use as we let the
 * client use the rest. */
instr_t *
instr_create_save_to_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs);
instr_t *
instr_create_restore_from_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs);
/* For -x86_to_x64, we can spill to 64-bit extra registers (xref i#751). */
instr_t *
instr_create_save_to_reg(dcontext_t *dcontext, reg_id_t reg1, reg_id_t reg2);
instr_t *
instr_create_restore_from_reg(dcontext_t *dcontext, reg_id_t reg1, reg_id_t reg2);

#ifdef X86_64
byte *
instr_raw_is_rip_rel_lea(byte *pc, byte *read_end);
#endif

/* DR_API EXPORT TOFILE dr_ir_instr.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * EFLAGS/CONDITION CODES
 *
 * The EFLAGS_READ_* and EFLAGS_WRITE_* constants are used by API routines
 * instr_get_eflags(), instr_get_opcode_eflags(), and instr_get_arith_flags().
 */
#ifdef X86
/* we only care about these 11 flags, and mostly only about the first 6
 * we consider an undefined effect on a flag to be a write
 */
#    define EFLAGS_READ_CF 0x00000001  /**< Reads CF (Carry Flag). */
#    define EFLAGS_READ_PF 0x00000002  /**< Reads PF (Parity Flag). */
#    define EFLAGS_READ_AF 0x00000004  /**< Reads AF (Auxiliary Carry Flag). */
#    define EFLAGS_READ_ZF 0x00000008  /**< Reads ZF (Zero Flag). */
#    define EFLAGS_READ_SF 0x00000010  /**< Reads SF (Sign Flag). */
#    define EFLAGS_READ_TF 0x00000020  /**< Reads TF (Trap Flag). */
#    define EFLAGS_READ_IF 0x00000040  /**< Reads IF (Interrupt Enable Flag). */
#    define EFLAGS_READ_DF 0x00000080  /**< Reads DF (Direction Flag). */
#    define EFLAGS_READ_OF 0x00000100  /**< Reads OF (Overflow Flag). */
#    define EFLAGS_READ_NT 0x00000200  /**< Reads NT (Nested Task). */
#    define EFLAGS_READ_RF 0x00000400  /**< Reads RF (Resume Flag). */
#    define EFLAGS_WRITE_CF 0x00000800 /**< Writes CF (Carry Flag). */
#    define EFLAGS_WRITE_PF 0x00001000 /**< Writes PF (Parity Flag). */
#    define EFLAGS_WRITE_AF 0x00002000 /**< Writes AF (Auxiliary Carry Flag). */
#    define EFLAGS_WRITE_ZF 0x00004000 /**< Writes ZF (Zero Flag). */
#    define EFLAGS_WRITE_SF 0x00008000 /**< Writes SF (Sign Flag). */
#    define EFLAGS_WRITE_TF 0x00010000 /**< Writes TF (Trap Flag). */
#    define EFLAGS_WRITE_IF 0x00020000 /**< Writes IF (Interrupt Enable Flag). */
#    define EFLAGS_WRITE_DF 0x00040000 /**< Writes DF (Direction Flag). */
#    define EFLAGS_WRITE_OF 0x00080000 /**< Writes OF (Overflow Flag). */
#    define EFLAGS_WRITE_NT 0x00100000 /**< Writes NT (Nested Task). */
#    define EFLAGS_WRITE_RF 0x00200000 /**< Writes RF (Resume Flag). */

#    define EFLAGS_READ_ALL 0x000007ff           /**< Reads all flags. */
#    define EFLAGS_READ_NON_PRED EFLAGS_READ_ALL /**< Flags not read by predicates. */
#    define EFLAGS_WRITE_ALL 0x003ff800          /**< Writes all flags. */
/* 6 most common flags ("arithmetic flags"): CF, PF, AF, ZF, SF, OF */
/** Reads all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#    define EFLAGS_READ_6 0x0000011f
/** Writes all 6 arithmetic flags (CF, PF, AF, ZF, SF, OF). */
#    define EFLAGS_WRITE_6 0x0008f800

/** Platform-independent macro for reads all arithmetic flags. */
#    define EFLAGS_READ_ARITH EFLAGS_READ_6
/** Platform-independent macor for writes all arithmetic flags. */
#    define EFLAGS_WRITE_ARITH EFLAGS_WRITE_6

/** Converts an EFLAGS_WRITE_* value to the corresponding EFLAGS_READ_* value. */
#    define EFLAGS_WRITE_TO_READ(x) ((x) >> 11)
/** Converts an EFLAGS_READ_* value to the corresponding EFLAGS_WRITE_* value. */
#    define EFLAGS_READ_TO_WRITE(x) ((x) << 11)

/**
 * The actual bits in the eflags register that we care about:\n<pre>
 *   11 10  9  8  7  6  5  4  3  2  1  0
 *   OF DF IF TF SF ZF  0 AF  0 PF  1 CF  </pre>
 */
enum {
    EFLAGS_CF = 0x00000001, /**< The bit in the eflags register of CF (Carry Flag). */
    EFLAGS_PF = 0x00000004, /**< The bit in the eflags register of PF (Parity Flag). */
    EFLAGS_AF = 0x00000010, /**< The bit in the eflags register of AF (Aux Carry Flag). */
    EFLAGS_ZF = 0x00000040, /**< The bit in the eflags register of ZF (Zero Flag). */
    EFLAGS_SF = 0x00000080, /**< The bit in the eflags register of SF (Sign Flag). */
    EFLAGS_DF = 0x00000400, /**< The bit in the eflags register of DF (Direction Flag). */
    EFLAGS_OF = 0x00000800, /**< The bit in the eflags register of OF (Overflow Flag). */
    /** The bits in the eflags register of CF, PF, AF, ZF, SF, OF. */
    EFLAGS_ARITH = EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_OF,
};

#elif defined(AARCHXX)
#    define EFLAGS_READ_N 0x00000001  /**< Reads N (negative flag). */
#    define EFLAGS_READ_Z 0x00000002  /**< Reads Z (zero flag). */
#    define EFLAGS_READ_C 0x00000004  /**< Reads C (carry flag). */
#    define EFLAGS_READ_V 0x00000008  /**< Reads V (overflow flag). */
#    define EFLAGS_READ_Q 0x00000010  /**< Reads Q (saturation flag). */
#    define EFLAGS_READ_GE 0x00000020 /**< Reads GE (>= for parallel arithmetic). */
#    define EFLAGS_READ_NZCV \
        (EFLAGS_READ_N | EFLAGS_READ_Z | EFLAGS_READ_C | EFLAGS_READ_V)
/** Platform-independent macro for reads all arithmetic flags. */
#    define EFLAGS_READ_ARITH (EFLAGS_READ_NZCV | EFLAGS_READ_Q | EFLAGS_READ_GE)
#    define EFLAGS_READ_ALL (EFLAGS_READ_ARITH) /**< Reads all flags. */
#    define EFLAGS_READ_NON_PRED                                         \
        EFLAGS_READ_GE                /**< Flags not read by predicates. \
                                       */
#    define EFLAGS_WRITE_N 0x00000040 /**< Reads N (negative). */
#    define EFLAGS_WRITE_Z 0x00000080 /**< Reads Z (zero). */
#    define EFLAGS_WRITE_C 0x00000100 /**< Reads C (carry). */
#    define EFLAGS_WRITE_V 0x00000200 /**< Reads V (overflow). */
#    define EFLAGS_WRITE_Q 0x00000400 /**< Reads Q (saturation). */
#    define EFLAGS_WRITE_GE                                    \
        0x00000800 /**< Reads GE (>= for parallel arithmetic). \
                    */
#    define EFLAGS_WRITE_NZCV \
        (EFLAGS_WRITE_N | EFLAGS_WRITE_Z | EFLAGS_WRITE_C | EFLAGS_WRITE_V)
/** Platform-independent macro for writes all arithmetic flags. */
#    define EFLAGS_WRITE_ARITH (EFLAGS_WRITE_NZCV | EFLAGS_WRITE_Q | EFLAGS_WRITE_GE)
#    define EFLAGS_WRITE_ALL (EFLAGS_WRITE_ARITH) /**< Writes all flags. */

/** Converts an EFLAGS_WRITE_* value to the corresponding EFLAGS_READ_* value. */
#    define EFLAGS_WRITE_TO_READ(x) ((x) >> 6)
/** Converts an EFLAGS_READ_* value to the corresponding EFLAGS_WRITE_* value. */
#    define EFLAGS_READ_TO_WRITE(x) ((x) << 6)

/**
 * The actual bits in the CPSR that we care about:\n<pre>
 *   31 30 29 28 27 ... 19 18 17 16 ... 5
 *    N  Z  C  V  Q       GE[3:0]       T </pre>
 */
enum {
    EFLAGS_N = 0x80000000,  /**< The bit in the CPSR register of N (negative flag). */
    EFLAGS_Z = 0x40000000,  /**< The bit in the CPSR register of Z (zero flag). */
    EFLAGS_C = 0x20000000,  /**< The bit in the CPSR register of C (carry flag). */
    EFLAGS_V = 0x10000000,  /**< The bit in the CPSR register of V (overflow flag). */
    EFLAGS_Q = 0x08000000,  /**< The bit in the CPSR register of Q (saturation flag). */
    EFLAGS_GE = 0x000f0000, /**< The bits in the CPSR register of GE[3:0]. */
    /** The bits in the CPSR register of N, Z, C, V, Q, and GE. */
    EFLAGS_ARITH = EFLAGS_N | EFLAGS_Z | EFLAGS_C | EFLAGS_V | EFLAGS_Q | EFLAGS_GE,
    /**
     * The bit in the CPSR register of T (Thumb mode indicator bit).  This is
     * not readable from user space and should only be examined when looking at
     * machine state from the kernel, such as in a signal handler.
     */
    EFLAGS_T = 0x00000020,
    /**
     * The bits in the CPSR register of the T32 IT block base condition.
     * This is not readable from user space and should only be examined when
     * looking at machine state from the kernel, such as in a signal handler.
     */
    EFLAGS_IT_COND = 0x0000e000,
    /**
     * The bits in the CPSR register of the T32 IT block size.
     * This is not readable from user space and should only be examined when
     * looking at machine state from the kernel, such as in a signal handler.
     */
    EFLAGS_IT_SIZE = 0x06001c00,
};

/** The bits in the CPSR register of the T32 IT block state. */
#    define EFLAGS_IT (EFLAGS_IT_COND | EFLAGS_IT_SIZE)

/** The bit in the 4-bit OP_msr immediate that selects the nzcvq status flags. */
#    define EFLAGS_MSR_NZCVQ 0x8
/** The bit in the 4-bit OP_msr immediate that selects the apsr_g status flags. */
#    define EFLAGS_MSR_G 0x4
/** The bits in the 4-bit OP_msr immediate that select the nzcvqg status flags. */
#    define EFLAGS_MSR_NZCVQG (EFLAGS_MSR_NZCVQ | EFLAGS_MSR_G)
#endif
/* DR_API EXPORT END */

/* even on x64, displacements are 32 bits, so we keep the "int" type and 4-byte size
 */
#define PC_RELATIVE_TARGET(addr) (*((int *)(addr)) + (addr) + 4)

/* length of our mangling of jecxz/loop*, beyond a possible addr prefix byte */
#ifdef X86
#    define CTI_SHORT_REWRITE_LENGTH 9
#else
/* cbz/cbnz + b */
#    define CTI_SHORT_REWRITE_LENGTH 6
#    define CTI_SHORT_REWRITE_B_OFFS 2
#endif

#include "instr_inline.h"

#endif /* _INSTR_H_ */
