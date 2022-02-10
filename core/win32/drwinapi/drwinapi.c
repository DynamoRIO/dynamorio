/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.   All rights reserved.
 * Copyright (c) 2009-2010 Derek Bruening   All rights reserved.
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

/* Shared redirection code for custom private library loader */

#include "kernel32_redir.h" /* must be included first */
#include "../globals.h"
#include "ntdll_redir.h"
#include "rpcrt4_redir.h"
#include "advapi32_redir.h"

#ifndef WINDOWS
#    error Windows-only
#endif

void
drwinapi_init(void)
{
    ntdll_redir_init();
    kernel32_redir_init();
    rpcrt4_redir_init();
    advapi32_redir_init();
}

void
drwinapi_exit(void)
{
    advapi32_redir_exit();
    rpcrt4_redir_exit();
    kernel32_redir_exit();
    ntdll_redir_exit();
}

void
drwinapi_onload(privmod_t *mod)
{
    if (mod->name == NULL)
        return;
    if (strcasecmp(mod->name, "kernel32.dll") == 0)
        kernel32_redir_onload(mod);
    else if (strcasecmp(mod->name, "rpcrt4.dll") == 0)
        rpcrt4_redir_onload(mod);
    else if (strcasecmp(mod->name, "advapi32.dll") == 0)
        advapi32_redir_onload(mod);
}

app_pc
drwinapi_redirect_imports(privmod_t *impmod, const char *name, privmod_t *importer)
{
    if (strcasecmp(impmod->name, "ntdll.dll") == 0) {
        return ntdll_redir_lookup(name);
    } else if (strcasecmp(impmod->name, "kernel32.dll") == 0 ||
               strcasecmp(impmod->name, "kernelbase.dll") == 0) {
        app_pc res = kernel32_redir_lookup(name);
        if (res == NULL) {
            /* win7 has some Reg* routines in kernel32 so we check advapi */
            res = advapi32_redir_lookup(name);
        }
        if (res != NULL && get_os_version() >= WINDOWS_VERSION_7 && importer != NULL &&
            strcasecmp(importer->name, "kernel32.dll") == 0) {
            /* We can't redirect kernel32.dll's calls to kernelbase when we ourselves
             * call the kernel32.dll routine when our redirection fails.
             *
             * XXX: we could add a 2nd return val from the lookup, but there are
             * only a few of these and as time goes forward this set should ideally
             * shrink to zero.  Thus we hardcode here.
             *
             * XXX: might some dlls import from kernelbase instead of kernel32 and
             * bypass our redirection altogether?  Yet another reason to eliminate
             * our redirection routines calling back into the priv libs.
             */
            if (strcmp(name, "GetModuleHandleA") == 0 ||
                strcmp(name, "GetModuleHandleW") == 0 ||
                strcmp(name, "GetProcAddress") == 0 ||
                strcmp(name, "LoadLibraryA") == 0 || strcmp(name, "LoadLibraryW") == 0)
                return NULL;
        }
        return res;
    } else if (strcasecmp(impmod->name, "rpcrt4.dll") == 0) {
        return rpcrt4_redir_lookup(name);
    } else if (strcasecmp(impmod->name, "advapi32.dll") == 0) {
        return advapi32_redir_lookup(name);
    }
    return NULL;
}

bool
drwinapi_redirect_getprocaddr(app_pc modbase, const char *name, app_pc *res_out OUT)
{
    privmod_t *mod;
    app_pc res = NULL;
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base((app_pc)modbase);
    if (mod != NULL) {
        const char *forwarder;
        res = drwinapi_redirect_imports(mod, name, NULL);
        /* I assume GetProcAddress returns NULL for forwarded exports? */
        if (res == NULL)
            res = (app_pc)get_proc_address_ex((app_pc)modbase, name, &forwarder);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s => " PFX "\n", __FUNCTION__, name, res);
    }
    release_recursive_lock(&privload_lock);
    *res_out = res;
    return (mod != NULL);
}

