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

/* file "instr_shared.c" -- IR instr_t utilities */

/* We need to provide at least one out-of-line definition for our inline
 * functions in instr_inline.h in case they are all inlined away within DR.
 *
 * For gcc, we use -std=gnu99, which uses the C99 inlining model.  Using "extern
 * inline" will provide a definition, but we can only do this in one C file.
 * Elsewhere we use plain "inline", which will not emit an out of line
 * definition if inlining fails.
 *
 * MSVC always emits link_once definitions for dllexported inline functions, so
 * this macro magic is unnecessary.
 * http://msdn.microsoft.com/en-us/library/xa0d9ste.aspx
 */
#define INSTR_INLINE extern inline

#include "../globals.h"
#include "instr.h"
#include "arch.h"
#include "../link.h"
#include "decode.h"
#include "decode_fast.h"
#include "instr_create_shared.h"
/* FIXME i#1551: refactor this file and avoid this x86-specific include in base arch/ */
#include "x86/decode_private.h"

#ifdef DEBUG
#    include "disassemble.h"
#endif

#ifdef VMX86_SERVER
#    include "vmkuw.h" /* VMKUW_SYSCALL_GATEWAY */
#endif

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* returns an empty instr_t object */
instr_t *
instr_create(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *instr = (instr_t *)heap_alloc(dcontext, sizeof(instr_t) HEAPACCT(ACCT_IR));
    /* everything initializes to 0, even flags, to indicate
     * an uninitialized instruction */
    memset((void *)instr, 0, sizeof(instr_t));
#if defined(X86) && defined(X64)
    instr_set_isa_mode(instr, X64_CACHE_MODE_DC(dcontext) ? DR_ISA_AMD64 : DR_ISA_IA32);
#elif defined(ARM)
    instr_set_isa_mode(instr, dr_get_isa_mode(dcontext));
#endif
    return instr;
}

/* deletes the instr_t object with handle "instr" and frees its storage */
void
instr_destroy(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
#ifdef ARM
    /* i#4680: Reset encode state to avoid dangling pointers.  This doesn't cover
     * auto-scope instr_t vars so the whole IT tracking is still fragile.
     */
    if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB)
        encode_instr_freed_event(dcontext, instr);
#endif
    instr_free(dcontext, instr);

    /* CAUTION: assumes that instr is not part of any instrlist */
    heap_free(dcontext, instr, sizeof(instr_t) HEAPACCT(ACCT_IR));
}

/* returns a clone of orig, but with next and prev fields set to NULL */
instr_t *
instr_clone(void *drcontext, instr_t *orig)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    /* We could heap-allocate an instr_noalloc_t but it's intended for use in a
     * signal handler or other places where we don't want any heap allocation.
     */
    CLIENT_ASSERT(!TEST(INSTR_IS_NOALLOC_STRUCT, orig->flags),
                  "Cloning an instr_noalloc_t is not supported.");

    instr_t *instr = (instr_t *)heap_alloc(dcontext, sizeof(instr_t) HEAPACCT(ACCT_IR));
    memcpy((void *)instr, (void *)orig, sizeof(instr_t));
    instr->next = NULL;
    instr->prev = NULL;

    /* PR 214962: clients can see some of our mangling
     * (dr_insert_mbr_instrumentation(), traces), but don't let the flag
     * mark other client instrs, which could mess up state translation
     */
    instr_set_our_mangling(instr, false);

    if ((orig->flags & INSTR_RAW_BITS_ALLOCATED) != 0) {
        /* instr length already set from memcpy */
        instr->bytes =
            (byte *)heap_reachable_alloc(dcontext, instr->length HEAPACCT(ACCT_IR));
        memcpy((void *)instr->bytes, (void *)orig->bytes, instr->length);
    } else if (instr_is_label(orig) && instr_get_label_callback(instr) != NULL) {
        /* We don't know what this callback does, we can't copy this. The caller that
         * makes the clone needs to take care of this, xref i#3926.
         */
        instr_clear_label_callback(instr);
    }
    if (orig->num_dsts > 0) { /* checking num_dsts, not dsts, b/c of label data */
        instr->dsts = (opnd_t *)heap_alloc(
            dcontext, instr->num_dsts * sizeof(opnd_t) HEAPACCT(ACCT_IR));
        memcpy((void *)instr->dsts, (void *)orig->dsts, instr->num_dsts * sizeof(opnd_t));
    }
    if (orig->num_srcs > 1) { /* checking num_src, not srcs, b/c of label data */
        instr->srcs = (opnd_t *)heap_alloc(
            dcontext, (instr->num_srcs - 1) * sizeof(opnd_t) HEAPACCT(ACCT_IR));
        memcpy((void *)instr->srcs, (void *)orig->srcs,
               (instr->num_srcs - 1) * sizeof(opnd_t));
    }
    /* copy note (we make no guarantee, and have no way, to do a deep clone) */
    instr->note = orig->note;
    instr->offset = orig->offset;
    if (instr_is_label(orig))
        memcpy(&instr->label_data, &orig->label_data, sizeof(instr->label_data));
    return instr;
}

/* zeroes out the fields of instr */
void
instr_init(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    /* everything initializes to 0, even flags, to indicate
     * an uninitialized instruction */
    memset((void *)instr, 0, sizeof(instr_t));
    instr_set_isa_mode(instr, dr_get_isa_mode(dcontext));
#ifndef NOT_DYNAMORIO_CORE_PROPER
    /* Just like in global_heap_alloc() we pay the cost of this check to support
     * drdecode use even with full DR linked in (i#2499).  Decoding of simple
     * single-source-no-dest instrs never hits the heap code so we check here too.
     */
    if (dcontext == GLOBAL_DCONTEXT && !dynamo_heap_initialized) {
        /* TODO i#2499: We have no control point currently to call standalone_exit().
         * We need to develop a solution with atexit() or ELF destructors or sthg.
         */
        standalone_init();
    }
#endif
}

/* zeroes out the fields of instr */
void
instr_noalloc_init(void *drcontext, instr_noalloc_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    memset(instr, 0, sizeof(*instr));
    instr->instr.flags |= INSTR_IS_NOALLOC_STRUCT;
    instr_set_isa_mode(&instr->instr, dr_get_isa_mode(dcontext));
}

/* Frees all dynamically allocated storage that was allocated by instr */
void
instr_free(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (instr_is_label(instr) && instr_get_label_callback(instr) != NULL)
        (*instr->label_cb)(dcontext, instr);
    if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags))
        return;
    if (TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags)) {
        instr_free_raw_bits(dcontext, instr);
    }
    if (instr->num_dsts > 0) { /* checking num_dsts, not dsts, b/c of label data */
        heap_free(dcontext, instr->dsts,
                  instr->num_dsts * sizeof(opnd_t) HEAPACCT(ACCT_IR));
        instr->dsts = NULL;
        instr->num_dsts = 0;
    }
    if (instr->num_srcs > 1) { /* checking num_src, not src, b/c of label data */
        /* remember one src is static, rest are dynamic */
        heap_free(dcontext, instr->srcs,
                  (instr->num_srcs - 1) * sizeof(opnd_t) HEAPACCT(ACCT_IR));
        instr->srcs = NULL;
        instr->num_srcs = 0;
    }
}

int
instr_mem_usage(instr_t *instr)
{
    if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags))
        return sizeof(instr_noalloc_t);
    int usage = 0;
    if ((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0) {
        usage += instr->length;
    }
    if (instr->dsts != NULL) {
        usage += instr->num_dsts * sizeof(opnd_t);
    }
    if (instr->srcs != NULL) {
        /* remember one src is static, rest are dynamic */
        usage += (instr->num_srcs - 1) * sizeof(opnd_t);
    }
    usage += sizeof(instr_t);
    return usage;
}

/* Frees all dynamically allocated storage that was allocated by instr
 * Also zeroes out instr's fields
 * This instr must have been initialized before!
 */
void
instr_reset(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_free(dcontext, instr);
    if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags)) {
        instr_init(dcontext, instr);
        instr->flags |= INSTR_IS_NOALLOC_STRUCT;
    } else {
        instr_init(dcontext, instr);
    }
}

/* Frees all dynamically allocated storage that was allocated by instr,
 * except for allocated raw bits.
 * Also zeroes out instr's fields, except for raw bit fields and next and prev
 * fields, whether instr is ok to mangle, and instr's x86 mode.
 * Use this routine when you want to decode more information into the
 * same instr_t structure.
 * This instr must have been initialized before!
 */
void
instr_reuse(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte *bits = NULL;
    uint len = 0;
    bool alloc = false;
    bool mangle = instr_is_app(instr);
    dr_isa_mode_t isa_mode = instr_get_isa_mode(instr);
#ifdef X86
    uint rip_rel_pos = instr_rip_rel_valid(instr) ? instr->rip_rel_pos : 0;
#endif
    instr_t *next = instr->next;
    instr_t *prev = instr->prev;
    if (instr_raw_bits_valid(instr)) {
        if (instr_has_allocated_bits(instr)) {
            /* pretend has no allocated bits to prevent freeing of them */
            instr->flags &= ~INSTR_RAW_BITS_ALLOCATED;
            alloc = true;
        }
        bits = instr->bytes;
        len = instr->length;
    }
    instr_free(dcontext, instr);
    instr_init(dcontext, instr);
    /* now re-add them */
    instr->next = next;
    instr->prev = prev;
    if (bits != NULL) {
        instr->bytes = bits;
        instr->length = len;
        /* assume that the bits are now valid and the operands are not
         * (operand and eflags flags are already unset from init)
         */
        instr->flags |= INSTR_RAW_BITS_VALID;
        if (alloc)
            instr->flags |= INSTR_RAW_BITS_ALLOCATED;
    }
    /* preserve across the up-decode */
    instr_set_isa_mode(instr, isa_mode);
#ifdef X86
    if (rip_rel_pos > 0)
        instr_set_rip_rel_pos(instr, rip_rel_pos);
#endif
    if (!mangle)
        instr->flags |= INSTR_DO_NOT_MANGLE;
}

instr_t *
instr_build(void *drcontext, int opcode, int instr_num_dsts, int instr_num_srcs)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *instr = instr_create(dcontext);
    instr_set_opcode(instr, opcode);
    instr_set_num_opnds(dcontext, instr, instr_num_dsts, instr_num_srcs);
    return instr;
}

instr_t *
instr_build_bits(void *drcontext, int opcode, uint num_bytes)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *instr = instr_create(dcontext);
    instr_set_opcode(instr, opcode);
    instr_allocate_raw_bits(dcontext, instr, num_bytes);
    return instr;
}

/* encodes to buffer, then returns length.
 * needed for things we must have encoding for: length and eflags.
 * if !always_cache, only caches the encoding if instr_is_app();
 * if always_cache, the caller should invalidate the cache when done.
 */
static int
private_instr_encode(dcontext_t *dcontext, instr_t *instr, bool always_cache)
{
    byte *buf;
    byte stack_buf[MAX_INSTR_LENGTH];
    if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags)) {
        /* We have no choice: we live with no persistent caching if the stack is
         * too far away, because the instr's raw bits will be on the stack.
         * (We can't use encode_buf here bc the re-rel below does not support
         * the same buffer; maybe it could w/ a memmove in the encode code?)
         */
        buf = stack_buf;
    } else {
        /* We cannot efficiently use a stack buffer for encoding since our stack on x64
         * linux can be too far to reach from our heap.  We need reachable heap.
         * Otherwise we can't keep the encoding around since re-relativization won't
         * work.
         */
        buf = heap_reachable_alloc(dcontext, MAX_INSTR_LENGTH HEAPACCT(ACCT_IR));
    }
    uint len;
    /* Do not cache instr opnds as they are pc-relative to final encoding location.
     * Rather than us walking all of the operands separately here, we have
     * instr_encode_check_reachability tell us while it does its normal walk.
     * Xref i#731.
     */
    bool has_instr_opnds;
    byte *nxt = instr_encode_check_reachability(dcontext, instr, buf, &has_instr_opnds);
    bool valid_to_cache = !has_instr_opnds;
    if (nxt == NULL) {
        nxt = instr_encode_ignore_reachability(dcontext, instr, buf);
        if (nxt == NULL) {
#ifdef AARCH64
            /* We do not use instr_info_t encoding info on AArch64. FIXME i#1569 */
            SYSLOG_INTERNAL_WARNING("cannot encode %s",
                                    get_opcode_name(instr_get_opcode(instr)));
#else
            SYSLOG_INTERNAL_WARNING("cannot encode %s",
                                    opcode_to_encoding_info(instr->opcode,
                                                            instr_get_isa_mode(instr)
                                                                _IF_ARM(false))
                                        ->name);
#endif
            if (!TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags))
                heap_reachable_free(dcontext, buf, MAX_INSTR_LENGTH HEAPACCT(ACCT_IR));
            return 0;
        }
        /* if unreachable, we can't cache, since re-relativization won't work */
        valid_to_cache = false;
    }
    len = (int)(nxt - buf);
    CLIENT_ASSERT(len > 0 || instr_is_label(instr),
                  "encode instr for length/eflags error: zero length");
    CLIENT_ASSERT(len <= MAX_INSTR_LENGTH,
                  "encode instr for length/eflags error: instr too long");

    /* do not cache encoding if mangle is false, that way we can have
     * non-cti-instructions that are pc-relative.
     * we also cannot cache if a rip-relative operand is unreachable.
     * we can cache if a rip-relative operand is present b/c instr_encode()
     * sets instr_set_rip_rel_pos() for us.
     */
    if (len > 0 &&
        ((valid_to_cache && instr_is_app(instr)) ||
         always_cache /*caller will use then invalidate*/)) {
        bool valid = instr_operands_valid(instr);
#ifdef X86
        /* we can't call instr_rip_rel_valid() b/c the raw bytes are not yet
         * set up: we rely on instr_encode() setting instr->rip_rel_pos and
         * the valid flag, even though raw bytes weren't there at the time.
         * we rely on the INSTR_RIP_REL_VALID flag being invalidated whenever
         * the raw bits are.
         */
        bool rip_rel_valid = TEST(INSTR_RIP_REL_VALID, instr->flags);
#endif
        byte *tmp;
        CLIENT_ASSERT(!instr_raw_bits_valid(instr),
                      "encode instr: bit validity error"); /* else shouldn't get here */
        instr_allocate_raw_bits(dcontext, instr, len);
        /* we use a hack in order to take advantage of
         * copy_and_re_relativize_raw_instr(), which copies from instr->bytes
         * using rip-rel-calculating routines that also use instr->bytes.
         */
        tmp = instr->bytes;
        instr->bytes = buf;
#ifdef X86
        instr_set_rip_rel_valid(instr, rip_rel_valid);
#endif
        copy_and_re_relativize_raw_instr(dcontext, instr, tmp, tmp);
        instr->bytes = tmp;
        instr_set_operands_valid(instr, valid);
    }
    if (!TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags))
        heap_reachable_free(dcontext, buf, MAX_INSTR_LENGTH HEAPACCT(ACCT_IR));
    return len;
}

#define inlined_instr_get_opcode(instr)                                           \
    (IF_DEBUG_(CLIENT_ASSERT(sizeof(*instr) == sizeof(instr_t), "invalid type"))( \
        ((instr)->opcode == OP_UNDECODED)                                         \
            ? (instr_decode_with_current_dcontext(instr), (instr)->opcode)        \
            : (instr)->opcode))
int
instr_get_opcode(instr_t *instr)
{
    return inlined_instr_get_opcode(instr);
}
/* in rest of file, directly de-reference for performance (PR 622253) */
#define instr_get_opcode inlined_instr_get_opcode

