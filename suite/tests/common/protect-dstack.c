/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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
/* Tests protection of DynamoRIO's stack
 * This test when run natively will fail with an error message
 */

#    include "tools.h"

#    ifdef UNIX
#        include <unistd.h>
#        include <signal.h>
#        include <ucontext.h>
#        include <errno.h>
#    endif

#    include <setjmp.h>

/* asm routines */
void
clear_eflags(void);
void
evil_copy(void *start, int count, ptr_int_t value);

#    define VERBOSE 0

#    define EXPANDSTR(x) #    x
#    define STRINGIFY(x) EXPANDSTR(x)

/* bottom page is a guard page, so ignore it -- consider only top 8KB */
#    define DSTACK_SIZE (8 * 1024)

/* N.B.: dependent on exact DR offsets here! */
#    ifdef UNIX
/* this used to be 44 prior to 1/24/06 commit, and 20 prior to Mar 11 2006 */
#        define DCONTEXT_TLS_OFFSET IF_X64_ELSE(32, 16)
#        define DSTACK_OFFSET_IN_DCONTEXT IF_X64_ELSE(0x548, 0x2a8)
#        ifdef X64
#            define GET_DCONTEXT(var)                                                   \
                asm("mov  %%gs:" STRINGIFY(DCONTEXT_TLS_OFFSET) ", %%rax" : : : "rax"); \
                asm("mov  %%rax, %0" : "=m"((var)));
#        else
#            define GET_DCONTEXT(var)                                                   \
                asm("movl %%fs:" STRINGIFY(DCONTEXT_TLS_OFFSET) ", %%eax" : : : "eax"); \
                asm("movl %%eax, %0" : "=m"((var)));
#        endif
#    else
unsigned int dcontext_tls_offset;

#        define DSTACK_OFFSET_IN_DCONTEXT IF_X64_ELSE(0x2c8, 0x2a4)
#        define GET_DCONTEXT(var) \
            var = (void *)IF_X64_ELSE(__readgsqword, __readfsdword)(dcontext_tls_offset);

/*     0:001> dt getdc owning_thread
 *        +0x05c owning_thread
 */
#        define OWNING_THREAD_OFFSET_IN_DCONTEXT IF_X64_ELSE(0x350, 0x2f0)
/* offset varies based on release/debug build (# of slots we need)
 * and cache line size (must be aligned) and the -ibl_table_in_tls
 * option being set to true
 */

#    endif

SIGJMP_BUF mark;
int where; /* 0 = normal, 1 = segfault SIGLONGJMP, 2 = evil takeover */

#    ifdef UNIX
static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
#        if VERBOSE
        print("Got seg fault\n");
#        endif
        SIGLONGJMP(mark, 1);
    }
    exit(-1);
}
#    else
/* sort of a hack to avoid the MessageBox of the unhandled exception spoiling
 * our batch runs
 */
#        include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#        if VERBOSE
        /* DR gets the target addr wrong for far call/jmp so we don't print it */
        print("\tPC " PFX " tried to %s address " PFX "\n",
              pExceptionInfo->ExceptionRecord->ExceptionAddress,
              (pExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0) ? "read"
                                                                              : "write",
              pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
#        endif
        SIGLONGJMP(mark, 1);
    }
#        if VERBOSE
    print("Exception occurred, process about to die silently\n");
#        endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#    endif

/* global so that evil can use it */
int *dstack_base;

/* the goal is to get DR to jmp to here by clobbering the fcache_return ret addr
 * on the dstack
 */
static void
evil()
{
    /* popf of DR's eflags (in old DR design) w/ our clobbered value could
     * set some funny flags -- clear them all here
     */
    clear_eflags();
    /* don't trusk stack -- hopefully enough there to call SIGLONGJMP
     * certainly can't return from this function since not called
     */
    SIGLONGJMP(mark, 2);
}

