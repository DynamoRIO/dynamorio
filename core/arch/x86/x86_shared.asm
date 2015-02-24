/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * x86_shared.asm - x86 specific assembly code for sharing.
 * See comments in x86.asm on the format here.
 */

#include "../asm_defines.asm"
START_FILE

DECL_EXTERN(unexpected_return)

/* we share dynamorio_syscall w/ preload */
#ifdef UNIX
/* to avoid libc wrappers we roll our own syscall here
 * hardcoded to use int 0x80 for 32-bit -- FIXME: use something like do_syscall
 * and syscall for 64-bit.
 * signature: dynamorio_syscall(sysnum, num_args, arg1, arg2, ...)
 * For Linux, the argument max is 6.
 * For MacOS, the argument max is 6 for x64 and 7 for x86.
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX /* stack now aligned for x64 */
# ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax for ARG1 (or movzx) */
#  ifdef MACOS
        /* for now we assume a BSD syscall */
        or       rax, 0x2000000
#  endif
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
#  ifdef MACOS
        cmp      ecx, 6
        je       syscall_6args
#   ifdef INTERNAL
        cmp      ecx, 7
        jg       GLOBAL_REF(unexpected_return)
#   endif
        mov      eax, [16+36 + esp] /* arg7 */
syscall_6args:
#  elif defined(INTERNAL)
        cmp      ecx, 6
        jg       GLOBAL_REF(unexpected_return)
#  endif
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
#  ifdef MACOS
        push     eax /* 7th arg, if any */
        /* Arg size is encoded in upper bits.
         * XXX: or is that only for sysenter gateway?
         * We assume this is size, not count, and so for our "7 arg"
         * call that's really 6 with one 64-bit we leave it.
         */
        mov      eax, [20+ 8 + esp] /* num_args */
        shl      eax, 18 /* <<16 but also *4 for size */
        or       eax, [20+ 4 + esp] /* sysnum */
        /* args are on stack, w/ an extra slot (retaddr of syscall wrapper) */
        push     ebp
        push     edi
        push     esi
        push     edx
        push     ecx
        push     ebx /* aligned to 16 after this push */
        push     0 /* extra slot (app retaddr) */
        /* It simplifies our syscall calling to have a single dynamorio_syscall()
         * signature that returns int64 -- but most syscalls just return a 32-bit
         * value and the kernel does not clear edx.  Thus we need to do so, which
         * should be safe since edx is caller-saved.  (Note that we do not risk
         * doing this for app syscalls: only those called by DR.)
         */
        mov      edx, 0
#  else
        mov      eax, [16+ 4 + esp] /* sysnum */
#  endif
        /* PR 254280: we assume int$80 is ok even for LOL64, maybe slow is all.
         * For Mac, it's possible to do sysenter here as we can store the retaddr
         * in edx ourselves (in fact see r2514 dynamorio_syscall_sysenter for an
         * implementation, now removed), but we still need int for certain syscalls
         * (returning 64-bit values, e.g.) so we go w/ int always and assume our
         * syscall perf doesn't matter much (should be rare).
         */
        int      HEX(80)
#  ifdef MACOS
        lea      esp, [8*ARG_SZ + esp] /* must not change flags */
#  endif
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
# endif /* X64 */
        pop      REG_XBX
        /* return val is in eax for us */
        /* for MacOS, it can also include edx, so be sure not to clobber that! */
# ifdef MACOS
        /* convert to -errno */
        jae      syscall_success
        neg      eax
syscall_success:
# endif
        ret
        END_FUNC(dynamorio_syscall)

# ifdef MACOS
/* Mach dep syscall invocation.
 * Signature: dynamorio_mach_dep_syscall(sysnum, num_args, arg1, arg2, ...)
 * Only supports up to 4 args.
 */
        DECLARE_FUNC(dynamorio_mach_dep_syscall)
