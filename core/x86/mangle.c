/* **********************************************************
 * Copyright (c) 2000-2009 VMware, Inc.  All rights reserved.
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

/* file "mangle.c" */

#include "../globals.h"
#include "arch.h"
#include "../link.h"
#include "../fragment.h"
#include "../instrlist.h"
#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "decode.h"
#include "decode_fast.h"
#include "disassemble.h"
#ifdef STEAL_REGISTER
#include "steal_reg.h"
#endif
#include "instrument.h" /* for dr_insert_call */

#ifdef RCT_IND_BRANCH
# include "../rct.h" /* rct_add_rip_rel_addr */
#endif

#ifdef LINUX
#include <sys/syscall.h>
#endif

#ifdef WINDOWS
/* in callback.c */
extern void callback_start_return(void);
#endif

/* make code more readable by shortening long lines
 * we mark everything we add as a meta-instr to avoid hitting
 * client asserts on setting translation fields
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert

#if defined(NATIVE_RETURN) && !defined(DEBUG)
extern int num_fragments;
#endif

/* Convert a short-format CTI into an equivalent one using
 * near-rel-format.
 * Remember, the target is kept in the 0th src array position,
 * and has already been converted from an 8-bit offset to an
 * absolute PC, so we can just pretend instructions are longer
 * than they really are.
 */
static instr_t *
convert_to_near_rel_common(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    DEBUG_DECLARE(const instr_info_t * info = instr_get_instr_info(instr);)
    app_pc target = NULL;

    if (opcode == OP_jmp_short) {
        instr_set_opcode(instr, OP_jmp);
        return instr;
    }

    if (OP_jo_short <= opcode && opcode <= OP_jnle_short) {
        /* WARNING! following is OP_ enum order specific */
        instr_set_opcode(instr, opcode - OP_jo_short + OP_jo);
        return instr;
    }

    if (OP_loopne <= opcode && opcode <= OP_jecxz) {
        /*
         * from "info as" on GNU/linux system:
       Note that the `jcxz', `jecxz', `loop', `loopz', `loope', `loopnz'
       and `loopne' instructions only come in byte displacements, so that if
       you use these instructions (`gcc' does not use them) you may get an
       error message (and incorrect code).  The AT&T 80386 assembler tries to
       get around this problem by expanding `jcxz foo' to
                     jcxz cx_zero
                     jmp cx_nonzero
            cx_zero: jmp foo
            cx_nonzero:
        *
        * We use that same expansion, but we want to treat the entire
        * three-instruction sequence as a single conditional branch.
        * Thus we use a special instruction that stores the entire
        * instruction sequence as mangled bytes, yet w/ a valid target operand
        * (xref PR 251646).
        * patch_branch and instr_invert_cbr
        * know how to find the target pc (final 4 of 9 bytes).
        * When decoding anything we've written we know the only jcxz or
        * loop* instructions are part of these rewritten packages, and
        * we use remangle_short_rewrite to read back in the instr.
        * (have to do this everywhere call decode() except original
        * interp, plus in input_trace())
        *
        * An alternative is to change 'jcxz foo' to:
                    <save eflags>
                    cmpb %cx,$0
                    je   foo_restore
                    <restore eflags>
                    ...
       foo_restore: <restore eflags>
               foo:
        * However the added complications of restoring the eflags on
        * the taken-branch path made me choose the former solution.
        */

        /* SUMMARY: 
         * expand 'shortjump foo' to:
                          shortjump taken
                          jmp-short nottaken
                   taken: jmp foo
                nottaken:
        */
        if (ilist != NULL) {
            /* PR 266292: for meta instrs, insert separate instrs */
            /* reverse order */
            opnd_t tgt = instr_get_target(instr);
            instr_t *nottaken = INSTR_CREATE_label(dcontext);
            instr_t *taken = INSTR_CREATE_jmp(dcontext, tgt);
            ASSERT(!instr_ok_to_mangle(instr));
            instrlist_meta_postinsert(ilist, instr, nottaken);
            instrlist_meta_postinsert(ilist, instr, taken);
            instrlist_meta_postinsert(ilist, instr, INSTR_CREATE_jmp_short
                                      (dcontext, opnd_create_instr(nottaken)));
            instr_set_target(instr, opnd_create_instr(taken));
            return taken;
        }

        if (opnd_is_near_pc(instr_get_target(instr)))
            target = opnd_get_pc(instr_get_target(instr));
        else if (opnd_is_near_instr(instr_get_target(instr))) {
            instr_t *tgt = opnd_get_instr(instr_get_target(instr));
            /* assumption: target's translation or raw bits are set properly */
            target = instr_get_translation(tgt);
            if (target == NULL && instr_raw_bits_valid(tgt))
                target = instr_get_raw_bits(tgt);
            ASSERT(target != NULL);
        } else
            ASSERT_NOT_REACHED();

        /* PR 251646: cti_short_rewrite: target is in src0, so operands are
         * valid, but raw bits must also be valid, since they hide the multiple
         * instrs.  For x64, it is marked for re-relativization, but it's
         * special since the target must be obtained from src0 and not
         * from the raw bits (since that might not reach).
         */
        /* need 9 bytes */
        instr_allocate_raw_bits(dcontext, instr, CTI_SHORT_REWRITE_LENGTH);
        /* first 2 bytes: jecxz 8-bit-offset */
        instr_set_raw_byte(instr, 0, decode_first_opcode_byte(opcode));
        /* remember pc-relative offsets are from start of next instr */
        instr_set_raw_byte(instr, 1, (byte)2);
        /* next 2 bytes: jmp-short 8-bit-offset */
        instr_set_raw_byte(instr, 2, decode_first_opcode_byte(OP_jmp_short));
        instr_set_raw_byte(instr, 3, (byte)5);
        /* next 5 bytes: jmp 32-bit-offset */
        instr_set_raw_byte(instr, 4, decode_first_opcode_byte(OP_jmp));
        /* for x64 we may not reach, but we go ahead and try */
        instr_set_raw_word(instr, 5, (int)
                           (target - (instr->bytes + CTI_SHORT_REWRITE_LENGTH)));
        LOG(THREAD, LOG_INTERP, 2, "convert_to_near_rel: jecxz/loop* opcode\n");
        /* original target operand is still valid */
        instr_set_operands_valid(instr, true);
        return instr;
    }

    LOG(THREAD, LOG_INTERP, 1, "convert_to_near_rel: unknown opcode: %d %s\n",
        opcode, info->name);
    ASSERT_NOT_REACHED();      /* conversion not possible OR not a short-form cti */
    return instr;
}

instr_t *
convert_to_near_rel_meta(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    return convert_to_near_rel_common(dcontext, ilist, instr);
}

void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr)
{
    convert_to_near_rel_common(dcontext, NULL, instr);
}

/* For jecxz and loop*, we create 3 instructions in a single
 * instr that we treat like a single conditional branch.
 * On re-decoding our own output we need to recreate that instr.
 * This routine assumes that the instructions encoded at pc
 * are indeed a mangled cti short.
 * Assumes that the first instr has already been decoded into instr,
 * that pc points to the start of that instr.
 * Converts instr into a new 3-raw-byte-instr with a private copy of the
 * original raw bits.
 * Optionally modifies the target to "target" if "target" is non-null.
 * Returns the pc of the instruction after the remangled sequence.
 */
byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    ASSERT(instr_is_cti_short_rewrite(instr, pc));

    /* first set the target in the actual operand src0 */
    if (target == NULL) {
        /* acquire existing absolute target */
        int rel_target = *((int *)(pc+5));
        target = pc + CTI_SHORT_REWRITE_LENGTH + rel_target;
    }
    instr_set_target(instr, opnd_create_pc(target));
    /* now set up the bundle of raw instructions
     * we've already read the first 2-byte instruction, jecxz/loop*
     * they all take up CTI_SHORT_REWRITE_LENGTH bytes
     */
    instr_allocate_raw_bits(dcontext, instr, CTI_SHORT_REWRITE_LENGTH);
    instr_set_raw_bytes(instr, pc, CTI_SHORT_REWRITE_LENGTH);
    /* for x64 we may not reach, but we go ahead and try */
    instr_set_raw_word(instr, 5, (int)(target - (pc + CTI_SHORT_REWRITE_LENGTH)));
    /* now make operands valid */
    instr_set_operands_valid(instr, true);
    return (pc+CTI_SHORT_REWRITE_LENGTH);
}

/* Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a dr_mcontext_t and have to do further xsp
 * fixups anyway).
 * Includes xmm0-5 for PR 264138.
 * If stack_align16 is true, assumes the stack pointer is currently aligned
 * on a 16-byte boundary.
 */
uint
insert_push_all_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                          bool stack_align16)
{
    PRE(ilist, instr, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XSP),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, - XMM_SLOTS_SIZE)));
    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        /* PR 266305: see discussion in emit_fcache_enter_shared on
         * which opcode is better.  Note that the AMD optimization
         * guide says to use movlps+movhps for unaligned stores, but
         * for simplicity and smaller code I'm using movups anyway.
         */
        uint opcode = (proc_has_feature(FEATURE_SSE2) ?
                       (stack_align16 ? OP_movdqa : OP_movdqu) :
                       (stack_align16 ? OP_movaps : OP_movups));
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            PRE(ilist, instr, instr_create_1dst_1src
                (dcontext, opcode,
                 opnd_create_base_disp(REG_XSP, REG_NULL, 0, i*XMM_REG_SIZE, OPSZ_16),
                 opnd_create_reg(REG_XMM0 + (reg_id_t)i)));
        }
        ASSERT(i*XMM_REG_SIZE == XMM_SAVED_SIZE);
        ASSERT(XMM_SAVED_SIZE <= XMM_SLOTS_SIZE);
    }
#ifdef X64
    /* keep dr_mcontext_t order */
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R15)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R14)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R13)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R12)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R11)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R10)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R9)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R8)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RAX)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RCX)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDX)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBX)));
    /* we do NOT match pusha xsp value */
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSP)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBP)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSI)));
    PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDI)));
    return 16*XSP_SZ + XMM_SLOTS_SIZE;
#else
    PRE(ilist, instr, INSTR_CREATE_pusha(dcontext));
    return 8*XSP_SZ + XMM_SLOTS_SIZE;
#endif
}

/* If stack_align16 is true, assumes the stack pointer is currently aligned
 * on a 16-byte boundary.
 */
void
insert_pop_all_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool stack_align16)
{
#ifdef X64
    /* in dr_mcontext_t order */
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDI)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSI)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBP)));
    /* skip xsp by popping into dead rbx */
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDX)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RCX)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RAX)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R8)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R9)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R10)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R11)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R12)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R13)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R14)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R15)));
#else
    PRE(ilist, instr, INSTR_CREATE_popa(dcontext));
#endif
    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        /* See discussion in emit_fcache_enter_shared on which opcode
         * is better. */
        uint opcode = (proc_has_feature(FEATURE_SSE2) ?
                       (stack_align16 ? OP_movdqa : OP_movdqu) :
                       (stack_align16 ? OP_movaps : OP_movups));
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            PRE(ilist, instr, instr_create_1dst_1src
                (dcontext, opcode, opnd_create_reg(REG_XMM0 + (reg_id_t)i),
                 opnd_create_base_disp(REG_XSP, REG_NULL, 0, i*XMM_REG_SIZE, OPSZ_16)));
        }
        ASSERT(i*XMM_REG_SIZE == XMM_SAVED_SIZE);
        ASSERT(XMM_SAVED_SIZE <= XMM_SLOTS_SIZE);
    }
    PRE(ilist, instr, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XSP),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, XMM_SLOTS_SIZE)));
}

/* utility routines for inserting clean calls to an instrumentation routine 
 * strategy is very similar to fcache_enter/return
 * FIXME: try to share code with fcache_enter/return?
 *
 * first swap stacks to DynamoRIO stack:
 *      SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *      RESTORE_FROM_DCONTEXT dstack_OFFSET,%xsp
 * now save app eflags and registers, being sure to lay them out on
 * the stack in dr_mcontext_t order:
 *      push $0 # for dr_mcontext_t.pc; wasted, for now
 *      pushf
 *      pusha # xsp is dstack-XSP_SZ*2; rest are app values
 * clear the eflags for our usage
 * ASSUMPTION (also made in x86.asm): 0 ok, reserved bits are not set by popf,
 *                                    and clearing, not preserving, is good enough
 *      push   $0
 *      popf
 * save app errno
 *      .ifdef WINDOWS
 *      call  _GetLastError@0
 *      push  %eax  # put errno on top of stack
 *      .else
 *      RESTORE_FROM_DCONTEXT errno_OFFSET,%eax
 *      push  %eax  # for symmetry w/ win32, rather than -> app_errno_OFFSET
 *      .endif
 * make the call
 *      call routine
 * restore app errno
 *      .ifdef WINDOWS
 *      # errno is on top of stack as 1st param
 *      call  _SetLastError@4
 *      # win32 API functions use __stdcall = callee clears args!
 *      .else
 *      pop    %eax
 *      SAVE_TO_DCONTEXT %eax,errno_OFFSET
 *      .endif
 * restore app regs and eflags
 *      popa
 *      popf
 *      lea XSP_SZ(xsp),xsp # clear dr_mcontext_t.pc slot
 * restore app stack
 *      RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 */

void
insert_get_mcontext_base(dcontext_t *dcontext, instrlist_t *ilist, 
                         instr_t *where, reg_id_t reg)
{
    PRE(ilist, where, instr_create_restore_from_tls
        (dcontext, reg, TLS_DCONTEXT_SLOT));

    /* An extra level of indirection with SELFPROT_DCONTEXT */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        ASSERT_NOT_TESTED();
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(reg), 
             OPND_CREATE_MEMPTR(reg, offsetof(dcontext_t, upcontext))));
    }
}

/* What prepare_for_clean_call() adds to xsp beyond sizeof(dr_mcontext_t) */
static inline int
clean_call_beyond_mcontext(void)
{
    return XSP_SZ/*errno*/ IF_X64(+ XSP_SZ/*align*/);
}

/* prepare_for and cleanup_after assume that the stack looks the same after
 * the call to the instrumentation routine, since it stores the app state
 * on the stack.
 * Returns the size of the data stored on the DR stack.
 * WARNING: this routine does NOT save the fp/mmx/sse state, to do that the
 * instrumentation routine should call proc_save_fpstate() and then
 * proc_restore_fpstate()
 * (This is because of expense:
 *   fsave takes 118 cycles!  
 *   frstor (separated by 6 instrs from fsave) takes 89 cycles
 *   fxsave and fxrstor are not available on HP machine!
 *   supposedly they came out in PII
 *   on balrog: fxsave 91 cycles, fxrstor 173)
 *
 * For x64, changes the stack pointer by a multiple of 16.
 * 
 * NOTE: The client interface's get/set mcontext functions and the
 * hotpatching gateway rely on the app's context being available
 * on the dstack in a particular format.  Do not corrupt this data
 * unless you update all users of this data!
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot.
 * We guarantee to clients that all other slots (except the XAX mcontext slot)
 * will remain untouched.
 *
 * N.B.: insert_parameter_preparation (and our documentation for
 * dr_prepare_for_call) assumes that this routine only modifies xsp
 * and xax and no other registers.
 */
uint
prepare_for_clean_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    uint dstack_offs = 0;
    /* Swap stacks.  For thread-shared, we need to get the dcontext
     * dynamically rather than use the constant passed in here.  Save
     * away xax in a TLS slot and then load the dcontext there.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr, instr_create_save_to_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));

        insert_get_mcontext_base(dcontext, ilist, instr,
                                 REG_XAX);

        PRE(ilist, instr, instr_create_save_to_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, XSP_OFFSET));

        /* DSTACK_OFFSET isn't within the upcontext so if it's separate this won't
         * work right.  FIXME - the dcontext accessing routines are a mess of shared
         * vs. no shared support, separate context vs. no separate context support etc. */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         */
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*dc*/, REG_XSP/*scratch*/, true/*to priv*/);
        }
