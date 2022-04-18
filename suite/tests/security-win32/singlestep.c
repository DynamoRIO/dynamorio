/* **********************************************************
 * Copyright (c) 2017 Simorfo, Inc.  All rights reserved.
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
 * * Neither the name of Simorfo nor the names of its contributors may be
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include <windows.h>

static int count = 0;
int
foo();

void
single_step_addr(void);

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        print("single step exception\n");
        if (pExceptionInfo->ExceptionRecord->ExceptionAddress == single_step_addr) {
            count++;
        } else {
            print("got address " PFX ", expected " PFX "\n",
                  pExceptionInfo->ExceptionRecord->ExceptionAddress, single_step_addr);
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

int
main(void)
{
    INIT();

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);

    print("start of test, count = %d\n", count);
    protect_mem(foo, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    count += foo();
    print("end of test, count = %d\n", count);

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

/* int foo()
 *   Generates a single step execution on jump and should return 2.
 */
#define FUNCNAME foo
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
/* sets sandoxing mode in dynamorio by doing a self modification. */
        mov      REG_XAX, HEX(1)
        lea      REG_XDX, SYMREF(sandbox_immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(sandbox_immediate_addr_plus_four:)
        mov      REG_XAX, REG_XDX
/* push flags on the stack */
        PUSHF
/* setting the trap flag to 1 on top of the stack */
        or       PTRSZ [REG_XSP], HEX(100)
/* popping flags from top of the stack */
        POPF
/* single step exception should be triggered on this jump instruction */
        jmp      single_step
        ret
    single_step:
DECLARE_GLOBAL(single_step_addr)
ADDRTAKEN_LABEL(single_step_addr:)
        inc      eax
        ret
END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
