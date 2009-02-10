/* **********************************************************
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

/* nativeexec.exe that calls routines in nativeexec.dll.dll via 
 * different call* constructions
 */
#include <windows.h>
#include "tools.h"

#ifdef USE_DYNAMO
#include "dynamorio.h"
#endif

/* from nativeexec.dll.dll */
__declspec(dllimport) import_me1(int x);
__declspec(dllimport) import_me2(int x);
__declspec(dllimport) import_me3(int x);

int
main()
{
#ifdef USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    INIT();

    print("calling via IAT-style call\n");
    import_me1(57);

    print("calling via PLT-style call\n");
    __asm {
        push 37
        call plt_ind_call
        jmp after_plt_call
    plt_ind_call:
        jmp dword ptr [import_me2]
    after_plt_call:
        add esp,4
    }

    /* funky ind call is only caught by us w/ -native_exec_guess_calls
     * FIXME: add a -no_native_exec_guess_calls runregression run
     * for that run:
     *    FIXME: assert curiosity in debug run, would like to add to template!
     *    FIXME: have way for nativeexec.dll.c to know whether native or not?
     *      call DR routine?
     *      then can have release build die too
     *
     *    06:13 PM ~/dr/suite/tests
     *    % useops -no_native_exec_guess_calls
     *    % make win32/nativeexec.runinjector
     *    PASS
     *    % make DEBUG=yes win32/nativeexec.runinjector
     *    > <CURIOSITY : false && "inside native_exec dll" in file x86/interp.c line 1967
     */

    print("calling via funky ind call\n");
    __asm {
        push 17
        call funky_ind_call
        jmp after_funky_call
    funky_ind_call:
        xor eax,eax
        push eax
        pop eax
        jmp dword ptr [import_me3]
    after_funky_call:
        add esp,4
    }

    print("all done\n");

#ifdef USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif
    return 0;
}
