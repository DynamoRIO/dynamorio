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

/* The machine-specific IR consists of instruction lists, instructions,
 * operands, and opcodes.  You can find related declarations and interface
 * functions in corresponding headers.
 */
/* file "instr.h" -- instr_t specific definitions and utilities */

#ifndef _INSTR_H_
#define _INSTR_H_ 1

#include "opcode_api.h"
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

#include "instr_api.h" /* After the INSTR_INLINE, etc. above. */

/* can't include decode.h, it includes us, just declare struct */
struct instr_info_t;

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

/* flags */
enum {
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
#ifdef X64
    /* PR 257963: since we don't store targets of ind branches, we need a flag
     * so we know whether this is a trace cmp exit, which has its own ibl entry
     */
    INSTR_TRACE_CMP_EXIT = LINK_TRACE_CMP,
#endif
#ifdef WINDOWS
    INSTR_CALLBACK_RETURN = LINK_CALLBACK_RETURN,
#else
    INSTR_NI_SYSCALL_INT = LINK_NI_SYSCALL_INT,
#endif
    INSTR_NI_SYSCALL = LINK_NI_SYSCALL,
    INSTR_NI_SYSCALL_ALL = LINK_NI_SYSCALL_ALL,
    /* meta-flag */
    EXIT_CTI_TYPES = (INSTR_DIRECT_EXIT | INSTR_INDIRECT_EXIT | INSTR_RETURN_EXIT |
                      INSTR_CALL_EXIT | INSTR_JMP_EXIT | INSTR_FAR_EXIT |
                      INSTR_BRANCH_SPECIAL_EXIT | INSTR_BRANCH_PADDED |
#ifdef X64
                      INSTR_TRACE_CMP_EXIT |
#endif
#ifdef WINDOWS
                      INSTR_CALLBACK_RETURN |
#else
                      INSTR_NI_SYSCALL_INT |
#endif
                      INSTR_NI_SYSCALL),

    /* instr_t-internal flags (not shared with LINK_) */
    INSTR_OPERANDS_VALID = 0x00010000,
    /* meta-flag */
    INSTR_FIRST_NON_LINK_SHARED_FLAG = INSTR_OPERANDS_VALID,
    INSTR_EFLAGS_VALID = 0x00020000,
    INSTR_EFLAGS_6_VALID = 0x00040000,
    INSTR_RAW_BITS_VALID = 0x00080000,
    INSTR_RAW_BITS_ALLOCATED = 0x00100000,
    /* This flag is in instr_api.h as it's needed for inlining support.
     *   INSTR_DO_NOT_MANGLE = 0x00200000,
     */
    /* This flag is set by instr_noalloc_init() and used to identify the
     * instr_noalloc_t "subclass" of instr_t.  It should not be otherwise used.
     */
    INSTR_IS_NOALLOC_STRUCT = 0x00400000,
    /* used to indicate that an indirect call can be treated as a direct call */
    INSTR_IND_CALL_DIRECT = 0x00800000,
#ifdef WINDOWS
    /* used to indicate that a syscall should be executed via shared syscall */
    INSTR_SHARED_SYSCALL = 0x01000000,
#else
    /* Indicates an instruction that's part of the rseq endpoint.  We use this in
     * instrlist_t.flags (sort of the same namespace: INSTR_OUR_MANGLING is used there,
     * but also EDI_VAL_*); we no longer use it on individual instructions since
     * the label note field DR_NOTE_RSEQ now survives encoding.
     */
    INSTR_RSEQ_ENDPOINT = 0x01000000,
#endif

