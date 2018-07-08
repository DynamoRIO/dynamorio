/* **********************************************************
 * Copyright (c) 2013-2018 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll file-related redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "../ntdll.h"
#include "../os_private.h"
#include "drwinapi_private.h"
/* avoid define conflicts with ntdll.h with recent WDK */
#undef FILE_READ_ACCESS
#undef FILE_WRITE_ACCESS
#include <winioctl.h> /* DEVICE_TYPE_FROM_CTL_CODE */

static HANDLE(WINAPI *priv_kernel32_OpenConsoleW)(LPCWSTR, DWORD, BOOL, DWORD);

static HANDLE base_named_obj_dir;
static HANDLE base_named_pipe_dir;

/* Returns a pointer either to wbuf or a const string elsewhere. */
static wchar_t *
get_base_named_obj_dir_name(wchar_t *wbuf OUT, size_t wbuflen)
{
    /* PEB.ReadOnlyStaticServerData has an array of pointers sized to match the
     * kernel (so 64-bit for WOW64).  The second pointer points at a
     * BASE_STATIC_SERVER_DATA structure.
     *
     * The Windows library code BaseGetNamedObjectDirectory() seems to
     * deal with TEB->IsImpersonating, but by initializing at startup
     * here and not lazily I'm hoping we can avoid that complexity
     * (XXX: what about attach?).
     */
    byte *ptr = (byte *)get_peb(NT_CURRENT_PROCESS)->ReadOnlyStaticServerData;
    int len;
    /* For win8 wow64, data is above 4GB so we can't read it as easily.  Rather than
     * muck around with NtWow64ReadVirtualMemory64 we construct the string.
     * For x64, the string is the 6th pointer, instead of the 2nd: just
     * seems more fragile to read it than to construct.
     */
    if (ptr != NULL && get_os_version() < WINDOWS_VERSION_8) {
#ifndef X64
        if (is_wow64_process(NT_CURRENT_PROCESS)) {
            BASE_STATIC_SERVER_DATA_64 *data =
                *(BASE_STATIC_SERVER_DATA_64 **)(ptr + 2 * sizeof(void *));
            /* we assume null-terminated */
            if (data != NULL)
                return data->NamedObjectDirectory.u.b32.Buffer32;
        } else
#endif
        {
            BASE_STATIC_SERVER_DATA *data =
                *(BASE_STATIC_SERVER_DATA **)(ptr + sizeof(void *));
            /* we assume null-terminated */
            if (data != NULL)
                return data->NamedObjectDirectory.Buffer;
        }
    }
    /* For earliest injection, these PEB pointers are not set up yet.
     * Thus we construct the string using what we've observed:
     * + Prior to Vista, just use BASE_NAMED_OBJECTS;
     * + On Vista+, use L"\Sessions\N\BaseNamedObjects"
     *   where N = PEB.SessionId.
     */
    if (get_os_version() < WINDOWS_VERSION_VISTA) {
        len = _snwprintf(wbuf, wbuflen, BASE_NAMED_OBJECTS);
    } else {
        uint sid = get_peb(NT_CURRENT_PROCESS)->SessionId;
        /* we assume it's only called at init time */
        len = _snwprintf(wbuf, wbuflen, L"\\Sessions\\%d\\BaseNamedObjects", sid);
    }
    ASSERT(len >= 0 && (size_t)len < wbuflen);
    wbuf[wbuflen - 1] = L'\0';
    return wbuf;
}

void
kernel32_redir_init_file(void)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *name = get_base_named_obj_dir_name(wbuf, BUFFER_SIZE_ELEMENTS(wbuf));
    NTSTATUS res =
        nt_open_object_directory(&base_named_obj_dir, name, true /*create perms*/);
    ASSERT(NT_SUCCESS(res));

    /* The trailing \ is critical: w/o it, NtCreateNamedPipeFile returns
     * STATUS_OBJECT_NAME_INVALID.
     */
    res = nt_open_file(&base_named_pipe_dir, L"\\Device\\NamedPipe\\", GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, 0);
    ASSERT(NT_SUCCESS(res));
}

void
kernel32_redir_exit_file(void)
{
    close_handle(base_named_pipe_dir);
    nt_close_object_directory(base_named_obj_dir);
}

void
kernel32_redir_onload_file(privmod_t *mod)
{
    priv_kernel32_OpenConsoleW = (HANDLE(WINAPI *)(
        LPCWSTR, DWORD, BOOL, DWORD))get_proc_address_ex(mod->base, "OpenConsoleW", NULL);
}

static void
init_object_attr_for_files(OBJECT_ATTRIBUTES *oa, UNICODE_STRING *us,
                           SECURITY_ATTRIBUTES *sa, SECURITY_QUALITY_OF_SERVICE *sqos)
{
    ULONG obj_flags = OBJ_CASE_INSENSITIVE;
    if (sa != NULL && sa->nLength >= sizeof(SECURITY_ATTRIBUTES) && sa->bInheritHandle)
        obj_flags |= OBJ_INHERIT;
    InitializeObjectAttributes(
        oa, us, obj_flags, NULL,
        (sa != NULL &&
         sa->nLength >= offsetof(SECURITY_ATTRIBUTES, lpSecurityDescriptor) +
                 sizeof(sa->lpSecurityDescriptor))
            ? sa->lpSecurityDescriptor
            : NULL);
    if (sqos != NULL)
        oa->SecurityQualityOfService = sqos;
}

static void
largeint_to_filetime(const LARGE_INTEGER *li, FILETIME *ft OUT)
{
    ft->dwHighDateTime = li->HighPart;
    ft->dwLowDateTime = li->LowPart;
}

/***************************************************************************
 * DIRECTORIES
 */

/* nt_path_name must already be in NT format */
static BOOL
create_dir_common(__in LPCWSTR nt_path_name,
                  __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iob = { 0, 0 };
    UNICODE_STRING file_path_unicode;
    HANDLE handle;
    ACCESS_MASK access = SYNCHRONIZE | FILE_LIST_DIRECTORY;
    ULONG file_attributes = FILE_ATTRIBUTE_NORMAL;
    ULONG sharing = FILE_SHARE_READ;
    ULONG disposition = FILE_CREATE;
    ULONG options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE |
        FILE_OPEN_FOR_BACKUP_INTENT /*docs say to use for dir handle*/;

    res = wchar_to_unicode(&file_path_unicode, nt_path_name);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    init_object_attr_for_files(&oa, &file_path_unicode, lpSecurityAttributes, NULL);

    res = nt_raw_CreateFile(&handle, access, &oa, &iob, NULL, file_attributes, sharing,
                            disposition, options, NULL, 0);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    close_handle(handle);
    return TRUE;
}

