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

# if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
        DECLARE_FUNC(_start)
GLOBAL_LABEL(_start:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(_start)

        DECLARE_FUNC(xfer_to_new_libdr)
GLOBAL_LABEL(xfer_to_new_libdr:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(xfer_to_new_libdr)
# endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */

        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
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

        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
ADDRTAKEN_LABEL(safe_read_asm_pre:)
ADDRTAKEN_LABEL(safe_read_asm_mid:)
ADDRTAKEN_LABEL(safe_read_asm_post:)
ADDRTAKEN_LABEL(safe_read_asm_recover:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(safe_read_asm)

        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(memcpy)

        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
.hidden __memcpy_chk
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
.set __memset_chk,memset

#ifdef CLIENT_INTERFACE

        DECLARE_EXPORTED_FUNC(dr_try_start)
GLOBAL_LABEL(dr_try_start:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_try_start)

        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_setjmp)

        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dr_longjmp)

        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(atomic_swap)

#endif /* CLIENT_INTERFACE */

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

#ifndef NOT_DYNAMORIO_CORE_PROPER

#ifndef HAVE_SIGALTSTACK
# error NYI
#endif
        DECLARE_FUNC(master_signal_handler)
GLOBAL_LABEL(master_signal_handler:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(master_signal_handler)

#endif /* NOT_DYNAMORIO_CORE_PROPER */

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

        DECLARE_FUNC(cache_sync_asm)
GLOBAL_LABEL(cache_sync_asm:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(cache_sync_asm)

END_FILE
