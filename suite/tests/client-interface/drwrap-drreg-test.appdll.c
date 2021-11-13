/* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
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

/***************************************************************************/
#ifndef ASM_CODE_ONLY /* C code */

#    include "tools.h"

/* In asm code. */
int
reg_val_test(void);
int
multipath_test(int skip_clean_call);

int EXPORT
two_args(int x, int y)
{
    print("two_args %d %d\n", x, y);
    return (x + y);
}

void
run_tests(void)
{
    print("two_args returned %d\n", two_args(1, 2));
    print("reg_val_test returned %d\n", reg_val_test());
    print("multipath_test A returned %d\n", multipath_test(0));
    print("multipath_test B returned %d\n", multipath_test(1));
}

#    ifdef WINDOWS
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: run_tests(); break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}
#    else
int __attribute__((constructor)) so_init(void)
{
    run_tests();
    return 0;
}
#    endif

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE
#define FUNCNAME reg_val_test
        DECLARE_EXPORTED_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# ifdef X86
        push     REG_XBP  /* Needed only to maintain 16-byte alignment for x64. */
        mov      REG_XDX, HEX(4)
        /* The clean call writes to xcx, replacing this value. */
        mov      REG_XCX, HEX(42)
        /* The clean call is inserted after 3 nops. */
        nop
        nop
        nop
        pop      REG_XBP
        add      REG_XAX, REG_XCX
        add      REG_XAX, REG_XDX
        ret
# elif defined(AARCH64)
        mov      x1, #4
        /* The clean call writes to r2, replacing this value. */
        mov      x2, #42
        /* The clean call is inserted after 3 nops. */
        nop
        nop
        nop
        add      x0, x1, x2
        ret
# elif defined(ARM)
        mov      r1, #4
        /* The clean call writes to r2, replacing this value. */
        mov      r2, #42
        /* The clean call is inserted after 3 nops. */
        nop
        nop
        nop
        add      r0, r1, r2
        bx       lr
# endif
        END_FUNC(FUNCNAME)
#  undef FUNCNAME

#define FUNCNAME multipath_test
        DECLARE_EXPORTED_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# ifdef X86
        push     REG_XBP  /* Needed only to maintain 16-byte alignment for x64. */
        mov      REG_XCX, ARG1   /* Used to skip clean call. */
        mov      REG_XDX, HEX(4) /* Read in clean call. */
        mov      REG_XAX, HEX(ff00) /* To set aflags. */
        sahf                        /* Read in clean call. */
        mov      REG_XAX, HEX(42) /* Read in clean call. */
        /* The clean call is inserted after 4 nops. */
        nop
        nop
        nop
        nop
        pop      REG_XBP
        /* We want a read of xax so it's not dead and drreg can recover it.
         * But we want to keep aflags live too, so no ADD instruction.
         */
        lea      REG_XAX, [REG_XDX + REG_XAX]
        ret
# elif defined(AARCH64)
        mov      x0, ARG1 /* Used to skip clean call. */
        mov      x1, #4   /* Read in clean call. */
        mov      x2, #0x42
        /* Aflags has special x86 behavior; we do not test it here. */
        /* The clean call is inserted after 4 nops. */
        nop
        nop
        nop
        nop
        add      x0, x1, x2
        ret
# elif defined(ARM)
        mov      r0, ARG1 /* Used to skip clean call. */
        mov      r1, #4   /* Read in clean call. */
        mov      r2, #0x42
        /* Aflags has special x86 behavior; we do not test it here. */
        /* The clean call is inserted after 4 nops. */
        nop
        nop
        nop
        nop
        add      r0, r1, r2
        bx       lr
# endif
        END_FUNC(FUNCNAME)
#  undef FUNCNAME

END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
