/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

#include "share.h"
#include "drlibc.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef WINDOWS
#    include "config.h"
#    include "elm.h"
#    include "events.h"      /* for canary */
#    include "processes.h"   /* for canary */
#    include "../options.h"  /* for option checking */
#    include "ntdll_types.h" /* for NT_SUCCESS */

#    include <io.h>    /* for canary */
#    include <Fcntl.h> /* for canary */
#    include <aclapi.h>
#else
#    include <sys/stat.h>
#endif

#ifndef UNIT_TEST

#    ifdef WINDOWS

#        ifdef DEBUG

int debuglevel = DL_FATAL;
int abortlevel = DL_FATAL;

void
set_debuglevel(int level)
{
    debuglevel = level;
}

void
set_abortlevel(int level)
{
    abortlevel = level;
}

#            define CONFIG_MAX 8192

#            define HEADER_SNIPPET(defsfile)                  \
                "POLICY_VERSION=30000\n"                      \
                "BEGIN_BLOCK\n"                               \
                "GLOBAL\n"                                    \
                "DYNAMORIO_OPTIONS=\n"                        \
                "DYNAMORIO_RUNUNDER=1\n"                      \
                "DYNAMORIO_AUTOINJECT=\\lib\\dynamorio.dll\n" \
                "DYNAMORIO_HOT_PATCH_POLICIES=" defsfile "\n" \
                "DYNAMORIO_UNSUPPORTED=\n"                    \
                "END_BLOCK\n"

DWORD
load_test_config(const char *snippet, BOOL use_hotpatch_defs)
{
    char buf[CONFIG_MAX];
    _snprintf(buf, CONFIG_MAX, "%s%s",
              use_hotpatch_defs ? HEADER_SNIPPET("\\conf") : HEADER_SNIPPET(""), snippet);
    NULL_TERMINATE_BUFFER(buf);
    DO_ASSERT(strlen(buf) < CONFIG_MAX - 2);
    DO_DEBUG(DL_VERB, printf("importing %s\n", buf););
    CHECKED_OPERATION(policy_import(buf, FALSE, NULL, NULL));
    return ERROR_SUCCESS;
}

void
get_testdir(WCHAR *buf, UINT maxchars)
{
    WCHAR *filePart;
    WCHAR tmp[MAX_PATH];
    DWORD len;

    len = GetEnvironmentVariable(L"DYNAMORIO_WINDIR", tmp, maxchars);
    DO_ASSERT(len < maxchars);
    if (len == 0) {
        len = GetEnvironmentVariable(L_DYNAMORIO_VAR_HOME, tmp, maxchars);
        DO_ASSERT(len < maxchars);
        /* check for cygwin paths on windows */
        if (!file_exists(buf))
            len = 0;

        DO_DEBUG(DL_INFO, printf("ignoring invalid-looking DYNAMORIO_HOME=%S\n", tmp););
    }

    if (len == 0) {
        wcsncpy(tmp, L"..", MAX_PATH);
    }

    len = GetFullPathName(tmp, maxchars, buf, &filePart);

    DO_DEBUG(DL_INFO, printf("using drhome: %S\n", buf););

    DO_ASSERT(len != 0);

    return;
}

void
error_cb(unsigned int errcode, WCHAR *message)
{
    if (errcode || !errcode || message) {
        DO_ASSERT(0);
    }
}

extern BOOL do_once;

typedef struct evthelp__ {
    DWORD type;
    WCHAR *exename;
    ULONG pid;
    WCHAR *s3;
    WCHAR *s4;
    UINT maxchars;
    BOOL found;
} evthelp;

evthelp *cb_eh = NULL;
int last_record = -1;

void
check_event_cb(EVENTLOGRECORD *record)
{
    const WCHAR *strings;

    if (cb_eh->found)
        return;

    last_record = record->RecordNumber;

    if (record->EventID == cb_eh->type) {

        if (cb_eh->exename != NULL &&
            0 != wcscmp(get_event_exename(record), cb_eh->exename))
            return;

        if (cb_eh->pid != 0 && cb_eh->pid != get_event_pid(record))
            return;

        strings = get_message_strings(record);
        strings = next_message_string(strings);
        strings = next_message_string(strings);
        if (cb_eh->s3 != NULL) {
            cb_eh->s3[0] = L'\0';
            if (strings != NULL) {
                wcsncpy(cb_eh->s3, strings, cb_eh->maxchars);
                cb_eh->s3[cb_eh->maxchars - 1] = L'\0';
            }
        }
        if (cb_eh->s4 != NULL) {
            cb_eh->s4[0] = L'\0';
            strings = next_message_string(strings);
            if (strings != NULL) {
                wcsncpy(cb_eh->s4, strings, cb_eh->maxchars);
                cb_eh->s4[cb_eh->maxchars - 1] = L'\0';
            }
        }
        cb_eh->found = TRUE;
    }
}

void
reset_last_event()
{
    last_record = -1;
}

/* checks for events matching type, exename (if not null), and pid (if
 *  not 0). fills in s3 and s4 with 3rd and 4th message strings of
 *  match, if not null.
 * next search will start with event after matched event. */
BOOL
check_for_event(DWORD type, WCHAR *exename, ULONG pid, WCHAR *s3, WCHAR *s4,
                UINT maxchars)
{
    evthelp eh;

    eh.type = type;
    eh.exename = exename;
    eh.pid = pid;
    eh.s3 = s3;
    eh.s4 = s4;
    eh.maxchars = maxchars;
    eh.found = FALSE;

    cb_eh = &eh;

    /* backdoor */
    do_once = TRUE;

    CHECKED_OPERATION(
        start_eventlog_monitor(FALSE, NULL, check_event_cb, error_cb, last_record));
    DO_ASSERT(WAIT_OBJECT_0 ==
              WaitForSingleObject(get_eventlog_monitor_thread_handle(), 10000));
    stop_eventlog_monitor();

    return eh.found;
}

FILE *event_list_fp;

void
show_event_cb(unsigned int mID, unsigned int type, WCHAR *message, DWORD timestamp)
{
    /* fool the compiler */
    DO_ASSERT(type == 0 || type != 0 || timestamp == 0);
    fprintf(event_list_fp, " Event %d: %S\n", mID, message);
}

void
show_all_events(FILE *fp)
{
    DO_ASSERT(fp != NULL);

    event_list_fp = fp;
    /* backdoor */
    do_once = TRUE;
    CHECKED_OPERATION(
        start_eventlog_monitor(TRUE, show_event_cb, NULL, error_cb, (DWORD)-1));
    DO_ASSERT(WAIT_OBJECT_0 ==
              WaitForSingleObject(get_eventlog_monitor_thread_handle(), 10000));
    stop_eventlog_monitor();

    return;
}

#        endif /* _DEBUG */

void
wcstolower(WCHAR *str)
{
    UINT i;
    for (i = 0; i < wcslen(str); i++)
        str[i] = towlower(str[i]);
}

WCHAR *
get_exename_from_path(const WCHAR *path)
{
    WCHAR *name = wcsrchr(path, L'\\');
    if (name == NULL)
        name = (WCHAR *)path;
    else
        name += 1;
    return name;
}

DWORD
acquire_shutdown_privilege()
{
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES Priv;

    // get current thread token
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, FALSE,
                         &hToken)) {
        // can't get thread token, try process token instead
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                              &hToken)) {
            return GetLastError();
        }
    }

    Priv.PrivilegeCount = 1;
    Priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &Priv.Privileges[0].Luid);
    // try to enable the privilege
    if (!AdjustTokenPrivileges(hToken, FALSE, &Priv, sizeof(Priv), NULL, 0))
        return GetLastError();

    return ERROR_SUCCESS;
}

/*
 * FIXME: shutdown reason. we should probably use this, BUT
 *  InitiateSystemShutdownEx is not included in VS6.0, so we'll have
 *  to dynamically link it in.
 *
 * from msdn:
 *  SHTDN_REASON_FLAG_PLANNED: The shutdown was planned. On Windows .NET
 *   Server, the system generates a state snapshot. For more information,
 *   see the help for Shutdown Event Tracker.
 *
 *  various combinations of major/minor flags are "recognized by the
 *   system"; the APPLICATION|RECONFIG is NOT one of these. However,
 *   "You can also define your own shutdown reasons and add them to the
 *    the registry." we should probably do this at installation, once
 *    we're happy we've got a good reason code.
 */
DWORD
reboot_system()
{
    DWORD res;

    res = acquire_shutdown_privilege();
    if (res != ERROR_SUCCESS)
        return res;

    /* do we need to harden this at all?
     * "If the the system is not ready to handle the request, the last
     *  error code is ERROR_NOT_READY. The application should wait a
     *  short while and retry the call."
     * also ERROR_MACHINE_LOCKED, ERROR_SHUTDOWN_IN_PROGRESS, etc. */

    res =
        InitiateSystemShutdown(NULL, L"A System Restart was requested.", 30, TRUE, TRUE);
    //                                SHTDN_REASON_MAJOR_APPLICATION |
    //                                SHTDN_REASON_MINOR_RECONFIG);

    return res;
}

#        define LAST_WCHAR(wstr) wstr[wcslen(wstr) - 1]

#    endif /* WINDOWS */

/* this sucks.
 * i can't believe this is best way to implement this in Win32...
 *  but i can't seem to find a better way.
 * msdn suggests using CreateFile() with CREATE_NEW or OPEN_EXISTING,
 *   and then checking error codes; but the problem there is that C:\\
 *   returns PATH_NOT_FOUND regardless. */
