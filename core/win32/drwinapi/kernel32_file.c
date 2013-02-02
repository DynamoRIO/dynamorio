/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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

static HANDLE (WINAPI *priv_kernel32_OpenConsoleW)(LPCWSTR, DWORD, BOOL, DWORD);

void
kernel32_redir_onload_file(privmod_t *mod)
{
    priv_kernel32_OpenConsoleW = (HANDLE (WINAPI *)(LPCWSTR, DWORD, BOOL, DWORD))
        get_proc_address_ex(mod->base, "OpenConsoleW", NULL);
}

BOOL
WINAPI
redirect_CloseHandle(
    __in HANDLE hObject
    )
{
    return (BOOL) close_handle(hObject);
}

static void
init_object_attr_for_files(OBJECT_ATTRIBUTES *oa, UNICODE_STRING *us,
                           SECURITY_ATTRIBUTES *sa,
                           SECURITY_QUALITY_OF_SERVICE *sqos)
{
    ULONG obj_flags = OBJ_CASE_INSENSITIVE;
    if (sa != NULL && sa->nLength >= sizeof(SECURITY_ATTRIBUTES) && sa->bInheritHandle)
        obj_flags |= OBJ_INHERIT;
    InitializeObjectAttributes(oa, us, obj_flags, NULL,
                               (sa != NULL && sa->nLength >= 
                                offsetof(SECURITY_ATTRIBUTES, lpSecurityDescriptor) +
                                sizeof(sa->lpSecurityDescriptor)) ?
                               sa->lpSecurityDescriptor : NULL);
    if (sqos != NULL)
        oa->SecurityQualityOfService = sqos;
}

BOOL
WINAPI
redirect_CreateDirectoryA(
    __in     LPCSTR lpPathName,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes
    )
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iob = {0,0};
    UNICODE_STRING file_path_unicode;
    HANDLE handle;
    ACCESS_MASK access = SYNCHRONIZE | FILE_LIST_DIRECTORY;
    ULONG file_attributes = FILE_ATTRIBUTE_NORMAL;
    ULONG sharing = FILE_SHARE_READ;
    ULONG disposition = FILE_CREATE;
    ULONG options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE |
        FILE_OPEN_FOR_BACKUP_INTENT/*docs say to use for dir handle*/;
    wchar_t wbuf[MAX_PATH];

    if (lpPathName == NULL ||
        !convert_to_NT_file_path(wbuf, lpPathName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */

    res = wchar_to_unicode(&file_path_unicode, wbuf);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    init_object_attr_for_files(&oa, &file_path_unicode, lpSecurityAttributes, NULL);

    res = nt_raw_CreateFile(&handle, access, &oa, &iob, NULL, file_attributes,
                            sharing, disposition, options, NULL, 0);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    close_handle(handle);
    return TRUE;
}


BOOL
WINAPI
redirect_CreateDirectoryW(
    __in     LPCWSTR lpPathName,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes
    )
{
    /* convert_to_NT_file_path takes in UTF-8 and converts back to UTF-16.
     * XXX: have a convert_to_NT_file_path_wide() or sthg to avoid double conversion.
     */
    char buf[MAX_PATH];
    int len = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%ls", lpPathName);
    if (len <= 0 || len >= BUFFER_SIZE_ELEMENTS(buf)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(buf);
    return redirect_CreateDirectoryA(buf, lpSecurityAttributes);
}

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

__out
HANDLE
WINAPI
redirect_CreateFileA(
    __in     LPCSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile
    )
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iob = {0,0};
    UNICODE_STRING file_path_unicode;
    SECURITY_QUALITY_OF_SERVICE sqos, *sqos_ptr = NULL;
    HANDLE handle;
    ACCESS_MASK access;
    ULONG file_attributes;
    ULONG sharing = dwShareMode;
    ULONG disposition;
    ULONG options;
    wchar_t wbuf[MAX_PATH];

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
        sqos.ImpersonationLevel = (dwFlagsAndAttributes >> 16) & 0x3;;
        if (TEST(SECURITY_CONTEXT_TRACKING, dwFlagsAndAttributes))
            sqos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
        else
            sqos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
        sqos.EffectiveOnly = TEST(SECURITY_EFFECTIVE_ONLY, dwFlagsAndAttributes);
    }

    /* map the non-FILE_ATTRIBUTE_* flags */
    options = file_options_to_nt(dwFlagsAndAttributes, &access);

    if (lpFileName == NULL ||
        !convert_to_NT_file_path(wbuf, lpFileName, BUFFER_SIZE_ELEMENTS(wbuf))) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */

    res = wchar_to_unicode(&file_path_unicode, wbuf);
    if (!NT_SUCCESS(res)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    if (strcmp(lpFileName, "CONIN$") == 0 || strcmp(lpFileName, "CONOUT$") == 0) {
        /* route to OpenConsole */
        SYSLOG_INTERNAL_WARNING_ONCE("priv lib called CreateFile on the console");
        ASSERT(priv_kernel32_OpenConsoleW != NULL);
        return (*priv_kernel32_OpenConsoleW)(wbuf, dwDesiredAccess,
                                             (lpSecurityAttributes == NULL) ? FALSE :
                                             lpSecurityAttributes->bInheritHandle,
                                             OPEN_EXISTING);
    }

    if (hTemplateFile != NULL) {
        /* FIXME: copy extended attributes */
        ASSERT_NOT_IMPLEMENTED(false);
    }

    init_object_attr_for_files(&oa, &file_path_unicode, lpSecurityAttributes, sqos_ptr);

    res = nt_raw_CreateFile(&handle, access, &oa, &iob, NULL, file_attributes,
                            sharing, disposition, options, NULL, 0);
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
            (dwCreationDisposition == OPEN_ALWAYS &&
             iob.Information == FILE_OPENED))
            set_last_error(ERROR_ALREADY_EXISTS);
        else
            set_last_error(ERROR_SUCCESS);
        return handle;
    }
}

