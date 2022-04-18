/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* reload-race.c: tests repeatedly loads and unloads a dll, and
 * executes from it in between in different tests - case 6293
 */

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include "tools.h"

#include <setjmp.h>

#define VERBOSE 0

#define UNIPROC

/* FIXME: we may even want to have a sleep */

/* for .B race we'd want to reach the unloaded module, note we provide
 * a .C opportunity by having the unloaded module call thread_yield()
 * and .E and .F violations on our IAT vs PLT calls.  (No explicit
 * function pointers necessary.)
 */

#ifdef UNIPROC
#    define YIELD() thread_yield() /* or do nothing on a multi processor */
#else
#    define YIELD() /* or do nothing on a multi processor */
#endif

/* Need to ensure we're doing the same amount of work - so we need to
 * count our races - we need edge detector whether we were in unloaded
 * area or in a good module NUM_TRANSITIONS counts how many should be
 * reproduced to give the race a chance.  (We can't detect whether a
 * B/C/E/F violation was suppressed though.)
 */

/* make as STRESS=1 for a performance test */

#ifdef NIGHTLY_REGRESSION
/* don't ask to compute fact or fib too high */
#    define MAX_FACT_FIB 8
#    define NUM_TRANSITIONS 10
#else
/* PERF or STRESS */
/* very low computation here */
#    define MAX_FACT_FIB 8
#    define NUM_TRANSITIONS 100
#endif

enum { LAST_OK, LAST_FAULT, LAST_START };

typedef int (*funptr)();

/* note we must have dynamically linked DLLs */
funptr import1;
funptr import2;
/* Note we may not want to use GetProcAddress() in target thread if
 * that is serialized with loader (and to allow for running -aslr 1 we
 * can't grab once only */

/* If a DLL shows up at the same address yet not its preferred base
 * and it has to be rebased, we'll also have bad execution before it
 * gets rebased, which is not likely to happen in real apps
 */

int transitions; /* atomic writes necessary */

jmp_buf mark;
int where; /* 0 = normal, 1 = segfault longjmp */

int sum1 = 0;
int sum2 = 0;
int done1 = 0;
int done2 = 0;

/* we create a 2nd thread to complicate DR cache flushing */
int WINAPI
run_func(void *arg)
{
    int last_good = LAST_START; /* so we can count proper transitions */
    int same_run = 0;

    /* need to run as long as necessary */
    while (transitions < NUM_TRANSITIONS) {
#if VERBOSE
        print("about to call\n");
#endif
        /* try to write to every single page */
        where = setjmp(mark);
        if (where == 0) {
            /* .B for target if it is unloaded in a race, as well as
             * .E .F on the way there should all be suppressed */

            /* note that the target itself may get a .C and a .B on
             * its way back from thread_yield()
             */

            if (same_run % 2 == 0)
                YIELD();
            /* .E shouldn't be seen here */
            sum1 += import1(transitions % MAX_FACT_FIB);
            done1++;
            if (same_run % 3 == 0)
                YIELD();
            /* .F shouldn't be seen here */
            sum2 += import2(transitions % MAX_FACT_FIB);
            done2++;
            /* note the above will be racy - e.g. not all transitions
             * will exercise both */
#if VERBOSE
            print("made it in and out on %d, same_run %d\n", transitions, same_run);
#endif
            if (last_good != LAST_OK) {
                last_good = LAST_OK;
                InterlockedIncrement(&transitions);
                same_run = 0;
            } else {
                same_run++;
            }
        } else {
            /* fault */
            if (last_good != LAST_FAULT) {
                last_good = LAST_FAULT;
                InterlockedIncrement(&transitions);
                same_run = 0;
            } else {
                same_run++;
            }
        }
    }

    print("made it in and out %d transitions\n", transitions);
#if VERBOSE
    print("import_me1 ran %d, sum %d\n", done1, sum1);
    print("import_me2 ran %d, sum %d\n", done2, sum2);
#endif

    WaitForSingleObject((HANDLE)arg, INFINITE);
    return 0;
}

