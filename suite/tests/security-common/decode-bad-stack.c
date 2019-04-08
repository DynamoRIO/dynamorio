/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */
#    include <assert.h>
#    include <stdio.h>
#    include <math.h>

#    ifdef UNIX
#        include <unistd.h>
#        include <signal.h>
#        include <ucontext.h>
#        include <errno.h>
#    endif

/* asm routine */
void
jumpto(unsigned char *buf);

#    define ITERS 1500000

static int a[ITERS];

#    ifdef UNIX
static void
signal_handler(int sig)
{
    if (sig == SIGILL)
        print("Got an illegal instruction\n");
    abort();
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
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
        print("Got an illegal instruction\n");
#        if VERBOSE
    print("Exception occurred, process about to die silently\n");
#        endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#    endif

int
main(int argc, char *argv[])
{
    double res = 0.;
    unsigned char buf[8];
    buf[0] = 0xdf;
    buf[1] = 0xfa;
    /* FIXME we don't call INIT here - template can't use SEC_VIO_AUTO_STOP */

#    ifdef UNIX
    intercept_signal(SIGILL, (handler_3_t)signal_handler, false);
#    else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#    endif

    print("Bad instr about to happen\n");

    /* jmp to bad instr on stack */
    jumpto(buf);

    print("SHOULD NEVER GET HERE\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#define FUNCNAME jumpto
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        jmp      ARG1
        /* won't come back */
        ret
        END_FUNC(FUNCNAME)

END_FILE
/* clang-format on */
#endif
