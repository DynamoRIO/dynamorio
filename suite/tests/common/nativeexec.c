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

#ifndef ASM_CODE_ONLY

/* nativeexec.exe that calls routines in nativeexec.dll.dll via 
 * different call* constructions
 */
#include "tools.h"

/* from nativeexec.dll.dll */
IMPORT void import_me1(int x);
IMPORT void import_me2(int x);
IMPORT void import_me3(int x);

void call_plt(void (*fn)(int));
void call_funky(void (*fn)(int));

int
main(int argc, char **argv)
{
    INIT();

    if (argc > 2 && strcmp("-bind_now", argv[1])) {
#ifdef WINDOWS
        print("-bind_now is Linux-only\n");
#else
        /* Re-exec the test with LD_BIND_NOW in the environment to force eager
         * binding.
         */
        setenv("LD_BIND_NOW", "1", true/*overwrite*/);
        execv(argv[0], argv);
#endif
    }

    print("calling via IAT-style call\n");
    import_me1(57);

    /* XXX: Should assert that &import_me2 is within the bounds of the current
     * module, since that's what we want to test.
     */
    print("calling via PLT-style call\n");
    call_plt(&import_me2);

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
    call_funky(&import_me3);

    print("all done\n");

    return 0;
}

#else /* ASM_CODE_ONLY */

#include "asm_defines.asm"

START_FILE

        DECLARE_FUNC(call_plt)
GLOBAL_LABEL(call_plt:)
        /* XXX: Not doing SEH prologue for test code. */
        mov REG_XDX, ARG1           /* XDX is volatile and not regparm 0. */
        enter 0, 0                  /* Maintain 16-byte alignment. */
        CALLC1(plt_ind_call, 37)
        jmp after_plt_call
    plt_ind_call:
        jmp REG_XDX
    after_plt_call:
        leave
        ret
        END_FUNC(call_plt)

        DECLARE_FUNC(call_funky)
GLOBAL_LABEL(call_funky:)
        /* XXX: Not doing SEH prologue for test code. */
        mov REG_XDX, ARG1           /* XDX is volatile and not regparm 0. */
        enter 0, 0                  /* Maintain 16-byte alignment. */
        CALLC1(funky_ind_call, 17)
        jmp after_funky_call
    funky_ind_call:
        xor eax,eax
        push REG_XAX
        pop REG_XAX
        jmp REG_XDX
    after_funky_call:
        leave
        ret
        END_FUNC(call_funky)

END_FILE

#endif
