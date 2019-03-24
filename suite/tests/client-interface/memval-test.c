/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"

void
test_asm(void);

int
main(void)
{
    print("memval-test running\n");
    test_asm();
    print("memval-test finished\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define FUNCNAME test_asm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        mov      REG_XBP, REG_XSP
        sub      REG_XSP, 256
        END_PROLOG
        jmp      setup_test

        /* i#2449: We target the following basic block, which caused a failure on
         * memval_simple:
         *   mov ecx, dword [edi + 0xc]
         *   mov edx, dword [local_48h]
         *   mov ebx, dword [local_50h]
         *   mov dword [local_48h], ecx    [1]
         *   mov dword [ebx + eax*4], edx  [2]
         *   mov ebx, esi
         *   pop ecx
         *   push dword [local_64h]
         *   call sub.std.__once_call_c50
         * Specifically, immediately after line [1] drreg reserved %eax to get the app
         * value written at [1]. On line [2], drreg also reserved reg %eax to get the app
         * address of the operand [ebx + eax*4]. This caused drreg to elide the app value
         * save/restore of eax, causing [ebx + eax*4] to be computed with a meta value
         * rather than an app value
         */
     setup_test:
        xor      REG_XAX, REG_XAX
        mov      REG_XDI, REG_XBP
        sub      REG_XDI, 12
        mov      [REG_XBP - 80], REG_XBP
        /* We can't use "test" as a label on Mac NASM. */
        jmp      test_start

     test_start:
        mov      REG_XCX, [REG_XDI + 12]
        mov      REG_XDX, [REG_XBP - 72]
        mov      REG_XBX, [REG_XBP - 80]
        mov      [REG_XBP - 72], REG_XCX
        mov      [REG_XBX + REG_XAX*4], REG_XDX
        mov      REG_XBX, REG_XSI
        pop      REG_XCX
        push     PTRSZ [REG_XBP - 100]
        jmp      epilog

     epilog:
        mov      REG_XSP, REG_XBP
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
END_FILE
/* clang-format on */
#endif