    /* This enum is also used for INSTR_OUR_MANGLING_EPILOGUE. Its semantics are
     * orthogonal to this and must not overlap.
     */
    INSTR_CLOBBER_RETADDR = 0x02000000,

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
#ifdef DEBUG
    /* case 9151: only report invalid instrs for normal code decoding */
    INSTR_IGNORE_INVALID = 0x08000000,
#endif
    /* Currently used for frozen coarse fragments with final jmps and
     * jmps to ib stubs that are elided: we need the jmp instr there
     * to build the linkstub_t but we do not want to emit it. */
    INSTR_DO_NOT_EMIT = 0x10000000,
    /* PR 251479: re-relativization support: is instr->rip_rel_pos valid? */
    INSTR_RIP_REL_VALID = 0x20000000,
#ifdef X86
    /* PR 278329: each instr stores its own mode */
    INSTR_X86_MODE = 0x40000000,
#elif defined(ARM)
    /* We assume we don't need to distinguish A64 from A32 as you cannot swap
     * between them in user mode.  Thus we only need one flag.
     * XXX: we might want more power for drdecode, though the global isa_mode
     * should be sufficient there.
     */
    INSTR_THUMB_MODE = 0x40000000,
#endif
    /* PR 267260: distinguish our own mangling from client-added instrs */
    INSTR_OUR_MANGLING = 0x80000000,
};

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
    (((1U << PREFIX_PRED_BITS) - 1) << PREFIX_PRED_BITPOS) /*0xf8000000 */

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

/* not exported */
void
instr_shift_raw_bits(instr_t *instr, ssize_t offs);
int
instr_exit_branch_type(instr_t *instr);
void
instr_exit_branch_set_type(instr_t *instr, uint type);

const struct instr_info_t *
instr_get_instr_info(instr_t *instr);

const struct instr_info_t *
get_instr_info(int opcode);

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

/*
 ******************************************************************/

bool
instr_predicate_reads_srcs(dr_pred_type_t pred);

bool
instr_predicate_writes_eflags(dr_pred_type_t pred);

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

/***********************************************************************/
/* utility functions */

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

app_pc
instr_compute_address_priv(instr_t *instr, priv_mcontext_t *mc);

bool
instr_compute_address_ex_priv(instr_t *instr, priv_mcontext_t *mc, uint index,
                              OUT app_pc *addr, OUT bool *write, OUT uint *pos);

/* Get a label instructions callback function */
instr_label_callback_t
instr_get_label_callback(instr_t *instr);

byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target);

bool
instr_saves_float_pc(instr_t *instr);

#ifdef AARCH64
bool
instr_is_icache_op(instr_t *instr);
#endif

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
 * not set, it checks the destinations of \p instr. The decoded instr should be
 * available for inspection as this is invoked only when some client is present.
 */
bool
instr_may_write_zmm_or_opmask_register(instr_t *instr);
#endif

/* Given a machine state, returns whether or not the cbr instr would be taken
 * if the state is before execution (pre == true) or after (pre == false).
 * (not exported since machine state isn't)
 */
bool
instr_cbr_taken(instr_t *instr, priv_mcontext_t *mcontext, bool pre);

/* utility routines that are in optimize.c */
opnd_t
instr_get_src_mem_access(instr_t *instr);

void
d_r_loginst(dcontext_t *dcontext, uint level, instr_t *instr, const char *string);

void
d_r_logopnd(dcontext_t *dcontext, uint level, opnd_t opnd, const char *string);

DR_UNS_API
/**
 * Convenience routine to create a nop of a certain size.  If \p raw
 * is true, sets raw bytes rather than filling in the operands or opcode.
 */
instr_t *
instr_create_nbyte_nop(dcontext_t *dcontext, uint num_bytes, bool raw);

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

#ifdef X86
enum {
    RAW_OPCODE_nop = 0x90,
    RAW_OPCODE_jmp_short = 0xeb,
    RAW_OPCODE_call = 0xe8,
    RAW_OPCODE_ret = 0xc3,
    RAW_OPCODE_jmp = 0xe9,
    RAW_OPCODE_push_imm32 = 0x68,
    RAW_OPCODE_pop_eax = 0x58,
    RAW_OPCODE_jcc_short_start = 0x70,
    RAW_OPCODE_jne_short = 0x75,
    RAW_OPCODE_jcc_short_end = 0x7f,
    RAW_OPCODE_jcc_byte1 = 0x0f,
    RAW_OPCODE_jcc_byte2_start = 0x80,
    RAW_OPCODE_jcc_byte2_end = 0x8f,
    RAW_OPCODE_loop_start = 0xe0,
    RAW_OPCODE_loop_end = 0xe3,
    RAW_OPCODE_lea = 0x8d,
    RAW_OPCODE_SIGILL = 0x0b0f,
    RAW_PREFIX_jcc_not_taken = 0x2e,
    RAW_PREFIX_jcc_taken = 0x3e,
    RAW_PREFIX_lock = 0xf0,
    RAW_PREFIX_xacquire = 0xf2,
    RAW_PREFIX_xrelease = 0xf3,
};

