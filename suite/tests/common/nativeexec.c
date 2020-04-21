/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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

/* nativeexec.exe that calls routines in nativeexec.appdll.dll via
 * different call* constructions
 */
#    include "tools.h"
#    include "dr_annotations.h"

#    include <setjmp.h>

typedef void (*int_fn_t)(int);
typedef int (*int2_fn_t)(int, int);
typedef void (*tail_caller_t)(int_fn_t, int);

/* from nativeexec.appdll.dll */
IMPORT void
import_me1(int x);
IMPORT void
import_me2(int x);
IMPORT void
import_me3(int x);
IMPORT void
import_me4(int_fn_t fn, int x);
IMPORT int
import_ret_imm(int x, int y);
#    if 0
IMPORT void *tail_caller(int_fn_t, int);
#    endif

/* Test unwinding across back_from_native retaddrs. */
IMPORT void
unwind_level1(int_fn_t fn, int x);
static void
unwind_setjmp(int x);
IMPORT void
unwind_level3(int_fn_t fn, int x);
static void
unwind_level4(int x);
IMPORT void
unwind_level5(int_fn_t fn, int x);
static void
unwind_longjmp(int x);

void
call_plt(int_fn_t fn);
void
call_funky(int_fn_t fn);
int
call_ret_imm(int2_fn_t fn);

void
print_int(int x)
{
    print("nativeexec.exe:print_int(%d) %sunder DR\n", x,
          DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO() ? "" : "not ");
}

static jmp_buf jump_buf;

void
unwind_setjmp(int x)
{
    if (setjmp(jump_buf)) {
        print("after longjmp\n");
    } else {
        unwind_level3(unwind_level4, x - 1);
    }
}

void
unwind_level4(int x)
{
    unwind_level5(unwind_longjmp, x - 1);
}

void
unwind_longjmp(int x)
{
    print("before longjmp, %d\n", x);
    longjmp(jump_buf, 1);
}

#    define NUM_ITERS 10
#    define MALLOC_SIZE 8
void
loop_test(void)
{
    int i, j;
    void *volatile ptr;
    for (i = 0; i < NUM_ITERS; i++) {
        for (j = 0; j < NUM_ITERS; j++) {
            ptr = malloc(MALLOC_SIZE);
            free(ptr);
        }
    }
}
#    undef NUM_ITERS
#    undef MALLOC_SIZE

int
main(int argc, char **argv)
{
    int x;

    INIT();

    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        print("Running under DR\n");
    else
        print("Not running under DR\n");

    if (argc > 2 && strcmp("-bind_now", argv[1])) {
#    ifdef WINDOWS
        print("-bind_now is Linux-only\n");
#    else
        /* Re-exec the test with LD_BIND_NOW in the environment to force eager
         * binding.
         */
        setenv("LD_BIND_NOW", "1", true /*overwrite*/);
        execv(argv[0], argv);
#    endif
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
     *    <CURIOSITY : false && "inside native_exec dll" in file x86/interp.c:1967
     */

    print("calling via funky ind call\n");
    call_funky(&import_me3);

    print("calling nested native\n");
    import_me4(print_int, 42);

    print("calling cross-module unwinder\n");
    unwind_level1(unwind_setjmp, 3);

    print("calling indirect ret_imm\n");
    x = call_ret_imm(import_ret_imm);
    print(" -> %d\n", x);

#    if 0
    /* i#1077: If the appdll is native, DR will lose control in tail_caller's
     * asm code "jmp $xax". DR may gain control back from the mangled retaddr,
     * but unless we can mangle the $xax, DR still loses control, so disable
     * it for now.
     */
    print("calling tail caller\n");
    tail_caller(print_int, 35);
#    endif

    print("calling loop_test\n");
    loop_test();

    /* i#2372: make sure to verify we did not lose control! */
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        print("Running under DR\n");
    else
        print("Not running under DR\n");
    print("all done\n");

    return 0;
}

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

/* TODO i#3966: Maintain 16-byte alignment for 32-bit too in these routines. */
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

        DECLARE_FUNC(call_ret_imm)
GLOBAL_LABEL(call_ret_imm:)
        /* XXX: Not doing SEH prologue for test code. */
        mov      REG_XDX, ARG1          /* XDX is volatile and not regparm 0. */
        enter    0, 0                   /* Maintain 16-byte alignment. */
        /* Do a callee cleanup style call that uses ret imm similar to stdcall
         * from win32.
         */
        push     19
        push     21
        call     REG_XDX
        leave
        ret
        END_FUNC(call_ret_imm)

END_FILE
/* clang-format on */

#endif