#endif
        PRE(ilist, instr, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, DSTACK_OFFSET));

        /* restore xax before pushing the context on the dstack */
        PRE(ilist, instr, instr_create_restore_from_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));  
    }
    else {
        PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_XSP, XSP_OFFSET));
        PRE(ilist, instr, instr_create_restore_dynamo_stack(dcontext));
    }
    
    /* Save flags and all registers, in dr_mcontext_t order.
     * Leave a slot for pc, which we do not fill in: it's wasted for now.
     * FIXME PR 218131: we could have a special dstack+XSP_SZ field that we
     * start from, and avoid this push; should do that if we start adding more
     * fields to dr_mcontext_t, like a flags field, that are not set here.
     */
    PRE(ilist, instr, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
    dstack_offs += XSP_SZ;
    PRE(ilist, instr, INSTR_CREATE_pushf(dcontext));
    dstack_offs += XSP_SZ;
    /* Base of dstack is 16-byte aligned, and we've done 2 pushes, so we're
     * 16-byte aligned for x64
     */
    dstack_offs += insert_push_all_registers(dcontext, ilist, instr,
                                             IF_X64_ELSE(true, false));

    /* Note that we do NOT bother to put the correct pre-push app xsp value on the
     * stack here, as an optimization for callees who never ask for it: instead we
     * rely on dr_[gs]et_mcontext() to fix it up if asked for.  We can get away w/
     * this while hotpatching cannot (hotp_inject_gateway_call() fixes it up every
     * time) b/c the callee has to ask for the dr_mcontext_t.
     */

    /* clear eflags for callee's usage */
    PRE(ilist, instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
    PRE(ilist, instr, INSTR_CREATE_popf(dcontext));

#ifdef WINDOWS
    /* must preserve the LastErrorCode (if call a Win32 API routine could
     * overwrite the app's error code) */
    preinsert_get_last_error(dcontext, ilist, instr,
                             REG_EAX);
    /* by pushing errno onto stack, it's then in place to be an argument
     * to SetLastError for cleanup!
     */
    /* FIXME: no longer necessary, update this and cleanup_after_clean_call.
     * All cleanup_call users most importantly, pre_system_call and 
     * post_system_call would need not to reserve room for errno:
     * except for our private loader w/ client-dependent libs we do need
     * to handle and isolate (limited) Win32 API usage
     */
#else
    /* put shared errno on stack, for symmetry w/ win32, rather than
     * into app storage slot */
    if (SCRATCH_ALWAYS_TLS()) {
        /* eax is dead here (already saved to stack) */
        insert_get_mcontext_base(dcontext, ilist, instr, REG_XAX);
        PRE(ilist, instr, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_EAX, ERRNO_OFFSET));
    } else {
        PRE(ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_EAX, ERRNO_OFFSET));
    }
#endif
    PRE(ilist, instr,
        /* top 32 bits were zeroed on x64 */
        INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
    dstack_offs += XSP_SZ;
#ifdef X64
    /* PR 218790: maintain 16-byte rsp alignment.
     * insert_parameter_preparation() currently assumes we leave rsp aligned.
     */
    PRE(ilist, instr, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XSP),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -(int)XSP_SZ)));
    dstack_offs += XSP_SZ;
#endif
    ASSERT(dstack_offs == sizeof(dr_mcontext_t) + clean_call_beyond_mcontext());
    return dstack_offs;
}

void
cleanup_after_clean_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* saved error code is currently on the top of the stack */

#ifdef X64
    /* PR 218790: remove the padding we added for 16-byte rsp alignment */
    PRE(ilist, instr, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XSP),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, XSP_SZ)));
#endif
    /* restore app's error code */
    PRE(ilist, instr,
        /* top 32 bits were zeroed on x64 */
        INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XAX)));
#ifdef WINDOWS
    /* must preserve the LastErrorCode (if call a Win32 API routine could
     * overwrite the app's error code) */
    preinsert_set_last_error(dcontext, ilist, instr, REG_EAX);
#else
    if (SCRATCH_ALWAYS_TLS()) {
        /* xbx is dead (haven't restored yet) and eax contains the errno */
        insert_get_mcontext_base(dcontext, ilist, instr, REG_XBX);
        PRE(ilist, instr, instr_create_save_to_dc_via_reg
            (dcontext, REG_XBX, REG_EAX, ERRNO_OFFSET));
    } else {
        PRE(ilist, instr,
            instr_create_save_to_dcontext(dcontext, REG_EAX, ERRNO_OFFSET));
    }
#endif

    /* now restore everything */
    insert_pop_all_registers(dcontext, ilist, instr,
                             /* see notes in prepare_for_clean_call() */
                             IF_X64_ELSE(true, false));
    PRE(ilist, instr, INSTR_CREATE_popf(dcontext));

    /* Swap stacks back.  For thread-shared, we need to get the dcontext
     * dynamically.  Save xax in TLS so we can use it as scratch.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr, instr_create_save_to_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));

        insert_get_mcontext_base(dcontext, ilist, instr,
                                 REG_XAX);

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         */
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*dc*/, REG_XSP/*scratch*/, false/*to app*/);
        }
#endif

        PRE(ilist, instr, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, XSP_OFFSET));

        PRE(ilist, instr, instr_create_restore_from_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));
    }
    else {
        PRE(ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_XSP, XSP_OFFSET));
    }
}

bool
parameters_stack_padded(void)
{
    return (REGPARM_MINSTACK > 0 || REGPARM_END_ALIGN > XSP_SZ);
}

static reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
#ifdef X64
    if (opnd_get_size(arg) == OPSZ_4) { /* we ignore var-sized */
        /* PR 250976 #2: leave 64-bit only if an immed w/ top bit set (we
         * assume user wants sign-extension; that is after all what happens
         * on a push of a 32-bit immed) */
        if (!opnd_is_immed_int(arg) ||
            (opnd_get_immed_int(arg) & 0x80000000) == 0)
            return reg_64_to_32(regular);
    }
#endif
    return regular;
}


/* Returns the change in the stack pointer.
 * N.B.: due to stack alignment and minimum stack reservation, do
 * not use parameters involving esp/rsp, as its value can change!
 *
 * This routine only supports passing arguments that are integers or
 * pointers of a size equal or smaller than the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 *
 * For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * Arguments that reference REG_XSP will work for clean calls, but are not guaranteed
 * to work for non-clean, especially for 64-bit where we align, etc.  Arguments that
 * reference sub-register portions of REG_XSP are not supported.
 *
 * FIXME PR 307874: w/ a post optimization pass, or perhaps more clever use of
 * existing passes, we could do much better on calling convention and xsp conflicting
 * args.  We should also really consider inlining client callees (PR 218907), since
 * clean calls for 64-bit are enormous (71 instrs/264 bytes for 2-arg x64; 26
 * instrs/99 bytes for x86) and we could avoid all the xmm saves and replace pushf w/
 * lahf.
 */
static uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, va_list ap)
{
    uint i;
    int r;
    uint preparm_padding = 0;
    uint param_stack = 0, total_stack = 0;
    bool push = true;
    bool restore_xax = false;
    bool restore_xsp = false;
    /* we need two passes for PR 250976 optimization */
    opnd_t *args = NULL;
    /* Push/mov in reverse order.  We need a label so we can also add
     * instrs prior to the regular param prep.  So params are POST-mark, while
     * pre-param-prep is POST-prev or PRE-mark.
     */
#ifdef X64
    uint arg_pre_push = 0, total_pre_push = 0;
#endif
    instr_t *prev = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    instr_t *mark = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, mark);

    if (num_args > 0)
        args = HEAP_ARRAY_ALLOC(dcontext, opnd_t, num_args, ACCT_OTHER, PROTECTED);

    /* For a clean call, xax is dead (clobbered by prepare_for_clean_call()).
     * Rather than use as scratch and restore prior to each param that uses it,
     * we restore once up front if any use it, and use regparms[0] as scratch,
     * which is symmetric with non-clean-calls: regparms[0] is dead since we're
     * doing args in reverse order.  However, we then can't use regparms[0]
     * directly if referenced in earlier params, but similarly for xax, so
     * there's no clear better way. (prepare_for_clean_call also clobbers xsp,
     * but we just disallow args that use it).
     */

    /* We can get away w/ one pass, except for PR 250976 we want calling conv
     * regs to be able to refer to dr_mcontext_t as well as potentially being
     * pushed: but we need to know the total # pushes ahead of time (since hard
     * to mark for post-patching)
     */
    for (i = 0; i < num_args; i++) {
        IF_X64(bool is_pre_push = false;)
        /* There's no way to check num_args vs actual args passed in */
        args[i] = va_arg(ap, opnd_t);
        CLIENT_ASSERT(opnd_is_valid(args[i]),
                      "Call argument: bad operand. Did you create a valid opnd_t?");
        for (r = 0; r < opnd_num_regs_used(args[i]); r++) {
            reg_id_t used = opnd_get_reg_used(args[i], r);
            IF_X64(int parm;)
            LOG(THREAD, LOG_INTERP, 4,
                "ipp: considering arg %d reg %d == %s\n", i, r, reg_names[used]);
            if (clean_call && !restore_xax && reg_overlap(used, REG_XAX))
                restore_xax = true;
            if (reg_overlap(used, REG_XSP)) {
                IF_X64(CLIENT_ASSERT(clean_call,
                                     "Non-clean-call argument: REG_XSP not supported"));
                CLIENT_ASSERT(used == REG_XSP,
                              "Call argument: sub-reg-xsp not supported");
                if (clean_call && /*x64*/parameters_stack_padded() && !restore_xsp)
                    restore_xsp = true;
            }
#ifdef X64
            /* PR 250976 #A: count the number of pre-pushes we need */
            parm = reg_parameter_num(used);
            /* We can read a register used in an earlier arg since we store that
             * arg later (we do reverse order), except arg0, which we use as
             * scratch (we don't always need it, but not worth another pre-pass
             * through all args to find out), and xsp.  Otherwise, if a plain reg,
             * we point at mcontext (we restore xsp slot in mcontext if nec.).
             * If a mem ref, we need to pre-push onto stack.
             * N.B.: this conditional is duplicated in 2nd loop.
             */
            if (!is_pre_push &&
                ((parm == 0 && num_args > 1) || parm > (int)i ||
                 reg_overlap(used, REG_XSP)) &&
                (!clean_call || !opnd_is_reg(args[i]))) {
                total_pre_push++;
                is_pre_push = true; /* ignore further regs in same arg */
            }
#endif
        }
    }

    if (parameters_stack_padded()) {
        /* For x64, supposed to reserve rsp space in function prologue; we
         * do next best thing and reserve it prior to setting up the args.
         */
        push = false; /* store args to xsp offsets instead of pushing them */
        total_stack = REGPARM_MINSTACK;
        if (num_args > NUM_REGPARM)
            total_stack += XSP_SZ * (num_args - NUM_REGPARM);
        param_stack = total_stack;
        IF_X64(total_stack += XSP_SZ * total_pre_push);
        /* We assume rsp is currently 16-byte aligned.  End of arguments is supposed
         * to be 16-byte aligned for x64 SysV (note that retaddr will then make
         * rsp 8-byte-aligned, which is ok: callee has to rectify that).
         * For clean calls, prepare_for_clean_call leaves rsp aligned for x64.
         * FIXME PR 218790: we require users of dr_insert_call to ensure
         * alignment; should we put in support to dynamically align?
         */
        preparm_padding =
            ALIGN_FORWARD_UINT(total_stack, REGPARM_END_ALIGN) - total_stack;
        total_stack += preparm_padding;
        /* we have to wait to insert the xsp adjust */
    } else {
        ASSERT(NUM_REGPARM == 0);
        ASSERT(push);
        IF_X64(ASSERT(total_pre_push == 0));
        total_stack = XSP_SZ * num_args;
    }
    LOG(THREAD, LOG_INTERP, 3,
        "insert_parameter_preparation: %d args, %d in-reg, %d pre-push, %d/%d stack\n",
        num_args, NUM_REGPARM, IF_X64_ELSE(total_pre_push, 0), param_stack, total_stack);

    for (i = 0; i < num_args; i++) {
        /* FIXME PR 302951: we need to handle state restoration if any
         * of these args references app memory.  We should pull the state from
         * the dr_mcontext_t on the stack if in a clean call.  FIXME: what if not?
         */
        opnd_t arg = args[i];
        CLIENT_ASSERT(opnd_get_size(arg) == OPSZ_PTR || opnd_is_immed_int(arg)
                      IF_X64(|| opnd_get_size(arg) == OPSZ_4),
                      "Clean call arg has unsupported size");

#ifdef X64
        /* PR 250976 #A: support args that reference param regs */
        for (r = 0; r < opnd_num_regs_used(arg); r++) {
            reg_id_t used = opnd_get_reg_used(arg, r);
            int parm = reg_parameter_num(used);
            /* See comments in loop above */ 
            if ((parm == 0 && num_args > 1) || parm > (int)i ||
                 reg_overlap(used, REG_XSP)) {
                int disp = 0;
                if (clean_call && opnd_is_reg(arg)) {
                    /* We can point at the dr_mcontext_t slot.
                     * dr_mcontext_t is at the base of dstack: compute offset
                     * from xsp to the field we want and replace arg.
                     */
                    disp += opnd_get_reg_dcontext_offs(opnd_get_reg(arg));
                    /* skip rest of what prepare_for_clean_call adds */
                    disp += clean_call_beyond_mcontext();
                    /* skip what this routine added */
                    disp += total_stack;
                } else {
                    /* Push a temp on the stack and point at it.  We
                     * could try to optimize by juggling registers, but
                     * not worth it.
                     */
                    /* xsp was adjusted up above; we simply store to xsp offsets */
                    disp = param_stack + XSP_SZ * arg_pre_push;
                    if (opnd_is_reg(arg) && opnd_get_size(arg) == OPSZ_PTR) {
                        POST(ilist, prev, INSTR_CREATE_mov_st
                             (dcontext, OPND_CREATE_MEMPTR(REG_XSP, disp), arg));
                    } else {
                        reg_id_t xsp_scratch = regparms[0];
                        /* don't want to just change size since will read extra bytes.
                         * can't do mem-to-mem so go through scratch reg */
                        if (reg_overlap(used, REG_XSP)) {
                            /* Get original xsp into scratch[0] and replace in arg */
                            if (opnd_uses_reg(arg, regparms[0])) {
                                xsp_scratch = REG_XAX;
                                ASSERT(!opnd_uses_reg(arg, REG_XAX)); /* can't use 3 */
                                /* FIXME: rather than putting xsp into mcontext
                                 * slot, better to just do local get from dcontext
                                 * like we do for 32-bit below? */
                                POST(ilist, prev, instr_create_restore_from_tls
                                     (dcontext, REG_XAX, TLS_XAX_SLOT));
                            }
                            opnd_replace_reg(&arg, REG_XSP, xsp_scratch);
                        }
                        POST(ilist, prev,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, disp),
                                                 opnd_create_reg(regparms[0])));
                        /* If sub-ptr-size, zero-extend is what we want so no movsxd */
                        POST(ilist, prev, INSTR_CREATE_mov_ld
                             (dcontext, opnd_create_reg
                              (shrink_reg_for_param(regparms[0], arg)), arg));
                        if (reg_overlap(used, REG_XSP)) {
                            int xsp_disp = opnd_get_reg_dcontext_offs(REG_XSP) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev, INSTR_CREATE_mov_ld
                                 (dcontext, opnd_create_reg(xsp_scratch),
                                  OPND_CREATE_MEMPTR(REG_XSP, xsp_disp)));
                            if (xsp_scratch == REG_XAX) {
                                POST(ilist, prev, instr_create_save_to_tls
                                     (dcontext, REG_XAX, TLS_XAX_SLOT));
                            }
                        }
                        if (opnd_uses_reg(arg, regparms[0])) {
                            /* must restore since earlier arg might have clobbered */
                            int mc_disp = opnd_get_reg_dcontext_offs(regparms[0]) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev, INSTR_CREATE_mov_ld
                                 (dcontext, opnd_create_reg(regparms[0]),
                                  OPND_CREATE_MEMPTR(REG_XSP, mc_disp)));
                        }
                    }
                    arg_pre_push++; /* running counter */
                }
                arg = opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                            disp, opnd_get_size(arg));
                break; /* once we've handled arg ignore futher reg refs */
            }
        }
