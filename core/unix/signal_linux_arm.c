/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * signal_linux_arm.c - Linux and ARM specific signal code
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
#    error Linux-only
#endif

#ifndef ARM
#    error ARM-only
#endif

#include "arch.h"

#define VFP_QUERY_SIG SIGILL

/**** floating point support ********************************************/

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

#ifdef DEBUG
void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
    LOG(THREAD, LOG_ASYNCH, 1, "\tr0  =" PFX "\n", sc->SC_R0);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr1  =" PFX "\n", sc->SC_R1);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr2  =" PFX "\n", sc->SC_R2);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr3  =" PFX "\n", sc->SC_R3);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr4  =" PFX "\n", sc->SC_R4);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr5  =" PFX "\n", sc->SC_R5);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr6  =" PFX "\n", sc->SC_R6);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr7  =" PFX "\n", sc->SC_R7);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr8  =" PFX "\n", sc->SC_R8);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr9  =" PFX "\n", sc->SC_R9);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr10 =" PFX "\n", sc->SC_R10);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr11 =" PFX "\n", sc->SC_R11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr12 =" PFX "\n", sc->SC_R12);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp  =" PFX "\n", sc->SC_XSP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr14 =" PFX "\n", sc->SC_LR);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpc  =" PFX "\n", sc->SC_XIP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcpsr=" PFX "\n", sc->SC_XFLAGS);
    /* XXX: should we take in sig_full_cxt_t to dump SIMD regs? */
}
#endif /* DEBUG */

/* There is a bug in all released kernels up to 4.12 when CONFIG_IWMMXT is
 * enabled but the hardware is not present or not used: the VFP frame in the
 * ucontext is offset as if there were a preceding IWMMXT frame, though this
 * memory is in fact not written to by the kernel. In 4.13 the bug is fixed
 * with a minimal change to the ABI by writing a dummy padding block before
 * the VFP frame. We work around the bug and handle all cases by sending
 * ourselves a signal at init time and looking for the VFP frame in both places.
 * If it was found in the offset position we do not expect there to be a valid
 * dummy padding block when reading the sigcontext but we create one when
 * writing the sigcontext.
 * XXX: Because of the IWMMXT space being unset by earlier kernels it is
 * possible that we might find the VFP frame header in both places. We could
 * guard against that by clearing some memory below the SP before sending the
 * signal (assuming sigaltstack has not been used).
 */

static bool vfp_is_offset = false;

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
    if (sc_full->fp_simd_state == NULL)
        return;
    char *frame = ((char *)sc_full->fp_simd_state +
                   (vfp_is_offset ? sizeof(kernel_iwmmxt_sigframe_t) : 0));
    kernel_vfp_sigframe_t *vfp = (kernel_vfp_sigframe_t *)frame;
    ASSERT(sizeof(mc->simd) == sizeof(vfp->ufp.fpregs));
    ASSERT(vfp->magic == VFP_MAGIC);
    ASSERT(vfp->size == sizeof(kernel_vfp_sigframe_t));
    memcpy(&mc->simd[0], &vfp->ufp.fpregs[0], sizeof(mc->simd));
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
    if (sc_full->fp_simd_state == NULL)
        return;
    char *frame = (char *)sc_full->fp_simd_state;
    kernel_vfp_sigframe_t *vfp;
    if (vfp_is_offset) {
        kernel_iwmmxt_sigframe_t *dummy = (kernel_iwmmxt_sigframe_t *)frame;
        dummy->magic = DUMMY_MAGIC;
        dummy->size = sizeof(*dummy);
        frame += sizeof(*dummy);
    }
    vfp = (kernel_vfp_sigframe_t *)frame;
    ASSERT(sizeof(mc->simd) == sizeof(vfp->ufp.fpregs));
    vfp->magic = VFP_MAGIC;
    vfp->size = sizeof(kernel_vfp_sigframe_t);
    memcpy(&vfp->ufp.fpregs[0], &mc->simd[0], sizeof(vfp->ufp.fpregs));
    /* Terminate the frame list with zero magic. */
    vfp[1].magic = 0;
}

size_t
signal_frame_extra_size(bool include_alignment)
{
    return 0;
}

static void
vfp_query_signal_handler(int sig, kernel_siginfo_t *siginfo, kernel_ucontext_t *ucxt)
{
    uint offset = sizeof(kernel_iwmmxt_sigframe_t);
    char *coproc = (char *)&ucxt->coproc;
    /* We look for the VFP frame in two places, hoping to find it in
     * exactly one of them. See longer comment above.
     */
    kernel_vfp_sigframe_t *vfp0 = (kernel_vfp_sigframe_t *)coproc;
    kernel_vfp_sigframe_t *vfp1 = (kernel_vfp_sigframe_t *)(coproc + offset);
    bool vfp0_good =
        (vfp0->magic == VFP_MAGIC && vfp0->size == sizeof(kernel_vfp_sigframe_t));
    bool vfp1_good =
        (vfp1->magic == VFP_MAGIC && vfp1->size == sizeof(kernel_vfp_sigframe_t));
    ASSERT(offset == 160);
    if (vfp0_good == vfp1_good) {
        SYSLOG(SYSLOG_CRITICAL, CANNOT_FIND_VFP_FRAME, 2, get_application_name(),
               get_application_pid());
        os_terminate(NULL, TERMINATE_PROCESS);
    }
    vfp_is_offset = vfp1_good;
    /* Detect if we unexpectedly have a filled-in IWMMXT frame. */
    ASSERT(!(vfp0->magic == IWMMXT_MAGIC && vfp0->size == offset));
}

void
signal_arch_init(void)
{
    kernel_sigaction_t act, oldact;
    int rc;
    memset(&act, 0, sizeof(act));
    set_handler_sigact(&act, VFP_QUERY_SIG, (handler_t)vfp_query_signal_handler);
    rc = sigaction_syscall(VFP_QUERY_SIG, &act, &oldact);
    ASSERT(rc == 0);
    thread_signal(get_process_id(), get_sys_thread_id(), VFP_QUERY_SIG);
    rc = sigaction_syscall(VFP_QUERY_SIG, &oldact, NULL);
    ASSERT(rc == 0);
}
