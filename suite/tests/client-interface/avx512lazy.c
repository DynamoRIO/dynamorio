/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"

NOINLINE void
before_marker();
NOINLINE void
after_marker();
NOINLINE void
avx512_instr();

void
run_avx512()
{
#    ifndef __AVX512F__
#        error "Build error, should only be added with AVX-512 support."
#    endif
    before_marker();
    /* DynamoRIO will detect AVX-512 during decode. We're making sure that the AVX-512
     * instruction is not in the same basic block as the markers.
     */
    avx512_instr();
    after_marker();
    print("Ok\n");
}

int
main()
{
    run_avx512();
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define FUNCNAME before_marker
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        mov      REG_XAX, 0x12345678
        mov      REG_XAX, 0x12345678

        add      REG_XSP, FRAME_PADDING
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME after_marker
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        mov      REG_XAX, 0x12345678
        mov      REG_XAX, 0x12345678

        add      REG_XSP, FRAME_PADDING
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME avx512_instr
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        vmovups  REG_ZMM0, REG_ZMM1

        add      REG_XSP, FRAME_PADDING
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
