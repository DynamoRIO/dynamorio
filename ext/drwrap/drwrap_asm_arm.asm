/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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

/***************************************************************************
 * assembly utilities for which there are no intrinsics
 */

#include "cpp2asm_defines.h"

START_FILE


DECL_EXTERN(replace_native_xfer_stack_adjust)
DECL_EXTERN(replace_native_xfer_app_retaddr)
DECL_EXTERN(replace_native_xfer_target)

/* void replace_native_xfer(void)
 *
 * Replacement function returns here instead of to app.
 * This routine is in assembly b/c it needs to jump to the client ibl
 * xfer routine w/ the same stack value as when it entered.
 */
#define FUNCNAME replace_native_xfer
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        push     {r0} /* save return value from replacement function */
        /* put app retaddr into the slot we made above the 2 pushes */
        blx      GLOBAL_REF(replace_native_xfer_app_retaddr)
        push     {r0}

        /* now get our target */
        blx      GLOBAL_REF(replace_native_xfer_target)
        mov      r3, r0

        /* restore regs and go to ibl */
        pop      {lr}
        pop      {r0}
        bx       r3
        /* never reached */
        bx       lr
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* See i#778 notes in the x86 asm: we want an app return instr. */
DECLARE_GLOBAL(replace_native_ret_imms)
DECLARE_GLOBAL(replace_native_ret_imms_end)
#define FUNCNAME replace_native_rets
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        bx       lr
ADDRTAKEN_LABEL(replace_native_ret_imms:)
ADDRTAKEN_LABEL(replace_native_ret_imms_end:)
        nop
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* See i#778 notes in the x86 asm: we want an app return instr. */
#define FUNCNAME get_cur_xsp
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      r0, sp
        bx       lr
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* We just need a sentinel block that does not cause DR to complain about
 * non-executable code or illegal instrutions, for DRWRAP_REPLACE_RETADDR.
 */
#define FUNCNAME replace_retaddr_sentinel
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        bx       lr
        END_FUNC(FUNCNAME)

END_FILE
