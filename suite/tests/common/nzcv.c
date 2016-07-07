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

#include "tools.h"

#define VERBOSE 0

/* asm routine */
void test_nzcv_pos(uint pos);

const char *flags[] = {
    "N", "Z", "C", "V"
};

const uint nzcv_pos[] = {
    31, 30, 29, 28
};

#define NUM_FLAGS (sizeof(nzcv_pos) / sizeof(nzcv_pos[0]))

void
test_flag(uint nzcv, uint pos, bool set)
{
    const char *flag = "*";
    int i;
#if VERBOSE
    print("NZCV where %c should be %d: "PFX"\n", flag, set, nzcv);
#endif
    bool value = TEST(nzcv, 1U << pos);

    for (i = 0; i < NUM_FLAGS; i++) {
        if (nzcv_pos[i] == pos)
            flag = flags[i];
    }

    if (value != set)
        print("ERROR %d %s\n", set, flag);
    else
        print("OK %d %s\n", set, flag);
}

int
main()
{
    uint i;
    INIT();

    for (i = 0; i < NUM_FLAGS; i++) {
        test_nzcv_pos(nzcv_pos[i]);
    }
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
START_FILE

DECL_EXTERN(test_flag)

#define FUNCNAME test_nzcv_pos
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov     x19,    ARG1
        mov     x20,    x30
        CALLC1(GLOBAL_REF(set_flag), x19)
        mrs     x0,     nzcv
        mov     w1,     w19
        CALLC3(GLOBAL_REF(test_flag), x0, x1, 1)
        CALLC1(GLOBAL_REF(clear_flag), x19)
        mrs     x0,     nzcv
        mov     w1,     w19
        CALLC3(GLOBAL_REF(test_flag), x0, x1, 0)
        mov     x30,     x20
        ret
        END_FUNC(FUNCNAME)

    /* void set_flag(uint pos) */
#undef FUNCNAME
#define FUNCNAME set_flag
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov     w1,     #1
        lsl     w1,     w1, w0
        mrs     x0,     nzcv
        orr     w0,     w0, w1
        msr     nzcv,   x0
        ret
        END_FUNC(FUNCNAME)

    /* void clear_flag(uint pos) */
#undef FUNCNAME
#define FUNCNAME clear_flag
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov     w1,     #1
        lsl     w1,     w1, w0
        mvn     w1,     w1
        mrs     x0,     nzcv
        and     w0,     w0, w1
        msr     nzcv,   x0
        ret
        END_FUNC(FUNCNAME)

END_FILE
#endif