/* XXX i#6238: This API is not yet supported for synthetic instructions */
#define inlined_instr_get_category(instr)                                         \
    (IF_DEBUG_(CLIENT_ASSERT(sizeof(*instr) == sizeof(instr_t), "invalid type"))( \
        ((instr)->category == DR_INSTR_CATEGORY_UNCATEGORIZED ||                  \
         !instr_operands_valid(instr))                                            \
            ? (instr_decode_with_current_dcontext(instr), (instr)->category)      \
            : (instr)->category))
uint
instr_get_category(instr_t *instr)
{
    return inlined_instr_get_category(instr);
}
/* in rest of file, directly de-reference for performance (PR 622253) */
#define instr_get_category inlined_instr_get_category

static inline void
instr_being_modified(instr_t *instr, bool raw_bits_valid)
{
    if (!raw_bits_valid) {
        /* if we're modifying the instr, don't use original bits to encode! */
        instr_set_raw_bits_valid(instr, false);
    }
    /* PR 214962: if client changes our mangling, un-mark to avoid bad translation */
    instr_set_our_mangling(instr, false);
}

void
instr_set_category(instr_t *instr, uint category)
{
    instr->category = category;
}

void
instr_set_opcode(instr_t *instr, int opcode)
{
    instr->opcode = opcode;
    /* if we're modifying opcode, don't use original bits to encode! */
    instr_being_modified(instr, false /*raw bits invalid*/);
    /* do not assume operands are valid, they are separate from opcode,
     * but if opcode is invalid operands shouldn't be valid
     */
    CLIENT_ASSERT((opcode != OP_INVALID && opcode != OP_UNDECODED) ||
                      !instr_operands_valid(instr),
                  "instr_set_opcode: operand-opcode validity mismatch");
}

/* Returns true iff instr's opcode is NOT OP_INVALID.
 * Not to be confused with an invalid opcode, which can be OP_INVALID or
 * OP_UNDECODED.  OP_INVALID means an instruction with no valid fields:
 * raw bits (may exist but do not correspond to a valid instr), opcode,
 * eflags, or operands.  It could be an uninitialized
 * instruction or the result of decoding an invalid sequence of bytes.
 */
bool
instr_valid(instr_t *instr)
{
    return (instr->opcode != OP_INVALID);
}

DR_API
/* Get the original application PC of the instruction if it exists. */
app_pc
instr_get_app_pc(instr_t *instr)
{
    return instr_get_translation(instr);
}

DR_API
size_t
instr_get_offset(instr_t *instr)
{
    return instr->offset;
}

/* Returns true iff instr's opcode is valid.  If the opcode is not
 * OP_INVALID or OP_UNDECODED it is assumed to be valid.  However, calling
 * instr_get_opcode() will attempt to decode an OP_UNDECODED opcode, hence the
 * purpose of this routine.
 */
bool
instr_opcode_valid(instr_t *instr)
{
    return (instr->opcode != OP_INVALID && instr->opcode != OP_UNDECODED);
}

const instr_info_t *
instr_get_instr_info(instr_t *instr)
{
    dr_isa_mode_t isa_mode;
#ifdef ARM
    bool in_it_block = false;
#endif
    if (instr == NULL)
        return NULL;
    isa_mode = instr_get_isa_mode(instr);

#ifdef ARM
    if (isa_mode == DR_ISA_ARM_THUMB) {
        /* A predicated OP_b_short could be either in an IT block or not,
         * we assume it is not in an IT block in the case of OP_b_short.
         */
        if (instr_get_opcode(instr) != OP_b_short &&
            instr_get_predicate(instr) != DR_PRED_NONE)
            in_it_block = true;
    }
#endif
    return opcode_to_encoding_info(instr_get_opcode(instr),
                                   isa_mode _IF_ARM(in_it_block));
}

const instr_info_t *
get_instr_info(int opcode)
{
    /* Assuming the use case of this function is to get the opcode related info,
     *e.g., eflags in instr_get_opcode_eflags for OP_adds vs OP_add, so it does
     * not matter whether it is in an IT block or not.
     */
    return opcode_to_encoding_info(
        opcode, dr_get_isa_mode(get_thread_private_dcontext()) _IF_ARM(false));
}

#undef instr_get_src
opnd_t
instr_get_src(instr_t *instr, uint pos)
{
    return INSTR_GET_SRC(instr, pos);
}
#define instr_get_src INSTR_GET_SRC

#undef instr_get_dst
opnd_t
instr_get_dst(instr_t *instr, uint pos)
{
    return INSTR_GET_DST(instr, pos);
}
#define instr_get_dst INSTR_GET_DST

/* allocates storage for instr_num_srcs src operands and instr_num_dsts dst operands
 * assumes that instr is currently all zeroed out!
 */
void
instr_set_num_opnds(void *drcontext, instr_t *instr, int instr_num_dsts,
                    int instr_num_srcs)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (instr_num_dsts > 0) {
        CLIENT_ASSERT(instr->num_dsts == 0 && instr->dsts == NULL,
                      "instr_set_num_opnds: dsts are already set");
        CLIENT_ASSERT_TRUNCATE(instr->num_dsts, byte, instr_num_dsts,
                               "instr_set_num_opnds: too many dsts");
        instr->num_dsts = (byte)instr_num_dsts;
        if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags)) {
            instr_noalloc_t *noalloc = (instr_noalloc_t *)instr;
            noalloc->instr.dsts = noalloc->dsts;
        } else {
            instr->dsts = (opnd_t *)heap_alloc(
                dcontext, instr_num_dsts * sizeof(opnd_t) HEAPACCT(ACCT_IR));
        }
    }
    if (instr_num_srcs > 0) {
        /* remember that src0 is static, rest are dynamic */
        if (instr_num_srcs > 1) {
            CLIENT_ASSERT(instr->num_srcs <= 1 && instr->srcs == NULL,
                          "instr_set_num_opnds: srcs are already set");
            if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags)) {
                instr_noalloc_t *noalloc = (instr_noalloc_t *)instr;
                noalloc->instr.srcs = noalloc->srcs;
            } else {
                instr->srcs = (opnd_t *)heap_alloc(
                    dcontext, (instr_num_srcs - 1) * sizeof(opnd_t) HEAPACCT(ACCT_IR));
            }
        }
        CLIENT_ASSERT_TRUNCATE(instr->num_srcs, byte, instr_num_srcs,
                               "instr_set_num_opnds: too many srcs");
        instr->num_srcs = (byte)instr_num_srcs;
    }
    instr_being_modified(instr, false /*raw bits invalid*/);
    /* assume all operands are valid */
    instr_set_operands_valid(instr, true);
}

/* sets the src opnd at position pos in instr */
void
instr_set_src(instr_t *instr, uint pos, opnd_t opnd)
{
    CLIENT_ASSERT(pos >= 0 && pos < instr->num_srcs, "instr_set_src: ordinal invalid");
    /* remember that src0 is static, rest are dynamic */
    if (pos == 0)
        instr->src0 = opnd;
    else
        instr->srcs[pos - 1] = opnd;
    /* if we're modifying operands, don't use original bits to encode! */
    instr_being_modified(instr, false /*raw bits invalid*/);
    /* assume all operands are valid */
    instr_set_operands_valid(instr, true);
}

/* sets the dst opnd at position pos in instr */
void
instr_set_dst(instr_t *instr, uint pos, opnd_t opnd)
{
    CLIENT_ASSERT(pos >= 0 && pos < instr->num_dsts, "instr_set_dst: ordinal invalid");
    instr->dsts[pos] = opnd;
    /* if we're modifying operands, don't use original bits to encode! */
    instr_being_modified(instr, false /*raw bits invalid*/);
    /* assume all operands are valid */
    instr_set_operands_valid(instr, true);
}

/* end is open-ended (so pass pos,pos+1 to remove just the pos-th src) */
void
instr_remove_srcs(void *drcontext, instr_t *instr, uint start, uint end)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    opnd_t *new_srcs;
    CLIENT_ASSERT(!TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags),
                  /* We could implement, but it does not seem an important use case. */
                  "instr_remove_srcs not supported for instr_noalloc_t");
    CLIENT_ASSERT(start >= 0 && end <= instr->num_srcs && start < end,
                  "instr_remove_srcs: ordinals invalid");
    if (instr->num_srcs - 1 > (byte)(end - start)) {
        new_srcs = (opnd_t *)heap_alloc(dcontext,
                                        (instr->num_srcs - 1 - (end - start)) *
                                            sizeof(opnd_t) HEAPACCT(ACCT_IR));
        if (start > 1)
            memcpy(new_srcs, instr->srcs, (start - 1) * sizeof(opnd_t));
        if ((byte)end < instr->num_srcs - 1) {
            memcpy(new_srcs + (start == 0 ? 0 : (start - 1)), instr->srcs + end,
                   (instr->num_srcs - 1 - end) * sizeof(opnd_t));
        }
    } else
        new_srcs = NULL;
    if (start == 0 && end < instr->num_srcs)
        instr->src0 = instr->srcs[end - 1];
    heap_free(dcontext, instr->srcs,
              (instr->num_srcs - 1) * sizeof(opnd_t) HEAPACCT(ACCT_IR));
    instr->num_srcs -= (byte)(end - start);
    instr->srcs = new_srcs;
    instr_being_modified(instr, false /*raw bits invalid*/);
    instr_set_operands_valid(instr, true);
}

/* end is open-ended (so pass pos,pos+1 to remove just the pos-th dst) */
void
instr_remove_dsts(void *drcontext, instr_t *instr, uint start, uint end)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    opnd_t *new_dsts;
    CLIENT_ASSERT(!TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags),
                  /* We could implement, but it does not seem an important use case. */
                  "instr_remove_srcs not supported for instr_noalloc_t");
    CLIENT_ASSERT(start >= 0 && end <= instr->num_dsts && start < end,
                  "instr_remove_dsts: ordinals invalid");
    if (instr->num_dsts > (byte)(end - start)) {
        new_dsts = (opnd_t *)heap_alloc(dcontext,
                                        (instr->num_dsts - (end - start)) *
                                            sizeof(opnd_t) HEAPACCT(ACCT_IR));
        if (start > 0)
            memcpy(new_dsts, instr->dsts, start * sizeof(opnd_t));
        if (end < instr->num_dsts) {
            memcpy(new_dsts + start, instr->dsts + end,
                   (instr->num_dsts - end) * sizeof(opnd_t));
        }
    } else
        new_dsts = NULL;
    heap_free(dcontext, instr->dsts, instr->num_dsts * sizeof(opnd_t) HEAPACCT(ACCT_IR));
    instr->num_dsts -= (byte)(end - start);
    instr->dsts = new_dsts;
    instr_being_modified(instr, false /*raw bits invalid*/);
    instr_set_operands_valid(instr, true);
}

#undef instr_get_target
opnd_t
instr_get_target(instr_t *instr)
{
    return INSTR_GET_TARGET(instr);
}
#define instr_get_target INSTR_GET_TARGET

/* Assumes that if an instr has a jump target, it's stored in the 0th src
 * location.
 */
void
instr_set_target(instr_t *instr, opnd_t target)
{
    CLIENT_ASSERT(instr->num_srcs >= 1, "instr_set_target: instr has no sources");
    instr->src0 = target;
    /* if we're modifying operands, don't use original bits to encode,
     * except for jecxz/loop*
     */
    instr_being_modified(instr, instr_is_cti_short_rewrite(instr, NULL));
    /* assume all operands are valid */
    instr_set_operands_valid(instr, true);
}

instr_t *
instr_set_prefix_flag(instr_t *instr, uint prefix)
{
    instr->prefixes |= prefix;
    instr_being_modified(instr, false /*raw bits invalid*/);
    return instr;
}

bool
instr_get_prefix_flag(instr_t *instr, uint prefix)
{
    return ((instr->prefixes & prefix) != 0);
}

void
instr_set_prefixes(instr_t *instr, uint prefixes)
{
    instr->prefixes = prefixes;
    instr_being_modified(instr, false /*raw bits invalid*/);
}

uint
instr_get_prefixes(instr_t *instr)
{
    return instr->prefixes;
}

bool
instr_is_predicated(instr_t *instr)
{
    /* XXX i#1556: we should also mark jecxz and string loops as predicated! */
    dr_pred_type_t pred = instr_get_predicate(instr);
    return instr_predicate_is_cond(pred);
}

dr_pred_type_t
instr_get_predicate(instr_t *instr)
{
    /* Optimization: we assume prefixes are the high bits to avoid an & */
    return instr->prefixes >> PREFIX_PRED_BITPOS;
}

instr_t *
instr_set_predicate(instr_t *instr, dr_pred_type_t pred)
{
    instr->prefixes = ((instr->prefixes & ~PREFIX_PRED_MASK) |
                       ((pred << PREFIX_PRED_BITPOS) & PREFIX_PRED_MASK));
    return instr;
}

bool
instr_branch_is_padded(instr_t *instr)
{
    return TEST(INSTR_BRANCH_PADDED, instr->flags);
}

void
instr_branch_set_padded(instr_t *instr, bool val)
{
    if (val)
        instr->flags |= INSTR_BRANCH_PADDED;
    else
        instr->flags &= ~INSTR_BRANCH_PADDED;
}

/* Returns true iff instr has been marked as a special exit cti */
bool
instr_branch_special_exit(instr_t *instr)
{
    return TEST(INSTR_BRANCH_SPECIAL_EXIT, instr->flags);
}

/* If val is true, indicates that instr is a special exit cti.
 * If val is false, indicates otherwise
 */
void
instr_branch_set_special_exit(instr_t *instr, bool val)
{
    if (val)
        instr->flags |= INSTR_BRANCH_SPECIAL_EXIT;
    else
        instr->flags &= ~INSTR_BRANCH_SPECIAL_EXIT;
}

/* Returns the type of the original indirect branch of an exit
 */
int
instr_exit_branch_type(instr_t *instr)
{
    return instr->flags & EXIT_CTI_TYPES;
}

/* Set type of indirect branch exit
 */
void
instr_exit_branch_set_type(instr_t *instr, uint type)
{
    /* set only expected flags */
    type &= EXIT_CTI_TYPES;
    instr->flags &= ~EXIT_CTI_TYPES;
    instr->flags |= type;
}

void
instr_set_ok_to_mangle(instr_t *instr, bool val)
{
    if (val)
        instr_set_app(instr);
    else
        instr_set_meta(instr);
}

void
instr_set_app(instr_t *instr)
{
    instr->flags &= ~INSTR_DO_NOT_MANGLE;
}

void
instr_set_meta(instr_t *instr)
{
    instr->flags |= INSTR_DO_NOT_MANGLE;
}

bool
instr_is_meta_may_fault(instr_t *instr)
{
    /* no longer using a special flag (i#496) */
    return instr_is_meta(instr) && instr_get_translation(instr) != NULL;
}

void
instr_set_meta_may_fault(instr_t *instr, bool val)
{
    /* no longer using a special flag (i#496) */
    instr_set_meta(instr);
    CLIENT_ASSERT(instr_get_translation(instr) != NULL,
                  "meta_may_fault instr must have translation");
}

/* convenience routine */
void
instr_set_meta_no_translation(instr_t *instr)
{
    instr_set_meta(instr);
    instr_set_translation(instr, NULL);
}

void
instr_set_ok_to_emit(instr_t *instr, bool val)
{
    CLIENT_ASSERT(instr != NULL, "instr_set_ok_to_emit: passed NULL");
    if (val)
        instr->flags &= ~INSTR_DO_NOT_EMIT;
    else
        instr->flags |= INSTR_DO_NOT_EMIT;
}

