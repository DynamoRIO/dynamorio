/* **********************************************************
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

#define _WIN32_WINNT 0x0400

#include <windows.h>
#include "tools.h"

/* case 1543 - Fibers on Win2003 RAC false positive */

typedef struct {
    LPVOID pFiberUI;
} FIBERINFO, *PFIBERINFO;

FIBERINFO g_FiberInfo;

void WINAPI
FiberFunc(PVOID pvParam)
{

    PFIBERINFO pFiberInfo = (PFIBERINFO)pvParam;

    print("in worker fiber\n");
    GetCurrentFiber();
    if (GetFiberData() != pvParam) {
        print("GetFiberData() mismatch!\n");
        abort();
    }

    print("back to main\n");
    SwitchToFiber(pFiberInfo->pFiberUI);

    print("in worker fiber again\n");

    // Reschedule the UI thread. When the UI thread is running
    // and has no events to process, the thread is put to sleep.
    // NOTE: If we just allow the fiber function to return,
    // the thread and the UI fiber die -- we don't want this!
    SwitchToFiber(pFiberInfo->pFiberUI);
    print("SHOULD NOT GET HERE!\n");

    /* map user32.dll for RunAll testing */
    MessageBeep(0);
}

int
main()
{
    int i;
    PVOID pFiberCounter = NULL;
    INIT();

    print("in main thread\n");

    g_FiberInfo.pFiberUI = ConvertThreadToFiber(NULL);

    print("main thread converted to fiber\n");

    for (i = 0; i < 2; i++) {
        print("creating worker fiber %d\n", i);
        pFiberCounter = CreateFiber(0, FiberFunc, &g_FiberInfo);

        print("switching to worker fiber first time\n");
        SwitchToFiber(pFiberCounter);

        print("switching to worker fiber second time\n");
        SwitchToFiber(pFiberCounter);

        print("deleting worker fiber %d\n", i);
        DeleteFiber(pFiberCounter);
    }

    print("all done\n");
}
