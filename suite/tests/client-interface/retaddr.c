/* **********************************************************
 * Copyright (c) 2012-2015 Google, Inc.  All rights reserved.
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

#    include <assert.h>

void
callee(void)
{
    print("in callee\n");
}

/* asm routines */
void *
test_ret();
void *
test_iret();
void *
test_far_ret();

int
main(int argc, char *argv[])
{
    void *addr = test_ret();
    print("retaddr 0x%x\n", addr);
    addr = test_iret();
    print("retaddr 0x%x\n", addr);
    addr = test_far_ret();
    print("retaddr 0x%x\n", addr);
    print("All done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 8
#else
/* Don't need to align, but we do need to keep the "add esp, 0" to make a legal
 * SEH64 epilog.
 */
# define FRAME_PADDING 0
#endif

DECL_EXTERN(callee)

#define FUNCNAME test_ret
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* push callee-saved registers */
        /* XXX i#3289: test fails when changing this to
         * PUSH_CALLEE_SAVED_REGS, POP_CALLEE_SAVED_REGS
         */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG
        CALLC0(GLOBAL_REF(callee))
        mov      REG_XAX, PTRSZ [REG_XSP - ARG_SZ]
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_iret
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* push callee-saved registers */
        /* XXX i#3289: test fails when changing this to
         * PUSH_CALLEE_SAVED_REGS, POP_CALLEE_SAVED_REGS
         */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG
#ifdef X64
        push     HEX(2b) /* typical %ss value */
        lea      rax, [rsp + 8]
        push     rax
        pushfq
        push     HEX(33) /* typical %cs value */
#else
        pushfd
        push     cs
#endif
        call skip_iret
     next_instr_iret:
#ifdef X64
        mov      REG_XAX, PTRSZ [REG_XSP - 5*ARG_SZ]
#else
        mov      REG_XAX, PTRSZ [REG_XSP - 3*ARG_SZ]
#endif
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
     skip_iret:
#ifdef X64
        iretq /* yeah the default is ridiculously iretd */
#else
        iretd
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_far_ret
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* push callee-saved registers */
        /* XXX i#3289: test fails when changing this to
         * PUSH_CALLEE_SAVED_REGS, POP_CALLEE_SAVED_REGS
         */
        PUSH_SEH(REG_XBX)
        PUSH_SEH(REG_XBP)
        PUSH_SEH(REG_XSI)
        PUSH_SEH(REG_XDI)
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG
#ifdef X64
        push     HEX(33) /* typical %cs value */
#else
        push     cs
#endif
        call skip_far
     next_instr_far:
        mov      REG_XAX, PTRSZ [REG_XSP - 2*ARG_SZ]
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
        pop      REG_XBX
        ret
     skip_far:
#ifdef X64
        RAW(48) /* yeah the default is ridiculously 4 bytes */
#endif
        RAW(cb) /* far ret */
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
