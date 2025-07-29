/* **********************************************************
 * Copyright (c) 2012-2025 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* **********************************************************
 * Portions Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
 * ***********************************************************/
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


/***************************************************************************
 * assembly utilities for which there are no intrinsics
 */

#include "cpp2asm_defines.h"

START_FILE

/* void get_stack_registers(reg_t *xsp OUT, reg_t *xbp OUT);
 *
 * Returns the current values of xsp and xbp.
 * There's no cl intrinsic to get xbp (gcc has one), and even
 * then would have to assume no FPO to get xsp.
 */
#define FUNCNAME get_stack_registers
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* we don't bother to be SEH64 compliant */
        mov      REG_XAX, ARG1
        mov      REG_XCX, ARG2
        mov      PTRSZ [REG_XCX], REG_XBP
        mov      REG_XCX, REG_XSP
        /* The caller may not use push to call us: it may allocate its
         * max frame up front.  However, all current uses are from functions
         * with large frames themselves, so our removals here are not a problem.
         */
#if defined(X64) && defined(WINDOWS)
        add      REG_XCX, 32 + ARG_SZ   /* remove frame space + retaddr */
#elif !defined(X64)
        add      REG_XCX, 3 * ARG_SZ    /* remove args + retaddr */

#endif
        mov      PTRSZ [REG_XAX], REG_XCX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


/* void get_unwind_registers(reg_t *xsp OUT, reg_t *xbp OUT, app_pc *xip OUT);
 *
 * Returns the caller's values of xsp and xbp and xip.
 */
#define FUNCNAME get_unwind_registers
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* we don't bother to be SEH64 compliant */
        mov      REG_XAX, ARG1
        mov      REG_XCX, ARG2
        mov      PTRSZ [REG_XCX], REG_XBP
        mov      REG_XCX, ARG3
        mov      REG_XDX, PTRSZ [REG_XSP]
        mov      PTRSZ [REG_XCX], REG_XDX
        /* The caller may not use push to call us: it may allocate its
         * max frame up front.  However, all current uses are from functions
         * with large frames themselves, so our removals here are not a problem.
         */
#if defined(X64) && defined(WINDOWS)
        add      REG_XCX, 32 + ARG_SZ   /* remove frame space + retaddr */
#elif !defined(X64)
        add      REG_XCX, 3 * ARG_SZ    /* remove args + retaddr */

#endif
        mov      PTRSZ [REG_XAX], REG_XCX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


#ifdef LINUX
/* Straight from DynamoRIO as a faster fix than continually tweaking the
 * C routines raw_syscall_N_args() since i#199 has complex issues and is
 * non-trivial to put in place.
 * XXX: if we remove this and use an export from DR instead, we can remove
 * the DR license statement from the top of this file.
 */
/* to avoid libc wrappers we roll our own syscall here
 * hardcoded to use int 0x80 for 32-bit -- XXX: use something like do_syscall
 * and syscall for 64-bit.
 * signature: raw_syscall(sysnum, num_args, arg1, arg2, ...)
 */
        DECLARE_FUNC(raw_syscall)
GLOBAL_LABEL(raw_syscall:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX
# ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax to use ARG1 (or movzx) */
        cmp      REG_XBX, 0
        je       syscall_ready
        mov      ARG1, ARG3
        cmp      REG_XBX, 1
        je       syscall_ready
        mov      ARG2, ARG4
        cmp      REG_XBX, 2
        je       syscall_ready
        mov      ARG3, ARG5
        cmp      REG_XBX, 3
        je       syscall_ready
        mov      ARG4, ARG6
        cmp      REG_XBX, 4
        je       syscall_ready
        mov      ARG5, [2*ARG_SZ + REG_XSP] /* arg7: above xbx and retaddr */
        cmp      REG_XBX, 5
        je       syscall_ready
        mov      ARG6, [3*ARG_SZ + REG_XSP] /* arg8: above arg7, xbx, retaddr */
syscall_ready:
        mov      r10, rcx
        syscall
# else
        push     REG_XBP
        push     REG_XSI
        push     REG_XDI
        /* add 16 to skip the 4 pushes
         * XXX: rather than this dispatch, could have separate routines
         * for each #args, or could just blindly read upward on the stack.
         * for dispatch, if assume size of mov instr can do single ind jmp */
        mov      ecx, [16+ 8 + esp] /* num_args */
        cmp      ecx, 0
        je       syscall_0args
        cmp      ecx, 1
        je       syscall_1args
        cmp      ecx, 2
        je       syscall_2args
        cmp      ecx, 3
        je       syscall_3args
        cmp      ecx, 4
        je       syscall_4args
        cmp      ecx, 5
        je       syscall_5args
        mov      ebp, [16+32 + esp] /* arg6 */
syscall_5args:
        mov      edi, [16+28 + esp] /* arg5 */
syscall_4args:
        mov      esi, [16+24 + esp] /* arg4 */
syscall_3args:
        mov      edx, [16+20 + esp] /* arg3 */
syscall_2args:
        mov      ecx, [16+16 + esp] /* arg2 */
syscall_1args:
        mov      ebx, [16+12 + esp] /* arg1 */
syscall_0args:
        mov      eax, [16+ 4 + esp] /* sysnum */
        /* PR 254280: we assume int$80 is ok even for LOL64 */
        int      HEX(80)
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
# endif /* X64 */
        pop      REG_XBX
        /* return val is in eax for us */
        ret
        END_FUNC(raw_syscall)
#endif /* LINUX */


/* void zero_pointers_on_stack(size_t count);
 * assuming count must be multiple of ARG_SZ
 *
 * Scans the memory in [xsp - count - ARG_SZ, xsp - ARG_SZ) and set it to 0
 * if the content looks like a pointer (> 0x10000).
 * Meant to be used to zero the stack, which is dangerous to do
 * from C as we can easily clobber our own local variables.
 */
#define FUNCNAME zero_pointers_on_stack
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* we don't bother to be SEH64 compliant */
        mov      REG_XCX, ARG1
        END_PROLOG
        neg      REG_XCX
        /* i#1284: we used "rep stosd" before, which is slower than
         * the check-before-write optimization we have below.
         * The tight loop with fewer stores has better performance.
         */
check_stack:
        /* we assume no pointer pointing to address below 0x10000 */
        cmp      PTRSZ [REG_XSP+REG_XCX-ARG_SZ], HEX(10000)
        jbe      update_xcx /* skip store if not a pointer */
        mov      PTRSZ [REG_XSP+REG_XCX-ARG_SZ], 0
update_xcx:
        add      REG_XCX, ARG_SZ
        jne      check_stack
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


END_FILE
