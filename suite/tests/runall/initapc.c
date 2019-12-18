/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* initapc: tests having DllMain of a statically-linked dll send an APC
 * (thus the APC is prior to the image entry point)
 *
 * must be run w/ AppInit injection, else the APC will be prior to DR taking over
 *
 * expect:
 * w/ -native_exec_syscalls:
 *   *** re-taking-over @INTERCEPT_SYSCALL after losing control ***
 * w/ -no_native_exec_syscalls:
 *   *** re-taking-over @INTERCEPT_EARLY_ASYNCH after losing control ***
 * and, of course, to have no .C violations from bottoming out
 *
 * FIXME: add mechanism to get info from core w/o getting start/stop and
 * w/o sending to eventlog since don't want customers to get even if
 * ask for info events!
 * Right now, since can't do DRview on test that completes right away,
 * and aren't getting info from core, this test will pass even if it runs
 * natively.
 */
#include <windows.h>
#include "tools.h"

/* from initapc-dll.dll */
__declspec(dllimport) import_me(int x);

int
main()
{
    INIT();

    print("initapc main()\n");
    import_me(37);

    /* ensure still checking ret-after-call after stack bottom issues w/ APC
     * use a nop between push and ret to avoid VB push/ret pattern match
     */
    __asm {
        push offset next_instr
        nop
        ret
    next_instr:
        nop
    }

    print("*** invalid ret allowed!\n");
    return 0;
}