bool
file_exists(const TCHAR *fn)
{
#    ifdef WINDOWS
    WIN32_FIND_DATA fd;
    HANDLE search;

    DO_ASSERT(fn != NULL);

    search = FindFirstFile(fn, &fd);

    if (search == INVALID_HANDLE_VALUE) {

        /* special handling for e.g. C:\\ */
        if (LAST_WCHAR(fn) == L'\\' || LAST_WCHAR(fn) == L':') {
            WCHAR buf[MAX_PATH];
            _snwprintf(buf, MAX_PATH, L"%s%s*", fn,
                       LAST_WCHAR(fn) == L'\\' ? L"" : L"\\");
            NULL_TERMINATE_BUFFER(buf);
            search = FindFirstFile(buf, &fd);
            if (search != INVALID_HANDLE_VALUE) {
                FindClose(search);
                return TRUE;
            } else {
                DO_DEBUG(
                    DL_VERB,
                    printf("%S: even though we tried hard, %d\n", buf, GetLastError()););
            }
        }

        DO_DEBUG(DL_VERB,
                 printf("%S doesn't exist because of: %d\n", fn, GetLastError()););
        return FALSE;
    } else {
        FindClose(search);
        return TRUE;
    }
#    else
    struct stat64 st;
    /* Use the raw syscall to avoid glibc 2.33 deps (i#5474). */
    return dr_stat_syscall(fn, &st) == 0;
#    endif
}

#    ifdef WINDOWS
#        define MAX_COUNTER 999999

/* grokked from the core.
 * FIXME: shareme!
 * if NULL is passed for directory, then it is ignored and no directory
 *  check is done, and filename_base is assumed to be absolute.
 * TODO: make this a proactive check: make sure the file can be
 *  opened, eg, do a create/delete on the filename to be returned.
 */
BOOL
get_unique_filename(const WCHAR *directory, const WCHAR *filename_base,
                    const WCHAR *file_type, WCHAR *filename_buffer, UINT maxlen)
{
    UINT counter = 0;

    if (directory != NULL && !file_exists(directory))
        return FALSE;

    do {
        if (directory == NULL)
            _snwprintf(filename_buffer, maxlen, L"%s.%.8d%s", filename_base, counter,
                       file_type);
        else
            _snwprintf(filename_buffer, maxlen, L"%s\\%s.%.8d%s", directory,
                       filename_base, counter, file_type);
        filename_buffer[maxlen - 1] = L'\0';
    } while (file_exists(filename_buffer) && (++counter < MAX_COUNTER));

    return (counter < MAX_COUNTER);
}

