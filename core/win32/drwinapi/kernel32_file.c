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

static HANDLE base_named_obj_dir;
static HANDLE base_named_pipe_dir;

static wchar_t *
get_base_named_obj_dir_name(void)
{
    /* PEB.ReadOnlyStaticServerData has an array of pointers sized to match the
     * kernel (so 64-bit for WOW64).  The second pointer points at a
     * BASE_STATIC_SERVER_DATA structure.
     *
     * If this proves fragile in the future, AFAIK we could construct this:
     * + Prior to Vista, just use BASE_NAMED_OBJECTS;
     * + On Vista+, use L"\Sessions\N\BaseNamedObjects"
     *   where N = PEB.SessionId.
     *
     * The Windows library code BaseGetNamedObjectDirectory() seems to
     * deal with TEB->IsImpersonating, but by initializing at startup
     * here and not lazily I'm hoping we can avoid that complexity
     * (XXX: what about attach?).
     */
    byte *ptr = (byte *) get_peb(NT_CURRENT_PROCESS)->ReadOnlyStaticServerData;
#ifndef X64
    if (is_wow64_process(NT_CURRENT_PROCESS)) {
        BASE_STATIC_SERVER_DATA_64 *data =
            *(BASE_STATIC_SERVER_DATA_64 **)(ptr + 2*sizeof(void*));
        /* we assume null-terminated */
        return data->NamedObjectDirectory.Buffer;
    } else
#endif
        {
            BASE_STATIC_SERVER_DATA *data =
                *(BASE_STATIC_SERVER_DATA **)(ptr + sizeof(void*));
            /* we assume null-terminated */
            return data->NamedObjectDirectory.Buffer;
        }
}

