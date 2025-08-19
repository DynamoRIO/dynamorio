/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2007 VMware, Inc.  All rights reserved.
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

/* Separated out from tools.h because it requires linking with pthreads. */

#ifndef THREADS_H
#define THREADS_H

/***************************************************************************/
#ifdef UNIX

#    define WINAPI

#    include <pthread.h>
#    include <unistd.h>
#    include <sched.h>

typedef pthread_t thread_t;

#    define THREAD_FUNC_RETURN_TYPE void *
#    define THREAD_FUNC_RETURN_ZERO NULL

/* Create a new thread. It should be passed "run_func", a function which
 * takes one argument ("arg"), for the thread to execute.
 * Returns the handle to the new thread.
 */
static thread_t
create_thread(THREAD_FUNC_RETURN_TYPE (*run_func)(void *), void *arg)
{
    thread_t thread;
    pthread_create(&thread, NULL, run_func, arg);
    return thread;
}

static void
join_thread(thread_t thread)
{
    pthread_join(thread, NULL);
}

void
thread_sleep(int ms)
{
    usleep(1000 * ms);
}

void
thread_yield(void)
{
    sched_yield();
}

/***************************************************************************/
#else /* WINDOWS */

#    include <windows.h>
#    include <process.h> /* for _beginthreadex */

typedef HANDLE thread_t;

#    define THREAD_FUNC_RETURN_TYPE unsigned int __stdcall
#    define THREAD_FUNC_RETURN_ZERO 0

/* Create a new thread. It should be passed "run_func", a function which
 * takes one argument ("arg"), for the thread to execute.
 * Returns a handle to the new thread.
 */
thread_t
create_thread(unsigned int(__stdcall *run_func)(void *), void *arg)
{
    unsigned int tid;
    return (thread_t)_beginthreadex(NULL, 0, run_func, arg, 0, &tid);
}

void
delete_thread(thread_t thread, void *stack)
{
    VERBOSE_PRINT("Waiting for child to exit\n");
    WaitForSingleObject(thread, INFINITE);
    VERBOSE_PRINT("Child has exited\n");
}

void
join_thread(thread_t thread)
{
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void
thread_sleep(int ms)
{
    Sleep(ms);
}

void
suspend_thread(thread_t thread)
{
    SuspendThread(thread);
}

void
resume_thread(thread_t thread)
{
    ResumeThread(thread);
}

#    ifndef STATIC_LIBRARY /* FIXME i#975: conflicts with DR's symbols. */
void
thread_yield()
{
    Sleep(0); /* stay ready */
}
#    endif

#endif /* WINDOWS */

#endif /* THREADS_H */
