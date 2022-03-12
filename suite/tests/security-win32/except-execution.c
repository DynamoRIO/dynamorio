/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#include "tools.h"
#include <windows.h>

#include "except.h"

/* We used to have a custom stack allocated at a particular address.
 * We used a raw NtCreateThread with associated code borrowed from the core
 * that used this stack.  But this doesn't work on modern Windows, and at
 * least for instrumentation modes we actively support today, there seems to
 * be no need for all of that: so we just run the code on the initial stack.
 */

typedef void (*funcptr)();

char *badfunc;

void
run_test()
{
    EXCEPTION_RECORD exception;
    CONTEXT *context;
    /* allocate on the stack for more reliable addresses for template
     * than if in data segment
     */
    char badfuncbuf[1000];

    badfunc = (char *)ALIGN_FORWARD(badfuncbuf, 256);

    /* FIXME: make this fancier */
    badfunc[0] = 0xc3; /* ret */

    /* FIXME: move this to a general win32/ test */
    /* Verifying RaiseException, FIXME: maybe move to a separate unit test */
    __try {
        ULONG arguments[] = { 0, 0xabcd };
        initialize_registry_context();
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 2, arguments);
        print("Never after RaiseException\n");
    } __except (print("In RaiseException filter\n"),
#ifdef RAISE_EXCEPTION_EIP_NOT_CONSTANT
                dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                    (GetExceptionInformation())->ContextRecord),
#endif
                EXCEPTION_EXECUTE_HANDLER) {
        print("In RaiseException handler\n");
    }

    /* FIXME: move this to a general win32/ test except-unreadable.c
          see case 197 and should also cover a direct CTI to bad memory,
          the latter not very likely in real apps though */

    /* These invalid address exceptions are to unreadable memory
       and we should try to transparently return normal exceptions
       [most likely these will remain unhandled]

       We want to make sure people get the seriousness of the situation
       and every case in which this happens should be taken as a real attack vector.
       Usually they would see this as a bug,
       but it may also be an attacker probing with AAAA.

       Therefore, we should let execution continue only in -detect_mode
       (fake) exceptions should be generated as if generated normally
    */
    __try {
        __try {
            __try {
                initialize_registry_context();
                __asm {
                    mov   eax, 0xbadcdef0
                    call  dword ptr [eax]
                }
                print("At statement after exception\n");
            } __except (context = (GetExceptionInformation())->ContextRecord,
                        print("Inside first filter eax=%x\n", context->CXT_XAX),
                        dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                            context),
                        context->CXT_XAX = 0xcafebabe,
                        /* FIXME: should get a CONTINUE to work  */
                        EXCEPTION_CONTINUE_EXECUTION, EXCEPTION_EXECUTE_HANDLER) {
                print("Inside first handler\n");
            }
            print("At statement after 1st try-except\n");
            __try {
                initialize_registry_context();
                __asm {
                    mov   edx, 0xdeadbeef
                    call  edx
                }
                print("NEVER Inside 2nd try\n");
            } __except (print("Inside 2nd filter\n"), EXCEPTION_CONTINUE_SEARCH) {
                print("This should NOT be printed\n");
            }
        } __finally {
            print("Finally!\n");
        }
        print("NEVER At statement after 2nd try-finally\n");
    } __except (print("Inside 3rd filter\n"),
                exception = *(GetExceptionInformation())->ExceptionRecord,
                context = (GetExceptionInformation())->ContextRecord,
                dump_exception_info(&exception, context),
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        /* copy of exception can be used */
        print("Expected memory access violation, ignoring it!\n");
    }
    print("After exception handler\n");

    /* Security violation handled by exceptions here */
    /* Now we should start getting different results with code origins,
       because the addresses are normally readable, but now we impose our own policy

       -detect_mode should behave normally
          no exceptions are triggered

       -throw_exception -no_detect_mode
          should behave similarly to the handling of unreadable memory above
          (fake) exceptions should be generated claiming that badfunc is not executable
     */

    print("Attempting execution of badfunc\n");
    __try {
        __try {
            __try {
                initialize_registry_context();
                ((funcptr)badfunc)();
                print("DATA: At statement after exception\n");
            } __except (
                context = (GetExceptionInformation())->ContextRecord,
                print("DATA VIOLATION: Inside first filter eax=%x\n", context->CXT_XAX),
                dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                    context),
                EXCEPTION_EXECUTE_HANDLER) {
                print("DATA VIOLATION: Inside first handler\n");
            }
            print("DATA: At statement after 1st try-except\n");
            __try {
                initialize_registry_context();
                ((funcptr)badfunc)();
                print("DATA: Inside 2nd try\n");
            } __except (EXCEPTION_CONTINUE_SEARCH) {
                print("DATA: This should NOT be printed\n");
            }
        } __finally {
            print("DATA: Finally!\n");
        }
        print("DATA: At statement after 2nd try-finally\n");
    } __except (exception = *(GetExceptionInformation())->ExceptionRecord,
                context = (GetExceptionInformation())->ContextRecord,
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        print("DATA: Expected execution violation!\n");
        dump_exception_info(&exception, context);
    }
    print("DATA: After exception handler\n");
}

int
thread_func()
{
    __try {
        run_test();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("Should never have exception bubble up to thread function\n");
    }
    /* thread is hanging so just exit here */
    ExitThread(0);
    return 0;
}

int
main()
{
    thread_func();

    return 0;
}
