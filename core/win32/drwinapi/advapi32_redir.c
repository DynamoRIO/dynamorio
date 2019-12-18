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

/* advapi32 redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "drwinapi.h"
#include "drwinapi_private.h"
#include "advapi32_redir.h"

#ifndef WINDOWS
#    error Windows-only
#endif

#define MAX_REG_KEY_NAME_LEN 255 /* from docs: don't see in headers */

/* We use a hashtale for faster lookups than a linear walk */
static strhash_table_t *advapi32_table;

static const redirect_import_t redirect_advapi32[] = {
    { "RegCloseKey", (app_pc)redirect_RegCloseKey },
    { "RegOpenKeyExA", (app_pc)redirect_RegOpenKeyExA },
    { "RegOpenKeyExW", (app_pc)redirect_RegOpenKeyExW },
    { "RegQueryValueExA", (app_pc)redirect_RegQueryValueExA },
    { "RegQueryValueExW", (app_pc)redirect_RegQueryValueExW },
};
#define REDIRECT_ADVAPI32_NUM (sizeof(redirect_advapi32) / sizeof(redirect_advapi32[0]))

void
advapi32_redir_init(void)
{
    uint i;
    advapi32_table = strhash_hash_create(
        GLOBAL_DCONTEXT, hashtable_num_bits(REDIRECT_ADVAPI32_NUM * 2),
        80 /* load factor: not perf-critical, plus static */,
        HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
        NULL _IF_DEBUG("advapi32 redirection table"));
    TABLE_RWLOCK(advapi32_table, write, lock);
    for (i = 0; i < REDIRECT_ADVAPI32_NUM; i++) {
        strhash_hash_add(GLOBAL_DCONTEXT, advapi32_table, redirect_advapi32[i].name,
                         (void *)redirect_advapi32[i].func);
    }
    TABLE_RWLOCK(advapi32_table, write, unlock);
}

void
advapi32_redir_exit(void)
{
    strhash_hash_destroy(GLOBAL_DCONTEXT, advapi32_table);
}

void
advapi32_redir_onload(privmod_t *mod)
{
    /* nothing yet */
}

app_pc
advapi32_redir_lookup(const char *name)
{
    app_pc res;
    TABLE_RWLOCK(advapi32_table, read, lock);
    res = strhash_hash_lookup(GLOBAL_DCONTEXT, advapi32_table, name);
    TABLE_RWLOCK(advapi32_table, read, unlock);
    return res;
}

LSTATUS
WINAPI
redirect_RegCloseKey(__in HKEY hKey)
{
    NTSTATUS res = nt_raw_close(hKey);
    return ntstatus_to_last_error(res);
}

LSTATUS
WINAPI
redirect_RegOpenKeyExA(__in HKEY hKey, __in_opt LPCSTR lpSubKey, __in_opt DWORD ulOptions,
                       __in REGSAM samDesired, __out PHKEY phkResult)
{
    wchar_t *wkey;
    wchar_t wbuf[MAX_REG_KEY_NAME_LEN];
    int len;
    if (lpSubKey != NULL) {
        len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpSubKey);
        if (len < 0 || len >= BUFFER_SIZE_ELEMENTS(wbuf))
            return ERROR_INVALID_PARAMETER;
        NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */
        wkey = wbuf;
    } else
        wkey = NULL;
    return redirect_RegOpenKeyExW(hKey, wkey, ulOptions, samDesired, phkResult);
}

static NTSTATUS
open_key_common(HKEY parent_key, LPCWSTR subkey, DWORD access, PHKEY key OUT)
{
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS res = wchar_to_unicode(&us, subkey);
    if (!NT_SUCCESS(res))
        return res;
    InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, parent_key, NULL);
    res = nt_raw_OpenKey(key, access, &oa);
    return ntstatus_to_last_error(res);
}

static bool
key_is_special(HKEY key)
{
    return (key == HKEY_LOCAL_MACHINE || key == HKEY_CURRENT_USER ||
            key == HKEY_CURRENT_CONFIG || key == HKEY_CLASSES_ROOT || key == HKEY_USERS);
}

