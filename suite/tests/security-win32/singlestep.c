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
#include "tools.h"
#include <windows.h>


static int count = 0;
int foo();

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        count++;
        print("single step exception\n");
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

int
main(void)
{
    INIT();

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);

    print("start of test, count = %d\n", count);
    count+=foo();
    print("end of test, count = %d\n", count);

    return 0;
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
START_FILE

/* int foo()
 *   Generates a single step execution on jump and should return 2.
 */
#define FUNCNAME foo
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        xor      eax, eax
/* push flags on the stack */
#ifdef X64
        pushfq
        or       qword ptr [esp], HEX(100)
#else
        pushfd
        or       dword ptr [esp], HEX(100)
#endif
/* Setting the trap flag to 1 on top of the stack */
        jnz      flag_set
        ret
    flag_set:
        inc      eax
/* popping flags from top of the stack */
#ifdef X64
        popfq
#else
        popfd
#endif
/* single step exception should be triggered on this jump instruction */
        jmp      single_step
        ret
    single_step:
        inc      eax
        ret
END_FUNC(FUNCNAME)

END_FILE
#endif

