/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "configure.h"
#include "dr_api.h"
#ifdef WINDOWS
/* Importing from DR causes trouble injecting. */
#    include "dr_annotations.h"
#    define IS_UNDER_DR() DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO()
#else
#    define IS_UNDER_DR() dr_app_running_under_dynamorio()
#endif
#include "tools.h"
#include "thread.h"
#include "condvar.h"
#include <setjmp.h>
#include <signal.h>

/***************************************************************************
 * Test go-native features.
 * Strategy: create a thread, use a pattern to tell the client to have it go
 * native, and test things like fault handling while native.
 */

static SIGJMP_BUF mark;
static int count;

static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    print("Got signal %d; count %d\n", signal, count);
    count++;
    SIGLONGJMP(mark, count);
}

THREAD_FUNC_RETURN_TYPE
thread_func(void *arg)
{
    /* Trigger the client to have us go native. */
    NOP_NOP_NOP;
    print("Under DR?: %d\n", IS_UNDER_DR());
    /* Now test tricky while-native things like a fault. */
    if (SIGSETJMP(mark) == 0)
        *(int *)arg = 42;
    /* Try a default-ignore signal. */
    if (SIGSETJMP(mark) == 0)
        pthread_kill(pthread_self(), SIGURG);
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(int argc, const char *argv[])
{
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal, false);
    intercept_signal(SIGURG, (handler_3_t)&handle_signal, false);
    thread_t thread = create_thread(thread_func, NULL);
    join_thread(thread);
    print("All done.\n");
    return 0;
}
