/* ******************************************************************************
 * Copyright (c) 2010-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* file "mangle_shared.c" */

#include "../globals.h"
#include "arch.h"
#include "instr_create.h"
#include "instrument.h"  /* for insert_get_mcontext_base */
#include "decode_fast.h" /* for decode_next_pc */

#ifdef ANNOTATIONS
#    include "../annotations.h"
#endif

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

clean_call_info_t default_clean_call_info;
callee_info_t default_callee_info;

/* the stack size of a full context switch for clean call */
int
get_clean_call_switch_stack_size(void)
{
    return ALIGN_FORWARD(sizeof(priv_mcontext_t), get_ABI_stack_alignment());
}

/* extra temporarily-used stack usage beyond
 * get_clean_call_switch_stack_size()
 */
int
get_clean_call_temp_stack_size(void)
{
#ifdef X86
    return XSP_SZ; /* for eflags clear code: push 0; popf */
#else
    return 0;
#endif
}

/* utility routines for inserting clean calls to an instrumentation routine
 * strategy is very similar to fcache_enter/return
 * FIXME: try to share code with fcache_enter/return?
 *
 * first swap stacks to DynamoRIO stack:
 *      SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *      RESTORE_FROM_DCONTEXT dstack_OFFSET,%xsp
 * swap peb/teb fields
 * now save app eflags and registers, being sure to lay them out on
 * the stack in priv_mcontext_t order:
 *      push $0 # for priv_mcontext_t.pc; wasted, for now
 *      pushf
 *      pusha # xsp is dstack-XSP_SZ*2; rest are app values
 * clear the eflags for our usage
 * ASSUMPTION (also made in x86.asm): 0 ok, reserved bits are not set by popf,
 *                                    and clearing, not preserving, is good enough
 *      push   $0
 *      popf
 * make the call
 *      call routine
 * restore app regs and eflags
 *      popa
 *      popf
 *      lea XSP_SZ(xsp),xsp # clear priv_mcontext_t.pc slot
 * swap peb/teb fields
 * restore app stack
 *      RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 */

void
insert_get_mcontext_base(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    PRE(ilist, where, instr_create_restore_from_tls(dcontext, reg, TLS_DCONTEXT_SLOT));

    /* An extra level of indirection with SELFPROT_DCONTEXT */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        ASSERT_NOT_TESTED();
        PRE(ilist, where,
            XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                              OPND_CREATE_MEMPTR(reg, offsetof(dcontext_t, upcontext))));
    }
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
/* number of extra slots in addition to register slots. */
#define NUM_EXTRA_SLOTS 2 /* pc, aflags */
uint
prepare_for_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *instr, byte *encode_pc)
{
    uint dstack_offs = 0;

    if (cci == NULL)
        cci = &default_clean_call_info;

    /* Swap stacks.  For thread-shared, we need to get the dcontext
     * dynamically rather than use the constant passed in here.  Save
     * away xax in a TLS slot and then load the dcontext there.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
        insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);
#ifdef AARCH64
        /* We need an addtional scratch register for saving the SP.
         * TLS_REG1_SLOT is not safe since it may be used by clients.
         * Instead we save it to dcontext.mcontext.x0, which is not
         * used by dr_save_reg (see definition of SPILL_SLOT_MC_REG).
         */
        PRE(ilist, instr,
            XINST_CREATE_store(dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG0, 0),
                               opnd_create_reg(SCRATCH_REG1)));
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(SCRATCH_REG1),
                              opnd_create_reg(DR_REG_XSP)));
        PRE(ilist, instr,
            XINST_CREATE_store(dcontext,
                               opnd_create_dcontext_field_via_reg_sz(
                                   dcontext, SCRATCH_REG0, XSP_OFFSET, OPSZ_PTR),
                               opnd_create_reg(SCRATCH_REG1)));
#else
        PRE(ilist, instr,
            instr_create_save_to_dc_via_reg(dcontext, SCRATCH_REG0, REG_XSP, XSP_OFFSET));
#endif
        /* DSTACK_OFFSET isn't within the upcontext so if it's separate this won't
         * work right.  FIXME - the dcontext accessing routines are a mess of shared
         * vs. no shared support, separate context vs. no separate context support etc. */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));

#ifdef WINDOWS
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         * We don't support non-SCRATCH_ALWAYS_TLS.
         */
        /* XXX: should use clean callee analysis to remove pieces of this
         * such as errno preservation
         */
        if (!cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX /*dc*/, REG_XSP /*scratch*/, true /*to priv*/);
        }
#endif
#ifdef AARCH64
        PRE(ilist, instr,
            XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                              opnd_create_dcontext_field_via_reg_sz(
                                  dcontext, SCRATCH_REG0, DSTACK_OFFSET, OPSZ_PTR)));
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_XSP),
                              opnd_create_reg(SCRATCH_REG1)));
        /* Restore scratch_reg from dcontext.mcontext.x0. */
        PRE(ilist, instr,
            XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                              OPND_CREATE_MEMPTR(SCRATCH_REG0, 0)));
#else
        PRE(ilist, instr,
            instr_create_restore_from_dc_via_reg(dcontext, SCRATCH_REG0, REG_XSP,
                                                 DSTACK_OFFSET));
#endif
        /* Restore SCRATCH_REG0 before pushing the context on the dstack. */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
    } else {
        IF_AARCH64(ASSERT_NOT_REACHED());
        PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_XSP, XSP_OFFSET));
