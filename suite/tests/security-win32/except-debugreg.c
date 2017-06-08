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
void set_debug_register();
void test_debug_register();
void single_step_addr(void);


/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        // FIXME : setting debug registers this way works only on 32 bit
        // We should find another way compatible with 64-bit to be able to test it

        // Should get here thanks to the int 3 instruction in set_debug_register
        // Set Dr0 to the address where to add a breakpoint
        pExceptionInfo->ContextRecord->Dr0 = (unsigned int) single_step_addr;
        pExceptionInfo->ContextRecord->Dr6 = 0xfffe0ff0;
        // Set Dr7 to enable Dr0 breakpoint
        pExceptionInfo->ContextRecord->Dr7 = 0x00000101;
        print("set debug register\n");
        // Increment pc pointer to go to next instruction and avoid infinite loop
#ifndef X64
        pExceptionInfo->ContextRecord->Eip++;
#else
        pExceptionInfo->ContextRecord->Rip++;
#endif
        return EXCEPTION_CONTINUE_EXECUTION;
    } else if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        print("single step seen\n");
        if (pExceptionInfo->ExceptionRecord->ExceptionAddress == single_step_addr) {
            count++;
        }
        else {
            print("got address "PFX", expected "PFX"\n",
                  pExceptionInfo->ExceptionRecord->ExceptionAddress,
                  single_step_addr);
        }
        //deactivate breakpoint
        pExceptionInfo->ContextRecord->Dr0 = 0;
        pExceptionInfo->ContextRecord->Dr6 = 0;
        pExceptionInfo->ContextRecord->Dr7 = 0;
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

    set_debug_register();
    test_debug_register();

    print("end of test, count = %d\n", count);

    return 0;
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
START_FILE

/* void set_debug_register()
 *   Generates one int 3 interruption and returns.
 */
#define FUNCNAME set_debug_register
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        int      3
        nop
        nop
        ret
END_FUNC(FUNCNAME)

/* void test_debug_register()
 * Some amount of dummy code where to put a breakpoint
 */
#define FUNCNAME test_debug_register
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        nop
        nop
DECLARE_GLOBAL(single_step_addr)
ADDRTAKEN_LABEL(single_step_addr:)
        nop
        xor      eax, eax
        test     eax, eax
        nop
        nop
        ret
END_FUNC(FUNCNAME)

END_FILE
#endif