/* make sure DR isn't using too much memory and is actually freeing fragments */
static void
check_mem_usage()
{
    VM_COUNTERS mem;
    get_process_mem_stats(GetCurrentProcess(), &mem);
#if VERBOSE
    print("Process Memory Statistics:\n");
    print("\tPeak virtual size:         %6d KB\n", mem.PeakVirtualSize / 1024);
    print("\tPeak working set size:     %6d KB\n", mem.PeakWorkingSetSize / 1024);
    print("\tPeak paged pool usage:     %6d KB\n", mem.QuotaPeakPagedPoolUsage / 1024);
    print("\tPeak non-paged pool usage: %6d KB\n", mem.QuotaPeakNonPagedPoolUsage / 1024);
    print("\tPeak pagefile usage:       %6d KB\n", mem.PeakPagefileUsage / 1024);
#endif
    /*
      native:
            Peak virtual size:           7772 KB
            Peak working set size:        996 KB
            Peak paged pool usage:          8 KB
            Peak non-paged pool usage:      1 KB
            Peak pagefile usage:          352 KB
    DR results with debug build, where library takes up more WSS (one
    reason we use pagefile usage as our discerning factor):
      -no_shared_deletion:
            Peak virtual size:         140388 KB
            Peak working set size:       7948 KB
            Peak paged pool usage:          9 KB
            Peak non-paged pool usage:      1 KB
            Peak pagefile usage:         6536 KB
      -no_syscalls_synch_flush:
            Peak virtual size:         140388 KB
            Peak working set size:       7784 KB
            Peak paged pool usage:          9 KB
            Peak non-paged pool usage:      1 KB
            Peak pagefile usage:         6368 KB
      -no_cache_shared_free_list:
            Peak virtual size:         140388 KB
            Peak working set size:       5156 KB
            Peak paged pool usage:          9 KB
            Peak non-paged pool usage:      1 KB
            Peak pagefile usage:         3736 KB
      defaults:
            Peak virtual size:         140388 KB
            Peak working set size:       3352 KB
            Peak paged pool usage:          9 KB
            Peak non-paged pool usage:      1 KB
            Peak pagefile usage:         1680 KB
    */
    /* native */
#if VERBOSE
    print("Pagefile usage is %d KB\n", mem.PeakPagefileUsage / 1024);
#endif
    if (mem.PeakPagefileUsage < 900 * 1024)
        print("Memory check: pagefile usage is < 900 KB\n");
    /* typical DR */
    else if (mem.PeakPagefileUsage < 2816 * 1024)
        print("Memory check: pagefile usage is >= 900 KB, < 2816 KB\n");
    /* prof_pcs uses a buffer the size of DR.dll */
    else if (mem.PeakPagefileUsage < 6000 * 1024)
        print("Memory check: pagefile usage is >= 2816 KB, < 6000 KB\n");
    /* detect_dangling_fcache doesn't free fcache */
    else if (mem.PeakPagefileUsage < 16384 * 1024)
        print("Memory check: pagefile usage is >= 6000 KB, < 16384 KB\n");
    else {
        /* give actual number here so we can see how high it is */
        print("Memory check: pagefile usage is %d KB >= 16384 KB\n",
              mem.PeakPagefileUsage / 1024);
    }
}

/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#if VERBOSE
        print("Got segfault\n");
#endif
        longjmp(mark, 1);
    }
#if VERBOSE
    print("Exception occurred, process about to die silently\n");
#endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}

int
main(int argc, char **argv)
{
    HANDLE lib;
    HANDLE event;
    ULONG_PTR hThread;
    int reloaded = 0;

    USE_USER32();

#ifdef UNIX
    /* though win32/ */
    intercept_signal(SIGSEGV, signal_handler);
#else
    /* note that can't normally if we have a debugger attached this
     * will not get this executed */
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#endif

    event = CreateEvent(NULL, TRUE, FALSE, NULL);
    hThread = _beginthreadex(NULL, 0, run_func, event, 0, NULL);

    /* need to run as long as necessary to hit the required faults */
    while (transitions < NUM_TRANSITIONS) {
        lib = LoadLibrary("win32.reload-race.dll.dll");
        if (lib == NULL) {
            print("error loading library\n");
            break;
        } else {
            reloaded++;
#if VERBOSE
            print("reloaded %d times %d\n", reloaded);
#endif
            import1 = (funptr)GetProcAddress(lib, (LPCSTR) "import_me1");
            import2 = (funptr)GetProcAddress(lib, (LPCSTR) "import_me2");
            /* may want to explicitly sleep here but that may be slower */
            if (reloaded % 2 == 0)
                YIELD();
            FreeLibrary(lib);
            if (reloaded % 3 == 0)
                YIELD();
        }
    }

    SetEvent(event);
    WaitForSingleObject((HANDLE)hThread, INFINITE);
    print("main loop done\n");
    check_mem_usage();
#if VERBOSE
    print("reloaded %d times %d\n", reloaded);
#endif

    return 0;
}
