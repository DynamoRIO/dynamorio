/* **********************************************************
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
 * AArch64-specific assembly and trampoline code
 */

#include "../asm_defines.asm"
START_FILE
#include "include/syscall.h"

#ifndef UNIX
# error Non-Unix is not supported
#endif

#ifndef X64
# error X64 must be defined
#endif

#if defined(UNIX)
DECL_EXTERN(dr_setjmp_sigmask)
#endif

#ifdef UNIX
# if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
        DECLARE_FUNC(_start)
GLOBAL_LABEL(_start:)
        mov      x29, #0   /* clear frame ptr for stack trace bottom */
        CALLC2(GLOBAL_REF(relocate_dynamorio), #0, #0)
        CALLC3(GLOBAL_REF(privload_early_inject), sp, #0, #0)
        /* shouldn't return */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(_start)

        DECLARE_FUNC(xfer_to_new_libdr)
GLOBAL_LABEL(xfer_to_new_libdr:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(xfer_to_new_libdr)
# endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */
#endif /* UNIX */

/* All CPU ID registers are accessible only in privileged modes. */
        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        mov      w0, #0
        ret
        END_FUNC(cpuid_supported)

/* void call_switch_stack(dcontext_t *dcontext,       // REG_X0
 *                        byte *stack,                // REG_X1
 *                        void (*func)(dcontext_t *), // REG_X2
 *                        void *mutex_to_free,        // REG_X3
 *                        bool return_on_return)      // REG_W4
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* We need two callee-save regs across the call to func. */
        stp      x20, x30, [sp, #-32]!
        str      x19, [sp, #16]
        /* Check mutex_to_free. */
        cbz      ARG4, call_dispatch_alt_stack_no_free
        /* Release the mutex. */
        str      wzr, [ARG4]
call_dispatch_alt_stack_no_free:
        /* Copy ARG5 (return_on_return) to callee-save reg. */
        mov      w20, w4
        /* Switch stack. */
        mov      x19, sp
        mov      sp, ARG2
        /* Call func. */
        blr      ARG3
        /* Switch stack back. */
        mov      sp, x19
        /* Test return_on_return. */
        cbz      w20, GLOBAL_REF(unexpected_return)
        /* Restore and return. */
        ldr      x19, [sp, #16]
        ldp      x20, x30, [sp], #32
        ret
        END_FUNC(call_switch_stack)

#ifdef CLIENT_INTERFACE
        DECLARE_EXPORTED_FUNC(dr_call_on_clean_stack)
GLOBAL_LABEL(dr_call_on_clean_stack:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_call_on_clean_stack)
#endif /* CLIENT_INTERFACE */

#ifndef NOT_DYNAMORIO_CORE_PROPER

#ifdef DR_APP_EXPORTS
        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_app_start)

        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_app_take_over)

        DECLARE_EXPORTED_FUNC(dr_app_running_under_dynamorio)
GLOBAL_LABEL(dr_app_running_under_dynamorio:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_app_running_under_dynamorio)
#endif /* DR_APP_EXPORTS */

        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_app_take_over)

        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(cleanup_and_terminate)

#endif /* NOT_DYNAMORIO_CORE_PROPER */

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(global_do_syscall_int)

DECLARE_GLOBAL(safe_read_asm_pre)
DECLARE_GLOBAL(safe_read_asm_mid)
DECLARE_GLOBAL(safe_read_asm_post)
DECLARE_GLOBAL(safe_read_asm_recover)

/* i#350: Xref comment in x86.asm about safe_read.
 *
 * FIXME i#1569: NYI: We need to save the PC's that can fault and have
 * is_safe_read_pc() identify them.
 *
 * FIXME i#1569: We should optimize this as it can be on the critical path.
 *
 * void *safe_read_asm(void *dst, const void *src, size_t n);
 */
        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
        cmp      ARG3, #0
1:      b.eq     safe_read_asm_recover
ADDRTAKEN_LABEL(safe_read_asm_pre:)
        ldrb     w3, [ARG2]
ADDRTAKEN_LABEL(safe_read_asm_mid:)
ADDRTAKEN_LABEL(safe_read_asm_post:)
        strb     w3, [ARG1]
        subs     ARG3, ARG3, #1
        add      ARG2, ARG2, #1
        add      ARG1, ARG1, #1
        b        1b
ADDRTAKEN_LABEL(safe_read_asm_recover:)
        mov      x0, ARG2
        ret
        END_FUNC(safe_read_asm)

#ifdef UNIX
/* i#46: Private memcpy and memset for libc isolation.  Xref comment in x86.asm.
 */

/* Private memcpy.
 * FIXME i#1569: We should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      ldrb     w4, [ARG2], #1
        strb     w4, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memcpy)

/* Private memset.
 * FIXME i#1569: we should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      strb     w1, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
.hidden __memcpy_chk
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
.set __memset_chk,memset
#endif /* UNIX */

#ifdef CLIENT_INTERFACE

        DECLARE_EXPORTED_FUNC(dr_try_start)
GLOBAL_LABEL(dr_try_start:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_try_start)

/* We save only the callee-save registers: X19-X30, (gap), SP, D8-D15.
 *
 * int dr_setjmp(dr_jmp_buf_t *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        stp      x19, x20, [ARG1]
        stp      x21, x22, [ARG1, #16]
        stp      x23, x24, [ARG1, #32]
        stp      x25, x26, [ARG1, #48]
        stp      x27, x28, [ARG1, #64]
        stp      x29, x30, [ARG1, #80]
        mov      x7, sp
        str      x7, [ARG1, #104]
        stp      d8, d9, [ARG1, #112]
        stp      d10, d11, [ARG1, #128]
        stp      d12, d13, [ARG1, #144]
        stp      d14, d15, [ARG1, #160]
# ifdef UNIX
        str      x30, [sp, #-16]! /* save link register */
        bl       GLOBAL_REF(dr_setjmp_sigmask)
        ldr      x30, [sp], #16
# endif
        mov      w0, #0
        ret
        END_FUNC(dr_setjmp)

/* int dr_longjmp(dr_jmp_buf_t *buf,  int val);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        ldp      x19, x20, [ARG1]
        ldp      x21, x22, [ARG1, #16]
        ldp      x23, x24, [ARG1, #32]
        ldp      x25, x26, [ARG1, #48]
        ldp      x27, x28, [ARG1, #64]
        ldp      x29, x30, [ARG1, #80]
        ldr      x7, [ARG1, #104]
        mov      sp, x7
        ldp      d8, d9, [ARG1, #112]
        ldp      d10, d11, [ARG1, #128]
        ldp      d12, d13, [ARG1, #144]
        ldp      d14, d15, [ARG1, #160]
        cmp      w0, #0
        csinc    w0, w0, wzr, ne
        br       x30
        END_FUNC(dr_longjmp)

        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(atomic_swap)

#endif /* CLIENT_INTERFACE */

#ifdef UNIX
        DECLARE_FUNC(client_int_syscall)
GLOBAL_LABEL(client_int_syscall:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(client_int_syscall)

        DECLARE_FUNC(native_plt_call)
GLOBAL_LABEL(native_plt_call:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(native_plt_call)

        DECLARE_FUNC(_dynamorio_runtime_resolve)
GLOBAL_LABEL(_dynamorio_runtime_resolve:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(_dynamorio_runtime_resolve)
#endif /* UNIX */

#ifdef LINUX

        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_nonrt_sigreturn)
GLOBAL_LABEL(dynamorio_nonrt_sigreturn:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_nonrt_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_sys_exit)

        DECLARE_FUNC(dynamorio_futex_wake_and_exit)
GLOBAL_LABEL(dynamorio_futex_wake_and_exit:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_futex_wake_and_exit)

# ifndef NOT_DYNAMORIO_CORE_PROPER

#  ifndef HAVE_SIGALTSTACK
#   error NYI
#  endif
        DECLARE_FUNC(master_signal_handler)
GLOBAL_LABEL(master_signal_handler:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(master_signal_handler)

# endif /* NOT_DYNAMORIO_CORE_PROPER */

#endif /* LINUX */

        DECLARE_FUNC(hashlookup_null_handler)
GLOBAL_LABEL(hashlookup_null_handler:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(hashlookup_null_handler)

        DECLARE_FUNC(back_from_native_retstubs)
GLOBAL_LABEL(back_from_native_retstubs:)
DECLARE_GLOBAL(back_from_native_retstubs_end)
ADDRTAKEN_LABEL(back_from_native_retstubs_end:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(back_from_native_retstubs)

        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(back_from_native)

/* CTR_EL0 [19:16] : Log2 of number of words in smallest dcache line
 * CTR_EL0 [3:0]   : Log2 of number of words in smallest icache line
 *
 * PoC = Point of Coherency
 * PoU = Point of Unification
 *
 * void cache_sync_asm(void *beg, void *end);
 */
        DECLARE_FUNC(cache_sync_asm)
GLOBAL_LABEL(cache_sync_asm:)
        mrs      x3, ctr_el0
        mov      w4, #4
        ubfx     w2, w3, #16, #4
        lsl      w2, w4, w2 /* bytes in dcache line */
        and      w3, w3, #15
        lsl      w3, w4, w3 /* bytes in icache line */
        sub      w4, w2, #1
        bic      x4, x0, x4 /* aligned beg */
        b        2f
1:      dc       cvau, x4 /* Data Cache Clean by VA to PoU */
        add      x4, x4, x2
2:      cmp      x4, x1
        b.cc     1b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        sub      w4, w3, #1
        bic      x4, x0, x4 /* aligned beg */
        b        4f
3:      ic       ivau, x4 /* Instruction Cache Invalidate by VA to PoU */
        add      x4, x4, x3
4:      cmp      x4, x1
        b.cc     3b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        isb      /* Instruction Synchronization Barrier */
        ret
        END_FUNC(cache_sync_asm)

END_FILE
