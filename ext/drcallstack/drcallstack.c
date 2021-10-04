/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.   All rights reserved.
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

/* DynamoRIO Register Management Extension: a mediator for
 * selecting, preserving, and using registers among multiple
 * instrumentation components.
 */

/* XXX i#511: currently the whole interface is tied to drmgr.
 * Should we also provide an interface that works on standalone instrlists?
 * Distinguish by name, "drregi_*" or sthg.
 */

#include "dr_api.h"
#include "drcallstack.h"
#include "../ext_utils.h"
#include "../../core/unix/os_public.h" /* SIGCXT_FROM_UCXT, SC_FIELD */

#define UNW_LOCAL_ONLY /* Speed up libunwind by disallowing remote. */
#include <libunwind.h>

static int drcallstack_init_count;

struct _drcallstack_walk_t {
    /* For now we only support libunwind. */
    unw_context_t uc;
    unw_cursor_t cursor;
};

drcallstack_status_t
drcallstack_init(drcallstack_options_t *ops_in)
{
    int count = dr_atomic_add32_return_sum(&drcallstack_init_count, 1);
    if (count > 1)
        return DRCALLSTACK_SUCCESS;
    /* This does nothing now.  We anticipate adding callstack storage and
     * module indexing which might need event registration in the future.
     */
    if (ops_in->struct_size != sizeof(*ops_in))
        return DRCALLSTACK_ERROR_INVALID_PARAMETER;
    return DRCALLSTACK_SUCCESS;
}

drcallstack_status_t
drcallstack_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drcallstack_init_count, -1);
    if (count != 0)
        return DRCALLSTACK_SUCCESS;
    return DRCALLSTACK_SUCCESS;
}