uint
instr_eflags_conditionally(uint full_eflags, dr_pred_type_t pred,
                           dr_opnd_query_flags_t flags)
{
    if (!TEST(DR_QUERY_INCLUDE_COND_SRCS, flags) && instr_predicate_is_cond(pred) &&
        !instr_predicate_reads_srcs(pred)) {
        /* i#1836: the predicate itself reads some flags */
        full_eflags &= ~EFLAGS_READ_NON_PRED;
    }
    if (!TEST(DR_QUERY_INCLUDE_COND_DSTS, flags) && instr_predicate_is_cond(pred) &&
        !instr_predicate_writes_eflags(pred))
        full_eflags &= ~EFLAGS_WRITE_ALL;
    return full_eflags;
}

uint
instr_get_eflags(instr_t *instr, dr_opnd_query_flags_t flags)
{
    if ((instr->flags & INSTR_EFLAGS_VALID) == 0) {
        bool encoded = false;
        dcontext_t *dcontext = get_thread_private_dcontext();
        dr_isa_mode_t old_mode;
        /* we assume we cannot trust the opcode independently of operands */
        if (instr_needs_encoding(instr)) {
            int len;
            encoded = true;
            len = private_instr_encode(dcontext, instr, true /*cache*/);
            if (len == 0) {
                CLIENT_ASSERT(instr_is_label(instr), "instr_get_eflags: invalid instr");
                return 0;
            }
        }
        dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);
        decode_eflags_usage(dcontext, instr_get_raw_bits(instr), &instr->eflags,
                            DR_QUERY_INCLUDE_ALL);
        dr_set_isa_mode(dcontext, old_mode, NULL);
        if (encoded) {
            /* if private_instr_encode passed us back whether it's valid
             * to cache (i.e., non-meta instr that can reach) we could skip
             * this invalidation for such cases */
            instr_free_raw_bits(dcontext, instr);
            CLIENT_ASSERT(!instr_raw_bits_valid(instr), "internal encoding buf error");
        }
        /* even if decode fails, set valid to true -- ok?  FIXME */
        instr_set_eflags_valid(instr, true);
    }
    return instr_eflags_conditionally(instr->eflags, instr_get_predicate(instr), flags);
}

DR_API
/* Returns the eflags usage of instructions with opcode "opcode",
 * as EFLAGS_ constants or'ed together
 */
uint
instr_get_opcode_eflags(int opcode)
{
    /* assumption: all encoding of an opcode have same eflags behavior! */
    const instr_info_t *info = get_instr_info(opcode);
    return info->eflags;
}

uint
instr_get_arith_flags(instr_t *instr, dr_opnd_query_flags_t flags)
{
    if ((instr->flags & INSTR_EFLAGS_6_VALID) == 0) {
        /* just get info on all the flags */
        return instr_get_eflags(instr, flags);
    }
    return instr_eflags_conditionally(instr->eflags, instr_get_predicate(instr), flags);
}

bool
instr_eflags_valid(instr_t *instr)
{
    return ((instr->flags & INSTR_EFLAGS_VALID) != 0);
}

void
instr_set_eflags_valid(instr_t *instr, bool valid)
{
    if (valid) {
        instr->flags |= INSTR_EFLAGS_VALID;
        instr->flags |= INSTR_EFLAGS_6_VALID;
    } else {
        /* assume that arith flags are also invalid */
        instr->flags &= ~INSTR_EFLAGS_VALID;
        instr->flags &= ~INSTR_EFLAGS_6_VALID;
    }
}

/* Returns true iff instr's arithmetic flags (the 6 bottom eflags) are
 * up to date */
bool
instr_arith_flags_valid(instr_t *instr)
{
    return ((instr->flags & INSTR_EFLAGS_6_VALID) != 0);
}

/* Sets instr's arithmetic flags (the 6 bottom eflags) to be valid if
 * valid is true, invalid otherwise */
void
instr_set_arith_flags_valid(instr_t *instr, bool valid)
{
    if (valid)
        instr->flags |= INSTR_EFLAGS_6_VALID;
    else {
        instr->flags &= ~INSTR_EFLAGS_VALID;
        instr->flags &= ~INSTR_EFLAGS_6_VALID;
    }
}

void
instr_set_operands_valid(instr_t *instr, bool valid)
{
    if (valid)
        instr->flags |= INSTR_OPERANDS_VALID;
    else
        instr->flags &= ~INSTR_OPERANDS_VALID;
}

/* N.B.: this routine sets the "raw bits are valid" flag */
void
instr_set_raw_bits(instr_t *instr, byte *addr, uint length)
{
    if ((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0) {
        /* this does happen, when up-decoding an instr using its
         * own raw bits, so let it happen, but make sure allocated
         * bits aren't being lost
         */
        CLIENT_ASSERT(addr == instr->bytes && length == instr->length,
                      "instr_set_raw_bits: bits already there, but different");
    }
    if (!instr_valid(instr))
        instr_set_opcode(instr, OP_UNDECODED);
    instr->flags |= INSTR_RAW_BITS_VALID;
    instr->bytes = addr;
    instr->length = length;
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

/* this is sort of a hack, used to allow dynamic reallocation of
 * the trace buffer, which requires shifting the addresses of all
 * the trace Instrs since they point into the old buffer
 */
void
instr_shift_raw_bits(instr_t *instr, ssize_t offs)
{
    if ((instr->flags & INSTR_RAW_BITS_VALID) != 0)
        instr->bytes += offs;
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

/* moves the instruction from USE_ORIGINAL_BITS state to a
 * needs-full-encoding state
 */
void
instr_set_raw_bits_valid(instr_t *instr, bool valid)
{
    if (valid)
        instr->flags |= INSTR_RAW_BITS_VALID;
    else {
        instr->flags &= ~INSTR_RAW_BITS_VALID;
        /* DO NOT set bytes to NULL or length to 0, we still want to be
         * able to point at the original instruction for use in translating
         * addresses for exception/signal handlers
         * Also do not de-allocate allocated bits
         */
#ifdef X86
        instr_set_rip_rel_valid(instr, false);
#endif
    }
}

void
instr_free_raw_bits(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if ((instr->flags & INSTR_RAW_BITS_ALLOCATED) == 0)
        return;
    if (!TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags))
        heap_reachable_free(dcontext, instr->bytes, instr->length HEAPACCT(ACCT_IR));
    instr->bytes = NULL;
    instr->flags &= ~INSTR_RAW_BITS_VALID;
    instr->flags &= ~INSTR_RAW_BITS_ALLOCATED;
}

/* creates array of bytes to store raw bytes of an instr into
 * (original bits are read-only)
 * initializes array to the original bits!
 */
void
instr_allocate_raw_bits(void *drcontext, instr_t *instr, uint num_bytes)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    byte *original_bits = NULL;
    if (TEST(INSTR_RAW_BITS_VALID, instr->flags))
        original_bits = instr->bytes;
    if (!TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags) || instr->length != num_bytes) {
        byte *new_bits;
        if (TEST(INSTR_IS_NOALLOC_STRUCT, instr->flags)) {
            /* This may not be reachable, so re-relativization is limited. */
            instr_noalloc_t *noalloc = (instr_noalloc_t *)instr;
            CLIENT_ASSERT(num_bytes <= sizeof(noalloc->encode_buf),
                          "instr_allocate_raw_bits exceeds instr_noalloc_t capacity");
            new_bits = noalloc->encode_buf;
        } else {
            /* We need reachable heap for rip-rel re-relativization. */
            new_bits =
                (byte *)heap_reachable_alloc(dcontext, num_bytes HEAPACCT(ACCT_IR));
        }
        if (original_bits != NULL) {
            /* copy original bits into modified bits so can just modify
             * a few and still have all info in one place
             */
            memcpy(new_bits, original_bits,
                   (num_bytes < instr->length) ? num_bytes : instr->length);
        }
        if ((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0)
            instr_free_raw_bits(dcontext, instr);
        instr->bytes = new_bits;
        instr->length = num_bytes;
    }
    /* assume that the bits are now valid and the operands are not */
    instr->flags |= INSTR_RAW_BITS_VALID;
    instr->flags |= INSTR_RAW_BITS_ALLOCATED;
    instr->flags &= ~INSTR_OPERANDS_VALID;
    instr->flags &= ~INSTR_EFLAGS_VALID;
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

void
instr_set_label_callback(instr_t *instr, instr_label_callback_t cb)
{
    CLIENT_ASSERT(instr_is_label(instr),
                  "only set callback functions for label instructions");
    CLIENT_ASSERT(instr->label_cb == NULL, "label callback function is already set");
    CLIENT_ASSERT(!TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags),
                  "instruction's raw bits occupying label callback memory");
    instr->label_cb = cb;
}

void
instr_clear_label_callback(instr_t *instr)
{
    CLIENT_ASSERT(instr_is_label(instr),
                  "only set callback functions for label instructions");
    CLIENT_ASSERT(instr->label_cb != NULL, "label callback function not set");
    CLIENT_ASSERT(!TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags),
                  "instruction's raw bits occupying label callback memory");
    instr->label_cb = NULL;
}

instr_label_callback_t
instr_get_label_callback(instr_t *instr)
{
    CLIENT_ASSERT(instr_is_label(instr),
                  "only label instructions have a callback function");
    CLIENT_ASSERT(!TEST(INSTR_RAW_BITS_ALLOCATED, instr->flags),
                  "instruction's raw bits occupying label callback memory");
    return instr->label_cb;
}

instr_t *
instr_set_translation(instr_t *instr, app_pc addr)
{
#if defined(WINDOWS) && !defined(STANDALONE_DECODER)
    addr = get_app_pc_from_intercept_pc_if_necessary(addr);
#endif
    instr->translation = addr;
    return instr;
}

app_pc
instr_get_translation(instr_t *instr)
{
    return instr->translation;
}

/* This makes it safe to keep an instr around indefinitely when an instrs raw
 * bits point into the cache. It allocates memory local to the instr to hold
 * a copy of the raw bits. If this was not done the original raw bits could
 * be deleted at some point.  This is necessary if you want to keep an instr
 * around for a long time (for clients, beyond returning from the call that
 * gave you the instr)
 */
void
instr_make_persistent(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if ((instr->flags & INSTR_RAW_BITS_VALID) != 0 &&
        (instr->flags & INSTR_RAW_BITS_ALLOCATED) == 0) {
        instr_allocate_raw_bits(dcontext, instr, instr->length);
    }
}

byte *
instr_get_raw_bits(instr_t *instr)
{
    return instr->bytes;
}

/* returns the pos-th instr byte */
byte
instr_get_raw_byte(instr_t *instr, uint pos)
{
    CLIENT_ASSERT(pos >= 0 && pos < instr->length && instr->bytes != NULL,
                  "instr_get_raw_byte: ordinal invalid, or no raw bits");
    return instr->bytes[pos];
}

/* returns the 4 bytes starting at position pos */
uint
instr_get_raw_word(instr_t *instr, uint pos)
{
    CLIENT_ASSERT(pos >= 0 && pos + 3 < instr->length && instr->bytes != NULL,
                  "instr_get_raw_word: ordinal invalid, or no raw bits");
    return *((uint *)(instr->bytes + pos));
}

/* Sets the pos-th instr byte by storing the unsigned
 * character value in the pos-th slot
 * Must call instr_allocate_raw_bits before calling this function
 * (original bits are read-only!)
 */
void
instr_set_raw_byte(instr_t *instr, uint pos, byte val)
{
    CLIENT_ASSERT((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0,
                  "instr_set_raw_byte: no raw bits");
    CLIENT_ASSERT(pos >= 0 && pos < instr->length && instr->bytes != NULL,
                  "instr_set_raw_byte: ordinal invalid, or no raw bits");
    instr->bytes[pos] = (byte)val;
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

/* Copies num_bytes bytes from start into the mangled bytes
 * array of instr.
 * Must call instr_allocate_raw_bits before calling this function.
 */
void
instr_set_raw_bytes(instr_t *instr, byte *start, uint num_bytes)
{
    CLIENT_ASSERT((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0,
                  "instr_set_raw_bytes: no raw bits");
    CLIENT_ASSERT(num_bytes <= instr->length && instr->bytes != NULL,
                  "instr_set_raw_bytes: ordinal invalid, or no raw bits");
    memcpy(instr->bytes, start, num_bytes);
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

/* Stores 32-bit value word in positions pos through pos+3 in
 * modified_bits.
 * Must call instr_allocate_raw_bits before calling this function.
 */
void
instr_set_raw_word(instr_t *instr, uint pos, uint word)
{
    CLIENT_ASSERT((instr->flags & INSTR_RAW_BITS_ALLOCATED) != 0,
                  "instr_set_raw_word: no raw bits");
    CLIENT_ASSERT(pos >= 0 && pos + 3 < instr->length && instr->bytes != NULL,
                  "instr_set_raw_word: ordinal invalid, or no raw bits");
    *((uint *)(instr->bytes + pos)) = word;
#ifdef X86
    instr_set_rip_rel_valid(instr, false); /* relies on original raw bits */
#endif
}

int
instr_length(void *drcontext, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    int res;

#ifdef ARM
    /* We can't handle IT blocks if we only track state on some instrs that
     * we have to encode for length, so unfortunately we must pay the cost
     * of tracking for every length call.
     */
    encode_track_it_block(dcontext, instr);
#endif

    if (!instr_needs_encoding(instr))
        return instr->length;

    res = instr_length_arch(dcontext, instr);
    if (res != -1)
        return res;

    /* else, encode to find length */
    return private_instr_encode(dcontext, instr, false /*don't need to cache*/);
}

instr_t *
instr_set_encoding_hint(instr_t *instr, dr_encoding_hint_type_t hint)
{
    instr->encoding_hints |= hint;
    return instr;
}

bool
instr_has_encoding_hint(instr_t *instr, dr_encoding_hint_type_t hint)
{
    return TEST(hint, instr->encoding_hints);
}

/***********************************************************************/
/* decoding routines */

/* If instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands instr into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.
 * Returns the replacement of instr, if any expansion is performed
 * (in which case the old instr is destroyed); otherwise returns
 * instr unchanged.
 * If encounters an invalid instr, stops expanding at that instr, and keeps
 * instr in the ilist pointing to the invalid bits as an invalid instr.
 */
instr_t *
instr_expand(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* Sometimes deleting instr but sometimes not (when return early)
     * is painful -- so we go to the trouble of re-using instr
     * for the first expanded instr
     */
    instr_t *newinstr, *firstinstr = NULL;
    int remaining_bytes, cur_inst_len;
    byte *curbytes, *newbytes;
    dr_isa_mode_t old_mode;

    /* make it easy for iterators: handle NULL
     * assume that if opcode is valid, is at Level 2, so not a bundle
     * do not expand meta-instrs -- FIXME: is that the right thing to do?
     */
    if (instr == NULL || instr_opcode_valid(instr) || instr_is_meta(instr) ||
        /* if an invalid instr (not just undecoded) do not try to expand */
        !instr_valid(instr))
        return instr;

    DOLOG(5, LOG_ALL, {
        /* disassembling might change the instruction object, we're cloning it
         * for the logger */
        instr_t *log_instr = instr_clone(dcontext, instr);
        d_r_loginst(dcontext, 4, log_instr, "instr_expand");
        instr_destroy(dcontext, log_instr);
    });

    /* decode routines use dcontext mode, but we want instr mode */
    dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);

    /* never have opnds but not opcode */
    CLIENT_ASSERT(!instr_operands_valid(instr), "instr_expand: opnds are already valid");
    CLIENT_ASSERT(instr_raw_bits_valid(instr), "instr_expand: raw bits are invalid");
    curbytes = instr->bytes;
    if ((uint)decode_sizeof(dcontext, curbytes, NULL _IF_X86_64(NULL)) == instr->length) {
        dr_set_isa_mode(dcontext, old_mode, NULL);
        return instr; /* Level 1 */
    }

    remaining_bytes = instr->length;
    while (remaining_bytes > 0) {
        /* insert every separated instr into list */
        newinstr = instr_create(dcontext);
        newbytes = decode_raw(dcontext, curbytes, newinstr);
#ifndef NOT_DYNAMORIO_CORE_PROPER
        if (expand_should_set_translation(dcontext))
            instr_set_translation(newinstr, curbytes);
#endif
        if (newbytes == NULL) {
            /* invalid instr -- stop expanding, point instr at remaining bytes */
            instr_set_raw_bits(instr, curbytes, remaining_bytes);
            instr_set_opcode(instr, OP_INVALID);
            if (firstinstr == NULL)
                firstinstr = instr;
            instr_destroy(dcontext, newinstr);
            dr_set_isa_mode(dcontext, old_mode, NULL);
            return firstinstr;
        }
        DOLOG(5, LOG_ALL,
              { d_r_loginst(dcontext, 4, newinstr, "\tjust expanded into"); });

        /* CAREFUL of what you call here -- don't call anything that
         * auto-upgrades instr to Level 2, it will fail on Level 0 bundles!
         */

        if (instr_has_allocated_bits(instr) &&
            !instr_is_cti_short_rewrite(newinstr, curbytes)) {
            /* make sure to have our own copy of any allocated bits
             * before we destroy the original instr
             */
            IF_X64(CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(newbytes - curbytes),
                                 "instr_expand: internal truncation error"));
            instr_allocate_raw_bits(dcontext, newinstr, (uint)(newbytes - curbytes));
        }

        /* special case: for cti_short, do not fully decode the
         * constituent instructions, leave as a bundle.
         * the instr will still have operands valid.
         */
        if (instr_is_cti_short_rewrite(newinstr, curbytes)) {
            newbytes = remangle_short_rewrite(dcontext, newinstr, curbytes, 0);
        } else if (instr_is_cti_short(newinstr)) {
            /* make sure non-mangled short ctis, which are generated by
             * us and never left there from app's, are not marked as exit ctis
             */
            instr_set_meta(newinstr);
        }

        IF_X64(CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_int(newbytes - curbytes),
                             "instr_expand: internal truncation error"));
        cur_inst_len = (int)(newbytes - curbytes);
        remaining_bytes -= cur_inst_len;
        curbytes = newbytes;

        instrlist_preinsert(ilist, instr, newinstr);
        if (firstinstr == NULL)
            firstinstr = newinstr;
    }

    /* delete original instr from list */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);

    CLIENT_ASSERT(firstinstr != NULL, "instr_expand failure");
    dr_set_isa_mode(dcontext, old_mode, NULL);
    return firstinstr;
}

