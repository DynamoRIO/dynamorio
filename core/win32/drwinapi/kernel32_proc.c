/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.   All rights reserved.
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

#define FLS_MAX_COUNT 128

/* If you add any new priv invocation pointer here, update the list in
 * drwinapi_redirect_imports().
 */
static DWORD (WINAPI *priv_kernel32_FlsAlloc)(PFLS_CALLBACK_FUNCTION);

/* Support for running private FlsCallback routines natively */
typedef struct _fls_cb_t {
    PFLS_CALLBACK_FUNCTION cb;
    struct _fls_cb_t *next;
} fls_cb_t;

static fls_cb_t *fls_cb_list; /* in .data, so we have a permanent head node */
DECLARE_CXTSWPROT_VAR(static mutex_t privload_fls_lock,
                      INIT_LOCK_FREE(privload_fls_lock));

void
kernel32_redir_init_proc(void)
{
    PEB *peb = get_own_peb();
    /* use permanent head node to avoid .data unprot */
    ASSERT(fls_cb_list == NULL);
    fls_cb_list = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fls_cb_t, ACCT_OTHER, PROTECTED);
    fls_cb_list->cb = NULL;
    fls_cb_list->next = NULL;

    ASSERT(peb->FlsBitmap->SizeOfBitMap == FLS_MAX_COUNT);
}

void
kernel32_redir_exit_proc(void)
{
    mutex_lock(&privload_fls_lock);
    while (fls_cb_list != NULL) {
        fls_cb_t *cb = fls_cb_list;
        fls_cb_list = fls_cb_list->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, cb, fls_cb_t, ACCT_OTHER, PROTECTED);
    }
    mutex_unlock(&privload_fls_lock);
    DELETE_LOCK(privload_fls_lock);
}

void
kernel32_redir_onload_proc(privmod_t *mod, strhash_table_t *kernel32_table)
{
    priv_kernel32_FlsAlloc = (DWORD (WINAPI *)(PFLS_CALLBACK_FUNCTION))
        get_proc_address_ex(mod->base, "FlsAlloc", NULL);
    if (priv_kernel32_FlsAlloc == NULL) {
        /* i#1385: msvc110+ calls GetProcAddress on FlsAlloc and we want it
         * return NULL if there is no underlying FlsAlloc.
         */
        IF_DEBUG(bool found =)
            strhash_hash_remove(GLOBAL_DCONTEXT, kernel32_table, "FlsAlloc");
        ASSERT(found);
    }
}

/***************************************************************************
 * PROCESSES
 */

HANDLE
WINAPI
redirect_GetCurrentProcess(
    VOID
    )
{
    return NT_CURRENT_PROCESS;
}

DWORD
WINAPI
redirect_GetCurrentProcessId(
    VOID
    )
{
    return (DWORD) get_process_id();
}

DECLSPEC_NORETURN
VOID
WINAPI
redirect_ExitProcess(
    __in UINT uExitCode
    )
{
#ifdef CLIENT_INTERFACE
    dr_exit_process(uExitCode);
#else
    os_terminate_with_code(get_thread_private_dcontext(), /* dcontext is required */
                           TERMINATE_CLEANUP|TERMINATE_PROCESS, uExitCode);
#endif
    ASSERT_NOT_REACHED();
}

/***************************************************************************
 * THREADS
 */

HANDLE
WINAPI
redirect_GetCurrentThread(
    VOID
    )
{
    return NT_CURRENT_THREAD;
}

DWORD
WINAPI
redirect_GetCurrentThreadId(
    VOID
    )
{
    return (DWORD) get_thread_id();
}

/***************************************************************************
 * FLS
 */

/* A LIST_ENTRY is stored at the start of TEB.FlsData */
#define FLS_DATA_OFFS (sizeof(LIST_ENTRY)/sizeof(void*))

