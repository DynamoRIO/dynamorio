/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test wrapping functionality using a library w/ exported routines
 * so they're easy for the client to locate.
 */

#include "tools.h"

static void
switch_modes(void)
{
    /* Signal the client to switch modes. */
#ifdef WINDOWS
    __nop();
    __nop();
    __nop();
    __nop();
#else
    __asm__ __volatile__("nop; nop; nop; nop");
#endif
}

int EXPORT
wrapme(int x)
{
    print("%s: arg %d\n", __FUNCTION__, x);
    if (x % 2 == 0) {
        /* Switch in the middle of a wrapped function. */
        switch_modes();
    }
    return x;
}

void
run_tests(void)
{
    /* First, a regular pre-and-post-wrapped instance. */
    print("first wrapme returned %d\n", wrapme(1));
    /* Now we'll have a pre but not post b/c we'll swap in the middle. */
    print("second wrapme returned %d\n", wrapme(2));
    /* Now we swap back and will have no pre or post. */
    print("third wrapme returned %d\n", wrapme(2));
    /* If we did not clean up drwrap state, we would see a pre and *two* posts as drwrap
     * tries to catch up to the interrupted state.  With the proper cleanup, we have
     * just one pre and one post.
     */
    print("fourth wrapme returned %d\n", wrapme(1));
}

#ifdef WINDOWS
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: run_tests(); break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}
#else
int __attribute__((constructor)) so_init(void)
{
    run_tests();
    return 0;
}
#endif
