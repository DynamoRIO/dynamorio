/* ******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
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
#include "instr_create_shared.h"
#include "instrument.h"  /* for insert_get_mcontext_base */
#include "decode_fast.h" /* for decode_next_pc */
#include "disassemble.h"

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

bool
clean_call_needs_simd(clean_call_info_t *cci)
{
    return (cci->preserve_mcontext ||
            cci->num_simd_skip !=
                proc_num_simd_registers()
                    IF_X86(|| cci->num_opmask_skip != proc_num_opmask_registers()));
}

/* Number of extra slots in addition to register slots. */
#define NUM_EXTRA_SLOTS 2 /* pc, aflags */

#if defined(X86) && (defined(X64) || defined(UNIX))
static uint
clean_call_prepare_stack_size(clean_call_info_t *cci)
{
    int simd = 0;
    if (clean_call_needs_simd(cci)) {
        simd =
            MCXT_TOTAL_SIMD_SLOTS_SIZE + MCXT_TOTAL_OPMASK_SLOTS_SIZE + PRE_XMM_PADDING;
    }
    uint num_slots = DR_NUM_GPR_REGS + NUM_EXTRA_SLOTS;
    if (cci->skip_save_flags)
        num_slots -= 2;
#    ifndef X86_32                   /* x86 uses pusha regardless of regs we could skip */
    num_slots -= cci->num_regs_skip; /* regs not saved */
#    endif
    return simd + num_slots * XSP_SZ;
}
#endif

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
 * Keeps the final stack pointer aligned to get_ABI_stack_alignment().
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
prepare_for_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *instr, byte *encode_pc)
{
    uint dstack_offs = 0;

    instr_t *start_label = INSTR_CREATE_label(dcontext);
    instr_set_note(start_label, (void *)DR_NOTE_CALL_SEQUENCE_START);
    PRE(ilist, instr, start_label);

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
#if defined(X86) && (defined(X64) || defined(UNIX))
        /* PR 218790: maintain 16-byte rsp alignment.
         * insert_parameter_preparation() currently assumes we leave rsp aligned.
         */
        int align = get_ABI_stack_alignment();
        int off = align - (dstack_offs % align);
        ASSERT(off % XSP_SZ == 0);
        /* Make sure cleanup_after_clean_call() can compute the same offset.
         * We could make the caller pass back in dstack_offs except for
         * dr_cleanup_after_call().
         */
        ASSERT(clean_call_prepare_stack_size(cci) == dstack_offs ||
               cci->out_of_line_swap);
        /* For out-of-line calls, the stack size gets aligned by
         * get_clean_call_switch_stack_size.
         */
        if (off != align && !cci->out_of_line_swap) {
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -off)));
            dstack_offs += off;
        }
#endif
        ASSERT((dstack_offs % get_ABI_stack_alignment()) == 0);
    }
    ASSERT(cci->skip_save_flags || cci->num_simd_skip != 0 ||
           IF_X86(cci->num_opmask_skip != 0 ||) cci->num_regs_skip != 0 ||
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

#if defined(X86) && (defined(X64) || defined(UNIX))
    /* PR 218790: remove the padding we added for 16-byte rsp alignment */
    if (cci->should_align) {
        int align = get_ABI_stack_alignment();
        int emulate_dstack_offs = clean_call_prepare_stack_size(cci);
        int off = align - (emulate_dstack_offs % align);
        /* For out-of-line calls, the stack size gets aligned by
         * get_clean_call_switch_stack_size.
         */
        if (off != align && !cci->out_of_line_swap) {
            /* XXX: We should optimize by combining this LEA with the LEA in
             * insert_meta_call_vargs() which cleans up parameter space.
             */
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, off)));
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
    instr_t *end_label = INSTR_CREATE_label(dcontext);
    instr_set_note(end_label, (void *)DR_NOTE_CALL_SEQUENCE_END);
    PRE(ilist, instr, end_label);
}

bool
parameters_stack_padded(void)
{
    return (REGPARM_MINSTACK > 0 || REGPARM_END_ALIGN > XSP_SZ);
}

/* Inserts a complete call to callee with the passed-in arguments.
 * Assumes the stack pointer is currently get_ABI_stack_alignment() aligned.
 * Clean calls ensure this by using clean base of dstack and having
 * dr_prepare_for_call pad to the ABI alignment.
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
    ASSERT(ALIGNED(stack_for_params, get_ABI_stack_alignment()));

    if (TEST(META_CALL_CLEAN, flags) && should_track_where_am_i()) {
        if (SCRATCH_ALWAYS_TLS()) {
#ifdef AARCHXX
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
                instr_create_save_to_dc_via_reg(
                    dcontext, DR_REG_LR,
                    IF_X64_ELSE(reg_64_to_32(SCRATCH_REG0), SCRATCH_REG0),
                    WHEREAMI_OFFSET));
            /* Restore scratch_reg from dcontext.mcontext.x0. */
            PRE(ilist, instr,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG0),
                                  OPND_CREATE_MEMPTR(DR_REG_LR, 0)));
#else
            /* SCRATCH_REG0 is dead here, because clean calls only support "cdecl",
             * which specifies that the caller must save xax (and xcx and xdx).
             */
            insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);
            PRE(ilist, instr,
                instr_create_save_immed_to_dc_via_reg(
                    dcontext, SCRATCH_REG0, WHEREAMI_OFFSET, (uint)DR_WHERE_CLEAN_CALLEE,
                    OPSZ_4));
