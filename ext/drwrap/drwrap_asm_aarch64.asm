/* **********************************************************
 * Copyright (c) 2020 Google, Inc. All rights reserved.
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

/***************************************************************************
 * assembly utilities for which there are no intrinsics
 */

#include "cpp2asm_defines.h"

START_FILE

        DECLARE_FUNC(replace_native_xfer)
GLOBAL_LABEL(replace_native_xfer:)
        stp     x0, x30, [sp, #-16]!
        bl      GLOBAL_REF(replace_native_xfer_app_retaddr)
        /* Store return address to x30 position. */
        str     x0, [sp, #8]
        bl      GLOBAL_REF(replace_native_xfer_target)
        mov     x1, x0
        ldp     x0, x30, [sp], #16
        br      x1
        /* never reached */
        END_FUNC(replace_native_xfer)

DECLARE_GLOBAL(replace_native_ret_imms)
DECLARE_GLOBAL(replace_native_ret_imms_end)
        DECLARE_FUNC(replace_native_rets)
GLOBAL_LABEL(replace_native_rets:)
        ret
ADDRTAKEN_LABEL(replace_native_ret_imms:)
ADDRTAKEN_LABEL(replace_native_ret_imms_end:)
        nop
        END_FUNC(replace_native_rets)

        DECLARE_FUNC(get_cur_xsp)
GLOBAL_LABEL(get_cur_xsp:)
        mov      x0, sp
        ret
        END_FUNC(get_cur_xsp)

/* We just need a sentinel block that does not cause DR to complain about
 * non-executable code or illegal instrutions, for DRWRAP_REPLACE_RETADDR.
 */
        DECLARE_FUNC(replace_retaddr_sentinel)
GLOBAL_LABEL(replace_retaddr_sentinel:)
        ret
        END_FUNC(replace_retaddr_sentinel)

END_FILE
