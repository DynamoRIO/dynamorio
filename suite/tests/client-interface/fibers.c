/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2004 VMware, Inc.  All rights reserved.
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

static DWORD flsA_index;
static DWORD flsB_index;

static bool verbose = false;

static void WINAPI
fls_delete(void *fls_val)
{
    /* Called on DeleteFiber, thread exit, and FlsFree */
    print("fls_delete val=0x%08x\n", fls_val);
}

static void WINAPI
run_fibers(void *arg)
{
    void *fiber_worker = GetCurrentFiber();
    void *fiber_main = arg;
    if (FlsGetValue(flsA_index) == arg)
        print("bogus");
    print("in worker fiber\n");
    if (GetFiberData() != fiber_main)
        print("GetFiberData() mismatch\n");

    FlsSetValue(flsA_index, (void *)(ULONG_PTR)0xdeadbeef);
    FlsSetValue(flsB_index, (void *)(ULONG_PTR)0x12345678);
    print("for worker, set FLS to:\n");
    print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
    print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

    print("back to main\n");
    SwitchToFiber(fiber_main);

    print("in worker fiber again\n");
    print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
    print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

    /* We have to switch back -- else the thread exits */
    print("back to main\n");
    SwitchToFiber(fiber_main);
}

static void
fls_index_iter(void)
{
    /* To keep the test passing on machines with 128 and 4096 we walk up
     * and break when we hit a failure.
     */
#define FLS_MAX_COUNT 16 * 1024
    DWORD idx[FLS_MAX_COUNT];
    int i;
    bool ran_out = false;
    for (i = 0; i < FLS_MAX_COUNT; i++) {
        idx[i] = FlsAlloc(fls_delete);
        if (verbose)
            print("request %d => index %d\n", i, idx[i]);
        /* We need to update FLS_MAX_COUNT in kernel32_proc.c if the max ever
         * goes up.  Several slots should already be taken by static libc.
         * Update: The max on Win10 1909 (and probably earlier Win10) is 4096,
         * and the slots are no longer kept in the PEB.
         */
        if (!ran_out && idx[i] == FLS_OUT_OF_INDEXES) {
            print("ran out of FLS slots\n", i);
            ran_out = true;
            break;
        }
    }
    for (int j = 0; j < i; j++) {
        if (idx[j] != FLS_OUT_OF_INDEXES)
            FlsFree(idx[j]);
    }
}

int
main()
{
    int i;
    void *fiber_main, *fiber;
    INIT();

    fiber_main = ConvertThreadToFiber(NULL);
    print("in main fiber\n");

    flsA_index = FlsAlloc(fls_delete);
    flsB_index = FlsAlloc(fls_delete);
    print("uninit values:\n");
    print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
    print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

    /* Test FlsFree on non-NULL which should call the callback */
    FlsSetValue(flsA_index, (void *)(ULONG_PTR)0x12345678);
    FlsFree(flsA_index);
    flsA_index = FlsAlloc(fls_delete);

    FlsSetValue(flsA_index, (void *)(ULONG_PTR)0x12345678);
    FlsSetValue(flsB_index, (void *)(ULONG_PTR)0xdeadbeef);
    print("for main, set FLS to:\n");
    print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
    print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

    fls_index_iter();

    for (i = 0; i < 2; i++) {
        print("creating worker fiber %d\n", i);
        fiber = CreateFiber(0, run_fibers, fiber_main);

        print("switching to worker fiber first time\n");
        SwitchToFiber(fiber);
        print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
        print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

        print("switching to worker fiber second time\n");
        SwitchToFiber(fiber);
        print("  flsA = 0x%08x\n", FlsGetValue(flsA_index));
        print("  flsB = 0x%08x\n", FlsGetValue(flsB_index));

        print("deleting worker fiber %d\n", i);
        DeleteFiber(fiber);
    }

    print("all done\n");
    /* With VS2017 the main fiber's fls_delete is *not* called (natively), so we
     * explicitly delete it in order to match the test output.
     */
    DeleteFiber(fiber_main);
}
