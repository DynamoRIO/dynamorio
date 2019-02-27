/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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
#include "x86_asm_defines.asm" /* PUSHGPR, POPGPR, etc. */
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

/* void dr_fpu_exception_init(void)
 * sets the exception mask flags for both regular float and xmm packed float
 */
#define FUNCNAME dr_fpu_exception_init
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        fninit
        push     HEX(1f80)
        ldmxcsr  DWORD [REG_XSP]
        pop      REG_XAX
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void get_mmx_val(OUT uint64 *val, uint index)
 * Returns the value of mmx register #index in val.
 */
#define FUNCNAME get_mmx_val
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        mov      REG_XCX, ARG2
        END_PROLOG
        cmp      ecx, 0
        je       get_mmx_0
        cmp      ecx, 1
        je       get_mmx_1
        cmp      ecx, 2
        je       get_mmx_2
        cmp      ecx, 3
        je       get_mmx_3
        cmp      ecx, 4
        je       get_mmx_4
        cmp      ecx, 5
        je       get_mmx_5
        cmp      ecx, 6
        je       get_mmx_6
        movq     QWORD [REG_XAX], mm7
        jmp get_mmx_done
get_mmx_6:
        movq     QWORD [REG_XAX], mm6
        jmp get_mmx_done
get_mmx_5:
        movq     QWORD [REG_XAX], mm5
        jmp get_mmx_done
get_mmx_4:
        movq     QWORD [REG_XAX], mm4
        jmp get_mmx_done
get_mmx_3:
        movq     QWORD [REG_XAX], mm3
        jmp get_mmx_done
get_mmx_2:
        movq     QWORD [REG_XAX], mm2
        jmp get_mmx_done
get_mmx_1:
        movq     QWORD [REG_XAX], mm1
        jmp get_mmx_done
get_mmx_0:
        movq     QWORD [REG_XAX], mm0
get_mmx_done:
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#ifdef WINDOWS /* on linux we use inline asm versions */

/* byte *get_frame_ptr(void)
 * returns the value of ebp
 */
        DECLARE_FUNC(get_frame_ptr)
GLOBAL_LABEL(get_frame_ptr:)
        mov      REG_XAX, REG_XBP
        ret
        END_FUNC(get_frame_ptr)

/* byte *get_stack_ptr(void)
 * returns the value of xsp before the call
 */
        DECLARE_FUNC(get_stack_ptr)
GLOBAL_LABEL(get_stack_ptr:)
        mov      REG_XAX, REG_XSP
        add      REG_XAX, ARG_SZ /* remove return address space */
        ret
        END_FUNC(get_stack_ptr)

#endif /* WINDOWS */


/***************************************************************************/
#if defined(WINDOWS) && !defined(X64)

/* Routines to switch to 64-bit mode from 32-bit WOW64, make a 64-bit
 * call, and then return to 32-bit mode.
 */

/*
 * int switch_modes_and_load(void *ntdll64_LdrLoadDll,
 *                           UNICODE_STRING_64 *lib,
 *                           HANDLE *result)
 * XXX i#1633: this routine does not yet support ntdll64 > 4GB
 */
# define FUNCNAME switch_modes_and_load
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* get args before we change esp */
        mov      eax, ARG1
        mov      ecx, ARG2
        mov      edx, ARG3
        /* save callee-saved registers */
        push     ebx
        /* far jmp to next instr w/ 64-bit switch: jmp 0033:<sml_transfer_to_64> */
        RAW(ea)
        DD offset sml_transfer_to_64
        DB CS64_SELECTOR
        RAW(00)
sml_transfer_to_64:
    /* Below here is executed in 64-bit mode, but with guarantees that
     * no address is above 4GB, as this is a WOW64 process.
     */
       /* Call LdrLoadDll to load 64-bit lib:
        *   LdrLoadDll(IN PWSTR DllPath OPTIONAL,
        *              IN PULONG DllCharacteristics OPTIONAL,
        *              IN PUNICODE_STRING DllName,
        *              OUT PVOID *DllHandle));
        */
        RAW(4c) RAW(8b) RAW(ca)  /* mov r9, rdx : 4th arg: result */
        RAW(4c) RAW(8b) RAW(c1)  /* mov r8, rcx : 3rd arg: lib */
        push     0               /* slot for &DllCharacteristics */
        lea      edx, dword ptr [esp] /* 2nd arg: &DllCharacteristics */
        xor      ecx, ecx        /* 1st arg: DllPath = NULL */
        /* save WOW64 state */
        RAW(41) push     esp /* push r12 */
        RAW(41) push     ebp /* push r13 */
        RAW(41) push     esi /* push r14 */
        RAW(41) push     edi /* push r15 */
        /* align the stack pointer */
        mov      ebx, esp        /* save esp in callee-preserved reg */
        sub      esp, 32         /* call conv */
        and      esp, HEX(fffffff0) /* align to 16-byte boundary */
        call     eax
        mov      esp, ebx        /* restore esp */
        /* restore WOW64 state */
        RAW(41) pop      edi /* pop r15 */
        RAW(41) pop      esi /* pop r14 */
        RAW(41) pop      ebp /* pop r13 */
        RAW(41) pop      esp /* pop r12 */
        /* far jmp to next instr w/ 32-bit switch: jmp 0023:<sml_return_to_32> */
        push     offset sml_return_to_32  /* 8-byte push */
        mov      dword ptr [esp + 4], CS32_SELECTOR /* top 4 bytes of prev push */
        jmp      fword ptr [esp]