DWORD
ntstatus_to_last_error(NTSTATUS status)
{
    /* I don't want to rely on RtlNtStatusToDosError not working
     * at earliest init time or something so I'm doing my own mapping.
     */
    switch (status) {
    case STATUS_SUCCESS: return ERROR_SUCCESS;
    case STATUS_INVALID_HANDLE: return ERROR_INVALID_HANDLE;
    case STATUS_ACCESS_DENIED: return ERROR_ACCESS_DENIED;
    case STATUS_INVALID_PARAMETER: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_1: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_2: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_3: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_4: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_5: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_6: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_7: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_8: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_9: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_10: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_11: return ERROR_INVALID_PARAMETER;
    case STATUS_INVALID_PARAMETER_12: return ERROR_INVALID_PARAMETER;
    case STATUS_OBJECT_NAME_EXISTS: return ERROR_ALREADY_EXISTS;
    case STATUS_OBJECT_NAME_COLLISION: return ERROR_ALREADY_EXISTS;
    case STATUS_OBJECT_NAME_NOT_FOUND: return ERROR_FILE_NOT_FOUND;
    case STATUS_OBJECT_NAME_INVALID: return ERROR_INVALID_NAME;
    case STATUS_OBJECT_PATH_INVALID: return ERROR_BAD_PATHNAME;
    case STATUS_OBJECT_PATH_NOT_FOUND: return ERROR_PATH_NOT_FOUND;
    case STATUS_MAPPED_FILE_SIZE_ZERO: return ERROR_FILE_INVALID;
    case STATUS_INVALID_PAGE_PROTECTION: return ERROR_INVALID_PARAMETER;
    case STATUS_FILE_LOCK_CONFLICT: return ERROR_LOCK_VIOLATION;
    case STATUS_INVALID_FILE_FOR_SECTION: return ERROR_BAD_EXE_FORMAT;
    case STATUS_SECTION_TOO_BIG: return ERROR_NOT_ENOUGH_MEMORY;
    case STATUS_OBJECT_TYPE_MISMATCH: return ERROR_INVALID_HANDLE;
    case STATUS_BUFFER_OVERFLOW: return ERROR_MORE_DATA;
    case STATUS_NO_SUCH_FILE: return ERROR_FILE_NOT_FOUND;
    case STATUS_NO_MORE_FILES: return ERROR_NO_MORE_FILES;
    case STATUS_INFO_LENGTH_MISMATCH: return ERROR_BAD_LENGTH;
    case STATUS_NOT_MAPPED_DATA: return ERROR_INVALID_ADDRESS;
    case STATUS_THREAD_IS_TERMINATING: return ERROR_ACCESS_DENIED;
    case STATUS_PROCESS_IS_TERMINATING: return ERROR_ACCESS_DENIED;
    case STATUS_END_OF_FILE: return ERROR_HANDLE_EOF;
    case STATUS_PENDING: return ERROR_IO_PENDING;
    case STATUS_NOT_A_REPARSE_POINT: return ERROR_NOT_A_REPARSE_POINT;
    case STATUS_PIPE_NOT_AVAILABLE: return ERROR_PIPE_BUSY;
    /* XXX: add more.  Variations by function are rare and handled in callers. */
    default: return ERROR_INVALID_PARAMETER;
    }
}

BOOL WINAPI
redirect_ignore_arg0(void)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg4(void *arg1)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg8(void *arg1, void *arg2)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg12(void *arg1, void *arg2, void *arg3)
{
    return TRUE;
}

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_proc(void);
void
unit_test_drwinapi_kernel32_mem(void);
void
unit_test_drwinapi_kernel32_lib(void);
void
unit_test_drwinapi_kernel32_file(void);
void
unit_test_drwinapi_kernel32_sync(void);
void
unit_test_drwinapi_kernel32_misc(void);
void
unit_test_drwinapi_rpcrt4(void);
void
unit_test_drwinapi_advapi32(void);

void
unit_test_drwinapi(void)
{
    print_file(STDERR, "testing drwinapi\n");

    loader_init_prologue();                /* not called by standalone_init */
    loader_init_epilogue(GLOBAL_DCONTEXT); /* not called by standalone_init */

    unit_test_drwinapi_kernel32_proc();
    unit_test_drwinapi_kernel32_mem();
    unit_test_drwinapi_kernel32_lib();
    unit_test_drwinapi_kernel32_file();
    unit_test_drwinapi_kernel32_sync();
    unit_test_drwinapi_kernel32_misc();
    unit_test_drwinapi_rpcrt4();
    unit_test_drwinapi_advapi32();
    swap_peb_pointer(GLOBAL_DCONTEXT, false);
}
#endif
