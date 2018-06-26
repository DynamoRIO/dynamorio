/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
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

/* nativeterminate.exe that calls routines in nativeterminate.dll.dll
 * running in separate threads
 * reproduces case 5455 on Windows2000
 */
#include <windows.h>
#include "tools.h"
#include "thread.h"

/* from nativeterminate.dll.dll */
__declspec(dllimport) __stdcall import_me1(int x);
__declspec(dllimport) __stdcall import_me_die(int x);

int
main()
{
    INIT();

    print("calling via IAT-style call\n");
    import_me1(57);
    print("calling in a thread\n");
    join_thread(create_thread((unsigned int(__stdcall *)(void *))import_me1, NULL));

    print("calling in a thread that dies\n");
    join_thread(create_thread((unsigned int(__stdcall *)(void *))import_me_die, NULL));
    print("case 5455 regression passed\n");

    print("all done\n");
    return 0;
}

/*
$ useops -loglevel 1 -dumpcore_mask 253 -stderr_mask 21 -native_exec_list
nativeterminate.dll.dll; make win32/nativeterminate.runinjector
 * make sure correctly executed
 */