bool
kernel32_redir_fls_cb(dcontext_t *dcontext, app_pc pc)
{

    fls_cb_t *e, *prev;
    bool redirected = false;
    mutex_lock(&privload_fls_lock);
    for (e = fls_cb_list, prev = NULL; e != NULL; prev = e, e = e->next) {
        LOG(GLOBAL, LOG_LOADER, 2,
            "%s: comparing cb "PFX" to pc "PFX"\n",
            __FUNCTION__, e->cb, pc);
        if (e->cb != NULL/*head node*/ && (app_pc)e->cb == pc) {
            priv_mcontext_t *mc = get_mcontext(dcontext);
            void *arg = NULL;
            app_pc retaddr;
            redirected = true;
            /* Extract the retaddr and the arg to the callback */
            if (!safe_read((app_pc)mc->xsp, sizeof(retaddr), &retaddr)) {
                /* in debug we'd assert in vmareas anyway */
                ASSERT_NOT_REACHED();
                redirected = false; /* in release we'll interpret the routine I guess */
            }
#ifdef X64
            arg = (void *) mc->xcx;
#else
            if (!safe_read((app_pc)mc->xsp + XSP_SZ, sizeof(arg), &arg))
                ASSERT_NOT_REACHED(); /* we'll still redirect and call w/ NULL */
#endif
            if (redirected) {
                LOG(GLOBAL, LOG_LOADER, 2,
                    "%s: native call to FLS cb "PFX", redirect to "PFX"\n",
                    __FUNCTION__, pc, retaddr);
                (*e->cb)(arg);
                /* This is stdcall so clean up the retaddr + param */
                mc->xsp += XSP_SZ IF_NOT_X64(+ sizeof(arg));
                /* Now we interpret from the retaddr */
                dcontext->next_tag = retaddr;
            }
            /* If we knew the reason for this call we would know whether to remove
             * from the list: for thread exit, leave entry, but for FlsExit, remove.
             * Since we don't know we just leave it.
             */
            break;
        }
    }
    mutex_unlock(&privload_fls_lock);
    return redirected;
}

DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb)
{
    if (!INTERNAL_OPTION(private_peb)) {
        /* XXX i#1063: we should be able to get rid of all of this code that
         * handles private FLS callbacks being invoked from app code, if we're
         * willing to require -private_peb.  Removing this will also help
         * earliest injection by not needing the app's kernel32.  For now I'm
         * leaving this code (though the other Fls* routines are intercepted so
         * this is not complete).
         */
        ASSERT(priv_kernel32_FlsAlloc != NULL);
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
            in_private_library((app_pc)cb)) {
            fls_cb_t *entry = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fls_cb_t,
                                              ACCT_OTHER, PROTECTED);
            mutex_lock(&privload_fls_lock);
            entry->cb = cb;
            /* we have a permanent head node to avoid .data unprot */
            entry->next = fls_cb_list->next;
            fls_cb_list->next = entry;
            mutex_unlock(&privload_fls_lock);
            /* ensure on DR areas list: won't already be only for client lib */
            dynamo_vm_areas_lock();
            if (!is_dynamo_address((app_pc)cb)) {
                add_dynamo_vm_area((app_pc)cb, ((app_pc)cb)+1, MEMPROT_READ|MEMPROT_EXEC,
                                   true _IF_DEBUG("fls cb in private lib"));
                /* we do not ever remove: not worth refcount effort, and probably
                 * good to catch future executions
                 */
            }
            dynamo_vm_areas_unlock();
            LOG(GLOBAL, LOG_LOADER, 2, "%s: cb="PFX"\n", __FUNCTION__, cb);
        }
        return (*priv_kernel32_FlsAlloc)(cb);
    } else {
        DWORD index;
        NTSTATUS res = redirect_RtlFlsAlloc(cb, &index);
        if (NT_SUCCESS(res))
            return index;
        else {
            set_last_error(ntstatus_to_last_error(res));
            return FLS_OUT_OF_INDEXES;
        }
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
