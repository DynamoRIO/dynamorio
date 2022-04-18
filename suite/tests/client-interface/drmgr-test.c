/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#include "tools.h"

#ifdef WINDOWS
/* based on suite/tests/win32/callback.c */

#    include <windows.h>
#    include <stdio.h>

static volatile bool thread_ready = false;
static volatile bool past_crash = false;
static uint last_received = 0;
static HWND hwnd;
static uint msgdata;

static const UINT MSG_CUSTOM = WM_APP + 1;
static const LRESULT MSG_SUCCESS = 1;

static const WPARAM WP_NOP = 0;
static const WPARAM WP_EXIT = 1;
static const WPARAM WP_CRASH = 3;

static const ptr_uint_t BAD_WRITE = 0x40;

#    ifndef WM_DWMNCRENDERINGCHANGED
#        define WM_DWMNCRENDERINGCHANGED 0x031F
#    endif

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
#    ifdef X64
            cross_cb_seh_supported = false;
#    else
            cross_cb_seh_supported = (get_windows_version() < WINDOWS_VERSION_7 ||
                                      !is_wow64(GetCurrentProcess()));
#    endif
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
main(int argc, char **argv)
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

    char ALIGN_VAR(64) buffer[2048];
    _xsave(buffer, -1);

    print("All done\n");

    HMODULE hmod;

    /*
     * Load and unload a module to cause a module unload event
     */
    hmod = LoadLibrary(argv[1]);
    FreeLibrary(hmod);

    return 0;
}

/***************************************************************************/
#else
/* based on suite/tests/pthreads/pthreads.c */

#    include <stdio.h>
#    include <stdlib.h>
#    include <pthread.h>
#    include <dlfcn.h>
#    include <signal.h>

volatile double pi = 0.0;  /* Approximation to pi (shared) */
pthread_mutex_t pi_lock;   /* Lock for above */
volatile double intervals; /* How many intervals? */

void *
process(void *arg)
{
    char *id = (char *)arg;
    register double width, localsum;
    register int i;
    register int iproc;

    iproc = (*((char *)arg) - '0');

    /* Set width */
    width = 1.0 / intervals;

    /* Do the local computations */
    localsum = 0;
    for (i = iproc; i < intervals; i += 2) {
        register double x = (i + 0.5) * width;
        localsum += 4.0 / (1.0 + x * x);
    }
    localsum *= width;

    /* Lock pi for update, update it, and unlock */
    pthread_mutex_lock(&pi_lock);
    pi += localsum;
    pthread_mutex_unlock(&pi_lock);

    return (NULL);
}

int
main(int argc, char **argv)
{
    pthread_t thread0, thread1;
    void *retval;
    char table[2] = { 'A', 'B' };
#    ifdef X86
    char ch;
    /* Test xlat for drutil_insert_get_mem_addr,
     * we do not bother to run this test on Windows side.
     */
    __asm("mov %1, %%" IF_X64_ELSE("rbx", "ebx") "\n\t"
                                                 "mov $0x1, %%eax\n\t"
                                                 "xlat\n\t"
                                                 "movb %%al, %0\n\t"
          : "=m"(ch)
          : "g"(&table)
          : "%eax", "%ebx");
    print("%c\n", ch);
    /* XXX: should come up w/ some clever way to ensure
     * this gets the right address: for now just making sure
     * it doesn't crash.
     */
#    else
    print("%c\n", table[1]);
#    endif

#    ifdef X86
    /* Test xsave for drutil_opnd_mem_size_in_bytes. We're assuming that
     * xsave support is available and enabled, which should be the case
     * on all machines we're running on.
     * Ideally we'd run whatever cpuid invocations are needed to figure out the exact
     * size but 16K is more than enough for the foreseeable future: it's 576 bytes
     * with SSE and ~2688 for AVX-512.
     */
    char ALIGN_VAR(64) buffer[16 * 1024];
    __asm("xor %%edx, %%edx\n\t"
          "or $-1, %%eax\n\t"
          "xsave %0\n\t"
          : "=m"(buffer)
          :
          : "eax", "edx", "memory");
#    endif

    /* Test rep string expansions. */
    char buf1[1024];
    char buf2[1024];
#    ifdef X86_64
    __asm("lea %[buf1], %%rdi\n\t"
          "lea %[buf2], %%rsi\n\t"
          "mov %[count], %%ecx\n\t"
          "rep movsq\n\t"
          :
          : [ buf1 ] "m"(buf1), [ buf2 ] "m"(buf2), [ count ] "i"(sizeof(buf1))
          : "ecx", "rdi", "rsi", "memory");
#    elif defined(X86_32)
    __asm("lea %[buf1], %%edi\n\t"
          "lea %[buf2], %%esi\n\t"
          "mov %[count], %%ecx\n\t"
          "rep movsd\n\t"
          :
          : [ buf1 ] "m"(buf1), [ buf2 ] "m"(buf2), [ count ] "i"(sizeof(buf1))
          : "ecx", "edi", "esi", "memory");
#    endif

    intervals = 10;
    /* Initialize the lock on pi */
    pthread_mutex_init(&pi_lock, NULL);

    /* Make the two threads */
    if (pthread_create(&thread0, NULL, process, (void *)"0") ||
        pthread_create(&thread1, NULL, process, (void *)"1")) {
        print("%s: cannot make thread\n", argv[0]);
        exit(1);
    }

    /* Join (collapse) the two threads */
    if (pthread_join(thread0, &retval) || pthread_join(thread1, &retval)) {
        print("%s: thread join failed\n", argv[0]);
        exit(1);
    }

    void *hmod;
    hmod = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
    if (hmod != NULL)
        dlclose(hmod);
    else
        print("module load failed: %s\n", dlerror());

    /* Print the result */
    print("Estimation of pi is %16.15f\n", pi);

    /* Let's raise a signal */
    raise(SIGUSR1);
    return 0;
}
#endif /* !WINDOWS */