GLOBAL_LABEL(dynamorio_mach_dep_syscall:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX
#  ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax to use ARG1 (or movzx) */
        cmp      REG_XBX, 0
        je       mach_dep_syscall_ready
        mov      ARG1, ARG3
        cmp      REG_XBX, 1
        je       mach_dep_syscall_ready
        mov      ARG2, ARG4
        cmp      REG_XBX, 2
        je       mach_dep_syscall_ready
        mov      ARG3, ARG5
        cmp      REG_XBX, 3
        je       mach_dep_syscall_ready
        mov      ARG4, ARG6
#  else
        push     REG_XBP
        push     REG_XSI
        push     REG_XDI
        /* add 16 to skip the 4 pushes */
        mov      ecx, [16+ 8 + esp] /* num_args */
        cmp      ecx, 0
        je       mach_dep_syscall_0args
        cmp      ecx, 1
        je       mach_dep_syscall_1args
        cmp      ecx, 2
        je       mach_dep_syscall_2args
        cmp      ecx, 3
        je       mach_dep_syscall_3args
        mov      esi, [16+24 + esp] /* arg4 */
mach_dep_syscall_3args:
        mov      edx, [16+20 + esp] /* arg3 */
mach_dep_syscall_2args:
        mov      ecx, [16+16 + esp] /* arg2 */
mach_dep_syscall_1args:
        mov      ebx, [16+12 + esp] /* arg1 */
mach_dep_syscall_0args:
        mov      eax, [16+ 4 + esp] /* sysnum */
        lea      REG_XSP, [-2*ARG_SZ + REG_XSP] /* maintain align-16: retaddr-5th below */
        /* args are on stack, w/ an extra slot (retaddr of syscall wrapper) */
        push     esi
        push     edx
        push     ecx
        push     ebx
        push     0 /* extra slot */
        /* clear the top half so we can always consider the result 64-bit */
        mov      edx, 0
#  endif
        /* mach dep syscalls use interrupt 0x82 */
        int      HEX(82)
#  ifndef X64
        lea      esp, [7*ARG_SZ + esp] /* must not change flags */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
#  endif
        pop      REG_XBX
        /* return val is in eax for us */
        /* for MacOS, it can also include edx, so be sure not to clobber that! */
        /* convert to -errno */
        jae      mach_dep_syscall_success
        neg      eax
mach_dep_syscall_success:
        ret
        END_FUNC(dynamorio_mach_dep_syscall)


/* Mach syscall invocation.
 * Signature: ptr_int_t dynamorio_mach_syscall(sysnum, num_args, arg1, arg2, ...)
 * Only supports up to 4 args.
 * Does not support returning a 64-bit value in 32-bit mode.
 */
        DECLARE_FUNC(dynamorio_mach_syscall)
GLOBAL_LABEL(dynamorio_mach_syscall:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX
#  ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax to use ARG1 (or movzx) */
        cmp      REG_XBX, 0
        je       dynamorio_mach_syscall_ready
        mov      ARG1, ARG3
        cmp      REG_XBX, 1
        je       dynamorio_mach_syscall_ready
        mov      ARG2, ARG4
        cmp      REG_XBX, 2
        je       dynamorio_mach_syscall_ready
        mov      ARG3, ARG5
        cmp      REG_XBX, 3
        je       dynamorio_mach_syscall_ready
        mov      ARG4, ARG6
#  else
        push     REG_XBP
        push     REG_XSI
        push     REG_XDI
        /* add 16 to skip the 4 pushes */
        mov      ecx, [16+ 8 + esp] /* num_args */
        cmp      ecx, 0
        je       dynamorio_mach_syscall_0args
        cmp      ecx, 1
        je       dynamorio_mach_syscall_1args
        cmp      ecx, 2
        je       dynamorio_mach_syscall_2args
        cmp      ecx, 3
        je       dynamorio_mach_syscall_3args
        mov      esi, [16+24 + esp] /* arg4 */
dynamorio_mach_syscall_3args:
        mov      edx, [16+20 + esp] /* arg3 */
dynamorio_mach_syscall_2args:
        mov      ecx, [16+16 + esp] /* arg2 */
dynamorio_mach_syscall_1args:
        mov      ebx, [16+12 + esp] /* arg1 */
dynamorio_mach_syscall_0args:
        mov      eax, [16+ 4 + esp] /* sysnum */
#  ifdef X64
        or       eax, SYSCALL_NUM_MARKER_MACH
#  else
        /* The sysnum is passed as a negative number */
        neg      eax
#  endif
        /* args are on stack, w/ an extra slot (retaddr of syscall wrapper) */
        lea      REG_XSP, [-2*ARG_SZ + REG_XSP] /* maintain align-16: retaddr-5th below */
        /* args are on stack, w/ an extra slot (retaddr of syscall wrapper) */
        push     esi
        push     edx
        push     ecx
        push     ebx
        push     0 /* extra slot */
#  endif
        /* If we use ADDRTAKEN_LABEL and GLOBAL_REF we get text relocation
         * complaints so we instead do this hack:
         */
        call     dynamorio_mach_syscall_next
dynamorio_mach_syscall_next:
        pop      REG_XDX
        lea      REG_XDX, [1/*pop*/ + 3/*lea*/ + 2/*sysenter*/ + 2/*mov*/ + REG_XDX]
        mov      REG_XCX, REG_XSP
        /* We have to use sysenter for a Mach syscall, else we get SIGSYS.
         * This implies that we can't return 64-bit in 32-bit mode.
         */
        sysenter
#  ifndef X64
        lea      esp, [7*ARG_SZ + esp] /* must not change flags */
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
#  endif
        pop      REG_XBX
        /* Return val is in eax for us.
         * Note that unlike BSD and Machdep syscalls, Mach syscalls do not
         * use flags to indicate success.
         */
        ret
        END_FUNC(dynamorio_mach_syscall)

# endif /* MACOS */
#endif /* UNIX */

END_FILE