#endif
        } else {
            PRE(ilist, instr,
                XINST_CREATE_store(
                    dcontext,
                    opnd_create_dcontext_field_sz(dcontext, WHEREAMI_OFFSET, OPSZ_4),
                    OPND_CREATE_INT32(DR_WHERE_CLEAN_CALLEE)));
        }
    }

    /* If we need an indirect call, we use r11 as the last of the scratch regs.
     * We document this to clients using dr_insert_call_ex() or DR_CLEANCALL_INDIRECT.
     */
    direct = insert_reachable_cti(dcontext, ilist, instr, encode_pc, (byte *)callee,
                                  false /*call*/, TEST(META_CALL_RETURNS, flags),
                                  false /*!precise*/, CALL_SCRATCH_REG, NULL);
    if (stack_for_params > 0) {
        /* XXX PR 245936: let user decide whether to clean up?
         * i.e., support calling a stdcall routine?
         * XXX: Combine with the LEA in cleanup_after_clean_call() which undoes
         * alignment padding from prepare_for_clean_call().
         */
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT32(stack_for_params)));
    }

    if (TEST(META_CALL_CLEAN, flags) && should_track_where_am_i()) {
        uint whereami;

        if (TEST(META_CALL_RETURNS_TO_NATIVE, flags))
            whereami = (uint)DR_WHERE_APP;
        else
            whereami = (uint)DR_WHERE_FCACHE;

        if (SCRATCH_ALWAYS_TLS()) {
            /* SCRATCH_REG0 is dead here: restore of the app stack will clobber xax */
            insert_get_mcontext_base(dcontext, ilist, instr, SCRATCH_REG0);
#ifdef AARCHXX
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
                instr_create_save_to_dc_via_reg(
                    dcontext, SCRATCH_REG0,
                    IF_X64_ELSE(reg_64_to_32(SCRATCH_REG1), SCRATCH_REG1),
                    WHEREAMI_OFFSET));
            /* Restore scratch_reg from dcontext.mcontext.x0. */
            PRE(ilist, instr,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                                  OPND_CREATE_MEMPTR(SCRATCH_REG0, 0)));
#else
            PRE(ilist, instr,
                instr_create_save_immed_to_dc_via_reg(dcontext, SCRATCH_REG0,
                                                      WHEREAMI_OFFSET, whereami, OPSZ_4));
#endif
        } else {
            PRE(ilist, instr,
                XINST_CREATE_store(
                    dcontext,
                    opnd_create_dcontext_field_sz(dcontext, WHEREAMI_OFFSET, OPSZ_4),
                    OPND_CREATE_INT32(whereami)));
        }
    }

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
 * MANGLING ROUTINES
 */

/* This routine is not shared with drdecode, so it's here instead of mangle_utils.c. */
void
patch_mov_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                      instr_t *last)
{
    patch_mov_immed_arch(dcontext, val, pc, first, last);
}

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

    /* i#620: provide API to set fall-through and retaddr targets at end of bb */
    if (instr_is_call(instr) && instrlist_get_return_target(ilist) != NULL) {
        retaddr = (ptr_uint_t)instrlist_get_return_target(ilist);
        LOG(THREAD, LOG_INTERP, 3, "set return target " PFX " by client\n", retaddr);
        return retaddr;
    }
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
        if (instr_get_opcode(&instr) == OP_jmp_short) {
#    ifdef AARCH64
            /* For A64, both skip_pc and cti_pc are an OP_jmp_short instr. */
            skip_pc = cti_pc;
            cti_pc = prev_pc;
#    else
            skip_pc = prev_pc;
#    endif
        } else if (instr_get_opcode(&instr) == OP_jmp) {
#    ifdef ARM
            /* For A32, both skip_pc and cti_pc are an OP_jmp instr. */
            skip_pc = cti_pc;
#    endif
            cti_pc = prev_pc;
        }
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
        nxt_pc = instr_encode_to_copy(dcontext, &instr, vmcode_get_writable_addr(skip_pc),
                                      skip_pc);
        ASSERT(nxt_pc != NULL);
        nxt_pc = vmcode_get_executable_addr(nxt_pc);
        ASSERT(nxt_pc == cti_pc);
        machine_cache_sync(skip_pc, nxt_pc, true);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3, "\ttarget of syscall jmp is already " PFX "\n",
            target);
    }
    instr_free(dcontext, &instr);
    return true;
}
#endif /* UNIX */

#ifdef LINUX
/***************************************************************************
 * Rseq (restartable sequence) mangling.
 */
enum {
    DR_RSEQ_LABEL_START = 0,
    DR_RSEQ_LABEL_END = 1,
    DR_RSEQ_LABEL_ABORT = 2,
    DR_RSEQ_LABEL_CS = 3,
};

static inline void
save_tls_or_dc(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
               uint tls_offs, uint dc_offs)
{
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, where, instr_create_save_to_tls(dcontext, reg, tls_offs));
    } else {
        PRE(ilist, where, instr_create_save_to_dcontext(dcontext, reg, dc_offs));
    }
}

static inline void
restore_tls_or_dc(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
                  uint tls_offs, uint dc_offs)
{
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, where, instr_create_restore_from_tls(dcontext, reg, tls_offs));
    } else {
        PRE(ilist, where, instr_create_restore_from_dcontext(dcontext, reg, dc_offs));
    }
}

static instr_t *
mangle_rseq_create_label(dcontext_t *dcontext, int type, ptr_uint_t data)
{
    instr_t *label = INSTR_CREATE_label(dcontext);
    instr_set_note(label, (void *)DR_NOTE_RSEQ);
    dr_instr_label_data_t *label_data = instr_get_label_data_area(label);
    label_data->data[0] = type;
    label_data->data[1] = data;
    return label;
}

/* scratch_reg is *not* spilled on entry. */
static void
mangle_rseq_write_exit_reason(dcontext_t *dcontext, instrlist_t *ilist,
                              instr_t *insert_at, reg_id_t scratch_reg)
{
    /* We use slot 1 to avoid conflict with segment mangling. */
    save_tls_or_dc(dcontext, ilist, insert_at, scratch_reg, TLS_REG1_SLOT, REG1_OFFSET);
    if (SCRATCH_ALWAYS_TLS()) {
        insert_get_mcontext_base(dcontext, ilist, insert_at, scratch_reg);
    } else {
        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)dcontext,
                               opnd_create_reg(scratch_reg), ilist, insert_at, NULL,
                               NULL);
    }
#    ifdef AARCHXX
    /* We need a 2nd scratch for our immediate. */
    ASSERT(SCRATCH_ALWAYS_TLS());
    reg_id_t scratch2 =
        (scratch_reg == DR_REG_START_GPR) ? DR_REG_START_GPR + 1 : DR_REG_START_GPR;
    PRE(ilist, insert_at, instr_create_save_to_tls(dcontext, scratch2, TLS_REG2_SLOT));
    insert_mov_immed_ptrsz(dcontext, EXIT_REASON_RSEQ_ABORT, opnd_create_reg(scratch2),
                           ilist, insert_at, NULL, NULL);