DWORD
delete_file_on_boot(WCHAR *filename)
{
    DWORD res;
    BOOL success = MoveFileEx(filename, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    /* reboot removal adds an entry to
     * HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session
     * Manager\PendingFileRenameOperations and smss.exe will delete the file on next boot
     */
    if (success)
        res = ERROR_SUCCESS;
    else
        res = GetLastError();
    return res;
}

DWORD
delete_file_rename_in_use(WCHAR *filename)
{
    DWORD res;
    BOOL success = DeleteFile(filename);

    if (success)
        return ERROR_SUCCESS;

    /* xref case 4512: if we leave a dll in a process after we're done
     *  using it, we won't be able to delete it; however, hopefully
     *  we can rename it so there won't be issues replacing it later. */
    res = GetLastError();
    if (res != ERROR_SUCCESS) {
        WCHAR tempname[MAX_PATH];
        if (get_unique_filename(NULL, filename, L".tmp", tempname, MAX_PATH)) {
            success = MoveFile(filename, tempname);
            if (success) {
                res = ERROR_SUCCESS;

                /* as best effort, we also schedule cleanup of the
                 * temporary file on next boot */
                delete_file_on_boot(tempname);
            } else
                res = GetLastError();
        }
    }

    return res;
}

#        ifndef PROTECTED_DACL_SECURITY_INFORMATION
#            define PROTECTED_DACL_SECURITY_INFORMATION (0x80000000L)
#        endif

/*
 * quick permissions xfer workaround for updating permissions
 *  on upgrade.
 */
DWORD
copy_file_permissions(WCHAR *filedst, WCHAR *filesrc)
{
    DWORD res = ERROR_SUCCESS;
    SECURITY_DESCRIPTOR *sd = NULL;
    ACL *dacl = NULL;
    res = GetNamedSecurityInfo(filesrc, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                               NULL, &dacl, NULL, &sd);

    if (res != ERROR_SUCCESS)
        return res;

    res = SetNamedSecurityInfo(filedst, SE_FILE_OBJECT,
                               DACL_SECURITY_INFORMATION |
                                   PROTECTED_DACL_SECURITY_INFORMATION,
                               NULL, NULL, dacl, NULL);

    LocalFree(sd);

    return res;
}

/* NOTE: for now we only consider the major/minor versions and
 *  platform id.
 *
 * the osinfo.szCSDVersion string contains service pack information,
 *  which could be used to distinguish e.g. XPSP2, 2K3SP1, if
 *  necessary.
 */

DWORD
get_platform(DWORD *platform)
{
    /* determine the OS version information */
    OSVERSIONINFOW osinfo;
    /* i#1418: GetVersionEx is just plain broken on win8.1+ so we use the Rtl version */
    typedef NTSTATUS(NTAPI * RtlGetVersion_t)(OSVERSIONINFOW * info);
    RtlGetVersion_t RtlGetVersion;
    NTSTATUS res = -1;
    HANDLE ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
    /* i#1598: on any error or on unknown ver, best to assume it's a new ver
     * and will look most like the most recent known ver.  We'll still return error
     * return val below, but many callers don't check that (!).
     */
    *platform = PLATFORM_WIN_10;
    if (ntdll_handle == NULL)
        return GetLastError();
    RtlGetVersion =
        (RtlGetVersion_t)GetProcAddress((HMODULE)ntdll_handle, "RtlGetVersion");
    if (RtlGetVersion == NULL)
        return GetLastError();
    osinfo.dwOSVersionInfoSize = sizeof(osinfo);
    res = RtlGetVersion(&osinfo);
    if (NT_SUCCESS(res)) {

        DO_DEBUG(DL_VERB, WCHAR verbuf[MAX_PATH];
                 _snwprintf(verbuf, MAX_PATH, L"Major=%d, Minor=%d, Build=%d, SPinfo=%s",
                            osinfo.dwMajorVersion, osinfo.dwMinorVersion,
                            osinfo.dwBuildNumber, osinfo.szCSDVersion);
                 NULL_TERMINATE_BUFFER(verbuf); printf("%S\n", verbuf););

        if (osinfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
            return ERROR_UNSUPPORTED_OS;

        if (osinfo.dwMajorVersion == 4) {
            if (osinfo.dwMinorVersion == 0) {
                *platform = PLATFORM_WIN_NT_4;
                return ERROR_SUCCESS;
            }
        } else if (osinfo.dwMajorVersion == 5) {
            if (osinfo.dwMinorVersion == 0) {
                *platform = PLATFORM_WIN_2000;
                return ERROR_SUCCESS;
            } else if (osinfo.dwMinorVersion == 1) {
                *platform = PLATFORM_WIN_XP;
                return ERROR_SUCCESS;
            } else if (osinfo.dwMinorVersion == 2) {
                *platform = PLATFORM_WIN_2003;
                return ERROR_SUCCESS;
            }
        } else if (osinfo.dwMajorVersion == 6) {
            if (osinfo.dwMinorVersion == 0) {
                *platform = PLATFORM_VISTA;
                return ERROR_SUCCESS;
            } else if (osinfo.dwMinorVersion == 1) {
                *platform = PLATFORM_WIN_7;
                return ERROR_SUCCESS;
            } else if (osinfo.dwMinorVersion == 2) {
                *platform = PLATFORM_WIN_8;
                return ERROR_SUCCESS;
            } else if (osinfo.dwMinorVersion == 3) {
                *platform = PLATFORM_WIN_8_1;
                return ERROR_SUCCESS;
            }
        } else if (osinfo.dwMajorVersion == 10) {
            if (osinfo.dwMinorVersion == 0) {
                if (GetProcAddress((HMODULE)ntdll_handle, "NtAllocateVirtualMemoryEx") !=
                    NULL)
                    *platform = PLATFORM_WIN_10_1803;
                else if (GetProcAddress((HMODULE)ntdll_handle, "NtCallEnclave") != NULL)
                    *platform = PLATFORM_WIN_10_1709;
                else if (GetProcAddress((HMODULE)ntdll_handle, "NtLoadHotPatch") != NULL)
                    *platform = PLATFORM_WIN_10_1703;
                else if (GetProcAddress((HMODULE)ntdll_handle,
                                        "NtCreateRegistryTransaction") != NULL)
                    *platform = PLATFORM_WIN_10_1607;
                else if (GetProcAddress((HMODULE)ntdll_handle, "NtCreateEnclave") != NULL)
                    *platform = PLATFORM_WIN_10_1511;
                else
                    *platform = PLATFORM_WIN_10;
                return ERROR_SUCCESS;
            }
        }
        return ERROR_UNSUPPORTED_OS;
    } else {
        return res;
    }
}

BOOL
is_wow64(HANDLE hProcess)
{
    /* IsWow64Pocess is only available on XP+ */
    typedef DWORD(WINAPI * IsWow64Process_Type)(HANDLE hProcess, PBOOL isWow64Process);
    static HANDLE kernel32_handle;
    static IsWow64Process_Type IsWow64Process;
    if (kernel32_handle == NULL)
        kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (IsWow64Process == NULL && kernel32_handle != NULL) {
        IsWow64Process =
            (IsWow64Process_Type)GetProcAddress(kernel32_handle, "IsWow64Process");
    }
    if (IsWow64Process == NULL) {
        /* should be NT or 2K */
        DO_DEBUG(DL_INFO, {
            DWORD platform = 0;
            get_platform(&platform);
            DO_ASSERT(platform == PLATFORM_WIN_NT_4 || platform == PLATFORM_WIN_2000);
        });
        return FALSE;
    } else {
        BOOL res;
        if (!IsWow64Process(hProcess, &res))
            return FALSE;
        return res;
    }
}

static const TCHAR *
get_dynamorio_home_helper(BOOL reset)
{
    static TCHAR dynamorio_home[MAXIMUM_PATH] = { 0 };
    int res;

    if (reset)
        dynamorio_home[0] = L'\0';

    if (dynamorio_home[0] != L'\0')
        return dynamorio_home;

    res = get_config_parameter(L_PRODUCT_NAME, FALSE, L_DYNAMORIO_VAR_HOME,
                               dynamorio_home, MAXIMUM_PATH);

    if (res == ERROR_SUCCESS && dynamorio_home[0] != L'\0')
        return dynamorio_home;
    else
        return NULL;
}

const TCHAR *
get_dynamorio_home()
{
    return get_dynamorio_home_helper(FALSE);
}

static const TCHAR *
get_dynamorio_logdir_helper(BOOL reset)
{
    static TCHAR dynamorio_logdir[MAXIMUM_PATH] = { 0 };
    DWORD res;

    if (reset)
        dynamorio_logdir[0] = L'\0';

    if (dynamorio_logdir[0] != L'\0')
        return dynamorio_logdir;

    res = get_config_parameter(L_PRODUCT_NAME, FALSE, L_DYNAMORIO_VAR_LOGDIR,
                               dynamorio_logdir, MAXIMUM_PATH);

    if (res == ERROR_SUCCESS && dynamorio_logdir[0] != L'\0')
        return dynamorio_logdir;
    else
        return NULL;
}

const TCHAR *
get_dynamorio_logdir()
{
    return get_dynamorio_logdir_helper(FALSE);
}

/* If a path is passed in, it is checked for 8.3 compatibility; else,
 * the default path is checked.  This routine does not check the
 * actual 8.3 reg key.
 */
BOOL
using_system32_for_preinject(const WCHAR *preinject)
{
    DWORD platform = 0;
    get_platform(&platform);
    if (platform == PLATFORM_WIN_NT_4) {
        return TRUE;
    } else {
        /* case 7586: we need to check if the system has disabled
         *  8.3 names; if so, we need to use the system32 for
         *  preinject (since spaces are not allowed in AppInitDLLs)
         */
        WCHAR short_path[MAX_PATH];
        WCHAR long_path[MAX_PATH];
        if (preinject == NULL) {
            /* note: with force_local_path == TRUE, we don't have
             *  to worry about get_preinject_path() calling this
             *  method back, and it will always return success.
             */
            get_preinject_path(short_path, MAX_PATH, TRUE, TRUE);
            wcsncat(short_path, L"\\" L_EXPAND_LEVEL(INJECT_DLL_8_3_NAME),
                    MAX_PATH - wcslen(short_path));
            NULL_TERMINATE_BUFFER(short_path);
            get_preinject_path(long_path, MAX_PATH, TRUE, FALSE);
            wcsncat(long_path, L"\\" L_EXPAND_LEVEL(INJECT_DLL_8_3_NAME),
                    MAX_PATH - wcslen(long_path));
            NULL_TERMINATE_BUFFER(long_path);
        } else {
            /* Check the passed-in file */
            GetShortPathName(preinject, short_path, BUFFER_SIZE_ELEMENTS(short_path));
            NULL_TERMINATE_BUFFER(short_path);
            wcsncpy(long_path, preinject, BUFFER_SIZE_ELEMENTS(long_path));
        }

        /* if 8.3 names are disabled, file_exists will return FALSE on
         * the GetShortPathName()'ed path.
         */
        return (file_exists(long_path) && !file_exists(short_path));
    }
}

/* if force_local_path, then this returns the in-installation
 *  path regardless of using_system32_for_preinject().
 * otherwise, this returns the path to the actuall DLL that
 *  will be injected, which depends on
 *  using_system32_for_preinject()
 * if short_path, calls GetShortPathName() on the path before returning it.
 *  for a canonical preinject path, this parameter should be TRUE.
 */
DWORD
get_preinject_path(WCHAR *buf, int nchars, BOOL force_local_path, BOOL short_path)
{
    if (!force_local_path && using_system32_for_preinject(NULL)) {
        UINT len;
        len = GetSystemDirectory(buf, MAX_PATH);

        if (len == 0)
            return GetLastError();
    } else {
        const WCHAR *home = get_dynamorio_home();
        /* using_system32_for_preinject() assumes we always succeed */
        _snwprintf(buf, nchars, L"%s\\lib", home == NULL ? L"" : home);
    }

    buf[nchars - 1] = L'\0';
    if (short_path)
        GetShortPathName(buf, buf, nchars);

    return ERROR_SUCCESS;
}

DWORD
get_preinject_name(WCHAR *buf, int nchars)
{
    DWORD res;

    if (using_system32_for_preinject(NULL)) {
        wcsncpy(buf, L_EXPAND_LEVEL(INJECT_DLL_NAME), nchars);
    } else {
        res = get_preinject_path(buf, nchars, FALSE, TRUE);
        if (res != ERROR_SUCCESS)
            return res;
        wcsncat(buf, L"\\" L_EXPAND_LEVEL(INJECT_DLL_8_3_NAME), nchars - wcslen(buf));
    }

    buf[nchars - 1] = L'\0';

    return ERROR_SUCCESS;
}

#    endif /* WINDOWS */

static dr_platform_t registry_view = DR_PLATFORM_DEFAULT;

void
set_dr_platform(dr_platform_t platform)
{
    registry_view = platform;
}

dr_platform_t
get_dr_platform()
{
    if (registry_view ==
        DR_PLATFORM_64BIT IF_X64(|| registry_view == DR_PLATFORM_DEFAULT))
        return DR_PLATFORM_64BIT;
    return DR_PLATFORM_32BIT;
}

#    ifdef WINDOWS

DWORD
platform_key_flags()
{
    /* PR 244206: have control over whether using WOW64 redirection or
     * raw 64-bit registry view.
     * These flags should be used for all Reg{Create,Open,Delete}KeyEx calls,
     * on XP+ (invalid on earlier platforms) on redirected keys
     * (most of HKLM\Software).
     * The flags don't matter on non-redirected trees like HKLM\System.
     * Since too many functions in libutil/ end up calling something
     * that reads/writes the registry, we don't pass the dr_platform_t
     * around and instead use a global variable.
     */
    DWORD platform = 0;
    get_platform(&platform);
    if (platform == PLATFORM_WIN_NT_4 || platform == PLATFORM_WIN_2000)
        return 0;
    else {
        switch (registry_view) {
        case DR_PLATFORM_DEFAULT: return 0;
        case DR_PLATFORM_32BIT: return KEY_WOW64_32KEY;
        case DR_PLATFORM_64BIT: return KEY_WOW64_64KEY;
        default: DO_ASSERT(false); return 0;
        }
    }
}

/* PR 244206: use this instead of RegDeleteKey for deleting redirected keys
 * (most of HKLM\Software)
 */
DWORD
delete_product_key(HKEY hkey, LPCWSTR subkey)
{
    /* RegDeleteKeyEx is only available on XP+.  We cannot delete
     * from 64-bit registry if we're WOW64 using RegDeleteKey, so we
     * dynamically look up RegDeleteKeyEx.
     * We could instead use NtDeleteKey and first open the subkey HKEY:
     * we could link with the core's ntdll.c, and also use is_wow64_process().
     */
    typedef DWORD(WINAPI * RegDeleteKeyExW_Type)(HKEY hKey, LPCWSTR lpSubKey,
                                                 REGSAM samDesired, DWORD Reserved);
    static HANDLE advapi32_handle;
    static RegDeleteKeyExW_Type RegDeleteKeyExW;
    if (advapi32_handle == NULL)
        advapi32_handle = GetModuleHandle(L"advapi32.dll");
    if (RegDeleteKeyExW == NULL && advapi32_handle != NULL) {
        RegDeleteKeyExW =
            (RegDeleteKeyExW_Type)GetProcAddress(advapi32_handle, "RegDeleteKeyExW");
    }
    if (RegDeleteKeyExW == NULL) {
        /* should be NT or 2K */
        DO_DEBUG(DL_INFO, {
            DWORD platform = 0;
            get_platform(&platform);
            DO_ASSERT(platform == PLATFORM_WIN_NT_4 || platform == PLATFORM_WIN_2000);
        });
        return RegDeleteKey(hkey, subkey);
    } else
        return RegDeleteKeyExW(hkey, subkey, platform_key_flags(), 0);
}

DWORD
create_root_key()
{
    int res;
    HKEY hkroot;

    res = RegCreateKeyEx(DYNAMORIO_REGISTRY_HIVE, L_DYNAMORIO_REGISTRY_KEY, 0, NULL,
                         REG_OPTION_NON_VOLATILE,
                         platform_key_flags() | KEY_WRITE | KEY_ENUMERATE_SUB_KEYS, NULL,
                         &hkroot, NULL);
    RegCloseKey(hkroot);

    return res;
}

/* Deletes the reg key created by create_root_key/setup_installation and the parent
 * company key if it's empty afterwards (might not be if PE or nodemgr has config subkeys
 * there. */
DWORD
destroy_root_key()
{
    DWORD res;
    /* This deletes just the product key. */
    res = recursive_delete_key(DYNAMORIO_REGISTRY_HIVE, L_DYNAMORIO_REGISTRY_KEY, NULL);
    /* Delete the company key (this will only work if it is empty, so no need to worry
     * about clobbering any config settings or doing too much damage if we screw up. */
    if (res == ERROR_SUCCESS) {
        WCHAR company_key[MAX_PATH];
        WCHAR *pop;
        wcsncpy(company_key, L_DYNAMORIO_REGISTRY_KEY, MAX_PATH);
        NULL_TERMINATE_BUFFER(company_key);
        pop = wcsstr(company_key, L_COMPANY_NAME);
        if (pop != NULL) {
            pop += wcslen(L_COMPANY_NAME);
            /* sanity check */
            if (pop == wcsrchr(company_key, L'\\')) {
                *pop = L'\0';
                delete_product_key(DYNAMORIO_REGISTRY_HIVE, company_key);
            } else
                res = ERROR_BAD_FORMAT;
        } else
            res = ERROR_BAD_FORMAT;
    }
    return res;
}

DWORD
setup_installation(const WCHAR *path, BOOL overwrite)
{
    WCHAR buf[MAX_PATH];

    /* if there's something there, leave it */
    if (!overwrite && get_dynamorio_home() != NULL)
        return ERROR_SUCCESS;

    DO_DEBUG(DL_INFO, printf("setting up installation at: %S\n", path););

    mkdir_with_parents(path);

    if (!file_exists(path))
        return ERROR_PATH_NOT_FOUND;

    _snwprintf(buf, MAX_PATH, L"%s\\%s", path, L"conf");
    NULL_TERMINATE_BUFFER(buf);

    DO_DEBUG(DL_INFO, printf("making config dir: %S\n", buf););

    mkdir_with_parents(buf);

    if (!file_exists(buf))
        return ERROR_PATH_NOT_FOUND;

    _snwprintf(buf, MAX_PATH, L"%s\\%s", path, L"logs");
    NULL_TERMINATE_BUFFER(buf);

    DO_DEBUG(DL_INFO, printf("making logdir: %S\n", buf););

    mkdir_with_parents(buf);

    if (!file_exists(buf))
        return ERROR_PATH_NOT_FOUND;

    CHECKED_OPERATION(create_root_key());

    CHECKED_OPERATION(
        set_config_parameter(L_PRODUCT_NAME, FALSE, L_DYNAMORIO_VAR_HOME, path));

    CHECKED_OPERATION(
        set_config_parameter(L_PRODUCT_NAME, FALSE, L_DYNAMORIO_VAR_LOGDIR, buf));

    /* reset the DR_HOME cache */
    get_dynamorio_home_helper(TRUE);

    return ERROR_SUCCESS;
}

/* modifies permissions for 4.3 cache/User-SID directories to be
 * created by users themselves
 */
DWORD
setup_cache_permissions(WCHAR *cacheRootDirectory)
{
    DWORD result = ERROR_UNSUPPORTED_OS;

#        define NUM_ACES 2
    /* in C const int isn't good enough */
    EXPLICIT_ACCESS ea[NUM_ACES];

    PSID pSIDEveryone = NULL;
    PSID pSIDCreatorOwner = NULL;
    PACL pACL = NULL;
    PACL pOldDACL = NULL;

    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY SIDAuthCreator = SECURITY_CREATOR_SID_AUTHORITY;
    DWORD dwRes;

    SECURITY_DESCRIPTOR *pSD = NULL;

    DWORD platform = 0; /* accomodating NT permissions */
    get_platform(&platform);

    /* Note that we prefer to not create ACLs from scratch, so that we
     * can accommodate Administrator groups unknown to us that would
     * have been inherited from \Program Files\.  We should always
     * start with a known ACL and just edit the new ACEs
     */

    dwRes = GetNamedSecurityInfo(cacheRootDirectory, SE_FILE_OBJECT,
                                 DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL,
                                 &pSD);

    if (dwRes != ERROR_SUCCESS)
        return dwRes;

    /* Note: Although we are ADDING possibly existing ACE, it seems
     * like this is handled well and we don't grow the ACL.  For now
     * this doesn't matter to us, since we expect to have just copied
     * the flags from the lib\ directory so can't really accumulate.
     */

    // Create a SID for the Everyone group.
    if (!AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0,
                                  0, &pSIDEveryone)) {
        DO_DEBUG(DL_VERB, printf("AllocateAndInitializeSid (Everyone).\n"););

        goto cleanup;
    }

    // Create a SID for the CREATOR OWNER group
    if (!AllocateAndInitializeSid(&SIDAuthCreator, 1, SECURITY_CREATOR_OWNER_RID, 0, 0, 0,
                                  0, 0, 0, 0, &pSIDCreatorOwner)) {
        DO_DEBUG(DL_VERB, printf("AllocateAndInitializeSid (CreatorOwner).\n"););

        goto cleanup;
    }

    ZeroMemory(&ea, NUM_ACES * sizeof(EXPLICIT_ACCESS));

    /* Grant create directory access to Everyone, which will be in
     * addition to existing Read/Execute permissions we are starting
     * with.
     */
    ea[0].grfAccessPermissions = FILE_ADD_SUBDIRECTORY;
    ea[0].grfAccessMode = GRANT_ACCESS;    /* not SET_ACCESS */
    ea[0].grfInheritance = NO_INHERITANCE; /* ONLY in cache\ folder! */
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName = (LPTSTR)pSIDEveryone;

    /* Set full control for CREATOR OWNER on any subfolders */
    ea[1].grfAccessPermissions = GENERIC_ALL;
    ea[1].grfAccessMode = SET_ACCESS; /* we SET ALL */
    if (platform == PLATFORM_WIN_NT_4) {
        /* case 10502 INHERIT_ONLY_ACE seems to not work */
        /* we are mostly interested in any subdirectory, and cache/ is
         * already created (and also trusted), so adding it there
         * doesn't affect anything.
         */
        ea[1].grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
    } else {
        /* not using the same as NT, since Creator Owner may already
         * have this ACE (and normally does) so we'll clutter with a
         * new incomplete one */
        ea[1].grfInheritance =
            INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
    }
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[1].Trustee.ptstrName = (LPTSTR)pSIDCreatorOwner;

    /* FIXME: we may want to disable the default group maybe should
     * set CREATOR GROUP to no access otherwise we get the default
     * Domain Users group (which usually is the Primary group) added,
     * e.g. KRAMMER\None:R(ead)
     */

    /* MSDN gave a false alarm that this doesn't exist on NT - It is
     * present at least on sp6.  FIXME: may want to use GetProcAddress
     * if we support earlier versions, but we'll know early enough.
     * We don't really need to support anything other than User SYSTEM
     * on NT for which we don't need this to work and can return
     * ERROR_UNSUPPORTED_OS
     */
    if (ERROR_SUCCESS !=
        SetEntriesInAcl(NUM_ACES, ea, pOldDACL, /* original DACL */
                        &pACL)) {
        DO_DEBUG(DL_VERB, printf("SetEntriesInAcl 0x%x\n", GetLastError()););
        goto cleanup;
    }

    // Try to modify the object's DACL.
    result = SetNamedSecurityInfo(
        cacheRootDirectory, // name of the object
        SE_FILE_OBJECT,     // type of object
        DACL_SECURITY_INFORMATION |
            PROTECTED_DACL_SECURITY_INFORMATION, // change only the object's DACL
        NULL, NULL,                              // do not change owner or group
        pACL,                                    // new DACL specified
        NULL);                                   // do not change SACL

    if (ERROR_SUCCESS == result) {
        DO_DEBUG(DL_VERB, printf("Successfully changed DACL\n"););
    }

cleanup:
    if (pSIDEveryone)
        FreeSid(pSIDEveryone);
    if (pSIDCreatorOwner)
        FreeSid(pSIDCreatorOwner);
    if (pACL)
        LocalFree(pACL);
    if (pSD)
        LocalFree(pSD);

    return result;
#        undef NUM_ACES
}

/* cache_root should normally be get_dynamorio_home() */
DWORD
setup_cache_shared_directories(const WCHAR *cache_root)
{
    DWORD res;

    /* support for new-in-4.2 directories, update the permissions
     * on the cache/ to be the same as those on lib/, and the
     * cache/shared/ folder to be the same as those on logs/
     *
     * note that the relative paths of the cache and shared cache
     * directories here should match the values set in
     * setup_cache_shared_registry()
     */
    WCHAR libpath[MAX_PATH];
    WCHAR cachepath[MAX_PATH];
    WCHAR logspath[MAX_PATH];
    WCHAR sharedcachepath[MAX_PATH];

    _snwprintf(libpath, MAX_PATH, L"%s\\lib", get_dynamorio_home());
    NULL_TERMINATE_BUFFER(libpath);
    _snwprintf(cachepath, MAX_PATH, L"%s\\cache", cache_root);
    NULL_TERMINATE_BUFFER(cachepath);
    _snwprintf(logspath, MAX_PATH, L"%s\\logs", get_dynamorio_home());
    NULL_TERMINATE_BUFFER(logspath);
    _snwprintf(sharedcachepath, MAX_PATH, L"%s\\shared", cachepath);
    NULL_TERMINATE_BUFFER(sharedcachepath);

    mkdir_with_parents(sharedcachepath);
    /* FIXME: no error checking */

    res = copy_file_permissions(cachepath, libpath);
    if (res != ERROR_SUCCESS) {
        return res;
    }

    res = copy_file_permissions(sharedcachepath, logspath);
    if (res != ERROR_SUCCESS) {
        return res;
    }

    /* For in 4.3 ONLY if all users (most importantly services)
     * validate their per-user directory (or files) for ownership
     */
    res = setup_cache_permissions(cachepath);
    if (res != ERROR_SUCCESS) {
        return res;
    }

    return ERROR_SUCCESS;
}

/* cache_root should normally be get_dynamorio_home() */
DWORD
setup_cache_shared_registry(const WCHAR *cache_root, ConfigGroup *policy)
{
    /* note that nodemgr doesn't need to do call this routine,
     * since the registry keys are added to the node policies in
     * controller/servlets/PolicyUpdateResponseHandler.java in the controller. but
     * anyway we expect these to be forever the same, and in any
     * case not configurable from the controller.
     */
    WCHAR wpathbuf[MAX_PATH];

    /* set up cache\ shared\ registry keys */
    _snwprintf(wpathbuf, MAX_PATH, L"%s\\cache", cache_root);
    NULL_TERMINATE_BUFFER(wpathbuf);
    set_config_group_parameter(policy, L_IF_WIN(DYNAMORIO_VAR_CACHE_ROOT), wpathbuf);

    /* set up cache\ shared\ registry keys */
    _snwprintf(wpathbuf, MAX_PATH, L"%s\\cache\\shared", cache_root);
    NULL_TERMINATE_BUFFER(wpathbuf);
    set_config_group_parameter(policy, L_IF_WIN(DYNAMORIO_VAR_CACHE_SHARED), wpathbuf);
    return ERROR_SUCCESS;
}

/* note that this checks the opstring against the
 *  version of core that matches this build, NOT the version
 *  of the core that's actually installed! */
BOOL
check_opstring(const WCHAR *opstring)
{
    char *cbuf;
    options_t ops;
    int res;
    size_t cbuf_size = wcslen(opstring) + 1;
    cbuf = (char *)malloc(cbuf_size);
    /* FIXME: if malloc fails, do something */
    _snprintf(cbuf, cbuf_size, "%S", opstring);
    cbuf[cbuf_size - 1] = '\0';
    res = set_dynamo_options(&ops, cbuf);
    free(cbuf);
    return !res;
}

HANDLE hToken = NULL;
TOKEN_PRIVILEGES Priv, OldPriv;
DWORD PrivSize = sizeof(OldPriv);

DWORD
acquire_privileges()
{
    DWORD error;

    /* if the privileges are already acquired, don't bother.
       this almost certainly will cause failures if multiple
        threads are trying to acquire privileges.
     */
    // FIXME - this should have real synchronization!!!
    if (hToken != NULL)
        return ERROR_ALREADY_INITIALIZED;

    // get current thread token
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, FALSE,
                         &hToken)) {
        // can't get thread token, try process token instead
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
                              &hToken)) {
            return GetLastError();
        }
    }

    Priv.PrivilegeCount = 1;
    Priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Priv.Privileges[0].Luid);
    // try to enable the privilege
    if (!AdjustTokenPrivileges(hToken, FALSE, &Priv, sizeof(Priv), &OldPriv, &PrivSize)) {
        return GetLastError();
    }
    error = GetLastError();
    if (error == ERROR_NOT_ALL_ASSIGNED) {
        /* acquiring SeDebugPrivilege requires being admin */
        return error;
    }

    return ERROR_SUCCESS;
}