/* Caller needs to close the key if key_is_special(key) */
static NTSTATUS
special_to_handle(HKEY key, HKEY *special_key OUT)
{
    wchar_t entry[MAX_REG_KEY_NAME_LEN];
    int len;
    NTSTATUS res;
    if (key == HKEY_CURRENT_USER) {
        wchar_t sid[MAX_REG_KEY_NAME_LEN];
        res = get_current_user_SID(sid, sizeof(sid));
        if (!NT_SUCCESS(res))
            return res;
        len =
            _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry), L"\\Registry\\User\\%s", sid);
    } else {
        if (key == HKEY_LOCAL_MACHINE) {
            len = _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry), L"\\Registry\\Machine");
        } else if (key == HKEY_CURRENT_CONFIG) {
            len = _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry),
                             L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                             L"Hardware Profiles\\Current");
        } else if (key == HKEY_CLASSES_ROOT) {
            len = _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry),
                             L"\\Registry\\Machine\\Software\\CLASSES");
        } else if (key == HKEY_USERS) {
            len = _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry), L"\\Registry\\User");
        } else if (key == HKEY_PERFORMANCE_DATA || key == HKEY_PERFORMANCE_NLSTEXT ||
                   key == HKEY_PERFORMANCE_TEXT) {
            /* XXX: NYI */
            return STATUS_NOT_IMPLEMENTED;
        } else {
            *special_key = key;
            return STATUS_SUCCESS;
        }
    }
    if (len < 0 || len >= BUFFER_SIZE_ELEMENTS(entry))
        return STATUS_BUFFER_TOO_SMALL; /* XXX: what should we return? */
    NULL_TERMINATE_BUFFER(entry);
    return open_key_common(NULL, entry, MAXIMUM_ALLOWED, special_key);
}

static void
key_close_special(HKEY orig_key, HKEY resolved_key)
{
    if (key_is_special(orig_key))
        redirect_RegCloseKey(resolved_key);
}

LSTATUS
WINAPI
redirect_RegOpenKeyExW(__in HKEY hKey, __in_opt LPCWSTR lpSubKey,
                       __in_opt DWORD ulOptions, __in REGSAM samDesired,
                       __out PHKEY phkResult)
{
    NTSTATUS res;
    HKEY parent_key;

    if (ulOptions != 0 || phkResult == NULL ||
        /* lpSubKey can only be NULL if using a special key */
        (lpSubKey == NULL && !key_is_special(hKey)))
        return ERROR_INVALID_PARAMETER;

    res = special_to_handle(hKey, &parent_key);
    if (!NT_SUCCESS(res))
        return ntstatus_to_last_error(res);

    if ((lpSubKey == NULL && hKey == HKEY_CLASSES_ROOT) ||
        (*lpSubKey == L'\0' && hKey != HKEY_CLASSES_ROOT)) {
        /* Return a new handle */
        res = duplicate_handle(NT_CURRENT_PROCESS, parent_key, NT_CURRENT_PROCESS,
                               phkResult, SYNCHRONIZE, 0, 0);
        key_close_special(hKey, parent_key);
        return ntstatus_to_last_error(res);
    } else if ((lpSubKey == NULL && hKey != HKEY_CLASSES_ROOT) ||
               (*lpSubKey == L'\0' && hKey == HKEY_CLASSES_ROOT)) {
        /* Return hKey back */
        *phkResult = hKey;
        key_close_special(hKey, parent_key);
        return ERROR_SUCCESS;
    }

    res = open_key_common(parent_key, lpSubKey, samDesired, phkResult);
    key_close_special(hKey, parent_key);
    return ntstatus_to_last_error(res);
}

