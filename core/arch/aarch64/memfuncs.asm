/* **********************************************************
 * Copyright (c) 2019-2020 Google, Inc. All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * memfuncs.asm: Contains our custom memcpy and memset routines.
 *
 * See the long comment at the top of x86/memfuncs.asm.
 */

#include "../asm_defines.asm"
START_FILE
#ifdef UNIX

/* Private memcpy.
 * FIXME i#1569: We should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      ldrb     w4, [ARG2], #1
        strb     w4, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memcpy)

/* Private memset.
 * FIXME i#1569: we should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      strb     w1, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
HIDDEN(__memcpy_chk)
WEAK(__memcpy_chk)
.set __memcpy_chk,GLOBAL_REF(memcpy)

.global __memset_chk
HIDDEN(__memset_chk)
WEAK(__memset_chk)
.set __memset_chk,GLOBAL_REF(memset)

#endif /* UNIX */

END_FILE