DWORD
release_privileges()
{
    if (hToken == NULL)
        return ERROR_NO_SUCH_PRIVILEGE;

    AdjustTokenPrivileges(hToken, FALSE, &OldPriv, sizeof(OldPriv), NULL, NULL);
    CloseHandle(hToken);
    hToken = NULL;

    return ERROR_SUCCESS;
}

void
wstr_replace(WCHAR *str, WCHAR orig, WCHAR new)
{
    UINT i;
    for (i = 0; i < wcslen(str); i++)
        if (str[i] == orig)
            str[i] = new;
    return;
}

/* FIXME: should return error code if the directory wasn't created and
 * doesn't exist already
 */
void
mkdir_with_parents(const WCHAR *dirname)
{
    WCHAR buf[MAX_PATH], *temp_subdir;

    wcsncpy(buf, dirname, MAX_PATH);
    NULL_TERMINATE_BUFFER(buf);

    /* ensure proper slashes */
    wstr_replace(buf, L'/', L'\\');

    temp_subdir = buf;

    while (temp_subdir != NULL) {
        temp_subdir = wcschr(temp_subdir, L'\\');
        if (temp_subdir != NULL)
            *temp_subdir = L'\0';

        DO_DEBUG(DL_VERB, printf("trying to make: %S\n", buf););

        /* ok if this fails, eg the first time it will be C: */
        CreateDirectory(buf, NULL);

        if (temp_subdir != NULL) {
            *temp_subdir = L'\\';
            temp_subdir = temp_subdir + 1;
        }
    }
    return;
}