bool
instr_is_level_0(instr_t *instr)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    dr_isa_mode_t old_mode;
    /* assume that if opcode is valid, is at Level 2, so not a bundle
     * do not expand meta-instrs -- FIXME: is that the right to do? */
    if (instr == NULL || instr_opcode_valid(instr) || instr_is_meta(instr) ||
        /* if an invalid instr (not just undecoded) do not try to expand */
        !instr_valid(instr))
        return false;

    /* never have opnds but not opcode */
    CLIENT_ASSERT(!instr_operands_valid(instr),
                  "instr_is_level_0: opnds are already valid");
    CLIENT_ASSERT(instr_raw_bits_valid(instr), "instr_is_level_0: raw bits are invalid");
    dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);
    if ((uint)decode_sizeof(dcontext, instr->bytes, NULL _IF_X86_64(NULL)) ==
        instr->length) {
        dr_set_isa_mode(dcontext, old_mode, NULL);
        return false; /* Level 1 */
    }
    dr_set_isa_mode(dcontext, old_mode, NULL);
    return true;
}

/* If the next instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new next instr.
 */
instr_t *
instr_get_next_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    instr_expand(dcontext, ilist, instr_get_next(instr));
    return instr_get_next(instr);
}

/* If the prev instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new prev instr.
 */
instr_t *
instr_get_prev_expanded(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    instr_expand(dcontext, ilist, instr_get_prev(instr));
    return instr_get_prev(instr);
}

/* If the first instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new first instr.
 */
instr_t *
instrlist_first_expanded(dcontext_t *dcontext, instrlist_t *ilist)
{
    instr_expand(dcontext, ilist, instrlist_first(ilist));
    return instrlist_first(ilist);
}

/* If the last instr is at Level 0 (i.e., a bundled group of instrs as raw bits),
 * expands it into a sequence of Level 1 instrs using decode_raw() which
 * are added in place to ilist.  Then returns the new last instr.
 */
instr_t *
instrlist_last_expanded(dcontext_t *dcontext, instrlist_t *ilist)
{
    instr_expand(dcontext, ilist, instrlist_last(ilist));
    return instrlist_last(ilist);
}

/* If instr is not already at the level of decode_cti, decodes enough
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
instr_decode_cti(dcontext_t *dcontext, instr_t *instr)
{
    /* if arith flags are missing but otherwise decoded, who cares,
     * next get_arith_flags() will fill it in
     */
    if (!instr_opcode_valid(instr) ||
        (instr_is_cti(instr) && !instr_operands_valid(instr))) {
        DEBUG_EXT_DECLARE(byte * next_pc;)
        DEBUG_EXT_DECLARE(int old_len = instr->length;)
        /* decode_cti() will use the dcontext mode, but we want the instr mode */
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);
        CLIENT_ASSERT(instr_raw_bits_valid(instr),
                      "instr_decode_cti: raw bits are invalid");
        instr_reuse(dcontext, instr);
        DEBUG_EXT_DECLARE(next_pc =) decode_cti(dcontext, instr->bytes, instr);
        dr_set_isa_mode(dcontext, old_mode, NULL);
        /* ok to be invalid, let caller deal with it */
        CLIENT_ASSERT(next_pc == NULL || (next_pc - instr->bytes == old_len),
                      "instr_decode_cti requires a Level 1 or higher instruction");
    }
}

/* If instr is not already at the level of decode_opcode, decodes enough
 * from the raw bits pointed to by instr to bring it to that level.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 *
 * decode_opcode decodes the opcode and eflags usage of the instruction.
 * This corresponds to a Level 2 decoding.
 */
void
instr_decode_opcode(dcontext_t *dcontext, instr_t *instr)
{
    if (!instr_opcode_valid(instr)) {
        DEBUG_EXT_DECLARE(byte * next_pc;)
        DEBUG_EXT_DECLARE(int old_len = instr->length;)
#ifdef X86
        bool rip_rel_valid = instr_rip_rel_valid(instr);
#endif
        /* decode_opcode() will use the dcontext mode, but we want the instr mode */
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);
        CLIENT_ASSERT(instr_raw_bits_valid(instr),
                      "instr_decode_opcode: raw bits are invalid");
        instr_reuse(dcontext, instr);
        DEBUG_EXT_DECLARE(next_pc =) decode_opcode(dcontext, instr->bytes, instr);
        dr_set_isa_mode(dcontext, old_mode, NULL);
#ifdef X86
        /* decode_opcode sets raw bits which invalidates rip_rel, but
         * it should still be valid on an up-decode of the opcode */
        if (rip_rel_valid)
            instr_set_rip_rel_pos(instr, instr->rip_rel_pos);
#endif
        /* ok to be invalid, let caller deal with it */
        CLIENT_ASSERT(next_pc == NULL || (next_pc - instr->bytes == old_len),
                      "instr_decode_opcode requires a Level 1 or higher instruction");
    }
}

/* If instr is not already fully decoded, decodes enough
 * from the raw bits pointed to by instr to bring it Level 3.
 * Assumes that instr is a single instr (i.e., NOT Level 0).
 */
void
instr_decode(dcontext_t *dcontext, instr_t *instr)
{
    if (!instr_operands_valid(instr)) {
        DEBUG_EXT_DECLARE(byte * next_pc;)
        DEBUG_EXT_DECLARE(int old_len = instr->length;)
#ifdef X86
        bool rip_rel_valid = instr_rip_rel_valid(instr);
#endif
        /* decode() will use the current dcontext mode, but we want the instr mode */
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dcontext, instr_get_isa_mode(instr), &old_mode);
        CLIENT_ASSERT(instr_raw_bits_valid(instr), "instr_decode: raw bits are invalid");
        instr_reuse(dcontext, instr);
        DEBUG_EXT_DECLARE(next_pc =) decode(dcontext, instr_get_raw_bits(instr), instr);
#ifndef NOT_DYNAMORIO_CORE_PROPER
        if (expand_should_set_translation(dcontext))
            instr_set_translation(instr, instr_get_raw_bits(instr));
#endif
        dr_set_isa_mode(dcontext, old_mode, NULL);
#ifdef X86
        /* decode sets raw bits which invalidates rip_rel, but
         * it should still be valid on an up-decode */
        if (rip_rel_valid)
            instr_set_rip_rel_pos(instr, instr->rip_rel_pos);
#endif
        /* ok to be invalid, let caller deal with it */
        CLIENT_ASSERT(next_pc == NULL || (next_pc - instr->bytes == old_len),
                      "instr_decode requires a Level 1 or higher instruction");
    }
}

/* Calls instr_decode() with the current dcontext.  Mostly useful as the slow
 * path for IR routines that get inlined.
 */
NOINLINE /* rarely called */
    instr_t *
    instr_decode_with_current_dcontext(instr_t *instr)
{
    instr_decode(get_thread_private_dcontext(), instr);
    return instr;
}

/* Brings all instrs in ilist up to the decode_cti level, and
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
instrlist_decode_cti(dcontext_t *dcontext, instrlist_t *ilist)
{
    instr_t *instr;

    LOG(THREAD, LOG_ALL, 3, "\ninstrlist_decode_cti\n");

    DOLOG(4, LOG_ALL, {
        LOG(THREAD, LOG_ALL, 4, "beforehand:\n");
        instrlist_disassemble(dcontext, 0, ilist, THREAD);
    });

    /* just use the expanding iterator to get to Level 1, then decode cti */
    for (instr = instrlist_first_expanded(dcontext, ilist); instr != NULL;
         instr = instr_get_next_expanded(dcontext, ilist, instr)) {
        /* if arith flags are missing but otherwise decoded, who cares,
         * next get_arith_flags() will fill it in
         */
        if (!instr_opcode_valid(instr) ||
            (instr_is_cti(instr) && !instr_operands_valid(instr))) {
            DOLOG(4, LOG_ALL, {
                d_r_loginst(dcontext, 4, instr, "instrlist_decode_cti: about to decode");
            });
            instr_decode_cti(dcontext, instr);
            DOLOG(4, LOG_ALL, { d_r_loginst(dcontext, 4, instr, "\tjust decoded"); });
        }
    }

    /* must fix up intra-ilist cti's to have instr_t targets
     * assumption: all intra-ilist cti's have been marked as do-not-mangle,
     * plus all targets have their raw bits already set
     */
    for (instr = instrlist_first(ilist); instr != NULL; instr = instr_get_next(instr)) {
        /* N.B.: if we change exit cti's to have instr_t targets, we have to
         * change other modules like emit to handle that!
         * FIXME
         */
        if (!instr_is_exit_cti(instr) &&
            instr_opcode_valid(instr) && /* decode_cti only filled in cti opcodes */
            instr_is_cti(instr) && instr_num_srcs(instr) > 0 &&
            opnd_is_near_pc(instr_get_src(instr, 0))) {
            instr_t *tgt;
            DOLOG(4, LOG_ALL, {
                d_r_loginst(dcontext, 4, instr,
                            "instrlist_decode_cti: found cti w/ pc target");
            });
            for (tgt = instrlist_first(ilist); tgt != NULL; tgt = instr_get_next(tgt)) {
                DOLOG(4, LOG_ALL, { d_r_loginst(dcontext, 4, tgt, "\tchecking"); });
                LOG(THREAD, LOG_INTERP | LOG_OPTS, 4, "\t\taddress is " PFX "\n",
                    instr_get_raw_bits(tgt));
                if (opnd_get_pc(instr_get_target(instr)) == instr_get_raw_bits(tgt)) {
                    /* cti targets this instr */
                    app_pc bits = 0;
                    int len = 0;
                    if (instr_raw_bits_valid(instr)) {
                        bits = instr_get_raw_bits(instr);
                        len = instr_length(dcontext, instr);
                    }
                    instr_set_target(instr, opnd_create_instr(tgt));
                    if (bits != 0)
                        instr_set_raw_bits(instr, bits, len);
                    DOLOG(4, LOG_ALL,
                          { d_r_loginst(dcontext, 4, tgt, "\tcti targets this"); });
                    break;
                }
            }
        }
    }

    DOLOG(4, LOG_ALL, {
        LOG(THREAD, LOG_ALL, 4, "afterward:\n");
        instrlist_disassemble(dcontext, 0, ilist, THREAD);
    });
    LOG(THREAD, LOG_ALL, 4, "done with instrlist_decode_cti\n");
}

/****************************************************************************/
/* utility routines */

void
d_r_loginst(dcontext_t *dcontext, uint level, instr_t *instr, const char *string)
{
    DOLOG(level, LOG_ALL, {
        LOG(THREAD, LOG_ALL, level, "%s: ", string);
        instr_disassemble(dcontext, instr, THREAD);
        LOG(THREAD, LOG_ALL, level, "\n");
    });
}

void
d_r_logopnd(dcontext_t *dcontext, uint level, opnd_t opnd, const char *string)
{
    DOLOG(level, LOG_ALL, {
        LOG(THREAD, LOG_ALL, level, "%s: ", string);
        opnd_disassemble(dcontext, opnd, THREAD);
        LOG(THREAD, LOG_ALL, level, "\n");
    });
}

void
d_r_logtrace(dcontext_t *dcontext, uint level, instrlist_t *trace, const char *string)
{
    DOLOG(level, LOG_ALL, {
        instr_t *inst;
        instr_t *next_inst;
        LOG(THREAD, LOG_ALL, level, "%s:\n", string);
        for (inst = instrlist_first(trace); inst != NULL; inst = next_inst) {
            next_inst = instr_get_next(inst);
            instr_disassemble(dcontext, inst, THREAD);
            LOG(THREAD, LOG_ALL, level, "\n");
        }
        LOG(THREAD, LOG_ALL, level, "\n");
    });
}

/* Shrinks all registers not used as addresses, and all immed int and
 * address sizes, to 16 bits
 */
void
instr_shrink_to_16_bits(instr_t *instr)
{
    int i;
    opnd_t opnd;
    const instr_info_t *info;
    byte optype;
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_shrink_to_16_bits: invalid opnds");
    /* Our use of get_encoding_info() with no final PC specified works
     * as there are no encoding template choices involving reachability
     * which affect whether an operand has an indirect register.
     */
    info = get_encoding_info(instr);
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        /* some non-memory references vary in size by addr16, not data16:
         * e.g., the edi/esi inc/dec of string instrs
         */
        optype = instr_info_opnd_type(info, false /*dst*/, i);
        if (!opnd_is_memory_reference(opnd) && !optype_is_indir_reg(optype)) {
            instr_set_dst(instr, i, opnd_shrink_to_16_bits(opnd));
        }
    }
    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        optype = instr_info_opnd_type(info, true /*dst*/, i);
        if (!opnd_is_memory_reference(opnd) && !optype_is_indir_reg(optype)) {
            instr_set_src(instr, i, opnd_shrink_to_16_bits(opnd));
        }
    }
}

#ifdef X64
/* Shrinks all registers, including addresses, and all immed int and
 * address sizes, to 32 bits
 */