#ifdef WINDOWS
        if (!cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX /*unused*/, REG_XSP /*scratch*/, true /*to priv*/);
        }
#endif
        PRE(ilist, instr, instr_create_restore_dynamo_stack(dcontext));
    }

    /* Save flags and all registers, in priv_mcontext_t order.
     * We're at base of dstack so should be nicely aligned.
     */
    ASSERT(ALIGNED(dcontext->dstack, PAGE_SIZE));

    /* Note that we do NOT bother to put the correct pre-push app xsp value on the
     * stack here, as an optimization for callees who never ask for it: instead we
     * rely on dr_[gs]et_mcontext() to fix it up if asked for.  We can get away w/
     * this while hotpatching cannot (hotp_inject_gateway_call() fixes it up every
     * time) b/c the callee has to ask for the priv_mcontext_t.
     */
    if (cci->out_of_line_swap) {
        dstack_offs +=
            insert_out_of_line_context_switch(dcontext, ilist, instr, true, encode_pc);
    } else {
        dstack_offs +=
            insert_push_all_registers(dcontext, cci, ilist, instr, (uint)PAGE_SIZE,
                                      OPND_CREATE_INT32(0), REG_NULL _IF_AARCH64(false));

        insert_clear_eflags(dcontext, cci, ilist, instr);
        /* XXX: add a cci field for optimizing this away if callee makes no calls */
    }

    /* We no longer need to preserve the app's errno on Windows except
     * when using private libraries, so its preservation is in
     * preinsert_swap_peb().
     * We do not need to preserve DR's Linux errno across app execution.
     */

    /* check if need adjust stack for alignment. */
    if (cci->should_align) {
#if (defined(X86) && defined(X64)) || defined(MACOS)
        /* PR 218790: maintain 16-byte rsp alignment.
         * insert_parameter_preparation() currently assumes we leave rsp aligned.
         */
        uint num_slots = NUM_GP_REGS + NUM_EXTRA_SLOTS;
        if (cci->skip_save_flags)
            num_slots -= 2;
        num_slots -= cci->num_regs_skip; /* regs that not saved */
        /* For out-of-line calls, the stack size gets aligned by
         * get_clean_call_switch_stack_size.
         */
        if (!cci->out_of_line_swap && (num_slots % 2) == 1) {
            ASSERT((dstack_offs % 16) == 8);
            PRE(ilist, instr,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -(int)XSP_SZ)));
            dstack_offs += XSP_SZ;
        }
#endif
        ASSERT((dstack_offs % get_ABI_stack_alignment()) == 0);
    }
    ASSERT(cci->skip_save_flags || cci->num_simd_skip != 0 || cci->num_regs_skip != 0 ||
           (int)dstack_offs ==
               (get_clean_call_switch_stack_size() + clean_call_beyond_mcontext()));
    return dstack_offs;
}

void
cleanup_after_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, byte *encode_pc)
{
    if (cci == NULL)
        cci = &default_clean_call_info;
        /* saved error code is currently on the top of the stack */

#if (defined(X86) && defined(X64)) || defined(MACOS)
    /* PR 218790: remove the padding we added for 16-byte rsp alignment */
    if (cci->should_align) {
        uint num_slots = NUM_GP_REGS + NUM_EXTRA_SLOTS;
        if (cci->skip_save_flags)
            num_slots += 2;
        num_slots -= cci->num_regs_skip; /* regs that not saved */
        /* For out-of-line calls, the stack size gets aligned by
         * get_clean_call_switch_stack_size.
         */
        if (!cci->out_of_line_swap && (num_slots % 2) == 1) {
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, XSP_SZ)));
        }
    }
#endif

    /* now restore everything */
    if (cci->out_of_line_swap) {
        insert_out_of_line_context_switch(dcontext, ilist, instr, false, encode_pc);
    } else {
        /* XXX: add a cci field for optimizing this away if callee makes no calls */
        insert_pop_all_registers(dcontext, cci, ilist, instr,
                                 /* see notes in prepare_for_clean_call() */
                                 (uint)PAGE_SIZE _IF_AARCH64(false));
    }

    /* Swap stacks back.  For thread-shared, we need to get the dcontext
     * dynamically.  Save xax in TLS so we can use it as scratch.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));

        insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);

#ifdef WINDOWS
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         * We don't support non-SCRATCH_ALWAYS_TLS.
         */
        if (!cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX /*dc*/, REG_XSP /*scratch*/, false /*to app*/);
        }
#endif

#ifdef AARCH64
        /* TLS_REG1_SLOT is not safe since it may be used by clients.
         * We save it to dcontext.mcontext.x0.
         */
        PRE(ilist, instr,
            XINST_CREATE_store(dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG0, 0),
                               opnd_create_reg(SCRATCH_REG1)));
        PRE(ilist, instr,
            XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                              opnd_create_dcontext_field_via_reg_sz(
                                  dcontext, SCRATCH_REG0, XSP_OFFSET, OPSZ_PTR)));
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_XSP),
                              opnd_create_reg(SCRATCH_REG1)));
        /* Restore scratch_reg from dcontext.mcontext.x0. */
        PRE(ilist, instr,
            XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                              OPND_CREATE_MEMPTR(SCRATCH_REG0, 0)));