#    endif
    /* FIXME i#3544: Not implemented */
    PRE(ilist, insert_at,
        XINST_CREATE_store_2bytes(
            dcontext,
            opnd_create_dcontext_field_via_reg_sz(dcontext, scratch_reg,
                                                  EXIT_REASON_OFFSET, OPSZ_2),
            IF_AARCHXX_ELSE(opnd_create_reg(scratch2),
                            OPND_CREATE_INT16(EXIT_REASON_RSEQ_ABORT))));
#    ifdef AARCHXX
    PRE(ilist, insert_at,
        instr_create_restore_from_tls(dcontext, scratch2, TLS_REG2_SLOT));
#    endif
    restore_tls_or_dc(dcontext, ilist, insert_at, scratch_reg, TLS_REG1_SLOT,
                      REG1_OFFSET);
}

/* May modify next_instr. */
static void
mangle_rseq_insert_native_sequence(dcontext_t *dcontext, instrlist_t *ilist,
                                   instr_t *instr, INOUT instr_t **next_instr,
                                   uint *flags INOUT, app_pc start, app_pc end,
                                   app_pc handler, reg_id_t scratch_reg,
                                   bool *reg_written, int reg_written_count)
{
    /* We just ran the instrumented version of the rseq code, with the stores
     * removed.  Now we need to invoke it again natively for real.  We would prefer
     * to invoke the abort handler, as it may perform some setup, but in too many
     * cases it is truly an "abort" handler that just exits rather than a "restart
     * handler".  Furthermore, to support executing a copy of the code natively in
     * order to provide guarantees on regaining control and not rely on call-return
     * semantics, it is simpler to execute only the limited-scope rseq region.
     * Thus, we target the start point.
     *
     * In case the abort handler does perform setup, we checkpoint and restore GPR
     * register values.  Memory should remain as it was, due to nop-ing of stores.
     *
     * XXX i#2350: We ignore the app's rseq flags for when to restart.  It's
     * possible the app disabled restarts on preempts and migrations and can't
     * handle our restart here, but that seems pathological: we expect the rseq
     * feature to be used for restarts rather than just a detection mechanism of
     * preemption.
     */
    /* TODO i#2350: We may want to watch exits from the sequence in the instrumented
     * run and do this native invocation on those as well and not just on completion
     * of the rseq sequence, in case a store we nop-ed is read post-exit.  We would
     * flag all rseq exit ctis and process in dispatch or sthg.
     */
    LOG(THREAD, LOG_INTERP, 3, "mangle: inserting native rseq code " PFX "\n", start);
    RSTATS_INC(num_rseq_native_calls_inserted);
    instr_t *insert_at = *next_instr;

    /* We've already inserted a DR_NOTE_REG_BARRIER label to ensure that clients will
     * restore app state before this native invocation.
     */

    /* Create a scratch register. Use slot 1 to avoid conflict with segment
     * mangling below.
     */
    save_tls_or_dc(dcontext, ilist, insert_at, scratch_reg, TLS_REG1_SLOT, REG1_OFFSET);
    /* Restore the entry state we preserved earlier. */
    if (reg_written_count > 0) {
        if (SCRATCH_ALWAYS_TLS())
            insert_get_mcontext_base(dcontext, ilist, insert_at, scratch_reg);
        else {
            insert_mov_immed_ptrsz(dcontext, (ptr_int_t)dcontext,
                                   opnd_create_reg(scratch_reg), ilist, insert_at, NULL,
                                   NULL);
        }
        int i;
        for (i = 0; i < DR_NUM_GPR_REGS; i++) {
            if (reg_written[i]) {
                /* XXX: Keep this consistent with instr_is_rseq_load() in translate.c. */
                size_t offs = offsetof(dcontext_t, rseq_entry_state) + sizeof(reg_t) * i;
#    ifdef AARCH64
                if (DR_REG_START_GPR + (reg_id_t)i == DR_REG_SP) {
                    /* Unfortunately SP cannot be directly loaded into. */
                    reg_id_t scratch2 = scratch_reg == DR_REG_X0 ? DR_REG_X1 : DR_REG_X0;
                    save_tls_or_dc(dcontext, ilist, insert_at, scratch2, TLS_REG1_SLOT,
                                   REG1_OFFSET);
                    PRE(ilist, insert_at,
                        XINST_CREATE_load(dcontext, opnd_create_reg(scratch2),
                                          OPND_CREATE_MEMPTR(scratch_reg, offs)));
                    PRE(ilist, insert_at,
                        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_SP),
                                          opnd_create_reg(scratch2)));
                    restore_tls_or_dc(dcontext, ilist, insert_at, scratch2, TLS_REG1_SLOT,
                                      REG1_OFFSET);
                    continue;
                }
#    endif
                PRE(ilist, insert_at,
                    XINST_CREATE_load(dcontext,
                                      opnd_create_reg(DR_REG_START_GPR + (reg_id_t)i),
                                      OPND_CREATE_MEMPTR(scratch_reg, offs)));
            }
        }
    }

    instr_t *label_start = mangle_rseq_create_label(dcontext, DR_RSEQ_LABEL_START, 0);
    instr_t *label_end = mangle_rseq_create_label(dcontext, DR_RSEQ_LABEL_END, 0);
    instr_t *label_abort = mangle_rseq_create_label(dcontext, DR_RSEQ_LABEL_ABORT, 0);

    /* Create an exit cti targeting the abort handler, to use as our handler.
     * XXX: I'd like to use DO_NOT_EMIT but that's not supported for regular
     * blocks and so we pay the cost of a little jump.
     * The jump helps with the signature data we embed in any case.
     */
    instr_t *skip_abort = INSTR_CREATE_label(dcontext);
    PRE(ilist, insert_at,
        XINST_CREATE_jump_short(dcontext, opnd_create_instr(skip_abort)));
    /* We have to put the signature just prior to the abort handler pc. */
    int signature = rseq_get_signature();