#endif

        if (i < NUM_REGPARM) {
            reg_id_t regparm = shrink_reg_for_param(regparms[i], arg);
            if (opnd_is_immed_int(arg)) {
                POST(ilist, mark,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparm), arg));
            } else {
                POST(ilist, mark,
                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(regparm), arg));
            }
        } else {
            if (push) {
                IF_X64(ASSERT_NOT_REACHED()); /* no 64-bit push_imm! */
                if (opnd_is_immed_int(arg))
                    POST(ilist, mark, INSTR_CREATE_push_imm(dcontext, arg));
                else {
                    if (clean_call && opnd_uses_reg(arg, REG_XSP)) {
                        /* We do a purely local expansion:
                         * spill eax, mc->eax, esp->eax, arg->eax, push eax, restore eax
                         */
                        reg_id_t scratch = REG_XAX;
                        if (opnd_uses_reg(arg, scratch)) {
                            scratch = REG_XCX;
                            ASSERT(!opnd_uses_reg(arg, scratch)); /* can't use 3 regs */
                        }
                        opnd_replace_reg(&arg, REG_XSP, scratch);
                        POST(ilist, mark, instr_create_restore_from_tls
                             (dcontext, scratch, TLS_XAX_SLOT));
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                        POST(ilist, mark, instr_create_restore_from_dc_via_reg
                            (dcontext, scratch, scratch, XSP_OFFSET));
                        insert_get_mcontext_base
                            (dcontext, ilist, instr_get_next(mark), scratch);
                        POST(ilist, mark, instr_create_save_to_tls
                             (dcontext, scratch, TLS_XAX_SLOT));
                    } else
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                }
            } else {
                /* xsp was adjusted up above; we simply store to xsp offsets */
                uint offs = REGPARM_MINSTACK + XSP_SZ * (i - NUM_REGPARM);
#ifdef X64
                if (opnd_is_immed_int(arg)) {
                    /* PR 250976 #3: there is no memory store of 64-bit-immediate,
                     * so go through scratch reg */
                    ASSERT(NUM_REGPARM > 0);
                    POST(ilist, mark,
                         INSTR_CREATE_mov_st(dcontext,
                                             OPND_CREATE_MEMPTR(REG_XSP, offs),
                                             opnd_create_reg(regparms[0])));
                    POST(ilist, mark,
                         INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparms[0]),
                                              arg));
                } else {
#endif
                    if (opnd_is_memory_reference(arg)) {
                        /* can't do mem-to-mem so go through scratch */
                        ASSERT(NUM_REGPARM > 0);
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs),
                                                 opnd_create_reg(regparms[0])));
                        POST(ilist, mark,
                             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg
                                                 (shrink_reg_for_param(regparms[0], arg)),
                                                 arg));
                    } else {
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs), arg));
                    }
#ifdef X64
                }
#endif
            }
        }
    }
    if (!push && total_stack > 0) {
        POST(ilist, prev, /* before everything else: pre-push and args */
            /* can we use sub?  may as well preserve eflags */
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0,
                                                 -(int)total_stack)));
    }
    if (restore_xsp) {
        /* before restore_xax, since we're going to clobber xax */
        int disp = opnd_get_reg_dcontext_offs(REG_XSP);
        instr_t *where = instr_get_next(prev);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        insert_get_mcontext_base(dcontext, ilist, where, REG_XAX);
        PRE(ilist, where, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XAX, XSP_OFFSET));
        PRE(ilist, where,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEMPTR(REG_XSP, disp),
                                opnd_create_reg(REG_XAX)));
        /* now we need restore_xax to be AFTER this */
        prev = instr_get_prev(where);
    }
    if (restore_xax) {
        int disp = opnd_get_reg_dcontext_offs(REG_XAX);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        POST(ilist, prev, /* before everything else: pre-push, args, and stack adjust */
             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_MEMPTR(REG_XSP, disp)));
    }
    if (num_args > 0)
        HEAP_ARRAY_FREE(dcontext, args, opnd_t, num_args, ACCT_OTHER, PROTECTED);
    return total_stack;
}

/* Inserts a complete call to callee with the passed-in arguments.
 * For x64, assumes the stack pointer is currently 16-byte aligned.
 * Clean calls ensure this by using clean base of dstack and having
 * dr_prepare_for_call pad to 16 bytes.
 */
void
insert_meta_call_vargs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       bool clean_call, void *callee, uint num_args, va_list ap)
{
    instr_t *in = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    uint stack_for_params = 
        insert_parameter_preparation(dcontext, ilist, instr, clean_call, num_args, ap);
    IF_X64(ASSERT(ALIGNED(stack_for_params, 16)));
    PRE(ilist, instr, INSTR_CREATE_call(dcontext, opnd_create_pc(callee)));
    if (stack_for_params > 0) {
        /* FIXME PR 245936: let user decide whether to clean up?
         * i.e., support calling a stdcall routine?
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                   stack_for_params, OPSZ_lea)));
    }
    /* mark it all meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != instr) {
        instr_set_ok_to_mangle(in, false);
        in = instr_get_next(in);
    }
}

/* If jmp_instr == NULL, uses jmp_tag, otherwise uses jmp_instr
 */
void
insert_clean_call_with_arg_jmp_if_ret_true(dcontext_t *dcontext,
            instrlist_t *ilist, instr_t *instr,  void *callee, int arg,
                                           app_pc jmp_tag, instr_t *jmp_instr)
{
    instr_t *false_popa, *jcc;
    prepare_for_clean_call(dcontext, ilist, instr);

    dr_insert_call(dcontext, ilist, instr, callee, 1, OPND_CREATE_INT32(arg));

    /* if the return value (xax) is 0, then jmp to internal false path */
    PRE(ilist,instr, /* can't cmp w/ 64-bit immed so use test (shorter anyway) */
        INSTR_CREATE_test(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XAX)));
    /* fill in jcc target once have false path */
    jcc = INSTR_CREATE_jcc(dcontext, OP_jz, opnd_create_pc(NULL));
    PRE(ilist, instr, jcc);
    
    /* if it falls through, then it's true, so restore and jmp to true tag
     * passed in by caller
     */
    cleanup_after_clean_call(dcontext, ilist, instr);
    if (jmp_instr == NULL) {
        /* an exit cti, not a meta instr */
        instrlist_preinsert
            (ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_pc(jmp_tag)));
    } else {
        PRE(ilist, instr,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(jmp_instr)));
    }

    /* otherwise (if returned false), just do standard popf and continue */
    /* get 1st instr of cleanup path */
    false_popa = instr_get_prev(instr);
    cleanup_after_clean_call(dcontext, ilist, instr);
    false_popa = instr_get_next(false_popa);
    instr_set_target(jcc, opnd_create_instr(false_popa));
}

/*###########################################################################
 *###########################################################################
 *
 *   M A N G L I N G   R O U T I N E S
 */

/*###########################################################################*/
#ifdef NATIVE_RETURN

/* ENORMOUS HACK: written by mangle_direct_call, read by
 * native_ret_mangle_return
 */
static app_pc static_retaddr;

static instr_t *
native_ret_mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                              instr_t *next_instr, uint retaddr)
{
# ifdef NATIVE_RETURN_CALLDEPTH
    uint flags = 0;
# endif
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /* ENORMOUS HACK -- READ BY MANGLE_RETURN */
    static_retaddr = (app_pc) retaddr;

    /* HACKS to deal with places where ret addr is taken inside a real callee:
         0x40170199   e8 32 fa ff ff       call   $0x4016fbd0 
             continuing in callee at 0x4016fbd0
         0x4016fbd0   8b 1c 24             mov    (%esp) -> %ebx 
         0x4016fbd3   c3                   ret    %esp (%esp) -> %esp 
     * also this:
         0x400a9bab   e8 00 00 00 00       call   $0x400a9bb0 
             continuing in callee at 0x400a9bb0
         0x400a9bb0   8d 04 c0             lea    (%eax,%eax,8) -> %eax 
         0x400a9bb3   03 04 24             add    (%esp) %eax -> %eax 
         0x400a9bb6   05 0d 00 00 00       add    $0x0000000d %eax -> %eax 
         0x400a9bbb   83 c4 04             add    $0x04 %esp -> %esp 
         0x400a9bbe   ff e0                jmp    %eax 
     */
    if (instr_raw_bits_valid(next_instr) &&
        instr_length(dcontext, next_instr) == 3) {
        byte *b = instr_get_raw_bits(next_instr);
        if (*b==0x8b && *(b+1)==0x1c && *(b+2)==0x24) {
            LOG(THREAD, LOG_INTERP, 3, "mangling load of return address!\n");
            /* we can't delete next_instr (in local var in mangle()) */
            /* cannot call instr_reset, it will kill prev & next ptrs */
            instr_free(dcontext, next_instr);
            instr_set_opcode(next_instr, OP_mov_imm);
            instr_set_num_opnds(dcontext, next_instr, 1, 1);
            instr_set_dst(next_instr, 0, opnd_create_reg(REG_EBX));
            instr_set_src(next_instr, 0, OPND_CREATE_INT32(retaddr));
        }
    } else if (instr_raw_bits_valid(next_instr) &&
               instr_length(dcontext, next_instr) == 14) {
        byte *b = instr_get_raw_bits(next_instr);
        if (*b==0x8d && *(b+1)==0x04 && *(b+2)==0xc0 &&
            *(b+3)==0x03 && *(b+4)==0x04 && *(b+5)==0x24) {
            LOG(THREAD, LOG_INTERP, 3, "mangling load of return address!\n");
            instrlist_preinsert(ilist, next_instr,
                                instr_create_raw_3bytes(dcontext, 0x8d, 0x04, 0xc0));
            instrlist_preinsert(ilist, next_instr,
                                INSTR_CREATE_add(dcontext, opnd_create_reg(REG_EAX),
                                                 OPND_CREATE_INT32(retaddr)));
            instr_set_raw_bits(next_instr, b+6, 8);
            next_instr = instr_get_next(instr);
        }
    }

    /*********************************************************/
    /* NEW CALL HANDLING: NATIVE RETURN
          save flags
          inc call_depth
          restore flags
          call skip
FIXME: coordinate this w/ ret site:       restore flags
          jmp app_ret_addr
        skip:
          continue in callee
     */
# ifdef NATIVE_RETURN_CALLDEPTH
    if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
        /* save app's eax */
        instrlist_preinsert(ilist, instr,
                         instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
        /* save flags */
        instrlist_preinsert(ilist, instr, INSTR_CREATE_lahf(dcontext));
    }
    if (!TEST(FRAG_WRITES_EFLAGS_OF, flags)) {
        /* must have saved eax */
        ASSERT(!TEST(FRAG_WRITES_EFLAGS_6, flags));
        /* seto al */
        instrlist_preinsert(ilist, instr,
                INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
    }

    /* inc call_depth */
    instrlist_preinsert(ilist, instr,
                        INSTR_CREATE_inc(dcontext, opnd_create_dcontext_field(dcontext,
                                                                              CALL_DEPTH_OFFSET)));

    if ((flags & FRAG_WRITES_EFLAGS_OF) == 0) {
        /* now do an add such that OF will be set only if seto set al to 1 */
        instrlist_preinsert(ilist, instr,
         INSTR_CREATE_add(dcontext,
                          opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    if ((flags & FRAG_WRITES_EFLAGS_6) == 0) {
        instrlist_preinsert(ilist, instr, INSTR_CREATE_sahf(dcontext));
        instrlist_preinsert(ilist, instr,
                 instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    }
# endif

    /* tell call to target next instr */
    instr_set_target(instr, opnd_create_instr(next_instr));

# ifdef NATIVE_RETURN_CALLDEPTH
    /* FIXME: these flags should be based on retaddr */
    if ((flags & FRAG_WRITES_EFLAGS_OF) == 0) {
        /* now do an add such that OF will be set only if seto set al to 1 */
        instrlist_preinsert(ilist, instr,
         INSTR_CREATE_add(dcontext,
                          opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    if ((flags & FRAG_WRITES_EFLAGS_6) == 0) {
        instrlist_preinsert(ilist, next_instr, INSTR_CREATE_sahf(dcontext));
        instrlist_preinsert(ilist, next_instr,
                 instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    }
# endif

    instrlist_preinsert(ilist, next_instr, INSTR_CREATE_jmp(dcontext,
                                                opnd_create_pc((app_pc)retaddr)));
    return next_instr;
}

static void
native_ret_mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr)
{
    bool code_cache_ret = true;
    /*
        cmp call_depth, 0
        je normal_ret
        dec call_depth
FIXME: currently restoring flags at call's return site too
plus need to move the flag saving up above here, now it's after
save edx
        restore eflags
        ret
     normal_ret:
        save edx
        pop edx
        <add ret_imm, esp>
        jmp ind_br_lookup (via stub)
    */
    uint flags = (FRAG_WRITES_EFLAGS_OF | FRAG_WRITES_EFLAGS_6);
    instr_t *addinstr = NULL;
    
    /* do normal_ret first */
    instr_t *nxt = instr_get_next(instr); /* next_instr is after flag saving! */

    /* save away ecx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    instr_t *save_ecx = instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET);
    instrlist_preinsert(ilist, nxt, save_ecx);

    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    /* see if ret has an immed int operand, assumed to be 1st src */
            
    if (instr_num_srcs(instr) > 0 && opnd_is_immed_int(instr_get_src(instr, 0))) {
        /* if has an operand, return removes some stack space,
         * AFTER the return address is popped
         */
        ptr_int_t val = opnd_get_immed_int(instr_get_src(instr, 0));
        opnd_t add;
        if (val >= -128 && val <= 127)
            add = opnd_create_immed_int(val, OPSZ_1);
        else
            add = opnd_create_immed_int(val, OPSZ_4);
        /* addl sizeof_param_area, %esp */
        /* insert this add AFTER the flags have been saved! */
         if (!INTERNAL_OPTION(unsafe_ignore_eflags)) {
             instrlist_preinsert(ilist, next_instr,
                                 INSTR_CREATE_add(dcontext,
                                                  opnd_create_reg(REG_ESP), add));
         }
         else {
             addinstr = INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP), add);
         }
    }
            
    if (instr_raw_bits_valid(instr)) {
        app_pc pc = (app_pc) instr_get_raw_bits(instr);
        LOG(THREAD, LOG_INTERP, 3, "checking ret at address "PFX"\n", pc);
        if (is_dynamo_address(pc)) {
            LOG(THREAD, LOG_INTERP, 3, "found a ret at dynamo address "PFX"\n", pc);
        }
        /* 0x4000d090 on cagfarm* and atari, 0x4000d080 on kobold */
        if (pc == (app_pc) 0x4000d090 || pc == (app_pc) 0x4000d080 ||
            (is_dynamo_address(pc) &&
# ifdef DEBUG
             ((automatic_startup && GLOBAL_STAT(num_fragments) < 32) ||
              (!automatic_startup && GLOBAL_STAT(num_fragments) < 5))
# else
             /* could use GLOBAL_STAT but only if option is on */
             ((automatic_startup && num_fragments < 32) ||
              (!automatic_startup && num_fragments < 5))
# endif
             )) {
            code_cache_ret = false;
        }
    }

    /* change RET into a POP */
    instrlist_preinsert(ilist, nxt, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_ECX)));

    if (INTERNAL_OPTION(unsafe_ignore_eflags) && addinstr != NULL)
        instrlist_preinsert(ilist, nxt, addinstr);
    /* now do first part, before normal_ret */
    if (!code_cache_ret) { /*dcontext->call_depth == 0) {*/
        /* ASSUMPTION: no calls in this basic block preceding the ret,
         * no other places in program where return below dynamorio start stack
         * frame
         */
        LOG(THREAD, LOG_INTERP, 3, "found a non-code-cache ret\n");
        /* remove the ret */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    } else {
        instr_t *prev;

# ifdef NATIVE_RETURN_CALLDEPTH
        /* cmp call_depth, 0 */
        instrlist_preinsert(ilist, instr,
                            INSTR_CREATE_cmp(dcontext,
                                             opnd_create_dcontext_field(dcontext, CALL_DEPTH_OFFSET),
                                             OPND_CREATE_INT32(0)));
        /* je normal_ret */
        instrlist_preinsert(ilist, instr, INSTR_CREATE_jcc(dcontext, OP_je,
                                                           opnd_create_instr(save_ecx)));
        /* dec call_depth */
        instrlist_preinsert(ilist, instr,
                            INSTR_CREATE_dec(dcontext, opnd_create_dcontext_field(dcontext,
                                                                                  CALL_DEPTH_OFFSET)));

        if ((flags & FRAG_WRITES_EFLAGS_OF) == 0) {
            /* now do an add such that OF will be set only if seto set al to 1 */
            instrlist_preinsert(ilist, instr,
                                INSTR_CREATE_add(dcontext,
                                                 opnd_create_reg(REG_AL),
                                                 OPND_CREATE_INT8(0x7f)));
        }
        if ((flags & FRAG_WRITES_EFLAGS_6) == 0) {
            instrlist_preinsert(ilist, instr, INSTR_CREATE_sahf(dcontext));
            instrlist_preinsert(ilist, instr,
                                instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
        }
# endif

        /* leave ret instr where it is */

        /* HACKS to deal with places where ret addr is taken inside a real callee:
         0x400864b6   8b 4c 24 00          mov    (%esp) -> %ecx 
         0x400864ba   89 4a 14             mov    %ecx -> 0x14(%edx) 
         0x400864bd   89 6a 0c             mov    %ebp -> 0xc(%edx) 
         0x400864c0   89 42 18             mov    %eax -> 0x18(%edx) 
         0x400864c3   c3                   ret    %esp (%esp) -> %esp 
         */
        prev = instr_get_prev(instr);
        if (prev != NULL && instr_raw_bits_valid(prev) &&
            instr_length(dcontext, prev) > 4) {
            uint len = instr_length(dcontext, prev);
            byte *b = instr_get_raw_bits(prev) + len - 1;
            if (len > 13 &&
                       *b==0x18 && *(b-1)==0x42 && *(b-2)==0x89 &&
                       *(b-3)==0x0c && *(b-4)==0x6a && *(b-5)==0x89 && 
                       *(b-6)==0x14 && *(b-7)==0x4a && *(b-8)==0x89 && 
                       *(b-9)==0x00 && *(b-10)==0x24 && *(b-11)==0x4c && *(b-12)==0x8b) {
                instr_t *raw;
                LOG(THREAD, LOG_INTERP, 3, "mangling load of return address!\n");
                instr_set_raw_bits(prev, instr_get_raw_bits(prev), len-13);
                /* PROBLEM: don't know app retaddr!
                 * ENORMOUS HACK: assume no calls or threads in between, 
                 * use statically stored one from last call
                 * Also assumes only a single caller
                 */
                IF_X64(ASSERT_NOT_IMPLEMENTED(false));
                instrlist_preinsert(ilist, instr,
                                    INSTR_CREATE_mov_imm(dcontext,
                                                         opnd_create_reg(REG_ECX),
                                                         OPND_CREATE_INT32((uint)static_retaddr)));
                raw = instr_build_bits(dcontext, OP_UNDECODED, 9);
                instr_set_raw_bytes(raw, b-8, 9);               
                instrlist_preinsert(ilist, instr, raw);
            }
        }
    }
}
#endif /* NATIVE_RETURN */

/*###########################################################################*/
#ifdef RETURN_STACK
static instr_t *
return_stack_mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist,
                                instr_t *instr, instr_t *next_instr, uint retaddr)
{
    instr_t *cleanup;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /*********************************************************/
    /* NEW CALL HANDLING: RETURN STACK! */
    /* (optional: build basic block for after call
     * Then change call to this:
     *   push app ret addr
     *   swap to return stack
     *   push app ret addr
     *   call cleanup_stack
     *   jmp after_call_fragment
     * cleanup_stack:
     *   swap to app stack
     */
    cleanup = instr_create_save_dynamo_return_stack(dcontext);
    PRE(ilist, instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(retaddr)));
    PRE(ilist, instr,
        instr_create_save_to_dcontext(dcontext, REG_ESP, XSP_OFFSET));
    PRE(ilist, instr,
        instr_create_restore_dynamo_return_stack(dcontext));
    PRE(ilist, instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(retaddr)));
    instr_set_target(instr, opnd_create_instr(cleanup));
    /* an exit cti, not a meta instr */
    instrlist_preinsert
        (ilist, next_instr, INSTR_CREATE_jmp(dcontext,
                                             opnd_create_pc(retaddr)));
    PRE(ilist, next_instr, cleanup);
    PRE(ilist, next_instr,
        instr_create_restore_from_dcontext(dcontext, REG_ESP, XSP_OFFSET));
    return next_instr;
}
#endif /* RETURN_STACK */

void
insert_push_immed_ptrsz(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                        ptr_int_t val)
{
#ifdef X64
    /* do push-64-bit-immed in two pieces.  tiny corner-case risk of racy
     * access to TOS if this thread is suspended in between or another
     * thread is trying to read its stack, but o/w we have to spill and
     * restore a register. */
    PRE(ilist, instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32((int)val)));
    /* push is sign-extended, so we can skip top half if nothing in top 33 bits */
    if (val >= 0x80000000) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 4),
                                OPND_CREATE_INT32((int)(val >> 32))));
    }
