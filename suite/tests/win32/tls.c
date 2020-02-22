/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include <stdio.h>
#include "tools.h"

#define VERBOSE 0

/* As a nice benefit of tools.h now including globals_shared.h, we have
 * the NUDGE_ defines already here.
 */

byte *
get_nudge_target()
{
    /* read DR marker
     * just hardcode the offsets for now
     */
    static const uint DR_NUDGE_FUNC_OFFSET = IF_X64_ELSE(0x28, 0x20);
    return get_drmarker_field(DR_NUDGE_FUNC_OFFSET);
}

typedef unsigned int(__stdcall *threadfunc_t)(void *);

static bool test_tls = false;
#define TLS_SLOTS 64
static bool tls_own[TLS_SLOTS];

byte *
get_own_teb()
{
    return (byte *)IF_X64_ELSE(__readgsqword, __readfsdword)(IF_X64_ELSE(0x30, 0x18));
}

int WINAPI
thread_func(void *arg)
{
    int i;
    while (!test_tls)
        ; /* spin */
    for (i = 0; i < TLS_SLOTS; i++) {
        if (tls_own[i]) {
            if (TlsGetValue(i) != 0) {
                print("TLS slot %d is " PFX " when it should be 0!\n", i, TlsGetValue(i));
            }
        }
    }
    print("Done testing tls slots\n");
    return 0;
}

int
main()
{
    int tid;
    HANDLE detach_thread;
    HANDLE my_thread;
    byte *nudge_target;
    int i;
    INIT();

    my_thread = (HANDLE)_beginthreadex(NULL, 0, thread_func, NULL, 0, &tid);

    nudge_target = get_nudge_target();
    if (nudge_target == NULL) {
        print("Cannot find DRmarker -- are you running natively?\n");
    } else {
        /* This test needs to do some work after detaching.  We exploit a
         * hole in DR by creating a thread that directly targets the DR
         * detach routine.  Hopefully this will motivate us to close
         * the hole (case 552) :)
         * Update: rather than use raw system calls, which are complex across
         * Windows versions and duplicate code with core/, we use a high-level
         * thread creation API.  DR does detect and stop this, but we have a
         * relaxation for this test in DEBUG.
         * The alternative is to create a new runall type that detaches from
         * the outside and then waits a while, but would be hard to time.
         */
        nudge_arg_t *arg = (nudge_arg_t *)VirtualAlloc(NULL, sizeof(nudge_arg_t),
                                                       MEM_COMMIT, PAGE_READWRITE);
        arg->version = NUDGE_ARG_CURRENT_VERSION;
        arg->nudge_action_mask = NUDGE_GENERIC(detach);
        arg->flags = 0;
        arg->client_arg = 0;
        print("About to detach using underhanded methods\n");
        detach_thread =
            (HANDLE)_beginthreadex(NULL, 0, (threadfunc_t)nudge_target, arg, 0, &tid);
        assert(detach_thread != NULL);
        WaitForSingleObject(detach_thread, INFINITE);

        assert(get_nudge_target() == NULL);
        print("Running natively now\n");
    }

    for (i = 0; i < TLS_SLOTS; i++) {
        /* case 8143: a runtime-loaded dll calling TlsAlloc needs to set
         * a value for already-existing threads.  The "official" method
         * is to directly TlsGetValue() and if not NULL assume that dll
         * has already set that value.  Our detach needs to clear values
         * to ensure this.  We have the simplest test possible here, of course.
         * We do need another thread as TlsAlloc seems to clear the slot for
         * the current thread.
         * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/using_thread_local_storage_in_a_dynamic_link_library.asp
         */
        unsigned long tlshandle = 0UL;
        tlshandle = TlsAlloc();
        if (tlshandle == TLS_OUT_OF_INDEXES) {
            break;
        }
#if VERBOSE
        print("handle %d is %d\n", i, tlshandle);
#endif
        assert(tlshandle >= 0);
        /* we only want teb slots */
        if (tlshandle >= TLS_SLOTS)
            break;
        tls_own[tlshandle] = true;
    }

    /* tell thread to GO */
    test_tls = true;

    WaitForSingleObject(my_thread, INFINITE);

    return 0;
}