drcallstack_status_t
drcallstack_init_walk(dr_mcontext_t *mc, OUT drcallstack_walk_t **walk_out)
{
    if (!TEST(DR_MC_CONTROL, mc->flags))
        return DRCALLSTACK_ERROR_INVALID_PARAMETER;

    drcallstack_walk_t *walk = dr_thread_alloc(dr_get_current_drcontext(), sizeof(*walk));
    *walk_out = walk;

    /* We assume that SIMD registers are not needed and thus we do not call
     * unw_getcontext(&walk->uc).
     */

    /* Convert dr_mcontext_t into unw_context_t. */
#ifdef LINUX
#    ifdef X86
    /* unw_context_t is ucontext_t. */
    sigcontext_t *sc = SIGCXT_FROM_UCXT(&walk->uc);
    /* These are the 4 that are needed for a fast trace. */
    sc->SC_XIP = (ptr_uint_t)mc->xip;
    sc->SC_XSP = mc->xsp;
    sc->SC_XBP = mc->xbp;
    sc->SC_XBX = mc->xbx;
    /* For completeness we do all the GPR's.  We do not bother w/ SIMD. */
    sc->SC_XAX = mc->xax;
    sc->SC_XCX = mc->xcx;
    sc->SC_XDX = mc->xdx;
    sc->SC_XSI = mc->xsi;
    sc->SC_XDI = mc->xdi;
#        ifdef X64
    sc->SC_R8 = mc->r8;
    sc->SC_R9 = mc->r9;
    sc->SC_R10 = mc->r10;
    sc->SC_R11 = mc->r11;
    sc->SC_R12 = mc->r12;
    sc->SC_R13 = mc->r13;
    sc->SC_R14 = mc->r14;
    sc->SC_R15 = mc->r15;
#        endif
#    elif defined(AARCH64)
    /* unw_context_t matches at least the GPR portion of ucontext_t. */
    sigcontext_t *sc = SIGCXT_FROM_UCXT(&walk->uc);
    sc->SC_XIP = (ptr_uint_t)mc->xip;
    sc->SC_R0 = mc->r0;
    sc->SC_R1 = mc->r1;
    sc->SC_R2 = mc->r2;
    sc->SC_R3 = mc->r3;
    sc->SC_R4 = mc->r4;
    sc->SC_R5 = mc->r5;
    sc->SC_R6 = mc->r6;
    sc->SC_R7 = mc->r7;
    sc->SC_R8 = mc->r8;
    sc->SC_R9 = mc->r9;
    sc->SC_R10 = mc->r10;
    sc->SC_R11 = mc->r11;
    sc->SC_R12 = mc->r12;
    sc->SC_R13 = mc->r13;
    sc->SC_R14 = mc->r14;
    sc->SC_R15 = mc->r15;
    sc->SC_R16 = mc->r16;
    sc->SC_R17 = mc->r17;
    sc->SC_R18 = mc->r18;
    sc->SC_R19 = mc->r19;
    sc->SC_R20 = mc->r20;
    sc->SC_R21 = mc->r21;
    sc->SC_R22 = mc->r22;
    sc->SC_R23 = mc->r23;
    sc->SC_R24 = mc->r24;
    sc->SC_R25 = mc->r25;
    sc->SC_R26 = mc->r26;
    sc->SC_R27 = mc->r27;
    sc->SC_R28 = mc->r28;
    sc->SC_FP = mc->29;
    sc->SC_LR = mc->lr;
    sc->SC_XSP = mc->xsp;
#    elif defined(ARM)
    /* libunwind defines its own struct of 16 regs. */
    walk->uc.regs[0] = mc->r0;
    walk->uc.regs[1] = mc->r1;
    walk->uc.regs[2] = mc->r2;
    walk->uc.regs[3] = mc->r3;
    walk->uc.regs[4] = mc->r4;
    walk->uc.regs[5] = mc->r5;
    walk->uc.regs[6] = mc->r6;
    walk->uc.regs[7] = mc->r7;
    walk->uc.regs[8] = mc->r8;
    walk->uc.regs[9] = mc->r9;
    walk->uc.regs[10] = mc->r10;
    walk->uc.regs[11] = mc->r11;
    walk->uc.regs[12] = mc->r12;
    walk->uc.regs[13] = mc->r13;
    walk->uc.regs[14] = mc->r14;
    walk->uc.regs[15] = mc->r15; /* The pc. */
#    else
    return DRCALLSTACK_ERROR_FEATURE_NOT_AVAILABLE;
#    endif
#else
    /* TODO i#2414: Implement Windows and MacOS support. */
    return DRCALLSTACK_ERROR_FEATURE_NOT_AVAILABLE;
#endif

    /* Set up libunwind. */
    unw_init_local2(&walk->cursor, &walk->uc, UNW_INIT_SIGNAL_FRAME);

    return DRCALLSTACK_SUCCESS;
}

drcallstack_status_t
drcallstack_cleanup_walk(drcallstack_walk_t *walk)
{
    dr_thread_free(dr_get_current_drcontext(), walk, sizeof(*walk));
    return DRCALLSTACK_SUCCESS;
}

drcallstack_status_t
drcallstack_next_frame(drcallstack_walk_t *walk, OUT drcallstack_frame_t *frame)
{
    if (frame->struct_size != sizeof(*frame))
        return DRCALLSTACK_ERROR_INVALID_PARAMETER;
    int res = unw_step(&walk->cursor);
    if (res == 0)
        return DRCALLSTACK_NO_MORE_FRAMES;
    if (res < 0) {
        /* We could expose different error codes if we find a use case. */
#ifdef VERBOSE
        dr_fprintf(STDERR, "libunwind raw error %d\n", res);
#endif
        return DRCALLSTACK_ERROR;
    }
    if (unw_get_reg(&walk->cursor, UNW_REG_IP, (ptr_uint_t *)&frame->pc) != 0 ||
        unw_get_reg(&walk->cursor, UNW_REG_SP, &frame->sp) != 0)
        return DRCALLSTACK_ERROR;
    return DRCALLSTACK_SUCCESS;
}
