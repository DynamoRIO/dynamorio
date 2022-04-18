/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

typedef void (*funcptr)();

/* PR 229292: we need to isolate this var to avoid flushes to it via
 * writes to other vars
 */
#pragma data_seg(".isolate")
char badfuncbuf[1000] = {
    0,
};
#pragma data_seg()
char *badfunc;

DWORD WINAPI
call_bad_code(LPVOID parm)
{
    EXCEPTION_RECORD exception;
    CONTEXT *context;

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
                exception = *((GetExceptionInformation())->ExceptionRecord),
                context = (GetExceptionInformation())->ContextRecord,
                print("DATA VIOLATION: Inside first filter eax=%x\n", context->CXT_XAX),
#ifdef DUMP_REGISTER_STATE
                dump_exception_info(&exception, context),
#else
                print("Address match : %s\n",
                      (exception.ExceptionAddress == (PVOID)badfunc &&
                       context->CXT_XIP == (ptr_uint_t)badfunc)
                          ? "yes"
                          : "no"),
                print("Exception match : %s\n",
                      (exception.ExceptionCode == 0xc0000005 &&
                       exception.ExceptionInformation[0] == 0 &&
                       exception.ExceptionInformation[1] == (ptr_uint_t)badfunc)
                          ? "yes"
                          : "no"),
#endif
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
    } __except (exception = *((GetExceptionInformation())->ExceptionRecord),
                context = (GetExceptionInformation())->ContextRecord,
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        print("DATA: Expected execution violation!\n");
#ifdef DUMP_REGISTER_STATE
        dump_exception_info(&exception, context);
#else
        print("Address match : %s\n",
              (exception.ExceptionAddress == (PVOID)badfunc &&
               context->CXT_XIP == (ptr_uint_t)badfunc)
                  ? "yes"
                  : "no");
        print("Exception match : %s\n",
              (exception.ExceptionCode == 0xc0000005 &&
               exception.ExceptionInformation[0] == 0 &&
               exception.ExceptionInformation[1] == (ptr_uint_t)badfunc)
                  ? "yes"
                  : "no");
#endif
    }
    print("DATA: After exception handler\n");
    return 0;
}

int
main()
{
    DWORD tid;
    HANDLE ht;

    badfunc = (char *)ALIGN_FORWARD(badfuncbuf, 512);

    /* FIXME: make this fancier */
    badfunc[0] = 0xc3; /* ret */

    print("THREAD0: Creating thread 1\n");
    ht = CreateThread(NULL, 0, &call_bad_code, NULL, 0, &tid);
    WaitForSingleObject(ht, INFINITE);
    print("THREAD0: After running other thread\n");

    call_bad_code(0);
    print("THREAD0: After calling more bad code here\n");
}
