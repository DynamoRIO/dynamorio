/* **********************************************************
 * Copyright (c) 2019-2021 Google, Inc.  All rights reserved.
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
#include "thread.h"

#ifdef WINDOWS
#    define TLS_ATTR __declspec(thread)
#    pragma warning(disable : 4100) /* Unreferenced formal parameter. */
#else
#    define TLS_ATTR __thread
#endif

/* Create a static TLS variable to create LdrpHandleTlsData triggers calling
 * NtSetInformationProcess.ProcessTlsInformation and exercising DR's handling
 * of the kernel modifying all thread's TLS (i#4136).
 */
#define STATIC_TLS_INIT_VAL 0xdeadbeef
static TLS_ATTR unsigned int static_tls_test = STATIC_TLS_INIT_VAL;

static
#ifdef WINDOWS
    unsigned int __stdcall
#else
    void *
#endif
    do_work(void *vargp)
{
    print("sum is %d\n", 7 + 7);
    if (static_tls_test != STATIC_TLS_INIT_VAL)
        print("incorrect static TLS value %d\n", static_tls_test);
    return IF_WINDOWS_ELSE(0, NULL);
}

int
main()
{
    /* Make some threads to help test client and private loader TLS. */
    thread_t thread = create_thread(do_work, NULL);
    join_thread(thread);
    thread = create_thread(do_work, NULL);
    join_thread(thread);
    return 0;
}
