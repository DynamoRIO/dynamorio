/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2022 Arm Limited   All rights reserved.
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

/* Executes two indexed memory instructions, one using X28 for index register,
 * the other W28. Intention is for DR to call drutil_insert_get_mem_addr() in
 * order to test the 'if (index == stolen)' clause in
 * drutil_insert_get_mem_addr_arm() in the case of W28.
 */

#ifndef ASM_CODE_ONLY /* C code */

#    include "tools.h"

/* In asm code. */
void
indexed_mem_test(int *val);

int
main(int argc, char *argv[])
{
    int value = 41;
    indexed_mem_test(&value);
    if (value != 42)
        print("indexed_mem_test() failed with %d, expected 42.\n", value);
    else
        print("indexed_mem_test() passed.\n");

    print("Tested the use of stolen register as memory index register.\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE
#define FUNCNAME indexed_mem_test
        DECLARE_EXPORTED_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)

        stp      x0, x1, [sp, #-16]!

        /* Load passed in value using index register X28, then increment. */
        mov      x28, #0
        ldr      x1, [x0, x28, lsl #0]
        add      x1, x1, #1

        /* Store incremented value using index register W28. */
        mov      w28, #0
        str      x1, [x0, w28, uxtw #0]

        ldp      x0, x1, [sp], #16
        ret

        END_FUNC(FUNCNAME)
#  undef FUNCNAME

END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