void
instr_shrink_to_32_bits(instr_t *instr)
{
    int i;
    opnd_t opnd;
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_shrink_to_32_bits: invalid opnds");
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        instr_set_dst(instr, i, opnd_shrink_to_32_bits(opnd));
    }
    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        if (opnd_is_immed_int(opnd)) {
            CLIENT_ASSERT(opnd_get_immed_int(opnd) <= INT_MAX,
                          "instr_shrink_to_32_bits: immed int will be truncated");
        }
        instr_set_src(instr, i, opnd_shrink_to_32_bits(opnd));
    }
}
#endif

bool
instr_uses_reg(instr_t *instr, reg_id_t reg)
{
    return (instr_reg_in_dst(instr, reg) || instr_reg_in_src(instr, reg));
}

bool
instr_reg_in_dst(instr_t *instr, reg_id_t reg)
{
#ifdef AARCH64
    /* FFR does not appear in any operand, it is implicit upon the instruction type or
     * accessed via SVE predicate registers.
     */
    if (reg == DR_REG_FFR) {
        switch (instr_get_opcode(instr)) {
        case OP_setffr:
        case OP_rdffr:

        case OP_ldff1b:
        case OP_ldff1d:
        case OP_ldff1h:
        case OP_ldff1sb:
        case OP_ldff1sh:
        case OP_ldff1sw:
        case OP_ldff1w:

        case OP_ldnf1b:
        case OP_ldnf1d:
        case OP_ldnf1h:
        case OP_ldnf1sb:
        case OP_ldnf1sh:
        case OP_ldnf1sw:
        case OP_ldnf1w: return true;
        default: break;
        }
    }
#endif

    int i;
    for (i = 0; i < instr_num_dsts(instr); i++) {
        if (opnd_uses_reg(instr_get_dst(instr, i), reg))
            return true;
    }
    return false;
}

bool
instr_reg_in_src(instr_t *instr, reg_id_t reg)
{
    int i;
#ifdef X86
    /* special case (we don't want all of instr_is_nop() special-cased: just this one) */
    if (instr_get_opcode(instr) == OP_nop_modrm)
        return false;
#endif

#ifdef AARCH64
    /* FFR does not appear in any operand, it is implicit upon the instruction type or
     * accessed via SVE predicate registers.
     */
    if (reg == DR_REG_FFR) {
        switch (instr_get_opcode(instr)) {
        case OP_wrffr:
        case OP_rdffrs: return true;
        default: break;
        }
    }
#endif
    for (i = 0; i < instr_num_srcs(instr); i++) {
        if (opnd_uses_reg(instr_get_src(instr, i), reg))
            return true;
    }
    return false;
}

/* checks regs in dest base-disp but not dest reg */
bool
instr_reads_from_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags)
{
    int i;
    opnd_t opnd;

    if (!TEST(DR_QUERY_INCLUDE_COND_SRCS, flags) && instr_is_predicated(instr) &&
        !instr_predicate_reads_srcs(instr_get_predicate(instr)))
        return false;

    if (instr_reg_in_src(instr, reg))
        return true;

    /* As a special case, the addressing registers inside a destination memory
     * operand are covered by DR_QUERY_INCLUDE_COND_SRCS rather than
     * DR_QUERY_INCLUDE_COND_DSTS (i#1849).
     */
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (!opnd_is_reg(opnd) && opnd_uses_reg(opnd, reg))
            return true;
    }
    return false;
}

/* In this func, it must be the exact same register, not a sub reg. ie. eax!=ax */
bool
instr_reads_from_exact_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags)
{
    int i;
    opnd_t opnd;

    if (!TEST(DR_QUERY_INCLUDE_COND_SRCS, flags) && instr_is_predicated(instr) &&
        !instr_predicate_reads_srcs(instr_get_predicate(instr)))
        return false;

#ifdef X86
    /* special case */
    if (instr_get_opcode(instr) == OP_nop_modrm)
        return false;
#endif

    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == reg &&
            opnd_get_size(opnd) == reg_get_size(reg))
            return true;
        else if (opnd_is_base_disp(opnd) &&
                 (opnd_get_base(opnd) == reg || opnd_get_index(opnd) == reg ||
                  opnd_get_segment(opnd) == reg))
            return true;
    }

    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_base_disp(opnd) &&
            (opnd_get_base(opnd) == reg || opnd_get_index(opnd) == reg ||
             opnd_get_segment(opnd) == reg))
            return true;
    }

    return false;
}

/* this checks sub-registers */
bool
instr_writes_to_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags)
{
    int i;
    opnd_t opnd;

    if (!TEST(DR_QUERY_INCLUDE_COND_DSTS, flags) && instr_is_predicated(instr))
        return false;

    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(opnd) && (dr_reg_fixer[opnd_get_reg(opnd)] == dr_reg_fixer[reg]))
            return true;
    }
    return false;
}

/* In this func, it must be the exact same register, not a sub reg. ie. eax!=ax */
bool
instr_writes_to_exact_reg(instr_t *instr, reg_id_t reg, dr_opnd_query_flags_t flags)
{
    int i;
    opnd_t opnd;

    if (!TEST(DR_QUERY_INCLUDE_COND_DSTS, flags) && instr_is_predicated(instr))
        return false;

    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(opnd) &&
            (opnd_get_reg(opnd) == reg)
            /* for case like OP_movt on ARM and SIMD regs on X86,
             * partial reg written with full reg name in opnd
             */
            && opnd_get_size(opnd) == reg_get_size(reg))
            return true;
    }
    return false;
}

bool
instr_replace_src_opnd(instr_t *instr, opnd_t old_opnd, opnd_t new_opnd)
{
    int srcs, a;

    srcs = instr_num_srcs(instr);

    for (a = 0; a < srcs; a++) {
        if (opnd_same(instr_get_src(instr, a), old_opnd) ||
            opnd_same_address(instr_get_src(instr, a), old_opnd)) {
            instr_set_src(instr, a, new_opnd);
            return true;
        }
    }
    return false;
}

bool
instr_replace_reg_resize(instr_t *instr, reg_id_t old_reg, reg_id_t new_reg)
{
    int i;
    bool found = false;
    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd_t opnd = instr_get_src(instr, i);
        if (opnd_uses_reg(opnd, old_reg)) {
            found = true;
            opnd_replace_reg_resize(&opnd, old_reg, new_reg);
            instr_set_src(instr, i, opnd);
        }
    }
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd_t opnd = instr_get_dst(instr, i);
        if (opnd_uses_reg(opnd, old_reg)) {
            found = true;
            opnd_replace_reg_resize(&opnd, old_reg, new_reg);
            instr_set_dst(instr, i, opnd);
        }
    }
    return found;
}

bool
instr_same(instr_t *inst1, instr_t *inst2)
{
    int dsts, srcs, a;

    if (instr_get_opcode(inst1) != instr_get_opcode(inst2))
        return false;

    if ((srcs = instr_num_srcs(inst1)) != instr_num_srcs(inst2))
        return false;
    for (a = 0; a < srcs; a++) {
        if (!opnd_same(instr_get_src(inst1, a), instr_get_src(inst2, a)))
            return false;
    }

    if ((dsts = instr_num_dsts(inst1)) != instr_num_dsts(inst2))
        return false;
    for (a = 0; a < dsts; a++) {
        if (!opnd_same(instr_get_dst(inst1, a), instr_get_dst(inst2, a)))
            return false;
    }

    /* We encode some prefixes in the operands themselves, such that
     * we shouldn't consider the whole-instr_t flags when considering
     * equality of Instrs
     */
    if ((instr_get_prefixes(inst1) & PREFIX_SIGNIFICANT) !=
        (instr_get_prefixes(inst2) & PREFIX_SIGNIFICANT))
        return false;

    if (instr_get_isa_mode(inst1) != instr_get_isa_mode(inst2))
        return false;

    if (instr_get_predicate(inst1) != instr_get_predicate(inst2))
        return false;

    return true;
}

bool
instr_reads_memory(instr_t *instr)
{
    int a;
    opnd_t curop;
    int opc = instr_get_opcode(instr);

    if (opc_is_not_a_real_memory_load(opc))
        return false;

    for (a = 0; a < instr_num_srcs(instr); a++) {
        curop = instr_get_src(instr, a);
        if (opnd_is_memory_reference(curop)) {
            return true;
        }
    }
    return false;
}

bool
instr_writes_memory(instr_t *instr)
{
    int a;
    opnd_t curop;
    for (a = 0; a < instr_num_dsts(instr); a++) {
        curop = instr_get_dst(instr, a);
        if (opnd_is_memory_reference(curop)) {
            return true;
        }
    }
    return false;
}

#ifdef X86

bool
instr_zeroes_ymmh(instr_t *instr)
{
    int i;
    /* Our use of get_encoding_info() with no final PC specified works
     * as there are no encoding template choices involving reachability
     * which affect whether ymmh is zeroed.
     */
    const instr_info_t *info = get_encoding_info(instr);
    if (info == NULL)
        return false;
    /* Legacy (SSE) instructions always preserve top half of YMM.
     * Moreover, EVEX encoded instructions clear upper ZMM bits, but also
     * YMM bits if an XMM reg is used.
     */
    if (!TEST(REQUIRES_VEX, info->flags) && !TEST(REQUIRES_EVEX, info->flags))
        return false;

    /* Handle zeroall special case. */
    if (instr->opcode == OP_vzeroall)
        return true;

    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd_t opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(opnd) && reg_is_vector_simd(opnd_get_reg(opnd)) &&
            reg_is_strictly_xmm(opnd_get_reg(opnd)))
            return true;
    }
    return false;
}

bool
instr_zeroes_zmmh(instr_t *instr)
{
    int i;
    const instr_info_t *info = get_encoding_info(instr);
    if (info == NULL)
        return false;
    if (!TEST(REQUIRES_VEX, info->flags) && !TEST(REQUIRES_EVEX, info->flags))
        return false;
    /* Handle special cases, namely zeroupper and zeroall. */
    /* XXX: DR ir should actually have these two instructions have all SIMD vector regs
     * as operand even though they are implicit.
     */
    if (instr->opcode == OP_vzeroall || instr->opcode == OP_vzeroupper)
        return true;

    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd_t opnd = instr_get_dst(instr, i);
        if (opnd_is_reg(opnd) && reg_is_vector_simd(opnd_get_reg(opnd)) &&
            (reg_is_strictly_xmm(opnd_get_reg(opnd)) ||
             reg_is_strictly_ymm(opnd_get_reg(opnd))))
            return true;
    }
    return false;
}

bool
instr_is_xsave(instr_t *instr)
{
    int opcode = instr_get_opcode(instr); /* force decode */
    if (opcode == OP_xsave32 || opcode == OP_xsaveopt32 || opcode == OP_xsave64 ||
        opcode == OP_xsaveopt64 || opcode == OP_xsavec32 || opcode == OP_xsavec64)
        return true;
    return false;
}
#endif /* X86 */

/* PR 251479: support general re-relativization.  If INSTR_RIP_REL_VALID is set and
 * the raw bits are valid, instr->rip_rel_pos is assumed to hold the offset into the
 * instr of a 32-bit rip-relative displacement, which is used to re-relativize during
 * encoding.  We only use this for level 1-3 instrs, and we invalidate it if the raw
 * bits are modified at all.
 * For caching the encoded bytes of a Level 4 instr, instr_encode() sets
 * the rip_rel_pos field and flag without setting the raw bits valid:
 * private_instr_encode() then sets the raw bits, after examing the rip rel flag
 * by itself.  Thus, we must invalidate the rip rel flag when we invalidate
 * raw bits: we can't rely just on the raw bits invalidation.
 * There can only be one rip-relative operand per instruction.
 */
/* TODO i#4016: for AArchXX we don't have a large displacement on every reference.
 * Some have no disp at all, others have just 12 bits or smaller.
 * We need to come up with a strategy for handling encode-time re-relativization.
 * Xref copy_and_re_relativize_raw_instr().
 * For now, we do use some of these routines, but none that use the rip_rel_pos.
 */

#ifdef X86
bool
instr_rip_rel_valid(instr_t *instr)
{
    return instr_raw_bits_valid(instr) && TEST(INSTR_RIP_REL_VALID, instr->flags);
}

void
instr_set_rip_rel_valid(instr_t *instr, bool valid)
{
    if (valid)
        instr->flags |= INSTR_RIP_REL_VALID;
    else
        instr->flags &= ~INSTR_RIP_REL_VALID;
}

uint
instr_get_rip_rel_pos(instr_t *instr)
{
    return instr->rip_rel_pos;
}

void
instr_set_rip_rel_pos(instr_t *instr, uint pos)
{
    CLIENT_ASSERT_TRUNCATE(instr->rip_rel_pos, byte, pos,
                           "instr_set_rip_rel_pos: offs must be <= 256");
    instr->rip_rel_pos = (byte)pos;
    instr_set_rip_rel_valid(instr, true);
}
#endif /* X86 */

#ifdef X86
static bool
instr_has_rip_rel_instr_operand(instr_t *instr)
{
    /* XXX: See comment in instr_get_rel_target() about distinguishing data from
     * instr rip-rel operands.  We don't want to go so far as adding yet more
     * data plumbed through the decode_fast tables.
     * Perhaps we should instead break compatibility and have all these relative
     * target and operand index routines include instr operands, and update
     * mangle_rel_addr() to somehow distinguish instr on its own?
     * For now we get by with the simple check for a cti or xbegin.
     * No instruction has 2 rip-rel immeds so a direct cti must be instr.
     */
    return (instr_is_cti(instr) && !instr_is_mbr(instr)) ||
        instr_get_opcode(instr) == OP_xbegin;
}
#endif

bool
instr_get_rel_target(instr_t *instr, /*OUT*/ app_pc *target, bool data_only)
{
    if (!instr_valid(instr))
        return false;

    /* For PC operands we have to look at the high-level *before* rip_rel_pos, to
     * support decode_from_copy().  As documented, we ignore instr_t targets.
     */
    if (!data_only && instr_operands_valid(instr) && instr_num_srcs(instr) > 0 &&
        opnd_is_pc(instr_get_src(instr, 0))) {
        if (target != NULL)
            *target = opnd_get_pc(instr_get_src(instr, 0));
        return true;
    }

#ifdef X86
    /* PR 251479: we support rip-rel info in level 1 instrs */
    if (instr_rip_rel_valid(instr)) {
        int rip_rel_pos = instr_get_rip_rel_pos(instr);
        if (rip_rel_pos > 0) {
            if (data_only) {
                /* XXX: Distinguishing data from instr is a pain here b/c it might be
                 * during init (e.g., callback.c's copy_app_code()) and we can't
                 * easily do an up-decode (hence the separate "local" instr_t below).
                 * We do it partly for backward compatibility for external callers,
                 * but also for our own mangle_rel_addr().  Would it be cleaner some
                 * other way: breaking compat and not supporting data-only here and
                 * having mangle call instr_set_rip_rel_valid() for all cti's (and
                 * xbegin)?
                 */
                bool not_data = false;
                if (!instr_opcode_valid(instr) && get_thread_private_dcontext() == NULL) {
                    instr_t local;
                    instr_init(GLOBAL_DCONTEXT, &local);
                    if (decode_opcode(GLOBAL_DCONTEXT, instr_get_raw_bits(instr),
                                      &local) != NULL) {
                        not_data = instr_has_rip_rel_instr_operand(&local);
                    }
                    instr_free(GLOBAL_DCONTEXT, &local);
                } else
                    not_data = instr_has_rip_rel_instr_operand(instr);
                if (not_data)
                    return false;
            }
            if (target != NULL) {
                /* We only support non-4-byte rip-rel disps for 1-byte instr-final
                 * (jcc_short).
                 */
                if (rip_rel_pos + 1 == (int)instr->length) {
                    *target = instr->bytes + instr->length +
                        *((char *)(instr->bytes + rip_rel_pos));
                } else {
                    ASSERT(rip_rel_pos + 4 <= (int)instr->length);
                    *target = instr->bytes + instr->length +
                        *((int *)(instr->bytes + rip_rel_pos));
                }
            }
            return true;
        } else
            return false;
    }
#endif
#if defined(X64) || defined(ARM)
    int i;
    opnd_t curop;
    /* else go to level 3 operands */
    for (i = 0; i < instr_num_dsts(instr); i++) {
        curop = instr_get_dst(instr, i);
        IF_ARM_ELSE(
            {
                /* DR_REG_PC as an index register is not allowed */
                if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_PC) {
                    if (target != NULL) {
                        *target = opnd_get_disp(curop) +
                            decode_cur_pc(instr_get_app_pc(instr),
                                          instr_get_isa_mode(instr),
                                          instr_get_opcode(instr), instr);
                    }
                    return true;
                }
            },
            {
                if (opnd_is_rel_addr(curop)) {
                    if (target != NULL)
                        *target = opnd_get_addr(curop);
                    return true;
                }
            });
    }
    for (i = 0; i < instr_num_srcs(instr); i++) {
        curop = instr_get_src(instr, i);
        IF_ARM_ELSE(
            {
                /* DR_REG_PC as an index register is not allowed */
                if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_PC) {
                    if (target != NULL) {
                        *target = opnd_get_disp(curop) +
                            decode_cur_pc(instr_get_app_pc(instr),
                                          instr_get_isa_mode(instr),
                                          instr_get_opcode(instr), instr);
                    }
                    return true;
                }
            },
            {
                if (opnd_is_rel_addr(curop)) {
                    if (target != NULL)
                        *target = opnd_get_addr(curop);
                    return true;
                }
            });
    }