#    ifdef X86
    /* To avoid errors decoding in decode_fragment() or disassembly, we spend 3
     * extra bytes making it into a 7-byte nop instr on variable-length x86.
     */
    instr_t *abort_sig = INSTR_CREATE_nop_modrm(
        dcontext, opnd_create_base_disp(DR_REG_NULL, DR_REG_NULL, 0, signature, OPSZ_4));
#    else
    /* XXX i#2350: This may still have trouble with decode_fragment() if it
     * happens to look like a branch or invalid opcode.
     */
    instr_t *abort_sig = XINST_CREATE_nop(dcontext);
    instr_allocate_raw_bits(dcontext, abort_sig, sizeof(signature));
    instr_set_raw_word(abort_sig, 0, (uint)signature);
#    endif
    PRE(ilist, insert_at, abort_sig);
    PRE(ilist, insert_at, label_abort);
    /* To raise a kernel xfer event we need to go back to DR.  Thus this exit will
     * never be linked.  This should be quite rare, however, and should not impose
     * a performance burden.
     */
    mangle_rseq_write_exit_reason(dcontext, ilist, insert_at, scratch_reg);
    instr_t *abort_exit = XINST_CREATE_jump(dcontext, opnd_create_pc(handler));
    instr_branch_set_special_exit(abort_exit, true);
    instrlist_preinsert(ilist, insert_at, abort_exit);
    PRE(ilist, insert_at, skip_abort);

    /* Point this thread's struct rseq ptr at an rseq_cs which points at the bounds
     * of this fragment's region.  We want to create a new rseq_cs, point at it
     * here, and finalize it in mangle_rseq_finalize(), but allocating memory here
     * leads to complexities freeing it and determinism complexities in various
     * non-emitted-block scenarios, primarily translation.  Thus we instead make a
     * placeholder and patch it in mangle_rseq_finalize().  To ensure the immed will
     * reach we always ask for a large immediate (paying the price of an extra zero
     * write for the top half for aarchxx).
     *
     * An alternative is to embed the rseq_cs as data inside the fragment, next to
     * the signature above.  To ensure alignment, that would add 64 bytes to the
     * cache and require extra intructions to do the alignment (if we got the
     * address from an instr opnd), but the real problem is it messes up
     * decode_fragment() and even disassembly.
     */
    instr_t *immed_first, *immed_last;
    insert_mov_immed_ptrsz(dcontext, (ptr_int_t)-1, opnd_create_reg(scratch_reg), ilist,
                           insert_at, &immed_first, &immed_last);
    ASSERT(immed_first != NULL);
    IF_X86(ASSERT(immed_last == NULL));
    int immed_count = 1;
    for (instr_t *immed_inst = immed_first;
         immed_last != NULL && immed_inst != immed_last;
         immed_inst = instr_get_next(immed_inst)) {
        ++immed_count;
    }
    instr_t *label_rseq_cs =
        mangle_rseq_create_label(dcontext, DR_RSEQ_LABEL_CS, immed_count);
    PRE(ilist, immed_first /*prior to immeds*/, label_rseq_cs);
#    ifdef X86
    /* We need to mangle this segment ref, and all of the subsequent local copy. */
    instr_t *start_mangling = XINST_CREATE_store(
        dcontext,
        opnd_create_far_base_disp(LIB_SEG_TLS, DR_REG_NULL, DR_REG_NULL, 0,
                                  rseq_get_tls_ptr_offset(), OPSZ_PTR),
        opnd_create_reg(scratch_reg));
    instrlist_preinsert(ilist, insert_at, start_mangling);
#    elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    instr_t *start_mangling = NULL;
#    else
    /* We need another scratch reg to write to TLS. */
    ASSERT(SCRATCH_ALWAYS_TLS());
    reg_id_t scratch2 =
        (scratch_reg == DR_REG_START_GPR) ? DR_REG_START_GPR + 1 : DR_REG_START_GPR;
    PRE(ilist, insert_at, instr_create_save_to_tls(dcontext, scratch2, TLS_REG2_SLOT));
    /* We need to mangle this segment ref, and the local copy below. */
    instr_t *start_mangling = INSTR_CREATE_mrs(dcontext, opnd_create_reg(scratch2),
                                               opnd_create_reg(LIB_SEG_TLS));
    instrlist_preinsert(ilist, insert_at, start_mangling);
    PRE(ilist, insert_at,
        XINST_CREATE_store(dcontext,
                           opnd_create_base_disp(scratch2, DR_REG_NULL, 0,
                                                 rseq_get_tls_ptr_offset(), OPSZ_PTR),
                           opnd_create_reg(scratch_reg)));
    PRE(ilist, insert_at,
        instr_create_restore_from_tls(dcontext, scratch2, TLS_REG2_SLOT));
