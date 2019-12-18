/* **********************************************************
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"

int
foo(int value);

int
main(void)
{
    INIT();

    protect_mem(foo, PAGE_SIZE, ALLOW_EXEC | ALLOW_WRITE | ALLOW_READ);

    print("foo returned %d\n", foo(10));
    print("foo returned %d\n", foo(10));

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

/* int bar(int value)
 *   Returns value.  We avoid issues with bar resolving to a jump table on
 *   Windows by skipping DECLARE_FUNC/END_FUNC and putting the labels in
 *   manually.
 */
ADDRTAKEN_LABEL(bar:)
        mov    REG_XAX, ARG1
        shl    REG_XAX, 1
        ret
ADDRTAKEN_LABEL(bar_end:)

/* int foo(int value)
 *   copies bar over the front of itself, so future invocations will
 *   run bar's code
 */
#define FUNCNAME foo
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, ARG1
        /* save callee-saved regs */
        push REG_XSI
        push REG_XDI
        /* set up for copy */
        lea  REG_XSI, SYMREF(bar)
        lea  REG_XDI, SYMREF(foo)
        lea  REG_XCX, SYMREF(bar_end)
        sub  REG_XCX, REG_XSI
        cld
        rep movsb
        /* restore callee-saved regs */
        pop REG_XDI
        pop REG_XSI
        ret
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
