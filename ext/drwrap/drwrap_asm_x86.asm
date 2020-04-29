/* **********************************************************
 * Copyright (c) 2012-2020 Google, Inc.  All rights reserved.
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
        /* We must adjust the stack for stdcall (i#778), push the app retaddr,
         * and finally jmp to client ibl xfer.  However, we have to do all
         * that w/o disturbing the return value from the replacement function.
         * We thus only have xcx as a reg we can modify, and we have to preserve
         * xax and xdx around every call.  Our only simple OUT value is via
         * the return address, so we make 3 separate calls.
         * An alternative would be to look for our gencode bb and add meta
         * instrs to do this, but we can't pass the retaddr there easily
         * (ibl xfer clobbers scratch slots), and if we're going to put it
         * on the stack here anyway, we may as well do the adjust here.
         *
         * XXX: what about xmm0 return reg?
         */
        push     0 /* maintain 16-byte alignment for call */
        push     REG_XAX
        push     REG_XDX
        CALLC0(GLOBAL_REF(replace_native_xfer_stack_adjust))
        mov      ecx, eax
        pop      REG_XDX
        pop      REG_XAX
        lea      REG_XSP, [ ARG_SZ + REG_XSP ]
        /* DrMem i#1217: zero out these slots as they can contain retaddrs, which
         * messes up high-performance callstack stack scans.  This is beyond TOS,
         * but these are writes so it's signal-safe.
         */
        mov      PTRSZ [-2 * ARG_SZ + REG_XSP], 0
        mov      PTRSZ [-1 * ARG_SZ + REG_XSP], 0

        /* ok, now we have stack back to what it was.  undo the stdcall stack adjust. */
        sub      REG_XSP, REG_XCX
        /* make space for app retaddr */
        push     REG_XAX

        push     REG_XAX
        push     REG_XDX

        /* We have to align the stack for our calls: o/w dyld lazy resolution
         * crashes there.  Current alignment could be anything so we save a ptr
         * and mask off esp.
         */
        push     REG_XBX
        mov      REG_XBX, REG_XSP
        and      REG_XSP, -16 /* align to 16 */

        /* put app retaddr into the slot we made above the 2 pushes */
        CALLC0(GLOBAL_REF(replace_native_xfer_app_retaddr))
        mov      PTRSZ [3 * ARG_SZ + REG_XBX], REG_XAX

        /* now get our target */
        CALLC0(GLOBAL_REF(replace_native_xfer_target))
        mov      REG_XCX, REG_XAX

        mov      REG_XSP, REG_XBX
        pop      REG_XBX

        pop      REG_XDX
        pop      REG_XAX
        /* DrMem i#1217: zero out these slots as they can contain retaddrs, which
         * messes up high-performance callstack stack scans.  This is beyond TOS,
         * but these are writes so it's signal-safe.
         */
        mov      PTRSZ [-2 * ARG_SZ + REG_XSP], 0
        mov      PTRSZ [-1 * ARG_SZ + REG_XSP], 0
        jmp      REG_XCX
        /* never reached */
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* i#778: We want the app code stream to have its stack operations all
 * line up.  The stdcall args + retaddr teardown executed natively (i#900),
 * so we re-extend the stack in C code and then target a ret_imm instr
 * from this series.  We only support pointer-aligned.  We do support
 * for x64 although it's quite rare to need it there.
 */
DECLARE_GLOBAL(replace_native_ret_imms)
DECLARE_GLOBAL(replace_native_ret_imms_end)
#define FUNCNAME replace_native_rets
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ret
ADDRTAKEN_LABEL(replace_native_ret_imms:)
        ret      1 * ARG_SZ
        ret      2 * ARG_SZ
        ret      3 * ARG_SZ
        ret      4 * ARG_SZ
        ret      5 * ARG_SZ
        ret      6 * ARG_SZ
        ret      7 * ARG_SZ
        ret      8 * ARG_SZ
        ret      9 * ARG_SZ
        ret      10 * ARG_SZ
        ret      11 * ARG_SZ
        ret      12 * ARG_SZ
        ret      13 * ARG_SZ
        ret      14 * ARG_SZ
        ret      15 * ARG_SZ
        ret      16 * ARG_SZ
        ret      17 * ARG_SZ
        ret      18 * ARG_SZ
        ret      19 * ARG_SZ
        ret      20 * ARG_SZ
        ret      21 * ARG_SZ
        ret      22 * ARG_SZ
        ret      23 * ARG_SZ
        ret      24 * ARG_SZ
        ret      25 * ARG_SZ
        ret      26 * ARG_SZ
        ret      27 * ARG_SZ
        ret      28 * ARG_SZ
        ret      29 * ARG_SZ
        ret      30 * ARG_SZ
        ret      31 * ARG_SZ
        ret      32 * ARG_SZ
        ret      33 * ARG_SZ
        ret      34 * ARG_SZ
        ret      35 * ARG_SZ
        ret      36 * ARG_SZ
        ret      37 * ARG_SZ
        ret      38 * ARG_SZ
        ret      39 * ARG_SZ
        ret      40 * ARG_SZ
ADDRTAKEN_LABEL(replace_native_ret_imms_end:)
        nop
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* We just need a sentinel block that does not cause DR to complain about
 * non-executable code or illegal instrutions, for DRWRAP_REPLACE_RETADDR.
 */
#define FUNCNAME replace_retaddr_sentinel
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ret
        END_FUNC(FUNCNAME)


END_FILE