#endif
    return false;
}

bool
instr_get_rel_data_or_instr_target(instr_t *instr, /*OUT*/ app_pc *target)
{
    return instr_get_rel_target(instr, target, false /*all*/);
}

#if defined(X64) || defined(ARM)
bool
instr_get_rel_addr_target(instr_t *instr, /*OUT*/ app_pc *target)
{
    return instr_get_rel_target(instr, target, true /*data-only*/);
}

bool
instr_has_rel_addr_reference(instr_t *instr)
{
    return instr_get_rel_addr_target(instr, NULL);
}

int
instr_get_rel_addr_dst_idx(instr_t *instr)
{
    int i;
    opnd_t curop;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i = 0; i < instr_num_dsts(instr); i++) {
        curop = instr_get_dst(instr, i);
        IF_ARM_ELSE(
            {
                if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_PC)
                    return i;
            },
            {
                if (opnd_is_rel_addr(curop))
                    return i;
            });
    }
    return -1;
}

int
instr_get_rel_addr_src_idx(instr_t *instr)
{
    int i;
    opnd_t curop;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i = 0; i < instr_num_srcs(instr); i++) {
        curop = instr_get_src(instr, i);
        IF_ARM_ELSE(
            {
                if (opnd_is_base_disp(curop) && opnd_get_base(curop) == DR_REG_PC)
                    return i;
            },
            {
                if (opnd_is_rel_addr(curop))
                    return i;
            });
    }
    return -1;
}
#endif /* X64 || ARM */

bool
instr_is_our_mangling(instr_t *instr)
{
    return TEST(INSTR_OUR_MANGLING, instr->flags);
}

void
instr_set_our_mangling(instr_t *instr, bool ours)
{
    if (ours)
        instr->flags |= INSTR_OUR_MANGLING;
    else
        instr->flags &= ~INSTR_OUR_MANGLING;
}

bool
instr_is_our_mangling_epilogue(instr_t *instr)
{
    ASSERT(!TEST(INSTR_OUR_MANGLING_EPILOGUE, instr->flags) ||
           instr_is_our_mangling(instr));
    return TEST(INSTR_OUR_MANGLING_EPILOGUE, instr->flags);
}

void
instr_set_our_mangling_epilogue(instr_t *instr, bool epilogue)
{
    if (epilogue) {
        instr->flags |= INSTR_OUR_MANGLING_EPILOGUE;
    } else
        instr->flags &= ~INSTR_OUR_MANGLING_EPILOGUE;
}

instr_t *
instr_set_translation_mangling_epilogue(dcontext_t *dcontext, instrlist_t *ilist,
                                        instr_t *instr)
{
    if (instrlist_get_translation_target(ilist) != NULL) {
        int sz = decode_sizeof(dcontext, instrlist_get_translation_target(ilist),
                               NULL _IF_X86_64(NULL));
        instr_set_translation(instr, instrlist_get_translation_target(ilist) + sz);
    }
    instr_set_our_mangling_epilogue(instr, true);
    return instr;
}

/* Emulates instruction to find the address of the index-th memory operand.
 * Either or both OUT variables can be NULL.
 */
static bool
instr_compute_address_helper(instr_t *instr, priv_mcontext_t *mc, size_t mc_size,
                             dr_mcontext_flags_t mc_flags, uint index, OUT app_pc *addr,
                             OUT bool *is_write, OUT uint *pos)
{
    /* for string instr, even w/ rep prefix, assume want value at point of
     * register snapshot passed in
     */
    int i;
    opnd_t curop = { 0 };
    int memcount = -1;
    bool write = false;
    bool have_addr = false;
    /* We allow not selecting xmm fields since clients may legitimately
     * emulate a memref w/ just GPRs
     */
    CLIENT_ASSERT(TESTALL(DR_MC_CONTROL | DR_MC_INTEGER, mc_flags),
                  "dr_mcontext_t.flags must include DR_MC_CONTROL and DR_MC_INTEGER");
    for (i = 0; i < instr_num_dsts(instr); i++) {
        curop = instr_get_dst(instr, i);
        if (opnd_is_memory_reference(curop)) {
            if (opnd_is_vsib(curop)) {
#ifdef X86
                if (instr_compute_address_VSIB(instr, mc, mc_size, mc_flags, curop, index,
                                               &have_addr, addr, &write)) {
                    CLIENT_ASSERT(
                        write,
                        "VSIB found in destination but instruction is not a scatter");
                    break;
                } else {
                    return false;
                }
#else
                CLIENT_ASSERT(false, "VSIB should be x86-only");
#endif
            }
            memcount++;
            if (memcount == (int)index) {
                write = true;
                break;
            }
        }
    }
    if (!write && memcount != (int)index &&
        /* lea has a mem_ref source operand, but doesn't actually read */
        !opc_is_not_a_real_memory_load(instr_get_opcode(instr))) {
        for (i = 0; i < instr_num_srcs(instr); i++) {
            curop = instr_get_src(instr, i);
            if (opnd_is_memory_reference(curop)) {
                if (opnd_is_vsib(curop)) {
#ifdef X86
                    if (instr_compute_address_VSIB(instr, mc, mc_size, mc_flags, curop,
                                                   index, &have_addr, addr, &write))
                        break;
                    else
                        return false;
#else
                    CLIENT_ASSERT(false, "VSIB should be x86-only");
#endif
                }
                memcount++;
                if (memcount == (int)index)
                    break;
            }
        }
    }
    if (!have_addr) {
        if (memcount != (int)index)
            return false;
        if (addr != NULL)
            *addr = opnd_compute_address_priv(curop, mc);
    }
    if (is_write != NULL)
        *is_write = write;
    if (pos != 0)
        *pos = i;
    return true;
}

bool
instr_compute_address_ex_priv(instr_t *instr, priv_mcontext_t *mc, uint index,
                              OUT app_pc *addr, OUT bool *is_write, OUT uint *pos)
{
    return instr_compute_address_helper(instr, mc, sizeof(*mc), DR_MC_ALL, index, addr,
                                        is_write, pos);
}

DR_API
bool
instr_compute_address_ex(instr_t *instr, dr_mcontext_t *mc, uint index, OUT app_pc *addr,
                         OUT bool *is_write)
{
    return instr_compute_address_helper(instr, dr_mcontext_as_priv_mcontext(mc), mc->size,
                                        mc->flags, index, addr, is_write, NULL);
}

/* i#682: add pos so that the caller knows which opnd is used. */
DR_API
bool
instr_compute_address_ex_pos(instr_t *instr, dr_mcontext_t *mc, uint index,
                             OUT app_pc *addr, OUT bool *is_write, OUT uint *pos)
{
    return instr_compute_address_helper(instr, dr_mcontext_as_priv_mcontext(mc), mc->size,
                                        mc->flags, index, addr, is_write, pos);
}

/* Returns NULL if none of instr's operands is a memory reference.
 * Otherwise, returns the effective address of the first memory operand
 * when the operands are considered in this order: destinations and then
 * sources.  The address is computed using the passed-in registers.
 */
app_pc
instr_compute_address_priv(instr_t *instr, priv_mcontext_t *mc)
{
    app_pc addr;
    if (!instr_compute_address_ex_priv(instr, mc, 0, &addr, NULL, NULL))
        return NULL;
    return addr;
}

DR_API
app_pc
instr_compute_address(instr_t *instr, dr_mcontext_t *mc)
{
    app_pc addr;
    if (!instr_compute_address_ex(instr, mc, 0, &addr, NULL))
        return NULL;
    return addr;
}

/* Calculates the size, in bytes, of the memory read or write of instr
 * If instr does not reference memory, or is invalid, returns 0
 */
uint
instr_memory_reference_size(instr_t *instr)
{
    int i;
    if (!instr_valid(instr))
        return 0;
    for (i = 0; i < instr_num_dsts(instr); i++) {
        if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
            return opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr, i)));
        }
    }
    for (i = 0; i < instr_num_srcs(instr); i++) {
        if (opnd_is_memory_reference(instr_get_src(instr, i))) {
            return opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i)));
        }
    }
    return 0;
}

/* Calculates the size, in bytes, of the memory read or write of
 * the instr at pc.
 * Returns the pc of the following instr.
 * If the instr at pc does not reference memory, or is invalid,
 * returns NULL.
 */
app_pc
decode_memory_reference_size(void *drcontext, app_pc pc, uint *size_in_bytes)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    app_pc next_pc;
    instr_t instr;
    instr_init(dcontext, &instr);
    next_pc = decode(dcontext, pc, &instr);
    if (!instr_valid(&instr))
        return NULL;
    CLIENT_ASSERT(size_in_bytes != NULL, "decode_memory_reference_size: passed NULL");
    *size_in_bytes = instr_memory_reference_size(&instr);
    instr_free(dcontext, &instr);
    return next_pc;
}

DR_API
dr_instr_label_data_t *
instr_get_label_data_area(instr_t *instr)
{
    CLIENT_ASSERT(instr != NULL, "invalid arg");
    if (instr_is_label(instr))
        return &instr->label_data;
    else
        return NULL;
}

DR_API
/* return the taken target pc of the (direct branch) inst */
app_pc
instr_get_branch_target_pc(instr_t *cti_instr)
{
    CLIENT_ASSERT(opnd_is_pc(instr_get_target(cti_instr)),
                  "instr_branch_target_pc: target not pc");
    return opnd_get_pc(instr_get_target(cti_instr));
}

DR_API
/* set the taken target pc of the (direct branch) inst */
void
instr_set_branch_target_pc(instr_t *cti_instr, app_pc pc)
{
    opnd_t op = opnd_create_pc(pc);
    instr_set_target(cti_instr, op);
}

bool
instr_is_call(instr_t *instr)
{
    instr_get_opcode(instr); /* force decode */
    return instr_is_call_arch(instr);
}

bool
instr_is_cbr(instr_t *instr)
{
    instr_get_opcode(instr); /* force decode */
    return instr_is_cbr_arch(instr);
}

bool
instr_is_mbr(instr_t *instr)
{
    instr_get_opcode(instr); /* force decode */
    return instr_is_mbr_arch(instr);
}

bool
instr_is_ubr(instr_t *instr)
{
    instr_get_opcode(instr); /* force decode */
    return instr_is_ubr_arch(instr);
}

/* An exit CTI is a control-transfer instruction whose target
 * is a pc (and not an instr_t pointer).  This routine assumes
 * that no other input operands exist in a CTI.
 * An undecoded instr cannot be an exit cti.
 * This routine does NOT try to decode an opcode in a Level 1 or Level
 * 0 routine, and can thus be called on Level 0 routines.
 */
bool
instr_is_exit_cti(instr_t *instr)
{
    if (!instr_operands_valid(instr) || /* implies !opcode_valid */
        instr_is_meta(instr))
        return false;
    /* The _arch versions assume the opcode is already valid, avoiding
     * the conditional decode in instr_get_opcode().
     */
    if (instr_is_ubr_arch(instr) || instr_is_cbr_arch(instr)) {
        /* far pc should only happen for mangle's call to here */
        return opnd_is_pc(instr_get_target(instr));
    }
    return false;
}

bool
instr_is_cti(instr_t *instr) /* any control-transfer instruction */
{
    instr_get_opcode(instr); /* force opcode decode, just once */
    return (instr_is_cbr_arch(instr) || instr_is_ubr_arch(instr) ||
            instr_is_mbr_arch(instr) || instr_is_call_arch(instr));
}

int
instr_get_interrupt_number(instr_t *instr)
{
    CLIENT_ASSERT(instr_get_opcode(instr) ==
                      IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc)),
                  "instr_get_interrupt_number: instr not interrupt");
    if (instr_operands_valid(instr)) {
        ptr_int_t val = opnd_get_immed_int(instr_get_src(instr, 0));
        /* undo the sign extension.  prob return value shouldn't be signed but
         * too late to bother changing that.
         */
        CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_sbyte(val), "invalid interrupt number");
        return (int)(byte)val;
    } else if (instr_raw_bits_valid(instr)) {
        /* widen as unsigned */
        return (int)(uint)instr_get_raw_byte(instr, 1);
    } else {
        CLIENT_ASSERT(false, "instr_get_interrupt_number: invalid instr");
        return 0;
    }
}

/* Returns true iff instr is a label meta-instruction */
bool
instr_is_label(instr_t *instr)
{
    return instr_opcode_valid(instr) && instr_get_opcode(instr) == OP_LABEL;
}

bool
instr_uses_fp_reg(instr_t *instr)
{
    int a;
    opnd_t curop;
    for (a = 0; a < instr_num_dsts(instr); a++) {
        curop = instr_get_dst(instr, a);
        if (opnd_is_reg(curop) && reg_is_fp(opnd_get_reg(curop)))
            return true;
        else if (opnd_is_memory_reference(curop)) {
            if (reg_is_fp(opnd_get_base(curop)))
                return true;
            else if (reg_is_fp(opnd_get_index(curop)))
                return true;
        }
    }

    for (a = 0; a < instr_num_srcs(instr); a++) {
        curop = instr_get_src(instr, a);
        if (opnd_is_reg(curop) && reg_is_fp(opnd_get_reg(curop)))
            return true;
        else if (opnd_is_memory_reference(curop)) {
            if (reg_is_fp(opnd_get_base(curop)))
                return true;
            else if (reg_is_fp(opnd_get_index(curop)))
                return true;
        }
    }
    return false;
}

/* We place these here rather than in mangle_shared.c to avoid the work of
 * linking mangle_shared.c into drdecodelib.
 */
instr_t *
convert_to_near_rel_meta(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    return convert_to_near_rel_arch(dcontext, ilist, instr);
}

void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr)
{
    convert_to_near_rel_arch(dcontext, NULL, instr);
}