#else
    PRE(ilist, instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(val)));
#endif
}

/* N.B.: keep in synch with instr_check_xsp_mangling() in arch.c */
static void
insert_push_retaddr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                    ptr_int_t retaddr, opnd_size_t opsize)
{
    if (opsize == OPSZ_2) {
        ptr_int_t val = retaddr & (ptr_int_t) 0x0000ffff;
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -2,
                                                   OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM16(REG_XSP, 2),
                                OPND_CREATE_INT16(val)));
    } else if (opsize == OPSZ_PTR) {
        insert_push_immed_ptrsz(dcontext, ilist, instr, retaddr);
    } else {
#ifdef X64
        ptr_int_t val = retaddr & (ptr_int_t) 0xffffffff;
        ASSERT(opsize == OPSZ_4);
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -4,
                                                   OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 4),
                                OPND_CREATE_INT32(val)));
#else
        ASSERT_NOT_REACHED();
#endif
    }
}

static void
insert_push_cs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               ptr_int_t retaddr, opnd_size_t opsize)
{
#ifdef X64
    /* "push cs" is invalid; for now we just push 0x33, a common value of cs.
     * PR 271317 covers doing this properly.
     */
    insert_push_retaddr(dcontext, ilist, instr, 0x33, opsize);
#else
    opnd_t stackop;
    /* we go ahead and push cs, but we won't pop into cs */
    instr_t *push = INSTR_CREATE_push(dcontext, opnd_create_reg(SEG_CS));
    /* 2nd dest is the stack operand size */
    stackop = instr_get_dst(push, 1);
    opnd_set_size(&stackop, opsize);
    instr_set_dst(push, 1, stackop);
    PRE(ilist, instr, push);
#endif
}

/***************************************************************************
 * DIRECT CALL
 * Returns new next_instr
 */
static instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls)
{
    ptr_uint_t retaddr, curaddr;
    app_pc target = NULL;
    int len = instr_length(dcontext, instr);
    opnd_t pushop = instr_get_dst(instr, 1);
    if (opnd_is_near_pc(instr_get_target(instr)))
        target = opnd_get_pc(instr_get_target(instr));
    else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        /* assumption: target's raw bits are meaningful */
        target = instr_get_raw_bits(tgt);
        ASSERT(target != 0);
        /* FIXME case 6962: for far instr, we ignore the segment and
         * assume it matches current cs */
    } else if (opnd_is_far_pc(instr_get_target(instr))) {
        target = opnd_get_pc(instr_get_target(instr));
        /* FIXME case 6962: we ignore the segment and assume it matches current cs */
    } else
        ASSERT_NOT_REACHED();

    if (!mangle_calls ||
#ifdef INTERNAL
        !dynamo_options.inline_calls ||
#endif
        must_not_be_inlined(target)) {
        /* off-trace call that will be executed natively */

        /* relative target must be re-encoded */
        instr_set_raw_bits_valid(instr, false);

#ifdef STEAL_REGISTER
        /* FIXME: need to push edi prior to call and pop after.
         * However, need to push edi prior to any args to this call,
         * and it may be hard to find pre-arg-pushing spot...
         * edi is supposed to be callee-saved, we're trusting this
         * off-trace call to return, we may as well trust it to
         * not trash edi -- these no-inline calls are dynamo's
         * own routines, after all.
         */
#endif
        return next_instr;
    } 

    /* For CI builds, use the translation field so we can handle cases
     * where the client has changed the target and invalidated the raw
     * bits.  We'll make sure the translation is always set for direct
     * calls.
     */
    curaddr = (ptr_uint_t) instr_get_translation(instr);
    if (curaddr == 0 && instr_raw_bits_valid(instr))
        curaddr = (ptr_uint_t) instr_get_raw_bits(instr);
    ASSERT(curaddr != 0);
    retaddr = curaddr + len;
    ASSERT(retaddr == (ptr_uint_t) decode_next_pc(dcontext, (byte *)curaddr));

#if defined(RETURN_STACK) || defined(NATIVE_RETURN)
    /* ASSUMPTION: a call to the next instr is not going to ever have a matching ret!
     * FIXME: have a flag to turn this off...aggressiveness level?
     */
    if (target == (app_pc)retaddr) {
        LOG(THREAD, LOG_INTERP, 3, "found call to next instruction "PFX"\n", target);
#endif

#ifdef CHECK_RETURNS_SSE2
        /* ASSUMPTION: a call to the next instr is not going to ever have a
         * matching ret! */
        if (target == (app_pc)retaddr) {
            LOG(THREAD, LOG_INTERP, 3, "found call to next instruction "PFX"\n", target);
        } else {
            check_return_handle_call(dcontext, ilist, next_instr);
        }
        /* now do the normal thing for a call */
#endif

        if (instr_get_opcode(instr) == OP_call_far) {
            /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
             * if the app wants to change segments, we won't actually issue
             * a segment change, and so will only work properly if the new segment
             * is also 0-based.  To properly issue new segments, we'd need a special
             * ibl that ends in a far cti, and all prior address manipulations would
             * need to be relative to the new segment, w/o messing up current segment.
             * FIXME: can we do better without too much work?
             */
            SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct call");
            STATS_INC(num_far_dir_calls);
            insert_push_cs(dcontext, ilist, instr, 0, opnd_get_size(pushop));
        }

        /* convert a direct call to a push of the return address */
        insert_push_retaddr(dcontext, ilist, instr, retaddr, opnd_get_size(pushop));
        
        /* remove the call */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return next_instr;

#if defined(RETURN_STACK) || defined(NATIVE_RETURN)
    }
    /* "real" call (not to next instr) */
# ifdef NATIVE_RETURN
    return native_ret_mangle_direct_call(dcontext, ilist, instr,
                                         next_instr, retaddr);
# else /* RETURN_STACK */
    return return_stack_mangle_direct_call(dcontext, ilist, instr,
                                           next_instr, retaddr);
# endif
#endif
}


/* We spill to XCX(private dcontext) slot for private fragments, 
 * and to TLS MANGLE_XCX_SPILL_SLOT for shared fragments.
 * (Except for DYNAMO_OPTION(private_ib_in_tls), for which all use tls,
 *  but that has a performance hit because of the extra data cache line)
 * We can get away with the split by having the shared ibl routine copy
 * xcx to the private dcontext, and by having the private ibl never
 * target shared fragments.
 * We also have to modify the xcx spill from tls to private dcontext when
 * adding a shared basic block to a trace.
 *
 * FIXME: if we do make non-trace-head basic blocks valid indirect branch
 * targets, we should have the private ibl have special code to test the
 * flags and copy xcx to the tls slot if necessary.
 */
#define SAVE_TO_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs)                      \
    ((DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags))) ?           \
     INSTR_CREATE_mov_st(dcontext, opnd_create_tls_slot(os_tls_offset(tls_offs)), \
                         opnd_create_reg(reg)) :                                  \
     instr_create_save_to_dcontext((dc), (reg), (dc_offs)))

/***************************************************************************
 * INDIRECT CALL
 */
static void
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    opnd_t target;
    ptr_uint_t retaddr, curaddr;
#ifdef RETURN_STACK
    instr_t *cleanup;
#endif
    opnd_t pushop = instr_get_dst(instr, 1);
    reg_id_t reg_target = REG_XCX;

    if (!mangle_calls)
        return;

    /* Convert near, indirect calls.  The jump to the exit_stub that
     * jumps to indirect_branch_lookup was already inserted into the
     * instr list by interp EXCEPT for the case in which we're converting
     * an indirect call to a direct call. In that case, mangle later
     * inserts a direct exit stub.
     */

    /* If a client changes an instr, or our own mangle_rel_addr() does,
     * the raw bits won't be valid but the translation should be */
    curaddr = (ptr_uint_t) instr_get_translation(instr);
    if (curaddr == 0 && instr_raw_bits_valid(instr))
        curaddr = (ptr_uint_t) instr_get_raw_bits(instr);
    ASSERT(curaddr != 0);
    if (instr_raw_bits_valid(instr))
        retaddr = curaddr + instr->length;
    else /* mangle_rel_addr() may have changed length: use original! */
        retaddr = (ptr_uint_t) decode_next_pc(dcontext, (byte *)curaddr);
    ASSERT(retaddr != 0);

    /* If this call is marked for conversion, do minimal processing.
     * FIXME Just a note that converted calls are not subjected to any of
     * the specialized builds' processing further down.
     */
    if (TEST(INSTR_IND_CALL_DIRECT, instr->flags)) {
        /* convert the call to a push of the return address */
        insert_push_retaddr(dcontext, ilist, instr, retaddr, opnd_get_size(pushop));
        /* remove the call */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return;
    }

#if !defined(RETURN_STACK) && !defined(NATIVE_RETURN)
    /* put the push AFTER the instruction that calculates
     * the target, b/c if target depends on xsp we must use
     * the value of xsp prior to this call instruction!
     * we insert before next_instr to accomplish this.
     */
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        /* goes right before the push of the ret addr */
        insert_push_cs(dcontext, ilist, next_instr, 0, opnd_get_size(pushop));
        /* see notes below -- we don't really support switching segments,
         * though we do go ahead and push cs, we won't pop into cs
         */
    }
    insert_push_retaddr(dcontext, ilist, next_instr, retaddr, opnd_get_size(pushop));
#endif
    
    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT, XCX_OFFSET));
    
#ifdef STEAL_REGISTER
    /* Steal edi if call uses it, using original call instruction */
    steal_reg(dcontext, instr, ilist);
    if (ilist->flags)
        restore_state(dcontext, next_instr, ilist);
    /* It's impossible for our register stealing to use ecx
     * because no call can simultaneously use 3 registers, right?
     * Maximum is 2, in something like "call *(edi,ecx,4)"?
     * If it is possible, need to make sure stealing's use of ecx
     * doesn't conflict w/ our use
     */
#endif    
    
    /* change: call /2, Ev -> movl Ev, %xcx */
    target = instr_get_src(instr, 0);
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        opnd_size_t addr_size;
        /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
         * if the app wants to change segments, we won't actually issue
         * a segment change, and so will only work properly if the new segment
         * is also 0-based.  To properly issue new segments, we'd need a special
         * ibl that ends in a far cti, and all prior address manipulations would
         * need to be relative to the new segment, w/o messing up current segment.
         * FIXME: can we do better without too much work?
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect call");
        STATS_INC(num_far_ind_calls);
        /* opnd type is i_Ep, it's not a far base disp b/c segment is at
         * memory location, not specified as segment prefix on instr.
         * we assume register operands are marked as invalid instrs long
         * before this point.
         */
        /* FIXME: if it is a far base disp we assume 0 base */
        ASSERT(opnd_is_base_disp(target));
        /* Segment selector is the final 2 bytes.
         * We ignore it and assume DS base == target cti CS base.
         */
        /* if data16 then just 2 bytes for address
         * if x64 mode and Intel and rex then 8 bytes for address */
        ASSERT((X64_MODE_DC(dcontext) && opnd_get_size(target) == OPSZ_10 &&
                proc_get_vendor() != VENDOR_AMD) ||
               opnd_get_size(target) == OPSZ_6 || opnd_get_size(target) == OPSZ_4);
        if (opnd_get_size(target) == OPSZ_10) {
            addr_size = OPSZ_8;
            reg_target = REG_RCX;
        } else if (opnd_get_size(target) == OPSZ_6) {
            addr_size = OPSZ_4;
            reg_target = REG_ECX;
        } else /* target has OPSZ_4 */ {
            addr_size = OPSZ_2;
            reg_target = REG_XCX; /* we use movzx below so size doesn't have to match */
        }
            
        target = opnd_create_base_disp(opnd_get_base(target), opnd_get_index(target),
                                       opnd_get_scale(target), opnd_get_disp(target),
                                       addr_size);
    }
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);
    
#ifdef RETURN_STACK
    /* NEW CALL HANDLING: RETURN STACK! */
    /* Change call to this:
     *   push app ret addr
     *   swap to return stack
     *   push app ret addr
     *   call cleanup_stack
     *   jmp after_call_fragment
     * cleanup_stack:
     *   swap to app stack
     *   jmp exit_stub (already added (==next_instr))
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    cleanup = instr_create_save_dynamo_return_stack(dcontext);
    PRE(ilist, next_instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(retaddr)));
    PRE(ilist, next_instr,
        instr_create_save_to_dcontext(dcontext, REG_ESP, XSP_OFFSET));
    PRE(ilist, next_instr,
        instr_create_restore_dynamo_return_stack(dcontext));
    PRE(ilist, next_instr,
        INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(retaddr)));
    PRE(ilist, next_instr,
        INSTR_CREATE_call(dcontext, opnd_create_instr(cleanup)));
    /* an exit cti, not a meta instr */
    instrlist_preinsert
        (ilist, next_instr,
         INSTR_CREATE_jmp(dcontext, opnd_create_pc(retaddr))); 
    PRE(ilist, next_instr, cleanup);
    PRE(ilist, next_instr,
        instr_create_restore_from_dcontext(dcontext, REG_ESP, XSP_OFFSET));
#endif /* RETURN_STACK */

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_call(dcontext, ilist, next_instr);
#endif

#ifdef NATIVE_RETURN
    /*********************************************************/
    /* NEW CALL HANDLING: NATIVE RETURN
          <save flags already here>
          inc call_depth
          call skip
          jmp app_ret_addr
        skip:
          jmp exit_stub (already added (==next_instr))
     */
    /* inc call_depth */
# ifdef NATIVE_RETURN_CALLDEPTH
    instrlist_preinsert(ilist, next_instr,
                        INSTR_CREATE_inc(dcontext, opnd_create_dcontext_field(dcontext,
                                                                              CALL_DEPTH_OFFSET)));