void
ensure_directory_exists_for_file(WCHAR *filename)
{
    WCHAR *slashptr, buf[MAX_PATH];
    wcsncpy(buf, filename, MAX_PATH);
    NULL_TERMINATE_BUFFER(buf);

    slashptr = wcsrchr(buf, L'\\');
    if (slashptr == NULL)
        return;

    *slashptr = L'\0';

    mkdir_with_parents(buf);
}

/* FIXME: apparently there's a bug in MSVCRT that converts
 *  \r\n to \r\r\n ? anyway that's what google and the evidence
 *  seem to indicate. (see policy.c for more)
 *
 * so we may want to convert this to using Win32 API instead of
 *  CRT. but then again we may not, just on principle. */
DWORD
write_file_contents(WCHAR *path, char *contents, BOOL overwrite)
{
    FILE *fp = NULL;
    DWORD res = ERROR_SUCCESS;

    ensure_directory_exists_for_file(path);

    fp = _wfopen(path, L"r");
    if (fp != NULL) {
        if (!overwrite)
            return ERROR_ALREADY_EXISTS;
        fclose(fp);
    }

    fp = _wfopen(path, L"w");
    if (fp == NULL) {
        res = delete_file_rename_in_use(path);
        if (res != ERROR_SUCCESS || (fp = _wfopen(path, L"w")) == NULL) {
            DO_DEBUG(DL_ERROR, printf("Unable to open file: %S (%d)\n", path, errno););
            return res;
        }
    }

    if (strlen(contents) != fwrite(contents, 1, strlen(contents), fp)) {
        DO_DEBUG(DL_ERROR, printf("Write failed to file: %S (errno=%d)\n", path, errno););
        res = ERROR_WRITE_FAULT;
    }

    DO_DEBUG(DL_INFO, printf("wrote file %S\n", path););

    fclose(fp);
    return res;
}

DWORD
write_file_contents_if_different(WCHAR *path, char *contents, BOOL *changed)
{
    char *existing;
    DWORD res;

    DO_ASSERT(path != NULL);
    DO_ASSERT(contents != NULL);
    DO_ASSERT(changed != NULL);

    existing = (char *)malloc(strlen(contents) + 1);
    res = read_file_contents(path, existing, strlen(contents) + 1, NULL);

    if (res == ERROR_SUCCESS && 0 == strcmp(contents, existing)) {
        *changed = FALSE;
        res = ERROR_SUCCESS;
    } else {
        *changed = TRUE;
        res = write_file_contents(path, contents, TRUE);
    }

    free(existing);

    return res;
}

#        define READ_BUF_SZ 1024

DWORD
read_file_contents(WCHAR *path, char *contents, SIZE_T maxchars, SIZE_T *needed)
{
    FILE *fp = NULL;
    DWORD res = ERROR_SUCCESS;
    SIZE_T n_read = 0;
    SIZE_T n_needed = 0;
    char buf[READ_BUF_SZ];

    DO_ASSERT(path != NULL);
    DO_ASSERT(contents != NULL || needed != NULL);
    DO_ASSERT(contents == NULL || maxchars > 0);

    fp = _wfopen(path, L"r");
    if (fp == NULL) {
        DO_DEBUG(DL_INFO, printf("Not found: %S\n", path););
        return ERROR_FILE_NOT_FOUND;
    }

    if (contents != NULL) {
        n_read = fread(contents, 1, maxchars, fp);

        /* NULL terminate string. */
        contents[n_read == maxchars ? n_read - 1 : n_read] = '\0';

        DO_DEBUG(DL_FINEST,
                 printf("*Read %zu bytes from %S (max=%zu)\n", n_read, path, maxchars););
    }

    n_needed = n_read;

    while (!feof(fp)) {
        res = ERROR_MORE_DATA;

        n_read = fread(buf, 1, READ_BUF_SZ, fp);

        DO_DEBUG(DL_FINEST, printf("  Read an additional %zu bytes\n", n_read););

        if (n_read == 0 && !feof(fp)) {
            res = ERROR_READ_FAULT;
            break;
        }

        n_needed += n_read;
    }

    /* + 1 for the NULL terminator */
    n_needed += 1;

    if (needed != NULL)
        *needed = n_needed;

    fclose(fp);

    if (res == ERROR_SUCCESS || res == ERROR_MORE_DATA) {
        DO_DEBUG(
            DL_VERB,
            printf("file %S contents: (%zu needed)\n\n%s\n", path, n_needed, contents););
    } else {
        DO_DEBUG(DL_ERROR, printf("read failed, error %d\n", res););
    }

    return res;
}