#    endif

    /* Restore scratch_reg. */
    restore_tls_or_dc(dcontext, ilist, insert_at, scratch_reg, TLS_REG1_SLOT,
                      REG1_OFFSET);

    /* Make a local copy of the rseq code (otherwise we would have to assume that
     * all rseq sequences are callees with a nice return to come back to us, which
     * is not true).  All exits become fragment exits; we assume all ind branches
     * are exits.  We mangle the code here (but do *not* pass to clients), which is
     * required (rip-rel reachable, segments, stolen reg!) and safe (mostly loads,
     * not stores, and restartable).
     */
    app_pc pc = start;
    /* Store the PC values for faster conversion of intra-region targets. */
    generic_table_t *pc2instr =
        generic_hash_create(dcontext, 6 /*expect few entries*/, 80 /* load factor */, 0,
                            NULL _IF_DEBUG("pc2instr"));
    PRE(ilist, insert_at, label_start);
    while (pc < end) {
        instr_t *copy = instr_create(dcontext);
        pc = decode(dcontext, pc, copy);
        if (pc == NULL) {
            REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3,
                                        get_application_name(), get_application_pid(),
                                        "Invalid instruction inside rseq region");
            ASSERT_NOT_REACHED();
        }
        generic_hash_add(dcontext, pc2instr, (ptr_uint_t)get_app_instr_xl8(copy), copy);
        /* Make intra-region branches meta; all others are exit ctis. */
        if ((instr_is_cbr(copy) || instr_is_ubr(copy)) &&
            opnd_is_pc(instr_get_target(copy))) {
            app_pc tgt = opnd_get_pc(instr_get_target(copy));
            if (tgt >= start && tgt < end) {
                /* We change the target from a PC to an instr_t* at the end. */
                PRE(ilist, insert_at, copy);
                continue;
            }
        }
        instrlist_preinsert(ilist, insert_at, copy);
        if (instr_is_mbr(copy)) {
            /* We need to add the exit cti that interp adds for a regular block.
             * We explicitly disallow targeting inside the sequence, but we have
             * no way to easily verify that: if it happens we'll end up interpreting
             * the target part of the sequence, leading to app errors.
             */
            uint exit_type = instr_branch_type(copy);
            byte *pc = get_ibl_routine(dcontext, get_ibl_entry_type(exit_type),
                                       TEST(FRAG_IS_TRACE, *flags) ? DEFAULT_IBL_TRACE()
                                                                   : DEFAULT_IBL_BB(),
                                       get_ibl_branch_type(copy));
            instr_t *exit = XINST_CREATE_jump(dcontext, opnd_create_pc(pc));
            instr_exit_branch_set_type(exit, exit_type);
            instrlist_preinsert(ilist, insert_at, exit);
        }
#    if defined(DEBUG) && defined(X86)
        /* Support for the api.rseq test with (officially unsupported) syscall in
         * its rseq code executing before the app executes a syscall.
         */
        if (instr_is_syscall(copy) &&
            get_syscall_method() == SYSCALL_METHOD_UNINITIALIZED) {
            ASSERT(instr_get_opcode(copy) == OP_syscall &&
                   check_filter("api.rseq", get_short_name(get_application_name())));
            set_syscall_method(SYSCALL_METHOD_SYSCALL);
            update_syscalls(dcontext);
        }
#    endif
    }
    PRE(ilist, insert_at, label_end);
    /* Update all intra-region targets to use instr_t* operands.  We can't simply
     * leave absolute PC's and re-relativize (that would point into the app code).
     * Nor can we use the hardcoded relative offset by calling
     * instr_set_rip_rel_valid(, false) because there can be subsequent mangling that
     * changes the offsets.
     */
    for (instr_t *walk = label_start; walk != label_end; walk = instr_get_next(walk)) {
        if (!instr_is_app(walk) && (instr_is_cbr(walk) || instr_is_ubr(walk))) {
            ASSERT(opnd_is_pc(instr_get_target(walk)));
            app_pc tgt_pc = opnd_get_pc(instr_get_target(walk));
            instr_t *tgt_inst =
                (instr_t *)generic_hash_lookup(dcontext, pc2instr, (ptr_uint_t)tgt_pc);
            if (tgt_inst == NULL) {
                LOG(THREAD, LOG_INTERP, 1,
                    "%s: pc2instr failed for branch from " PFX " to " PFX "\n",
                    __FUNCTION__, get_app_instr_xl8(walk), tgt_pc);
                REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3,
                                            get_application_name(), get_application_pid(),
                                            "Rseq branch target is mid-instruction");
                ASSERT_NOT_REACHED();
            }
            instr_set_target(walk, opnd_create_instr(tgt_inst));
        }
    }
    generic_hash_destroy(dcontext, pc2instr);
    /* Now mangle from this point. */
    ASSERT(start_mangling != NULL);
    *next_instr = start_mangling;

    /* Clear the rseq ptr on exit to avoid problems if we free the rseq_cs and
     * the kernel finds invalid addresses there and forces a SIGSEGV on us.
     * For midpoint exits above, it's not easy to insert this clear directly.
     * Instead, we rely on rseq_shared_fragment_flushtime_update() and
     * rseq_remove_fragment() to clear the pointer before rseq_cs is freed.
     */
#    ifdef X86
    instrlist_preinsert(ilist, insert_at,
                        XINST_CREATE_store(dcontext,
                                           opnd_create_far_base_disp(
                                               LIB_SEG_TLS, DR_REG_NULL, DR_REG_NULL, 0,
                                               rseq_get_tls_ptr_offset(), OPSZ_PTR),
                                           OPND_CREATE_INT32(0)));
#    elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
#    else
    PRE(ilist, insert_at, instr_create_save_to_tls(dcontext, scratch2, TLS_REG2_SLOT));
    instrlist_preinsert(ilist, insert_at,
                        INSTR_CREATE_mrs(dcontext, opnd_create_reg(scratch2),
                                         opnd_create_reg(LIB_SEG_TLS)));
#        ifdef ARM /* No zero register. */
    PRE(ilist, insert_at, instr_create_save_to_tls(dcontext, scratch_reg, TLS_REG1_SLOT));
    PRE(ilist, insert_at,
        XINST_CREATE_load_int(dcontext, opnd_create_reg(scratch_reg),
                              OPND_CREATE_INT(0)));
#        endif
    instrlist_preinsert(
        ilist, insert_at,
        XINST_CREATE_store(dcontext,
                           opnd_create_base_disp(scratch2, DR_REG_NULL, 0,
                                                 rseq_get_tls_ptr_offset(), OPSZ_PTR),
                           opnd_create_reg(IF_AARCH64_ELSE(DR_REG_XZR, scratch_reg))));
#        ifdef ARM /* No zero register. */
    PRE(ilist, insert_at,
        instr_create_restore_from_tls(dcontext, scratch_reg, TLS_REG1_SLOT));
#        endif
    PRE(ilist, insert_at,
        instr_create_restore_from_tls(dcontext, scratch2, TLS_REG2_SLOT));
#    endif

    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "New ilist for rseq:\n");
        instrlist_disassemble(dcontext, start, ilist, THREAD);
    });
}

/* The caller should only call this for instr_writes_memory(instr).
 * Returns whether it destroyed "instr".
 */