# endif
    instrlist_preinsert(ilist, next_instr,
                       INSTR_CREATE_call(dcontext, opnd_create_instr(next_instr)));
    instrlist_preinsert(ilist, next_instr, INSTR_CREATE_jmp(dcontext,
                                                opnd_create_pc((app_pc)retaddr)));
#endif
}

/***************************************************************************
 * RETURN
 */
static void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    instr_t *pop;
    opnd_t retaddr;

#ifdef NATIVE_RETURN
    native_ret_mangle_return(dcontext, ilist, instr, next_instr);
    return;
#endif

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_return(dcontext, ilist, next_instr);
    /* now do the normal ret mangling */
#endif

    /* Convert returns.  If aggressive we could take advantage of the
     * fact that xcx is dead at the return and not bother saving it?
     * The jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT, XCX_OFFSET));

    /* see if ret has an immed int operand, assumed to be 1st src */
            
    if (instr_num_srcs(instr) > 0 && opnd_is_immed_int(instr_get_src(instr, 0))) {
        /* if has an operand, return removes some stack space,
         * AFTER the return address is popped
         */
        int val = (int) opnd_get_immed_int(instr_get_src(instr, 0));
        IF_X64(ASSERT_TRUNCATE(val, int, opnd_get_immed_int(instr_get_src(instr, 0))));
        /* addl sizeof_param_area, %xsp
         * except that clobbers the flags, so we use lea */
        PRE(ilist, next_instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, val, OPSZ_lea)));
    }
            
    /* don't need to steal edi since return cannot use registers */

    /* the retaddr operand is always the final source for all OP_ret* instrs */
    retaddr = instr_get_src(instr, instr_num_srcs(instr) - 1);

    if (X64_MODE_DC(dcontext) && 
        (instr_get_opcode(instr) == OP_iret || instr_get_opcode(instr) == OP_ret_far) &&
        opnd_get_size(retaddr) == OPSZ_4) {
        /* N.B.: For some unfathomable reason iret and ret_far default to operand
         * size 4 in 64-bit mode (making them, along w/ call_far, the only stack
         * operation instructions to do so). So if we see an iret or far ret with
         * OPSZ_4 in 64-bit mode we need a 4-byte pop, but since we can't actually
         * generate a 4-byte pop we have to emulate it here. */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered iretd/lretd in 64-bit mode!");
        /* Note moving into ecx automatically zero extends which is what we want. */
        PRE(ilist, instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        /* iret could use add since going to pop the eflags, but not lret.
         * lret could combine w/ segment lea below: but not perf-crit instr, and
         * anticipating cs preservation PR 271317 I'm leaving separate. */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, 4, OPSZ_lea)));
    } else {
        /* change RET into a POP, keeping the operand size */
        pop = INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX));
        instr_set_src(pop, 1, retaddr);
        if (opnd_get_size(retaddr) == OPSZ_2)
            instr_set_dst(pop, 0, opnd_create_reg(REG_CX));
        /* We can't do a 4-byte pop in 64-bit mode, but excepting iretd and lretd
         * handled above we should never see one. */
        ASSERT(!X64_MODE_DC(dcontext) || opnd_get_size(retaddr) != OPSZ_4);
        PRE(ilist, instr, pop);
        if (opnd_get_size(retaddr) == OPSZ_2) {
            /* we need to zero out the top 2 bytes */
            PRE(ilist, instr, INSTR_CREATE_movzx
                (dcontext,
                 opnd_create_reg(REG_ECX), opnd_create_reg(REG_CX)));
        }
    }

    if (instr_get_opcode(instr) == OP_ret_far) {
        /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
         * if the app wants to change segments, we won't actually issue
         * a segment change, and so will only work properly if the new segment
         * is also 0-based.  To properly issue new segments, we'd need a special
         * ibl that ends in a far cti, and all prior address manipulations would
         * need to be relative to the new segment, w/o messing up current segment.
         * FIXME: can we do better without too much work?
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far ret");
        STATS_INC(num_far_rets);
        /* pop selector from stack, but not into cs, just junk it
         * (the 16-bit selector is expanded to 32 bits on the push, unless data16)
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp
                             (REG_XSP, REG_NULL, 0,
                              opnd_size_in_bytes(opnd_get_size(retaddr)), OPSZ_lea)));
    }

    if (instr_get_opcode(instr) == OP_iret) {
        instr_t *popf;

        /* Xref PR 215553 and PR 191977 - we actually see this on 64-bit Vista */
#ifndef X64
        ASSERT_NOT_TESTED();
#endif
        LOG(THREAD, LOG_INTERP, 2, "Encountered iret at "PFX" - mangling\n",
            instr_get_translation(instr));
        STATS_INC(num_irets);

        /* In 32-bit mode and 64-bit mode with 32-bit operand size this is a
         * pop->EIP pop->CS pop->eflags.  64-bit mode with 64-bit operand size extends
         * the above and additionally adds pop->RSP pop->ss.  N.B.: like OP_far_ret we
         * ignore the CS and SS segment changes (FIXME: see the comments there for why,
         * can we do better?). */
        
        /* Return address is already popped, next up is CS segment which we ignore so
         * adjust stack pointer. Note we can use an add here since the eflags will
         * be written below. */
        PRE(ilist, instr,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT8
                             (opnd_size_in_bytes(opnd_get_size(retaddr)))));

        /* Next up is xflags, we use a popf. Popf should be setting the right flags 
         * (it's difficult to tell because in the docs iret lists the flags it does
         * set while popf lists the flags it doesn't set). The docs aren't entirely
         * clear, but any flag that we or a user mode program would care about should
         * be right. */
        popf = INSTR_CREATE_popf(dcontext);
        if (X64_MODE_DC(dcontext) && opnd_get_size(retaddr) == OPSZ_4) {
            /* We can't actually create a 32-bit popf and there's no easy way to
             * simulate one.  For now we'll do a 64-bit popf and fixup the stack offset.
             * If AMD/INTEL ever start using the top half of the rflags register then
             * we could have problems here. We could also break stack transparency and
             * do a mov, push, popf to zero extend the value. */
            ASSERT_NOT_TESTED();
            PRE(ilist, instr, popf);
            /* flags are already set, must use lea to fix stack */
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                                 opnd_create_base_disp
                                 (REG_XSP, REG_NULL, 0, -4, OPSZ_lea)));
        } else {
            /* get popf size right the same way we do it for the return address */
            DODEBUG(if (opnd_get_size(retaddr) == OPSZ_2)
                        ASSERT_NOT_TESTED(););
            instr_set_src(popf, 1, retaddr);
            PRE(ilist, instr, popf);
        }

        /* If the operand size is 64-bits iret additionally does pop->RSP and pop->ss. */
        if (opnd_get_size(retaddr) == OPSZ_8) {
            PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSP)));
            /* We're ignoring the set of SS and since we just set RSP we don't need
             * to do anything to adjust the stack for the pop (since the pop would have
             * occurred with the old RSP). */
        } else {
            ASSERT_NOT_TESTED();
        }
    }

    /* remove the ret */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
}

/***************************************************************************
 * INDIRECT JUMP
 */
static void
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    opnd_t target;
    reg_id_t reg_target = REG_XCX;

    /* Convert indirect branches (that are not returns).  Again, the
     * jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT, XCX_OFFSET));

#ifdef STEAL_REGISTER
    /* Steal edi if branch uses it, using original instruction */
    steal_reg(dcontext, instr, ilist);
    if (ilist->flags)
        restore_state(dcontext, next_instr, ilist);
#endif

    /* change: jmp /4, i_Ev -> movl i_Ev, %xcx */
    target = instr_get_target(instr);
    if (instr_get_opcode(instr) == OP_jmp_far_ind) {
        opnd_size_t addr_size;
        /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
         * if the app wants to change segments, we won't actually issue
         * a segment change, and so will only work properly if the new segment
         * is also 0-based.  To properly issue new segments, we'd need a special
         * ibl that ends in a far cti, and all prior address manipulations would
         * need to be relative to the new segment, w/o messing up current segment.
         * FIXME: can we do better without too much work?
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect jump");
        STATS_INC(num_far_ind_jmps);
        /* opnd type is i_Ep, it's not a far base disp b/c segment is at
         * memory location, not specified as segment prefix on instr
         */
        /* FIXME: if it is a far base disp we assume 0 base */
        ASSERT(opnd_is_base_disp(target));
        /* Segment selector is the final 2 bytes.
         * We ignore it and assume DS base == target cti CS base.
         */
        /* if data16 then just 2 bytes for address
         * if x64 mode and Intel and rex then 8 bytes for address */
        ASSERT((X64_MODE_DC(dcontext) && opnd_get_size(target) == OPSZ_10 &&
                proc_get_vendor() != VENDOR_AMD) ||
               opnd_get_size(target) == OPSZ_6 || opnd_get_size(target) == OPSZ_4);
        if (opnd_get_size(target) == OPSZ_10) {
            addr_size = OPSZ_8;
            reg_target = REG_RCX;
        } else if (opnd_get_size(target) == OPSZ_6) {
            addr_size = OPSZ_4;
            reg_target = REG_ECX;
        } else /* target has OPSZ_4 */ {
            addr_size = OPSZ_2;
            reg_target = REG_XCX; /* we use movzx below */
        }

        target = opnd_create_base_disp(opnd_get_base(target), opnd_get_index(target),
                                       opnd_get_scale(target), opnd_get_disp(target),
                                       addr_size);
    }
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);

    /* It's impossible for our register stealing to use ecx
     * because no branch can simultaneously use 3 registers, right?
     * Maximum is 2, in something like "jmp *(edi,ebx,4)"?
     * If it is possible, need to make sure stealing's use of ecx
     * doesn't conflict w/ our use = FIXME
     */
}


/***************************************************************************
 * SYSCALL
 */

/* Tries to statically find the syscall number for the
 * syscall instruction instr.
 * Returns -1 upon failure.
 */
int
find_syscall_num(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int syscall = -1;
    instr_t *prev = instr_get_prev(instr);
    if (prev != NULL) {
        prev = instr_get_prev_expanded(dcontext, ilist, instr);
        /* walk backwards looking for "mov_imm imm->xax"
         * may be other instrs placing operands into registers
         * for the syscall in between
         */
        while (prev != NULL &&
               instr_num_dsts(prev) > 0 &&
               opnd_is_reg(instr_get_dst(prev, 0)) &&
#ifdef X64
               opnd_get_reg(instr_get_dst(prev, 0)) != REG_RAX &&
#endif
               opnd_get_reg(instr_get_dst(prev, 0)) != REG_EAX) {
#ifdef CLIENT_INTERFACE
            /* if client added cti in between, bail and assume non-ignorable */
            if (instr_is_cti(prev))
                return -1;
#endif
            prev = instr_get_prev_expanded(dcontext, ilist, prev);
        }
        if (prev != NULL &&
            instr_get_opcode(prev) == OP_mov_imm &&
            (IF_X64_ELSE(opnd_get_reg(instr_get_dst(prev, 0)) == REG_RAX, true) ||
             opnd_get_reg(instr_get_dst(prev, 0)) == REG_EAX)) {
#ifdef CLIENT_INTERFACE
            instr_t *walk, *tgt;
#endif
            IF_X64(ASSERT_TRUNCATE(int, int,
                                   opnd_get_immed_int(instr_get_src(prev, 0))));
            syscall = (int) opnd_get_immed_int(instr_get_src(prev, 0));
#ifdef CLIENT_INTERFACE
            /* if client added cti target in between, bail and assume non-ignorable */
            for (walk = instrlist_first_expanded(dcontext, ilist);
                 walk != NULL;
                 walk = instr_get_next_expanded(dcontext, ilist, walk)) {
                if (instr_is_cti(walk) && opnd_is_instr(instr_get_target(walk))) {
                    for (tgt = opnd_get_instr(instr_get_target(walk));
                         tgt != NULL;
                         tgt = instr_get_next_expanded(dcontext, ilist, tgt)) {
                        if (tgt == prev)
                            break;
                        if (tgt == instr)
                            return -1;
                    }
                }
            }
#endif
        }
    }
    return syscall;
}

#ifdef LINUX
/* Inserts code to handle clone into ilist.
 * instr is the syscall instr itself.
 * Assumes that instructions exist beyond instr in ilist.
 * pc_to_ecx is an instr that puts the pc after the app's syscall instr
 * into xcx.
 * skip decides whether the clone code is skipped by default or not.
 *
 * N.B.: mangle_clone_code() makes assumptions about this exact code layout
 *
 * CAUTION: don't use a lot of stack in the generated code because 
 *          get_clone_record() makes assumptions about the usage of stack being
 *          less than a page.
 */
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool skip _IF_X64(gencode_mode_t mode))
{
            /*
                int 0x80
            .if don't know sysnum statically:
                jmp ignore  <-- modifiable jmp
            .else
                jmp xchg    # need this so can jmp to ignore if !CLONE_VM
            .endif
              xchg:
                xchg xax,xcx
                jecxz child
                jmp parent
              child:
                # i#149/PR 403015: the child is on the dstack so no need to swap stacks
                jmp new_thread_dynamo_start
              parent:
                xchg xax,xcx
              ignore:
                <post system call, etc.>
            */
    instr_t *in = instr_get_next(instr);
    instr_t *xchg = INSTR_CREATE_label(dcontext);
    instr_t *child = INSTR_CREATE_label(dcontext);
    instr_t *parent = INSTR_CREATE_label(dcontext);
    ASSERT(in != NULL);
    /* we have to dynamically skip or not skip the clone code
     * see mangle_clone_code below
     */
    if (skip) {
        /* Insert a jmp that normally skips the clone stuff,
         * pre_system_call will modify it if it really is SYS_clone.
         */
        PRE(ilist, in,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(in)));
    } else {
        /* We have to do this even if we statically know the sysnum
         * because if CLONE_VM is not set this is a fork, and we then
         * want to skip our clone code.
         */
        PRE(ilist, in,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(xchg)));
    }
    PRE(ilist, in, xchg);
    PRE(ilist, in, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX),
                                     opnd_create_reg(REG_XCX)));
    PRE(ilist, in,
        INSTR_CREATE_jecxz(dcontext, opnd_create_instr(child)));
    PRE(ilist, in,
        INSTR_CREATE_jmp(dcontext, opnd_create_instr(parent)));

    PRE(ilist, in, child);
    /* an exit cti, not a meta instr */
    instrlist_preinsert
        (ilist, in,
         INSTR_CREATE_jmp(dcontext, opnd_create_pc
                          ((app_pc)get_new_thread_start(dcontext _IF_X64(mode)))));
    instr_set_ok_to_mangle(instr_get_prev(in), false);
    PRE(ilist, in, parent);
    PRE(ilist, in, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX),
                                     opnd_create_reg(REG_XCX)));
}
#endif /* LINUX */

#ifdef WINDOWS
/* Note that ignore syscalls processing for XP and 2003 is a two-phase operation.
 * For this reason, mangle_syscall() might be called with a 'next_instr' that's
 * not an original app instruction but one inserted by the earlier mangling phase.
 */
#endif
static void
mangle_syscall(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
               instr_t *instr, instr_t *next_instr)
{
#ifdef LINUX
    if (get_syscall_method() != SYSCALL_METHOD_INT &&
        get_syscall_method() != SYSCALL_METHOD_SYSCALL &&
        get_syscall_method() != SYSCALL_METHOD_SYSENTER) {
        /* don't know convention on return address from kernel mode! */
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        CLIENT_ASSERT(false, "Unsupported system call method detected. Please "
                      "reboot with the nosep kernel option if this is a 32-bit "
                      "2.5 or 2.6 version Linux kernel.");
    }
    /* cannot use dynamo stack in code cache, so we cannot insert a
     * call -- instead we have interp end bbs at interrupts unless
     * we can identify them as ignorable system calls.  Otherwise,
     * we just remove the instruction and jump back to dynamo to
     * handle it.
     */
    if (TESTANY(INSTR_NI_SYSCALL_ALL, instr->flags)) {
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return;
    }

    /* signal barrier: need to be able to exit fragment immediately
     * prior to syscall, so we set up an exit cti with a jmp right beforehand
     * that by default hops over the exit cti.
     * when we want to exit right before the syscall, we call the
     * mangle_syscall_code() routine below.
     */
    PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(instr)));
    /* assumption: raw bits of instr == app pc */
    ASSERT(instr_get_raw_bits(instr) != NULL);
    /* this should NOT be a meta-instr so we don't use PRE */
    instrlist_preinsert(ilist, instr, INSTR_CREATE_jmp
                        (dcontext, opnd_create_pc(instr_get_raw_bits(instr))));