DWORD
delete_tree(const WCHAR *path)
{
    WIN32_FIND_DATA data;
    HANDLE hFind;
    WCHAR pathbuf[MAX_PATH], subdirbuf[MAX_PATH];

    if (path == NULL)
        return ERROR_INVALID_PARAMETER;

    _snwprintf(pathbuf, MAX_PATH, L"%s\\*.*", path);
    NULL_TERMINATE_BUFFER(pathbuf);

    hFind = FindFirstFile(pathbuf, &data);
    if (hFind == INVALID_HANDLE_VALUE)
        return GetLastError();

    DO_DEBUG(DL_VERB, printf("dt working on %S\n", path););
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            continue;

        /* case 7407: FindNextFile on a FAT32 filesystem returns files in
         * the order they were written to disk, which could be different
         * from NTFS where the order is alphabetical (from MSDN).
         * Also on FAT32, FindNextFile sometimes puts us back in the loop
         * for the file we just renamed and we try to
         * delete_file_rename_in_use the file we just renamed for a very
         * long time (>3 hrs).
         *
         * FIXME: temporary hack:  if filename has .tmp in its name
         * (first ocurrance), assume we just renamed it and skip.
         *
         * note we may want to doublecheck that the file is indeed not
         * deletable although we now add it to the
         * PendingFileRenameOperations so such unused files can't stay
         * around for too long.
         */
        if (wcsstr(data.cFileName, L".tmp") != NULL)
            continue;

        DO_DEBUG(DL_VERB,
                 printf("dt still working on %S, %d\n", data.cFileName,
                        data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY););

        _snwprintf(subdirbuf, MAX_PATH, L"%s\\%s", path, data.cFileName);
        NULL_TERMINATE_BUFFER(subdirbuf);

        /* case 4512: use rename trick if file is in use, so that
         *  the uninstall/reinstall case will work */
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            delete_tree(subdirbuf);
        else
            delete_file_rename_in_use(subdirbuf);
    } while (FindNextFile(hFind, &data));

    if (!FindClose(hFind))
        return GetLastError();

    if (!RemoveDirectory(path))
        return GetLastError();

    return ERROR_SUCCESS;
}

/*
 * helper function for registry permissions workaround. stopgap until
 *  we can make a decent permissions api.
 *
 * based on example code obtained from:
 *  http://www.codeproject.com/system/secntobj.asp
 */

PSID
getSID(WCHAR *user)
{
    DWORD dwSidLen = 0, dwDomainLen = 0;
    SID_NAME_USE SidNameUse;
    PSID pRet = NULL;
    PSID pSid = NULL;
    WCHAR *lpDomainName = NULL;

    /* The function on the first call retives the length that we need
     * to initialize the SID & domain name pointers */
    if (!LookupAccountName(NULL, user, NULL, &dwSidLen, NULL, &dwDomainLen,
                           &SidNameUse)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {

            pSid = LocalAlloc(LMEM_ZEROINIT, dwSidLen);
            lpDomainName = LocalAlloc(LMEM_ZEROINIT, dwDomainLen * sizeof(WCHAR));

            if (pSid && lpDomainName &&
                LookupAccountName(NULL, user, pSid, &dwSidLen, lpDomainName, &dwDomainLen,
                                  &SidNameUse)) {
                pRet = pSid;
                pSid = NULL;
            }
        }
    }

    /* if successful, was set to NULL and left in pRet */
    LocalFree(pSid);
    LocalFree(lpDomainName);

    return pRet;
}

BOOL
make_acl(DWORD count, WCHAR **userArray, DWORD *maskArray, ACL **acl)
{
    DWORD dwLoop = 0;
    DWORD dwAclLen = 0;
    PACL pRetAcl = NULL;
    PSID *ppStoreSid = NULL;
    BOOL bRes = FALSE;

    if (acl == NULL)
        goto cleanup;

    ppStoreSid = LocalAlloc(LMEM_ZEROINIT, count * sizeof(void *));

    if (ppStoreSid == NULL)
        goto cleanup;

    for (dwLoop = 0; dwLoop < count; dwLoop++) {

        ppStoreSid[dwLoop] = getSID(userArray[dwLoop]);

        if (ppStoreSid[dwLoop] == NULL)
            goto cleanup;

        dwAclLen +=
            GetLengthSid(ppStoreSid[dwLoop]) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD);
    }

    dwAclLen += sizeof(ACL);
    pRetAcl = LocalAlloc(LMEM_ZEROINIT, dwAclLen);

    if (pRetAcl == NULL || !InitializeAcl(pRetAcl, dwAclLen, ACL_REVISION))
        goto cleanup;

    for (dwLoop = 0; dwLoop < count; dwLoop++) {
        /* only adding access allowed ACE's */
        if (!AddAccessAllowedAce(pRetAcl, ACL_REVISION, maskArray[dwLoop],
                                 ppStoreSid[dwLoop]))
            goto cleanup;
    }

    *acl = pRetAcl;
    pRetAcl = NULL;
    bRes = TRUE;

cleanup:

    if (ppStoreSid != NULL) {
        for (dwLoop = 0; dwLoop < count; dwLoop++)
            LocalFree(ppStoreSid[dwLoop]);
        LocalFree(ppStoreSid);
    }

    /* if successful, was set to NULL and left in *acl */
    LocalFree(pRetAcl);

    return bRes;
}

#        define NUM_ACL_ENTRIES 4

DWORD
set_registry_permissions_for_user(WCHAR *hklm_keyname, WCHAR *user)
{
    SECURITY_DESCRIPTOR sd;
    SID *owner = NULL;
    ACL *acl1 = NULL;
    DWORD res;
    HKEY hkey = NULL;

    WCHAR *users[NUM_ACL_ENTRIES] = {
        L"Administrators",
        L"Everyone",
        L"SYSTEM",
        NULL,
    };

    DWORD masks[NUM_ACL_ENTRIES] = {
        KEY_ALL_ACCESS | DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER,
        KEY_READ,
        KEY_ALL_ACCESS | DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER,
        KEY_ALL_ACCESS,
    };

    users[NUM_ACL_ENTRIES - 1] = user;

    DO_DEBUG(DL_VERB, printf("Starting acl..\n"););

    res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, hklm_keyname, 0,
                       platform_key_flags() | KEY_ALL_ACCESS, &hkey);

    if (res != ERROR_SUCCESS)
        goto error_out;

    DO_DEBUG(DL_VERB, printf("Got key handle.\n"););

    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
        res = GetLastError();
        goto error_out;
    }

    owner = getSID(users[0]);

    if (NULL == owner) {
        res = ERROR_INVALID_DATA;
        goto error_out;
    }

    if (!SetSecurityDescriptorOwner(&sd, owner, FALSE)) {
        res = GetLastError();
        goto error_out;
    }

    DO_DEBUG(DL_VERB, printf("Set owner.\n"););

    if (!make_acl(NUM_ACL_ENTRIES, users, masks, &acl1)) {
        res = ERROR_ACCESS_DENIED;
        goto error_out;
    }

    DO_DEBUG(DL_VERB, printf("Made ACL.\n"););

    if (!SetSecurityDescriptorDacl(&sd, TRUE, acl1, FALSE)) {
        res = GetLastError();
        goto error_out;
    }

    if (!IsValidSecurityDescriptor(&sd)) {
        res = GetLastError();
        goto error_out;
    }

    res = RegSetKeySecurity(hkey, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                            &sd);

    DO_DEBUG(DL_VERB, printf("Set sacl.\n"););

    goto cleanup;

error_out:

    /* make sure to return an error */
    if (res == ERROR_SUCCESS)
        res = ERROR_ACCESS_DENIED;

cleanup:

    if (hkey != NULL)
        RegCloseKey(hkey);

    LocalFree(owner);
    LocalFree(acl1);

    return res;
}

/* will limit to 1 MB */
#        define MAX_INSERT_SIZE (1024 * 1024)

static void
insert_file(FILE *file, wchar_t *file_src_name, BOOL delete)
{
    /* 3rd arg not needed but older headers do not declare as optional */
    int fd_src = _wopen(file_src_name, _O_RDONLY | _O_BINARY, 0);
    long length;
    int error;

    if (fd_src == -1) {
        fprintf(file, "Unable to open file \"%S\" for inserting\n", file_src_name);
        return;
    }
    length = _filelength(fd_src);
    if (length == -1L) {
        fprintf(file, "Unable to get file length for file \"%S\"\n", file_src_name);
        return;
    }
    if (length > MAX_INSERT_SIZE) {
        fprintf(file, "File size exceeds max insert length, truncating from %d to %d\n",
                length, MAX_INSERT_SIZE);
        length = MAX_INSERT_SIZE;
    }

    fprintf(file, "Inserting file: name=\"%S\" length=%d\n", file_src_name, length);
    /* hmm, there's prob. a better way to do this ... */
#        define COPY_BUF_SIZE 4096
    {
        char buf[COPY_BUF_SIZE] = { 0 };
        long i = 0;
        while (i + COPY_BUF_SIZE <= length) {
            _read(fd_src, buf, COPY_BUF_SIZE);
            fwrite(buf, 1, COPY_BUF_SIZE, file);
            i += COPY_BUF_SIZE;
        }
        if (i < length) {
            _read(fd_src, buf, length - i);
            fwrite(buf, 1, length - i, file);
        }
    }

    fprintf(file, "Finished inserting file\n");
    if (_read(fd_src, &error, sizeof(error)) != 0)
        fprintf(file, "ERROR : file continues beyond length\n");
    _close(fd_src);
    if (delete) {
        DeleteFile(file_src_name);
    }
    return;
}

