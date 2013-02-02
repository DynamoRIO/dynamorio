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
                           SECURITY_ATTRIBUTES *sa)
{
    ULONG obj_flags = OBJ_CASE_INSENSITIVE;
    if (sa != NULL && sa->nLength >= sizeof(SECURITY_ATTRIBUTES) && sa->bInheritHandle)
        obj_flags |= OBJ_INHERIT;
    InitializeObjectAttributes(oa, us, obj_flags, NULL,
                               (sa != NULL && sa->nLength >= 
                                offsetof(SECURITY_ATTRIBUTES, lpSecurityDescriptor) +
                                sizeof(sa->lpSecurityDescriptor)) ?
                               sa->lpSecurityDescriptor : NULL);
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

    init_object_attr_for_files(&oa, &file_path_unicode, lpSecurityAttributes);

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

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Files
 */


#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_kernel32_file(void)
{
    print_file(STDERR, "testing drwinapi kernel32 file-related routines\n");

    EXPECT(redirect_CreateDirectoryA("xyz:\\bogus\\name", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_PATH_NOT_FOUND);

    /* XXX: should look at SYSTEMDRIVE instead of assuming c:\windows exists */
    EXPECT(redirect_CreateDirectoryW(L"c:\\windows", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
}
#endif