enum { /* FIXME: vs RAW_OPCODE_* enum */
       CS_SEG_OPCODE = RAW_PREFIX_jcc_not_taken,
       DS_SEG_OPCODE = RAW_PREFIX_jcc_taken,
       ES_SEG_OPCODE = 0x26,
       FS_SEG_OPCODE = 0x64,
       GS_SEG_OPCODE = 0x65,
       SS_SEG_OPCODE = 0x36,

/* For Windows, we piggyback on native TLS via gs for x64 and fs for x86.
 * For Linux, we steal a segment register, and so use fs for x86 (where
 * pthreads uses gs) and gs for x64 (where pthreads uses fs) (presumably
 * to avoid conflicts w/ wine).
 */
#    ifdef X64
       TLS_SEG_OPCODE = GS_SEG_OPCODE,
#    else
       TLS_SEG_OPCODE = FS_SEG_OPCODE,
#    endif

       DATA_PREFIX_OPCODE = 0x66,
       ADDR_PREFIX_OPCODE = 0x67,
       REPNE_PREFIX_OPCODE = 0xf2,
       REP_PREFIX_OPCODE = 0xf3,
       REX_PREFIX_BASE_OPCODE = 0x40,
       REX_PREFIX_W_OPFLAG = 0x8,
       REX_PREFIX_R_OPFLAG = 0x4,
       REX_PREFIX_X_OPFLAG = 0x2,
       REX_PREFIX_B_OPFLAG = 0x1,
       REX_PREFIX_ALL_OPFLAGS = 0xf,
       MOV_REG2MEM_OPCODE = 0x89,
       MOV_MEM2REG_OPCODE = 0x8b,
       MOV_XAX2MEM_OPCODE = 0xa3, /* no ModRm */
       MOV_MEM2XAX_OPCODE = 0xa1, /* no ModRm */
       MOV_IMM2XAX_OPCODE = 0xb8, /* no ModRm */
       MOV_IMM2XBX_OPCODE = 0xbb, /* no ModRm */
       MOV_IMM2MEM_OPCODE = 0xc7, /* has ModRm */
       JECXZ_OPCODE = 0xe3,
       JMP_SHORT_OPCODE = 0xeb,
       JMP_OPCODE = 0xe9,
       JNE_OPCODE_1 = 0x0f,
       SAHF_OPCODE = 0x9e,
       LAHF_OPCODE = 0x9f,
       SETO_OPCODE_1 = 0x0f,
       SETO_OPCODE_2 = 0x90,
       ADD_AL_OPCODE = 0x04,
       INC_MEM32_OPCODE_1 = 0xff, /* has /0 as well */
       MODRM16_DISP16 = 0x06,     /* see vol.2 Table 2-1 for modR/M */
       SIB_DISP32 = 0x25,         /* see vol.2 Table 2-1 for modR/M */
       RET_NOIMM_OPCODE = 0xc3,
       RET_IMM_OPCODE = 0xc2,
       MOV_IMM_EDX_OPCODE = 0xba,
       VEX_2BYTE_PREFIX_OPCODE = 0xc5,
       VEX_3BYTE_PREFIX_OPCODE = 0xc4,
       EVEX_PREFIX_OPCODE = 0x62,
};
#elif defined(ARM)
enum {
    CBZ_BYTE_A = 0xb1,  /* this assumes the top bit of the disp is 0 */
    CBNZ_BYTE_A = 0xb9, /* this assumes the top bit of the disp is 0 */
};
#elif defined(AARCH64)
enum {
    RAW_NOP_INST = 0xd503201f,
};
#endif

#include "instr_inline_api.h"

#endif /* _INSTR_H_ */