static bool
mangle_rseq_nop_store(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    ASSERT(instr_writes_memory(instr));
    RSTATS_INC(num_rseq_stores_elided);
    /* XXX i#2350: We want to turn just the store portion of the instr into a nop
     * and keep any register side effects.  That is complex, however.  For now we
     * only support simple stores and aarchxx writebacks.
     */
#    ifdef AARCHXX
    /* Handle writeback via pre-index or post-index addressing. */
    opnd_t memop = instr_get_dst(instr, 0);
    if (opnd_is_base_disp(memop) && instr_num_dsts(instr) == 2 &&
        instr_num_srcs(instr) == 3 && opnd_is_reg(instr_get_src(instr, 0)) &&
        opnd_is_reg(instr_get_src(instr, 1)) &&
        opnd_is_immed_int(instr_get_src(instr, 2)) &&
        opnd_is_reg(instr_get_dst(instr, 1)) && opnd_is_base_disp(memop) &&
        opnd_get_index(memop) == DR_REG_NULL && opnd_get_scale(memop) == DR_REG_NULL) {
        /* We need to mangle this instruction in case it uses the stolen register.
         * We can't adjust next_instr backward as that will re-trigger rseq mangling:
         * we want to hit the stolen reg mangling checked after rseq.
         * Thus we re-use "instr".
         */
        int increment = (int)opnd_get_immed_int(instr_get_src(instr, 2));
        instr_t *add = INSTR_XL8(XINST_CREATE_add(dcontext, instr_get_dst(instr, 1),
                                                  OPND_CREATE_INT(increment)),
                                 get_app_instr_xl8(instr));
        LOG(THREAD, LOG_INTERP, 3,
            "mangle: turning writeback store inside rseq region to add @" PFX "\n",
            get_app_instr_xl8(instr));
        /* XXX: This is kind of hacky.  Should we provide a variant of instr_clone()?
         * Or should we directly call mangle_special_registers() here to avoid all
         * this?
         */
        instr_free(dcontext, instr);
        add->next = instr->next;
        add->prev = instr->prev;
        memcpy(instr, add, sizeof(*instr));
        instr_init(dcontext, add);
        instr_destroy(dcontext, add);
        return false;
    }
#    endif
    if (instr_num_dsts(instr) > 1) {
        REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                                    get_application_pid(),
                                    "Store inside rseq region has multiple destinations");
        ASSERT_NOT_REACHED();
    }
    LOG(THREAD, LOG_INTERP, 3, "mangle: removing store inside rseq region @" PFX "\n",
        get_app_instr_xl8(instr));
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return true; /* destroyed instr */
}

/* Returns whether it destroyed "instr".  May modify next_instr. */
static bool
mangle_rseq(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
            INOUT instr_t **next_instr, uint *flags INOUT)
{
    int i;
    app_pc pc = get_app_instr_xl8(instr);
    app_pc start, end, handler;
    bool *reg_written;
    int reg_written_size;
    reg_id_t scratch_reg = DR_REG_START_GPR;
#    ifdef ARM
    ASSERT_NOT_TESTED();
#    endif
    if (!rseq_get_region_info(pc, &start, &end, &handler, &reg_written,
                              &reg_written_size)) {
        ASSERT_NOT_REACHED(); /* Caller was supposed to check for overlap */
        return false;
    }
    /* We need to know the type of register so we can't completely abstract this. */
    ASSERT(reg_written_size == DR_NUM_GPR_REGS);
    int reg_written_count = 0;
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        if (reg_written[i]) {
            /* For simplicity we avoid our scratch being a register we're preserving. */
            if (DR_REG_START_GPR + (reg_id_t)i == scratch_reg)
                scratch_reg++;
            reg_written_count++;
        }
    }
    if (scratch_reg == DR_NUM_GPR_REGS) {
        /* We could handle this by an xchg or sthg but it seems so rare, and given
         * that we already have so many rseq limitations, I'm bailing on it.
         */
        REPORT_FATAL_ERROR_AND_EXIT(
            RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(), get_application_pid(),
            "Rseq sequence writes every general-purpose register");
        ASSERT_NOT_REACHED();
    }
    if (pc == start && reg_written_count > 0) {
        /* Preserve any input register state that will be re-set-up by the abort
         * handler on a restart.  We directly invoke start on a restart to make it
         * easier to use a copy of the code, and to support non-restarting handlers.
         */
        /* XXX i#3798: Be sure to insert these register saves prior to any client
         * instrumentation, which may move app register values elsewhere.  We've
         * arranged the rseq start to always be a block start, and with current drreg
         * implementation, all values are native (i.e., in registers) at block start, so
         * we're ok for now, but we may want some kind of barrier API in the future.
         */
        instr_t *first = instrlist_first(ilist);
        save_tls_or_dc(dcontext, ilist, first, scratch_reg, TLS_REG0_SLOT, REG0_OFFSET);
        if (SCRATCH_ALWAYS_TLS()) {
            insert_get_mcontext_base(dcontext, ilist, first, scratch_reg);
        } else {
            insert_mov_immed_ptrsz(dcontext, (ptr_int_t)dcontext,
                                   opnd_create_reg(scratch_reg), ilist, first, NULL,
                                   NULL);
        }
        for (i = 0; i < DR_NUM_GPR_REGS; i++) {
            if (reg_written[i]) {
                size_t offs = offsetof(dcontext_t, rseq_entry_state) + sizeof(reg_t) * i;
#    ifdef AARCH64
                if (DR_REG_START_GPR + (reg_id_t)i == DR_REG_SP) {
                    /* Unfortunately SP cannot be directly stored. */
                    reg_id_t scratch2 = scratch_reg == DR_REG_X0 ? DR_REG_X1 : DR_REG_X0;
                    save_tls_or_dc(dcontext, ilist, first, scratch2, TLS_REG1_SLOT,
                                   REG1_OFFSET);
                    PRE(ilist, first,
                        XINST_CREATE_move(dcontext, opnd_create_reg(scratch2),
                                          opnd_create_reg(DR_REG_SP)));
                    PRE(ilist, first,
                        XINST_CREATE_store(dcontext,
                                           OPND_CREATE_MEMPTR(scratch_reg, offs),
                                           opnd_create_reg(scratch2)));
                    restore_tls_or_dc(dcontext, ilist, first, scratch2, TLS_REG1_SLOT,
                                      REG1_OFFSET);
                    continue;
                }
#    endif
                PRE(ilist, first,
                    XINST_CREATE_store(dcontext, OPND_CREATE_MEMPTR(scratch_reg, offs),
                                       opnd_create_reg(DR_REG_START_GPR + (reg_id_t)i)));
            }
        }
        restore_tls_or_dc(dcontext, ilist, first, scratch_reg, TLS_REG0_SLOT,
                          REG0_OFFSET);
    }
    int len = instr_length(dcontext, instr);
    if (pc + len >= end) {
        ilist->flags |= INSTR_RSEQ_ENDPOINT;
        /* We should already have this flag set by the bb builder. */
        ASSERT(TEST(FRAG_HAS_RSEQ_ENDPOINT, *flags));
        if (pc + len != end) {
            REPORT_FATAL_ERROR_AND_EXIT(
                RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                get_application_pid(),
                "Malformed rseq endpoint: not on instruction boundary");
            ASSERT_NOT_REACHED();
        }
        if (instr_is_cti(instr)) {
            REPORT_FATAL_ERROR_AND_EXIT(
                RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                get_application_pid(),
                "Rseq sequences must fall through their endpoints");
            ASSERT_NOT_REACHED();
        }
        rseq_set_final_instr_pc(start, pc);
        /* We need to insert the native sequence before the barrier label,
         * as that is where the app code has native values.
         * It is possible a client inserted code in between so we have to go
         * and find it.
         */
        instr_t *find = *next_instr;
        while (find != NULL &&
               !(instr_is_label(find) &&
                 instr_get_note(find) == (void *)DR_NOTE_REG_BARRIER))
            find = instr_get_next(find);
        if (find == NULL) {
            REPORT_FATAL_ERROR_AND_EXIT(
                RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                get_application_pid(),
                "Rseq sequence DR_NOTE_REG_BARRIER must not be deleted");
            ASSERT_NOT_REACHED();
        }
        instr_t *where_next = instr_get_next(find);
        /* We actually don't need to set next_instr because the inserted native code
         * is still beyond the next_instr of this app instr.
         */
        mangle_rseq_insert_native_sequence(dcontext, ilist, find, &where_next, flags,
                                           start, end, handler, scratch_reg, reg_written,
                                           reg_written_count);
        /* TODO i#2350: We should also invoke the native sequence on a midpoint exit
         * from the sequence during instrumentation, since there may be state changes
         * in the early part that are visible outside.
         */
    }

    /* If we're inside a restartable sequence, this is the first run which is
     * instrumented and will be aborted/restarted.  We need to avoid *all* stores,
     * not just the final commit point, because the sequence could be using the wrong cpu
     * and could be editing a per-cpu data structure that another thread is touching
     * at the same time.
     */
    if (!instr_writes_memory(instr))
        return false;
    /* We perform this mangling of earlier instructions in the region out of logical
     * order (*after* the mangling above of the end of the region) to avoid issues
     * with accessing "instr" after we delete it.
     */
    return mangle_rseq_nop_store(dcontext, ilist, instr);
}

