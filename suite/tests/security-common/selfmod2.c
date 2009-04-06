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
#include "tools.h"

#ifdef USE_DYNAMO
# include "dynamorio.h"
#endif

/* Can't just copy from (ptr_int_t)bar, since that's often a separate entry point
 * and not the start of bar's code, so we just have a hunk of code
 * sitting here:
 *
 *  int
 *  bar(int value)
 *  {
 *      return value*2;
 *  }
 * =>
 *  00401030: 55                 push        ebp
 *  00401031: 8B EC              mov         ebp,esp
 *  00401033: 8B 45 08           mov         eax,dword ptr [ebp+8]
 *  00401036: D1 E0              shl         eax,1
 *  00401038: 5D                 pop         ebp
 *  00401039: C3                 ret
 *
 */
unsigned char bar_code[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0xd1, 0xe0, 0x5d, 0xc3
};
unsigned int bar_code_size = sizeof(bar_code);
int foo(int value);

int
main()
{
    INIT();

#ifdef USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    protect_mem(foo, PAGE_SIZE, ALLOW_EXEC|ALLOW_WRITE|ALLOW_READ);
    
    print("foo returned %d\n", foo(10));
    print("foo returned %d\n", foo(10));

#ifdef USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif

    return 0;
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
START_FILE

DECL_EXTERN(bar_code)
DECL_EXTERN(bar_code_size)

/* int foo(int value)
 *   copies bar over the front of itself, so future invocations will
 *   run bar's code
 */
#define FUNCNAME foo
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, ARG1
        /* save callee-saved regs */
        push REG_XSI
        push REG_XDI
        /* set up for copy */
        mov  REG_XSI, offset bar_code
        mov  REG_XDI, offset foo
        mov  ecx, DWORD SYMREF(bar_code_size)
        cld
        rep movsb
        /* restore callee-saved regs */
        pop REG_XDI
        pop REG_XSI
        ret
        END_FUNC(FUNCNAME)

END_FILE
#endif
