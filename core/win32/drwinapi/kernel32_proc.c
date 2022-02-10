/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll process and thread redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "instrument.h"
#include "drwinapi_private.h"
#include "ntdll_redir.h"

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Processes and Threads
 */

/* The max is 4096 on Win10-1909 (and probably earlier) but we do not try to emulate
 * that maximum since we're using the limited FlsBitmapBits in the PEB still.
 */
#define FLS_MAX_COUNT 128

void
kernel32_redir_init_proc(void)
{
    PEB *peb = get_own_peb();
    /* i#3633: Implement FLS isolation for Win10 1903+ where FLS data is no longer
     * in the PEB.
     */
    ASSERT(get_os_version() < WINDOWS_VERSION_2003 ||
           (peb->FlsBitmap == NULL || peb->FlsBitmap->SizeOfBitMap == FLS_MAX_COUNT));
    /* We rely on -private_peb for FLS isolation.  Otherwise we'd have to
     * put back in place all the code to handle mixing private and app FLS
     * callbacks, and we'd have to tweak our FLS redirection.
     */
    ASSERT(INTERNAL_OPTION(private_peb));
}

void
kernel32_redir_exit_proc(void)
{
}

void
kernel32_redir_onload_proc(privmod_t *mod, strhash_table_t *kernel32_table)
{
    if (get_proc_address_ex(mod->base, "FlsAlloc", NULL) == NULL) {
        /* i#1385: msvc110+ calls GetProcAddress on FlsAlloc and we want it to
         * return NULL if there is no underlying FlsAlloc.
         */
        IF_DEBUG(bool found =)
        strhash_hash_remove(GLOBAL_DCONTEXT, kernel32_table, "FlsAlloc");
        ASSERT(found);
        /* i#2453: VS2013 checks other Fls routines as well so we clear them all. */
        IF_DEBUG(found =)
        strhash_hash_remove(GLOBAL_DCONTEXT, kernel32_table, "FlsFree");
        ASSERT(found);
        IF_DEBUG(found =)
        strhash_hash_remove(GLOBAL_DCONTEXT, kernel32_table, "FlsGetValue");
        ASSERT(found);
        IF_DEBUG(found =)
        strhash_hash_remove(GLOBAL_DCONTEXT, kernel32_table, "FlsSetValue");
        ASSERT(found);
    }
}

/***************************************************************************
 * PROCESSES
 */

HANDLE
WINAPI
redirect_GetCurrentProcess(VOID)
{
    return NT_CURRENT_PROCESS;
}

DWORD
WINAPI
redirect_GetCurrentProcessId(VOID)
{
    return (DWORD)get_process_id();
}

DECLSPEC_NORETURN
VOID WINAPI
redirect_ExitProcess(__in UINT uExitCode)
{
    dr_exit_process(uExitCode);
    ASSERT_NOT_REACHED();
}

/***************************************************************************
 * THREADS
 */

HANDLE
WINAPI
redirect_GetCurrentThread(VOID)
{
    return NT_CURRENT_THREAD;
}

DWORD
WINAPI
redirect_GetCurrentThreadId(VOID)
{
    return (DWORD)d_r_get_thread_id();
}

/***************************************************************************
 * FLS
 */

DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb)
{
    DWORD index;
    NTSTATUS res = redirect_RtlFlsAlloc(cb, &index);
    if (NT_SUCCESS(res))
        return index;
    else {
        set_last_error(ntstatus_to_last_error(res));
        return FLS_OUT_OF_INDEXES;
    }
}

BOOL WINAPI
redirect_FlsFree(DWORD index)
{
    NTSTATUS res = redirect_RtlFlsFree(index);
    if (NT_SUCCESS(res))
        return TRUE;
    else {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
}

PVOID WINAPI
redirect_FlsGetValue(DWORD index)
{
    TEB *teb = get_own_teb();
    if (index >= FLS_MAX_COUNT || teb->FlsData == NULL) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return NULL;
    } else {
        return teb->FlsData[index + TEB_FLS_DATA_OFFS];
    }
}

BOOL WINAPI
redirect_FlsSetValue(DWORD index, PVOID value)
{
    TEB *teb = get_own_teb();
    if (index >= FLS_MAX_COUNT) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (teb->FlsData == NULL) {
        NTSTATUS res = redirect_RtlProcessFlsData(0);
        if (!NT_SUCCESS(res)) {
            set_last_error(ntstatus_to_last_error(res));
            return FALSE;
        }
    }
    teb->FlsData[index + TEB_FLS_DATA_OFFS] = value;
    return TRUE;
}

/***************************************************************************
 * TESTS
 */

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_proc(void)
{
    print_file(STDERR, "testing drwinapi kernel32 control-related routines\n");

    EXPECT(redirect_GetCurrentProcess() == GetCurrentProcess(), true);
    EXPECT(redirect_GetCurrentProcessId() == GetCurrentProcessId(), true);
    EXPECT(redirect_GetCurrentThread() == GetCurrentThread(), true);
    EXPECT(redirect_GetCurrentThreadId() == GetCurrentThreadId(), true);
}
#endif