# ifdef STEAL_REGISTER
    /* in linux, system calls get their parameters via registers.
     * edi is the last one used, but there are system calls that
     * use it, so we put the real value into edi.  plus things
     * like fork() should get the real register values.
     * it's also a good idea to put the real edi into %edi for
     * debugger interrupts (int3).
     */

    /* the only way we can save and then restore our dc
     * ptr is to use the stack!
     * this should be fine, all interrupt instructions push
     * both eflags and return address on stack, so esp must
     * be valid at this point.  there could be an application
     * assuming only 2 slots on stack will be used, we use a 3rd
     * slot, could mess up that app...but what can we do?
     * also, if kernel examines user stack, we could have problems.
     *   push edi          # push dcontext ptr
     *   restore edi       # restore app edi
     *   <syscall>
     *   push ebx
     *   mov edi, ebx
     *   mov 4(esp), edi   # get dcontext ptr
     *   save ebx to edi slot
     *   pop ebx
     *   add 4,esp         # clean up push of dcontext ptr
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    PRE(ilist, instr,
        INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EDI)));
    PRE(ilist, instr,
        instr_create_restore_from_dcontext(dcontext, REG_EDI, XDI_OFFSET));

    /* insert after in reverse order: */
    POST(ilist, instr,
         INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP),
                          OPND_CREATE_INT8(4)));
    POST(ilist, instr,
         INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_EBX)));
    POST(ilist, instr,
         instr_create_save_to_dcontext(dcontext, REG_EBX, XDI_OFFSET));
    POST(ilist, instr,
         INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDI),
                             OPND_CREATE_MEM32(REG_ESP, 4)));
    POST(ilist, instr,
         INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EBX),
                             opnd_create_reg(REG_EDI)));
    POST(ilist, instr,
         INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EBX)));
# endif /* STEAL_REGISTER */

#else /* WINDOWS */
    /* special handling of system calls is performed in shared_syscall or
     * in do_syscall
     */
    
    /* FIXME: for ignorable syscalls,
     * do we need support for exiting mid-fragment prior to a syscall
     * like we do on Linux, to bound time in cache?
     */

    if (does_syscall_ret_to_callsite()) {
        uint len = instr_length(dcontext, instr);
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
            /* this syscall will be performed by the shared_syscall code
             * we just need to place a return address into the dcontext
             * xsi slot or the mangle-next-tag tls slot
             */
            if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
# ifdef X64
                ASSERT(instr_raw_bits_valid(instr));
                /* PR 244741: no 64-bit store-immed-to-mem 
                 * FIXME: would be nice to move this to the stub and
                 * use the dead rbx register!
                 */
                PRE(ilist, instr,
                    instr_create_save_to_tls(dcontext, REG_XCX, MANGLE_NEXT_TAG_SLOT));
                PRE(ilist, instr,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INTPTR((instr->bytes + len))));
                PRE(ilist, instr, INSTR_CREATE_xchg
                    (dcontext, opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                     opnd_create_reg(REG_XCX)));
# else
                PRE(ilist, instr, INSTR_CREATE_mov_st
                    (dcontext, opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                     OPND_CREATE_INTPTR((instr->bytes + len))));
# endif
            }
            else {
                PRE(ilist, instr, instr_create_save_immed_to_dcontext
                    (dcontext, (uint)(ptr_uint_t)(instr->bytes + len), XSI_OFFSET));
            }
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {
            if (get_syscall_method() == SYSCALL_METHOD_INT &&
                DYNAMO_OPTION(sygate_int)) {
                /* for Sygate need to mangle into a call to int_syscall_addr
                 * is anyone going to get screwed up by this change
                 * (say flags change?) [-ignore_syscalls only]*/
                ASSERT_NOT_TESTED();
                instrlist_replace(ilist, instr, create_syscall_instr(dcontext));
                instr_destroy(dcontext, instr);
            } else if (get_syscall_method() == SYSCALL_METHOD_SYSCALL)
                ASSERT_NOT_TESTED();
            else if (get_syscall_method() == SYSCALL_METHOD_WOW64)
                ASSERT_NOT_TESTED();
            return;
        }
    } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* on XP/2003 we have a choice between inserting a trampoline at the
         * return pt of the sysenter, which is 0x7ffe0304 (except for
         * SP2-patched XP), which is bad since it would clobber whatever's after
         * the ret there (unless we used a 0xcc, like Visual Studio 2005 debugger
         * does), or replacing the ret addr on the stack -- we choose the
         * latter as the lesser of two transparency evils. Note that the
         * page at 0x7ffe0000 can't be made writable anyway, so hooking
         * isn't possible.
         */
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {

            instr_t *mov_imm;

            /* even w/ ignorable syscall, need to make sure regain control */
            ASSERT(next_instr != NULL);
            ASSERT(DYNAMO_OPTION(indcall2direct));
            /* for sygate hack need to basically duplicate what is done in
             * shared_syscall, but here we could be shared so would need to
             * grab dcontext first etc. */
            ASSERT_NOT_IMPLEMENTED(!DYNAMO_OPTION(sygate_sysenter));
            /* PR 253943: we don't support sysenter in x64 */
            IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* can't have 8-byte imm-to-mem */
            /* FIXME PR 303413: we won't properly translate a fault in our
             * app stack reference here.  It's marked as our own mangling
             * so we'll at least return failure from our translate routine.
             */
            mov_imm = INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 0),
                                          opnd_create_instr(next_instr));
            ASSERT(instr_is_mov_imm_to_tos(mov_imm));
            PRE(ilist, instr, mov_imm);
            /* do not let any encoding for length be cached!
             * o/w will lose pc-relative opnd
             */
            /* 'next_instr' is executed after the after-syscall vsyscall
             * 'ret', which is executed natively. */
            instr_set_ok_to_mangle(instr_get_prev(instr), false);
            return; /* leave syscall instr alone */
        }
    } else {
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        if (!TEST(INSTR_NI_SYSCALL, instr->flags))
            return;
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* destroy the syscall instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
#endif /* WINDOWS */
}

#ifdef LINUX

/* Makes sure the jmp immediately after the syscall instruction
 * either skips or doesn't skip the clone code following it,
 * as indicated by the parameter skip.
 * pc must be either the return address of pre_system_call or
 * the address of do_syscall.
 */
void
mangle_clone_code(dcontext_t *dcontext, byte *pc, bool skip)
{
    byte *target, *prev_pc;
    instr_t instr;
    instr_init(dcontext, &instr);
    LOG(THREAD, LOG_SYSCALLS, 3,
        "mangle_clone_code: pc="PFX", skip=%d\n", pc, skip);
    do {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* our own code! */
    } while (!instr_is_syscall(&instr));
    /* jmp is right after syscall */
    instr_reset(dcontext, &instr);
    prev_pc = pc;
    pc = decode(dcontext, pc, &instr);
    ASSERT(pc != NULL); /* our own code! */
    ASSERT(instr_get_opcode(&instr) == OP_jmp);
    if (skip) {
        /* target is after 3rd xchg */
        instr_t tmp_instr;
        int num_xchg = 0;
        target = pc;
        instr_init(dcontext, &tmp_instr);
        while (num_xchg <= 2) {
            instr_reset(dcontext, &tmp_instr);
            target = decode(dcontext, target, &tmp_instr);
            ASSERT(target != NULL); /* our own code! */
            if (instr_get_opcode(&tmp_instr) == OP_xchg)
                num_xchg++;
        }
    } else {
        target = pc;
    }
    if (opnd_get_pc(instr_get_target(&instr)) != target) {
        DEBUG_DECLARE(byte *nxt_pc;)
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\tmodifying target of after-clone jmp to "PFX"\n", target);
        instr_set_target(&instr, opnd_create_pc(target));
        DEBUG_DECLARE(nxt_pc = ) instr_encode(dcontext, &instr, prev_pc);
        ASSERT(nxt_pc != NULL && nxt_pc == pc);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\ttarget of after-clone jmp is already "PFX"\n", target);
    }
    instr_reset(dcontext, &instr);
}

/* If skip is false:
 *   changes the jmp right before the next syscall (after pc) to target the
 *   exit cti immediately following it;
 * If skip is true:
 *   changes back to the default, where skip hops over the exit cti,
 *   which is assumed to be located at pc.
 */
void
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip)
{
    byte *stop_pc = fragment_body_end_pc(dcontext, f);
    byte *target, *prev_pc, *cti_pc, *skip_pc;
    instr_t instr, cti;
    instr_init(dcontext, &instr);
    instr_init(dcontext, &cti);
    LOG(THREAD, LOG_SYSCALLS, 3,
        "mangle_syscall_code: pc="PFX", skip=%d\n", pc, skip);
    do {
        instr_reset(dcontext, &instr);
        prev_pc = pc;
        pc = decode(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* our own code! */
        if (pc >= stop_pc) {
            LOG(THREAD, LOG_SYSCALLS, 3, "\tno syscalls found\n");
            return;
        }
    } while (!instr_is_syscall(&instr));
    /* jmps are right before syscall */
    cti_pc = prev_pc - 6;
    skip_pc = cti_pc - 6;
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, skip_pc, &instr);
    ASSERT(pc != NULL); /* our own code! */
    ASSERT(instr_get_opcode(&instr) == OP_jmp);
    ASSERT(pc == cti_pc);
    DODEBUG({
        pc = decode(dcontext, cti_pc, &cti);
        ASSERT(pc != NULL); /* our own code! */
        ASSERT(instr_get_opcode(&cti) == OP_jmp);
        ASSERT(pc == prev_pc);
        instr_reset(dcontext, &cti);
    });
    if (skip) {
        /* target is syscall itself */
        target = prev_pc;
    } else {
        /* target is exit cti */
        target = cti_pc;
    }
    /* FIXME : this should work out to just a 1 byte write, but let's make
     * it more clear that this is atomic! */
    if (opnd_get_pc(instr_get_target(&instr)) != target) {
        DEBUG_DECLARE(byte *nxt_pc;)
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\tmodifying target of syscall jmp to "PFX"\n", target);
        instr_set_target(&instr, opnd_create_pc(target));
        DEBUG_DECLARE(nxt_pc = ) instr_encode(dcontext, &instr, skip_pc);
        ASSERT(nxt_pc != NULL && nxt_pc == cti_pc);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\ttarget of syscall jmp is already "PFX"\n", target);
    }
    instr_reset(dcontext, &instr);
}
#endif /* LINUX */

/***************************************************************************
 * NON-SYSCALL INTERRUPT
 */
static void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
#ifdef WINDOWS
    int num;
    if (instr_get_opcode(instr) != OP_int)
        return;
    num = instr_get_interrupt_number(instr);
    if (num == 0x2b) {
        /* A callback finishes and returns to the interruption
         * point of the thread with the instruction "int 2b".
         * The interrupt ends the block; remove the instruction
         * since we'll come back to dynamo to perform the
         * interrupt.
         */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    }
#endif /* WINDOWS */
}


/***************************************************************************
 * CPUID FOOLING
 */
#ifdef FOOL_CPUID

/* values returned by cpuid for Mobile Pentium MMX processor (family 5, model 8) 
 * minus mmx (==0x00800000 in CPUID_1_EDX)
 * FIXME: change model number to a Pentium w/o MMX!
 */
#define CPUID_0_EAX 0x00000001 
#define CPUID_0_EBX 0x756e6547
#define CPUID_0_ECX 0x6c65746e
#define CPUID_0_EDX 0x49656e69
/* extended family, extended model, type, family, model, stepping id: */
/* 20:27,           16:19,          12:13, 8:11,  4:7,   0:3    */
#define CPUID_1_EAX 0x00000581
#define CPUID_1_EBX 0x00000000
#define CPUID_1_ECX 0x00000000
#define CPUID_1_EDX 0x000001bf

static void
mangle_cpuid(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
             instr_t *next_instr)
{
    /* assumption: input value is put in eax on prev instr, or
     * on instr prior to that and prev is an inc instr.
     * alternative is to insert conditional branch...and save eflags, etc.
     */
    instr_t *prev = instr_get_prev(instr);
    opnd_t op;
    int input, out_eax, out_ebx, out_ecx, out_edx;

    LOG(THREAD, LOG_INTERP, 1, "fooling cpuid instruction!\n");

    ASSERT(prev != NULL);
    prev = instr_get_prev_expanded(dcontext, ilist, instr);
    instr_decode(dcontext, instr);
    if (!instr_valid(instr))
        goto cpuid_give_up;
    loginst(dcontext, 2, prev, "prior to cpuid");

    /* FIXME: maybe should insert code to dispatch on eax, rather than
     * this hack, which is based on photoshop, which either does
     * "xor eax,eax" or "xor eax,eax; inc eax"
     */
    if (!instr_is_mov_constant(prev, &input)) {
        /* we only allow inc here */
        if (instr_get_opcode(prev) != OP_inc)
            goto cpuid_give_up;
        op = instr_get_dst(prev, 0);
        if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
            goto cpuid_give_up;
        /* now check instr before inc */
        prev = instr_get_prev(prev);
        if (!instr_is_mov_constant(prev, &input) || input != 0)
            goto cpuid_give_up;
        input = 1;
        /* now check that mov 0 is into eax */
    }
    if (instr_num_dsts(prev) == 0)
        goto cpuid_give_up;
    op = instr_get_dst(prev, 0);
    if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
        goto cpuid_give_up;
    
    if (input == 0) {
        out_eax = CPUID_0_EAX;
        out_ebx = CPUID_0_EBX;
        out_ecx = CPUID_0_ECX;
        out_edx = CPUID_0_EDX;
    } else {
        /* 1 or anything higher all return same info */
        out_eax = CPUID_1_EAX;
        out_ebx = CPUID_1_EBX;
        out_ecx = CPUID_1_ECX;
        out_edx = CPUID_1_EDX;
    }

    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                             OPND_CREATE_INT32(out_eax)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                             OPND_CREATE_INT32(out_ebx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                             OPND_CREATE_INT32(out_ecx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EDX),
                             OPND_CREATE_INT32(out_edx)));
    
    /* destroy the cpuid instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return;
 cpuid_give_up:
    LOG(THREAD, LOG_INTERP, 1, "\tcpuid fool: giving up\n");
    return;
}
#endif /* FOOL_CPUID */

static void
mangle_exit_cti_prefixes(dcontext_t *dcontext, instr_t *instr)
{
    uint prefixes = instr_get_prefixes(instr);
    if (TESTANY(~(PREFIX_JCC_TAKEN|PREFIX_JCC_NOT_TAKEN), prefixes)) {
        /* Case 8738: while for transparency it would be best to maintain all
         * prefixes, our patching and other routines make assumptions about
         * the length of exit ctis.  Plus our elision removes the whole 
         * instr in any case.
         */
        LOG(THREAD, LOG_INTERP, 4,
            "\tremoving unknown prefixes "PFX" from "PFX"\n",
            prefixes, instr_get_raw_bits(instr));
        prefixes &= (PREFIX_JCC_TAKEN|PREFIX_JCC_NOT_TAKEN);
        instr_set_prefixes(instr, prefixes);
    }
}

#ifdef X64
/* PR 215397: re-relativize rip-relative data addresses */
static void
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    uint opc = instr_get_opcode(instr);
    app_pc tgt;
    opnd_t dst, src;
    ASSERT(instr_has_rel_addr_reference(instr));
    instr_get_rel_addr_target(instr, &tgt);
    STATS_INC(rip_rel_instrs);
# ifdef RCT_IND_BRANCH
    if (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
        TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) {
        /* PR 215408: record addresses taken via rip-relative instrs */
        rct_add_rip_rel_addr(dcontext, tgt _IF_DEBUG(instr_get_translation(instr)));
    }
