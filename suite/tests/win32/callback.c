/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include "tools.h"
#include <stdio.h>

static bool thread_ready = false;
static bool past_crash = false;
static uint last_received = 0;
static HWND hwnd;
static uint msgdata;

static const UINT MSG_CUSTOM = WM_APP + 1;
static const LRESULT MSG_SUCCESS = 1;

static const WPARAM WP_NOP = 0;
static const WPARAM WP_EXIT = 1;
static const WPARAM WP_CRASH = 3;

static const ptr_uint_t BAD_WRITE = 0x40;

#ifndef WM_DWMNCRENDERINGCHANGED
#    define WM_DWMNCRENDERINGCHANGED 0x031F
#endif

/* This is where all our callbacks come.  We get 4 default messages:
 *   WM_GETMINMAXINFO                0x0024
 *   WM_NCCREATE                     0x0081
 *   WM_NCCALCSIZE                   0x0083
 *   WM_CREATE                       0x0001
 * and then our 2 custom messages that we send.
 *
 * On Windows 7 we also get (i#520):
 *   WM_DWMNCRENDERINGCHANGED        0x031F
 * and we avoid printing anything about it to simplify the test suite.
 */
LRESULT CALLBACK
wnd_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool cross_cb_seh_supported;
    if (message == MSG_CUSTOM) {
        print("in wnd_callback " PFX " %d %d\n", message, wParam, lParam);
        if (wParam == WP_CRASH) {
            /* ensure SendMessage returns prior to our crash */
            ReplyMessage(TRUE);
            print("About to crash\n");
#ifdef X64
            cross_cb_seh_supported = false;
#else
            cross_cb_seh_supported = (get_windows_version() < WINDOWS_VERSION_7 ||
                                      !is_wow64(GetCurrentProcess()));
#endif
            if (!cross_cb_seh_supported) {
                /* FIXME i#266: even natively this exception is not making it across
                 * the callback boundary!  Is that a fundamental limitation of the
                 * overly-structured SEH64?  32-bit SEH has no problem.  Neither does
                 * WOW64, except on win7+.
                 * For now we have a local try/except.
                 */
                __try {
                    *((int *)BAD_WRITE) = 4;
                    print("Should not get here\n");
                } __except (/* Only catch the bad write, to not mask DR errors (like
                             * case 10579) */
                            (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION &&
                             (GetExceptionInformation())
                                     ->ExceptionRecord->ExceptionInformation[0] ==
                                 1 /* write */
                             && (GetExceptionInformation())
                                     ->ExceptionRecord->ExceptionInformation[1] ==
                                 BAD_WRITE)
                                ? EXCEPTION_EXECUTE_HANDLER
                                : EXCEPTION_CONTINUE_SEARCH) {
                    print("Inside handler\n");
                    past_crash = true;
                }
            } else {
                *((int *)BAD_WRITE) = 4;
                print("Should not get here\n");
            }
        }
        return MSG_SUCCESS;
    } else {
        /* lParam varies so don't make template nondet */
        if (message != WM_DWMNCRENDERINGCHANGED)
            print("in wnd_callback " PFX " %d\n", message, wParam);
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

int WINAPI
run_func(void *arg)
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
    /* deliberately not calling ShowWindow */

    /* For case 10579 we want a handled system call in this thread prior
     * to our crash inside a callback */
    VirtualAlloc(0, 1024, MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    thread_ready = true;
    while (true) {
        __try {
            if (GetMessage(&msg, NULL, 0, 0)) {
                /* Messages not auto-sent to callbacks are processed here */
                if ((msg.message != MSG_CUSTOM || msg.wParam != WP_NOP) &&
                    msg.message != WM_DWMNCRENDERINGCHANGED) {
                    print("Got message " PFX " %d %d\n", msg.message, msg.wParam,
                          msg.lParam);
                }
                last_received = msg.message;
                if (msg.message == MSG_CUSTOM && msg.wParam == WP_EXIT)
                    break;              /* Done */
                TranslateMessage(&msg); /* convert virtual-key msgs to character msgs */
                DispatchMessage(&msg);
            }
        } __except (/* Only catch the bad write, to not mask DR errors (like
                     * case 10579) */
                    (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION &&
                     (GetExceptionInformation())
                             ->ExceptionRecord->ExceptionInformation[0] == 1 /* write */
                     && (GetExceptionInformation())
                             ->ExceptionRecord->ExceptionInformation[1] == BAD_WRITE)
                        ? EXCEPTION_EXECUTE_HANDLER
                        : EXCEPTION_CONTINUE_SEARCH) {
            /* This should have crossed the callback boundary
             * On xpsp2 and earlier we never see a callback return for
             * the crashing callback, while on 2k3sp1 we do see one.
             */
            print("Inside handler\n");
            past_crash = true;
            continue;
        }
    }
    return (int)msg.wParam;
}

int
main()
{
    int tid;
    HANDLE hThread;
    uint msgnum = 0;

    print("About to create thread\n");
    hThread = CreateThread(NULL, 0, run_func, NULL, 0, &tid);
    if (hThread == NULL) {
        print("Error creating thread\n");
        return -1;
    }
    while (!thread_ready)
        Sleep(0);

    /* We have to send a message to a window to get a callback.
     * We go ahead and use the blocking SendMessage for simplicity;
     * could use SendMessageCallback and get a callback back, but have to
     * ask for messages to receive it and then have no clear exit path.
     */
    if (SendMessage(hwnd, MSG_CUSTOM, WP_CRASH, msgnum++) != MSG_SUCCESS) {
        print("Error %d posting window message\n", GetLastError());
        return -1;
    }
    /* On bucephalus (win2k3sp1) I need to send a message to get the
     * thread to go into the except block: it sits waiting in the
     * kernel at the NtCallbackReturn from
     * KiUserCallbackExceptionHandler, and that is where it receives
     * the callback for this message: seems problematic natively?
     */
    PostThreadMessage(tid, MSG_CUSTOM, WP_NOP, msgnum++);
    while (!past_crash)
        Sleep(0);
    if (SendMessage(hwnd, MSG_CUSTOM, WP_NOP, msgnum++) != MSG_SUCCESS) {
        print("Error %d posting window message\n", GetLastError());
        return -1;
    }

    /* Message not sent to a window is processed inside the GetMessage loop,
     * with no callback involved.  So this bit here is mainly to get the thread
     * to exit. */
    if (!PostThreadMessage(tid, MSG_CUSTOM, WP_EXIT, msgnum++)) {
        print("Error %d posting thread message\n", GetLastError());
        return -1;
    }
    while (last_received != MSG_CUSTOM)
        Sleep(0);

    WaitForSingleObject(hThread, INFINITE);
    print("All done\n");
    return 0;
}
