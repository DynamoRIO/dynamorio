/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <stdio.h>

#define DO_SIMPLE_SUSPEND_TEST 1
/* output =
 * starting thread...suspended(count = 0)...resuming...exiting thread...resumed(count = 1)
 */

#define DO_SYNCH_WITH_SUSPEND_SELF_TEST 1
/* output =
 * suspended(count = 1)...resumed,exiting
 */

#define DO_SYNCH_WITH_ALL_SUSPEND_SELF_TEST 1
/* output =
 * Testing exiting with self suspended thread.
 */

#define SLEEP_FOR_NUDGE 0
/* Sleep for 20 sec while one thread has suspended itself in
 * DO_SYNCH_WITH_SUSPEND_SELF_TEST so can manually test nudges that synch with the app
 * in this state (detach, reset, hotp_update etc.). */

/* All combinations finish with :
 * done
 */

BOOL synch_1 = TRUE;
BOOL synch_2 = TRUE;

DWORD WINAPI
ThreadProc1(LPVOID param)
{
    printf("starting thread...");
    fflush(stdout);
    synch_2 = FALSE;
    while (synch_1) {
        SwitchToThread();
    }
    printf("exiting thread...");
    fflush(stdout);
    return 0;
}

DWORD WINAPI
ThreadProc2(LPVOID param)
{
    SuspendThread(GetCurrentThread());
    printf("resumed,exiting\n");
    fflush(stdout);
    return 0;
}

#define CHECK_SUSPEND_COUNT(val, expect)                                               \
    do {                                                                               \
        if ((val) != (expect)) {                                                       \
            printf("\nfailure, suspend count is %d instead of %d on line %d\n", (val), \
                   (expect), __LINE__);                                                \
        }                                                                              \
    } while (0)

int
main(void)
{
    HANDLE ht;
    DWORD tid;
    DWORD res;

#if DO_SIMPLE_SUSPEND_TEST
    ht = CreateThread(NULL, 0, &ThreadProc1, NULL, 0, &tid);

    while (synch_2) {
        SwitchToThread();
    }

    res = SuspendThread(ht);
    printf("suspended(count = %d)...", res);
    fflush(stdout);

    synch_1 = FALSE;

    printf("resuming...");
    fflush(stdout);
    SwitchToThread();
    res = ResumeThread(ht);

    WaitForSingleObject(ht, INFINITE);
    printf("resumed(count = %d)\n", res);
    fflush(stdout);
    CloseHandle(ht);
#endif

#if DO_SYNCH_WITH_SUSPEND_SELF_TEST
    res = 0;
    /* First we test suspending a new thread that hasn't been initialized by dr yet. */
    ht = CreateThread(NULL, 0, &ThreadProc2, NULL, CREATE_SUSPENDED, &tid);
    res = SuspendThread(ht);
    CHECK_SUSPEND_COUNT(res, 1);
    res = ResumeThread(ht);
    CHECK_SUSPEND_COUNT(res, 2);
    res = ResumeThread(ht);
    CHECK_SUSPEND_COUNT(res, 1);
    res = 0;
    /* Thread is now running and should suspend itself.  We want to test suspending
     * an already self suspended thread (xref 9333 for why this is a special case). */
    while (res == 0) {
        res = SuspendThread(ht);
        if (res == 0) {
            /* Thread might not yet have gotten around to suspending itself */
            ResumeThread(ht);
            /* short sleep to wait */
            Sleep(200);
        }
    }
    CHECK_SUSPEND_COUNT(res, 1);
    printf("suspended(count = %d)...", res);
    fflush(stdout);
#    if SLEEP_FOR_NUDGE
    Sleep(20000);
#    endif
    res = ResumeThread(ht);
    CHECK_SUSPEND_COUNT(res, 2);
    res = ResumeThread(ht);
    CHECK_SUSPEND_COUNT(res, 1);
    WaitForSingleObject(ht, INFINITE);
    CloseHandle(ht);
#endif

#if DO_SYNCH_WITH_ALL_SUSPEND_SELF_TEST
    /* xref case 9333, our new thread will suspend itself and we then want to trigger
     * a synch_with_all_threads, will use the process exit one */
    ht = CreateThread(NULL, 0, &ThreadProc2, NULL, 0, &tid);
    SwitchToThread();
    /* FIXME - this is racy, we can't be sure thread has suspended itself without
     * suspending it ourselves, we'll just sleep a little to try and be sure. (We could
     * use the same loop as DO_SYNCH_WITH_SUSPEND_SELF_TEST but is nice to keep the
     * synch_with_thread and synch_with_all_threads tests separate (though I guess that's
     * mainly because only one of them worked when the test was written). */
    Sleep(1000);
    printf("Testing exiting with self suspended thread.\n");
#endif

    printf("done\n");

    return 0;
}