sml_return_to_32:
        add      esp, 16         /* clean up far jmp target and &DllCharacteristics */
        pop      ebx             /* restore callee-saved reg */
        ret                      /* return value already in eax */
        END_FUNC(FUNCNAME)

/*
 * int switch_modes_and_call(uint64 func, void *arg1, void *arg2, void *arg3)
 */
# undef FUNCNAME
# define FUNCNAME switch_modes_and_call
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      eax, esp    /* get.. */
        add      eax, ARG_SZ /* ...address of func */
        mov      ecx, ARG3 /* arg1 */
        mov      edx, ARG4 /* arg2 */
        /* save callee-saved registers */
        push     ebx
        mov      ebx, ARG6 /* really ARG5==arg3, but we have 1 push */
        /* far jmp to next instr w/ 64-bit switch: jmp 0033:<smc_transfer_to_64> */
        RAW(ea)
        DD offset smc_transfer_to_64
        DB CS64_SELECTOR
        RAW(00)
smc_transfer_to_64:
    /* Below here is executed in 64-bit mode, but with guarantees that
     * no address is above 4GB, as this is a WOW64 process.
     */
        /* save WOW64 state */
        RAW(41) push     esp /* push r12 */
        RAW(41) push     ebp /* push r13 */
        RAW(41) push     esi /* push r14 */
        RAW(41) push     edi /* push r15 */
        RAW(44) mov      eax, ebx /* mov arg3 from ebx to r8d (3rd arg slot) */
        /* align the stack pointer */
        mov      ebx, esp        /* save esp in callee-preserved reg */
        sub      esp, 32         /* call conv */
        and      esp, HEX(fffffff0) /* align to 16-byte boundary */
        /* arg1 is already in rcx, arg2 in rdx, and arg3 now in r8 */
        RAW(48) mov eax, DWORD [eax] /* mov rax, qword ptr [rax] */
        call     eax             /* call rax */
        mov      esp, ebx        /* restore esp */
        /* restore WOW64 state */
        RAW(41) pop      edi /* pop r15 */
        RAW(41) pop      esi /* pop r14 */
        RAW(41) pop      ebp /* pop r13 */
        RAW(41) pop      esp /* pop r12 */
        /* far jmp to next instr w/ 32-bit switch: jmp 0023:<smc_return_to_32> */
        push     offset smc_return_to_32  /* 8-byte push */
        mov      dword ptr [esp + 4], CS32_SELECTOR /* top 4 bytes of prev push */
        jmp      fword ptr [esp]
smc_return_to_32:
        add      esp, 8          /* clean up far jmp target */
        pop      ebx             /* restore callee-saved reg */
        ret                      /* return value already in eax */
        END_FUNC(FUNCNAME)

#endif /* WINDOWS && !X64 */

/****************************************************************************
 * Injection code shared between core and drinjectlib.
 * XXX: since we are exporting this file in the "drlibc" lib we may want
 * to should move this code to a new file inject_shared.asm or sthg.
 */
#ifdef WINDOWS

/* void load_dynamo(void)
 *
 * used for injection into a child process
 * N.B.:  if the code here grows, SIZE_OF_LOAD_DYNAMO in win32/inject.c
 * must be updated.
 */
        DECLARE_FUNC(load_dynamo)
GLOBAL_LABEL(load_dynamo:)
    /* the code for this routine is copied into an allocation in the app
       and invoked upon return from the injector. When it is invoked,
       it expects the app's stack to look like this:

                xsp-->| &LoadLibrary  |  for x64 xsp must be 16-aligned
                      | &dynamo_path  |
                      | &GetProcAddr  |
                      | &dynamo_entry |___
                      |               |   |
                      |(saved context)| priv_mcontext_t struct
                      | &code_alloc   |   | pointer to the code allocation
                      | sizeof(code_alloc)| size of the code allocation
                      |_______________|___| (possible padding for x64 xsp alignment)
       &dynamo_path-->|               |   |
                      | (dynamo path) | TEXT(DYNAMORIO_DLL_PATH)
                      |_______________|___|
      &dynamo_entry-->|               |   |
                      | (dynamo entry)| "dynamo_auto_start"
                      |               |___|


        in separate allocation         ___
                      |               |   |
                      |     CODE      |  load_dynamo() code
                      |               |___|

        The load_dynamo routine will load the dynamo DLL into memory, then jump
        to its dynamo_auto_start entry point, passing it the saved app context as
        parameters.
     */
        /* two byte NOP to satisfy third party braindead-ness documented in case 3821 */
        mov      edi, edi