instr_t *
instr_convert_short_meta_jmp_to_long(void *drcontext, instrlist_t *ilist, instr_t *instr)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    /* PR 266292: we convert to a sequence of separate meta instrs for jecxz, etc. */
    CLIENT_ASSERT(instr_is_meta(instr),
                  "instr_convert_short_meta_jmp_to_long: instr is not meta");
    CLIENT_ASSERT(instr_is_cti_short(instr),
                  "instr_convert_short_meta_jmp_to_long: instr is not a short cti");
    if (instr_is_app(instr) || !instr_is_cti_short(instr))
        return instr;
    return convert_to_near_rel_meta(dcontext, ilist, instr);
}

/***********************************************************************
 * instr_t creation routines
 * To use 16-bit data sizes, must call set_prefix after creating instr
 * To support this, all relevant registers must be of eAX form!
 * FIXME: how do that?
 * will an all-operand replacement work, or do some instrs have some
 * var-size regs but some const-size also?
 *
 * XXX: what if want eflags or modrm info on constructed instr?!?
 *
 * fld pushes onto top of stack, call that writing to ST0 or ST7?
 * f*p pops the stack -- not modeled at all!
 * should floating point constants be doubles, not floats?!?
 *
 * opcode complaints:
 * OP_imm vs. OP_st
 * OP_ret: build routines have to separate ret_imm and ret_far_imm
 * others, see FIXME's in instr_create_api.h
 */

instr_t *
instr_create_0dst_0src(void *drcontext, int opcode)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 0, 0);
    return in;
}

instr_t *
instr_create_0dst_1src(void *drcontext, int opcode, opnd_t src)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 0, 1);
    instr_set_src(in, 0, src);
    return in;
}

instr_t *
instr_create_0dst_2src(void *drcontext, int opcode, opnd_t src1, opnd_t src2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 0, 2);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    return in;
}

instr_t *
instr_create_0dst_3src(void *drcontext, int opcode, opnd_t src1, opnd_t src2, opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 0, 3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_0dst_4src(void *drcontext, int opcode, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 0, 4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_1dst_0src(void *drcontext, int opcode, opnd_t dst)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 0);
    instr_set_dst(in, 0, dst);
    return in;
}

instr_t *
instr_create_1dst_1src(void *drcontext, int opcode, opnd_t dst, opnd_t src)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 1);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src);
    return in;
}

instr_t *
instr_create_1dst_2src(void *drcontext, int opcode, opnd_t dst, opnd_t src1, opnd_t src2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 2);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    return in;
}

instr_t *
instr_create_1dst_3src(void *drcontext, int opcode, opnd_t dst, opnd_t src1, opnd_t src2,
                       opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 3);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_1dst_4src(void *drcontext, int opcode, opnd_t dst, opnd_t src1, opnd_t src2,
                       opnd_t src3, opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 4);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_1dst_5src(void *drcontext, int opcode, opnd_t dst, opnd_t src1, opnd_t src2,
                       opnd_t src3, opnd_t src4, opnd_t src5)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 5);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    return in;
}

instr_t *
instr_create_1dst_6src(void *drcontext, int opcode, opnd_t dst, opnd_t src1, opnd_t src2,
                       opnd_t src3, opnd_t src4, opnd_t src5, opnd_t src6)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 1, 6);
    instr_set_dst(in, 0, dst);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    instr_set_src(in, 5, src6);
    return in;
}

instr_t *
instr_create_2dst_0src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 0);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    return in;
}

instr_t *
instr_create_2dst_1src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t src)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 1);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_src(in, 0, src);
    return in;
}

instr_t *
instr_create_2dst_2src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t src1,
                       opnd_t src2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 2);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    return in;
}

instr_t *
instr_create_2dst_3src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t src1,
                       opnd_t src2, opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 3);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_2dst_4src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t src1,
                       opnd_t src2, opnd_t src3, opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 4);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_2dst_5src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t src1,
                       opnd_t src2, opnd_t src3, opnd_t src4, opnd_t src5)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 2, 5);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    return in;
}

instr_t *
instr_create_3dst_0src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 0);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    return in;
}

instr_t *
instr_create_3dst_1src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 1);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    return in;
}

instr_t *
instr_create_3dst_2src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 2);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    return in;
}

instr_t *
instr_create_3dst_3src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 3);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_3dst_4src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 4);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_3dst_5src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4, opnd_t src5)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 5);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    return in;
}

instr_t *

instr_create_3dst_6src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4, opnd_t src5,
                       opnd_t src6)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 3, 6);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    instr_set_src(in, 5, src6);
    return in;
}

instr_t *
instr_create_4dst_1src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 1);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src);
    return in;
}

instr_t *
instr_create_4dst_2src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 2);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    return in;
}

instr_t *
instr_create_4dst_3src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 3);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_4dst_4src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 4);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_4dst_5src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4,
                       opnd_t src5)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 5);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    return in;
}

instr_t *
instr_create_4dst_6src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4,
                       opnd_t src5, opnd_t src6)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 6);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    instr_set_src(in, 5, src6);
    return in;
}

instr_t *
instr_create_4dst_7src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t src1, opnd_t src2, opnd_t src3, opnd_t src4,
                       opnd_t src5, opnd_t src6, opnd_t src7)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 4, 7);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    instr_set_src(in, 5, src6);
    instr_set_src(in, 6, src7);
    return in;
}

instr_t *
instr_create_5dst_3src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t dst5, opnd_t src1, opnd_t src2, opnd_t src3)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 5, 3);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_dst(in, 4, dst5);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    return in;
}

instr_t *
instr_create_5dst_4src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t dst5, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 5, 4);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_dst(in, 4, dst5);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    return in;
}

instr_t *
instr_create_5dst_5src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t dst5, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 5, 5);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_dst(in, 4, dst5);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    return in;
}

instr_t *
instr_create_5dst_8src(void *drcontext, int opcode, opnd_t dst1, opnd_t dst2, opnd_t dst3,
                       opnd_t dst4, opnd_t dst5, opnd_t src1, opnd_t src2, opnd_t src3,
                       opnd_t src4, opnd_t src5, opnd_t src6, opnd_t src7, opnd_t src8)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *in = instr_build(dcontext, opcode, 5, 8);
    instr_set_dst(in, 0, dst1);
    instr_set_dst(in, 1, dst2);
    instr_set_dst(in, 2, dst3);
    instr_set_dst(in, 3, dst4);
    instr_set_dst(in, 4, dst5);
    instr_set_src(in, 0, src1);
    instr_set_src(in, 1, src2);
    instr_set_src(in, 2, src3);
    instr_set_src(in, 3, src4);
    instr_set_src(in, 4, src5);
    instr_set_src(in, 5, src6);
    instr_set_src(in, 6, src7);
    instr_set_src(in, 7, src8);
    return in;
}

instr_t *
instr_create_Ndst_Msrc_varsrc(void *drcontext, int opcode, uint fixed_dsts,
                              uint fixed_srcs, uint var_srcs, uint var_ord, ...)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    va_list ap;
    instr_t *in = instr_build(dcontext, opcode, fixed_dsts, fixed_srcs + var_srcs);
    uint i;
    DEBUG_EXT_DECLARE(reg_id_t prev_reg = REG_NULL;)
    bool check_order;
    va_start(ap, var_ord);
    for (i = 0; i < fixed_dsts; i++)
        instr_set_dst(in, i, va_arg(ap, opnd_t));
    for (i = 0; i < MIN(var_ord, fixed_srcs); i++)
        instr_set_src(in, i, va_arg(ap, opnd_t));
    for (i = var_ord; i < fixed_srcs; i++)
        instr_set_src(in, var_srcs + i, va_arg(ap, opnd_t));
    /* we require regs in reglist are stored in order for easy split if necessary */
    check_order = IF_ARM_ELSE(true, false);
    for (i = 0; i < var_srcs; i++) {
        opnd_t opnd = va_arg(ap, opnd_t);
        /* assuming non-reg opnds (if any) are in the fixed positon */
        CLIENT_ASSERT(!check_order ||
                          (opnd_is_reg(opnd) && opnd_get_reg(opnd) > prev_reg),
                      "instr_create_Ndst_Msrc_varsrc: wrong register order in reglist");
        instr_set_src(in, var_ord + i, opnd_add_flags(opnd, DR_OPND_IN_LIST));
        if (check_order)
            DEBUG_EXT_DECLARE(prev_reg = opnd_get_reg(opnd));
    }
    va_end(ap);
    return in;
}

instr_t *
instr_create_Ndst_Msrc_vardst(void *drcontext, int opcode, uint fixed_dsts,
                              uint fixed_srcs, uint var_dsts, uint var_ord, ...)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    va_list ap;
    instr_t *in = instr_build(dcontext, opcode, fixed_dsts + var_dsts, fixed_srcs);
    uint i;
    DEBUG_EXT_DECLARE(reg_id_t prev_reg = REG_NULL;)
    bool check_order;
    va_start(ap, var_ord);
    for (i = 0; i < MIN(var_ord, fixed_dsts); i++)
        instr_set_dst(in, i, va_arg(ap, opnd_t));
    for (i = var_ord; i < fixed_dsts; i++)
        instr_set_dst(in, var_dsts + i, va_arg(ap, opnd_t));
    for (i = 0; i < fixed_srcs; i++)
        instr_set_src(in, i, va_arg(ap, opnd_t));
    /* we require regs in reglist are stored in order for easy split if necessary */
    check_order = IF_ARM_ELSE(true, false);
    for (i = 0; i < var_dsts; i++) {
        opnd_t opnd = va_arg(ap, opnd_t);
        /* assuming non-reg opnds (if any) are in the fixed positon */
        CLIENT_ASSERT(!check_order ||
                          (opnd_is_reg(opnd) && opnd_get_reg(opnd) > prev_reg),
                      "instr_create_Ndst_Msrc_vardst: wrong register order in reglist");
        instr_set_dst(in, var_ord + i, opnd_add_flags(opnd, DR_OPND_IN_LIST));
        if (check_order)
            DEBUG_EXT_DECLARE(prev_reg = opnd_get_reg(opnd));
    }
    va_end(ap);
    return in;
}

/****************************************************************************/
/* build instructions from raw bits
 * convention: give them OP_UNDECODED opcodes
 */

instr_t *
instr_create_raw_1byte(dcontext_t *dcontext, byte byte1)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 1);
    instr_set_raw_byte(in, 0, byte1);
    return in;
}

instr_t *
instr_create_raw_2bytes(dcontext_t *dcontext, byte byte1, byte byte2)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 2);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    return in;
}

instr_t *
instr_create_raw_3bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 3);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    return in;
}

instr_t *
instr_create_raw_4bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 4);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    instr_set_raw_byte(in, 3, byte4);
    return in;
}

instr_t *
instr_create_raw_5bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 5);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    instr_set_raw_byte(in, 3, byte4);
    instr_set_raw_byte(in, 4, byte5);
    return in;
}

instr_t *
instr_create_raw_6bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 6);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    instr_set_raw_byte(in, 3, byte4);
    instr_set_raw_byte(in, 4, byte5);
    instr_set_raw_byte(in, 5, byte6);
    return in;
}

instr_t *
instr_create_raw_7bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6, byte byte7)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 7);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    instr_set_raw_byte(in, 3, byte4);
    instr_set_raw_byte(in, 4, byte5);
    instr_set_raw_byte(in, 5, byte6);
    instr_set_raw_byte(in, 6, byte7);
    return in;
}

instr_t *
instr_create_raw_8bytes(dcontext_t *dcontext, byte byte1, byte byte2, byte byte3,
                        byte byte4, byte byte5, byte byte6, byte byte7, byte byte8)
{
    instr_t *in = instr_build_bits(dcontext, OP_UNDECODED, 8);
    instr_set_raw_byte(in, 0, byte1);
    instr_set_raw_byte(in, 1, byte2);
    instr_set_raw_byte(in, 2, byte3);
    instr_set_raw_byte(in, 3, byte4);
    instr_set_raw_byte(in, 4, byte5);
    instr_set_raw_byte(in, 5, byte6);
    instr_set_raw_byte(in, 6, byte7);
    instr_set_raw_byte(in, 7, byte8);
    return in;
}

#ifndef STANDALONE_DECODER
/****************************************************************************/
/* dcontext convenience routines */

instr_t *
instr_create_restore_from_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs)
{
    opnd_t memopnd = opnd_create_dcontext_field(dcontext, offs);
    /* use movd for xmm/mmx */
    if (reg_is_xmm(reg) || reg_is_mmx(reg))
        return XINST_CREATE_load_simd(dcontext, opnd_create_reg(reg), memopnd);
    else
        return XINST_CREATE_load(dcontext, opnd_create_reg(reg), memopnd);
}

instr_t *
instr_create_save_to_dcontext(dcontext_t *dcontext, reg_id_t reg, int offs)
{
    opnd_t memopnd = opnd_create_dcontext_field(dcontext, offs);
    CLIENT_ASSERT(dcontext != GLOBAL_DCONTEXT,
                  "instr_create_save_to_dcontext: invalid dcontext");
    /* use movd for xmm/mmx */
    if (reg_is_xmm(reg) || reg_is_mmx(reg))
        return XINST_CREATE_store_simd(dcontext, memopnd, opnd_create_reg(reg));
    else
        return XINST_CREATE_store(dcontext, memopnd, opnd_create_reg(reg));
}

/* Use basereg==REG_NULL to get default (xdi, or xsi for upcontext)
 * Auto-magically picks the mem opnd size to match reg if it's a GPR.
 */
instr_t *
instr_create_restore_from_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, reg_id_t reg,
                                     int offs)
{
    /* use movd for xmm/mmx, and OPSZ_PTR */
    if (reg_is_xmm(reg) || reg_is_mmx(reg)) {
        opnd_t memopnd = opnd_create_dcontext_field_via_reg(dcontext, basereg, offs);
        return XINST_CREATE_load_simd(dcontext, opnd_create_reg(reg), memopnd);
    } else {
        opnd_t memopnd = opnd_create_dcontext_field_via_reg_sz(dcontext, basereg, offs,
                                                               reg_get_size(reg));
        return XINST_CREATE_load(dcontext, opnd_create_reg(reg), memopnd);
    }
}

/* Use basereg==REG_NULL to get default (xdi, or xsi for upcontext)
 * Auto-magically picks the mem opnd size to match reg if it's a GPR.
 */
instr_t *
instr_create_save_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, reg_id_t reg,
                                int offs)
{
    /* use movd for xmm/mmx, and OPSZ_PTR */
    if (reg_is_xmm(reg) || reg_is_mmx(reg)) {
        opnd_t memopnd = opnd_create_dcontext_field_via_reg(dcontext, basereg, offs);
        return XINST_CREATE_store_simd(dcontext, memopnd, opnd_create_reg(reg));
    } else {
        opnd_t memopnd = opnd_create_dcontext_field_via_reg_sz(dcontext, basereg, offs,
                                                               reg_get_size(reg));
        return XINST_CREATE_store(dcontext, memopnd, opnd_create_reg(reg));
    }
}

static instr_t *
instr_create_save_immedN_to_dcontext(dcontext_t *dcontext, opnd_size_t sz,
                                     opnd_t immed_op, int offs)
{
    opnd_t memopnd = opnd_create_dcontext_field_sz(dcontext, offs, sz);
    /* PR 244737: thread-private scratch space needs to fixed for x64 */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /* There is no immed to mem instr on ARM/AArch64. */
    IF_AARCHXX(ASSERT_NOT_IMPLEMENTED(false));
    return XINST_CREATE_store(dcontext, memopnd, immed_op);
}

instr_t *
instr_create_save_immed32_to_dcontext(dcontext_t *dcontext, int immed, int offs)
{
    return instr_create_save_immedN_to_dcontext(dcontext, OPSZ_4,
                                                OPND_CREATE_INT32(immed), offs);
}