# endif
    if (opc == OP_lea) {
        /* segment overrides are ignored on lea */
        opnd_t immed;
        dst = instr_get_dst(instr, 0);
        src = instr_get_src(instr, 0);
        ASSERT(opnd_is_reg(dst));
        ASSERT(opnd_is_rel_addr(src));
        ASSERT(opnd_get_addr(src) == tgt);
        /* Replace w/ an absolute immed of the target app address, following Intel
         * Table 3-59 "64-bit Mode LEA Operation with Address and Operand Size
         * Attributes" */
        /* FIXME PR 253446: optimization: we could leave this as rip-rel if it
         * still reaches from the code cache. */         
        if (reg_get_size(opnd_get_reg(dst)) == OPSZ_8) {
            /* PR 253327: there is no explicit addr32 marker; we assume
             * that decode or the user already zeroed out the top bits
             * if there was an addr32 prefix byte or the user wants
             * that effect */
            immed = OPND_CREATE_INTPTR((ptr_int_t)tgt);
        } else if (reg_get_size(opnd_get_reg(dst)) == OPSZ_4)
            immed = OPND_CREATE_INT32((int)(ptr_int_t)tgt);
        else {
            ASSERT(reg_get_size(opnd_get_reg(dst)) == OPSZ_2);
            immed = OPND_CREATE_INT16((short)(ptr_int_t)tgt);
        }
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, dst, immed));
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        STATS_INC(rip_rel_lea);
    } else {
        /* PR 251479 will automatically re-relativize if it reaches,
         * but if it doesn't we need to handle that here (since that
         * involves an encoding length change, which complicates many
         * use cases if done at instr encode time).
         * We don't yet know exactly where we're going to encode this bb,
         * so we're conservative and check for all reachability from our
         * heap (assumed to be a single heap: xref PR 215395, and xref
         * potential secondary code caches PR 253446.
         */
        if (!rel32_reachable_from_heap(tgt)) {
            int si = -1, di = -1;
            opnd_t relop, newop;
            bool spill = true;
            /* FIXME PR 253446: for mbr, should share the xcx spill */
            reg_id_t scratch_reg = REG_XAX;
            si = instr_get_rel_addr_src_idx(instr);
            di = instr_get_rel_addr_dst_idx(instr);
            if (si >= 0) {
                relop = instr_get_src(instr, si);
                ASSERT(di < 0 || opnd_same(relop, instr_get_dst(instr, di)));
                /* If it's a load (OP_mov_ld, or OP_movzx, etc.), use dead reg */
                if (instr_num_srcs(instr) == 1 && /* src is the rip-rel opnd */
                    instr_num_dsts(instr) == 1 && /* only one dest: a register */
                    opnd_is_reg(instr_get_dst(instr, 0))) {
                    opnd_size_t sz = opnd_get_size(instr_get_dst(instr, 0));
                    reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
                    /* if target is 16 or 8 bit sub-register the whole reg is not dead
                     * (for 32-bit, top 32 bits are cleared) */
                    if (reg_is_gpr(reg) && (reg_is_32bit(reg) || reg_is_64bit(reg))) {
                        spill = false;
                        scratch_reg = opnd_get_reg(instr_get_dst(instr, 0));
                        if (sz == OPSZ_4)
                            scratch_reg = reg_32_to_64(scratch_reg);
                        /* we checked all opnds: should not read reg */
                        ASSERT(!instr_reads_from_reg(instr, scratch_reg));
                        STATS_INC(rip_rel_unreachable_nospill);
                    }
                }
            } else {
                relop = instr_get_dst(instr, di);
            }
            /* PR 263369: we can't just look for instr_reads_from_reg here since
             * our no-spill optimization above may miss some writes.
             */
            if (spill && instr_uses_reg(instr, scratch_reg)) {
                /* mbr (for which we'll use xcx once we optimize) should not
                 * get here: can't use registers (except xsp) */
                ASSERT(scratch_reg == REG_XAX);
                do {
                    scratch_reg++;
                    ASSERT(scratch_reg <= REG_STOP_64);
                } while (instr_uses_reg(instr, scratch_reg));
            }
            ASSERT(!instr_reads_from_reg(instr, scratch_reg));
            ASSERT(!spill || !instr_writes_to_reg(instr, scratch_reg));
            /* FIXME PR 253446: Optimize by looking ahead for dead registers, and
             * sharing single spill across whole bb, or possibly building local code
             * cache to avoid unreachability: all depending on how many rip-rel
             * instrs we see.  We'll watch the stats.
             */
            if (spill) {
                PRE(ilist, instr,
                    SAVE_TO_DC_OR_TLS(dcontext, 0, scratch_reg, TLS_XAX_SLOT,
                                      XAX_OFFSET));
            }
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(scratch_reg),
                                     OPND_CREATE_INTPTR((ptr_int_t)tgt)));

            newop = opnd_create_far_base_disp(opnd_get_segment(relop), scratch_reg,
                                              REG_NULL, 0, 0, opnd_get_size(relop));
            if (si >= 0)
                instr_set_src(instr, si, newop);
            if (di >= 0)
                instr_set_dst(instr, di, newop);
            /* we need the whole spill...restore region to all be marked mangle */
            instr_set_our_mangling(instr, true);
            if (spill) {
                PRE(ilist, next_instr,
                    instr_create_restore_from_tls(dcontext, scratch_reg, TLS_XAX_SLOT));
            }
            STATS_INC(rip_rel_unreachable);
        }        
    }
}
#endif

/* TOP-LEVEL MANGLE
 * This routine is responsible for mangling a fragment into the form
 * we'd like prior to placing it in the code cache
 * If mangle_calls is false, ignores calls
 * If record_translation is true, records translation target for each
 * inserted instr -- but this slows down encoding in current implementation
 */
void
mangle(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
       bool mangle_calls, bool record_translation)
{
    instr_t *instr, *next_instr;
#ifdef WINDOWS
    bool ignorable_sysenter = DYNAMO_OPTION(ignore_syscalls) &&
        DYNAMO_OPTION(ignore_syscalls_follow_sysenter) &&
        (get_syscall_method() == SYSCALL_METHOD_SYSENTER) &&
        TEST(FRAG_HAS_SYSCALL, flags);
#endif

    /* Walk through instr list:
     * -- convert exit branches to use near_rel form;
     * -- convert direct calls into 'push %eip', aka return address;
     * -- convert returns into 'pop %xcx (; add $imm, %xsp)';
     * -- convert indirect branches into 'save %xcx; lea EA, %xcx';
     * -- convert indirect calls as a combination of direct call and
     *    indirect branch conversion;
     * -- ifdef STEAL_REGISTER, steal edi for our own use. 
     */

    KSTART(mangling);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */
    for (instr = instrlist_first(ilist);
         instr != NULL;
         instr = next_instr) {

        /* don't mangle anything that mangle inserts! */
        next_instr = instr_get_next(instr);

        if (!instr_opcode_valid(instr))
            continue;

        if (record_translation) {
            /* make sure inserted instrs translate to the original instr */
            instrlist_set_translation_target(ilist, instr_get_raw_bits(instr));
        }

#ifdef X64
        if (instr_has_rel_addr_reference(instr))
            mangle_rel_addr(dcontext, ilist, instr, next_instr);
#endif

        if (instr_is_exit_cti(instr)) {
            mangle_exit_cti_prefixes(dcontext, instr);

            /* to avoid reachability problems we convert all
             * 8-bit-offset jumps that exit the fragment to 32-bit.
             * Note that data16 jmps are implicitly converted via the
             * absolute target and loss of prefix info (xref PR 225937).
             */
            if (instr_is_cti_short(instr)) {
                /* convert short jumps */
                convert_to_near_rel(dcontext, instr);
            }
            
            if (instr_get_opcode(instr) == OP_jmp_far) {
                /* FIXME: case 6962: we don't support fully; just convert
                 * to near jmp.
                 */
                SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct jump");
                STATS_INC(num_far_dir_jmps);
                instr_set_opcode(instr, OP_jmp);
                instr_set_target(instr,
                                 opnd_create_pc(opnd_get_pc(instr_get_target(instr))));
                /* doesn't need to be marked as our_mangling */
            }
        }

        /* PR 240258: wow64 call* gateway is considered is_syscall */
        if (instr_is_syscall(instr)) {
#ifdef WINDOWS
            /* For XP & 2003, which use sysenter, we process the syscall after all
             * mangling is completed, since we need to insert a reference to the
             * post-sysenter instruction. If that instruction is a 'ret', which
             * we've seen on both os's at multiple patch levels, we'd have a
             * dangling reference since it's deleted in mangle_return(). To avoid
             * that case, we defer syscall processing until mangling is completed.
             */
            if (!ignorable_sysenter)
#endif
                mangle_syscall(dcontext, ilist, flags, instr, next_instr);
            continue;
        }
        else if (instr_is_interrupt(instr)) { /* non-syscall interrupt */
            mangle_interrupt(dcontext, ilist, instr, next_instr);
            continue;
        }
#ifdef FOOL_CPUID
        else if (instr_get_opcode(instr) == OP_cpuid) {
            mangle_cpuid(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif

        if (!instr_is_cti(instr) || !instr_ok_to_mangle(instr)) {
#ifdef STEAL_REGISTER
            steal_reg(dcontext, instr, ilist);
#endif
            continue;
        }

#ifdef STEAL_REGISTER
        if (ilist->flags) {
            restore_state(dcontext, instr, ilist); /* end of edi calculation */
        }
#endif

        if (instr_is_call_direct(instr)) {
            /* mangle_direct_call may inline a call and remove next_instr, so
             * it passes us the updated next instr */
            next_instr =
                mangle_direct_call(dcontext, ilist, instr, next_instr, mangle_calls);
        } else if (instr_is_call_indirect(instr)) {
            mangle_indirect_call(dcontext, ilist, instr, next_instr, mangle_calls, flags);
        } else if (instr_is_return(instr)) {
            mangle_return(dcontext, ilist, instr, next_instr, flags);
        } else if (instr_is_mbr(instr)) {
            mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
        } else if (instr_get_opcode(instr) == OP_jmp_far) {
            /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
             * if the app wants to change segments, we won't actually issue
             * a segment change, and so will only work properly if the new segment
             * is also 0-based.  To properly issue new segments, we'd need a special
             * ibl that ends in a far cti, and all prior address manipulations would
             * need to be relative to the new segment, w/o messing up current segment.
             * FIXME: can we do better without too much work?
             */
            SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct jmp");
        }
        /* else nothing to do, e.g. direct branches */
    }

#ifdef WINDOWS
    /* Do XP & 2003 ignore-syscalls processing now. */
    if (ignorable_sysenter) {
        /* Check for any syscalls and process them. */
        for (instr = instrlist_first(ilist);
             instr != NULL;
             instr = next_instr) {
            next_instr = instr_get_next(instr);
            if (instr_opcode_valid(instr) && instr_is_syscall(instr))
                mangle_syscall(dcontext, ilist, flags, instr, next_instr);
        }
    }
#endif
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */

    /* The following assertion should be guaranteed by fact that all
     * blocks end in some kind of branch, and the code above restores
     * the register state on a branch. */
    ASSERT(ilist->flags == 0);
    KSTOP(mangling);
}

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

/* SELF-MODIFYING-CODE SANDBOXING
 *
 * When we detect it, we take an exit that targets our own routine
 * fragment_self_write.  Dispatch checks for that target and if it finds it,
 * it calls that routine, so don't worry about building a bb for it.
 * Returns false if the bb has invalid instrs in the middle and it should
 * be rebuilt from scratch.
 */

#undef SAVE_TO_DC_OR_TLS
/* PR 244737: x64 uses tls to avoid reachability issues w/ absolute addresses */
#ifdef X64
# define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_save_to_tls((dc), (reg), (tls_offs))
# define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_restore_from_tls((dc), (reg), (tls_offs))
#else
# define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_save_to_dcontext((dc), (reg), (dc_offs))
# define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_restore_from_dcontext((dc), (reg), (dc_offs))
#endif

static void
sandbox_rep_instr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
                  app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* put checks before instr, set some reg as a flag, act on it
     * after instr (even if overwrite self will execute rep to completion)
     * want to read DF to find direction (0=inc xsi/xdi, 1=dec),
     * but only way to read is to do a pushf!
     * Solution: if cld or std right before rep instr, use that info,
     * otherwise check for BOTH directions!
     * xcx is a pre-check, xsi/xdi are inc/dec after memory op, so
     * xdi+xcx*opndsize == instr of NEXT write, so open-ended there:
     * if DF==0:
     *   if (xdi < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * if DF==1:
     *   if (xdi > start_pc && xdi-xcx*opndsize > end_pc) => self-write
     * both:
     *   if (xdi-xcx*opndsize < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * opndsize is 1,2, or 4 => use lea for mul
     *   lea (xdi,xcx,opndsize),xcx
     * 
     *   save flags and xax
     *   save xbx
     *   lea (xdi,xcx,opndsize),xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xdx
     * if x64 && start_pc > 4GB: mov start_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, start_pc)
     *   mov $0,xbx # for if ok
     *   jle ok # open b/c address of next rep write
     *   lea (,xcx,opndsize),xbx
     *   neg xbx # sub does dst - src
     *   add xdi,xbx
     * if x64 && end_pc > 4GB: mov end_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, end_pc)
     *   mov $0,xbx # for if ok
     *   jge ok    # end is open
     *   mov $1,xbx
     * ok:
     *   restore flags and xax (xax used by stos)
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xdx
     *   <rep instr> # doesn't use xbx
     *     (FIXME PR 267764: restore xbx on cxt xl8 if this instr faults)
     *   mov xbx,xcx # we can use xcx, it's dead since 0 after rep
     *   restore xbx
     *   jecxz ok2  # if xbx was 1 we'll fall through and exit
     *   mov $0,xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SELFMOD_EXIT>
     * ok2:
     *   <label> # ok2 can't == next, b/c next may be ind br -> mangled w/ instrs
     *           # inserted before it, so jecxz would target too far
     */
    instr_t *ok = INSTR_CREATE_label(dcontext);
    instr_t *ok2 = INSTR_CREATE_label(dcontext);
    instr_t *jmp;
    app_pc after_write;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr,0)));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    instr_t *next_app = next;
    DOLOG(3, LOG_INTERP, { loginst(dcontext, 3, instr, "writes memory"); });

    ASSERT(!instr_is_call_indirect(instr)); /* FIXME: can you have REP on on CALL's */

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && !instr_ok_to_mangle(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190) 
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            after_write = opnd_get_pc(instr_get_target(next_app));
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        after_write = end_pc;
    }

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls);
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_XDI, REG_XCX, opndsize, 0, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(0)));
    PRE(ilist, instr,
        INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_NULL, REG_XCX, opndsize, 0, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_neg(dcontext, opnd_create_reg(REG_XBX)));
    PRE(ilist, instr,
        INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_reg(REG_XDI)));
#ifdef X64
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32(0)));
    PRE(ilist, instr,
        INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(1)));
    PRE(ilist, instr, ok);
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls);
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
#endif
    /* instr goes here */
    PRE(ilist, next,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX), opnd_create_reg(REG_XBX)));
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, next,
        INSTR_CREATE_jecxz(dcontext, opnd_create_instr(ok2)));
    PRE(ilist, next,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                             OPND_CREATE_INT32(0))); /* on x64 top 32 bits zeroed */
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_selfmod_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok2);
}

static void
sandbox_write(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
              opnd_t op, app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* can only test for equality w/o modifying flags, so save them 
     * if (addr < end_pc && addr+opndsize > start_pc) => self-write
     *   <write memory>
     *   save flags and xax
     *   save xbx
     *   lea memory,xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xcx
     * if x64 && end_pc > 4GB: mov end_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, end_pc)
     *   jge ok    # end is open
     *   lea opndsize(xbx),xbx
     * if x64 && start_pc > 4GB: mov start_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, start_pc)
     *   jle ok    # open since added size
     *   restore flags (using xbx) and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SELFMOD_EXIT>
     * ok:
     *   restore flags and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     */
    instr_t *ok = INSTR_CREATE_label(dcontext), *jmp;
    app_pc after_write = NULL;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(op));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    instr_t *next_app = next;
    DOLOG(3, LOG_INTERP, { loginst(dcontext, 3, instr, "writes memory"); });

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && !instr_ok_to_mangle(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190) 
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final artificially added jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            /* for sure this is the last jmp out, but it
             * doesn't have to be a direct jmp but instead
             * it could be the exit branch we add as an
             * for an indirect call - which is the only ind branch
             * that writes to memory
             * CALL* already means that we're leaving the block and it cannot be a selfmod
             * instruction even though it writes to memory
             */
            DOLOG(4, LOG_INTERP, { 
                loginst(dcontext, 4, next_app, "next app instr");
            });
            after_write = opnd_get_pc(instr_get_target(next_app));
            LOG(THREAD, LOG_INTERP, 4, "after_write = "PFX" next should be final jmp\n",
                after_write);
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        ASSERT_NOT_TESTED();
        after_write = end_pc;
    }

    insert_save_eflags(dcontext, ilist, next, flags, use_tls, !use_tls);
    PRE(ilist, next,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    /* change to OPSZ_lea for lea */
    opnd_set_size(&op, OPSZ_lea);
    PRE(ilist, next,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX), op));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next,
        INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, next,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                               opndsize, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next,
        INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls, !use_tls);
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_selfmod_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok);
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls, !use_tls);
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
}

static bool
sandbox_top_of_bb_check_s2ro(dcontext_t *dcontext, app_pc start_pc)
{
    return (DYNAMO_OPTION(sandbox2ro_threshold) > 0 &&
            /* we can't make stack regions ro so don't put in the instrumentation */
            !is_address_on_stack(dcontext, start_pc) &&
            /* case 9098 we don't want to ever make RO untrackable driver areas */
            !is_driver_address(start_pc));
}