#else
        PRE(ilist, instr,
            instr_create_restore_from_dc_via_reg(dcontext, SCRATCH_REG0, REG_XSP,
                                                 XSP_OFFSET));
#endif
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
    } else {
        IF_AARCH64(ASSERT_NOT_REACHED());
#ifdef WINDOWS
        if (!cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX /*unused*/, REG_XSP /*scratch*/, false /*to app*/);
        }
#endif
        PRE(ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_XSP, XSP_OFFSET));
    }
}

bool
parameters_stack_padded(void)
{
    return (REGPARM_MINSTACK > 0 || REGPARM_END_ALIGN > XSP_SZ);
}

/* Inserts a complete call to callee with the passed-in arguments.
 * For x64, assumes the stack pointer is currently 16-byte aligned.
 * Clean calls ensure this by using clean base of dstack and having
 * dr_prepare_for_call pad to 16 bytes.
 * Returns whether the call is direct.
 */
bool
insert_meta_call_vargs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       meta_call_flags_t flags, byte *encode_pc, void *callee,
                       uint num_args, opnd_t *args)
{
    instr_t *in = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    bool direct;
    uint stack_for_params = insert_parameter_preparation(
        dcontext, ilist, instr, TEST(META_CALL_CLEAN, flags), num_args, args);
    IF_X64(ASSERT(ALIGNED(stack_for_params, 16)));

#ifdef CLIENT_INTERFACE
    if (TEST(META_CALL_CLEAN, flags) && should_track_where_am_i()) {
        if (SCRATCH_ALWAYS_TLS()) {
#    ifdef AARCHXX
            /* DR_REG_LR is dead here */
            insert_get_mcontext_base(dcontext, ilist, instr, DR_REG_LR);
            /* TLS_REG0_SLOT is not safe since it may be used by clients.
             * We save it to dcontext.mcontext.x0.
             */
            PRE(ilist, instr,
                XINST_CREATE_store(dcontext, OPND_CREATE_MEMPTR(DR_REG_LR, 0),
                                   opnd_create_reg(SCRATCH_REG0)));
            instrlist_insert_mov_immed_ptrsz(dcontext, (ptr_int_t)DR_WHERE_CLEAN_CALLEE,
                                             opnd_create_reg(SCRATCH_REG0), ilist, instr,
                                             NULL, NULL);
            PRE(ilist, instr,
                instr_create_save_to_dc_via_reg(dcontext, DR_REG_LR, SCRATCH_REG0,
                                                WHEREAMI_OFFSET));
            /* Restore scratch_reg from dcontext.mcontext.x0. */
            PRE(ilist, instr,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG0),
                                  OPND_CREATE_MEMPTR(DR_REG_LR, 0)));
#    else
            /* SCRATCH_REG0 is dead here, because clean calls only support "cdecl",
             * which specifies that the caller must save xax (and xcx and xdx).
             */
            insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);
            PRE(ilist, instr,
                instr_create_save_immed_to_dc_via_reg(
                    dcontext, SCRATCH_REG0, WHEREAMI_OFFSET, (uint)DR_WHERE_CLEAN_CALLEE,
                    OPSZ_4));
#    endif
        } else {
            PRE(ilist, instr,
                XINST_CREATE_store(dcontext,
                                   opnd_create_dcontext_field(dcontext, WHEREAMI_OFFSET),
                                   OPND_CREATE_INT32(DR_WHERE_CLEAN_CALLEE)));
        }
    }
#endif

    /* If we need an indirect call, we use r11 as the last of the scratch regs.
     * We document this to clients using dr_insert_call_ex() or DR_CLEANCALL_INDIRECT.
     */
    direct = insert_reachable_cti(dcontext, ilist, instr, encode_pc, (byte *)callee,
                                  false /*call*/, TEST(META_CALL_RETURNS, flags),
                                  false /*!precise*/, DR_REG_R11, NULL);
    if (stack_for_params > 0) {
        /* XXX PR 245936: let user decide whether to clean up?
         * i.e., support calling a stdcall routine?
         */
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT32(stack_for_params)));
    }

#ifdef CLIENT_INTERFACE
    if (TEST(META_CALL_CLEAN, flags) && should_track_where_am_i()) {
        uint whereami;

        if (TEST(META_CALL_RETURNS_TO_NATIVE, flags))
            whereami = (uint)DR_WHERE_APP;
        else
            whereami = (uint)DR_WHERE_FCACHE;

        if (SCRATCH_ALWAYS_TLS()) {
            /* SCRATCH_REG0 is dead here: restore of the app stack will clobber xax */
            insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);
#    ifdef AARCHXX
            /* TLS_REG1_SLOT is not safe since it may be used by clients.
             * We save it to dcontext.mcontext.x0.
             */
            PRE(ilist, instr,
                XINST_CREATE_store(dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG0, 0),
                                   opnd_create_reg(SCRATCH_REG1)));
            instrlist_insert_mov_immed_ptrsz(dcontext, (ptr_int_t)whereami,
                                             opnd_create_reg(SCRATCH_REG1), ilist, instr,
                                             NULL, NULL);
            PRE(ilist, instr,
                instr_create_save_to_dc_via_reg(dcontext, SCRATCH_REG0, SCRATCH_REG1,
                                                WHEREAMI_OFFSET));
            /* Restore scratch_reg from dcontext.mcontext.x0. */
            PRE(ilist, instr,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                                  OPND_CREATE_MEMPTR(SCRATCH_REG0, 0)));
