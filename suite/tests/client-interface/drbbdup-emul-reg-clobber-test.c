/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* Test i#5906: verify that drbbdup does not clobber app values when expanding
 * reps. */

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */
#    include <stdio.h>

/* in asm */
void
test_reg_clobber(char *buf1, char *buf2, int *x);

int
main()
{
    char buf1[1024];
    char buf2[1024];
    int x = 0;
    /* This function will copy some bytes from buf2 to buf1 and return a magic value in x.
     */
    test_reg_clobber(buf1, buf2, &x);
    print("x=%08x\n", x);
    print("Hello, world!\n");
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME test_reg_clobber
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XDI, ARG1
        mov      REG_XSI, ARG2
        mov      REG_XDX, ARG3
        PUSH_CALLEE_SAVED_REGS()
        END_PROLOG

        /* Save a magic value in XAX. The value is set before the rep
         * instruction and should still be in XAX after the rep instruction. */
        mov      REG_XAX, HEX(abcdabcd)

        /* Rep mov that is expanded by the client. It should not clobber the
         * value in XAX. */
        mov      REG_XCX, 10
        rep movsb

        /* Return the value in the output parameter. */
        mov      PTRSZ [REG_XDX], REG_XAX

        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)


END_FILE
/* clang-format on */
#endif
