/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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

#include "tools.h"

static HANDLE ready, go;
static volatile LONG calls;

NOINLINE void EXPORT
late_target(void)
{
    InterlockedIncrement(&calls);
}

NOINLINE void EXPORT
fresh_target(void)
{
    InterlockedIncrement(&calls);
}

NOINLINE void EXPORT
wrap_and_wait(HANDLE event, bool register_late)
{
    InterlockedIncrement(&calls);
}

static DWORD WINAPI
worker(void *arg)
{
    assert(SetEvent(ready));
    assert(WaitForSingleObject(go, INFINITE) == WAIT_OBJECT_0);
    fresh_target();
    return 0;
}

int
main(int argc, char **argv)
{
    HANDLE thread;
    INIT();
    ready = CreateEvent(NULL, FALSE, FALSE, NULL);
    go = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(ready != NULL && go != NULL);
    late_target();
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    assert(thread != NULL);
    assert(WaitForSingleObject(ready, INFINITE) == WAIT_OBJECT_0);
    wrap_and_wait(go, argc > 1);
    assert(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    late_target();
    CloseHandle(thread);
    CloseHandle(ready);
    CloseHandle(go);
    print("callback and translation completed\n");
    return 0;
}