#    else
            PRE(ilist, instr,
                instr_create_save_immed_to_dc_via_reg(dcontext, SCRATCH_REG0,
                                                      WHEREAMI_OFFSET, whereami, OPSZ_4));
#    endif
        } else {
            PRE(ilist, instr,
                XINST_CREATE_store(dcontext,
                                   opnd_create_dcontext_field(dcontext, WHEREAMI_OFFSET),
                                   OPND_CREATE_INT32(whereami)));
        }
    }
#endif

    /* mark it all meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != instr) {
        instr_set_meta(in);
        in = instr_get_next(in);
    }
    return direct;
}

/*###########################################################################
 *###########################################################################
 *
 *   M A N G L I N G   R O U T I N E S
 */

app_pc
get_app_instr_xl8(instr_t *instr)
{
    /* assumption: target's translation or raw bits are set properly */
    app_pc xl8 = instr_get_translation(instr);
    if (xl8 == NULL && instr_raw_bits_valid(instr))
        xl8 = instr_get_raw_bits(instr);
    return xl8;
}

ptr_uint_t
get_call_return_address(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    ptr_uint_t retaddr, curaddr;

#ifdef CLIENT_INTERFACE
    /* i#620: provide API to set fall-through and retaddr targets at end of bb */
    if (instr_is_call(instr) && instrlist_get_return_target(ilist) != NULL) {
        retaddr = (ptr_uint_t)instrlist_get_return_target(ilist);
        LOG(THREAD, LOG_INTERP, 3, "set return target " PFX " by client\n", retaddr);
        return retaddr;
    }
#endif
    /* For CI builds, use the translation field so we can handle cases
     * where the client has changed the target and invalidated the raw
     * bits.  We'll make sure the translation is always set for direct
     * calls.
     *
     * If a client changes an instr, or our own mangle_rel_addr() does,
     * the raw bits won't be valid but the translation should be.
     */
    curaddr = (ptr_uint_t)get_app_instr_xl8(instr);
    ASSERT(curaddr != 0);
    /* we use the next app instruction as return address as the client
     * or DR may change the instruction and so its length.
     */
    if (instr_raw_bits_valid(instr) &&
        instr_get_translation(instr) == instr_get_raw_bits(instr)) {
        /* optimization, if nothing changes, use instr->length to avoid
         * calling decode_next_pc.
         */
        retaddr = curaddr + instr->length;
    } else {
        retaddr = (ptr_uint_t)decode_next_pc(dcontext, (byte *)curaddr);
    }
    return retaddr;
}

#ifdef UNIX
/* find the system call number in instrlist for an inlined system call
 * by simpling walking the ilist backward and finding "mov immed => %eax"
 * without checking cti or expanding instr
 */
static int
ilist_find_sysnum(instrlist_t *ilist, instr_t *instr)
{
    for (; instr != NULL; instr = instr_get_prev(instr)) {
        ptr_int_t val;
        if (instr_is_app(instr) && instr_is_mov_constant(instr, &val) &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            reg_to_pointer_sized(opnd_get_reg(instr_get_dst(instr, 0))) ==
                reg_to_pointer_sized(DR_REG_SYSNUM))
            return (int)val;
    }
    ASSERT_NOT_REACHED();
    return -1;
}
#endif