static void
sandbox_top_of_bb(dcontext_t *dcontext, instrlist_t *ilist,
                  bool s2ro, uint flags,
                  app_pc start_pc, app_pc end_pc, /* end is open */
                  bool for_cache,
                  /* for obtaining the two patch locations: */
                  patch_list_t *patchlist,
                  cache_pc *copy_start_loc, cache_pc *copy_end_loc)
{
    /* add check at top of ilist that compares actual app instructions versus
     * copy we saved, stored in cache right after fragment itself.  leave its
     * start address blank here, will be touched up after emitting this ilist.
     *
     * FIXME case 8165/PR 212600: optimize this: move reg restores to
     * custom fcache_return, use cmpsd instead of cmpsb, etc.
     *
     * if eflags live entering this bb:
     *   save xax
     *   lahf
     *   seto  %al
     * endif
     * if (-sandbox2ro_threshold > 0)
     *  if x64: save xcx
     *     incl  &vm_area_t->exec_count (for x64, via xcx)
     *     cmp   sandbox2ro_threshold, vm_area_t->exec_count (for x64, via xcx)
     *  if eflags live entering this bb, or x64:
     *     jl    past_threshold
     *   if x64: restore xcx
     *   if eflags live entering this bb:
     *     jmp restore_eflags_and_exit
     *   else
     *     jmp   start_pc marked as selfmod exit
     *   endif
     *   past_threshold:
     *  else
     *     jge   start_pc marked as selfmod exit
     *  endif
     * endif
     * if (-sandbox2ro_threshold == 0) && !x64)
     *   save xcx
     * endif
     *   save xsi
     *   save xdi
     * if stats:
     *   inc num_sandbox_execs stat (for x64, via xsi)
     * endif
     *   mov start_pc,xsi
     *   mov copy_start_pc,xdi  # 1 opcode byte, then offset
     *       # => patch point 1
     *   cmpsb
     * if copy_size > 1 # not an opt: for correctness: if "repe cmpsb" has xcx==0, it
     *                  # doesn't touch eflags and we treat cmp results as cmpsb results
     *     jne check_results
     *   if x64 && start_pc > 4GB
     *     mov start_pc, xcx
     *     cmp xsi, xcx
     *   else
     *     cmp xsi, start_pc
     *   endif
     *     mov copy_size-1, xcx
     *     jge forward
     *     mov copy_end_pc, xdi
     *         # => patch point 2
     *     mov end_pc, xsi
     *   forward:
     *     repe cmpsb
     * endif # copy_size > 1
     *   check_results:
     *     restore xcx
     *     restore xsi
     *     restore xdi
     * if eflags live:
     *   je start_bb
     *  restore_eflags_and_exit:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     *   jmp start_pc marked as selfmod exit
     * else
     *   jne start_pc marked as selfmod exit
     * endif
     * start_bb:
     * if eflags live:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     * endif
     */
    instr_t *instr, *jmp;
    instr_t *restore_eflags_and_exit = NULL;
    bool use_tls = IF_X64_ELSE(true, false);
    bool saved_xcx = false;
    instr_t *check_results = INSTR_CREATE_label(dcontext);

    instr = instrlist_first_expanded(dcontext, ilist);

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls);

    if (s2ro) {
        /* It's difficult to use lea/jecxz here as we want to use a shared
         * counter but no lock, and thus need a relative comparison, while
         * lea/jecxz can only do an exact comparison.  We could be exact by
         * having a separate counter per (private) fragment but by spilling
         * eflags we can inc memory, making the scheme here not inefficient.
         */
        uint thresh = DYNAMO_OPTION(sandbox2ro_threshold);
        uint *counter;
        if (for_cache)
            counter = get_selfmod_exec_counter(start_pc);
        else {
            /* Won't find exec area since not a real fragment (probably
             * a recreation post-flush).  Won't execute, so NULL is fine.
             */
            counter = NULL;
        }
#ifdef X64
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
        saved_xcx = true;
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(counter)));
        PRE(ilist, instr,
            INSTR_CREATE_inc(dcontext, OPND_CREATE_MEM32(REG_XCX, 0)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, OPND_CREATE_MEM32(REG_XCX, 0),
                             OPND_CREATE_INT_32OR8((int)thresh)));
#else
        PRE(ilist, instr,
            INSTR_CREATE_inc(dcontext, OPND_CREATE_ABSMEM(counter, OPSZ_4)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext,
                             OPND_CREATE_ABSMEM(counter, OPSZ_4),
                             OPND_CREATE_INT_32OR8(thresh)));
#endif
        if (TEST(FRAG_WRITES_EFLAGS_6, flags) IF_X64(&& false)) {
            jmp = INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_pc(start_pc));
            instr_branch_set_selfmod_exit(jmp, true);
            /* an exit cti, not a meta instr */
            instrlist_preinsert(ilist, instr, jmp);
        } else {
            instr_t *past_threshold = INSTR_CREATE_label(dcontext);
            PRE(ilist, instr,
                INSTR_CREATE_jcc_short(dcontext, OP_jl_short,
                                       opnd_create_instr(past_threshold)));
#ifdef X64
            PRE(ilist, instr,
                RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
#endif
            if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
                ASSERT(restore_eflags_and_exit == NULL);
                restore_eflags_and_exit = INSTR_CREATE_label(dcontext);
                PRE(ilist, instr, INSTR_CREATE_jmp
                    (dcontext, opnd_create_instr(restore_eflags_and_exit)));
            }
#ifdef X64
            else {
                jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
                instr_branch_set_selfmod_exit(jmp, true);
                /* an exit cti, not a meta instr */
                instrlist_preinsert(ilist, instr, jmp);
            }
#endif
            PRE(ilist, instr, past_threshold);
        }
    }

    if (!saved_xcx) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    DOSTATS({
        if (GLOBAL_STATS_ON()) {
            /* We only do global inc, not bothering w/ thread-private stats.
             * We don't care about races: ballpark figure is good enough.
             * We could do a direct inc of memory for 32-bit.
             */
            PRE(ilist, instr, INSTR_CREATE_mov_imm
                (dcontext, opnd_create_reg(REG_XSI),
                 OPND_CREATE_INTPTR(GLOBAL_STAT_ADDR(num_sandbox_execs))));
            PRE(ilist, instr, INSTR_CREATE_inc
                (dcontext, opnd_create_base_disp(REG_XSI, REG_NULL, 0, 0, OPSZ_STATS)));
         }
    });
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                             OPND_CREATE_INTPTR(start_pc)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                             /* will become copy start */
                             OPND_CREATE_INTPTR(start_pc)));
    if (patchlist != NULL) {
        ASSERT(copy_start_loc != NULL);
        add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                         -(short)sizeof(cache_pc), (ptr_uint_t*)copy_start_loc);
    }
    PRE(ilist, instr, INSTR_CREATE_cmps_1(dcontext));
    /* For a 1-byte copy size we cannot use "repe cmpsb" as it won't
     * touch eflags and we'll treat the cmp results as cmpsb results, which
     * doesn't work (cmp will never be equal)
     */
    if (end_pc - start_pc > 1) {
        instr_t *forward = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_instr(check_results)));
#ifdef X64
        if ((ptr_uint_t)start_pc > UINT_MAX) {
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                     OPND_CREATE_INTPTR(start_pc)));
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 opnd_create_reg(REG_XCX)));
        } else {
#endif
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
        }
#endif
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc - (start_pc + 1))));
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(forward)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                                 /* will become copy end */
                                 OPND_CREATE_INTPTR(end_pc)));
        if (patchlist != NULL) {
            ASSERT(copy_end_loc != NULL);
            add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                             -(short)sizeof(cache_pc), (ptr_uint_t*)copy_end_loc);
        }
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, instr, forward);
        PRE(ilist, instr, INSTR_CREATE_rep_cmps_1(dcontext));
    }
    PRE(ilist, instr, check_results);
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
        instr_t *start_bb = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_je, opnd_create_instr(start_bb)));
        if (restore_eflags_and_exit != NULL) /* somebody needs this label */
            PRE(ilist, instr, restore_eflags_and_exit);
        insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls);
        jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
        instr_branch_set_selfmod_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
        PRE(ilist, instr, start_bb);
    } else {
        jmp = INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_pc(start_pc));
        instr_branch_set_selfmod_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
    }
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls);
    /* fall-through to bb start */
}

/* returns false if failed to add sandboxing b/c of a problematic ilist --
 * invalid instrs, elided ctis, etc.
 */
bool
insert_selfmod_sandbox(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                       app_pc start_pc, app_pc end_pc, /* end is open */
                       bool record_translation, bool for_cache)
{
    instr_t *instr, *next;

    if (!INTERNAL_OPTION(cache_consistency))
        return true; /* nothing to do */

    /* this code assumes bb covers single, contiguous region */
    ASSERT((flags & FRAG_HAS_DIRECT_CTI) == 0);

    /* store first instr so loop below will skip top check */
    instr = instrlist_first_expanded(dcontext, ilist);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */
    if (record_translation) {
        /* make sure inserted instrs translate to the proper original instr */
        instrlist_set_translation_target(ilist, instr_get_raw_bits(instr));
    }

    sandbox_top_of_bb(dcontext, ilist,
                      sandbox_top_of_bb_check_s2ro(dcontext, start_pc),
                      flags, start_pc, end_pc, for_cache,
                      NULL, NULL, NULL);

    if (INTERNAL_OPTION(sandbox_writes)) {
        for (; instr != NULL; instr = next) {
            int i, opcode;
            opnd_t op;

            opcode = instr_get_opcode(instr);
            if (!instr_valid(instr)) {
                /* invalid instr -- best to truncate block here, easiest way
                 * to do that and get all flags right is to re-build it,
                 * but this time we'll use full decode so we'll avoid the discrepancy
                 * between fast and full decode on invalid instr detection.
                 */
                if (record_translation)
                    instrlist_set_translation_target(ilist, NULL);
                instrlist_set_our_mangling(ilist, false); /* PR 267260 */
                return false;
            }
        
            /* don't mangle anything that mangle inserts! */
            next = instr_get_next_expanded(dcontext, ilist, instr);
            if (!instr_ok_to_mangle(instr))
                continue;
            /* don't mangle "meta-instruction that can fault" (xref PR 472190) */
            if (instr_is_meta_may_fault(instr))
                continue;
            if (record_translation) {
                /* make sure inserted instrs translate to the proper original instr */
                instrlist_set_translation_target(ilist, instr_get_raw_bits(instr));
            }

            if (opcode == OP_rep_ins || opcode == OP_rep_movs || opcode == OP_rep_stos) {
                sandbox_rep_instr(dcontext, ilist, instr, next, start_pc, end_pc);
                continue;
            }

            /* FIXME case 8165: optimize for multiple push/pop */
            for (i=0; i<instr_num_dsts(instr); i++) {
                op = instr_get_dst(instr, i);
                if (opnd_is_memory_reference(op)) {
                    /* ignore CALL* since last anyways */
                    if (instr_is_call_indirect(instr)) {
                        ASSERT(next != NULL && !instr_raw_bits_valid(next));
                        /* FIXME case 8165: why do we ever care about the last
                         * instruction modifying anything? 
                         */
                        /* conversion of IAT calls (but not elision)
                         * transforms this into a direct CALL,
                         * in that case 'next' is a direct jmp
                         * fall through, so has no exit flags
                         */
                        ASSERT(TEST(INSTR_CALL_EXIT, instr_exit_branch_type(next)) || 
                               (DYNAMO_OPTION(IAT_convert) && 
                                TEST(INSTR_IND_CALL_DIRECT, instr->flags)));

                        
                        LOG(THREAD, LOG_INTERP, 3, " ignoring CALL* at end of fragment\n");
                        /* This test could be done outside of this loop on
                         * destinations, but since it is rare it is faster
                         * to do it here.  Using continue instead of break in case
                         * it gets moved out.
                         */
                        continue;
                    }
                    sandbox_write(dcontext, ilist, instr, next, op, start_pc, end_pc);
                }
            }
        }
    }
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */
    return true;
}

/* Offsets within selfmod sandbox top-of-bb code that we patch once
 * the code is emitted, as the values depend on the emitted address.
 * These vary by whether sandbox_top_of_bb_check_s2ro() and whether
 * eflags are not written, all written, or just OF is written.
 * For the copy_size == 1 variation, we simply ignore the 2nd patch point.
 */
static bool selfmod_s2ro[] = { false, true };
static uint selfmod_eflags[] = { FRAG_WRITES_EFLAGS_6, FRAG_WRITES_EFLAGS_OF, 0 };
#define SELFMOD_NUM_S2RO   (sizeof(selfmod_s2ro)/sizeof(selfmod_s2ro[0]))
#define SELFMOD_NUM_EFLAGS (sizeof(selfmod_eflags)/sizeof(selfmod_eflags[0]))
#ifdef X64 /* additional complexity: start_pc > 4GB? */
static app_pc selfmod_gt4G[] = { NULL, (app_pc)(POINTER_MAX-2)/*so end can be +2*/ };
# define SELFMOD_NUM_GT4G   (sizeof(selfmod_gt4G)/sizeof(selfmod_gt4G[0]))
#endif
uint selfmod_copy_start_offs[SELFMOD_NUM_S2RO][SELFMOD_NUM_EFLAGS]
    IF_X64([SELFMOD_NUM_GT4G]);
uint selfmod_copy_end_offs[SELFMOD_NUM_S2RO][SELFMOD_NUM_EFLAGS]
    IF_X64([SELFMOD_NUM_GT4G]);

void
set_selfmod_sandbox_offsets(dcontext_t *dcontext)
{
    int i, j;
#ifdef X64
    int k;
#endif
    instrlist_t ilist;
    patch_list_t patch;
    static byte buf[256];
    uint len;
    /* We assume this is called at init, when .data is +w and we need no
     * synch on accessing buf */
    ASSERT(!dynamo_initialized);
    for (i = 0; i < SELFMOD_NUM_S2RO; i++) {
        for (j = 0; j < SELFMOD_NUM_EFLAGS; j++) {
#ifdef X64
            for (k = 0; k < SELFMOD_NUM_GT4G; k++) {
#endif
                cache_pc start_pc, end_pc;
                app_pc app_start;
                instrlist_init(&ilist);
                /* sandbox_top_of_bb assumes there's an instr there */
                instrlist_append(&ilist, INSTR_CREATE_label(dcontext));
                init_patch_list(&patch, PATCH_TYPE_ABSOLUTE);
                app_start = IF_X64_ELSE(selfmod_gt4G[k], NULL);
                sandbox_top_of_bb(dcontext, &ilist,
                                  selfmod_s2ro[i], selfmod_eflags[j],
                                  /* we must have a >1-byte region to get
                                   * both patch points */
                                  app_start, app_start + 2, false,
                                  &patch, &start_pc, &end_pc);
                len = encode_with_patch_list(dcontext, &patch, &ilist, buf);
                ASSERT(len < BUFFER_SIZE_BYTES(buf));
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(start_pc - buf)));
                selfmod_copy_start_offs[i][j]IF_X64([k]) = (uint) (start_pc - buf);
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(end_pc - buf)));
                selfmod_copy_end_offs[i][j]IF_X64([k]) = (uint) (end_pc - buf);
                LOG(THREAD, LOG_EMIT, 3, "selfmod offs %d %d"IF_X64(" %d")": %u %u\n",
                    i, j, IF_X64_(k)
                    selfmod_copy_start_offs[i][j]IF_X64([k]),
                    selfmod_copy_end_offs[i][j]IF_X64([k]));
                /* free the instrlist_t elements */
                instrlist_clear(dcontext, &ilist);
#ifdef X64
            }
#endif
        }
    }
}

void
finalize_selfmod_sandbox(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc copy_pc = FRAGMENT_SELFMOD_COPY_PC(f);
    byte *pc;
    int i, j;
#ifdef X64
    int k = ((ptr_uint_t)f->tag) > UINT_MAX ? 1 : 0;
#endif
    i = (sandbox_top_of_bb_check_s2ro(dcontext, f->tag)) ? 1 : 0;
    j = (TEST(FRAG_WRITES_EFLAGS_6, f->flags) ? 0 :
         (TEST(FRAG_WRITES_EFLAGS_OF, f->flags) ? 1 : 2));
    pc = FCACHE_ENTRY_PC(f) + selfmod_copy_start_offs[i][j]IF_X64([k]);
    *((cache_pc*)pc) = copy_pc;
    if (FRAGMENT_SELFMOD_COPY_SIZE(f) - sizeof(uint) > 1) {
        pc = FCACHE_ENTRY_PC(f) + selfmod_copy_end_offs[i][j]IF_X64([k]);
        /* subtract the size itself, stored at the end of the copy */
        *((cache_pc*)pc) = (copy_pc + FRAGMENT_SELFMOD_COPY_SIZE(f) - sizeof(uint));
    } /* else, no 2nd patch point */
}
