/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

/* TOTEST: windbg -d  to ignore all exceptions should have them all handled */

#include "../security-win32/except.h"

#include <setjmp.h>
#include "tools.h" /* for print() */

jmp_buf mark;
int count = 0;

/* INT3 is a trap-type exception
 * the saved EIP should point to the byte after the instruction
 * which should be the beginning of the next instruction.
 * (Note debuggers normally overwrite existing instructions
 * and then would have to decrement EIP by 1 (since they know they used 0XCC)
 * to execute to original interrupted instruction.)
 */

#define TRAP_OPT() __pragma optimize("", off) __debugbreak() __pragma optimize("", on)

/* don't use at CPL0 INT3 (or INT1, or BOUNDS) - will disable interrupts */
#define TRAP() __asm { int 3 } /* 0xcc */

#define TRAP_TWO_BYTE() __asm { __emit 0xcd __emit 0x03 }
/* see Intel manual - this version doesn't have all special characteristics of the 0xcc,
 * but in our mode should be handled ok
 */

#define SINGLE_STEP() __asm { int 1 }

#define DEBUGGER_INTERFACE() __asm { int 2d }

/* FIXME: test single stepping with TRAP bit set in EFLAGS - can one control this at CPL3?
 */

/* FIXME: case 11058 I don't understand this - INT3 is supposed to save the EIP _AFTER_
 * the instruction why am I getting the original instruction natively?
 */
/* as a workaround we increment EIP again */
#define SKIP_INT3 ((GetExceptionInformation())->ContextRecord->CXT_XIP++)

int
TrapIfDebugger()
{
    int trapped = 0;
    __try {
        TRAP();
        trapped = 1;
    } __except (SKIP_INT3, EXCEPTION_CONTINUE_EXECUTION) {
    }
    return !trapped;
}

int
TrapIfDebuggerVerbose()
{
    EXCEPTION_RECORD exception;
    CONTEXT *context;

    int dbg = 0;
    int trapped = 0;
    print("about to trap\n");
    __try {
        initialize_registry_context();
        TRAP();
        print("continued after trap\n");
        dbg = 1;
    } __except (
        context = (GetExceptionInformation())->ContextRecord,
        dump_exception_info((GetExceptionInformation())->ExceptionRecord, context),
        trapped = 1, print("in filter\n"),
        (GetExceptionInformation())->ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT
        ?
        /* breakpoint - continue next */ SKIP_INT3,
        EXCEPTION_CONTINUE_EXECUTION :
        /* not ours */ EXCEPTION_CONTINUE_SEARCH) {
        print("handler NOT REACHED\n");
    }
    if (!trapped)
        print("didn't trap, continued in debugger?!\n");
    return !trapped;
}

int
invalid_handle()
{
    int dbg;
    /* case 11051 - why is there an STATUS_INVALID_HANDLE exception on Vista
     * kernel32!CloseHandle */
    print("Invalid handle about to happen\n");
    print("about to close\n");
    __try {
        HANDLE h = (HANDLE)9999;
        dbg = 1;
        CloseHandle(h);
        dbg = 0;
    } __except (print("in close filter\n", EXCEPTION_CONTINUE_EXECUTION)) {
    }
    print("continued successfully\n");
    return dbg;
}

#define name_status(x, s) \
    if ((x) == (s))       \
        print(#s);

#include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    print("caught exception " PFX ": ", pExceptionInfo->ExceptionRecord->ExceptionCode);
    /* 0x8.. */
    name_status(pExceptionInfo->ExceptionRecord->ExceptionCode,
                STATUS_GUARD_PAGE_VIOLATION);
    /* STATUS_DATATYPE_MISALIGNMENT maybe on some syscalls? */
    name_status(pExceptionInfo->ExceptionRecord->ExceptionCode, STATUS_BREAKPOINT);
    name_status(pExceptionInfo->ExceptionRecord->ExceptionCode, STATUS_SINGLE_STEP);

    /* 0xC.. */
    name_status(pExceptionInfo->ExceptionRecord->ExceptionCode, STATUS_INVALID_HANDLE);

    count++;
    print("exception, instance %d\n", count);
    longjmp(mark, count);
}

int
main(int argc, char *argv[])
{
    int i, j;
    int dbg;
    USE_USER32();
    INIT();

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);

    i = setjmp(mark);
    print("Test %d\n", i);

    switch (i) {

    case 0:
        dbg = TrapIfDebuggerVerbose();
        print("%s\n", dbg ? "debugger handled on first chance" : "not handled");
        /* continue */
    case 1:
        dbg = TrapIfDebugger();
        print("%s\n", dbg ? "debugger handled on first chance" : "not handled");
        /* continue */
    case 2:
        dbg = invalid_handle();
        print("%s\n", dbg ? "debugger handled on first chance" : "not handled");
        /* continue */
    default:;
    }

    print("All done\n");

    return 0;
}
