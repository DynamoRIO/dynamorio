/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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
/* Tests preservation of eflags across indirect branches
 */

#    include "tools.h"

#    define VERBOSE 0

/* asm routine */
void
test_eflags_pos(uint pos);

/*
 * eflags we care about:
 *  11 10  9  8  7  6  5  4  3  2  1  0
 *  OF DF       SF ZF    AF    PF    CF
 */
const char *flags[] = { "CF", "", "PF", "", "AF", "", "ZF", "SF", "", "", "DF", "OF" };

const uint eflag_pos[] = { 0, 2, 4, 6, 7, 10, 11 };
#    define NUM_FLAGS (sizeof(eflag_pos) / sizeof(eflag_pos[0]))

void
test_flag(uint eflags, uint pos, bool set)
{
#    if VERBOSE
    print("eflags where %d should be %d: " PFX "\n", pos, set, eflags);
#    endif
    if ((set && ((eflags & (1 << pos)) == 0)) || (!set && ((eflags & (1 << pos)) != 0)))
        print("ERROR %d %s\n", set, flags[pos]);
    else
        print("OK %d %s\n", set, flags[pos]);
}

int
main()
{
    uint i;
    INIT();

    for (i = 0; i < NUM_FLAGS; i++) {
        test_eflags_pos(eflag_pos[i]);
    }
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

DECL_EXTERN(test_flag)

#define FUNCNAME test_eflags_pos
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* We don't bother w/ SEH64 directives, though we're an illegal leaf routine! */
        mov      REG_XCX, ARG1
        push     REG_XCX /* save */
        CALLC1(GLOBAL_REF(set_flag), REG_XCX)
        mov      REG_XCX, PTRSZ [REG_XSP]
        PUSHF
        pop      REG_XAX
        /* having DF set messes up printing for x64 */
        push     0
        POPF
        CALLC3(GLOBAL_REF(test_flag), REG_XAX, REG_XCX, 1)

        mov      REG_XCX, PTRSZ [REG_XSP]
        CALLC1(GLOBAL_REF(clear_flag), REG_XCX)
        mov      REG_XCX, PTRSZ [REG_XSP]
        PUSHF
        pop      REG_XAX
        /* having DF set messes up printing for x64 */
        push     0
        POPF
        CALLC3(GLOBAL_REF(test_flag), REG_XAX, REG_XCX, 0)

        pop      REG_XCX /* clean up */
        ret
        END_FUNC(FUNCNAME)

    /* void set_flag(uint pos) */
#undef FUNCNAME
#define FUNCNAME set_flag
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* We don't bother w/ SEH64 directives, though we're an illegal leaf routine! */
        PUSHF
        pop      REG_XAX
        mov      REG_XCX, ARG1
        mov      REG_XDX, 1
        shl      REG_XDX, cl
        or       REG_XAX, REG_XDX
        push     REG_XAX
        POPF
        ret
        END_FUNC(FUNCNAME)

    /* void clear_flag(uint pos) */
#undef FUNCNAME
#define FUNCNAME clear_flag
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* We don't bother w/ SEH64 directives, though we're an illegal leaf routine! */
        PUSHF
        pop      REG_XAX
        mov      REG_XCX, ARG1
        mov      REG_XDX, 1
        shl      REG_XDX, cl
        not      REG_XDX
        and      REG_XAX, REG_XDX
        push     REG_XAX
        POPF
        ret
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
