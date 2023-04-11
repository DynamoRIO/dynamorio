/* **********************************************************
 * Copyright (c) 2012-2014 Google, Inc.  All rights reserved.
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
#    include <stdio.h>

#    define VERBOSE 0

ptr_int_t
get_retaddr(void);
ptr_int_t
get_retaddr_from_frameptr(void);

#    undef NO_FRAME_POINTER
#    if (defined(X64) && defined(WINDOWS)) || defined(ARM)
#        define NO_FRAME_POINTER 1
#    endif

static void
foo(void *retaddr)
{
    ptr_int_t myaddr1, myaddr2;
#    ifdef NO_FRAME_POINTER
    /* no frame pointer available w/ the compiler we're using */
#    else
    myaddr1 = get_retaddr_from_frameptr();
#        if VERBOSE
    print("my own return address is " PFX "\n", myaddr1);
#        endif
#    endif
    myaddr2 = (ptr_int_t)retaddr;
#    ifdef NO_FRAME_POINTER
    myaddr1 = myaddr2;
#    endif
    if (myaddr1 == myaddr2)
        print("return addresses match\n");
    else
        print("ERROR -- return addresses do not match\n");
#    if VERBOSE
    print("my own return address is " PFX "\n", myaddr2);
#    endif
}

int
main()
{
    ptr_int_t myaddr;
    /* make sure dynamorio can handle this non-call */
    myaddr = get_retaddr();
#    if VERBOSE
    print("my address is something like " PFX "\n", myaddr);
#    endif
    tailcall_with_retaddr((void *)foo);
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME get_retaddr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# if defined(X86)
        /* We don't bother w/ SEH64 directives, though we're an illegal leaf routine! */
        call     next_instr
    next_instr:
        pop      REG_XAX
        ret
# elif defined(ARM)
        push     {r12, lr}
        bl       next_instr
    next_instr:
        mov      r0, lr
        pop      {r12, pc}
# elif defined(AARCH64)
        str      x30, [sp, #-16]!
        bl       next_instr
    next_instr:
        mov      x0, x30
        ldr      x30, [sp], #16
        ret
# elif defined(RISCV64)
        /* TODO i#3544: Port tests to RISC-V 64 */
        ret
# else
#  error NYI
# endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME get_retaddr_from_frameptr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# if defined(X86)
        mov      REG_XAX, PTRSZ [REG_XBP + ARG_SZ]
        ret
# elif defined(ARM)
        ldr      r0, [r11, #4]
        bx       lr
# elif defined(AARCH64)
        ldr      x0, [x29, #8]
        ret
# elif defined(RISCV64)
        /* TODO i#3544: Port tests to RISC-V 64 */
        ret
# else
#  error NYI
# endif
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
