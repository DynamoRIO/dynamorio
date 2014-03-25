/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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

#include "share.h"

#ifndef WINDOWS
# error WINDOWS-only
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include "config.h"
#include "share.h"

#include <string.h>
#include <errno.h>
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "dr_frontend.h"

/* Semi-compatibility with the Windows CRT _access function.
 */
drfront_status_t
drfront_access(const char *fname, drfront_access_mode_t mode, OUT bool *ret)
{
    int r;
    int msdn_mode = 00;
    TCHAR wfname[MAX_PATH];
    drfront_status_t status_check = DRFRONT_ERROR;

    if (ret == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    status_check = drfront_char_to_tchar(fname, wfname, MAX_PATH);
    if (status_check != DRFRONT_SUCCESS) {
        *ret = false;
        return status_check;
    }

    /* Translate drfront_access_mode_t to msdn _waccess mode */
    if (TEST(mode, DRFRONT_WRITE))
        msdn_mode |= 02;
    if (TEST(mode, DRFRONT_READ))
        msdn_mode |= 04;

    r = _waccess(wfname, msdn_mode);

    if (r == -1) {
        *ret = false;
        if (GetLastError() == EACCES)
            return DRFRONT_SUCCESS;
        return DRFRONT_ERROR;
    }

    *ret = true;
    return DRFRONT_SUCCESS;
}

/* Implements a normal path search for fname on the paths in env_var. Resolves symlinks,
 * which is needed to get the right config filename (i#1062).
 */
drfront_status_t
drfront_searchenv(const char *fname, const char *env_var, OUT char *full_path,
                  const size_t full_path_size, OUT bool *ret)
{
    drfront_status_t status_check = DRFRONT_ERROR;
    size_t size_needed = 0;
    TCHAR wfname[MAX_PATH];
    /* XXX: Not sure what the size for environment variable names should be.
     *      Perhaps we want a drfront_char_to_tchar_size_needed
     */
    TCHAR wenv_var[MAX_PATH];
    TCHAR wfull_path[MAX_PATH];

    if (full_path == NULL && ret == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    status_check = drfront_char_to_tchar(fname, wfname, MAX_PATH);
    if (status_check != DRFRONT_SUCCESS) {
        *ret = false;
        return status_check;
    }
    drfront_char_to_tchar(env_var, wenv_var, MAX_PATH);
    if (status_check != DRFRONT_SUCCESS) {
        *ret = false;
        return status_check;
    }

    _wsearchenv(wfname, wenv_var, wfull_path);

    if (strlen(full_path) == 0) {
        *ret = false;
        return DRFRONT_ERROR;
    }

    status_check = drfront_tchar_to_char_size_needed(wfull_path, &size_needed);
    if (status_check != DRFRONT_SUCCESS) {
        *ret = false;
        return status_check;
    } else if (full_path_size < size_needed) {
        *ret = true;
        return DRFRONT_ERROR_INVALID_SIZE;
    }

    status_check = drfront_tchar_to_char(wfull_path, full_path, full_path_size);
    if (status_check != DRFRONT_SUCCESS) {
        *ret = false;
        return status_check;
    }
    full_path[full_path_size - 1] = '\0';
    *ret = true;
    return DRFRONT_SUCCESS;
}

/* always null-terminates */
drfront_status_t
drfront_tchar_to_char(const TCHAR *wstr, OUT char *buf, size_t buflen/*# elements*/)
{
    int res = WideCharToMultiByte(CP_UTF8, 0, wstr, -1/*null-term*/,
                                  buf, (int)buflen, NULL, NULL);
    if (res <= 0)
        return DRFRONT_ERROR;
    buf[buflen - 1] = '\0';
    return DRFRONT_SUCCESS;
}

/* includes the terminating null */
drfront_status_t
drfront_tchar_to_char_size_needed(const TCHAR *wstr, OUT size_t *needed)
{
    if (needed == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    *needed =  WideCharToMultiByte(CP_UTF8, 0, wstr, -1/*null-term*/, NULL, 0, NULL,
                                   NULL);
    return DRFRONT_SUCCESS;
}

/* always null-terminates */
drfront_status_t
drfront_char_to_tchar(const char *str, OUT TCHAR *wbuf, size_t wbuflen/*# elements*/)
{
    int res = MultiByteToWideChar(CP_UTF8, 0/*=>MB_PRECOMPOSED*/, str, -1/*null-term*/,
                                  wbuf, (int)wbuflen);
    if (res <= 0)
        return DRFRONT_ERROR;
    wbuf[wbuflen - 1] = L'\0';
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_get_env_var(const TCHAR *name, OUT char *buf, size_t buflen/*# elements*/)
{
    TCHAR wbuf[MAX_PATH];
    int len = GetEnvironmentVariable(name, wbuf, BUFFER_SIZE_ELEMENTS(wbuf));
    drfront_status_t status_check = DRFRONT_ERROR;
    if (len > 0) {
        status_check = drfront_tchar_to_char(wbuf, buf, buflen);
        return status_check;
    }
    return DRFRONT_ERROR;
}

drfront_status_t
drfront_get_absolute_path(const char *src, OUT char *buf, size_t buflen/*# elements*/)
{
    TCHAR wsrc[MAX_PATH];
    TCHAR wdst[MAX_PATH];
    drfront_status_t status_check = DRFRONT_ERROR;

    status_check = drfront_char_to_tchar(src, wsrc, BUFFER_SIZE_ELEMENTS(wsrc));
    if (status_check != DRFRONT_SUCCESS)
        return status_check;

    int res = GetFullPathName(wsrc, BUFFER_SIZE_ELEMENTS(wdst), wdst, NULL);
    if (res <= 0)
        return DRFRONT_ERROR;

    NULL_TERMINATE_BUFFER(wdst);
    status_check = drfront_tchar_to_char(wdst, buf, buflen);
    return status_check;
}

drfront_status_t
drfront_get_app_full_path(const char *app, OUT char *buf, size_t buflen/*# elements*/)
{
    TCHAR wbuf[MAX_PATH];
    TCHAR wapp[MAX_PATH];
    drfront_status_t status_check = DRFRONT_ERROR;

    status_check = drfront_char_to_tchar(app, wapp, BUFFER_SIZE_ELEMENTS(wapp));
    if (status_check != DRFRONT_SUCCESS)
        return status_check;

    _tsearchenv(wapp, _T("PATH"), wbuf);
    NULL_TERMINATE_BUFFER(wbuf);
    if (wbuf[0] == _T('\0')) {
        /* may need to append .exe, FIXME : other executable types */
        TCHAR tmp_buf[MAX_PATH];
        _sntprintf(tmp_buf, BUFFER_SIZE_ELEMENTS(tmp_buf), _T("%s%s"), wapp, _T(".exe"));
        NULL_TERMINATE_BUFFER(wbuf);
        _tsearchenv(tmp_buf, _T("PATH"), wbuf);
    }
    if (wbuf[0] == _T('\0')) {
        /* last try: expand w/ cur dir */
        GetFullPathName(wapp, BUFFER_SIZE_ELEMENTS(wbuf), wbuf, NULL);
        NULL_TERMINATE_BUFFER(wbuf);
    }
    status_check = drfront_tchar_to_char(wbuf, buf, buflen);
    return status_check;
}

/* On failure returns INVALID_HANDLE_VALUE.
 * On success returns a file handle which must be closed via CloseHandle()
 * by the caller.
 */
static HANDLE
read_nt_headers(const char *exe, IMAGE_NT_HEADERS *nt)
{
    HANDLE f;
    DWORD offs;
    DWORD read;
    IMAGE_DOS_HEADER dos;
    TCHAR wexe[MAX_PATH];
    drfront_status_t status_check = DRFRONT_ERROR;

    status_check = drfront_char_to_tchar(exe, wexe, BUFFER_SIZE_ELEMENTS(wexe));
    if (status_check != DRFRONT_SUCCESS)
        return INVALID_HANDLE_VALUE;

    f = CreateFile(wexe, GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        goto read_nt_headers_error;
    if (!ReadFile(f, &dos, sizeof(dos), &read, NULL) ||
        read != sizeof(dos) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE)
        goto read_nt_headers_error;
    offs = SetFilePointer(f, dos.e_lfanew, NULL, FILE_BEGIN);
    if (offs == INVALID_SET_FILE_POINTER)
        goto read_nt_headers_error;
    if (!ReadFile(f, nt, sizeof(*nt), &read, NULL) ||
        read != sizeof(*nt) ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        goto read_nt_headers_error;
    return f;
 read_nt_headers_error:
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    return INVALID_HANDLE_VALUE;
}

drfront_status_t
drfront_is_64bit_app(const char *exe, OUT bool *is_64)
{
    bool res = false;
    IMAGE_NT_HEADERS nt;
    HANDLE f = NULL;

    if (is_64 == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    f = read_nt_headers(exe, &nt);
    if (f == INVALID_HANDLE_VALUE) {
        *is_64 = res;
        return DRFRONT_ERROR_INVALID_PARAMETER;
    }
    res = (nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    CloseHandle(f);
    *is_64 = res;
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_is_graphical_app(const char *exe, OUT bool *is_graphical)
{
    /* reads the PE headers to see whether the given image is a graphical app */
    bool res = false; /* err on side of console */
    IMAGE_NT_HEADERS nt;
    HANDLE f = NULL;

    if (is_graphical == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    f = read_nt_headers(exe, &nt);
    if (f == INVALID_HANDLE_VALUE) {
        *is_graphical = res;
        return DRFRONT_ERROR_INVALID_PARAMETER;
    }
    res = (nt.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI);
    CloseHandle(f);
    *is_graphical = res;
    return DRFRONT_SUCCESS;
}