int
main()
{
    int *pc;
    int release_build = 0; /* 1 == release, 0 == debug */
    void *dcontext;
    int *dstack;
    int tls_offs;
    ptr_int_t owning_thread;
    INIT();

#    ifdef UNIX
    intercept_signal(SIGSEGV, (handler_3_t)signal_handler, false);
#    else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#    endif

#    ifdef WINDOWS
    /* brute force loop over all TLS entries,
     * and see whether owning_thread is GetCurrentThreadId()
     *     0:001> dt getdc owning_thread
     *        +0x05c owning_thread : 0xed8
     *
     *      0:001> dt _TEB TLS64
     *        +0xe10 TLS64 : [64] Ptr32 Void
     */
    for (tls_offs = 63; tls_offs >= 0; tls_offs--) {
        enum { offsetof_TLS64_in_TEB = IF_X64_ELSE(0x1480, 0xe10) };
        dcontext_tls_offset = offsetof_TLS64_in_TEB + tls_offs * sizeof(void *);
        GET_DCONTEXT(dcontext);
#        if VERBOSE
        print("%d idx, %x offs\n", tls_offs, dcontext_tls_offset);
#        endif
        where = SIGSETJMP(mark);
        if (where == 0) {
            owning_thread =
                *(ptr_int_t *)(((char *)dcontext) + OWNING_THREAD_OFFSET_IN_DCONTEXT);
            /* we didn't crash reading, is it really thread ID? */
#        if VERBOSE
            print("     %d thread %d vs %d\n", tls_offs, owning_thread,
                  GetCurrentThreadId());
#        endif
            if (owning_thread == GetCurrentThreadId()) {
#        if VERBOSE
                print("     %d is dcontext!\n", tls_offs);
#        endif
                break;
            }
        } else {
#        if VERBOSE
            print("     %d crashed\n", tls_offs);
#        endif
            /* we crashed reading, try next offset */
        }
    }
    if (tls_offs < 0) {
        print("error obtaining dcontext (TLS offset not found): "
              "are you running natively?!?\n");
        exit(1);
    }
#    endif
    where = SIGSETJMP(mark);
    if (where != 0) {
        print("error obtaining dcontext (SIGSETJMP failed): "
              "are you running natively?!?\n");
        exit(1);
    }
    GET_DCONTEXT(dcontext)
#    if VERBOSE
    print("dcontext is " PFX "\n", dcontext);
#    endif
    dstack = *(int **)(((char *)dcontext) + DSTACK_OFFSET_IN_DCONTEXT);
    if (dstack == NULL || !ALIGNED(dstack, PAGE_SIZE)) {
        print("can't find dstack: old build, or new where dstack offset changed?\n");
        while (1)
            ;
        exit(-1);
    }
    dstack_base = (int *)(((char *)dstack) - DSTACK_SIZE);
#    if VERBOSE
    print("dstack is " PFX "-" PFX "\n", dstack_base, dstack);
#    endif
    print("dcontext->dstack successfully obtained\n");
    where = SIGSETJMP(mark);
#    if VERBOSE
    print("SIGSETJMP returned %d\n", where);
#    endif
    if (where == 0) {
        /* if we do the copy in a C loop, trace heads cause us to exit before
         * we've hit the cxt switch return address, so we crash rather than taking
         * control -- so we hand-code a copy that in C looks like this:
         *          for (pc = dstack_base; pc++; pc < dstack)
         *              *pc = (int) evil;
         * we assume df is cleared
         * FIXME: popf in old fcache_return can trigger a trap crash before
         * get to ret that goes to evil!
         * FIXME: I had this getting to evil w/o crashing first, but it's
         * a little fragile, and on win32 I get issues later b/c we have
         * trampolines, etc. and so don't completely lose control.
         * But, in all cases we fail, so whether it's a nice shell code
         * execution or a crash doesn't matter -- the test does what it's supposed
         * to do!
         */
        evil_copy(dstack_base, DSTACK_SIZE / sizeof(int), (ptr_int_t)evil);
        print("wrote to entire dstack without incident!\n");
    } else if (where == 1) {
        print("error writing to " PFX " in expected dstack " PFX "-" PFX "\n", pc,
              dstack_base, dstack);
    } else if (where == 2) {
        print("DR has been cracked!  Malicious code is now runnning...\n");
    }
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME clear_eflags
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* We don't bother w/ SEH64 directives, though we're an illegal leaf routine! */
        push     0
        popf
        ret
        END_FUNC(FUNCNAME)

/* void evil_copy(void *start, int count, ptr_int_t value); */
#undef FUNCNAME
#define FUNCNAME evil_copy
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
       /* Must use ARG macros before any pushes; but must preserve xdi */
        mov      REG_XAX, ARG3
        mov      REG_XCX, ARG1
        mov      REG_XDX, ARG2
        PUSH_SEH(REG_XDI)
        END_PROLOG
        xchg     REG_XDX, REG_XCX /* contortions b/c of x64 param regs */
        mov      REG_XDI, REG_XDX
        rep stosd
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        pop      REG_XDI
        ret
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
