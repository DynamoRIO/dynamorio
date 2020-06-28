/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
 * ********************************************************** */

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

/*
 * memfuncs.asm: Contains our custom memcpy and memset routines.
 *
 * See the long comment at the top of x86/memfuncs.asm.
 */

#include "../asm_defines.asm"
START_FILE

#ifdef UNIX

/* Private memcpy.
 * FIXME i#1551: we should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        cmp      ARG3, #0
        mov      REG_R12/*scratch reg*/, ARG1
1:      beq      2f
        ldrb     REG_R3, [ARG2]
        strb     REG_R3, [ARG1]
        subs     ARG3, ARG3, #1
        add      ARG2, ARG2, #1
        add      ARG1, ARG1, #1
        b        1b
2:      mov      REG_R0, REG_R12
        bx       lr
        END_FUNC(memcpy)

/* Private memset.
 * FIXME i#1551: we should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        cmp      ARG3, #0
        mov      REG_R12/*scratch reg*/, ARG1
1:      beq      2f
        strb     ARG2, [ARG1]
        subs     ARG3, ARG3, #1
        add      ARG1, ARG1, #1
        b        1b
2:      mov      REG_R0, REG_R12
        bx       lr
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
.hidden __memcpy_chk
WEAK(__memcpy_chk)
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
WEAK(__memset_chk)
.set __memset_chk,memset

#endif /* UNIX */

END_FILE
