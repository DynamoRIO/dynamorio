/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

/* Tests detaching from threads in a variety of states. */

#include <windows.h>
#include "tools.h"

#define PROC_NAME "detach_test.exe"
#define MAX_COUNT 10

static HWND hwnd = NULL;
static BOOL thread_ready = FALSE;

void CALLBACK
SendAsyncProc(HWND hwnd, UINT uMsg, ULONG *dwData, LRESULT lResult);
void
do_test(int count);

int
do_busy_work(int c)
{
    int i, t;
    for (i = 0, t = 0; i < c; i++)
        t = (i * c) - t;
    if (t != c * ((c + 1) / 2))
        print("Failure %d != %d\n", t, c * ((c + 1) / 2));
    return t;
}

/* detach from in cache thread */
static BOOL in_busy_work = FALSE;
static BOOL exit_busy_work = FALSE;
DWORD WINAPI
ThreadProcBusyWork(LPVOID param)
{
    print("Starting busy work\n");
    in_busy_work = TRUE;
    while (!exit_busy_work)
        do_busy_work(20);
    in_busy_work = FALSE;
    exit_busy_work = FALSE;
    print("Done busy working\n");
    return 0;
}

/* detach from actively building thread */
static BOOL in_busy_build = FALSE;
static BOOL exit_busy_build = FALSE;
#define BUF_LEN 0x1000
DWORD WINAPI
ThreadProcBusyBuild(LPVOID param)
{
    char *buf = VirtualAlloc(NULL, BUF_LEN, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    /* use selfmod to keep building */
    copy_to_buf(buf, BUF_LEN, NULL, CODE_SELF_MOD, COPY_NORMAL);
    print("Starting busy build\n");
    in_busy_build = TRUE;
    while (!exit_busy_build) {
        /* don't do more than sandbox2ro_threshold iters (20) to avoid case 9908
         * triggered resets (xref 10036 too) leading to hangs from detach at same time
         * (case 8492). We're only trying to test code creation here anyways.
         * Using 2 different values so there's a real change with the write. */
        test(buf, 4);
        test(buf, 5);
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    in_busy_build = FALSE;
    exit_busy_build = FALSE;
    print("Done busy building\n");
    return 0;
}

/* see win32/tls.c test for alt. method of starting a detach, this way is preferable
 * since we may at some point disallow the process from detaching itself */
void
detach()
{
    char buf[2 * MAX_PATH] = "\"";
    char *tools = getenv("DYNAMORIO_WINTOOLS");
    int size;
    STARTUPINFO sinfo = { sizeof(sinfo),
                          NULL,
                          "",
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          STARTF_USESTDHANDLES,
                          0,
                          0,
                          NULL,
                          GetStdHandle(STD_INPUT_HANDLE),
                          GetStdHandle(STD_OUTPUT_HANDLE),
                          GetStdHandle(STD_ERROR_HANDLE) };
    PROCESS_INFORMATION pinfo;
    strncat(buf, tools == NULL ? "" : tools, BUFFER_ROOM_LEFT(buf));
    NULL_TERMINATE_BUFFER(buf);
    strncat(buf, "\\DRcontrol.exe\" -detachexe " PROC_NAME, BUFFER_ROOM_LEFT(buf));
    NULL_TERMINATE_BUFFER(buf);
    print("Detaching\n");
    if (CreateProcess(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo)) {
        /* This thread will be detached at non-dr intercepted syscall (the wait below)
         * with multiple stacked callbacks (from SendAsyncProc). */
        WaitForSingleObject(pinfo.hProcess, INFINITE);
        CloseHandle(pinfo.hProcess);
        CloseHandle(pinfo.hThread);
    } else {
        print("Detach Failed!\n");
    }
}

static BOOL did_send_callback[MAX_COUNT];
static BOOL action_detach = FALSE;
static BOOL action_exit = FALSE;
void CALLBACK
SendAsyncProc(HWND hwnd, UINT uMsg, ULONG *dwData, LRESULT lResult)
{
    int count = (int)dwData;
    did_send_callback[count] = TRUE;
    if (count > 0) {
        do_test(count - 1);
    } else {
        if (action_detach) {
            HANDLE ht_busy_build, ht_busy_work;
            DWORD tid;
            ht_busy_work = CreateThread(NULL, 0, &ThreadProcBusyWork, NULL, 0, &tid);
            while (!in_busy_work)
                Sleep(10);
            ht_busy_build = CreateThread(NULL, 0, &ThreadProcBusyBuild, NULL, 0, &tid);
            while (!in_busy_build)
                Sleep(10);
            detach();
            print("Detach finished\n");
            exit_busy_build = TRUE;
            WaitForSingleObject(ht_busy_build, INFINITE);
            exit_busy_work = TRUE;
            WaitForSingleObject(ht_busy_work, INFINITE);
            CloseHandle(ht_busy_build);
            CloseHandle(ht_busy_work);
        } else if (action_exit) {
            print("Exiting with stacked callbacks\n");
            ExitThread(0);
        }
    }
}

/* Some website claimed that EnumWindows was done by a kernel callback, but it's not
 * (on XP at least).  SendMessageCallback does, however, use a kernel callback for its
 * callback (interestingly not just any alertable system call will do, has to be
 * a message system call for the callback to be delivered). So we nest those to build
 * up a callback stack. This routine will end up being recursively called count times
 * building up count stacked callbacks. */
#define MAX_SLEEP 30000
void
do_test(int count)
{
    MSG msg;
    int total_slept = 0;

    did_send_callback[count] = FALSE;
    if (!SendMessageCallback(hwnd, WM_NULL, 0, 0, &SendAsyncProc, count)) {
        print("SendMsg failed.\n");
        return;
    }

    while (!did_send_callback[count] && total_slept < MAX_SLEEP) {
        Sleep(100);
        total_slept += 100;
        /* FIXME - all callbacks will have the same after address, need to mix in a
         * GetMessage or some such so can verify order of callback stack.  */
        PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
    }
    if (total_slept >= MAX_SLEEP) {
        print("Callback never delivered.\n");
    }
}

DWORD WINAPI
ThreadProcDoTest(LPVOID param)
{
    do_test((int)param);
    print("Finished test (Not Reached)\n");
}

/* detach from self suspended thread (also covers thread at dr intercepted syscall) */
DWORD WINAPI
ThreadProcSelfSuspend(LPVOID param)
{
    SuspendThread(GetCurrentThread());
    print("SuspendSelf resumed, exiting\n");
    return 0;
}

LRESULT CALLBACK
wnd_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI
window_func(void *arg)
{
    MSG msg;
    char *winName = "foobar";
    WNDCLASS wndclass = {
        0,    wnd_callback, 0,    0,    NULL /* WinMain hwnd would be here */,
        NULL, NULL,         NULL, NULL, winName
    };

    if (!RegisterClass(&wndclass)) {
        print("Unable to create window class\n");
        return 0;
    }
    hwnd = CreateWindow(winName, winName, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, NULL, NULL, NULL /* WinMain hwnd would be here */,
                        NULL);
    if (hwnd == NULL) {
        print("Error creating window\n");
        return 0;
    }
    /* deliberately not calling ShowWindow */

    thread_ready = true;
    while (true) {
        if (GetMessage(&msg, NULL, 0, 0)) {
            /* Messages not auto-sent to callbacks are processed here */
            TranslateMessage(&msg); /* convert virtual-key msgs to character msgs */
            DispatchMessage(&msg);
        }
    }
    return msg.wParam;
}

int
main()
{
    HANDLE ht_selfsuspend, ht_exit, ht_window;
    DWORD tid, res;

    INIT();

    print("creating window\n");
    ht_window = CreateThread(NULL, 0, window_func, NULL, 0, &tid);
    if (ht_window == NULL) {
        print("Error creating window thread\n");
        return -1;
    }
    while (!thread_ready)
        Sleep(20);

    print("detach_callback start\n");

    ht_selfsuspend = CreateThread(NULL, 0, &ThreadProcSelfSuspend, NULL, 0, &tid);
    if (ht_selfsuspend == NULL) {
        print("Error creating self-suspend thread\n");
        return -1;
    }
    /* wait for thread to suspend itself */
    res = 0;
    while (res == 0) {
        res = SuspendThread(ht_selfsuspend);
        if (res == 0) {
            /* Thread might not yet have gotten around to suspending itself */
            ResumeThread(ht_selfsuspend);
            /* short sleep to wait */
            Sleep(20);
        }
    }

    do_test(2);
    print("finished first callback test\n");

    action_exit = TRUE;
    ht_exit = CreateThread(NULL, 0, &ThreadProcDoTest, (void *)2, 0, &tid);
    if (ht_exit == NULL) {
        print("Error creating exit thread\n");
        return -1;
    }
    WaitForSingleObject(ht_exit, INFINITE);
    CloseHandle(ht_exit);
    print("finished exit test\n");
    action_exit = FALSE;

    action_detach = TRUE;
    do_test(2);
    print("finished detach test\n");
    action_detach = FALSE;
    /* we are now detached */

    /* just a little extra work to make sure everything looks ok natively */
    do_test(1);
    print("finished second callback test\n");

    /* verify selfsuspended thread detached okay */
    ResumeThread(ht_selfsuspend);
    ResumeThread(ht_selfsuspend);
    WaitForSingleObject(ht_selfsuspend, INFINITE);
    CloseHandle(ht_selfsuspend);

    print("detach_callback done\n");
    return 0;
}