static void
mangle_syscall(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
               instr_t *next_instr)
{
#ifdef UNIX
    if (get_syscall_method() != SYSCALL_METHOD_INT &&
        get_syscall_method() != SYSCALL_METHOD_SYSCALL &&
        get_syscall_method() != SYSCALL_METHOD_SYSENTER &&
        get_syscall_method() != SYSCALL_METHOD_SVC) {
        /* don't know convention on return address from kernel mode! */
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        CLIENT_ASSERT(false,
                      "Unsupported system call method detected. Please "
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
    instr_t *skip_exit = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, XINST_CREATE_jump_short(dcontext, opnd_create_instr(skip_exit)));
    /* assumption: raw bits of instr == app pc */
    ASSERT(instr_get_raw_bits(instr) != NULL);
    /* this should NOT be a meta-instr so we don't use PRE */
    /* note that it's ok if this gets linked: we unlink all outgoing exits in
     * addition to changing the skip_exit jmp upon receiving a signal
     */
    instrlist_preinsert(
        ilist, instr,
        XINST_CREATE_jump(dcontext, opnd_create_pc(instr_get_raw_bits(instr))));
    PRE(ilist, instr, skip_exit);

    if (does_syscall_ret_to_callsite() &&
        sysnum_is_not_restartable(ilist_find_sysnum(ilist, instr))) {
        /* i#1216: we insert a nop instr right after inlined non-auto-restart
         * syscall to make it a safe point for suspending.
         */
        instr_t *nop = XINST_CREATE_nop(dcontext);
        /* We make a fake app nop instr for easy handling in recreate_app_state.
         * XXX: it is cleaner to mark our-mangling and handle it, but it seems
         * ok to use a fake app nop instr, since the client won't see it.
         */
        INSTR_XL8(nop, (instr_get_translation(instr) + instr_length(dcontext, instr)));
        instr_set_app(instr);
        instrlist_postinsert(ilist, instr, nop);
    }
#endif /* UNIX */

    mangle_syscall_arch(dcontext, ilist, flags, instr, next_instr);
}

#ifdef UNIX
/* If skip is false:
 *   changes the jmp right before the next syscall (after pc) to target the
 *   exit cti immediately following it;
 * If skip is true:
 *   changes back to the default, where skip hops over the exit cti,
 *   which is assumed to be located at pc.
 */
bool
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip)
{
    byte *stop_pc = fragment_body_end_pc(dcontext, f);
    byte *target, *prev_pc, *cti_pc = NULL, *skip_pc = NULL;
    instr_t instr;
    DEBUG_DECLARE(instr_t cti;)
    instr_init(dcontext, &instr);
    DODEBUG({ instr_init(dcontext, &cti); });
    LOG(THREAD, LOG_SYSCALLS, 3, "mangle_syscall_code: pc=" PFX ", skip=%d\n", pc, skip);
    do {
        instr_reset(dcontext, &instr);
        prev_pc = pc;
        pc = decode(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* our own code! */
        if (instr_get_opcode(&instr) ==
            OP_jmp_short
                /* For A32 it's not OP_b_short */
                IF_ARM(||
                       (instr_get_opcode(&instr) == OP_jmp &&
                        opnd_get_pc(instr_get_target(&instr)) == pc + ARM_INSTR_SIZE)))
            skip_pc = prev_pc;
        else if (instr_get_opcode(&instr) == OP_jmp)
            cti_pc = prev_pc;
        if (pc >= stop_pc) {
            LOG(THREAD, LOG_SYSCALLS, 3, "\tno syscalls found\n");
            instr_free(dcontext, &instr);
            return false;
        }
    } while (!instr_is_syscall(&instr));
    if (skip_pc == NULL) {
        /* signal happened after skip jmp: nothing we can do here
         *
         * FIXME PR 213040: we should tell caller difference between
         * "no syscalls" and "too-close syscall" and have it take
         * other actions to bound signal delay
         */
        instr_free(dcontext, &instr);
        return false;
    }
    ASSERT(skip_pc != NULL && cti_pc != NULL);
    /* jmps are right before syscall, but there can be nops to pad exit cti on x86 */
    ASSERT(cti_pc == prev_pc - JMP_LONG_LENGTH);
    ASSERT(skip_pc < cti_pc);
    ASSERT(
        skip_pc ==
        cti_pc -
            JMP_SHORT_LENGTH IF_X86(|| *(cti_pc - JMP_SHORT_LENGTH) == RAW_OPCODE_nop));
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, skip_pc, &instr);
    ASSERT(pc != NULL); /* our own code! */
    ASSERT(instr_get_opcode(&instr) ==
           OP_jmp_short
               /* For A32 it's not OP_b_short */
               IF_ARM(||
                      (instr_get_opcode(&instr) == OP_jmp &&
                       opnd_get_pc(instr_get_target(&instr)) == pc + ARM_INSTR_SIZE)));
    ASSERT(pc <= cti_pc); /* could be nops */
    DOCHECK(1, {
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
        byte *nxt_pc;
        LOG(THREAD, LOG_SYSCALLS, 3, "\tmodifying target of syscall jmp to " PFX "\n",
            target);
        instr_set_target(&instr, opnd_create_pc(target));
        nxt_pc = instr_encode(dcontext, &instr, skip_pc);
        ASSERT(nxt_pc != NULL && nxt_pc == cti_pc);
        machine_cache_sync(skip_pc, nxt_pc, true);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3, "\ttarget of syscall jmp is already " PFX "\n",
            target);
    }
    instr_free(dcontext, &instr);
    return true;
}
#endif /* UNIX */

/* TOP-LEVEL MANGLE
 * This routine is responsible for mangling a fragment into the form
 * we'd like prior to placing it in the code cache
 * If mangle_calls is false, ignores calls
 * If record_translation is true, records translation target for each
 * inserted instr -- but this slows down encoding in current implementation
 */