LSTATUS
WINAPI
redirect_RegQueryValueExA(__in HKEY hKey, __in_opt LPCSTR lpValueName,
                          __reserved LPDWORD lpReserved, __out_opt LPDWORD lpType,
                          __out_bcount_part_opt(*lpcbData, *lpcbData)
                              __out_data_source(REGISTRY) LPBYTE lpData,
                          __inout_opt LPDWORD lpcbData)
{
    NTSTATUS res;
    wchar_t wbuf[MAX_REG_KEY_NAME_LEN];
    DWORD type;
    DWORD max_len = 0;
    int len = _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", lpValueName);
    if (len < 0 || len >= BUFFER_SIZE_ELEMENTS(wbuf))
        return ERROR_INVALID_PARAMETER;
    NULL_TERMINATE_BUFFER(wbuf); /* be paranoid */
    if (lpcbData != NULL)
        max_len = *lpcbData;
    res = redirect_RegQueryValueExW(hKey, wbuf, lpReserved, &type, lpData, lpcbData);
    if (res != ERROR_SUCCESS)
        return res;
    if (lpType != NULL)
        *lpType = type;
    if (lpData != NULL &&
        (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)) {
        /* convert to narrow */
        char *buf = redirect_HeapAlloc(redirect_GetProcessHeap(), 0, max_len);
        size_t sofar = 0, prev_sofar;
        if (buf == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;
        /* using .* in case not null-terminated */
        do { /* loop for REG_MULTI_SZ */
            prev_sofar = sofar;
            if (!print_to_buffer(buf, max_len, &sofar, "%.*ls", *lpcbData - sofar,
                                 ((char *)lpData) + sofar * sizeof(wchar_t))) {
                /* XXX: we don't know what to set *lpcbData to: really shouldn't happen */
                return ERROR_MORE_DATA;
            }
            sofar++; /* include, and skip, the null between strings */
        } while (type == REG_MULTI_SZ && sofar > prev_sofar + 1 && sofar < *lpcbData);
        memcpy(lpData, buf, *lpcbData);
        redirect_HeapFree(redirect_GetProcessHeap(), 0, buf);
    }
    return ERROR_SUCCESS;
}

LSTATUS
WINAPI
redirect_RegQueryValueExW(__in HKEY hKey, __in_opt LPCWSTR lpValueName,
                          __reserved LPDWORD lpReserved, __out_opt LPDWORD lpType,
                          __out_bcount_part_opt(*lpcbData, *lpcbData)
                              __out_data_source(REGISTRY) LPBYTE lpData,
                          __inout_opt LPDWORD lpcbData)
{
    UNICODE_STRING us;
    NTSTATUS res;
    HKEY key;
    KEY_VALUE_PARTIAL_INFORMATION *kvpi;
    /* we first try with a stack buffer before dynamically allocating */
    char buf[128];
    ULONG kvpi_sz, res_sz;

    if (lpReserved != NULL || hKey == NULL || (lpData != NULL && lpcbData == NULL))
        return ERROR_INVALID_PARAMETER;

    res = wchar_to_unicode(&us, (lpValueName == NULL) ? L"" : lpValueName);
    if (!NT_SUCCESS(res))
        return ntstatus_to_last_error(res);

    res = special_to_handle(hKey, &key);
    if (!NT_SUCCESS(res))
        return ntstatus_to_last_error(res);

    kvpi = (KEY_VALUE_PARTIAL_INFORMATION *)buf;
    kvpi_sz = BUFFER_SIZE_BYTES(buf);

    res =
        nt_query_value_key(key, &us, KeyValuePartialInformation, kvpi, kvpi_sz, &res_sz);
    /* while loop in case of race */
    while (lpData != NULL && res == STATUS_BUFFER_OVERFLOW &&
           *lpcbData >= res_sz - offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data)) {
        if (kvpi != (KEY_VALUE_PARTIAL_INFORMATION *)buf)
            redirect_HeapFree(redirect_GetProcessHeap(), 0, kvpi);
        kvpi_sz = res_sz;
        kvpi = redirect_HeapAlloc(redirect_GetProcessHeap(), 0, kvpi_sz);
        if (kvpi == NULL) {
            key_close_special(hKey, key);
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        res = nt_query_value_key(key, &us, KeyValuePartialInformation, kvpi, kvpi_sz,
                                 &res_sz);
    }

    if (lpcbData != NULL) {
        if (*lpcbData < res_sz - offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data))
            res = STATUS_BUFFER_OVERFLOW;
        *lpcbData = res_sz - offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data);
    }

    if (NT_SUCCESS(res)) {
        if (lpType != NULL)
            *lpType = kvpi->Type;
        if (lpData != NULL) {
            memcpy(lpData, &kvpi->Data,
                   res_sz - offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data));
        }
    }

    if (kvpi != (KEY_VALUE_PARTIAL_INFORMATION *)buf)
        redirect_HeapFree(redirect_GetProcessHeap(), 0, kvpi);
    key_close_special(hKey, key);
    return ntstatus_to_last_error(res);
}

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_advapi32(void)
{
    LSTATUS res;
    HKEY key;
    DWORD type, size, handle_count = 0;
    /* NetworkService gets bigger then 512 so we go for 1024 */
#    define REG_KEY_DATA_SZ 1024
    char buf[REG_KEY_DATA_SZ];
    BOOL ok;

    print_file(STDERR, "testing drwinapi advapi32\n");

    if (get_os_version() >= WINDOWS_VERSION_XP) {
        /* Ensure we don't have handle leaks.
         * GetProcessHandleCount is not available on Win2K or NT.
         */
        ok = GetProcessHandleCount(GetCurrentProcess(), &handle_count);
        EXPECT(ok, true);
    }

    res = redirect_RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                                 KEY_READ, &key);
    EXPECT(res == ERROR_SUCCESS, true);
    size = BUFFER_SIZE_BYTES(buf);
    res = redirect_RegQueryValueExA(key, "SystemRoot", 0, &type, (LPBYTE)buf, &size);
    EXPECT(res == ERROR_SUCCESS, true);
    EXPECT(type == REG_SZ, true);
    EXPECT(strstr(buf, "Windows") != NULL || strstr(buf, "WINDOWS") != NULL ||
               /* Appveyor's Server 2012 R2 is all lower-case.
                * If we had a stristr I'd use it here.
                */
               strstr(buf, "windows") != NULL,
           true);

    size = 0;
    res = redirect_RegQueryValueExA(key, "SystemRoot", 0, NULL, NULL, &size);
    EXPECT(res == ERROR_MORE_DATA, true);
    EXPECT(size > 0, true);

    res = redirect_RegCloseKey(key);
    EXPECT(res == ERROR_SUCCESS, true);

    /* test REG_MULTI_SZ */
    res = redirect_RegOpenKeyExA(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost", 0,
        KEY_READ, &key);
    EXPECT(res == ERROR_SUCCESS, true);
    size = BUFFER_SIZE_BYTES(buf);
    res = redirect_RegQueryValueExA(key, "NetworkService", 0, &type, (LPBYTE)buf, &size);
    EXPECT(res == ERROR_SUCCESS, true);
    EXPECT(type == REG_MULTI_SZ, true);
    {
        char *s;
        bool found_dhcp = false, found_DNS = false;
        uint count = 0;
        s = buf;
        while (strlen(s) > 0) {
            count++;
            if (strcmp(s, "DHCP") == 0)
                found_dhcp = true;
            else if (strstr(s, "DNS") != NULL || strstr(s, "DnsCache") != NULL)
                found_DNS = true;
            s += strlen(s) + 1 /*null*/;
        }
        EXPECT((count == 1 /*seen on XP*/ && found_DNS) || (found_dhcp && found_DNS),
               true);
    }
    res = redirect_RegCloseKey(key);
    EXPECT(res == ERROR_SUCCESS, true);

    res = redirect_RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &key);
    EXPECT(res == ERROR_SUCCESS, true);
    size = BUFFER_SIZE_BYTES(buf);
    /* PATH is sometimes REG_SZ and sometimes REG_EXPAND_SZ.  TEMP is
     * REG_EXPAND_SZ by default, but if the user changes it it may be REG_SZ.
     */
    res = redirect_RegQueryValueExW(key, L"TEMP", 0, &type, (LPBYTE)buf, &size);
    EXPECT(res == ERROR_SUCCESS, true);
    EXPECT(type == REG_EXPAND_SZ || type == REG_SZ, true);
    res = redirect_RegCloseKey(key);
    EXPECT(res == ERROR_SUCCESS, true);

    if (get_os_version() >= WINDOWS_VERSION_XP) {
        /* As a final check, ensure no handle leaks. */
        ok = GetProcessHandleCount(GetCurrentProcess(), &size);
        EXPECT(ok, true);
        EXPECT(size == handle_count, true);
    }
}
#endif
