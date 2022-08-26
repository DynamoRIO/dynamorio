/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc. All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/***************************************************************************
 * Assembly and trampoline code shared between between ARM and AArch64.
 */

#include "../asm_defines.asm"
START_FILE

#ifdef UNIX
# if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
        DECLARE_FUNC(_start)
GLOBAL_LABEL(_start:)
        /* i#38: Attaching in middle of blocking syscall requires padded null bytes. */
        nop
        mov      FP, #0   /* clear frame ptr for stack trace bottom */
        /* i#1676, i#1708: relocate dynamorio if it is not loaded to preferred address.
         * We call this here to ensure it's safe to access globals once in C code
         * (xref i#1865).
         */
        CALLC3(GLOBAL_REF(relocate_dynamorio), #0, #0, sp)

        /* Clear 2nd & 3rd args to distinguish from xfer_to_new_libdr */
        mov      ARG2, #0
        mov      ARG3, #0

        /* Entry from xfer_to_new_libdr is here.  It has set up 2nd & 3rd args already. */
.L_start_invoke_C:
        mov      FP, #0   /* clear frame ptr for stack trace bottom */
        mov      ARG1, sp  /* 1st arg to privload_early_inject */
        bl       GLOBAL_REF(privload_early_inject)
        /* shouldn't return */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(_start)

/* i#1227: on a conflict with the app we reload ourselves.
 * xfer_to_new_libdr(entry, init_sp, cur_dr_map, cur_dr_size)
 * =>
 * Invokes entry after setting sp to init_sp and placing the current (old)
 * libdr bounds in registers for the new libdr to unmap.
 */
        DECLARE_FUNC(xfer_to_new_libdr)
GLOBAL_LABEL(xfer_to_new_libdr:)
        mov      REG_PRESERVED_1, ARG1
        /* Restore sp */
        mov      sp, ARG2
        /* Skip prologue that calls relocate_dynamorio() and clears args 2+3 by
         * adjusting the _start in the reloaded DR by the same distance as in
         * the current DR, but w/o clobbering ARG3 or ARG4.
         */
        adr      ARG1, .L_start_invoke_C
        adr      ARG2, GLOBAL_REF(_start)
        sub      ARG1, ARG1, ARG2
        add      REG_PRESERVED_1, REG_PRESERVED_1, ARG1
        /* _start expects these as 2nd & 3rd args */
        mov      ARG2, ARG3
        mov      ARG3, ARG4
        INDJMP   REG_PRESERVED_1
        END_FUNC(xfer_to_new_libdr)
# endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */
#endif /* UNIX */

/* We need to call futex_wakeall without using any stack.
 * Takes KSYNCH_TYPE* in r0 and the post-syscall jump target in r1.
 */
        DECLARE_FUNC(dynamorio_condvar_wake_and_jmp)
GLOBAL_LABEL(dynamorio_condvar_wake_and_jmp:)
        mov      REG_R12, REG_R1 /* save across syscall */
        mov      REG_R5, #0 /* arg6 */
        mov      REG_R4, #0 /* arg5 */
        mov      REG_R3, #0 /* arg4 */
        mov      REG_R2, #0x7fffffff /* arg3 = INT_MAX */
        mov      REG_R1, #1 /* arg2 = FUTEX_WAKE */
        /* arg1 = &futex, already in r0 */
#ifdef AARCH64
        mov      w8, #98 /* SYS_futex */
#else
        mov      r7, #240 /* SYS_futex */
#endif
        svc      #0
        INDJMP   REG_R12
        END_FUNC(dynamorio_condvar_wake_and_jmp)

END_FILE
