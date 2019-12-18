/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* Split C/asm source file. */
#ifndef ASM_CODE_ONLY

#    ifdef USE_DYNAMO
/* for dr_app_handle_mbr_target and dr_app_running_under_dynamorio */
#        include "configure.h"
#        include "dr_api.h"
#    endif /* USE_DYNAMO */

#    ifdef WINDOWS
/* importing from DR causes trouble injecting */
#        include "dr_annotations.h"
#        define IS_UNDER_DR() DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()
#    else
#        define IS_UNDER_DR() dr_app_running_under_dynamorio()
#    endif

/* nativeexec.appdll.dll
 * nativeexec.exe calls routines here w/ different call* constructions
 */
#    include "tools.h"

typedef void (*int_fn_t)(int);
typedef int (*int2_fn_t)(int, int);
typedef void (*tail_caller_t)(int_fn_t, int);

/* When the appdll is running natively, the indirect function call may
 * jump to a native module directly. Thus we must replace the function
 * pointer with the stub pc returned by dr_app_handle_mbr_target.
 */
#    ifdef UNIX
#        define CALL_FUNC(fn, x)                                         \
            do {                                                         \
                if (dr_app_running_under_dynamorio()) {                  \
                    fn(x);                                               \
                } else {                                                 \
                    int_fn_t fn2 = dr_app_handle_mbr_target((void *)fn); \
                    fn2(x);                                              \
                }                                                        \
            } while (0)
#    else
#        define CALL_FUNC(fn, x) \
            do {                 \
                fn(x);           \
            } while (0)
#    endif

int
import_ret_imm(int x, int y);
void
tail_caller(int_fn_t fn, int x);

void EXPORT
import_me1(int x)
{
    print("nativeexec.dll:import_me1(%d) %sunder DR\n", x, IS_UNDER_DR() ? "" : "not ");
}

void EXPORT
import_me2(int x)
{
    print("nativeexec.dll:import_me2(%d) %sunder DR\n", x, IS_UNDER_DR() ? "" : "not ");
}

void EXPORT
import_me3(int x)
{
    print("nativeexec.dll:import_me3(%d) %sunder DR\n", x, IS_UNDER_DR() ? "" : "not ");
}

void EXPORT
import_me4(int_fn_t fn, int x)
{
    CALL_FUNC(fn, x);
}

void EXPORT
unwind_level1(int_fn_t fn, int x)
{
    CALL_FUNC(fn, x);
}

void EXPORT
unwind_level3(int_fn_t fn, int x)
{
    CALL_FUNC(fn, x);
}

void EXPORT
unwind_level5(int_fn_t fn, int x)
{
    CALL_FUNC(fn, x);
}

#    ifdef WINDOWS
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    return TRUE;
}
#    endif

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

        DECLARE_EXPORTED_FUNC(import_ret_imm)
GLOBAL_LABEL(import_ret_imm:)
        /* XXX: Not doing SEH prologue for test code. */
        mov      REG_XAX, [REG_XSP + 1 * ARG_SZ] /* arg1 */
        add      REG_XAX, [REG_XSP + 2 * ARG_SZ] /* arg2 */
        ret      2 * ARG_SZ    /* Callee cleared args, ret_imm. */
        END_FUNC(import_ret_imm)

/* void tail_caller(int_fn_t fn, int x) -- Tail call fn(x).
 *
 * i#1077: If fn is in a non-native module and we take over, we used to end up
 * interpreting the back_from_native return address on the stack.
 */
        DECLARE_EXPORTED_FUNC(tail_caller)
GLOBAL_LABEL(tail_caller:)
        /* XXX: Not doing SEH prologue for test code. */
        mov      REG_XAX, ARG1      /* put fn in xax */
        mov      REG_XCX, ARG2      /* mov x to arg1 */
        mov      ARG1, REG_XCX
        jmp      REG_XAX            /* tail call */
        END_FUNC(tail_caller)

END_FILE
/* clang-format on */

#endif /* ASM_CODE_ONLY */
