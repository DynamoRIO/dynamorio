/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

/* This is adapted from MSDN help on 'longjmp' */

#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string.h>

/* This is only in float.h for VS2010+ */
#ifndef _FPE_MULTIPLE_TRAPS
#    define _FPE_MULTIPLE_TRAPS 0x8d
#endif

jmp_buf mark; /* Address for long jump to jump to */
int fperr;    /* Global error number */

void __cdecl fphandler(int sig, int num); /* Prototypes */
void
fpcheck(void);

void
test(double n1, double n2)
{
    double r;
    int jmpret;
    /* Save stack environment for return in case of error. First
     * time through, jmpret is 0, so true conditional is executed.
     * If an error occurs, jmpret will be set to -1 and false
     * conditional will be executed.
     */
    jmpret = setjmp(mark);
    if (jmpret == 0) {
        r = n1 / n2;
        /* This won't be reached if error occurs. */
        printf("\n\n%4.3g / %4.3g = %4.3g\n", n1, n2, r);
        r = n1 * n2;
        /* This won't be reached if error occurs. */
        printf("\n\n%4.3g * %4.3g = %4.3g\n", n1, n2, r);
    } else
        fpcheck();
}

int
main()
{
    /* Unmask all floating-point exceptions. */
    _control87(0, _MCW_EM);
    /* Set up floating-point error handler. The compiler
     * will generate a warning because it expects
     * signal-handling functions to take only one argument.
     * 'void (__cdecl *)(int,int)' differs in parameter lists from 'void (__cdecl *)(int)'
     */
#pragma warning(disable : 4113)
    if (signal(SIGFPE, fphandler) == SIG_ERR) {
        fprintf(stderr, "Couldn't set SIGFPE\n");
        abort();
    }
    test(4., 0.);
    test(0., DBL_MAX);
    test(DBL_MAX, DBL_MAX);
    return 0;
}

/* fphandler handles SIGFPE (floating-point error) interrupt. Note
 * that this prototype accepts two arguments and that the
 * prototype for signal in the run-time library expects a signal
 * handler to have only one argument.
 *
 * The second argument in this signal handler allows processing of
 * _FPE_INVALID, _FPE_OVERFLOW, _FPE_UNDERFLOW, and
 * _FPE_ZERODIVIDE, all of which are Microsoft-specific symbols
 * that augment the information provided by SIGFPE. The compiler
 * will generate a warning, which is harmless and expected.

 */
void
fphandler(int sig, int num)
{
    /* Set global for outside check since we don't want
     * to do I/O in the handler.
     */
    fperr = num;
    /* Initialize floating-point package. */
    _fpreset();
    /* Restore calling environment and jump back to setjmp. Return
     * -1 so that setjmp will return false for conditional test.
     */
    printf("about to do longjmp\n");
    fflush(stdout);
    longjmp(mark, -1);
}

void
fpcheck(void)
{
    char fpstr[30];
    switch (fperr) {
    case _FPE_INVALID: strcpy(fpstr, "Invalid number"); break;
    case _FPE_OVERFLOW: strcpy(fpstr, "Overflow"); break;
    case _FPE_UNDERFLOW: strcpy(fpstr, "Underflow"); break;
    /* FIXME i#910: on win8 this is raised instead of _FPE_ZERODIVIDE */
    case _FPE_MULTIPLE_TRAPS:
        fperr = _FPE_ZERODIVIDE;
        /* fall-through */
    case _FPE_ZERODIVIDE: strcpy(fpstr, "Divide by zero"); break;
    default: strcpy(fpstr, "Other floating point error"); break;
    }
    printf("Error %d: %s\n", fperr, fpstr);
}