BOOL WINAPI
redirect_CreateDirectoryA(__in LPCSTR lpPathName,
                          __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    wchar_t wbuf[MAX_PATH];
    if (lpPathName == NULL ||
        !convert_to_NT_file_path(wbuf, lpPathName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */
    return create_dir_common(wbuf, lpSecurityAttributes);
}

BOOL WINAPI
redirect_CreateDirectoryW(__in LPCWSTR lpPathName,
                          __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    BOOL res;
    if (lpPathName != NULL) {
        nt_path = convert_to_NT_file_path_wide(wbuf, lpPathName,
                                               BUFFER_SIZE_ELEMENTS(wbuf), &alloc_sz);
    }
    if (nt_path == NULL) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    res = create_dir_common(nt_path, lpSecurityAttributes);
    if (nt_path != NULL && nt_path != wbuf)
        convert_to_NT_file_path_wide_free(nt_path, alloc_sz);
    return res;
}

BOOL WINAPI
redirect_RemoveDirectoryA(__in LPCSTR lpPathName)
{
    /* Existing file code should work on dirs and links.
     * XXX: what about removing mount points for volumes?
     */
    return redirect_DeleteFileA(lpPathName);
}

BOOL WINAPI
redirect_RemoveDirectoryW(__in LPCWSTR lpPathName)
{
    /* Existing file code should work on dirs and links.
     * XXX: what about removing mount points for volumes?
     */
    return redirect_DeleteFileW(lpPathName);
}

DWORD
WINAPI
redirect_GetCurrentDirectoryA(__in DWORD nBufferLength,
                              __out_ecount_part_opt(nBufferLength, return +1)
                                  LPSTR lpBuffer)
{
    wchar_t *wbuf = NULL;
    DWORD res;
    if (lpBuffer != NULL) {
        wbuf = (wchar_t *)global_heap_alloc(nBufferLength *
                                            sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
    }
    res = redirect_GetCurrentDirectoryW(nBufferLength, wbuf);
    if (lpBuffer != NULL && res > 0 && res < nBufferLength) {
        int len = _snprintf(lpBuffer, nBufferLength, "%S", wbuf);
        if (len < 0) {
            set_last_error(ERROR_BAD_PATHNAME); /* any better errno to use? */
            res = 0;
        }
    }
    if (wbuf != NULL)
        global_heap_free(wbuf, nBufferLength * sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
    return res;
}

DWORD
WINAPI
redirect_GetCurrentDirectoryW(__in DWORD nBufferLength,
                              __out_ecount_part_opt(nBufferLength, return +1)
                                  LPWSTR lpBuffer)
{
    /* For the cur dir, we do not try to grab the PEB lock.
     * The Win32 API docs warn that accessing the cur dir is racy, and it's
     * not supported when there's more than one thread.
     * We could use TRY/EXCEPT but we'll assume that priv libs won't abuse these.
     */
    PEB *peb = get_own_peb();
    int len;
    DWORD total = peb->ProcessParameters->CurrentDirectoryPath.Length / sizeof(wchar_t) +
        1 /*null*/;
    if (lpBuffer == NULL)
        return total; /* no errno */
    else {
        len = _snwprintf(lpBuffer, nBufferLength, L"%.*s",
                         /* we've seen too many non-null-terminated paths */
                         peb->ProcessParameters->CurrentDirectoryPath.Length,
                         peb->ProcessParameters->CurrentDirectoryPath.Buffer);
        if (len < 0) {
            set_last_error(ERROR_BAD_PATHNAME); /* any better errno to use? */
            return 0;
        }
        if ((DWORD)len < nBufferLength)
            return (DWORD)len;
        else
            return total;
    }
}

BOOL WINAPI
redirect_SetCurrentDirectoryA(__in LPCSTR lpPathName)
{
    wchar_t wbuf[MAX_PATH];
    int len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpPathName);
    if (len < 0)
        return FALSE;
    NULL_TERMINATE_BUFFER(wbuf);
    return redirect_SetCurrentDirectoryW(wbuf);
}

BOOL WINAPI
redirect_SetCurrentDirectoryW(__in LPCWSTR lpPathName)
{
    PEB *peb = get_own_peb();
    UNICODE_STRING *us = &peb->ProcessParameters->CurrentDirectoryPath;
    int len;
    /* FIXME: once we have redirect_GetFullPathNameW() we should use it here.
     * For now we don't support relative paths.
     * Update: we now have i#298 so we do have some relative path support
     * in DR.
     */
    /* CurrentDirectoryPath.Buffer should have MAX_PATH space in it */
    len = _snwprintf(us->Buffer, us->MaximumLength, L"%s", lpPathName);
    us->Buffer[us->MaximumLength - 1] = L'\0';
    return (len > 0);
}

/***************************************************************************
 * FILES
 */

static DWORD
file_create_disp_winapi_to_nt(DWORD winapi)
{
    /* we don't support OF_ flags b/c we aren't redirecting OpenFile */
    switch (winapi) {
    case CREATE_NEW: return FILE_CREATE;
    case CREATE_ALWAYS: return FILE_OVERWRITE_IF;
    case OPEN_EXISTING: return FILE_OPEN;
    case OPEN_ALWAYS: return FILE_OPEN_IF;
    case TRUNCATE_EXISTING: return FILE_OVERWRITE;
    default: return 0;
    }
}

static DWORD
file_options_to_nt(DWORD winapi, ACCESS_MASK *access INOUT)
{
    DWORD options = 0;
    if (!TEST(FILE_FLAG_OVERLAPPED, winapi))
        options |= FILE_SYNCHRONOUS_IO_NONALERT;
    if (TEST(FILE_FLAG_BACKUP_SEMANTICS, winapi)) {
        options |= FILE_OPEN_FOR_BACKUP_INTENT;
        if (TEST(GENERIC_WRITE, *access))
            options |= FILE_OPEN_REMOTE_INSTANCE;
    } else {
        /* FILE_FLAG_BACKUP_SEMANTICS is supposed to be set for directories */
        options |= FILE_NON_DIRECTORY_FILE;
    }
    if (TEST(FILE_FLAG_DELETE_ON_CLOSE, winapi)) {
        *access |= DELETE; /* needed for FILE_DELETE_ON_CLOSE */
        options |= FILE_DELETE_ON_CLOSE;
    }
    if (TEST(FILE_FLAG_NO_BUFFERING, winapi))
        options |= FILE_NO_INTERMEDIATE_BUFFERING;
    if (TEST(FILE_FLAG_OPEN_NO_RECALL, winapi))
        options |= FILE_OPEN_NO_RECALL;
    if (TEST(FILE_FLAG_OPEN_REPARSE_POINT, winapi))
        options |= FILE_OPEN_REPARSE_POINT;
    if (TEST(FILE_FLAG_RANDOM_ACCESS, winapi))
        options |= FILE_RANDOM_ACCESS;
    if (TEST(FILE_FLAG_SEQUENTIAL_SCAN, winapi))
        options |= FILE_SEQUENTIAL_ONLY;
    if (TEST(FILE_FLAG_WRITE_THROUGH, winapi))
        options |= FILE_WRITE_THROUGH;

    /* XXX: not sure about FILE_FLAG_POSIX_SEMANTICS or
     * FILE_FLAG_SESSION_AWARE
     */

    return options;
}

static ACCESS_MASK
file_access_to_nt(ACCESS_MASK winapi)
{
    ACCESS_MASK access = winapi;
    /* always set these */
    access |= SYNCHRONIZE | FILE_READ_ATTRIBUTES;
    if (TEST(GENERIC_READ, winapi))
        access |= FILE_GENERIC_READ;
    if (TEST(GENERIC_WRITE, winapi))
        access |= FILE_GENERIC_WRITE;
    if (TEST(GENERIC_EXECUTE, winapi))
        access |= FILE_GENERIC_EXECUTE;
    return access;
}

/* nt_file_name must already be in NT format */
static HANDLE
create_file_common(__in LPCWSTR nt_file_name, __in DWORD dwDesiredAccess,
                   __in DWORD dwShareMode,
                   __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   __in DWORD dwCreationDisposition, __in DWORD dwFlagsAndAttributes,
                   __in_opt HANDLE hTemplateFile)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iob = { 0, 0 };
    UNICODE_STRING file_path_unicode;
    SECURITY_QUALITY_OF_SERVICE sqos, *sqos_ptr = NULL;
    HANDLE handle;
    ACCESS_MASK access;
    ULONG file_attributes;
    ULONG sharing = dwShareMode;
    ULONG disposition;
    ULONG options;

    access = file_access_to_nt(dwDesiredAccess);

    disposition = file_create_disp_winapi_to_nt(dwCreationDisposition);
    if (disposition == 0) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    /* select FILE_ATTRIBUTE_* which map directly to syscall */
    file_attributes = dwFlagsAndAttributes & FILE_ATTRIBUTE_VALID_FLAGS;
    file_attributes &= ~FILE_ATTRIBUTE_DIRECTORY; /* except this one */

    if (TEST(SECURITY_SQOS_PRESENT, dwFlagsAndAttributes)) {
        sqos_ptr = &sqos;
        sqos.Length = sizeof(sqos);
        /* SECURITY_* flags are from 4-member SECURITY_IMPERSONATION_LEVEL enum <<16 */
        sqos.ImpersonationLevel = (dwFlagsAndAttributes >> 16) & 0x3;
        ;
        if (TEST(SECURITY_CONTEXT_TRACKING, dwFlagsAndAttributes))
            sqos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
        else
            sqos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
        sqos.EffectiveOnly = TEST(SECURITY_EFFECTIVE_ONLY, dwFlagsAndAttributes);
    }

    /* map the non-FILE_ATTRIBUTE_* flags */
    options = file_options_to_nt(dwFlagsAndAttributes, &access);

    res = wchar_to_unicode(&file_path_unicode, nt_file_name);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    if (wcscmp(nt_file_name, L"CONIN$") == 0 || wcscmp(nt_file_name, L"CONOUT$") == 0) {
        /* route to OpenConsole */
        SYSLOG_INTERNAL_WARNING_ONCE("priv lib called CreateFile on the console");
        ASSERT(priv_kernel32_OpenConsoleW != NULL);
        return (*priv_kernel32_OpenConsoleW)(
            nt_file_name, dwDesiredAccess,
            (lpSecurityAttributes == NULL) ? FALSE : lpSecurityAttributes->bInheritHandle,
            OPEN_EXISTING);
    }

    if (hTemplateFile != NULL) {
        /* FIXME: copy extended attributes */
        ASSERT_NOT_IMPLEMENTED(false);
    }

    init_object_attr_for_files(&oa, &file_path_unicode, lpSecurityAttributes, sqos_ptr);

    res = nt_raw_CreateFile(&handle, access, &oa, &iob, NULL, file_attributes, sharing,
                            disposition, options, NULL, 0);
    if (!NT_SUCCESS(res)) {
        if (res == STATUS_OBJECT_NAME_COLLISION)
            set_last_error(ERROR_FILE_EXISTS); /* instead of ERROR_ALREADY_EXISTS */
        else
            set_last_error(ntstatus_to_last_error(res));
        return INVALID_HANDLE_VALUE;
    } else {
        /* on success, errno is still set in some cases */
        if ((dwCreationDisposition == CREATE_ALWAYS &&
             iob.Information == FILE_OVERWRITTEN) ||
            (dwCreationDisposition == OPEN_ALWAYS && iob.Information == FILE_OPENED))
            set_last_error(ERROR_ALREADY_EXISTS);
        else
            set_last_error(ERROR_SUCCESS);
        return handle;
    }
}

HANDLE
WINAPI
redirect_CreateFileA(__in LPCSTR lpFileName, __in DWORD dwDesiredAccess,
                     __in DWORD dwShareMode,
                     __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     __in DWORD dwCreationDisposition, __in DWORD dwFlagsAndAttributes,
                     __in_opt HANDLE hTemplateFile)
{
    wchar_t wbuf[MAX_PATH];
    if (lpFileName == NULL ||
        !convert_to_NT_file_path(wbuf, lpFileName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */
    return create_file_common(wbuf, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                              dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE
WINAPI
redirect_CreateFileW(__in LPCWSTR lpFileName, __in DWORD dwDesiredAccess,
                     __in DWORD dwShareMode,
                     __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     __in DWORD dwCreationDisposition, __in DWORD dwFlagsAndAttributes,
                     __in_opt HANDLE hTemplateFile)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    HANDLE res;
    if (lpFileName != NULL) {
        nt_path = convert_to_NT_file_path_wide(wbuf, lpFileName,
                                               BUFFER_SIZE_ELEMENTS(wbuf), &alloc_sz);
    }
    if (nt_path == NULL) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    res = create_file_common(nt_path, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                             dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (nt_path != NULL && nt_path != wbuf)
        convert_to_NT_file_path_wide_free(nt_path, alloc_sz);
    return res;
}

BOOL WINAPI
redirect_DeleteFileA(__in LPCSTR lpFileName)
{
    NTSTATUS res;
    wchar_t wbuf[MAX_PATH];
    if (lpFileName == NULL ||
        !convert_to_NT_file_path(wbuf, lpFileName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */
    res = nt_delete_file(wbuf);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_DeleteFileW(__in LPCWSTR lpFileName)
{
    NTSTATUS res;
    wchar_t wbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    if (lpFileName != NULL) {
        nt_path = convert_to_NT_file_path_wide(wbuf, lpFileName,
                                               BUFFER_SIZE_ELEMENTS(wbuf), &alloc_sz);
    }
    if (nt_path == NULL) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    res = nt_delete_file(nt_path);
    if (nt_path != NULL && nt_path != wbuf)
        convert_to_NT_file_path_wide_free(nt_path, alloc_sz);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_ReadFile(__in HANDLE hFile,
                  __out_bcount_part(nNumberOfBytesToRead, *lpNumberOfBytesRead)
                      LPVOID lpBuffer,
                  __in DWORD nNumberOfBytesToRead, __out_opt LPDWORD lpNumberOfBytesRead,
                  __inout_opt LPOVERLAPPED lpOverlapped)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };
    HANDLE event = NULL;
    PVOID apc_cxt = NULL;
    LARGE_INTEGER offs;
    LARGE_INTEGER *offs_ptr = NULL;

    /* XXX: should redirect console handle to ReadConsole */

    if (lpOverlapped != NULL) {
        event = lpOverlapped->hEvent;
        /* XXX: I don't fully understand this, and official ZwReadFile docs
         * don't help, but it seems that the APC context is passed and used even
         * when there's no APC routine.
         */
        apc_cxt = (PVOID)lpOverlapped;
        lpOverlapped->Internal = STATUS_PENDING;
        offs_ptr = &offs;
        offs.HighPart = lpOverlapped->OffsetHigh;
        offs.LowPart = lpOverlapped->Offset;
    }

    res = NtReadFile(hFile, event, NULL, apc_cxt, &iob, lpBuffer, nNumberOfBytesToRead,
                     offs_ptr, NULL);

    if (lpOverlapped == NULL && res == STATUS_PENDING) {
        /* If synchronous, wait for it */
        res = NtWaitForSingleObject(hFile, FALSE, NULL);
        if (NT_SUCCESS(res))
            res = iob.Status;
    }
    /* Warning status codes may still set the size */
    if (lpNumberOfBytesRead != NULL) {
        if (res == STATUS_END_OF_FILE)
            *lpNumberOfBytesRead = 0;
        else if (!NT_ERROR(res)) {
            if (lpOverlapped != NULL)
                *lpNumberOfBytesRead = (DWORD)lpOverlapped->InternalHigh;
            else
                *lpNumberOfBytesRead = (DWORD)iob.Information;
        }
    }
    if ((!NT_SUCCESS(res) && res != STATUS_END_OF_FILE) || res == STATUS_PENDING) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_WriteFile(__in HANDLE hFile, __in_bcount(nNumberOfBytesToWrite) LPCVOID lpBuffer,
                   __in DWORD nNumberOfBytesToWrite,
                   __out_opt LPDWORD lpNumberOfBytesWritten,
                   __inout_opt LPOVERLAPPED lpOverlapped)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };
    IO_STATUS_BLOCK *iob_ptr = &iob;
    HANDLE event = NULL;
    PVOID apc_cxt = NULL;
    LARGE_INTEGER offs;
    LARGE_INTEGER *offs_ptr = NULL;

    /* XXX: should redirect console handle to WriteConsole */

    if (lpOverlapped != NULL) {
        event = lpOverlapped->hEvent;
        /* XXX: I don't fully understand this, but it seems that the APC context
         * is passed and used even when there's no APC routine.
         */
        apc_cxt = (PVOID)lpOverlapped;
        lpOverlapped->Internal = STATUS_PENDING;
        offs_ptr = &offs;
        offs.HighPart = lpOverlapped->OffsetHigh;
        offs.LowPart = lpOverlapped->Offset;
        /* Have kernel's write to Information at offset one pointer in
         * go to InternalHigh instead.
         * XXX: why do I only seem to need this for WriteFile
         * and not ReadFile or DeviceIoControl?
         */
        iob_ptr = (IO_STATUS_BLOCK *)lpOverlapped;
    }

    res = NtWriteFile(hFile, event, NULL, apc_cxt, iob_ptr, lpBuffer,
                      nNumberOfBytesToWrite, offs_ptr, NULL);

    if (lpOverlapped == NULL && res == STATUS_PENDING) {
        /* If synchronous, wait for it */
        res = NtWaitForSingleObject(hFile, FALSE, NULL);
        if (NT_SUCCESS(res))
            res = iob.Status;
    }
    /* Warning status codes may still set the size */
    if (!NT_ERROR(res) && lpNumberOfBytesWritten != NULL) {
        if (lpOverlapped != NULL)
            *lpNumberOfBytesWritten = (DWORD)lpOverlapped->InternalHigh;
        else
            *lpNumberOfBytesWritten = (DWORD)iob.Information;
    }
    if (!NT_SUCCESS(res) || res == STATUS_PENDING) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 * FILE QUERIES
 */

DWORD
WINAPI
redirect_GetFileAttributesA(__in LPCSTR lpFileName)
{
    wchar_t wbuf[MAX_PATH];
    int len;
    if (lpFileName == NULL) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return INVALID_FILE_ATTRIBUTES;
    }
    len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpFileName);
    if (len < 0) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return INVALID_FILE_ATTRIBUTES;
    }
    NULL_TERMINATE_BUFFER(wbuf);
    return redirect_GetFileAttributesW(wbuf);
}

DWORD
WINAPI
redirect_GetFileAttributesW(__in LPCWSTR lpFileName)
{
    NTSTATUS res;
    wchar_t ntbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    FILE_NETWORK_OPEN_INFORMATION info;

    nt_path = convert_to_NT_file_path_wide(ntbuf, lpFileName, BUFFER_SIZE_ELEMENTS(ntbuf),
                                           &alloc_sz);
    if (nt_path == NULL) {
        set_last_error(ERROR_FILE_NOT_FOUND);
        return INVALID_FILE_ATTRIBUTES;
    }
    res = wchar_to_unicode(&us, nt_path);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return INVALID_FILE_ATTRIBUTES;
    }

    init_object_attr_for_files(&oa, &us, NULL, NULL);
    res = nt_raw_QueryFullAttributesFile(&oa, &info);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return INVALID_FILE_ATTRIBUTES;
    }
    return info.FileAttributes;
}

BOOL WINAPI
redirect_GetFileInformationByHandle(__in HANDLE hFile,
                                    __out LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
    NTSTATUS res;
    FILE_BASIC_INFORMATION basic;
    FILE_STANDARD_INFORMATION standard;
    FILE_INTERNAL_INFORMATION internal;
    byte volume_buf[sizeof(FILE_FS_VOLUME_INFORMATION) + sizeof(wchar_t) * MAX_PATH];
    FILE_FS_VOLUME_INFORMATION *volume = (FILE_FS_VOLUME_INFORMATION *)volume_buf;

    if (lpFileInformation == NULL) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    res = nt_query_file_info(hFile, &basic, sizeof(basic), FileBasicInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    lpFileInformation->dwFileAttributes = basic.FileAttributes;
    largeint_to_filetime(&basic.CreationTime, &lpFileInformation->ftCreationTime);
    largeint_to_filetime(&basic.LastAccessTime, &lpFileInformation->ftLastAccessTime);
    largeint_to_filetime(&basic.LastWriteTime, &lpFileInformation->ftLastWriteTime);

    res = nt_query_file_info(hFile, &internal, sizeof(internal), FileInternalInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    lpFileInformation->nFileIndexHigh = internal.IndexNumber.HighPart;
    lpFileInformation->nFileIndexLow = internal.IndexNumber.LowPart;

    res =
        nt_query_volume_info(hFile, volume, sizeof(volume_buf), FileFsVolumeInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    lpFileInformation->dwVolumeSerialNumber = volume->VolumeSerialNumber;

    res = nt_query_file_info(hFile, &standard, sizeof(standard), FileStandardInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    lpFileInformation->nNumberOfLinks = standard.NumberOfLinks;
    lpFileInformation->nFileSizeHigh = standard.EndOfFile.HighPart;
    lpFileInformation->nFileSizeLow = standard.EndOfFile.LowPart;

    return TRUE;
}

DWORD
WINAPI
redirect_GetFileSize(__in HANDLE hFile, __out_opt LPDWORD lpFileSizeHigh)
{
    NTSTATUS res;
    FILE_STANDARD_INFORMATION standard;
    res = nt_query_file_info(hFile, &standard, sizeof(standard), FileStandardInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return INVALID_FILE_SIZE;
    }
    if (lpFileSizeHigh != NULL)
        *lpFileSizeHigh = standard.EndOfFile.HighPart;
    return standard.EndOfFile.LowPart;
}

DWORD
WINAPI
redirect_GetFileType(__in HANDLE hFile)
{
    NTSTATUS res;
    FILE_FS_DEVICE_INFORMATION info;
    res = nt_query_volume_info(hFile, &info, sizeof(info), FileFsDeviceInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FILE_TYPE_UNKNOWN;
    }

    set_last_error(NO_ERROR);
    switch (info.DeviceType) {
    case FILE_DEVICE_CD_ROM:
    case FILE_DEVICE_CD_ROM_FILE_SYSTEM:
    case FILE_DEVICE_CONTROLLER:
    case FILE_DEVICE_DATALINK:
    case FILE_DEVICE_DFS:
    case FILE_DEVICE_DISK:
    case FILE_DEVICE_DISK_FILE_SYSTEM:
    case FILE_DEVICE_VIRTUAL_DISK: return FILE_TYPE_DISK;

    case FILE_DEVICE_KEYBOARD:
    case FILE_DEVICE_MOUSE:
    case FILE_DEVICE_NULL:
    case FILE_DEVICE_PARALLEL_PORT:
    case FILE_DEVICE_PRINTER:
    case FILE_DEVICE_SERIAL_PORT:
    case FILE_DEVICE_SCREEN:
    case FILE_DEVICE_SOUND:
    case FILE_DEVICE_MODEM: return FILE_TYPE_CHAR;

    case FILE_DEVICE_NAMED_PIPE: return FILE_TYPE_PIPE;
    }
    return FILE_TYPE_UNKNOWN;
}

/***************************************************************************
 * FILE MAPPING
 */

/* Xref os_map_file() which this code is modeled on, though here we
 * have to support anonymous mappings as well.
 */

HANDLE
WINAPI
redirect_CreateFileMappingA(__in HANDLE hFile,
                            __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                            __in DWORD flProtect, __in DWORD dwMaximumSizeHigh,
                            __in DWORD dwMaximumSizeLow, __in_opt LPCSTR lpName)
{
    wchar_t wbuf[MAX_PATH];
    LPCWSTR wname = NULL;
    if (lpName != NULL) {
        int len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpName);
        if (len < 0 || len >= BUFFER_SIZE_ELEMENTS(wbuf)) {
            set_last_error(ERROR_INVALID_PARAMETER);
            return NULL;
        }
        NULL_TERMINATE_BUFFER(wbuf);
        wname = wbuf;
    }
    return redirect_CreateFileMappingW(hFile, lpFileMappingAttributes, flProtect,
                                       dwMaximumSizeHigh, dwMaximumSizeLow, wname);
}

HANDLE
WINAPI
redirect_CreateFileMappingW(__in HANDLE hFile,
                            __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                            __in DWORD flProtect, __in DWORD dwMaximumSizeHigh,
                            __in DWORD dwMaximumSizeLow, __in_opt LPCWSTR lpName)
{
    NTSTATUS res;
    HANDLE section;
    OBJECT_ATTRIBUTES oa;
    ULONG prot = (flProtect & 0xffff);
    ULONG section_flags = (flProtect & 0xffff0000);
    DWORD access = SECTION_ALL_ACCESS;
    LARGE_INTEGER li_size;
    li_size.LowPart = dwMaximumSizeLow;
    li_size.HighPart = dwMaximumSizeHigh;

    if (section_flags == 0)
        section_flags = SEC_COMMIT; /* default, if none specified */

    if (!prot_is_executable(prot))
        access &= ~SECTION_MAP_EXECUTE;
    if (!prot_is_writable(prot))
        access &= ~SECTION_MAP_WRITE;

    init_object_attr_for_files(&oa, NULL, lpFileMappingAttributes, NULL);
    /* file mappings are case sensitive */
    oa.Attributes &= ~OBJ_CASE_INSENSITIVE;
    if (hFile == INVALID_HANDLE_VALUE)
        oa.Attributes |= OBJ_OPENIF;

    /* If lpName has a "\Global\" prefix, the kernel will move it to
     * the "\BaseNamedObjects" dir, so we can pass the local session
     * dir regardless of the name.
     */

    res = nt_create_section(
        &section, access,
        (dwMaximumSizeHigh == 0 && dwMaximumSizeLow == 0) ? NULL : &li_size, prot,
        section_flags, (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile,
        /* our nt_create_section() re-creates oa */
        lpName, oa.Attributes,
        /* anonymous section needs base dir, else we get
         * STATUS_OBJECT_PATH_SYNTAX_BAD
         */
        (hFile == INVALID_HANDLE_VALUE) ? base_named_obj_dir : NULL,
        oa.SecurityDescriptor);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return NULL;
    } else if (res == STATUS_OBJECT_NAME_EXISTS) {
        /* a non-section type name conflict will fail w/ STATUS_OBJECT_TYPE_MISMATCH */
        set_last_error(ERROR_ALREADY_EXISTS);
    } else
        set_last_error(ERROR_SUCCESS);
    return section;
}

LPVOID
WINAPI
redirect_MapViewOfFile(__in HANDLE hFileMappingObject, __in DWORD dwDesiredAccess,
                       __in DWORD dwFileOffsetHigh, __in DWORD dwFileOffsetLow,
                       __in SIZE_T dwNumberOfBytesToMap)
{
    return redirect_MapViewOfFileEx(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh,
                                    dwFileOffsetLow, dwNumberOfBytesToMap, NULL);
}

LPVOID
WINAPI
redirect_MapViewOfFileEx(__in HANDLE hFileMappingObject, __in DWORD dwDesiredAccess,
                         __in DWORD dwFileOffsetHigh, __in DWORD dwFileOffsetLow,
                         __in SIZE_T dwNumberOfBytesToMap, __in_opt LPVOID lpBaseAddress)
{
    NTSTATUS res;
    SIZE_T size = dwNumberOfBytesToMap;
    LPVOID map = lpBaseAddress;
    ULONG prot = 0;
    LARGE_INTEGER li_offs;
    li_offs.LowPart = dwFileOffsetLow;
    li_offs.HighPart = dwFileOffsetHigh;

    /* Easiest to deal w/ our bitmasks and then convert: */
    if (TESTANY(FILE_MAP_READ | FILE_MAP_WRITE | FILE_MAP_COPY, dwDesiredAccess))
        prot |= MEMPROT_READ;
    /* I checked: despite the docs talking about FILE_MAP_COPY working with
     * read-only file mapping objects, it does end up as PAGE_WRITECOPY.
     */
    if (TESTANY(FILE_MAP_WRITE | FILE_MAP_COPY, dwDesiredAccess))
        prot |= FILE_MAP_WRITE;
    if (TEST(FILE_MAP_EXECUTE, dwDesiredAccess))
        prot |= MEMPROT_EXEC;
    prot = memprot_to_osprot(prot);
    if (TEST(FILE_MAP_COPY, dwDesiredAccess) &&
        /* i#1368: not a true bitmask! */
        dwDesiredAccess != FILE_MAP_ALL_ACCESS)
        prot = osprot_add_writecopy(prot);

    res = nt_raw_MapViewOfSection(hFileMappingObject, NT_CURRENT_PROCESS, &map,
                                  0, /* no control over placement */
                                  /* if section created SEC_COMMIT, will all be
                                   * committed automatically
                                   */
                                  0, &li_offs, &size, ViewShare, /* not exposed */
                                  0 /* no special top-down or anything */, prot);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return NULL;
    }
    return map;
}

BOOL WINAPI
redirect_UnmapViewOfFile(__in LPCVOID lpBaseAddress)
{
    NTSTATUS res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, (void *)lpBaseAddress);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_FlushViewOfFile(__in LPCVOID lpBaseAddress, __in SIZE_T dwNumberOfBytesToFlush)
{
    NTSTATUS res;
    PVOID base = (PVOID)lpBaseAddress;
    ULONG_PTR size = (ULONG_PTR)dwNumberOfBytesToFlush;
    IO_STATUS_BLOCK iob = { 0, 0 };
    GET_NTDLL(NtFlushVirtualMemory,
              (IN HANDLE ProcessHandle, IN OUT PVOID * BaseAddress,
               IN OUT PULONG_PTR FlushSize, OUT PIO_STATUS_BLOCK IoStatusBlock));
    res = NtFlushVirtualMemory(NT_CURRENT_PROCESS, &base, &size, &iob);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 * DEVICES
 */

BOOL WINAPI
redirect_CreatePipe(__out_ecount_full(1) PHANDLE hReadPipe,
                    __out_ecount_full(1) PHANDLE hWritePipe,
                    __in_opt LPSECURITY_ATTRIBUTES lpPipeAttributes, __in DWORD nSize)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us = {
        0,
    };
    IO_STATUS_BLOCK iob = { 0, 0 };
    ACCESS_MASK access = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
    DWORD size = (nSize != 0) ? nSize : (DWORD)PAGE_SIZE; /* default size */
    LARGE_INTEGER timeout;
    wchar_t wbuf[MAX_PATH];
    static uint pipe_counter;
    GET_NTDLL(NtCreateNamedPipeFile,
              (OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock,
               IN ULONG ShareAccess, IN ULONG CreateDisposition, IN ULONG CreateOptions,
               /* XXX: when these are BOOLEAN, as Nebbett has them, we just
                * set the LSB and we get STATUS_INVALID_PARAMETER!
                * So I'm considering to be BOOOL.
                */
               IN BOOL TypeMessage, IN BOOL ReadmodeMessage, IN BOOL Nonblocking,
               IN ULONG MaxInstances, IN ULONG InBufferSize, IN ULONG OutBufferSize,
               IN PLARGE_INTEGER DefaultTimeout OPTIONAL));

    timeout.QuadPart = -1200000000; /* 120s */

    if (get_os_version() < WINDOWS_VERSION_VISTA) {
        /* These kernels require a name (else STATUS_OBJECT_NAME_INVALID),
         * which we build using the PID and a global counter.
         */
        uint counter = atomic_add_exchange_int((volatile int *)&pipe_counter, 1);
        int len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"Win32Pipes.%08x.%08x",
                             get_process_id(), counter);
        res = wchar_to_unicode(&us, wbuf);
        if (!NT_SUCCESS(res)) {
            set_last_error(ERROR_INVALID_NAME); /* XXX: what else to use? */
            return FALSE;
        }
    } else {
        /* We leave us with 0 length and NULL buffer b/c we don't want a name. */
    }

    init_object_attr_for_files(&oa, &us, lpPipeAttributes, NULL);
    oa.RootDirectory = base_named_pipe_dir;
    res = NtCreateNamedPipeFile(
        hReadPipe, access, &oa, &iob, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_CREATE,
        FILE_SYNCHRONOUS_IO_NONALERT, FILE_PIPE_BYTE_STREAM_TYPE,
        FILE_PIPE_BYTE_STREAM_MODE, FILE_PIPE_QUEUE_OPERATION, 1, size, size, &timeout);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    /* open the write handle */
    if (get_os_version() > WINDOWS_VERSION_XP) {
        /* XP requires the same name, while later versions just want RootDir */
        memset(&us, 0, sizeof(us));
    }
    oa.RootDirectory = *hReadPipe;
    res = nt_raw_OpenFile(hWritePipe, SYNCHRONIZE | FILE_GENERIC_WRITE, &oa, &iob,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(res)) {
        close_handle(*hReadPipe);
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_DeviceIoControl(__in HANDLE hDevice, __in DWORD dwIoControlCode,
                         __in_bcount_opt(nInBufferSize) LPVOID lpInBuffer,
                         __in DWORD nInBufferSize,
                         __out_bcount_part_opt(nOutBufferSize, *lpBytesReturned)
                             LPVOID lpOutBuffer,
                         __in DWORD nOutBufferSize, __out_opt LPDWORD lpBytesReturned,
                         __inout_opt LPOVERLAPPED lpOverlapped)
{
    NTSTATUS res;
    HANDLE event = NULL;
    PVOID apc_cxt = NULL;
    IO_STATUS_BLOCK iob = { 0, 0 };
    bool is_fs = (DEVICE_TYPE_FROM_CTL_CODE(dwIoControlCode) == FILE_DEVICE_FILE_SYSTEM);

    GET_NTDLL(NtDeviceIoControlFile,
              (IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
               IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
               OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG IoControlCode,
               IN PVOID InputBuffer OPTIONAL, IN ULONG InputBufferLength,
               OUT PVOID OutputBuffer OPTIONAL, IN ULONG OutputBufferLength));

    if (lpOverlapped != NULL) {
        event = lpOverlapped->hEvent;
        apc_cxt = (PVOID)lpOverlapped;
        lpOverlapped->Internal = STATUS_PENDING;
    }

    if (is_fs) {
        res = NtFsControlFile(hDevice, event, NULL, apc_cxt, &iob, dwIoControlCode,
                              lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize);
    } else {
        res =
            NtDeviceIoControlFile(hDevice, event, NULL, apc_cxt, &iob, dwIoControlCode,
                                  lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize);
    }

    if (lpOverlapped == NULL && res == STATUS_PENDING) {
        /* If synchronous, wait for it */
        res = NtWaitForSingleObject(hDevice, FALSE, NULL);
        if (NT_SUCCESS(res))
            res = iob.Status;
    }

    /* Warning error codes may still set the size */
    if (!NT_ERROR(res) && lpBytesReturned != NULL) {
        if (lpOverlapped != NULL)
            *lpBytesReturned = (DWORD)lpOverlapped->InternalHigh;
        else
            *lpBytesReturned = (DWORD)iob.Information;
    }
    if (!NT_SUCCESS(res) || res == STATUS_PENDING) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
redirect_GetDiskFreeSpaceA(__in_opt LPCSTR lpRootPathName,
                           __out_opt LPDWORD lpSectorsPerCluster,
                           __out_opt LPDWORD lpBytesPerSector,
                           __out_opt LPDWORD lpNumberOfFreeClusters,
                           __out_opt LPDWORD lpTotalNumberOfClusters)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *wpath = NULL;
    int len;
    if (lpRootPathName != NULL) {
        len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpRootPathName);
        if (len < 0) {
            set_last_error(ERROR_PATH_NOT_FOUND);
            return FALSE;
        }
        NULL_TERMINATE_BUFFER(wbuf);
        wpath = wbuf;
    }
    return redirect_GetDiskFreeSpaceW(wpath, lpSectorsPerCluster, lpBytesPerSector,
                                      lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

/* Shared among GetDiskFreeSpace and GetDriveType.
 * Returns NULL on error.  Up to caller to call set_last_error().
 * On success, up to user to close the handle returned.
 */
HANDLE
open_dir_from_path(__in_opt LPCWSTR lpRootPathName)
{
    NTSTATUS res;
    wchar_t wbuf[MAX_PATH];
    const wchar_t *wpath = NULL;
    wchar_t ntbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    HANDLE dir;
    bool use_cur_dir = false;

    if (lpRootPathName == NULL)
        use_cur_dir = true;
    else {
        /* For GetDiskFreeSpace, despite the man page claiming a
         * trailing \ is needed, on win7 it works fine without it.
         * A relative path seems to turn into the cur dir.
         * A \\server path needs a share (\\server\share).
         */
        if ((lpRootPathName[0] == L'\\' && lpRootPathName[1] == L'\\') ||
            (lpRootPathName[0] != L'\0' && lpRootPathName[1] == L':' &&
             lpRootPathName[2] == L'\\')) {
            /* x:\ or \\server => take as is.  Up to the caller to pass
             * in a directory instead of a file.
             */
            wpath = lpRootPathName;
        } else
            use_cur_dir = true;
    }
    if (use_cur_dir) {
        redirect_GetCurrentDirectoryW(BUFFER_SIZE_ELEMENTS(wbuf), wbuf);
        wpath = wbuf;
    }

    nt_path = convert_to_NT_file_path_wide(ntbuf, wpath, BUFFER_SIZE_ELEMENTS(ntbuf),
                                           &alloc_sz);
    if (nt_path == NULL)
        return NULL;

    res = nt_open_file(&dir, nt_path, GENERIC_READ | FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_DIRECTORY_FILE);

    if (nt_path != NULL && nt_path != ntbuf)
        convert_to_NT_file_path_wide_free(nt_path, alloc_sz);

    if (!NT_SUCCESS(res))
        return NULL;

    return dir;
}

BOOL WINAPI
redirect_GetDiskFreeSpaceW(__in_opt LPCWSTR lpRootPathName,
                           __out_opt LPDWORD lpSectorsPerCluster,
                           __out_opt LPDWORD lpBytesPerSector,
                           __out_opt LPDWORD lpNumberOfFreeClusters,
                           __out_opt LPDWORD lpTotalNumberOfClusters)
{
    NTSTATUS res;
    HANDLE dir;
    FILE_FS_SIZE_INFORMATION info;

    dir = open_dir_from_path(lpRootPathName);
    if (dir == NULL) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    res = nt_query_volume_info(dir, &info, sizeof(info), FileFsSizeInformation);
    close_handle(dir);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }

    if (lpSectorsPerCluster != NULL)
        *lpSectorsPerCluster = info.SectorsPerAllocationUnit;
    if (lpBytesPerSector != NULL)
        *lpBytesPerSector = info.BytesPerSector;
    if (lpNumberOfFreeClusters != NULL)
        *lpNumberOfFreeClusters = info.AvailableAllocationUnits.u.LowPart;
    if (lpTotalNumberOfClusters != NULL)
        *lpTotalNumberOfClusters = info.TotalAllocationUnits.u.LowPart;
    return TRUE;
}

UINT WINAPI
redirect_GetDriveTypeA(__in_opt LPCSTR lpRootPathName)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *wpath = NULL;
    int len;
    if (lpRootPathName != NULL) {
        len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpRootPathName);
        if (len < 0) {
            set_last_error(ERROR_PATH_NOT_FOUND);
            return DRIVE_NO_ROOT_DIR;
        }
        NULL_TERMINATE_BUFFER(wbuf);
        wpath = wbuf;
    }
    return redirect_GetDriveTypeW(wpath);
}

UINT WINAPI
redirect_GetDriveTypeW(__in_opt LPCWSTR lpRootPathName)
{
    NTSTATUS res;
    HANDLE dir;
    FILE_FS_DEVICE_INFORMATION info;

    dir = open_dir_from_path(lpRootPathName);
    if (dir == NULL) {
        set_last_error(ERROR_NOT_A_REPARSE_POINT); /* observed error code */
        return DRIVE_NO_ROOT_DIR;
    }

    res = nt_query_volume_info(dir, &info, sizeof(info), FileFsDeviceInformation);
    close_handle(dir);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return DRIVE_NO_ROOT_DIR;
    }

    switch (info.DeviceType) {
    case FILE_DEVICE_DISK:
    case FILE_DEVICE_DISK_FILE_SYSTEM: {
        if (TEST(FILE_REMOVABLE_MEDIA, info.Characteristics))
            return DRIVE_REMOVABLE;
        else if (TEST(FILE_REMOTE_DEVICE, info.Characteristics))
            return DRIVE_REMOTE;
        else
            return DRIVE_FIXED;
    }
    case FILE_DEVICE_CD_ROM:
    case FILE_DEVICE_CD_ROM_FILE_SYSTEM: return DRIVE_CDROM;
    case FILE_DEVICE_VIRTUAL_DISK: return DRIVE_RAMDISK;
    case FILE_DEVICE_NETWORK_FILE_SYSTEM: return DRIVE_REMOTE;
    }
    return DRIVE_UNKNOWN;
}

/***************************************************************************
 * HANDLES
 */

BOOL WINAPI
redirect_CloseHandle(__in HANDLE hObject)
{
    return (BOOL)close_handle(hObject);
}

BOOL WINAPI
redirect_DuplicateHandle(__in HANDLE hSourceProcessHandle, __in HANDLE hSourceHandle,
                         __in HANDLE hTargetProcessHandle,
                         __deref_out LPHANDLE lpTargetHandle, __in DWORD dwDesiredAccess,
                         __in BOOL bInheritHandle, __in DWORD dwOptions)
{
    NTSTATUS res = duplicate_handle(
        hSourceProcessHandle, hSourceHandle, hTargetProcessHandle, lpTargetHandle,
        /* real impl doesn't add SYNCHRONIZE, so we don't */
        dwDesiredAccess, bInheritHandle ? HANDLE_FLAG_INHERIT : 0, dwOptions);
    return NT_SUCCESS(res);
}

HANDLE
WINAPI
redirect_GetStdHandle(__in DWORD nStdHandle)
{
    switch (nStdHandle) {
    case STD_INPUT_HANDLE: return get_stdin_handle();
    case STD_OUTPUT_HANDLE: return get_stdout_handle();
    case STD_ERROR_HANDLE: return get_stderr_handle();
    }
    set_last_error(ERROR_INVALID_PARAMETER);
    return INVALID_HANDLE_VALUE;
}

/***************************************************************************
 * FILE TIME
 */

BOOL WINAPI
redirect_FileTimeToLocalFileTime(__in CONST FILETIME *lpFileTime,
                                 __out LPFILETIME lpLocalFileTime)
{
    /* XXX: the kernelbase version looks at KUSER_SHARED_DATA and
     * BASE_STATIC_SERVER_DATA to get the bias, but I'm assuming those
     * are just optimizations to avoid a syscall.
     */
    NTSTATUS res;
    LARGE_INTEGER local;
    SYSTEM_TIME_OF_DAY_INFORMATION info;
    res = query_system_info(SystemTimeOfDayInformation, sizeof(info), &info);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    local.LowPart = lpFileTime->dwLowDateTime;
    local.HighPart = lpFileTime->dwHighDateTime;
    local.QuadPart -= info.TimeZoneBias.QuadPart;
    lpLocalFileTime->dwLowDateTime = local.LowPart;
    lpLocalFileTime->dwHighDateTime = local.HighPart;
    return TRUE;
}

BOOL WINAPI
redirect_LocalFileTimeToFileTime(__in CONST FILETIME *lpLocalFileTime,
                                 __out LPFILETIME lpFileTime)
{
    NTSTATUS res;
    LARGE_INTEGER local;
    SYSTEM_TIME_OF_DAY_INFORMATION info;
    res = query_system_info(SystemTimeOfDayInformation, sizeof(info), &info);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    local.LowPart = lpLocalFileTime->dwLowDateTime;
    local.HighPart = lpLocalFileTime->dwHighDateTime;
    local.QuadPart += info.TimeZoneBias.QuadPart;
    lpFileTime->dwLowDateTime = local.LowPart;
    lpFileTime->dwHighDateTime = local.HighPart;
    return TRUE;
}

BOOL WINAPI
redirect_FileTimeToSystemTime(__in CONST FILETIME *lpFileTime,
                              __out LPSYSTEMTIME lpSystemTime)
{
    uint64 time = ((uint64)lpFileTime->dwHighDateTime << 32) | lpFileTime->dwLowDateTime;
    convert_100ns_to_system_time(time, lpSystemTime);
    return TRUE;
}

BOOL WINAPI
redirect_SystemTimeToFileTime(__in CONST SYSTEMTIME *lpSystemTime,
                              __out LPFILETIME lpFileTime)
{
    uint64 time;
    convert_system_time_to_100ns(lpSystemTime, &time);
    lpFileTime->dwHighDateTime = (DWORD)(time >> 32);
    lpFileTime->dwLowDateTime = (DWORD)time;
    return TRUE;
}

VOID WINAPI
redirect_GetSystemTimeAsFileTime(__out LPFILETIME lpSystemTimeAsFileTime)
{
    SYSTEMTIME st;
    query_system_time(&st);
    redirect_SystemTimeToFileTime(&st, lpSystemTimeAsFileTime);
}

BOOL WINAPI
redirect_GetFileTime(__in HANDLE hFile, __out_opt LPFILETIME lpCreationTime,
                     __out_opt LPFILETIME lpLastAccessTime,
                     __out_opt LPFILETIME lpLastWriteTime)
{
    NTSTATUS res;
    FILE_BASIC_INFORMATION info;

    res = nt_query_file_info(hFile, &info, sizeof(info), FileBasicInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    if (lpCreationTime != NULL) {
        largeint_to_filetime(&info.CreationTime, lpCreationTime);
    }
    if (lpLastAccessTime != NULL) {
        largeint_to_filetime(&info.LastAccessTime, lpLastAccessTime);
    }
    if (lpLastWriteTime != NULL) {
        largeint_to_filetime(&info.LastWriteTime, lpLastWriteTime);
    }
    return TRUE;
}

BOOL WINAPI
redirect_SetFileTime(__in HANDLE hFile, __in_opt CONST FILETIME *lpCreationTime,
                     __in_opt CONST FILETIME *lpLastAccessTime,
                     __in_opt CONST FILETIME *lpLastWriteTime)
{
    NTSTATUS res;
    FILE_BASIC_INFORMATION info;
    memset(&info, 0, sizeof(info));

    if (lpCreationTime != NULL) {
        info.CreationTime.HighPart = lpCreationTime->dwHighDateTime;
        info.CreationTime.LowPart = lpCreationTime->dwLowDateTime;
    }
    if (lpLastAccessTime != NULL) {
        info.LastAccessTime.HighPart = lpLastAccessTime->dwHighDateTime;
        info.LastAccessTime.LowPart = lpLastAccessTime->dwLowDateTime;
    }
    if (lpLastWriteTime != NULL) {
        info.LastWriteTime.HighPart = lpLastWriteTime->dwHighDateTime;
        info.LastWriteTime.LowPart = lpLastWriteTime->dwLowDateTime;
    }
    res = nt_set_file_info(hFile, &info, sizeof(info), FileBasicInformation);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 * FILE ITERATOR
 *
 * Because NtQueryDirectoryFile stores the wildcard passed in on the first
 * invocation for a given handle, we can just use the directory handle
 * as the return value for FindFirstFile.
 */

#define FIND_FILE_INFO_SZ (sizeof(FILE_BOTH_DIR_INFORMATION) + MAX_PATH * sizeof(wchar_t))

BOOL WINAPI
redirect_FindClose(__inout HANDLE hFindFile)
{
    return redirect_CloseHandle(hFindFile);
}

/* Fills in the fields shared between LPWIN32_FIND_DATAA and LPWIN32_FIND_DATAW, but
 * does not fill in the name fields.  This allows the caller to pass LPWIN32_FIND_DATAA
 * in and use info to fill in the names.
 */
static BOOL
find_next_file_common(
    __in HANDLE dir, __in LPCWSTR pattern, /* pass pattern for first, NULL for next */
    __out LPWIN32_FIND_DATAW lpFindFileData,
    __out FILE_BOTH_DIR_INFORMATION *info /* FIND_FILE_INFO_SZ bytes in length */
)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };
    UNICODE_STRING us;
    GET_NTDLL(NtQueryDirectoryFile,
              (IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
               IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
               OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID FileInformation,
               IN ULONG FileInformationLength,
               IN FILE_INFORMATION_CLASS FileInformationClass,
               IN BOOLEAN ReturnSingleEntry, IN PUNICODE_STRING FileName OPTIONAL,
               IN BOOLEAN RestartScan));

    res = wchar_to_unicode(&us, pattern);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    /* NtQueryDirectoryFile does the globbing for us */
    res =
        NtQueryDirectoryFile(dir, NULL, NULL, NULL, &iob, info, FIND_FILE_INFO_SZ,
                             FileBothDirectoryInformation, TRUE, &us, (pattern != NULL));
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }

    lpFindFileData->dwFileAttributes = info->FileAttributes;
    largeint_to_filetime(&info->CreationTime, &lpFindFileData->ftCreationTime);
    largeint_to_filetime(&info->LastAccessTime, &lpFindFileData->ftLastAccessTime);
    largeint_to_filetime(&info->LastWriteTime, &lpFindFileData->ftLastWriteTime);
    lpFindFileData->nFileSizeHigh = info->AllocationSize.HighPart;
    lpFindFileData->nFileSizeLow = info->AllocationSize.LowPart;
    /* caller fills in the names */
    return TRUE;
}

/* nt_file_name must already be in NT format.
 * Fills in the fields shared between LPWIN32_FIND_DATAA and LPWIN32_FIND_DATAW, but
 * does not fill in the name fields.  This allows the caller to pass LPWIN32_FIND_DATAA
 * in and use info to fill in the names.
 */
static HANDLE
find_first_file_common(
    LPCWSTR nt_file_name, __out LPWIN32_FIND_DATAW lpFindFileData,
    __out FILE_BOTH_DIR_INFORMATION *info /* FIND_FILE_INFO_SZ bytes in length */
)
{
    NTSTATUS res;
    HANDLE dir = INVALID_HANDLE_VALUE;
    HANDLE ans = INVALID_HANDLE_VALUE;
    wchar_t *sep = NULL;
    wchar_t *dirname = NULL, *fname = NULL;
    size_t fname_len = 0, dirname_len = 0;

    /* First see if we were passed a plain dir */
    res = nt_open_file(&dir, nt_file_name, GENERIC_READ | FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_DIRECTORY_FILE);
    if (!NT_SUCCESS(res)) {
        /* Extract the dir from the file path.
         * While convert_to_NT_file_path*() ensures there are no forward slashes,
         * a path w/ \\? prefix will come here w/ the rest of it unchanged.
         */
        sep = (wchar_t *)double_wcsrchr(nt_file_name, L_EXPAND_LEVEL(DIRSEP),
                                        L_EXPAND_LEVEL(ALT_DIRSEP));
        if (sep == NULL) {
            set_last_error(ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        /* we can't assume nt_file_name is writable so we must make a copy */
        dirname_len = (size_t)(sep - nt_file_name);
        dirname = (wchar_t *)global_heap_alloc((dirname_len + 1 /*null*/) *
                                               sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
        memcpy(dirname, nt_file_name, dirname_len * sizeof(wchar_t));
        dirname[dirname_len] = L'\0';
        res = nt_open_file(&dir, dirname, GENERIC_READ | FILE_LIST_DIRECTORY,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_DIRECTORY_FILE);
        if (!NT_SUCCESS(res)) {
            set_last_error(ntstatus_to_last_error(res));
            goto find_first_file_error;
        }

        /* Now separate the file name pattern */
        fname_len = wcslen(nt_file_name) - (dirname_len + 1 /*dir separator*/);
        fname = (wchar_t *)global_heap_alloc((fname_len + 1 /*null*/) *
                                             sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
        memcpy(fname, sep + 1, fname_len * sizeof(wchar_t));
        fname[fname_len] = L'\0';
    }

    if (find_next_file_common(dir, fname /*==NULL for plain dir */, lpFindFileData,
                              info)) {
        ans = dir; /* success */
    }              /* else, last errror was already set */

find_first_file_error:
    if (dirname != NULL) {
        global_heap_free(
            dirname, (dirname_len + 1 /*null*/) * sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
    }
    if (fname != NULL) {
        global_heap_free(fname,
                         (fname_len + 1 /*null*/) * sizeof(wchar_t) HEAPACCT(ACCT_OTHER));
    }
    if (ans == INVALID_HANDLE_VALUE && dir != INVALID_HANDLE_VALUE)
        close_handle(dir);
    return ans;
}

/* Just fills in the name fields */
static bool
find_nt_to_win32A(FILE_BOTH_DIR_INFORMATION *info, WIN32_FIND_DATAA *win32 OUT)
{
    int len;
    /* Names in info are not null-terminated so we have to copy the count
     * specified and then add a null ourselves.
     */
    len = _snprintf(win32->cFileName, BUFFER_SIZE_ELEMENTS(win32->cFileName), "%.*S",
                    info->FileNameLength / sizeof(wchar_t), info->FileName);
    /* MSDN doesn't say what happens if we hit MAX_PATH.  It's unlikely to happen
     * as most NTFS volumes have a 255 limit on path components.  We return error.
     */
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(win32->cFileName)) {
        set_last_error(ERROR_FILE_INVALID); /* XXX: not sure what to return */
        return false;
    }
    if (info->FileNameLength / sizeof(wchar_t) < BUFFER_SIZE_ELEMENTS(win32->cFileName))
        win32->cFileName[info->FileNameLength / sizeof(wchar_t)] = '\0';
    NULL_TERMINATE_BUFFER(win32->cFileName);
    len = _snprintf(win32->cAlternateFileName,
                    BUFFER_SIZE_ELEMENTS(win32->cAlternateFileName), "%.*S",
                    info->ShortNameLength / sizeof(wchar_t), info->ShortName);
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(win32->cAlternateFileName)) {
        set_last_error(ERROR_FILE_INVALID); /* XXX: not sure what to return */
        return false;
    }
    if (info->ShortNameLength / sizeof(wchar_t) <
        BUFFER_SIZE_ELEMENTS(win32->cAlternateFileName)) {
        win32->cAlternateFileName[info->ShortNameLength / sizeof(wchar_t)] = '\0';
    }
    NULL_TERMINATE_BUFFER(win32->cAlternateFileName);
    return true;
}

/* Just fills in the name fields */
static bool
find_nt_to_win32W(FILE_BOTH_DIR_INFORMATION *info, WIN32_FIND_DATAW *win32 OUT)
{
    int len;
    /* See comments in find_nt_to_win32A. */
    len = _snwprintf(win32->cFileName, BUFFER_SIZE_ELEMENTS(win32->cFileName), L"%.*s",
                     info->FileNameLength / sizeof(wchar_t), info->FileName);
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(win32->cFileName)) {
        set_last_error(ERROR_FILE_INVALID); /* XXX: not sure what to return */
        return false;
    }
    if (info->FileNameLength < BUFFER_SIZE_BYTES(win32->cFileName))
        win32->cFileName[info->FileNameLength / sizeof(wchar_t)] = L'\0';
    NULL_TERMINATE_BUFFER(win32->cFileName);
    len = _snwprintf(win32->cAlternateFileName,
                     BUFFER_SIZE_ELEMENTS(win32->cAlternateFileName), L"%.*s",
                     info->ShortNameLength / sizeof(wchar_t), info->ShortName);
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(win32->cAlternateFileName)) {
        set_last_error(ERROR_FILE_INVALID); /* XXX: not sure what to return */
        return false;
    }
    if (info->ShortNameLength < BUFFER_SIZE_BYTES(win32->cAlternateFileName)) {
        win32->cAlternateFileName[info->ShortNameLength / sizeof(wchar_t)] = L'\0';
    }
    NULL_TERMINATE_BUFFER(win32->cAlternateFileName);
    return true;
}

HANDLE
WINAPI
redirect_FindFirstFileA(__in LPCSTR lpFileName, __out LPWIN32_FIND_DATAA lpFindFileData)
{
    wchar_t wbuf[MAX_PATH];
    HANDLE dir;
    byte info_buf[FIND_FILE_INFO_SZ]; /* 616 bytes => ok to stack alloc */
    FILE_BOTH_DIR_INFORMATION *info = (FILE_BOTH_DIR_INFORMATION *)info_buf;
    if (lpFileName == NULL ||
        !convert_to_NT_file_path(wbuf, lpFileName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    /* convert_to_NT_file_path guarantees to null-terminate on success */
    dir = find_first_file_common(wbuf, (WIN32_FIND_DATAW *)lpFindFileData, info);
    if (dir != INVALID_HANDLE_VALUE) {
        if (!find_nt_to_win32A(info, lpFindFileData))
            return INVALID_HANDLE_VALUE;
    }
    return dir;
}

HANDLE
WINAPI
redirect_FindFirstFileW(__in LPCWSTR lpFileName, __out LPWIN32_FIND_DATAW lpFindFileData)
{
    wchar_t wbuf[MAX_PATH];
    wchar_t *nt_path = NULL;
    size_t alloc_sz = 0;
    HANDLE dir;
    char info_buf[FIND_FILE_INFO_SZ]; /* 616 bytes => ok to stack alloc */
    FILE_BOTH_DIR_INFORMATION *info = (FILE_BOTH_DIR_INFORMATION *)info_buf;
    if (lpFileName != NULL) {
        nt_path = convert_to_NT_file_path_wide(wbuf, lpFileName,
                                               BUFFER_SIZE_ELEMENTS(wbuf), &alloc_sz);
    }
    if (nt_path == NULL) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    dir = find_first_file_common(nt_path, lpFindFileData, info);
    if (nt_path != NULL && nt_path != wbuf)
        convert_to_NT_file_path_wide_free(nt_path, alloc_sz);
    if (dir != INVALID_HANDLE_VALUE) {
        if (!find_nt_to_win32W(info, lpFindFileData))
            return INVALID_HANDLE_VALUE;
    }
    return dir;
}

BOOL WINAPI
redirect_FindNextFileA(__in HANDLE hFindFile, __out LPWIN32_FIND_DATAA lpFindFileData)
{
    char info_buf[FIND_FILE_INFO_SZ]; /* 616 bytes => ok to stack alloc */
    FILE_BOTH_DIR_INFORMATION *info = (FILE_BOTH_DIR_INFORMATION *)info_buf;
    return (find_next_file_common(hFindFile, NULL, (WIN32_FIND_DATAW *)lpFindFileData,
                                  info) &&
            find_nt_to_win32A(info, lpFindFileData));
}

BOOL WINAPI
redirect_FindNextFileW(__in HANDLE hFindFile, __out LPWIN32_FIND_DATAW lpFindFileData)
{
    char info_buf[FIND_FILE_INFO_SZ]; /* 616 bytes => ok to stack alloc */
    FILE_BOTH_DIR_INFORMATION *info = (FILE_BOTH_DIR_INFORMATION *)info_buf;
    return (find_next_file_common(hFindFile, NULL, lpFindFileData, info) &&
            find_nt_to_win32W(info, lpFindFileData));
}

/***************************************************************************/

BOOL WINAPI
redirect_FlushFileBuffers(__in HANDLE hFile)
{
    NTSTATUS res = nt_flush_file_buffers(hFile);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Files
 */

/***************************************************************************
 * TESTS
 */

#ifdef STANDALONE_UNIT_TEST
static void
test_directories(void)
{
    char buf[MAX_PATH];
    wchar_t wbuf[MAX_PATH];
    DWORD dw, dw2;
    BOOL ok;

    EXPECT(redirect_CreateDirectoryA("xyz:\\bogus\\name", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_INVALID_NAME);

    EXPECT(redirect_CreateDirectoryA("c:\\_bogus_\\_no_way_\\_no_exist_", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_PATH_NOT_FOUND);

    /* XXX: should look at SYSTEMDRIVE instead of assuming c:\windows exists */
    EXPECT(redirect_CreateDirectoryW(L"c:\\windows", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);

    /* current dir */
    dw = redirect_GetCurrentDirectoryA(0, NULL);
    EXPECT(dw > 0 && dw < BUFFER_SIZE_ELEMENTS(buf), true);
    dw2 = redirect_GetCurrentDirectoryA(dw, buf);
    EXPECT(dw2 == dw - 1 /*null*/, true);
    dw2 = redirect_GetCurrentDirectoryA(dw - 1, buf);
    EXPECT(dw2 == dw, true);
    dw2 = redirect_GetCurrentDirectoryW(dw - 1, wbuf);
    EXPECT(dw2 == dw, true);

    ok = redirect_SetCurrentDirectoryW(L"c:\\windows");
    EXPECT(ok, true);
    dw = redirect_GetCurrentDirectoryW(BUFFER_SIZE_ELEMENTS(wbuf), wbuf);
    EXPECT(dw < BUFFER_SIZE_ELEMENTS(wbuf), true);
    EXPECT(wcscmp(L"c:\\windows", wbuf) == 0, true);

    /* Successfully creating a new dir, and redirect_RemoveDirectoryA,
     * are tested in test_find_file().
     */
}

static void
test_files(void)
{
    HANDLE h, h2;
    PVOID p;
    BOOL ok;
    DWORD dw, dw2;
    char env[MAX_PATH];
    char buf[MAX_PATH];
    int res;
    OVERLAPPED overlap = {
        0,
    };
    HANDLE e;
    BY_HANDLE_FILE_INFORMATION byh_info;

    res = GetEnvironmentVariableA("TMP", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(env);

    /* test creating files */
    h = redirect_CreateFileW(L"c:\\cygwin\\tmp\\_kernel32_file_test_bogus.txt",
                             GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h == INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error() == ERROR_FILE_NOT_FOUND ||
               get_last_error() == ERROR_PATH_NOT_FOUND,
           true);

    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\_kernel32_file_test_temp.txt",
                    env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
    /* clobber it and ensure we give the right errno */
    h2 = redirect_CreateFileA(
        buf, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(h2 != INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
    ok = redirect_FlushFileBuffers(h2);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    /* re-create and then test deleting it */
    h = redirect_CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);

    /* test read and write */
    ok = redirect_WriteFile(h, &h2, sizeof(h2), (LPDWORD)&dw, NULL);
    EXPECT(ok, true);
    EXPECT(dw == sizeof(h2), true);
    /* XXX: once we have redirect_SetFilePointer use it here */
    dw = SetFilePointer(h, 0, NULL, FILE_BEGIN);
    EXPECT(dw == 0, true);
    ok = redirect_ReadFile(h, &p, sizeof(p), (LPDWORD)&dw, NULL);
    EXPECT(ok, true);
    EXPECT(dw == sizeof(h2), true);
    EXPECT((HANDLE)p == h2, true);

    /* test queries */
    memset(&byh_info, 0, sizeof(byh_info));
    ok = redirect_GetFileInformationByHandle(h, &byh_info);
    EXPECT(ok, TRUE);
    EXPECT(byh_info.nFileSizeLow != 0, true);

    dw = redirect_GetFileAttributesA(buf);
    EXPECT(dw != INVALID_FILE_ATTRIBUTES, true);

    dw = redirect_GetFileSize(h, &dw2);
    EXPECT(dw != INVALID_FILE_SIZE, true);
    EXPECT(dw > 0 && dw2 == 0, true);

    dw = redirect_GetFileType(h);
    EXPECT(dw == FILE_TYPE_DISK, true);

    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
    ok = redirect_DeleteFileA(buf);
    EXPECT(ok, true);

    /* test asynch read and write */
    h = redirect_CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED /*for asynch*/,
                             NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    e = CreateEvent(NULL, TRUE, FALSE, "myevent");
    EXPECT(e != NULL, true);
    overlap.hEvent = e;
    ok = redirect_WriteFile(h, &h2, sizeof(h2), (LPDWORD)&dw, &overlap);
    EXPECT((!ok && get_last_error() == ERROR_IO_PENDING) ||
               /* On XP, 2K3, and win8.1 this returns TRUE (i#1196, i#2145) */
               ok,
           true);
    ok = GetOverlappedResult(h, &overlap, &dw, TRUE /*wait*/);
    EXPECT(ok, true);
    EXPECT(dw == sizeof(h2), true);
    /* XXX: once we have redirect_SetFilePointer use it here */
    dw = SetFilePointer(h, 0, NULL, FILE_BEGIN);
    EXPECT(dw == 0, true);
    ok = redirect_ReadFile(h, &p, sizeof(p), (LPDWORD)&dw, &overlap);
    EXPECT((!ok && get_last_error() == ERROR_IO_PENDING) ||
               /* On XP, 2K3, and win8.1 this returns TRUE (i#1196, i#2145) */
               ok,
           true);
    ok = GetOverlappedResult(h, &overlap, &dw, TRUE /*wait*/);
    EXPECT(ok, true);
    EXPECT(dw == sizeof(h2), true);
    EXPECT((HANDLE)p == h2, true);
    ok = redirect_CloseHandle(e);
    EXPECT(ok, true);

    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
    ok = redirect_DeleteFileA(buf);
    EXPECT(ok, true);
}

static void
test_file_mapping(void)
{
    HANDLE h, h2;
    BOOL ok;
    PVOID p;
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T sz;
    int res;
    char env[MAX_PATH];
    char buf[MAX_PATH];

    /* test anonymous mappings */
    h2 = CreateEvent(NULL, TRUE, TRUE, "Local\\mymapping"); /* for conflict */
    EXPECT(h2 != NULL, true);
    h = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000,
                                    "Local\\mymapping");
    EXPECT(h == NULL, true);
    EXPECT(get_last_error(), ERROR_INVALID_HANDLE);
    CloseHandle(h2); /* removes conflict */
    h = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000,
                                    "Local\\mymapping");
    EXPECT(h != NULL, true);
    h2 = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                     0x1000, "Local\\mymapping");
    EXPECT(h2 != NULL, true);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    p = redirect_MapViewOfFileEx(h, FILE_MAP_WRITE, 0, 0, 0x800, NULL);
    EXPECT(p != NULL, true);
    *(int *)p = 42; /* test writing: shouldn't crash */
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);

    /* i#1368: ensure FILE_MAP_ALL_ACCESS does not include COW */
    p = redirect_MapViewOfFileEx(h, FILE_MAP_ALL_ACCESS, 0, 0, 0x1000, NULL);
    EXPECT(p != NULL, true);
    sz = redirect_VirtualQuery(p, &mbi, sizeof(mbi));
    EXPECT(sz == sizeof(mbi), true);
    EXPECT(mbi.AllocationProtect == PAGE_READWRITE, true);
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);

    /* i#1368: ensure FILE_MAP_COPY does include COW */
    p = redirect_MapViewOfFileEx(h, FILE_MAP_COPY, 0, 0, 0x1000, NULL);
    EXPECT(p != NULL, true);
    sz = redirect_VirtualQuery(p, &mbi, sizeof(mbi));
    EXPECT(sz == sizeof(mbi), true);
    EXPECT(mbi.AllocationProtect == PAGE_WRITECOPY, true);
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);

    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);

    /* test file mappings */
    res = GetEnvironmentVariableA("SystemRoot", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(env);
    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\system32\\notepad.exe", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    h2 = redirect_CreateFileMappingA(h, NULL, PAGE_READONLY, 0, 0, NULL);
    EXPECT(h2 != NULL, true);
    p = redirect_MapViewOfFileEx(h2, FILE_MAP_READ, 0, 0, 0, NULL);
    EXPECT(p != NULL, true);
    EXPECT(*(WORD *)p == IMAGE_DOS_SIGNATURE, true); /* PE image */
    ok = redirect_FlushViewOfFile(p, 0);
    EXPECT(ok, true);
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
}

static void
test_pipe(void)
{
    HANDLE h, h2, h3;
    PVOID p;
    BOOL ok;
    DWORD res;

    /* test pipe */
    ok = redirect_CreatePipe(&h, &h2, NULL, 0);
    EXPECT(ok, true);
    /* This will block if the buffer is full, but we assume the buffer
     * is much bigger than the size of a handle for our single-threaded test.
     */
    ok = redirect_WriteFile(h2, &h2, sizeof(h2), (LPDWORD)&res, NULL);
    EXPECT(ok, true);
    ok = redirect_ReadFile(h, &p, sizeof(p), (LPDWORD)&res, NULL);
    EXPECT(ok, true);
    EXPECT((HANDLE)p == h2, true);
    res = redirect_GetFileType(h);
    EXPECT(res == FILE_TYPE_PIPE, true);
    ok = redirect_DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &h3, 0,
                                  FALSE, 0);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h3);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
}

static void
test_DeviceIoControl(void)
{
    HANDLE dev;
    BOOL ok;
    DWORD res;
    DISK_GEOMETRY geo;
    OVERLAPPED overlap;
    HANDLE e;

    /* Test synchronous */
    dev = redirect_CreateFileW(L"\\\\.\\PhysicalDrive0", 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0,
                               NULL);
    EXPECT(dev != INVALID_HANDLE_VALUE, true);
    ok = redirect_DeviceIoControl(dev, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo,
                                  sizeof(geo), &res, NULL);
    EXPECT(ok, true);
    EXPECT(res > 0, true);
    EXPECT(geo.Cylinders.QuadPart > 0, true);
    ok = redirect_CloseHandle(dev);
    EXPECT(ok, true);

    /* Test asynchronous */
    dev = redirect_CreateFileW(L"\\\\.\\PhysicalDrive0", 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, NULL);
    EXPECT(dev != INVALID_HANDLE_VALUE, true);
    e = CreateEvent(NULL, TRUE, FALSE, "myevent");
    EXPECT(e != NULL, true);
    overlap.hEvent = e;
    ok = redirect_DeviceIoControl(dev, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo,
                                  sizeof(geo), &res, &overlap);
    EXPECT(ok, true);
    ok = GetOverlappedResult(dev, &overlap, &res, TRUE /*wait*/);
    EXPECT(ok, true);
    EXPECT(res > 0, true);
    EXPECT(geo.Cylinders.QuadPart > 0, true);

    ok = redirect_CloseHandle(dev);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(e);
    EXPECT(ok, true);
}

static bool
within_one(uint v1, uint v2)
{
    uint diff = (v1 > v2) ? v1 - v2 : v2 - v1;
    return (diff <= 1);
}

static void
test_file_times(void)
{
    FILETIME ft_create, ft_access, ft_write;
    FILETIME local_ours, local_native;
    SYSTEMTIME st_ours, st_native;
    BOOL ok;
    char env[MAX_PATH];
    char buf[MAX_PATH];
    HANDLE h;
    int res;

    res = GetEnvironmentVariableA("TMP", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(env);
    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\_kernel32_file_time_temp.txt",
                    env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);

    ok = redirect_GetFileTime(h, &ft_create, &ft_access, &ft_write);
    EXPECT(ok, true);
    ok = redirect_FileTimeToLocalFileTime(&ft_create, &local_ours);
    EXPECT(ok, true);
    /* call the real thing to compare results */
    ok = FileTimeToLocalFileTime(&ft_create, &local_native);
    EXPECT(ok, true);
    EXPECT(local_ours.dwLowDateTime == local_native.dwLowDateTime &&
               local_ours.dwHighDateTime == local_native.dwHighDateTime,
           true);

    ok = redirect_LocalFileTimeToFileTime(&local_ours, &ft_access);
    EXPECT(ok, true);
    /* now ft_access holds ft_create converted to local and back to file */
    EXPECT(ft_access.dwLowDateTime == ft_create.dwLowDateTime &&
               ft_access.dwHighDateTime == ft_create.dwHighDateTime,
           true);

    ok = redirect_SetFileTime(h, &ft_create, &ft_access, &ft_write);
    EXPECT(ok, true);

    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);

    ok = redirect_FileTimeToSystemTime(&ft_create, &st_ours);
    EXPECT(ok, true);
    /* call the real thing to compare results */
    ok = FileTimeToSystemTime(&ft_create, &st_native);
    EXPECT(ok, true);
    EXPECT(memcmp(&st_ours, &st_native, sizeof(st_ours)), 0);

    ok = redirect_SystemTimeToFileTime(&st_ours, &ft_access);
    EXPECT(ok, true);
    /* Now ft_access holds ft_create converted to system and back to file,
     * except system time only goes to milliseconds so we've lost
     * the bottom bits.
     */
    EXPECT(within_one(ft_access.dwLowDateTime / TIMER_UNITS_PER_MILLISECOND,
                      ft_create.dwLowDateTime / TIMER_UNITS_PER_MILLISECOND) &&
               ft_access.dwHighDateTime == ft_create.dwHighDateTime,
           true);

    redirect_GetSystemTimeAsFileTime(&ft_access);
    /* call the real thing to compare results */
    GetSystemTimeAsFileTime(&ft_create);
    /* some time has passed so only compare high */
    EXPECT(ft_access.dwHighDateTime == ft_create.dwHighDateTime, true);
}

static void
test_find_file(void)
{
    WIN32_FIND_DATAA data;
    WIN32_FIND_DATAW wdata;
    HANDLE f1, f2, find;
    BOOL ok;
    char env[MAX_PATH];
    char dir[MAX_PATH];
    char buf[MAX_PATH];
    wchar_t wbuf[MAX_PATH];
    int res;
    const char *testdir = "_kernel32_test_dir";

    /* deliberately omitting NULL_TERMINATE_BUFFER b/c EXPECT ensures no overflow */

    /* create some files in a temp dir */
    res = GetEnvironmentVariableA("TMP", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(env), true);

    res = _snprintf(dir, BUFFER_SIZE_ELEMENTS(dir), "%s\\%s", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(dir), true);
    ok = redirect_CreateDirectoryA(dir, NULL);
    EXPECT(ok || get_last_error() == ERROR_ALREADY_EXISTS, TRUE);

    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\%s\\test123.txt", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    f1 = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f1 != INVALID_HANDLE_VALUE, true);

    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\%s\\test113.txt", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    f2 = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f2 != INVALID_HANDLE_VALUE, true);

    /* search for a pattern */
    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\%s\\test1?3.txt", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    find = redirect_FindFirstFileA(buf, &data);
    EXPECT(find != INVALID_HANDLE_VALUE, true);
    EXPECT(data.nFileSizeHigh == 0 && data.nFileSizeLow == 0, true); /* empty file */
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName), true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(ok, true);
    EXPECT(data.nFileSizeHigh == 0 && data.nFileSizeLow == 0, true); /* empty file */
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName), true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(!ok && get_last_error() == ERROR_NO_MORE_FILES, true); /* only 2 matches */
    ok = redirect_CloseHandle(find);
    EXPECT(ok, TRUE);

    /* iterate all files in dir */
    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s\\%s", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    find = redirect_FindFirstFileA(buf, &data);
    EXPECT(find != INVALID_HANDLE_VALUE, true);
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName) ||
               strcmp(".", data.cFileName) == 0 || strcmp("..", data.cFileName) == 0,
           true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(ok, true);
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName) ||
               strcmp(".", data.cFileName) == 0 || strcmp("..", data.cFileName) == 0,
           true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(ok, true);
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName) ||
               strcmp(".", data.cFileName) == 0 || strcmp("..", data.cFileName) == 0,
           true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(ok, true);
    EXPECT(check_filter_with_wildcards("test1?3.txt", data.cFileName) ||
               strcmp(".", data.cFileName) == 0 || strcmp("..", data.cFileName) == 0,
           true);
    ok = redirect_FindNextFileA(find, &data);
    EXPECT(!ok && get_last_error() == ERROR_NO_MORE_FILES, true);
    ok = redirect_CloseHandle(find);
    EXPECT(ok, TRUE);

    /* iterate in wide-char dir */
    res = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%S\\%S", env, testdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(wbuf), true);
    find = redirect_FindFirstFileW(wbuf, &wdata);
    EXPECT(find != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(find);
    EXPECT(ok, TRUE);

    ok = redirect_CloseHandle(f1);
    EXPECT(ok, TRUE);
    ok = redirect_CloseHandle(f2);
    EXPECT(ok, TRUE);

    EXPECT(os_file_exists(dir, true), true);
    ok = redirect_RemoveDirectoryA(dir);
    EXPECT(ok, TRUE);
    EXPECT(os_file_exists(dir, true), false);
}