static void
mangle_rseq_finalize(dcontext_t *dcontext, instrlist_t *ilist, fragment_t *f)
{
    instr_t *instr, *immed_first = NULL, *immed_last = NULL;
    cache_pc pc = FCACHE_ENTRY_PC(f), immed_start_pc = NULL;
    cache_pc rseq_start = NULL, rseq_end = NULL, rseq_abort = NULL;
    DEBUG_DECLARE(int label_sets_found = 0;)
    for (instr = instrlist_first(ilist); instr != NULL; instr = instr_get_next(instr)) {
        if (instr_is_label(instr) && instr_get_note(instr) == (void *)DR_NOTE_RSEQ) {
            dr_instr_label_data_t *label_data = instr_get_label_data_area(instr);
            switch (label_data->data[0]) {
            case DR_RSEQ_LABEL_ABORT: rseq_abort = pc; break;
            case DR_RSEQ_LABEL_CS:
                immed_start_pc = pc;
                immed_first = instr_get_next(instr);
                ptr_int_t immed_count = label_data->data[1];
                /* For A64 we should have 4 immeds to handle any address. */
                IF_AARCH64(ASSERT(immed_count == 4));
                if (immed_count > 1) {
                    immed_last = instr_get_next(immed_first);
                    --immed_count;
                    while (immed_count > 1) {
                        immed_last = instr_get_next(immed_last);
                        --immed_count;
                    }
                }
                break;
            case DR_RSEQ_LABEL_START: rseq_start = pc; break;
            case DR_RSEQ_LABEL_END: {
                rseq_end = pc;
                /* We assume this is the 4th and last label.  We handle it here,
                 * so we can start over on a new set if there are multiple rseq
                 * regions (such as from duplicated app copies by drbbdup).
                 */
                IF_DEBUG(++label_sets_found;)
                ASSERT(rseq_start != NULL && rseq_abort != NULL);
                byte *rseq_cs_alloc, *rseq_cs;
                /* The rseq_cs creation and recording is structured like this in two steps
                 * to provide flexibility in mangling.  Originally the alloc was done in
                 * mangle_rseq() and passed here in the label data, but to simplify
                 * freeing we now allocate here and patch the immediates.
                 */
                rseq_cs_alloc = rseq_get_rseq_cs_alloc(&rseq_cs);
                rseq_record_rseq_cs(rseq_cs_alloc, f, rseq_start, rseq_end, rseq_abort);
                ASSERT(immed_start_pc != NULL && immed_first != NULL);
                LOG(THREAD, LOG_INTERP, 4, "%s: start=%p, end=%p, abort=%p stored @%p\n",
                    __FUNCTION__, rseq_start, rseq_end, rseq_abort, rseq_cs);
                patch_mov_immed_ptrsz(dcontext, (ptr_int_t)rseq_cs, immed_start_pc,
                                      immed_first, immed_last);
                DODEBUG({
                    rseq_abort = NULL;
                    rseq_start = NULL;
                    immed_start_pc = NULL;
                    immed_first = NULL;
                });
                break;
            }
            default: ASSERT_NOT_REACHED();
            }
        }
        pc += instr_length(dcontext, instr);
    }
    /* We should have found at least one set of labels. */
    ASSERT(label_sets_found > 0);
}
#endif /* LINUX */

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