__out
HANDLE
WINAPI
redirect_CreateFileW(
    __in     LPCWSTR lpFileName,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwShareMode,
    __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    __in     DWORD dwCreationDisposition,
    __in     DWORD dwFlagsAndAttributes,
    __in_opt HANDLE hTemplateFile
    )
{
    /* convert_to_NT_file_path takes in UTF-8 and converts back to UTF-16.
     * XXX: have a convert_to_NT_file_path_wide() or sthg to avoid double conversion.
     */
    char buf[MAX_PATH];
    int len = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%ls", lpFileName);
    if (len <= 0 || len >= BUFFER_SIZE_ELEMENTS(buf)) {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    NULL_TERMINATE_BUFFER(buf);
    return redirect_CreateFileA(buf, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                dwCreationDisposition, dwFlagsAndAttributes,
                                hTemplateFile);
}

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Files
 */


#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_file(void)
{
    HANDLE h, h2;
    char tmpdir[MAX_PATH];
    char buf[MAX_PATH];
    int res;

    print_file(STDERR, "testing drwinapi kernel32 file-related routines\n");

    res = GetEnvironmentVariableA("TMP", tmpdir, BUFFER_SIZE_ELEMENTS(tmpdir));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(tmpdir);

    EXPECT(redirect_CreateDirectoryA("xyz:\\bogus\\name", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_PATH_NOT_FOUND);

    /* XXX: should look at SYSTEMDRIVE instead of assuming c:\windows exists */
    EXPECT(redirect_CreateDirectoryW(L"c:\\windows", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);

    h = redirect_CreateFileW(L"c:\\cygwin\\tmp\\_kernel32_file_test_bogus.txt",
                             GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h == INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error(), ERROR_FILE_NOT_FOUND);

    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
                    "%s\\_kernel32_file_test_temp.txt", tmpdir);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    redirect_CloseHandle(h);
    /* clobber it and ensure we give the right errno */
    h2 = redirect_CreateFileA(buf, GENERIC_WRITE,
                              FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|
                              FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(h2 != INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
    redirect_CloseHandle(h2);
}
#endif
