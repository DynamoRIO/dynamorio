/* **********************************************************
 * Copyright (c) 2015-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

#define _CRT_SECURE_NO_WARNINGS 1
#include "tools.h" /* for print() */
#include <stdint.h>

#include <setjmp.h>
static SIGJMP_BUF mark;

static char app_handler_message[1024];

/* borrowed from core/ */
static inline generic_func_t
convert_data_to_function(void *data_ptr)
{
#ifdef WINDOWS
#    pragma warning(push)
#    pragma warning(disable : 4055)
#endif
    return (generic_func_t)data_ptr;
#ifdef WINDOWS
#    pragma warning(pop)
#endif
}

#if defined(UNIX)
#    include <signal.h>

static void
handle_sigsegv(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    /* Unfortunately the kernel does not fill in siginfo->si_addr for exec faults! */
    sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
    snprintf(app_handler_message, BUFFER_SIZE_ELEMENTS(app_handler_message),
             "app handler got signal %d with addr " PFX, signal, (ptr_uint_t)sc->SC_XIP);
    NULL_TERMINATE_BUFFER(app_handler_message);
    SIGLONGJMP(mark, 1);
}

#elif defined(WINDOWS)
#    include <windows.h>

static LONG WINAPI
handle_exception(struct _EXCEPTION_POINTERS *exception_pointers)
{
    PEXCEPTION_RECORD exception_record = exception_pointers->ExceptionRecord;
    DWORD exception_code = exception_record->ExceptionCode;
    void *fault_address = (void *)exception_record->ExceptionInformation[1];
    _snprintf(app_handler_message, BUFFER_SIZE_ELEMENTS(app_handler_message),
              "app handler got exception %x with addr " PFX, exception_code,
              (ptr_uint_t)fault_address);
    NULL_TERMINATE_BUFFER(app_handler_message);
    SIGLONGJMP(mark, 1);
}
#endif

static void
execute_from(void *address)
{
    generic_func_t f = convert_data_to_function(address);
    strncpy(app_handler_message, "app handler was not called",
            BUFFER_SIZE_ELEMENTS(app_handler_message));
    NULL_TERMINATE_BUFFER(app_handler_message);
    if (SIGSETJMP(mark) == 0) {
        f();
    }
    print("%s\n", app_handler_message);
}

int
main(int argc, char *argv[])
{
#if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_sigsegv, false);
#elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception);
#endif

    print("executing from illegal addresses\n");
    execute_from((void *)(uintptr_t)42);
    execute_from((void *)(uintptr_t)77);

    print("all done\n");
    return 0;
}