#ifdef LOAD_DYNAMO_DEBUGBREAK
        /* having this code in front may hide the problem addressed with the
         * above padding */
        /* giant loop so can attach debugger, then change ebx to 1
         * to step through rest of code */
        mov      ebx, HEX(7fffffff)
load_dynamo_repeat_outer:
        mov      eax, HEX(7fffffff)
load_dynamo_repeatme:
        dec      eax
        cmp      eax, 0
        jg       load_dynamo_repeatme
        dec      ebx
        cmp      ebx, 0
        jg       load_dynamo_repeat_outer

# ifdef X64
        /* xsp is 8-aligned and our pop makes it 16-aligned */
# endif
        /* TOS has &DebugBreak */
        pop      REG_XBX /* pop   REG_XBX = &DebugBreak */
        CALLWIN0(REG_XBX) /* call DebugBreak  (in kernel32.lib) */
#endif
        /* TOS has &LoadLibraryA */
        pop      REG_XBX /* pop   REG_XBX = &LoadLibraryA */
        /* TOS has &dynamo_path */
        pop      REG_XAX /* for 32-bit we're doing "pop eax, push eax" */
        CALLWIN1(REG_XBX, REG_XAX) /* call LoadLibraryA  (in kernel32.lib) */

        /* check result */
        cmp      REG_XAX, 0
        jne      load_dynamo_success
        pop      REG_XBX /* pop off &GetProcAddress */
        pop      REG_XBX /* pop off &dynamo_entry */
        jmp      load_dynamo_failure
load_dynamo_success:
        /* TOS has &GetProcAddress */
        pop      REG_XBX /* pop   REG_XBX = &GetProcAddress */
        /* dynamo_handle is now in REG_XAX (returned by call LoadLibrary) */
        /* TOS has &dynamo_entry */
        pop      REG_XDI /* for 32-bit we're doing "pop edi, push edi" */
        CALLWIN2(REG_XBX, REG_XAX, REG_XDI) /* call GetProcAddress */
        cmp      REG_XAX, 0
        je       load_dynamo_failure

        /* jump to dynamo_auto_start (returned by GetProcAddress) */
        jmp      REG_XAX
        /* dynamo_auto_start will take over or continue natively at the saved
         * context via load_dynamo_failure.
        */
        END_FUNC(load_dynamo)
/* N.B.: load_dynamo_failure MUST follow load_dynamo, as both are
 * copied in one fell swoop by inject_into_thread()!
 */
/* not really a function but having issues getting both masm and gas to
 * let other asm routines jump here.
 * targeted by load_dynamo and dynamo_auto_start by a jump, not a call,
 * when we should not take over and should go native instead.
 * Xref case 7654: we come here to the child's copy from dynamo_auto_start
 * instead of returning to the parent's copy post-load_dynamo to avoid
 * incompatibilites with stack layout accross dr versions.
 */
        DECLARE_FUNC(load_dynamo_failure)
GLOBAL_LABEL(load_dynamo_failure:)
        /* Would be nice if we could free our allocation here as well, but
         * that's too much of a pain (esp. here).
         * Note TOS has the saved context at this point, xref layout in
         * auto_setup. Note this code is duplicated in dynamo_auto_start. */
        mov      REG_XAX, [MCONTEXT_XSP_OFFS + REG_XSP] /* load app xsp */
        mov      REG_XBX, [MCONTEXT_PC_OFFS + REG_XSP] /* load app start_pc */
        /* write app start_pc off top of app stack */
        mov      [-ARG_SZ + REG_XAX], REG_XBX
        /* it's ok to write past app TOS since we're just overwriting part of
         * the dynamo_entry string which is dead at this point, won't affect
         * the popping of the saved context */
        POPGPR
        POPF
        /* we assume reading beyond TOS is ok here (no signals on windows) */
        /* we assume xmm0-5 do not need to be restored */
        /* restore app xsp (POPGPR doesn't) */
        mov      REG_XSP, [-MCONTEXT_PC_OFFS + MCONTEXT_XSP_OFFS + REG_XSP]
        jmp      PTRSZ [-ARG_SZ + REG_XSP]      /* jmp to app start_pc */

        ret
        END_FUNC(load_dynamo_failure)

#endif /* WINDOWS */

END_FILE
