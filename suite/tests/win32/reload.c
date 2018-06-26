/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

/* reload.c: tests repeatedly loads and unloads a dll, and executes from it
 * in between, to stress our cache management
 */

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include "tools.h"

/* make as STRESS=1 for a performance test */

#ifdef NIGHTLY_REGRESSION
/* don't ask to compute fact or fib too high */
#    define MAX_FACT_FIB 30
#    define ITERS (2 * MAX_FACT_FIB)
#else
/* PERF or STRESS */
/* very low computation here */
#    define MAX_FACT_FIB 8
#    define ITERS 4000
#endif
#define VERBOSE 0

/* we create a 2nd thread to complicate DR cache flushing */
int WINAPI
run_func(void *arg)
{
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
    /* detect_dangling_fcache doesn't free fcache */
    /* But there's a lot of variation across machines so we try to make
     * this test less flaky:
     */
    else if (mem.PeakPagefileUsage < 16384 * 1024)
        print("Memory check: pagefile usage is >= 2816 KB, < 16384 KB\n");
    else {
        /* give actual number here so we can see how high it is */
        print("Memory check: pagefile usage is %d KB >= 16384 KB\n",
              mem.PeakPagefileUsage / 1024);
    }
}

int
main(int argc, char **argv)
{
    HANDLE lib;
    HANDLE event;
    int i;
    ULONG_PTR hThread;
    int sum = 0;

    USE_USER32();

    event = CreateEvent(NULL, TRUE, FALSE, NULL);
    hThread = _beginthreadex(NULL, 0, run_func, event, 0, &i);

    for (i = 0; i < ITERS; i++) {
        lib = LoadLibrary("win32.reload.dll.dll");
        if (lib == NULL) {
            print("error loading library\n");
            break;
        } else {
            /* don't ask to compute fact or fib too high */
            BOOL(WINAPI * proc)(DWORD);
            proc = (BOOL(WINAPI *)(DWORD))GetProcAddress(lib, (LPCSTR) "import_me");
            sum += (*proc)(i % MAX_FACT_FIB);
            FreeLibrary(lib);
        }
    }

    SetEvent(event);
    WaitForSingleObject((HANDLE)hThread, INFINITE);

    print("sum=%d\n", sum);
    check_mem_usage();
    return 0;
}
