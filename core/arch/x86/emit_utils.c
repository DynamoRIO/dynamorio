/* **********************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
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

/* file "emit_utils.c"
 * The Pentium processors maintain cache consistency in hardware, so we don't
 * worry about getting stale cache entries.
 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "instrlist.h"
#include "instrument.h" /* for dr_insert_call() */

#define APP  instrlist_meta_append

/* helper functions for emit_fcache_enter_common  */

/*  # append instructions to call exit_dr_hook
 *  if (EXIT_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    else
 *      # support for skipping the hook
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmpl    %edi,0
 *      jnz     post_hook
 *    endif
 *    call  EXIT_DR_HOOK # for x64 windows, reserve 32 bytes stack space for call
 *    if (!absolute)
 *      pop    %xsi
 *      pop    %xdi
 *    endif
 *  endif
 * post_hook:
 */
void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                         bool absolute, bool shared)
{
    instr_t *post_hook = INSTR_CREATE_label(dcontext);
    if (EXIT_DR_HOOK != NULL) {
        /* if absolute, don't bother to save any regs around the call */
        if (!absolute) {
            /* save xdi and xsi around call.
             * for x64, they're supposed to be callee-saved on windows,
             * but not linux (though we could move to r12-r15 on linux
             * instead of pushing them).
             */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XDI)));
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSI)));
        }
#ifdef WINDOWS
        else {
            /* For thread-private (used for syscalls), don't call if
             * dcontext->ignore_enterexit.  This is a perf hit to check: could
             * instead have a space hit via a separate routine.  This is only
             * needed right now for NtSuspendThread handling (see case 4942).
             */
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_EDI, IGNORE_ENTEREXIT_OFFSET));
            /* P4 opt guide says to use test to cmp reg with 0: shorter instr */
            APP(ilist, INSTR_CREATE_test(dcontext, opnd_create_reg(REG_EDI),
                                         opnd_create_reg(REG_EDI)));
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(post_hook)));
        }
#endif
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args,
         * and we don't want anyone clobbering our pushed registers!
         */
        dr_insert_call((void *)dcontext, ilist, NULL/*append*/, (void *)EXIT_DR_HOOK, 0);
        if (!absolute) {
            /* save edi and esi around call */
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSI)));
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XDI)));
        }

    }
    APP(ilist, post_hook/*label*/);
}

/* append instructions to restore xflags
 *  # restore the original register state
 *  RESTORE_FROM_UPCONTEXT xflags_OFFSET,%xax
 *  push  %xax
 *  popf        # restore eflags temporarily using dstack
 */
void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, XFLAGS_OFFSET));
    APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(SCRATCH_REG0)));
    /* restore eflags temporarily using dstack */
    APP(ilist, INSTR_CREATE_RAW_popf(dcontext));
}

/* append instructions to restore extension registers like xmm
 *  if preserve_xmm_caller_saved
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+0*16,%xmm0
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+1*16,%xmm1
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+2*16,%xmm2
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+3*16,%xmm3
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+4*16,%xmm4
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+5*16,%xmm5
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+6*16,%xmm6  # 32-bit Linux
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+7*16,%xmm7  # 32-bit Linux
 *  endif
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel.
         * Rather than try and optimize we save/restore on every cxt
         * sw.  The xmm field is aligned, so we can use movdqa/movaps,
         * though movdqu is stated to be as fast as movdqa when aligned:
         * but if so, why have two versions?  Is it only loads and not stores
         * for which that is true?  => PR 266305.
         * It's not clear that movdqa is any faster (and its opcode is longer):
         * movdqa and movaps are listed as the same latency and throughput in
         * the AMD optimization guide.  Yet examples of fast memcpy online seem
         * to use movdqa when sse2 is available.
         * Note that mov[au]p[sd] and movdq[au] are functionally equivalent.
         */
        /* FIXME i#438: once have SandyBridge processor need to measure
         * cost of vmovdqu and whether worth arranging 32-byte alignment
         */
        int i;
        uint opcode = move_mm_reg_opcode(true/*align32*/, true/*align16*/);
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            APP(ilist, instr_create_1dst_1src
                (dcontext, opcode, opnd_create_reg
                 (REG_SAVED_XMM0 + (reg_id_t)i),
                 OPND_DC_FIELD(absolute, dcontext,
                               OPSZ_SAVED_XMM, XMM_OFFSET + i*XMM_SAVED_REG_SIZE)));
        }
    }
}