#ifdef LINUX
        /* Mangle stores inside restartable sequences ("rseq").  We could avoid the
         * per-instr check if we disallowed rseq blocks in traces and prevented
         * fall-through in a bb, but that would lead to more problems than it would
         * solve.  We expect the vmvector_empty check to be fast enough for the common
         * case.
         */
        if (instr_is_app(instr) &&
            !instr_is_our_mangling(instr) /* avoid synthetic exit jump*/ &&
            !vmvector_empty(d_r_rseq_areas)) {
            app_pc pc = get_app_instr_xl8(instr);
            if (vmvector_overlap(d_r_rseq_areas, pc, pc + 1)) {
                if (mangle_rseq(dcontext, ilist, instr, &next_instr, flags))
                    continue; /* instr was destroyed */
            }
        }
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
        /* XXX i#1834: We do not limit mangling of pc-relative operands to app instrs,
         * because we were mangling tool operands before and now our own samples rely
         * on it (when they copy an app operand to pass to a clean call, e.g.).
         *
         * TODO i#1834: Add full support for this to DR's translation code.
         *
         * TODO i#1834: Also mangle stolen register and segment usage in tool code.
         * That may require a new opnd_t bit identifying "app operands".
         */
        if (instr_has_rel_addr_reference(instr)) {
            instr_t *res = mangle_rel_addr(dcontext, ilist, instr, next_instr);
            /* i#393: mangle_rel_addr might destroy the instr if it is a LEA,
             * which makes instr point to freed memory.
             * In such case, the control should skip later checks on the instr
             * for exit_cti and syscall.
             * It either returns NULL == destroyed "instr", or a new next_instr.
             */
            if (res == NULL)
                continue;
            else
                next_instr = res;
        }
#endif /* X64 || ARM */

#ifdef AARCHXX
        if (!instr_is_meta(instr) && instr_reads_thread_register(instr) &&
            INTERNAL_OPTION(private_loader)) {
            next_instr = mangle_reads_thread_register(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif /* ARM || AARCH64 */

#ifdef AARCH64
        if (!instr_is_meta(instr) && instr_writes_thread_register(instr) &&
            INTERNAL_OPTION(private_loader)) {
            next_instr =
                mangle_writes_thread_register(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif
#ifdef AARCHXX
        if (instr_is_app(instr) &&
            (instr_is_exclusive_load(instr) || instr_is_exclusive_store(instr) ||
             instr_get_opcode(instr) == OP_clrex)) {
            instr_t *res =
                mangle_exclusive_monitor_op(dcontext, ilist, instr, next_instr);
            if (res != NULL) {
                next_instr = res;
                continue;
            } /* Else, fall through. */
        }
#endif
#ifdef AARCH64
        if (!instr_is_meta(instr) && instr_uses_reg(instr, dr_reg_stolen))
            next_instr = mangle_special_registers(dcontext, ilist, instr, next_instr);
#endif
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
            if (TEST(INSTR_CLOBBER_RETADDR, instr->flags) && instr_is_label(instr)) {
                /* Move the value to the offset field (which the client cannot
                 * possibly use at this point) so we don't have to search for
                 * this label when we hit the ret instr.
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
                        tmp->offset = data->data[1]; /* the value to use */
                        break;
                    }
                }
            }
            continue;
        }

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

    KSTOP(mangling);
}

/***************************************************************************
 * SYSCALL
 */

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
    instr_t *walk, *tgt;

    if (prev == NULL) {
#if defined(WINDOWS) && defined(X64)
        if (get_os_version() >= WINDOWS_VERSION_10_1511) {
            /* Handle the branch added in 1511 that isolates OP_syscall:
             *   7ff9`13185630 4c8bd1          mov     r10,rcx
             *   7ff9`13185633 b843000000      mov     eax,43h
             *   7ff9`13185638 f604250803fe7f01 test byte ptr [SharedUserData+0x308],1
             *   7ff9`13185640 7503            jne     00007ff9`13185645
             *   7ff9`13185642 0f05            syscall
             */
#    define MOV_IMMED_OFFS_FROM_SYS -15
#    define RAW_SYS_TEST1 0xf6
#    define RAW_SYS_TEST2 0x04
#    define RAW_SYS_TEST3 0x25
#    define RAW_SYS_TEST_FINAL 0x01
            app_pc syscall_pc = get_app_instr_xl8(instr);
            byte buf[-MOV_IMMED_OFFS_FROM_SYS];
            if (d_r_safe_read(syscall_pc + MOV_IMMED_OFFS_FROM_SYS, sizeof(buf), buf) &&
                buf[0] == MOV_IMM2XAX_OPCODE && buf[5] == RAW_SYS_TEST1 &&
                buf[6] == RAW_SYS_TEST2 && buf[7] == RAW_SYS_TEST3 &&
                buf[12] == RAW_SYS_TEST_FINAL && buf[13] == RAW_OPCODE_jne_short) {
                syscall = *(int *)&buf[1];
                return syscall;
            }
        }
#endif
        return -1;
    }
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
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3, "%s: never found write of syscall number\n",
            __FUNCTION__);
    }
    IF_X64(ASSERT_TRUNCATE(int, int, syscall));
    return (int)syscall;
}

void
mangle_finalize(dcontext_t *dcontext, instrlist_t *ilist, fragment_t *f)
{
#ifdef X86
    if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
        finalize_selfmod_sandbox(dcontext, f);
    }
#endif
#ifdef LINUX
    if (TEST(INSTR_RSEQ_ENDPOINT, ilist->flags))
        mangle_rseq_finalize(dcontext, ilist, f);
#endif
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
    clean_call_opt_init();
    clean_call_info_init(&default_clean_call_info, NULL, false, 0);
}

void
mangle_exit(void)
{
    clean_call_opt_exit();
}
