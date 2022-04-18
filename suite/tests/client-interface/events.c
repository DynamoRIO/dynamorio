/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/*
 * API regression test that registers for all supported event callbacks
 * (except the nudge and security violation callback)
 * as a C++ application.
 */

#include "tools.h"

#ifdef WINDOWS
#    include <windows.h>
#    include <stdio.h>
#else
#    include <stdlib.h>
#    include <stdio.h>
#    include <string.h>
#    include <unistd.h>
#    include <dlfcn.h>
#    include <signal.h>
#    include <ucontext.h>
#    include <assert.h>
#    include <sys/wait.h> /* for wait */
#endif

#ifdef UNIX
static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGUSR1)
        print("Got SIGUSR1\n");
    else if (sig == SIGUSR2)
        print("Got SIGUSR2\n");
    else if (sig == SIGURG)
        print("Got SIGURG\n");
}
#endif /* UNIX */

int
main(int argc, char **argv)
{
#ifdef WINDOWS
    HMODULE hmod;

    /*
     * Cause an exception event
     */
    __try {
        HANDLE heap = GetProcessHeap();
        char *buf = (char *)HeapAlloc(((char *)heap) + 1, HEAP_GENERATE_EXCEPTIONS, 10);
    } __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
    }

    /*
     * Load and unload a module to cause a module unload event
     */
    hmod = LoadLibrary(argv[1]);
    if (hmod == NULL) {
        print("LoadLibrary failed: %x\n", GetLastError());
    }
    FreeLibrary(hmod);
#endif

#ifdef UNIX
    void *hmod;
    char buf[1000];
    size_t len = 0;
    char *end_path = NULL;
    /*
     * Load and unload a module to cause a module unload event
     */

    /* FIXME: We used to test a module with a large .bss here.  Try to do that
     * again.
     */
    hmod = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
    if (hmod != NULL)
        dlclose(hmod);
    else
        print("module load failed: %s\n", dlerror());

    /* test load of non-existent file */
    hmod = dlopen("foo_bar_no_exist.so", RTLD_LAZY | RTLD_LOCAL);
    if (hmod != NULL) {
        print("ERROR - module load of %s succeeded\n", buf);
        dlclose(hmod);
    }

    intercept_signal(SIGUSR1, signal_handler, false);
    intercept_signal(SIGUSR2, signal_handler, false);
    intercept_signal(SIGURG, signal_handler, false);
    print("Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);
    print("Sending SIGUSR2\n");
    kill(getpid(), SIGUSR2);
    print("Sending SIGURG\n");
    kill(getpid(), SIGURG);
    print("Done\n");
    /*
     * Cause a fork event
     */
    if (fork() == 0) {
        abort();
    } else {
        wait(NULL);
    }
#endif /* UNIX */

#ifdef WINDOWS
    /*
     * Cause an exception event, we test redirecting the application to redirect()
     */
    __try {
        HANDLE heap = GetProcessHeap();
        char *buf = (char *)HeapAlloc(((char *)heap) + 1, HEAP_GENERATE_EXCEPTIONS, 10);
    } __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
    }
#else
    *(int *)4 = 0;
#endif
    /* Never Reached */
    print("Shouldn't be reached\n");
}

#ifdef __cplusplus
extern "C"
#endif
#ifdef WINDOWS
    __declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
        void redirect(void)
{
    print("Redirect success!\n");
    exit(0);
}
