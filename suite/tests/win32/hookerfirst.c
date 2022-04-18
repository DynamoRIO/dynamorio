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

/* hookerfirst: tests having DllMain of a statically-linked dll
 * (thus prior to the image entry point) hook ntdll.dll
 * and then unhook in main() of the executable
 *
 * must be run w/ drinject injection, else we'd have to make sure
 * user32 is loaded after the DLL.
 *
 * case 2525
 *3) Ent hooks first
 *  We hook -- need to chain -- mangle their call
 *  Ent unhooks (dynamic off) -- need to unchain by emulating their write and then
 *restoring they could come back -- then follow 1)
 *
 *4) Ent hooks first
 *  We hook -- need to chain
 *  we unhook (detach) -- need to unchain - unmangle their call
 *  ent unhooks -- should be fine
 *
 */
#include <windows.h>
#include "tools.h"

#ifdef USE_DYNAMO
#    include "dynamorio.h"
#endif

/* from hookerfirst.dll */
__declspec(dllimport) hookit(int x);

__declspec(dllimport) unhookit(int x);

int
badfunc(void)
{
    __asm {
        push offset next_instr
        nop
        ret
    next_instr:
        nop
    }
}

int
main()
{
    INIT();

    print("hookerfirst main()\n");
    /* TODO: at this point we may want to detach in some cases - so
     * should figure out how to trigger detach on error here.
     */

    /* this is for testing with -internal_detach 0x2 -- we'd need to detach ourselves
     * cleanly */

    /* ensure still checking ret-after-call
     * use a nop between push and ret to avoid VB push/ret pattern match
     */
    __try {
        badfunc();
        print("*** invalid ret allowed!\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("invalid ret caught\n");
    }

    /* this is tested with -no_ret_after_call */
    unhookit(37);

    hookit(37);
    unhookit(37);

    return 0;
}
