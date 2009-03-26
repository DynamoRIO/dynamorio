/* **********************************************************
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

/* We would have the client handle clean call crashes, but today they
 * go to the app (PR 213600); plus there is no signal/exception event
 * on linux (PR 304708)
 */

#include "tools.h" /* for print() */

#ifdef LINUX
# include <unistd.h>
# include <signal.h>
# include <ucontext.h>
# include <errno.h>
# include <stdlib.h>
#endif

#include <setjmp.h>

#ifdef WINDOWS
# define NOP __nop()
#else /* LINUX */
# define NOP asm("nop")
#endif

static void foo(void)
{ /* nothing: just a marker */ }

jmp_buf mark;

/* just use single-arg handlers */
typedef void (*handler_t)(int);
typedef void (*handler_3_t)(int, struct siginfo *, void *);

static int count;

#ifdef LINUX
# define ALT_STACK_SIZE  (SIGSTKSZ*3)

static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
        count++;
        print("Access violation, instance %d\n", count);
        longjmp(mark, count);
    }
    exit(-1);
}

#define ASSERT_NOERR(rc) do {                                   \
  if (rc) {                                                     \
     fprintf(stderr, "%s:%d rc=%d errno=%d %s\n",               \
             __FILE__, __LINE__,                                \
             rc, errno, strerror(errno));                       \
  }                                                             \
} while (0);

/* set up signal_handler as the handler for signal "sig" */
static void
intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (handler_3_t) handler;
    /* FIXME: due to DR bug 840 we cannot block ourself in the handler
     * since the handler does not end in a sigreturn, so we have an empty mask
     * and we use SA_NOMASK
     */
    rc = sigemptyset(&act.sa_mask); /* block no signals within handler */
    ASSERT_NOERR(rc);
    /* FIXME: due to DR bug #654 we use SA_SIGINFO -- change it once DR works */
    act.sa_flags = SA_NOMASK | SA_SIGINFO | SA_ONSTACK;
    
    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

#else

# include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#if VERBOSE
        /* DR gets the target addr wrong for far call/jmp so we don't print it */
        print("\tPC "PFX" tried to %s address "PFX"\n",
            pExceptionInfo->ExceptionRecord->ExceptionAddress, 
            (pExceptionInfo->ExceptionRecord->ExceptionInformation[0]==0)?"read":"write",
            pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
#endif
        count++;
        print("Access violation, instance %d\n", count);
        longjmp(mark, count);
    }
    print("Exception "PFX" occurred, process about to die silently\n",
          pExceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

int main(int argc, char *argv[])
{
    int i, j;
#ifdef LINUX
    intercept_signal(SIGSEGV, signal_handler);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);
#endif

    /* Each test in cleancall.dll.c crashes at the end */
    for (j = 0; j < 5; j++) {
        i = setjmp(mark);
        if (i == 0) {
            print("testing clean calls\n");
            /* use 2 NOPs + call to indicate it's ok to do the tests
             * now that the handlers are all set up
             */
            NOP; NOP;
            foo();
            print("did not crash\n");
        }
    }
    print("done testing clean calls\n");

    return 0;
}