void
kernel32_redir_init_file(void)
{
    NTSTATUS res = nt_open_object_directory(&base_named_obj_dir,
                                            get_base_named_obj_dir_name(),
                                            true/*create perms*/);
    ASSERT(NT_SUCCESS(res));

    /* The trailing \ is critical: w/o it, NtCreateNamedPipeFile returns
     * STATUS_OBJECT_NAME_INVALID.
     */
    res = nt_open_file(&base_named_pipe_dir, L"\\Device\\NamedPipe\\",
                       GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE);
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

/***************************************************************************
 * DIRECTORIES
 */

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

BOOL
WINAPI
redirect_DeleteFileA(
    __in LPCSTR lpFileName
    )
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

BOOL
WINAPI
redirect_DeleteFileW(
    __in LPCWSTR lpFileName
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
    return redirect_DeleteFileA(buf);
}


/***************************************************************************
 * FILE MAPPING
 */

/* Xref os_map_file() which this code is modeled on, though here we
 * have to support anonymous mappings as well.
 */

HANDLE
WINAPI
redirect_CreateFileMappingA(
    __in     HANDLE hFile,
    __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    __in     DWORD flProtect,
    __in     DWORD dwMaximumSizeHigh,
    __in     DWORD dwMaximumSizeLow,
    __in_opt LPCSTR lpName
    )
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
redirect_CreateFileMappingW(
    __in     HANDLE hFile,
    __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    __in     DWORD flProtect,
    __in     DWORD dwMaximumSizeHigh,
    __in     DWORD dwMaximumSizeLow,
    __in_opt LPCWSTR lpName
    )
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

    res = nt_create_section(&section, access,
                            (dwMaximumSizeHigh == 0 && dwMaximumSizeLow == 0) ?
                            NULL : &li_size,
                            prot, section_flags,
                            (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile,
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
redirect_MapViewOfFile(
    __in HANDLE hFileMappingObject,
    __in DWORD dwDesiredAccess,
    __in DWORD dwFileOffsetHigh,
    __in DWORD dwFileOffsetLow,
    __in SIZE_T dwNumberOfBytesToMap
    )
{
    return redirect_MapViewOfFileEx(hFileMappingObject, dwDesiredAccess,
                                    dwFileOffsetHigh, dwFileOffsetLow,
                                    dwNumberOfBytesToMap, NULL);
}

LPVOID
WINAPI
redirect_MapViewOfFileEx(
    __in     HANDLE hFileMappingObject,
    __in     DWORD dwDesiredAccess,
    __in     DWORD dwFileOffsetHigh,
    __in     DWORD dwFileOffsetLow,
    __in     SIZE_T dwNumberOfBytesToMap,
    __in_opt LPVOID lpBaseAddress
    )
{
    NTSTATUS res;
    SIZE_T size = dwNumberOfBytesToMap;
    LPVOID map = lpBaseAddress;
    ULONG prot = 0;
    LARGE_INTEGER li_offs;
    li_offs.LowPart = dwFileOffsetLow;
    li_offs.HighPart = dwFileOffsetHigh;

    /* Easiest to deal w/ our bitmasks and then convert: */
    if (TESTANY(FILE_MAP_READ|FILE_MAP_WRITE|FILE_MAP_COPY, dwDesiredAccess))
        prot |= MEMPROT_READ;
    if (TEST(FILE_MAP_WRITE, dwDesiredAccess))
        prot |= FILE_MAP_WRITE;
    if (TEST(FILE_MAP_EXECUTE, dwDesiredAccess))
        prot |= MEMPROT_EXEC;
    prot = memprot_to_osprot(prot);
    if (TEST(FILE_MAP_COPY, dwDesiredAccess))
        prot = osprot_add_writecopy(prot);

    res = nt_raw_MapViewOfSection(hFileMappingObject, NT_CURRENT_PROCESS, &map,
                                  0, /* no control over placement */
                                  /* if section created SEC_COMMIT, will all be
                                   * committed automatically 
                                   */
                                  0,
                                  &li_offs, &size,
                                  ViewShare, /* not exposed */
                                  0 /* no special top-down or anything */,
                                  prot);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return NULL;
    }
    return map;
}

BOOL
WINAPI
redirect_UnmapViewOfFile(
    __in LPCVOID lpBaseAddress
    )
{
    NTSTATUS res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, (void *)lpBaseAddress);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL
WINAPI
redirect_CreatePipe(
    __out_ecount_full(1) PHANDLE hReadPipe,
    __out_ecount_full(1) PHANDLE hWritePipe,
    __in_opt LPSECURITY_ATTRIBUTES lpPipeAttributes,
    __in     DWORD nSize
    )
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us = {0,};
    IO_STATUS_BLOCK iob = {0,0};
    ACCESS_MASK access = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
    DWORD size = (nSize != 0) ? nSize : PAGE_SIZE; /* default size */
    LARGE_INTEGER timeout;
    GET_NTDLL(NtCreateNamedPipeFile,
              (OUT PHANDLE FileHandle,
               IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes,
               OUT PIO_STATUS_BLOCK IoStatusBlock,
               IN ULONG ShareAccess,
               IN ULONG CreateDisposition,
               IN ULONG CreateOptions,
               /* XXX: when these are BOOLEAN, as Nebbett has them, we just
                * set the LSB and we get STATUS_INVALID_PARAMETER!
                * So I'm considering to be BOOOL.
                */
               IN BOOL TypeMessage,
               IN BOOL ReadmodeMessage,
               IN BOOL Nonblocking,
               IN ULONG MaxInstances,
               IN ULONG InBufferSize,
               IN ULONG OutBufferSize,
               IN PLARGE_INTEGER DefaultTimeout OPTIONAL));

    timeout.QuadPart = -1200000000; /* 120s */

    /* We leave us with 0 length and NULL buffer b/c we don't want a name. */
    init_object_attr_for_files(&oa, &us, lpPipeAttributes, NULL);
    oa.RootDirectory = base_named_pipe_dir;
    res = NtCreateNamedPipeFile(hReadPipe, access, &oa, &iob,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                FILE_CREATE,
                                FILE_SYNCHRONOUS_IO_NONALERT,
                                FILE_PIPE_BYTE_STREAM_TYPE,
                                FILE_PIPE_BYTE_STREAM_MODE,
                                FILE_PIPE_QUEUE_OPERATION,
                                1, size, size, &timeout);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    /* open the write handle */
    oa.RootDirectory = *hReadPipe;
    res = nt_raw_OpenFile(hWritePipe, SYNCHRONIZE | FILE_GENERIC_WRITE,
                          &oa, &iob, FILE_SHARE_READ,
                          FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(res)) {
        close_handle(*hReadPipe);
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
void
unit_test_drwinapi_kernel32_file(void)
{
    HANDLE h, h2;
    PVOID p;
    BOOL ok;
    char env[MAX_PATH];
    char buf[MAX_PATH];
    int res;

    print_file(STDERR, "testing drwinapi kernel32 file-related routines\n");

    res = GetEnvironmentVariableA("TMP", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(env);

    /* test directories */
    EXPECT(redirect_CreateDirectoryA("xyz:\\bogus\\name", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_PATH_NOT_FOUND);

    /* XXX: should look at SYSTEMDRIVE instead of assuming c:\windows exists */
    EXPECT(redirect_CreateDirectoryW(L"c:\\windows", NULL), FALSE);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);

    /* test creating files */
    h = redirect_CreateFileW(L"c:\\_kernel32_file_test_bogus.txt",
                             GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h == INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error(), ERROR_FILE_NOT_FOUND);

    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
                    "%s\\_kernel32_file_test_temp.txt", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
    /* clobber it and ensure we give the right errno */
    h2 = redirect_CreateFileA(buf, GENERIC_WRITE,
                              FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|
                              FILE_FLAG_DELETE_ON_CLOSE, NULL);
    EXPECT(h2 != INVALID_HANDLE_VALUE, true);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    /* re-create and then test deleting it */
    h = redirect_CreateFileA(buf, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
    ok = redirect_DeleteFileA(buf);
    EXPECT(ok, true);

    /* test anonymous mappings */
    h2 = CreateEvent(NULL, TRUE, TRUE, "Local\\mymapping"); /* for conflict */
    EXPECT(h2 != NULL, true);
    h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                           0, 0x1000, "Local\\mymapping");
    h = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, 0x1000, "Local\\mymapping");
    EXPECT(h == NULL, true);
    EXPECT(get_last_error(), ERROR_INVALID_HANDLE);
    CloseHandle(h2); /* removes conflict */
    h = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, 0x1000, "Local\\mymapping");
    EXPECT(h != NULL, true);
    h2 = redirect_CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, 0x1000, "Local\\mymapping");
    EXPECT(h2 != NULL, true);
    EXPECT(get_last_error(), ERROR_ALREADY_EXISTS);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    p = redirect_MapViewOfFileEx(h, FILE_MAP_WRITE, 0, 0, 0x800, NULL);
    EXPECT(p != NULL, true);
    *(int *)p = 42; /* test writing: shouldn't crash */
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);

    /* test file mappings */
    res = GetEnvironmentVariableA("SystemRoot", env, BUFFER_SIZE_ELEMENTS(env));
    EXPECT(res > 0, true);
    NULL_TERMINATE_BUFFER(env);
    res = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
                    "%s\\system32\\notepad.exe", env);
    EXPECT(res > 0 && res < BUFFER_SIZE_ELEMENTS(buf), true);
    NULL_TERMINATE_BUFFER(buf);
    h = redirect_CreateFileA(buf, GENERIC_READ, 0, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT(h != INVALID_HANDLE_VALUE, true);
    h2 = redirect_CreateFileMappingA(h, NULL, PAGE_READONLY, 0, 0, NULL);
    EXPECT(h2 != NULL, true);
    p = redirect_MapViewOfFileEx(h2, FILE_MAP_READ, 0, 0, 0, NULL);
    EXPECT(p != NULL, true);
    EXPECT(*(WORD *)p == IMAGE_DOS_SIGNATURE, true); /* PE image */
    ok = redirect_UnmapViewOfFile(p);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);

    /* test pipe */
    ok = redirect_CreatePipe(&h, &h2, NULL, 0);
    EXPECT(ok, true);
    /* FIXME: once we redirect ReadFile and WriteFile, use those versions */
    /* This will block if the buffer is full, but we assume the buffer
     * is much bigger than the size of a handle for our single-threaded test.
     */
    ok = WriteFile(h2, &h2, sizeof(h2), (LPDWORD) &res, NULL);
    EXPECT(ok, true);
    ok = ReadFile(h, &p, sizeof(p), (LPDWORD) &res, NULL);
    EXPECT(ok, true);
    EXPECT((HANDLE)p == h2, true);
    ok = redirect_CloseHandle(h2);
    EXPECT(ok, true);
    ok = redirect_CloseHandle(h);
    EXPECT(ok, true);
}
#endif