/* see utils.h for description */
DWORD
get_violation_info(EVENTLOGRECORD *pevlr, /* INOUT */ VIOLATION_INFO *info)
{
    DO_ASSERT(pevlr != NULL && info != NULL && pevlr->EventID == MSG_SEC_FORENSICS);
    info->report = NULL;
    if (pevlr->EventID != MSG_SEC_FORENSICS)
        return ERROR_INVALID_PARAMETER;
    info->report = get_forensics_filename(pevlr);
    if (file_exists(info->report))
        return ERROR_SUCCESS;
    else
        return ERROR_FILE_NOT_FOUND;
}

wchar_t *canary_process_names[] = { L"canary.exe", L"services.exe", L"iexplore.exe" };
#        define num_canary_processes BUFFER_SIZE_ELEMENTS(canary_process_names)
/* how long to wait for an apparently hung canary process */
#        define CANARY_HANG_WAIT 20000
/* interval to wait for the canary process to do something */
#        define CANARY_SLEEP_WAIT 100

#        define OPTIONS_CANARY_NATIVE \
            L" -list_modules -check_for_hooked_mods_list ntdll.dll"
#        define OPTIONS_CANARY_THIN_CLIENT L""
#        define OPTIONS_CANARY_CLIENT L""
#        define OPTIONS_CANARY_MF L""
#        define OPTIONS_CANARY_INJECT L"-wait"

/* FIXME - could even get ldmps ... */
/* FIXME - xref case 10322 on -syslog_mask 0, eventually should remove and verify
 * expected eventlog output (and get PE to ignore them). */
#        define OPTIONS_THIN_CLIENT L"-thin_client -syslog_mask 0"
#        define OPTIONS_CLIENT L"-client -syslog_mask 0"
/* FIXME - temporary hack so virus scan correctly identified by canary. Weird case
 * since this is considered a survivable violation by default (and so ignores kill
 * proc).
 */
#        define OPTIONS_MF L"-apc_policy 0 -syslog_mask 0"

/* returns the appropriate canary fail code */
static int
run_individual_canary_test(FILE *file, WCHAR *logbase, WCHAR *dr_options, int exe_index,
                           ConfigGroup *policy, WCHAR *exe, WCHAR *exe_args,
                           BOOL inject_test, char *type, BOOL early_test)
{
    STARTUPINFO sinfo = { sizeof(sinfo), NULL, L"",  0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          NULL,          NULL, NULL, NULL };
    PROCESS_INFORMATION pinfo;
    int canary_code = CANARY_SUCCESS;
    WCHAR logbuf[MAX_PATH] = { 0 };
    WCHAR outfile[MAX_PATH];
    WCHAR cmd_buf[5 * MAX_PATH];

    /* set up registry */
    get_unique_filename(logbase, L"canary_logs", L"", logbuf,
                        BUFFER_SIZE_ELEMENTS(logbuf));
    CreateDirectory(logbuf, NULL);
    set_config_group_parameter(get_child(canary_process_names[exe_index], policy),
                               L_DYNAMORIO_VAR_LOGDIR, logbuf);
    set_config_group_parameter(get_child(canary_process_names[exe_index], policy),
                               L_DYNAMORIO_VAR_OPTIONS, dr_options);
    write_config_group(policy);

    /* set up cmd_buf */
    _snwprintf(outfile, BUFFER_SIZE_ELEMENTS(outfile), L"%s\\out.rep", logbuf);
    NULL_TERMINATE_BUFFER(outfile);
    if (early_test) {
        /* we get the canary_process to re-launch itself to run test with early inject */
        _snwprintf(cmd_buf, BUFFER_SIZE_ELEMENTS(cmd_buf),
                   L"\"%s\" \"%s\" -launch_child %s%d \"\\\"%s\\\" %s\"", exe, outfile,
                   inject_test ? L"-verify_inject " : L"", CANARY_HANG_WAIT / 2, outfile,
                   exe_args);
    } else {
        _snwprintf(cmd_buf, BUFFER_SIZE_ELEMENTS(cmd_buf), L"\"%s\" \"%s\" %s", exe,
                   outfile, exe_args);
    }
    NULL_TERMINATE_BUFFER(cmd_buf);

    fprintf(file, "Starting Canary Process \"%S\" core_ops=\"%S\" type=%s%s\n", cmd_buf,
            dr_options, type, inject_test ? " inject" : "");
    if (CreateProcess(NULL, cmd_buf, NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo)) {
        if (inject_test && !early_test) {
            DWORD sleep_count = 0, under_dr_code, ws, build = 0;
            do {
                ws = WaitForSingleObject(pinfo.hProcess, CANARY_SLEEP_WAIT);
                sleep_count += CANARY_SLEEP_WAIT;
                under_dr_code = under_dynamorio_ex(pinfo.dwProcessId, &build);
            } while (ws == WAIT_TIMEOUT && sleep_count < CANARY_HANG_WAIT &&
                     (under_dr_code == DLL_UNKNOWN || under_dr_code == DLL_NONE));
            if (under_dr_code == DLL_UNKNOWN || under_dr_code == DLL_NONE) {
                canary_code = CANARY_FAIL_APP_INIT_INJECTION;
                fprintf(file, "Injection Failed - verify registry settings\n");
            } else {
                fprintf(file, "Verified Injection, build %d\n", build);
            }
            if (ws == WAIT_TIMEOUT)
                terminate_process(pinfo.dwProcessId);
        } else {
            DWORD ws = WaitForSingleObject(pinfo.hProcess, CANARY_HANG_WAIT);
            if (ws == WAIT_TIMEOUT) {
                if (early_test && inject_test) {
                    canary_code = CANARY_FAIL_EARLY_INJECTION;
                    fprintf(file, "Early Injection Failed\n");
                } else {
                    canary_code = CANARY_FAIL_HUNG;
                    fprintf(file, "Canary Hung\n");
                }
                terminate_process(pinfo.dwProcessId);
            } else {
                DWORD exit_code = 0;
                GetExitCodeProcess(pinfo.hProcess, &exit_code);
                /* FIXME - check return value, shouldn't ever fail though */
                if (exit_code != CANARY_PROCESS_EXP_EXIT_CODE) {
                    /* FIXME - the -1 is based on the core value for kill
                     * proc, should export that and use it, or really just check for
                     * violations since we'll want the forensics anyways. Doesn't
                     * disambiguate between dr error and violation. */
                    if (exit_code == (DWORD)-1) {
                        canary_code = CANARY_FAIL_VIOLATION;
                        fprintf(file, "Canary Violation or DR error\n");
                    } else {
                        canary_code = CANARY_FAIL_CRASH;
                        fprintf(file, "Canary Crashed 0x%08x\n", exit_code);
                    }
                } else if (early_test && inject_test) {
                    fprintf(file, "Verified Early Injection\n");
                }
            }
        }
        CloseHandle(pinfo.hProcess);
        CloseHandle(pinfo.hThread);
        {
            HANDLE hFind;
            WIN32_FIND_DATA data;
            WCHAR file_name[MAX_PATH], pattern[MAX_PATH];

            _snwprintf(pattern, BUFFER_SIZE_ELEMENTS(pattern), L"%s\\*.*", logbuf);
            NULL_TERMINATE_BUFFER(pattern);
            hFind = FindFirstFile(pattern, &data);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(data.cFileName, L".") == 0 ||
                        wcscmp(data.cFileName, L"..") == 0)
                        continue;
                    _snwprintf(file_name, BUFFER_SIZE_ELEMENTS(file_name), L"%s\\%s",
                               logbuf, data.cFileName);
                    NULL_TERMINATE_BUFFER(file_name);
                    insert_file(file, file_name, FALSE);
                } while (FindNextFile(hFind, &data));
                FindClose(hFind);
            }
        }
        fprintf(file, "Canary Finished\n");
    } else {
        fprintf(file, "Canary \"%S\" Failed to Launch\n", cmd_buf);
    }
    return canary_code;
}

#        pragma warning( \
            disable : 4127) // conditional expression is constant i.e while (FALSE)