/* append instructions to restore general purpose registers
 *  ifdef X64
 *    RESTORE_FROM_UPCONTEXT r8_OFFSET,%r8
 *    RESTORE_FROM_UPCONTEXT r9_OFFSET,%r9
 *    RESTORE_FROM_UPCONTEXT r10_OFFSET,%r10
 *    RESTORE_FROM_UPCONTEXT r11_OFFSET,%r11
 *    RESTORE_FROM_UPCONTEXT r12_OFFSET,%r12
 *    RESTORE_FROM_UPCONTEXT r13_OFFSET,%r13
 *    RESTORE_FROM_UPCONTEXT r14_OFFSET,%r14
 *    RESTORE_FROM_UPCONTEXT r15_OFFSET,%r15
 *  endif
 *    RESTORE_FROM_UPCONTEXT xax_OFFSET,%xax
 *    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx
 *    RESTORE_FROM_UPCONTEXT xcx_OFFSET,%xcx
 *    RESTORE_FROM_UPCONTEXT xdx_OFFSET,%xdx
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xdx_OFFSET,%xdx
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *  endif
 *  if (absolute || TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *  endif
 *    RESTORE_FROM_UPCONTEXT xbp_OFFSET,%xbp
 *    RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 *  if (!absolute)
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *    else
 *      RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *    endif
 *  endif
 */
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
#ifdef X64
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R8, R8_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R9, R9_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R10, R10_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R11, R11_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R12, R12_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R13, R13_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R14, R14_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R15, R15_OFFSET));
#endif
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XAX, XAX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XBX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XCX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDX, XDX_OFFSET));
    /* must restore esi last */
    if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSI, XSI_OFFSET));
    /* must restore edi last */
    if (absolute || TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDI, XDI_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XBP, XBP_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSP, XSP_OFFSET));
    /* must restore esi last */
    if (!absolute) {
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSI, XSI_OFFSET));
        else
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDI, XDI_OFFSET));
    }
}

/* helper functions for append_fcache_return_common */

/* append instructions to save gpr
 *
 * if (!absolute)
 *   # get xax and xdi into their real slots, via xbx
 *   SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *   mov    fs:xax_OFFSET,%xbx
 *   SAVE_TO_UPCONTEXT %xbx,xax_OFFSET
 *   mov    fs:xdx_OFFSET,%xbx
 *   SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET
 *  endif
 *
 *  # save the current register state to context->regs
 *  # xax already in context
 *
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xcx,xcx_OFFSET
 *    SAVE_TO_UPCONTEXT %xdx,xdx_OFFSET
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    SAVE_TO_UPCONTEXT %xsi,xsi_OFFSET
 *  endif
 *
 *  # on X86
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xdi,xdi_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xbp,xbp_OFFSET
 *    SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *  ifdef X64
 *    SAVE_TO_UPCONTEXT %r8,r8_OFFSET
 *    SAVE_TO_UPCONTEXT %r9,r9_OFFSET
 *    SAVE_TO_UPCONTEXT %r10,r10_OFFSET
 *    SAVE_TO_UPCONTEXT %r11,r11_OFFSET
 *    SAVE_TO_UPCONTEXT %r12,r12_OFFSET
 *    SAVE_TO_UPCONTEXT %r13,r13_OFFSET
 *    SAVE_TO_UPCONTEXT %r14,r14_OFFSET
 *    SAVE_TO_UPCONTEXT %r15,r15_OFFSET
 *  endif
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    if (!absolute) {
        /* get xax and xdi from TLS into their real slots, via xbx */
        APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET));
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, DIRECT_STUB_SPILL_SLOT));
        if (linkstub != NULL) {
            /* app xax is still in  %xax, src info is in %xcx, while target pc
             * is now in %xbx
             */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XAX, XAX_OFFSET));
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, NEXT_TAG_OFFSET));
            APP(ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                            OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
            if (coarse_info) {
                APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, COARSE_DIR_EXIT_OFFSET));