void
d_r_mangle(dcontext_t *dcontext, instrlist_t *ilist, uint *flags INOUT, bool mangle_calls,
           bool record_translation)
{
    instr_t *instr, *next_instr;
#ifdef WINDOWS
    bool ignorable_sysenter = DYNAMO_OPTION(ignore_syscalls) &&
        DYNAMO_OPTION(ignore_syscalls_follow_sysenter) &&
        (get_syscall_method() == SYSCALL_METHOD_SYSENTER) &&
        TEST(FRAG_HAS_SYSCALL, *flags);
#endif

    /* Walk through instr list:
     * -- convert exit branches to use near_rel form;
     * -- convert direct calls into 'push %eip', aka return address;
     * -- convert returns into 'pop %xcx (; add $imm, %xsp)';
     * -- convert indirect branches into 'save %xcx; lea EA, %xcx';
     * -- convert indirect calls as a combination of direct call and
     *    indirect branch conversion;
     * -- ifdef STEAL_REGISTER, steal edi for our own use.
     * -- ifdef UNIX, mangle seg ref and mov_seg
     */

    /* Mangling routines need to be careful about whether or not to flag mangling
     * epilogue instructions (xref i#3307).
     * -- should be marked with mangling epilogue flag, if it can be translated to
          the next PC post-app instruction using/abusing translate_walk_restore.
     * -- should not be marked with mangling epilogue flag, it either is 1) logically not
          a PC post-app instruction, which is the case for control-flow instructions.
          Or 2) it is unsupported to advance to the next PC, and we're making the
          assumption here that all such instructions can be fully rolled back to the
          current PC.
     * Mangling routines should set mangling epilogue flag manually. This could get
     * improved by doing this automatically for next_instr, unless explictly flagged.
     */

    KSTART(mangling);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */

#ifdef ARM
    if (INTERNAL_OPTION(store_last_pc)) {
        /* This is a simple debugging feature.  There's a chance that some
         * mangling clobbers the r3 slot but it's slim, and it's much
         * simpler to put this at the top than try to put it right before
         * the exit cti(s).
         */
        PRE(ilist, instrlist_first(ilist),
            instr_create_save_to_tls(dcontext, DR_REG_PC, TLS_REG3_SLOT));
    }
#endif

    for (instr = instrlist_first(ilist); instr != NULL; instr = next_instr) {

        /* don't mangle anything that mangle inserts! */
        next_instr = instr_get_next(instr);

        if (!instr_opcode_valid(instr))
            continue;

#ifdef ANNOTATIONS
        if (is_annotation_return_placeholder(instr)) {
            instrlist_remove(ilist, instr);
            instr_destroy(dcontext, instr);
            continue;
        }
#endif

        if (record_translation) {
            /* make sure inserted instrs translate to the original instr */
            app_pc xl8 = get_app_instr_xl8(instr);
            instrlist_set_translation_target(ilist, xl8);
        }

#ifdef X86_64
        if (DYNAMO_OPTION(x86_to_x64) &&
            IF_WINDOWS_ELSE(is_wow64_process(NT_CURRENT_PROCESS), false) &&
            instr_get_x86_mode(instr))
            translate_x86_to_x64(dcontext, ilist, &instr);
#endif

#if defined(UNIX) && defined(X86)
        if (INTERNAL_OPTION(mangle_app_seg) && instr_is_app(instr)) {
            /* The instr might be changed by client, and we cannot rely on
             * PREFIX_SEG_FS/GS. So we simply call mangle_seg_ref on every
             * instruction and mangle it if necessary.
             */
            mangle_seg_ref(dcontext, ilist, instr, next_instr);
            if (instr_get_opcode(instr) == OP_mov_seg)
                mangle_mov_seg(dcontext, ilist, instr, next_instr);
        }
#endif

#ifdef X86
        if (instr_saves_float_pc(instr) && instr_is_app(instr)) {
            mangle_float_pc(dcontext, ilist, instr, next_instr, flags);
        }
#endif

#ifdef AARCH64
        if (instr_is_icache_op(instr) && instr_is_app(instr)) {
            next_instr = mangle_icache_op(dcontext, ilist, instr, next_instr,
                                          get_app_instr_xl8(instr) + AARCH64_INSTR_SIZE);
            continue;
        }
#endif

#if defined(X64) || defined(ARM)
        /* i#393: mangle_rel_addr might destroy the instr if it is a LEA,
         * which makes instr point to freed memory.
         * In such case, the control should skip later checks on the instr
         * for exit_cti and syscall.
         * skip the rest of the loop if instr is destroyed.
         */
        if (instr_has_rel_addr_reference(instr)
            /* XXX i#1834: it should be up to the app to re-relativize, yet on amd64
             * our own samples are relying on DR re-relativizing (and we just haven't
             * run big enough apps to hit reachability problems) so for now we continue
             * mangling meta instrs for x86 builds.
             */
            IF_ARM(&&instr_is_app(instr))) {
            instr_t *res = mangle_rel_addr(dcontext, ilist, instr, next_instr);
            /* Either returns NULL == destroyed "instr", or a new next_instr */
            if (res == NULL)
                continue;
            else
                next_instr = res;
        }
#endif /* X64 || ARM */

#ifdef AARCHXX
        if (!instr_is_meta(instr) && instr_reads_thread_register(instr)) {
            next_instr = mangle_reads_thread_register(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif /* ARM || AARCH64 */

#ifdef AARCH64
        if (!instr_is_meta(instr) && instr_writes_thread_register(instr)) {
            next_instr =
                mangle_writes_thread_register(dcontext, ilist, instr, next_instr);
            continue;
        }

        if (!instr_is_meta(instr) && instr_uses_reg(instr, dr_reg_stolen))
            next_instr = mangle_special_registers(dcontext, ilist, instr, next_instr);
#endif /* AARCH64 */

#ifdef ARM
        /* Our stolen reg model is to expose to the client.  We assume that any
         * meta instrs using it are using it as TLS.  Ditto w/ use of PC.
         */
        if (!instr_is_meta(instr) &&
            (instr_uses_reg(instr, DR_REG_PC) || instr_uses_reg(instr, dr_reg_stolen)))
            next_instr = mangle_special_registers(dcontext, ilist, instr, next_instr);
#endif /* ARM */

        if (instr_is_exit_cti(instr)) {
#ifdef X86
            mangle_exit_cti_prefixes(dcontext, instr);
#endif

            /* to avoid reachability problems we convert all
             * 8-bit-offset jumps that exit the fragment to 32-bit.
             * Note that data16 jmps are implicitly converted via the
             * absolute target and loss of prefix info (xref PR 225937).
             */
            if (instr_is_cti_short(instr)) {
                /* convert short jumps */
                convert_to_near_rel(dcontext, instr);
            }
        }

#ifdef ANNOTATIONS
        if (is_annotation_label(instr)) {
            mangle_annotation_helper(dcontext, instr, ilist);
            continue;
        }
#endif

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
                mangle_syscall(dcontext, ilist, *flags, instr, next_instr);
            continue;
        } else if (instr_is_interrupt(instr)) { /* non-syscall interrupt */
            mangle_interrupt(dcontext, ilist, instr, next_instr);
            continue;
        }
#ifdef X86
        /*
         * i#2144 : We look for single step exceptions generation.
         */
        else if (instr_can_set_single_step(instr) && instr_get_opcode(instr) != OP_iret) {
            /* iret is handled in mangle_return. */
            mangle_possible_single_step(dcontext, ilist, instr);
            continue;
        } else if (dcontext->single_step_addr != NULL && instr_is_app(instr) &&
                   dcontext->single_step_addr == instr->translation) {
            instr_t *last_addr = instr_get_next_app(instr);
            /* Checks if sandboxing added another app instruction. */
            if (last_addr == NULL || last_addr->translation != instr->translation) {
                mangle_single_step(dcontext, ilist, *flags, instr);
                /* Resets to generate single step exception only once. */
                dcontext->single_step_addr = NULL;
            }
        }
#endif
#ifdef FOOL_CPUID
        else if (instr_get_opcode(instr) == OP_cpuid) {
            mangle_cpuid(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif

        if (!instr_is_cti(instr) || instr_is_meta(instr)) {
#ifdef STEAL_REGISTER
            steal_reg(dcontext, instr, ilist);
#endif
#ifdef CLIENT_INTERFACE
            if (TEST(INSTR_CLOBBER_RETADDR, instr->flags) && instr_is_label(instr)) {
                /* move the value to the note field (which the client cannot
                 * possibly use at this point) so we don't have to search for
                 * this label when we hit the ret instr
                 */
                dr_instr_label_data_t *data = instr_get_label_data_area(instr);
                instr_t *tmp;
                instr_t *ret = (instr_t *)data->data[0];
                CLIENT_ASSERT(ret != NULL,
                              "dr_clobber_retaddr_after_read()'s label is corrupted");
                /* avoid use-after-free if client removed the ret by ensuring
                 * this instr_t pointer does exist.
                 * note that we don't want to go searching based just on a flag
                 * as we want tight coupling w/ a pointer as a general way
                 * to store per-instr data outside of the instr itself.
                 */
                for (tmp = instr_get_next(instr); tmp != NULL;
                     tmp = instr_get_next(tmp)) {
                    if (tmp == ret) {
                        tmp->note = (void *)data->data[1]; /* the value to use */
                        break;
                    }
                }
            }
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
            next_instr = mangle_direct_call(dcontext, ilist, instr, next_instr,
                                            mangle_calls, *flags);
        } else if (instr_is_call_indirect(instr)) {
            next_instr = mangle_indirect_call(dcontext, ilist, instr, next_instr,
                                              mangle_calls, *flags);
        } else if (instr_is_return(instr)) {
            mangle_return(dcontext, ilist, instr, next_instr, *flags);
        } else if (instr_is_mbr(instr)) {
            next_instr = mangle_indirect_jump(dcontext, ilist, instr, next_instr, *flags);
#ifdef X86
        } else if (instr_get_opcode(instr) == OP_jmp_far) {
            mangle_far_direct_jump(dcontext, ilist, instr, next_instr, *flags);
#endif
        }
        /* else nothing to do, e.g. direct branches */
    }

#ifdef WINDOWS
    /* Do XP & 2003 ignore-syscalls processing now. */
    if (ignorable_sysenter) {
        /* Check for any syscalls and process them. */
        for (instr = instrlist_first(ilist); instr != NULL; instr = next_instr) {
            next_instr = instr_get_next(instr);
            if (instr_opcode_valid(instr) && instr_is_syscall(instr))
                mangle_syscall(dcontext, ilist, *flags, instr, next_instr);
        }
    }
#endif
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */

#if defined(X86) && defined(X64)
    if (!X64_CACHE_MODE_DC(dcontext)) {
        instr_t *in;
        for (in = instrlist_first(ilist); in != NULL; in = instr_get_next(in)) {
            if (instr_is_our_mangling(in)) {
                instr_set_x86_mode(in, true /*x86*/);
                instr_shrink_to_32_bits(in);
            }
        }
    }
#endif

    /* The following assertion should be guaranteed by fact that all
     * blocks end in some kind of branch, and the code above restores
     * the register state on a branch. */
    ASSERT(ilist->flags == 0);
    KSTOP(mangling);
}

/***************************************************************************
 * SYSCALL
 */

#ifdef CLIENT_INTERFACE
static bool
cti_is_normal_elision(instr_t *instr)
{
    instr_t *next;
    opnd_t tgt;
    app_pc next_pc;
    if (instr == NULL || instr_is_meta(instr))
        return false;
    if (!instr_is_ubr(instr) && !instr_is_call_direct(instr))
        return false;
    next = instr_get_next(instr);
    if (next == NULL || instr_is_meta(next))
        return false;
    tgt = instr_get_target(instr);
    next_pc = get_app_instr_xl8(next);
    if (opnd_is_pc(tgt) && next_pc != NULL && opnd_get_pc(tgt) == next_pc)
        return true;
    return false;
}
#endif

/* Tries to statically find the syscall number for the
 * syscall instruction instr.
 * Returns -1 upon failure.
 * Note that on MacOS, 32-bit Mach syscalls are encoded using negative numbers
 * (although -1 is invalid), so be sure to test for -1 and not just <0 as a failure
 * code.
 */
int
find_syscall_num(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int syscall = -1;
    ptr_int_t value;
    instr_t *prev = instr_get_prev(instr);
    /* Allow either eax or rax for x86_64 */
    reg_id_t sysreg = reg_to_pointer_sized(DR_REG_SYSNUM);
#ifdef CLIENT_INTERFACE
    instr_t *walk, *tgt;
#endif

    if (prev == NULL)
        return -1;
    prev = instr_get_prev_expanded(dcontext, ilist, instr);
    /* walk backwards looking for "mov imm->xax"
     * may be other instrs placing operands into registers
     * for the syscall in between
     */
    while (prev != NULL &&
           /* We skip meta instrs under the assumption that a meta write to
            * sysreg is undone before the syscall.  If a tool wants to change the
            * real sysreg they should use an app instr.
            */
           (!instr_is_app(prev) ||
            (!instr_is_syscall(prev) && !instr_is_interrupt(prev) &&
             !instr_writes_to_reg(prev, sysreg, DR_QUERY_INCLUDE_ALL)))) {
#ifdef CLIENT_INTERFACE
        /* If client added cti in between that skips over the syscall, bail
         * and assume non-ignorable.
         */
        if (instr_is_cti(prev) &&
            (instr_is_app(prev) || opnd_is_instr(instr_get_target(prev))) &&
            !(cti_is_normal_elision(prev) IF_WINDOWS(
                || instr_is_call_sysenter_pattern(prev, instr_get_next(prev), instr)))) {
            for (tgt = opnd_get_instr(instr_get_target(prev)); tgt != NULL;
                 tgt = instr_get_next_expanded(dcontext, ilist, tgt)) {
                if (tgt == instr)
                    break;
            }
            if (tgt == NULL) {
                LOG(THREAD, LOG_SYSCALLS, 3,
                    "%s: cti skips syscall: bailing on syscall number\n", __FUNCTION__);
                return -1;
            }
        }
#endif
        prev = instr_get_prev_expanded(dcontext, ilist, prev);
    }
    if (prev != NULL && !instr_is_predicated(prev) &&
        instr_is_mov_constant(prev, &value) && opnd_is_reg(instr_get_dst(prev, 0)) &&
        reg_to_pointer_sized(opnd_get_reg(instr_get_dst(prev, 0))) == sysreg) {
        IF_X64(ASSERT_TRUNCATE(int, int, value));
        syscall = (int)value;
        LOG(THREAD, LOG_SYSCALLS, 3, "%s: found syscall number write: %d\n", __FUNCTION__,
            syscall);
#ifdef ARM
        if (opnd_get_size(instr_get_dst(prev, 0)) != OPSZ_PTR) {
            /* sub-reg write: special-case movw,movt, else bail */
            if (instr_get_opcode(prev) == OP_movt) {
                ptr_int_t val2;
                prev = instr_get_prev_expanded(dcontext, ilist, prev);
                if (prev != NULL && instr_is_mov_constant(prev, &val2)) {
                    syscall = (int)(value << 16) | (val2 & 0xffff);
                } else
                    return -1;
            } else
                return -1;
        }
#endif
#ifdef CLIENT_INTERFACE
        /* If client added cti that skips over the write, bail and assume
         * non-ignorable.
         */
        for (walk = instrlist_first_expanded(dcontext, ilist);
             walk != NULL && walk != prev;
             walk = instr_get_next_expanded(dcontext, ilist, walk)) {
            if (instr_is_cti(walk) && opnd_is_instr(instr_get_target(walk))) {
                for (tgt = opnd_get_instr(instr_get_target(walk)); tgt != NULL;
                     tgt = instr_get_next_expanded(dcontext, ilist, tgt)) {
                    if (tgt == prev)
                        break;
                    if (tgt == instr) {
                        LOG(THREAD, LOG_SYSCALLS, 3,
                            "%s: cti skips write: invalidating syscall number\n",
                            __FUNCTION__);
                        return -1;
                    }
                }
            }
        }
#endif
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3, "%s: never found write of syscall number\n",
            __FUNCTION__);
    }
    IF_X64(ASSERT_TRUNCATE(int, int, syscall));
    return (int)syscall;
}

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

void
clean_call_info_init(clean_call_info_t *cci, void *callee, bool save_fpstate,
                     uint num_args)
{
    memset(cci, 0, sizeof(*cci));
    cci->callee = callee;
    cci->num_args = num_args;
    cci->save_fpstate = save_fpstate;
    cci->save_all_regs = true;
    cci->should_align = true;
    cci->callee_info = &default_callee_info;
}

void
mangle_init(void)
{
    mangle_arch_init();
    /* create a default func_info for:
     * 1. clean call callee that cannot be analyzed.
     * 2. variable clean_callees will not be updated during the execution
     *    and can be set write protected.
     */
#ifdef CLIENT_INTERFACE
    clean_call_opt_init();
    clean_call_info_init(&default_clean_call_info, NULL, false, 0);
#endif
}

void
mangle_exit(void)
{
#ifdef CLIENT_INTERFACE
    clean_call_opt_exit();
#endif
}
