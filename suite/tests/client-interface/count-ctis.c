/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */
#    include <stdio.h>

/* in asm */
void
test_jecxz(int *x);

int
main()
{
    int x = 0;
    test_jecxz(&x);
    print("x=0x%08x\n", x);
    print("thank you for testing the client interface\n");
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
#ifdef X86
START_FILE

#define FUNCNAME test_jecxz
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        END_PROLOG

        /* test jecxz */
        mov      REG_XCX, 0
        jecxz    jecxz_taken
        nop
     jecxz_taken:
        mov      REG_XCX, 1
        jecxz    jecxz_not_taken
        nop
     jecxz_not_taken:
#ifdef X64
        mov      ecx, 0
        jecxz    jcxz_taken
#else
        mov      cx, 0
        jcxz     jcxz_taken
#endif
        nop
     jcxz_taken:

        /* test loop */
        mov      REG_XCX, REG_XAX
        inc      REG_XCX
        loop     loop_taken
        nop
    loop_taken:
        /* test xcx being preserved */
        mov      DWORD [REG_XCX], HEX(abcd1234)

        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)


END_FILE
#elif defined(AARCH64)
START_FILE

#define FUNCNAME test_jecxz
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* begin roi via nop; nop; add; nop */
        nop; nop; add x0, x0, 0; nop

        mov      x1, ARG1
        END_PROLOG

        /* test cbz */
        mov      x1, 0
        cbz      x1, cbz_taken
        nop
     cbz_taken:
        mov      x1, 1
        cbz      x1, cbz_not_taken
        nop
     cbz_not_taken:
        nop

        /* test cbnz */
        mov      x1, 1
        cbnz     x1, cbnz_taken
        nop
     cbnz_taken:
        mov      x1, 0
        cbnz     x1, cbnz_not_taken
        nop
     cbnz_not_taken:
        nop

        /* test cbz using stolen register x28 */
        mov      x1, x28
        mov      x28, 0
        cbz      x28, cbz_x28_taken
        nop
     cbz_x28_taken:
        mov      x28, 1
        cbz      x28, cbz_x28_not_taken
        nop
     cbz_x28_not_taken:
        nop
        mov      x28, x1

        /* test cbnz using stolen register x28 */
        mov      x1, x28
        mov      x28, 1
        cbnz     x28, cbnz_x28_taken
        nop
     cbnz_x28_taken:
        mov      x28, 0
        cbnz     x28, cbnz_x28_not_taken
        nop
     cbnz_x28_not_taken:
        nop
        mov      x28, x1

        /* test tbz */
        mov      x1, 0
        tbz      x1, #1, tbz_taken
        nop
     tbz_taken:
        mov      x1, 2
        tbz      x1, #1, tbz_not_taken
        nop
     tbz_not_taken:
        nop

        /* test tbnz */
        mov      x1, 4
        tbnz     x1, #2, tbnz_taken
        nop
     tbnz_taken:
        mov      x1, 0
        tbnz     x1, #2, tbnz_not_taken
        nop
     tbnz_not_taken:
        nop

        /* test tbz using stolen register x28 */
        mov      x1, x28
        mov      x28, 0
        tbz      x28, #1, tbz_x28_taken
        nop
     tbz_x28_taken:
        mov      x28, 2
        tbz      x28, #1, tbz_x28_not_taken
        nop
     tbz_x28_not_taken:
        nop
        mov      x28, x1

        /* test tbnz using stolen register x28 */
        mov      x1, x28
        mov      x28, 2
        tbnz     x28, #1, tbnz_x28_taken
        nop
     tbnz_x28_taken:
        mov      x28, 0
        tbnz     x28, #1, tbnz_x28_not_taken
        nop
     tbnz_x28_not_taken:
        nop
        mov      x28, x1

        /* test bcond */
        mov      x1, 0
        cmp      x1, 0
        beq      beq_taken
        nop
     beq_taken:
        cmp      x1, 0
        bne      bne_not_taken
        nop
     bne_not_taken:
        nop

        /* end roi via nop; nop; add; nop */
        nop; nop; add x0, x0, 0; nop

        /* test x0 being preserved */
        ldr      x1, =0xabcd1234
        str      x1, [x0]

        ret
        END_FUNC(FUNCNAME)


END_FILE
#endif
/* clang-format on */
#endif
