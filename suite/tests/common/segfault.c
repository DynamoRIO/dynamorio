/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

#include "tools.h" /* for print() */

#include <assert.h>
#include <stdio.h>
#include <math.h>

#ifdef LINUX
# include <unistd.h>
# include <signal.h>
# include <ucontext.h>
# include <errno.h>
#endif

/* just use single-arg handlers */
typedef void (*handler_t)(int);
typedef void (*handler_3_t)(int, struct siginfo *, void *);

#ifdef USE_DYNAMO
#include "dynamorio.h"
#endif

#define ITERS 1500000

static int a[ITERS];

#ifdef LINUX
static void
signal_handler(int sig)
{
    if (sig == SIGSEGV)
        print("Got a seg fault\n");
    abort();
}

#define ASSERT_NOERR(rc) do {					\
  if (rc) {							\
     fprintf(stderr, "%s:%d rc=%d errno=%d %s\n", 		\
	     __FILE__, __LINE__,				\
	     rc, errno, strerror(errno));  		        \
  }								\
} while (0);

/* set up signal_handler as the handler for signal "sig" */
static void
intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (handler_3_t) handler;
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
    ASSERT_NOERR(rc);
    act.sa_flags = SA_ONSTACK;

    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

#else
/* sort of a hack to avoid the MessageBox of the unhandled exception spoiling
 * our batch runs
 */
# include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        print("Got a seg fault\n");
# if VERBOSE
    print("Exception occurred, process about to die silently\n");
# endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

int main(int argc, char *argv[])
{
    double res = 0.;

#ifdef USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif
  
#ifdef LINUX
    intercept_signal(SIGSEGV, signal_handler);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);
#endif

    print("Segfault about to happen\n");

    *((volatile int *)0) = 4;

    print("SHOULD NEVER GET HERE\n");

#ifdef USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif
    return 0;
}
