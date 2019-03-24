/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll synchronization redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "ntdll_redir.h"
#include "drwinapi_private.h"

VOID WINAPI
redirect_InitializeCriticalSection(__out LPCRITICAL_SECTION lpCriticalSection)
{
    NTSTATUS res = redirect_RtlInitializeCriticalSection(lpCriticalSection);
    /* The man page for RtlInitializeCriticalSection implies it doesn't set
     * any error codes, but it seems reasonable to do so, esp on NULL being
     * passed in or sthg.
     */
    if (!NT_SUCCESS(res))
        set_last_error(ntstatus_to_last_error(res));
}

BOOL WINAPI
redirect_InitializeCriticalSectionAndSpinCount(__out LPCRITICAL_SECTION lpCriticalSection,
                                               __in DWORD dwSpinCount)
{
    NTSTATUS res =
        redirect_RtlInitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_InitializeCriticalSectionEx(__out LPCRITICAL_SECTION lpCriticalSection,
                                     __in DWORD dwSpinCount, __in DWORD Flags)
{
    NTSTATUS res =
        redirect_RtlInitializeCriticalSectionEx(lpCriticalSection, dwSpinCount, Flags);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

VOID WINAPI
redirect_DeleteCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection)
{
    redirect_RtlDeleteCriticalSection(lpCriticalSection);
}

VOID WINAPI
redirect_EnterCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection)
{
    /* XXX: invoking ntdll routine b/c DR is already doing so.
     * We've seen some alloc/free mismatches in Initialize and Delete
     * though (DrMem i#333, DR i#963) so be on the lookout.
     */
    RtlEnterCriticalSection(lpCriticalSection);
}

VOID WINAPI
redirect_LeaveCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection)
{
    /* XXX: invoking ntdll routine b/c DR is already doing so.
     * We've seen some alloc/free mismatches in Initialize and Delete
     * though (DrMem i#333, DR i#963) so be on the lookout.
     */
    RtlLeaveCriticalSection(lpCriticalSection);
}

LONG WINAPI
redirect_InterlockedCompareExchange(__inout __drv_interlocked LONG volatile *Destination,
                                    __in LONG ExChange, __in LONG Comperand)
{
    return _InterlockedCompareExchange(Destination, ExChange, Comperand);
}

LONG WINAPI
redirect_InterlockedDecrement(__inout __drv_interlocked LONG volatile *Addend)
{
    return _InterlockedDecrement(Addend);
}

LONG WINAPI
redirect_InterlockedExchange(__inout __drv_interlocked LONG volatile *Target,
                             __in LONG Value)
{
    return _InterlockedExchange(Target, Value);
}

LONG WINAPI
redirect_InterlockedIncrement(__inout __drv_interlocked LONG volatile *Addend)
{
    return _InterlockedIncrement(Addend);
}

DWORD
WINAPI
redirect_WaitForSingleObject(__in HANDLE hHandle, __in DWORD dwMilliseconds)
{
    NTSTATUS res;
    LARGE_INTEGER li;
    LARGE_INTEGER *timeout;
    if (dwMilliseconds == INFINITE)
        timeout = NULL;
    else {
        li.QuadPart = -((int)dwMilliseconds * TIMER_UNITS_PER_MILLISECOND);
        timeout = &li;
    }
    /* XXX: are there special handles we need to convert to real handles? */
    res = NtWaitForSingleObject(hHandle, FALSE /*!alertable*/, timeout);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return WAIT_FAILED;
    }
    if (res == STATUS_TIMEOUT)
        return WAIT_TIMEOUT;
    else if (res == STATUS_WAIT_0)
        return WAIT_OBJECT_0;
    else if (res == STATUS_ABANDONED_WAIT_0)
        return WAIT_ABANDONED;
    else
        return res; /* WAIT_ success codes tend to match STATUS_ values */
}

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_sync(void)
{
    CRITICAL_SECTION sec;
    BOOL ok;
    LONG volatile vol;
    LONG val1, val2, res;
    HANDLE e;
    DWORD dw;
    LARGE_INTEGER li;

    print_file(STDERR, "testing drwinapi kernel32 sync-related routines\n");

    /* We just ensure everything runs */
    redirect_InitializeCriticalSection(&sec);
    redirect_DeleteCriticalSection(&sec);
    ok = redirect_InitializeCriticalSectionAndSpinCount(&sec, 0);
    EXPECT(ok, TRUE);
    redirect_DeleteCriticalSection(&sec);
    ok = redirect_InitializeCriticalSectionEx(&sec, 0,
                                              RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO);
    EXPECT(ok, TRUE);
    redirect_EnterCriticalSection(&sec);
    redirect_LeaveCriticalSection(&sec);
    redirect_DeleteCriticalSection(&sec);

    vol = 4;
    val1 = 5;
    val2 = 6;
    res = redirect_InterlockedCompareExchange(&vol, val1, val2);
    EXPECT(res == vol && res == 4, true);
    vol = 4;
    val1 = 5;
    val2 = 4;
    res = redirect_InterlockedCompareExchange(&vol, val1, val2);
    EXPECT(res == 4 && vol == val1, true);

    vol = 42;
    res = redirect_InterlockedDecrement(&vol);
    EXPECT(res == 41 && vol == 41, true);

    vol = 42;
    val1 = 37;
    res = redirect_InterlockedExchange(&vol, val1);
    EXPECT(res == 42 && vol == val1, true);

    vol = 42;
    res = redirect_InterlockedIncrement(&vol);
    EXPECT(res == 43 && vol == 43, true);

    e = CreateEvent(NULL, TRUE, FALSE, "myevent");
    EXPECT(e != NULL, true);
    dw = redirect_WaitForSingleObject(e, 50);
    EXPECT(dw == WAIT_TIMEOUT, true);
    ok = SetEvent(e);
    EXPECT(ok, true);
    dw = redirect_WaitForSingleObject(e, 50);
    EXPECT(dw == WAIT_OBJECT_0, true);
    ok = redirect_CloseHandle(e);
    EXPECT(ok, true);

    /* Test INFINITE wait (i#1467) */
    e = CreateWaitableTimer(NULL, TRUE, "mytimer");
    EXPECT(e != NULL, true);
    li.QuadPart = -(50 * TIMER_UNITS_PER_MILLISECOND);
    ok = SetWaitableTimer(e, &li, 0, NULL, NULL, FALSE);
    EXPECT(ok, true);
    dw = redirect_WaitForSingleObject(e, INFINITE);
    EXPECT(dw == WAIT_OBJECT_0, true);
    ok = redirect_CloseHandle(e);
    EXPECT(ok, true);
}
#endif