static void
test_file_paths(void)
{
    HANDLE f;
    BOOL ok;
    wchar_t env[MAX_PATH];
    wchar_t wbuf[MAX_PATH * 2];
    int res;
    res = GetEnvironmentVariableW(L"TMP", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(env), true);

    res = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"\\\\.\\%s\\test123.txt", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(wbuf), true);
    f = redirect_CreateFileW(wbuf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    res = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"\\\\?\\%s\\test123.txt", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(wbuf), true);
    f = redirect_CreateFileW(wbuf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    res = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"\\??\\%s\\test123.txt", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(wbuf), true);
    f = redirect_CreateFileW(wbuf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    /* Ensure whole thing can go beyond MAX_PATH (though no component can be
     * > 255 on our NTFS volumes).
     */
    res = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf),
                     L"\\\\?\\%s\\test12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                     L"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                     L"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                     L"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                     L"3.txt",
                     env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(wbuf), true);
    f = redirect_CreateFileW(wbuf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    /* Test relative paths (i#298) */
    f = redirect_CreateFileW(L"test456.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);
    ok = redirect_DeleteFileW(L"test456.txt");
    EXPECT(ok, true);

    f = redirect_CreateFileW(L".\\test456.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    f = redirect_CreateFileW(L"./test456.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);

    /* Potentially risky if we can't write to parent dir but we need to test ..
     * with wide paths.
     */
    f = redirect_CreateFileW(L"..\\test456.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(f != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(f);
    EXPECT(ok, TRUE);
}

static void
test_drive(void)
{
    BOOL ok;
    DWORD secs_per_cluster, bytes_per_sector, free_clusters, num_clusters;
    UINT type;

    ok = redirect_GetDiskFreeSpaceA("c:\\", &secs_per_cluster, &bytes_per_sector,
                                    &free_clusters, &num_clusters);
    EXPECT(ok, TRUE);
    /* on win7 at least, trailing \ not required, despite man page */
    ok = redirect_GetDiskFreeSpaceA("c:", &secs_per_cluster, &bytes_per_sector,
                                    &free_clusters, &num_clusters);
    EXPECT(ok, TRUE);
    ok = redirect_GetDiskFreeSpaceA("bogus\\relative\\path", &secs_per_cluster,
                                    &bytes_per_sector, &free_clusters, &num_clusters);
    EXPECT(ok, TRUE);
    /* test passing in a file instead of a dir */
    ok = redirect_GetDiskFreeSpaceA("c:\\windows\\system.ini", &secs_per_cluster,
                                    &bytes_per_sector, &free_clusters, &num_clusters);
    EXPECT(ok, FALSE);
    EXPECT(get_last_error(), ERROR_PATH_NOT_FOUND);
    ok = redirect_GetDiskFreeSpaceW(L"c:\\", &secs_per_cluster, &bytes_per_sector,
                                    &free_clusters, &num_clusters);
    EXPECT(ok, TRUE);
    ok = redirect_GetDiskFreeSpaceW(NULL, &secs_per_cluster, &bytes_per_sector,
                                    &free_clusters, &num_clusters);
    EXPECT(ok, TRUE);
    /* I manually tested \\server => ERROR_PATH_NOT_FOUND and \\server\share => ok */

    EXPECT(redirect_GetDriveTypeA("c:\\"), DRIVE_FIXED);
    type = redirect_GetDriveTypeA("bogus\\relative\\path");
    EXPECT(type == DRIVE_FIXED ||
               /* handle tester's cur dir being elsewhere */
               type == DRIVE_RAMDISK || type == DRIVE_REMOTE,
           true);
    type = redirect_GetDriveTypeA(NULL);
    EXPECT(type == DRIVE_FIXED ||
               /* handle tester's cur dir being elsewhere */
               type == DRIVE_RAMDISK || type == DRIVE_REMOTE,
           true);
    /* test passing in a file instead of a dir */
    EXPECT(redirect_GetDriveTypeA("c:\\windows\\system.ini"), DRIVE_NO_ROOT_DIR);
    EXPECT(redirect_GetDriveTypeW(L"c:\\"), DRIVE_FIXED);
    EXPECT(redirect_GetDriveTypeW(NULL), DRIVE_FIXED);
    EXPECT(redirect_GetDriveTypeW(L"\\\\bogusserver\\bogusshare"), DRIVE_NO_ROOT_DIR);
    EXPECT(get_last_error() == ERROR_NOT_A_REPARSE_POINT, true);
    /* manually tested \\server => DRIVE_NO_ROOT_DIR. \\server\share\ => DRIVE_REMOTE */
}

static void
test_handles(void)
{
    HANDLE h;
    BOOL ok;
    DWORD written;
    const char *msg = "GetStdHandle test\n";

    h = redirect_GetStdHandle(STD_ERROR_HANDLE);
    ok = redirect_WriteFile(h, msg, (DWORD)strlen(msg), &written, NULL);
    EXPECT(ok && written == strlen(msg), true);
}

void
unit_test_drwinapi_kernel32_file(void)
{
    print_file(STDERR, "testing drwinapi kernel32 file-related routines\n");

    test_directories();

    test_files();

    test_pipe();

    test_file_mapping();

    test_DeviceIoControl();

    test_file_times();

    test_find_file();

    test_file_paths();

    test_drive();

    test_handles();
}
#endif
