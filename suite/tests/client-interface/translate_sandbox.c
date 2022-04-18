/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

int
sandbox();
int
usebx();

#    define MEMCHANGE_SIZE 1024

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        print("access violation exception\n");
        protect_mem(usebx, MEMCHANGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

int
main(int argc, char *argv[])
{
    INIT();
    int count = 0;

    assert(PAGE_SIZE <= 4096);

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);

    print("start of test, count = %d\n", count);
    protect_mem(sandbox, MEMCHANGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    protect_mem(usebx, MEMCHANGE_SIZE, ALLOW_READ);
    /* Sets DynamoRIO in sandboxing mode and generates an exception.
     * With a client storing translations, it is tested that
     * DynamoRIO manages to restore spilled ebx register.
     */
    count = sandbox();
    count += usebx();
    print("end of test, count = %d\n", count);
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#if defined(ASSEMBLE_WITH_GAS)
# define ALIGN_WITH_NOPS(bytes) .align (bytes)
# define FILL_WITH_NOPS(bytes)  .fill  (bytes), 1, 0x90
#elif defined(ASSEMBLE_WITH_MASM)
/* Cannot align to a value greater than the section alignment.  See .mytext
 * below.
 */
# define ALIGN_WITH_NOPS(bytes) ALIGN bytes
# define FILL_WITH_NOPS(bytes) \
    REPEAT (bytes) @N@\
        nop @N@\
        ENDM
#else
# define ALIGN_WITH_NOPS(bytes) ALIGN bytes
# define FILL_WITH_NOPS(bytes) TIMES bytes nop
#endif

#if defined(ASSEMBLE_WITH_MASM)
/* The .text section on Windows is only 16 byte aligned.  If we use plain
 * ALIGN_WITH_NOPS(), we get error A2189.  We can't re-declare .text, but we can
 * declare a new section with greater alignment.  The EXECUTE gets the right
 * protections and makes ALIGN_WITH_NOPS() do nop fill.
 */
_MYTEXT SEGMENT ALIGN(4096) READ EXECUTE SHARED ALIAS(".mytext")
#endif

/* int sandbox()
 *   Modifies self to enter sandbox.
 *   Then does another self modification in another page.
 *   Should return 2 (using ebx).
 */

#define FUNCNAME sandbox
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, HEX(1)
        lea      REG_XDX, SYMREF(sandbox_immediate_addr_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(sandbox_immediate_addr_plus_four:)
        mov      REG_XAX, HEX(1)
        mov      REG_XBX, HEX(2)
        lea      REG_XDI, SYMREF(usebx_immediate_addr_plus_four - 4)
        stosd    /* stos selfmod write in another page */
        mov      REG_XAX, REG_XBX
        ret
END_FUNC(FUNCNAME)
#undef FUNCNAME

ALIGN_WITH_NOPS(4096)
FILL_WITH_NOPS(4096)

/* int usebx()
 *   should return 1.
 */

#define FUNCNAME usebx
DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XDX, HEX(0)             /* mov_imm modified */
ADDRTAKEN_LABEL(usebx_immediate_addr_plus_four:)
        mov      REG_XAX, REG_XDX
        ret
END_FUNC(FUNCNAME)
#undef FUNCNAME

#ifdef ASSEMBLE_WITH_MASM
_MYTEXT ENDS
#endif

END_FILE
/* clang-format on */
#endif
