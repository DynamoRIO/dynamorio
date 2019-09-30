/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

#define _CRT_SECURE_NO_WARNINGS 1
#include <stdlib.h>
#include "tools.h" /* for print() */

#include <setjmp.h>
static SIGJMP_BUF mark;

EXPORT volatile void *expected_fault_address;
static volatile void *dummy_value;
static char app_handler_message[1024];
static bool abort_on_segv = false;

#if defined(UNIX)
#    include <signal.h>

static void
handle_sigsegv(int signal, siginfo_t *siginfo, void *context)
{
    void *fault_address = siginfo->si_addr;
    if (signal == SIGSEGV && fault_address == expected_fault_address) {
        strncpy(app_handler_message, "app handler ok",
                BUFFER_SIZE_ELEMENTS(app_handler_message));
        NULL_TERMINATE_BUFFER(app_handler_message);
        if (abort_on_segv) {
            print("app handler aborting\n");
            abort();
        }
    } else {
        snprintf(app_handler_message, BUFFER_SIZE_ELEMENTS(app_handler_message),
                 "app handler got signal %d with addr " PFX
                 ", but expected signal %d with addr " PFX,
                 signal, (ptr_uint_t)fault_address, SIGSEGV,
                 (ptr_uint_t)expected_fault_address);
        NULL_TERMINATE_BUFFER(app_handler_message);
    }
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
    if (exception_code == EXCEPTION_ACCESS_VIOLATION &&
        fault_address == expected_fault_address) {
        strncpy(app_handler_message, "app handler ok",
                BUFFER_SIZE_ELEMENTS(app_handler_message));
        NULL_TERMINATE_BUFFER(app_handler_message);
        if (abort_on_segv) {
            print("app handler aborting\n");
            return EXCEPTION_EXECUTE_HANDLER;
        }
    } else {
        _snprintf(app_handler_message, BUFFER_SIZE_ELEMENTS(app_handler_message),
                  "app handler got exception %x with addr " PFX
                  ", but expected exception %x with addr " PFX,
                  exception_code, (ptr_uint_t)fault_address, EXCEPTION_ACCESS_VIOLATION,
                  (ptr_uint_t)expected_fault_address);
        NULL_TERMINATE_BUFFER(app_handler_message);
    }
    SIGLONGJMP(mark, 1);
}
#endif

static void
access_memory(void *address, bool write, void *fault_address)
{
    expected_fault_address = fault_address;
    strncpy(app_handler_message, "app handler was not called",
            BUFFER_SIZE_ELEMENTS(app_handler_message));
    NULL_TERMINATE_BUFFER(app_handler_message);
    if (SIGSETJMP(mark) == 0) {
        if (write) {
            *(volatile void **)address = NULL;
        } else {
            dummy_value = *(volatile void **)address;
        }
    }
    print("%s\n", app_handler_message);
}

int
main(int argc, char *argv[])
{
    char *p, *base;
    int i;

#if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_sigsegv, false);
#elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception);
#endif

    /* Allocate two contiguous pages.
     * Make the first one readable and writable,
     * and the second one neither readable nor writable.
     */
    p = allocate_mem((int)PAGE_SIZE * 2, ALLOW_READ | ALLOW_WRITE);
    if (p == NULL) {
        print("allocate_mem() failed\n");
        abort();
    }
    if (!ALIGNED(p, PAGE_SIZE)) {
        print("allocate_mem() returned memory that is not page aligned\n");
        abort();
    }
    protect_mem(p + PAGE_SIZE, PAGE_SIZE, 0);

    print("accessing the first page\n");
    base = p + PAGE_SIZE - sizeof(void *);
    access_memory(base, false, NULL);
    access_memory(base, true, NULL);

    /* i#1045: verify that memory accesses spanning page boundaries
     * are handled correctly.
     */
    print("accessing both pages\n");
    for (i = 1; i < sizeof(void *); i++) {
        print("i=%d\n", i);
        access_memory(base + i, 0, p + PAGE_SIZE);
        access_memory(base + i, 1, p + PAGE_SIZE);
    }

    print("accessing the second page\n");
    for (i = sizeof(void *); i < 2 * sizeof(void *); i++) {
        print("i=%d\n", i);
        access_memory(base + i, 0, base + i);
        access_memory(base + i, 1, base + i);
    }

    print("accessing NULL\n");
    abort_on_segv = true;
    expected_fault_address = NULL;
    *(volatile int *)NULL = 4;
    print("SHOULD NEVER GET HERE\n");

    return 0;
}