#ifdef X64
                /* XXX: there are a few ways to perhaps make this a little
                 * cleaner: maybe a restore_indirect_branch_spill() or sthg,
                 * and IBL_REG to indirect xcx.
                 */
                if (GENCODE_IS_X86_TO_X64(code->gencode_mode) &&
                    DYNAMO_OPTION(x86_to_x64_ibl_opt))
                    APP(ilist, RESTORE_FROM_REG(dcontext, REG_XCX, REG_R9));
                else
#endif /* X64 */
                    APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
            }
        } else {
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XAX_OFFSET));
        }
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, DCONTEXT_BASE_SPILL_SLOT));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XDI_OFFSET));
    }

    /* save the current register state to context->regs
     * xax already in context
     */
    if (!ibl_end) {
        /* for ibl_end, xbx and xcx are already in their dcontext slots */
        if (absolute) /* else xbx saved above */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, XCX_OFFSET));
    }
    APP(ilist, SAVE_TO_DC(dcontext, REG_XDX, XDX_OFFSET));
    if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, SAVE_TO_DC(dcontext, REG_XSI, XSI_OFFSET));
    if (absolute) /* else xdi saved above */
        APP(ilist, SAVE_TO_DC(dcontext, REG_XDI, XDI_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_XBP, XBP_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_XSP, XSP_OFFSET));
#ifdef X64
    APP(ilist, SAVE_TO_DC(dcontext, REG_R8, R8_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R9, R9_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R10, R10_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R11, R11_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R12, R12_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R13, R13_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R14, R14_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R15, R15_OFFSET));
#endif /* X64 */
}

/* append instructions to save extension registers
 *  if preserve_xmm_caller_saved
 *    SAVE_TO_UPCONTEXT %xmm0,xmm_OFFSET+0*16
 *    SAVE_TO_UPCONTEXT %xmm1,xmm_OFFSET+1*16
 *    SAVE_TO_UPCONTEXT %xmm2,xmm_OFFSET+2*16
 *    SAVE_TO_UPCONTEXT %xmm3,xmm_OFFSET+3*16
 *    SAVE_TO_UPCONTEXT %xmm4,xmm_OFFSET+4*16
 *    SAVE_TO_UPCONTEXT %xmm5,xmm_OFFSET+5*16
 *    SAVE_TO_UPCONTEXT %xmm6,xmm_OFFSET+6*16  # 32-bit Linux
 *    SAVE_TO_UPCONTEXT %xmm7,xmm_OFFSET+7*16  # 32-bit Linux
 *  endif
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel.
         * Rather than try and optimize we save/restore on every cxt
         * sw.  The xmm field is aligned, so we can use movdqa/movaps,
         * though movdqu is stated to be as fast as movdqa when aligned:
         * but if so, why have two versions?  Is it only loads and not stores
         * for which that is true?  => PR 266305.
         * It's not clear that movdqa is any faster (and its opcode is longer):
         * movdqa and movaps are listed as the same latency and throughput in
         * the AMD optimization guide.  Yet examples of fast memcpy online seem
         * to use movdqa when sse2 is available.
         * Note that mov[au]p[sd] and movdq[au] are functionally equivalent.
         */
        /* FIXME i#438: once have SandyBridge processor need to measure
         * cost of vmovdqu and whether worth arranging 32-byte alignment
         */
        int i;
        uint opcode = move_mm_reg_opcode(true/*align32*/, true/*align16*/);
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            APP(ilist, instr_create_1dst_1src
                (dcontext, opcode,
                 OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_XMM,
                               XMM_OFFSET + i*XMM_SAVED_REG_SIZE),
                 opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i)));
        }
    }
}