/* see utils.h for description */
BOOL
run_canary_test_ex(FILE *file, /* INOUT */ CANARY_INFO *info, const WCHAR *scratch_folder,
                   const WCHAR *canary_process)
{
    ConfigGroup *policy, *save_policy;
    WCHAR exe_buf[num_canary_processes][MAX_PATH];
    WCHAR log_folder[MAX_PATH];
    DWORD i;
    BOOL autoinject_set = is_autoinjection_set();

    info->canary_code = ERROR_SUCCESS;
    info->url = L"CFail";
    info->msg = L"Canary Failed";

    read_config_group(&save_policy, L_PRODUCT_NAME, TRUE);
    save_policy->should_clear = TRUE;
    read_config_group(&policy, L_PRODUCT_NAME, TRUE);
    policy->should_clear = TRUE;
    remove_children(policy);

    _snwprintf(log_folder, BUFFER_SIZE_ELEMENTS(log_folder), L"%s\\canary_logs",
               scratch_folder);
    NULL_TERMINATE_BUFFER(log_folder);
    CreateDirectory(log_folder, NULL);

    for (i = 0; i < num_canary_processes; i++) {
        _snwprintf(exe_buf[i], BUFFER_SIZE_ELEMENTS(exe_buf[i]), L"%s\\%s",
                   scratch_folder, canary_process_names[i]);
        NULL_TERMINATE_BUFFER(exe_buf[i]);
        if (CopyFile(canary_process, exe_buf[i], FALSE) == 0) {
            fprintf(file, "Failed to copy canary file %S to %S\n", canary_process,
                    exe_buf[i]);
            /* FIXME- continue if file exists from a previous run that didn't clean up */
            info->canary_code = CANARY_UNABLE_TO_TEST;
            goto canary_exit;
        }
        add_config_group(policy, new_config_group(canary_process_names[i]));
        set_config_group_parameter(get_child(canary_process_names[i], policy),
                                   L_DYNAMORIO_VAR_RUNUNDER, L"1");
    }
    write_config_group(policy);

    /* FIXME - monitor eventlog though we should still detect via forensics and/or
     * exit code (crash/violation).  Xref 10322, for now we suppress eventlogs. */
    /* FIXME - the verify injection tests need work, should just talk to canary proc. */
    /* FIXME - verify canary output - necessary? not clear what action would be */
    /* Files are copied, begin runs */

#        define DO_RUN(run_flag, core_ops, canary_options, inject, run_name, test_type) \
            do {                                                                        \
                if (TEST(run_flag, info->run_flags)) {                                  \
                    WCHAR *canary_ops = TEST(run_flag, info->fault_run)                 \
                        ? info->canary_fault_args                                       \
                        : canary_options;                                               \
                    for (i = 0; i < num_canary_processes; i++) {                        \
                        int code = run_individual_canary_test(                          \
                            file, log_folder, core_ops, i, policy, exe_buf[i],          \
                            canary_ops, inject, run_name, FALSE /* not early */);       \
                        if (code >= 0 && test_type != CANARY_TEST_TYPE_NATIVE) {        \
                            code = run_individual_canary_test(                          \
                                file, log_folder, core_ops, i, policy, exe_buf[i],      \
                                canary_ops, inject, run_name, TRUE /* early inject*/);  \
                        }                                                               \
                        if (code < 0) {                                                 \
                            if (CANARY_RUN_REQUIRES_PASS(run_flag, info->run_flags)) {  \
                                info->canary_code = GET_CANARY_CODE(test_type, code);   \
                                goto canary_exit;                                       \
                            }                                                           \
                            break; /* skip remaining tests in run once first failure    \
                                      found */                                          \
                        }                                                               \
                    }                                                                   \
                }                                                                       \
            } while (FALSE)

    /* First the native runs. */
    unset_autoinjection();

    /* native info gathering run. */
    DO_RUN(CANARY_RUN_NATIVE, L"", OPTIONS_CANARY_NATIVE, FALSE, "native",
           CANARY_TEST_TYPE_NATIVE);

    set_autoinjection(); /* Going to do the non-native runs now */

    /* Now the -thin_client inject run */
    DO_RUN(CANARY_RUN_THIN_CLIENT_INJECT, OPTIONS_THIN_CLIENT, OPTIONS_CANARY_INJECT,
           TRUE, "-thin_client", CANARY_TEST_TYPE_THIN_CLIENT);

    /* now the full -thin_client run */
    DO_RUN(CANARY_RUN_THIN_CLIENT, OPTIONS_THIN_CLIENT, OPTIONS_CANARY_THIN_CLIENT, FALSE,
           "-thin_client", CANARY_TEST_TYPE_THIN_CLIENT);

    /* Now the -client run */
    DO_RUN(CANARY_RUN_CLIENT, OPTIONS_CLIENT, OPTIONS_CANARY_CLIENT, FALSE, "-client",
           CANARY_TEST_TYPE_CLIENT);

    /* Now the MF run */
    DO_RUN(CANARY_RUN_MF, OPTIONS_MF, OPTIONS_CANARY_MF, FALSE, "MF",
           CANARY_TEST_TYPE_MF);

#        undef DO_RUN

canary_exit:
    if (autoinject_set)
        set_autoinjection();
    else
        unset_autoinjection();
    free_config_group(policy);
    write_config_group(save_policy);
    free_config_group(save_policy);
    fprintf(file, "Canary code 0x%08x\n", info->canary_code);
    if (info->canary_code >= 0) {
        info->url = L"ctest";
        info->msg = L"Canary success";
    }
    return (info->canary_code >= 0);
}

/* see utils.h for description */
BOOL
run_canary_test(/* INOUT */ CANARY_INFO *info, WCHAR *version_msg)
{
    BOOL result;
    DWORD res;
    FILE *report_file;
    WCHAR scratch_folder[MAX_PATH], canary_process[MAX_PATH];
    const WCHAR *dynamorio_home = get_dynamorio_home();
    const WCHAR *dynamorio_logdir = get_dynamorio_logdir();
    _snwprintf(canary_process, BUFFER_SIZE_ELEMENTS(canary_process),
               L"%s\\bin\\canary.exe", dynamorio_home);
    NULL_TERMINATE_BUFFER(canary_process);
    _snwprintf(scratch_folder, BUFFER_SIZE_ELEMENTS(scratch_folder), L"%s\\canary_test",
               dynamorio_logdir);
    NULL_TERMINATE_BUFFER(scratch_folder);
    /* xref case 10157, let's try to make sure this stays clean */
    delete_tree(scratch_folder);
    CreateDirectory(scratch_folder, NULL);
    /* FIXME - verify directory created */
    /* Using get unique file name since we plan to run this more then once,
     * though only an issue if the caller doesn't cleanup the report file and
     * leaves it locked. */
    get_unique_filename(dynamorio_logdir, L"canary_report", L".crep", info->buf_report,
                        BUFFER_SIZE_ELEMENTS(info->buf_report));
    info->report = info->buf_report;
    report_file = _wfopen(info->report, L"wb");
    /* FIXME - verify file creation */
    fprintf(report_file, "%S\n", version_msg == NULL ? L"unknown version" : version_msg);
    result = run_canary_test_ex(report_file, info, scratch_folder, canary_process);
    res = delete_tree(scratch_folder);
    fprintf(report_file, "Deleted scratch folder \"%S\", code %d\n", scratch_folder, res);
    fclose(report_file);
    return result;
}

#    endif /* WINDOWS */

#else // ifdef UNIT_TEST

int
main()
{
    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    /* read/write file */
    {
        char *test1, *test2;
        char buffy[1024];
        WCHAR *fn = L"utils.tst";
        SIZE_T needed;
        BOOL changed;

        test1 = "This is a stupid file.\r\n\r\nDon't you think?\r\n";
        test2 = "foo\r\n";

        CHECKED_OPERATION(write_file_contents(fn, test1, TRUE));

        DO_ASSERT(ERROR_MORE_DATA == read_file_contents(fn, NULL, 0, &needed));
        DO_ASSERT(strlen(test1) + 1 == needed);

        CHECKED_OPERATION(read_file_contents(fn, buffy, needed, NULL));
        DO_ASSERT(0 == strcmp(test1, buffy));

        CHECKED_OPERATION(write_file_contents_if_different(fn, test1, &changed));
        DO_ASSERT(!changed);

        CHECKED_OPERATION(write_file_contents_if_different(fn, test2, &changed));
        DO_ASSERT(changed);

        CHECKED_OPERATION(read_file_contents(fn, buffy, 1024, NULL));
        DO_ASSERT(0 == strcmp(test2, buffy));
    }

    /* file existence */
    {
        WCHAR *fn = L"tester-file";

        DeleteFile(fn);
        DO_ASSERT(!file_exists(fn));
        DO_ASSERT(!file_exists(fn));

        CHECKED_OPERATION(write_file_contents(fn, "testing", TRUE));
        DO_ASSERT(file_exists(fn));
        DeleteFile(fn);

        DO_ASSERT(file_exists(L"C:\\"));
        DO_ASSERT(!file_exists(L"%%RY:\\\\zZsduf"));
    }

    /* mkdir_with_parents / delete_tree */
    {
        delete_tree(L"__foo_test");
        mkdir_with_parents(L"__foo_test");
        DO_ASSERT(file_exists(L"__foo_test"));
        mkdir_with_parents(L"__foo_test\\foo\\bar\\goo");
        DO_ASSERT(file_exists(L"__foo_test\\foo\\bar\\goo"));
        mkdir_with_parents(L"__foo_test/lib/bar/goo/dood");
        DO_ASSERT(file_exists(L"__foo_test\\lib\\bar\\goo\\dood"));
        CHECKED_OPERATION(delete_tree(L"__foo_test"));
        DO_ASSERT(!file_exists(L"__foo_test"));
        DO_ASSERT(!file_exists(L"__foo_test\\foo\\bar\\goo"));
        DO_ASSERT(!file_exists(L"__foo_test\\lib\\bar\\goo\\dood"));
    }

    /* setup_installation */
    {
        CHECKED_OPERATION(setup_installation(L"C:\\", TRUE));
        CHECKED_OPERATION(setup_installation(L"C:\\foobarra", FALSE));
        DO_ASSERT_WSTR_EQ(L"C:\\", get_dynamorio_home());
        CHECKED_OPERATION(setup_installation(L"C:\\foobarra", TRUE));
        DO_ASSERT_WSTR_EQ(L"C:\\foobarra", get_dynamorio_home());
    }

    {
        WCHAR piname[MAX_PATH];
        BOOL bres = using_system32_for_preinject(NULL);
        printf("Using SYSTEM32 for preinject: %s\n", bres ? "TRUE" : "FALSE");
        CHECKED_OPERATION(get_preinject_name(piname, MAX_PATH));
        printf("Preinject name: %S\n", piname);
    }

    printf("All Test Passed\n");

    return 0;
}

#endif
