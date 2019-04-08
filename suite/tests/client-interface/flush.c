/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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
#    include <stdio.h>

void
marker(void); /* in asm code */

int test = 1;

int
main()
{
    int i = 0;
    int count = 0;
    do {
        if (test) {
            /* I can't think of a good way to do this -- I want to
             * target a specific BB in the client, so I'll use a
             * couple of nops as markers.
             *
             * Given the value of 'test', this side of the conditional
             * should become part of a trace.
             */
            /* FIXME - pick unusual nops that instr_is_nop() recognizes.
             * 2 regular nops in a row is hit on Linux frequently so going with
             * xchg ebp, ebp since I haven't seen that one used before (mov edi, edi
             * or xchg eax, eax is more typical for 2 bytes). */
            marker();
            count++;
        } else {
            count--;
        }

        i++;
    } while (i < 402);

    fprintf(stderr, "count = %d\n", count);
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE
        DECLARE_FUNC(marker)
GLOBAL_LABEL(marker:)
        nop
        xchg REG_XBP, REG_XBP
        ret
        END_FUNC(marker)
END_FILE
/* clang-format on */
#endif