instr_t *
instr_create_save_immed16_to_dcontext(dcontext_t *dcontext, int immed, int offs)
{
    return instr_create_save_immedN_to_dcontext(dcontext, OPSZ_2,
                                                OPND_CREATE_INT16(immed), offs);
}

instr_t *
instr_create_save_immed8_to_dcontext(dcontext_t *dcontext, int immed, int offs)
{
    return instr_create_save_immedN_to_dcontext(dcontext, OPSZ_1, OPND_CREATE_INT8(immed),
                                                offs);
}

instr_t *
instr_create_save_immed_to_dc_via_reg(dcontext_t *dcontext, reg_id_t basereg, int offs,
                                      ptr_int_t immed, opnd_size_t sz)
{
    opnd_t memopnd = opnd_create_dcontext_field_via_reg_sz(dcontext, basereg, offs, sz);
    ASSERT(sz == OPSZ_1 || sz == OPSZ_2 || sz == OPSZ_4);
    /* There is no immed to mem instr on ARM or AArch64. */
    IF_NOT_X86(ASSERT_NOT_IMPLEMENTED(false));
    return XINST_CREATE_store(dcontext, memopnd, opnd_create_immed_int(immed, sz));
}

instr_t *
instr_create_jump_via_dcontext(dcontext_t *dcontext, int offs)
{
#    ifdef AARCH64
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
#    else
    opnd_t memopnd = opnd_create_dcontext_field(dcontext, offs);
    return XINST_CREATE_jump_mem(dcontext, memopnd);
#    endif
}

/* there is no corresponding save routine since we no longer support
 * keeping state on the stack while code other than our own is running
 * (in the same thread)
 */
instr_t *
instr_create_restore_dynamo_stack(dcontext_t *dcontext)
{
    return instr_create_restore_from_dcontext(dcontext, REG_XSP, DSTACK_OFFSET);
}

/* make sure to keep in sync w/ emit_utils.c's insert_spill_or_restore() */
bool
instr_raw_is_tls_spill(byte *pc, reg_id_t reg, ushort offs)
{
#    ifdef X86
    ASSERT_NOT_IMPLEMENTED(reg != REG_XAX);
#        ifdef X64
    /* match insert_jmp_to_ibl */
    if (*pc == TLS_SEG_OPCODE &&
        *(pc + 1) == (REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG) &&
        *(pc + 2) == MOV_REG2MEM_OPCODE &&
        /* 0x1c for ebx, 0x0c for ecx, 0x04 for eax */
        *(pc + 3) == MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), 4 /*rm*/) &&
        *(pc + 4) == 0x25 && *((uint *)(pc + 5)) == (uint)os_tls_offset(offs))
        return true;
        /* we also check for 32-bit.  we could take in flags and only check for one
         * version, but we're not worried about false positives.
         */
#        endif
    /* looking for:   67 64 89 1e e4 0e    addr16 mov    %ebx -> %fs:0xee4   */
    /* ASSUMPTION: when addr16 prefix is used, prefix order is fixed */
    return (*pc == ADDR_PREFIX_OPCODE && *(pc + 1) == TLS_SEG_OPCODE &&
            *(pc + 2) == MOV_REG2MEM_OPCODE &&
            /* 0x1e for ebx, 0x0e for ecx, 0x06 for eax */
            *(pc + 3) == MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), 6 /*rm*/) &&
            *((ushort *)(pc + 4)) == os_tls_offset(offs)) ||
        /* PR 209709: allow for no addr16 prefix */
        (*pc == TLS_SEG_OPCODE && *(pc + 1) == MOV_REG2MEM_OPCODE &&
         /* 0x1e for ebx, 0x0e for ecx, 0x06 for eax */
         *(pc + 2) == MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), 6 /*rm*/) &&
         *((uint *)(pc + 4)) == os_tls_offset(offs));
#    elif defined(AARCHXX)
    /* FIXME i#1551, i#1569: NYI on ARM/AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    elif defined(RISCV64)
    /* FIXME i#3544: Not implemented. */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    endif /* X86/ARM/RISCV64 */
}

/* this routine may upgrade a level 1 instr */
static bool
instr_check_tls_spill_restore(instr_t *instr, bool *spill, reg_id_t *reg, int *offs)
{
    opnd_t regop, memop;
    CLIENT_ASSERT(instr != NULL,
                  "internal error: tls spill/restore check: NULL argument");
    if (instr_get_opcode(instr) == OP_store) {
        regop = instr_get_src(instr, 0);
        memop = instr_get_dst(instr, 0);
        if (spill != NULL)
            *spill = true;
    } else if (instr_get_opcode(instr) == OP_load) {
        regop = instr_get_dst(instr, 0);
        memop = instr_get_src(instr, 0);
        if (spill != NULL)
            *spill = false;
#    ifdef X86
    } else if (instr_get_opcode(instr) == OP_xchg) {
        /* we use xchg to restore in dr_insert_mbr_instrumentation */
        regop = instr_get_src(instr, 0);
        memop = instr_get_dst(instr, 0);
        if (spill != NULL)
            *spill = false;
#    endif
    } else
        return false;
    if (opnd_is_reg(regop) &&
#    ifdef X86
        opnd_is_far_base_disp(memop) && opnd_get_segment(memop) == SEG_TLS &&
        opnd_is_abs_base_disp(memop)
#    elif defined(AARCHXX)
        opnd_is_base_disp(memop) && opnd_get_base(memop) == dr_reg_stolen &&
        opnd_get_index(memop) == DR_REG_NULL
#    elif defined(RISCV64)
        /* FIXME i#3544: Check if valid. */
        opnd_is_base_disp(memop) && opnd_get_base(memop) == DR_REG_TP &&
        opnd_get_index(memop) == DR_REG_NULL
#    endif
    ) {
        if (reg != NULL)
            *reg = opnd_get_reg(regop);
        if (offs != NULL)
            *offs = opnd_get_disp(memop);
        return true;
    }
    return false;
}

/* if instr is level 1, does not upgrade it and instead looks at raw bits,
 * to support identification w/o ruining level 0 in decode_fragment, etc.
 */
bool
instr_is_tls_spill(instr_t *instr, reg_id_t reg, ushort offs)
{
    reg_id_t check_reg = REG_NULL; /* init to satisfy some compilers */
    int check_disp = 0;            /* init to satisfy some compilers */
    bool spill;
    return (instr_check_tls_spill_restore(instr, &spill, &check_reg, &check_disp) &&
            spill && check_reg == reg && check_disp == os_tls_offset(offs));
}

/* if instr is level 1, does not upgrade it and instead looks at raw bits,
 * to support identification w/o ruining level 0 in decode_fragment, etc.
 */
bool
instr_is_tls_restore(instr_t *instr, reg_id_t reg, ushort offs)
{
    reg_id_t check_reg = REG_NULL; /* init to satisfy some compilers */
    int check_disp = 0;            /* init to satisfy some compilers */
    bool spill;
    return (instr_check_tls_spill_restore(instr, &spill, &check_reg, &check_disp) &&
            !spill && (reg == REG_NULL || check_reg == reg) &&
            check_disp == os_tls_offset(offs));
}

/* if instr is level 1, does not upgrade it and instead looks at raw bits,
 * to support identification w/o ruining level 0 in decode_fragment, etc.
 */
bool
instr_is_tls_xcx_spill(instr_t *instr)
{
#    ifdef X86
    if (instr_raw_bits_valid(instr)) {
        /* avoid upgrading instr */
        return instr_raw_is_tls_spill(instr_get_raw_bits(instr), REG_ECX,
                                      MANGLE_XCX_SPILL_SLOT);
    } else
        return instr_is_tls_spill(instr, REG_ECX, MANGLE_XCX_SPILL_SLOT);
#    elif defined(AARCHXX)
    /* FIXME i#1551, i#1569: NYI on ARM/AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    endif
}

/* this routine may upgrade a level 1 instr */
static bool
instr_check_mcontext_spill_restore(dcontext_t *dcontext, instr_t *instr, bool *spill,
                                   reg_id_t *reg, int *offs)
{
#    ifdef X64
    /* PR 244737: we always use tls for x64 */
    return false;
#    else
    opnd_t regop, memop;
    if (instr_get_opcode(instr) == OP_store) {
        regop = instr_get_src(instr, 0);
        memop = instr_get_dst(instr, 0);
        if (spill != NULL)
            *spill = true;
    } else if (instr_get_opcode(instr) == OP_load) {
        regop = instr_get_dst(instr, 0);
        memop = instr_get_src(instr, 0);
        if (spill != NULL)
            *spill = false;
#        ifdef X86
    } else if (instr_get_opcode(instr) == OP_xchg) {
        /* we use xchg to restore in dr_insert_mbr_instrumentation */
        regop = instr_get_src(instr, 0);
        memop = instr_get_dst(instr, 0);
        if (spill != NULL)
            *spill = false;
#        endif /* X86 */
    } else
        return false;
    if (opnd_is_near_base_disp(memop) && opnd_is_abs_base_disp(memop) &&
        opnd_is_reg(regop)) {
        byte *pc = (byte *)opnd_get_disp(memop);
        byte *mc = (byte *)get_mcontext(dcontext);
        if (pc >= mc && pc < mc + sizeof(priv_mcontext_t)) {
            if (reg != NULL)
                *reg = opnd_get_reg(regop);
            if (offs != NULL)
                *offs = pc - (byte *)dcontext;
            return true;
        }
    }
    return false;
#    endif
}

static bool
instr_is_reg_spill_or_restore_ex(void *drcontext, instr_t *instr, bool DR_only, bool *tls,
                                 bool *spill, reg_id_t *reg, uint *offs_out)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    int check_disp = 0; /* init to satisfy some compilers */
    reg_id_t myreg;
    CLIENT_ASSERT(instr != NULL, "invalid NULL argument");
    if (reg == NULL)
        reg = &myreg;
    if (instr_check_tls_spill_restore(instr, spill, reg, &check_disp)) {
        /* We do not want to count an mcontext base load as a reg spill/restore. */
        if ((!DR_only && check_disp != os_tls_offset((ushort)TLS_DCONTEXT_SLOT)) ||
            (reg_spill_tls_offs(*reg) != -1 &&
             /* Mangling may choose to spill registers to a not natural tls offset,
              * e.g. rip-rel mangling will, if rax is used by the instruction. We
              * allow for all possible internal DR slots to recognize a DR spill.
              */
             (check_disp == os_tls_offset((ushort)TLS_REG0_SLOT) ||
              check_disp == os_tls_offset((ushort)TLS_REG1_SLOT) ||
              check_disp == os_tls_offset((ushort)TLS_REG2_SLOT) ||
              check_disp == os_tls_offset((ushort)TLS_REG3_SLOT)
#    ifdef AARCHXX
              || check_disp == os_tls_offset((ushort)TLS_REG4_SLOT) ||
              check_disp == os_tls_offset((ushort)TLS_REG5_SLOT)
#    endif
                  ))) {
            if (tls != NULL)
                *tls = true;
            if (offs_out != NULL)
                *offs_out = check_disp;
            return true;
        }
    }
    if (dcontext != GLOBAL_DCONTEXT &&
        instr_check_mcontext_spill_restore(dcontext, instr, spill, reg, &check_disp)) {
        int offs = opnd_get_reg_dcontext_offs(dr_reg_fixer[*reg]);
        if (!DR_only || (offs != -1 && check_disp == offs)) {
            if (tls != NULL)
                *tls = false;
            if (offs_out != NULL)
                *offs_out = check_disp;
            return true;
        }
    }
    return false;
}

DR_API
bool
instr_is_reg_spill_or_restore(void *drcontext, instr_t *instr, bool *tls, bool *spill,
                              reg_id_t *reg, uint *offs)
{
    return instr_is_reg_spill_or_restore_ex(drcontext, instr, false, tls, spill, reg,
                                            offs);
}

bool
instr_is_DR_reg_spill_or_restore(void *drcontext, instr_t *instr, bool *tls, bool *spill,
                                 reg_id_t *reg, uint *offs)
{
    return instr_is_reg_spill_or_restore_ex(drcontext, instr, true, tls, spill, reg,
                                            offs);
}

/* N.B. : client meta routines (dr_insert_* etc.) should never use anything other
 * then TLS_XAX_SLOT unless the client has specified a slot to use as we let the
 * client use the rest. */
instr_t *
instr_create_save_to_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs)
{
    return XINST_CREATE_store(dcontext, opnd_create_tls_slot(os_tls_offset(offs)),
                              opnd_create_reg(reg));
}

instr_t *
instr_create_restore_from_tls(dcontext_t *dcontext, reg_id_t reg, ushort offs)
{
    return XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                             opnd_create_tls_slot(os_tls_offset(offs)));
}

/* For -x86_to_x64, we can spill to 64-bit extra registers (xref i#751). */
instr_t *
instr_create_save_to_reg(dcontext_t *dcontext, reg_id_t reg1, reg_id_t reg2)
{
    return XINST_CREATE_move(dcontext, opnd_create_reg(reg2), opnd_create_reg(reg1));
}

instr_t *
instr_create_restore_from_reg(dcontext_t *dcontext, reg_id_t reg1, reg_id_t reg2)
{
    return XINST_CREATE_move(dcontext, opnd_create_reg(reg1), opnd_create_reg(reg2));
}

#    ifdef X86_64
/* Returns NULL if pc is not the start of a rip-rel lea.
 * If it could be, returns the address it refers to (which we assume is
 * never NULL).
 */
byte *
instr_raw_is_rip_rel_lea(byte *pc, byte *read_end)
{
    /* PR 215408: look for "lea reg, [rip+disp]"
     * We assume no extraneous prefixes, and we require rex.w, though not strictly
     * necessary for say WOW64 or other known-lower-4GB situations
     */
    if (pc + 7 <= read_end) {
        if (*(pc + 1) == RAW_OPCODE_lea &&
            (TESTALL(REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG, *pc) &&
             !TESTANY(~(REX_PREFIX_BASE_OPCODE | REX_PREFIX_ALL_OPFLAGS), *pc)) &&
            /* does mod==0 and rm==5? */
            ((*(pc + 2)) | MODRM_BYTE(0, 7, 0)) == MODRM_BYTE(0, 7, 5)) {
            return pc + 7 + *(int *)(pc + 3);
        }
    }
    return NULL;
}
#    endif

uint
move_mm_reg_opcode(bool aligned16, bool aligned32)
{
#    ifdef X86
    if (YMM_ENABLED()) {
        /* must preserve ymm registers */
        return (aligned32 ? OP_vmovdqa : OP_vmovdqu);
    } else if (proc_has_feature(FEATURE_SSE2)) {
        return (aligned16 ? OP_movdqa : OP_movdqu);
    } else {
        CLIENT_ASSERT(proc_has_feature(FEATURE_SSE), "running on unsupported processor");
        return (aligned16 ? OP_movaps : OP_movups);
    }
#    elif defined(ARM)
    /* FIXME i#1551: which one we should return, OP_vmov, OP_vldr, or OP_vstr? */
    return OP_vmov;
#    elif defined(AARCH64)
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
#    elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
#    endif /* X86/ARM/RISCV64 */
}

uint
move_mm_avx512_reg_opcode(bool aligned64)
{
#    ifdef X86
    /* move_mm_avx512_reg_opcode can only be called on processors that support AVX-512. */
    ASSERT(ZMM_ENABLED());
    return (aligned64 ? OP_vmovaps : OP_vmovups);
#    else
    /* move_mm_avx512_reg_opcode not supported on ARM/AArch64. */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
#    endif /* X86 */
}

#endif /* !STANDALONE_DECODER */
/****************************************************************************/