/* append instructions to save xflags and clear it
 *  # now save eflags -- too hard to do without a stack on X86!
 *  pushf           # push eflags on stack
 *  pop     %xbx    # grab eflags value
 *  SAVE_TO_UPCONTEXT %xbx,xflags_OFFSET # save eflags value
 *
 *  # clear eflags now to avoid app's eflags messing up our ENTER_DR_HOOK
 *  # FIXME: this won't work at CPL0 if we ever run there!
 *  push  0
 *  popf
 */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    reg_id_t reg = IF_X86_ELSE(REG_XBX, DR_REG_R1);
#ifdef X86
    APP(ilist, INSTR_CREATE_RAW_pushf(dcontext));
    APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(reg)));
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
    APP(ilist, SAVE_TO_DC(dcontext, reg, XFLAGS_OFFSET));

    /* clear eflags now to avoid app's eflags (namely an app std)
     * messing up our ENTER_DR_HOOK
     */
#ifdef X86
    /* on x64 a push immed is sign-extended to 64-bit */
    /* XXX i#1147: can we clear DF and IF only? */
    APP(ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT8(0)));
    APP(ilist, INSTR_CREATE_RAW_popf(dcontext));
#elif defined(ARM)
    /* i#1551: do nothing on ARM, no DF or IF on ARM */
#endif /* X86/ARM */
}

/* append instructions to call enter_dr_hooks
 * # X86 only
 *  if (ENTER_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *      # don't bother to save any registers around call except for xax
 *      # and xcx, which holds next_tag
 *      push    %xcx
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    endif
 *      push    %xax
 *    if (absolute)
 *      # support for skipping the hook (note: 32-bits even on x64)
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmp     %edi,0
 *      jnz     post_hook
 *    endif
 *    # for x64 windows, reserve 32 bytes stack space for call prior to call
 *    call    ENTER_DR_HOOK
 *   post_hook:
 *    pop     %xax
 *    if (!absolute)
 *      pop     %xsi
 *      pop     %xdi
 *    endif
 *      pop     %xcx
 *  endif
 */
bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                          bool ibl_end, bool absolute)
{
    bool instr_target = false;
    if (ENTER_DR_HOOK != NULL) {
        /* xax is only reg we need to save around the call.
         * we could move to a callee-saved register instead of pushing.
         */
        instr_t *post_hook = INSTR_CREATE_label(dcontext);
        if (ibl_end) {
            /* also save xcx, which holds next_tag */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XCX)));
        }
        if (!absolute) {
            /* save xdi and xsi around call */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XDI)));
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSI)));
        }
        APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
#ifdef WINDOWS
        if (absolute) {
            /* For thread-private (used for syscalls), don't call if
             * dcontext->ignore_enterexit.  This is a perf hit to check: could
             * instead have a space hit via a separate routine.  This is only
             * needed right now for NtSuspendThread handling (see case 4942).
             */
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_EDI, IGNORE_ENTEREXIT_OFFSET));
            /* P4 opt guide says to use test to cmp reg with 0: shorter instr */
            APP(ilist, INSTR_CREATE_test(dcontext, opnd_create_reg(REG_EDI),
                                         opnd_create_reg(REG_EDI)));
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(post_hook)));
            instr_target = true;
        }
#endif /* WINDOWS */
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args,
         * and we don't want anyone clobbering our pushed registers!
         */
        dr_insert_call((void *)dcontext, ilist, NULL/*append*/,
                       (void *)ENTER_DR_HOOK, 0);
        APP(ilist, post_hook/*label*/);
        APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XAX)));
        if (!absolute) {
            /* save xdi and xsi around call */
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSI)));
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XDI)));
        }
        if (ibl_end) {
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX)));

            /* now we can store next tag */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, NEXT_TAG_OFFSET));
        }
    }
    return instr_target;
}
