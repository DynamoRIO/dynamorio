/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Because these routines use pthreads, we separate them from tools.c. */

#ifdef UNIX
#    include <pthread.h>
#else
#    include <windows.h>
#endif
#include <stdlib.h>

/***************************************************************************
 * Condition variables
 */

#ifdef WINDOWS
/* PulseEvent is not reliable enough so we use the CONDITION_VARIABLE API
 * which wasn't added until XP: but it seems fine to drop XP support for
 * building our tests.
 */
typedef struct _win_condvar_t {
    CONDITION_VARIABLE condvar;
    CRITICAL_SECTION critsec;
    bool flag;
} win_condvar_t;

static void *
create_cond_var(void)
{
    win_condvar_t *cv = (win_condvar_t *)malloc(sizeof(*cv));
    if (cv != NULL) {
        InitializeConditionVariable(&cv->condvar);
        InitializeCriticalSection(&cv->critsec);
        cv->flag = false;
    }
    return (void *)cv;
}

static void
wait_cond_var(void *var)
{
    win_condvar_t *cv = (win_condvar_t *)var;
    EnterCriticalSection(&cv->critsec);
    while (!cv->flag) {
        /* What should we do if it fails? */
        SleepConditionVariableCS(&cv->condvar, &cv->critsec, INFINITE);
    }
    LeaveCriticalSection(&cv->critsec);
}

/* Signals to *all* waiting threads */
static void
signal_cond_var(void *var)
{
    win_condvar_t *cv = (win_condvar_t *)var;
    EnterCriticalSection(&cv->critsec);
    cv->flag = true;
    WakeAllConditionVariable(&cv->condvar);
    LeaveCriticalSection(&cv->critsec);
}

static void
reset_cond_var(void *var)
{
    win_condvar_t *cv = (win_condvar_t *)var;
    EnterCriticalSection(&cv->critsec);
    cv->flag = false;
    LeaveCriticalSection(&cv->critsec);
}

static void
destroy_cond_var(void *var)
{
    win_condvar_t *cv = (win_condvar_t *)var;
    DeleteCriticalSection(&cv->critsec);
    free(cv);
}

#else
typedef struct _pthread_condvar_t {
    pthread_cond_t condvar;
    pthread_mutex_t lock;
    bool flag;
} pthread_condvar_t;

static void *
create_cond_var(void)
{
    pthread_condvar_t *cv = (pthread_condvar_t *)malloc(sizeof(*cv));
    if (cv != NULL) {
        pthread_mutex_init(&cv->lock, NULL);
        pthread_cond_init(&cv->condvar, NULL);
        cv->flag = false;
    }
    return (void *)cv;
}

static void
wait_cond_var(void *var)
{
    pthread_condvar_t *cv = (pthread_condvar_t *)var;
    pthread_mutex_lock(&cv->lock);
    while (!cv->flag)
        pthread_cond_wait(&cv->condvar, &cv->lock);
    pthread_mutex_unlock(&cv->lock);
}

/* Signals to *all* waiting threads */
static void
signal_cond_var(void *var)
{
    pthread_condvar_t *cv = (pthread_condvar_t *)var;
    pthread_mutex_lock(&cv->lock);
    cv->flag = true;
    pthread_cond_broadcast(&cv->condvar);
    pthread_mutex_unlock(&cv->lock);
}

static void
reset_cond_var(void *var)
{
    pthread_condvar_t *cv = (pthread_condvar_t *)var;
    pthread_mutex_lock(&cv->lock);
    cv->flag = false;
    pthread_mutex_unlock(&cv->lock);
}

static void
destroy_cond_var(void *var)
{
    pthread_condvar_t *cv = (pthread_condvar_t *)var;
    pthread_mutex_destroy(&cv->lock);
    pthread_cond_destroy(&cv->condvar);
    free(cv);
}
#endif /* !WINDOWS */

/***************************************************************************/
