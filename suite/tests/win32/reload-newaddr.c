/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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

/* reload.c: tests repeatedly loads and unloads a dll, and executes from it
 * in between, to stress our cache management
 */

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include "tools.h"

/* make as STRESS=1 for a performance test */

#ifdef NIGHTLY_REGRESSION
/* don't ask to compute fact or fib too high */
#    define MAX_FACT_FIB 20
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

static SIZE_T
get_mem_usage()
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
    return mem.PeakPagefileUsage;
}

static void
check_mem_usage(SIZE_T peakpage)
{
    SIZE_T newpeak = get_mem_usage();
#if VERBOSE
    print("Pagefile usage increase is %d KB\n", (newpeak - peakpage) / 1024);
#endif
    /* Since under trace threshold increase shouldn't be too much -- I
     * see 20KB w/ TOT 11/20/06 vs 336KB for -no_free_unmapped_futures
     * and 88KB for -rct_sticky.
     * If we went up into the 100's of iters we could keep it even lower
     * but we want a short test.

     FIXME: coarse units makes the increase larger, and traces do make
     a big difference:
      % for i in "" "-disable_traces" "-no_coarse_units" "-no_coarse_units
    -disable_traces"; do useops $i; (for j in 40 60 80 100; do rundr rel
    win32/reload-newaddr.exe $j; done) 2>&1 | grep '^Pagefile usage increase'; done
      DYNAMORIO_OPTIONS=
      Pagefile usage increase is 68 KB
      Pagefile usage increase is 112 KB
      Pagefile usage increase is 136 KB
      Pagefile usage increase is 152 KB
      DYNAMORIO_OPTIONS=-disable_traces
      Pagefile usage increase is 48 KB
      Pagefile usage increase is 52 KB
      Pagefile usage increase is 56 KB
      Pagefile usage increase is 52 KB
      DYNAMORIO_OPTIONS=-no_coarse_units
      Pagefile usage increase is 20 KB
      Pagefile usage increase is 72 KB
      Pagefile usage increase is 108 KB
      Pagefile usage increase is 96 KB
      DYNAMORIO_OPTIONS=-no_coarse_units -disable_traces
      Pagefile usage increase is 28 KB
      Pagefile usage increase is 32 KB
      Pagefile usage increase is 36 KB
      Pagefile usage increase is 32 KB

    Strangely debug build uses less memory for coarse:
      % for i in "" "-disable_traces" "-no_coarse_units" "-no_coarse_units
    -disable_traces"; do useops $i; (for j in 40; do rundr dbg win32/reload-newaddr.exe
    $j; done) 2>&1 | grep '^Pagefile usage increase'; done DYNAMORIO_OPTIONS= Pagefile
    usage increase is 56 KB DYNAMORIO_OPTIONS=-disable_traces Pagefile usage increase is
    40 KB DYNAMORIO_OPTIONS=-no_coarse_units Pagefile usage increase is 24 KB
      DYNAMORIO_OPTIONS=-no_coarse_units -disable_traces
      Pagefile usage increase is 32 KB

     But it seems that we have to run a LOT of iters to really prove there's no leak:
       % for i in 10 100 200 500 1000 2000; do echo $i; (norio win32/reload-newaddr.exe
    $i; usetree coarse; useops -no_coarse_units; rio win32/reload-newaddr.exe $i; useops
    -desktop; rio win32/reload-newaddr.exe $i; useops; rio win32/reload-newaddr.exe $i)
    2>&1 | grep 'Peak pagefile' | sed 's/Peak pagefile usage://' | xargs | awk '{printf
    "native %4d; nocoarse %4d +%3d; desktop %4d +%3d; tot %4d +%3d\n", $1, $3, $3-$1, $5,
    $5-$1, $7, $7-$1}'; done 10 native  428; nocoarse 1588 +1160; desktop 1424 +996; tot
    1516 +1088 100 native  436; nocoarse 1740 +1304; desktop 1448 +1012; tot 1712 +1276
      200
      native  448; nocoarse 1760 +1312; desktop 1460 +1012; tot 1756 +1308
      500
      native  488; nocoarse 1804 +1316; desktop 1500 +1012; tot 1804 +1316
      1000
      native  548; nocoarse 1864 +1316; desktop 1560 +1012; tot 1868 +1320
      2000
      native  676; nocoarse 1988 +1312; desktop 1688 +1012; tot 1992 +1316

    So for now for short regression we allow the larger values we've seen.
    FIXME: have a long regr test that does hundreds of iters!
    */
    /* typical DR */
    /* FIXME: I used to have a <32KB category but I seem to have nondet
     * behavior, make vs raw being different, etc. etc. so just checking
     * for under 80KB
     */
#ifdef X64
    if (newpeak - peakpage < 160 * 1024)
        print("Memory check: pagefile usage increase is < 160 KB\n");
    else {
        /* give actual number here so we can see how high it is */
        print("Memory check: pagefile usage increase is %d KB >= 160 KB\n",
              (newpeak - peakpage) / 1024);
    }
#else
    if (newpeak - peakpage < 90 * 1024)
        print("Memory check: pagefile usage increase is < 90 KB\n");
    /* detect_dangling_fcache doesn't free fcache */
    else if (newpeak - peakpage < 120 * 1024)
        print("Memory check: pagefile usage increase is >= 90 KB, < 120 KB\n");
    else {
        /* give actual number here so we can see how high it is */
        print("Memory check: pagefile usage increase is %d KB >= 120 KB\n",
              (newpeak - peakpage) / 1024);
    }
#endif
}

static int
doload(int iter)
{
    HANDLE lib;
    int sum = 0;
    void *p;
    lib = LoadLibrary("win32.reload-newaddr.dll.dll");
    if (lib == NULL) {
        print("error loading library\n");
        return 0;
    } else {
        /* don't ask to compute fact or fib too high */
        BOOL(WINAPI * proc)(DWORD);
        proc = (BOOL(WINAPI *)(DWORD))GetProcAddress(lib, (LPCSTR) "import_me");
        sum += (*proc)(iter % MAX_FACT_FIB);
        FreeLibrary(lib);
        /* prevent loading at the same address again */
        p = VirtualAlloc(lib, 4 * 1024, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        /* deliberately do not free */
#if VERBOSE
        print("alloced " PFX " @ last loaded slot " PFX "\n", p, lib);
#endif
    }
    return sum;
}

int
main(int argc, char **argv)
{
    HANDLE event;
    uint i, res;
    ULONG_PTR hThread;
    uint iters = 0;
    int sum = 0;
    SIZE_T peakpage;

    USE_USER32();

    if (argc != 2) {
        iters = ITERS;
    } else {
        iters = atoi(argv[1]);
    }
    fprintf(stderr, "iters is %d\n", iters);

    event = CreateEvent(NULL, TRUE, FALSE, NULL);
    hThread = _beginthreadex(NULL, 0, run_func, event, 0, &i);

    for (i = 0; i < iters / 2; i++) {
        res = doload(i);
        if (res == 0)
            break;
        sum += res;
    }

    peakpage = get_mem_usage();

    for (i = iters / 2; i < iters; i++) {
        res = doload(i);
        if (res == 0)
            break;
        sum += res;
    }

    check_mem_usage(peakpage);

    SetEvent(event);
    WaitForSingleObject((HANDLE)hThread, INFINITE);

    print("sum=%d\n", sum);
    return 0;
}
