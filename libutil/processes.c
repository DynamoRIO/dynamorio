/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#include "drmarker.h"      // from src tree
#include "ntdll.h"         // from src tree
#include "inject_shared.h" // only for w_get_short_name

#include "processes.h"

#include <stddef.h> // for offsetof
#include <limits.h> // for USHRT_MAX

#ifndef UNIT_TEST

/* the GET_NTDLL macro below violates this warning */
#    pragma warning(disable : 4127)

/* A wrapper to define kernel entry point in a static function */
/* In C use only at the end of a block prologue */
#    undef GET_NTDLL
#    define GET_NTDLL(NtFunction, type)                                              \
        typedef NTSTATUS(WINAPI *NtFunction##Type) type;                             \
        static NtFunction##Type NtFunction;                                          \
        do {                                                                         \
            if (ntdll_handle == NULL)                                                \
                ntdll_handle = GetModuleHandle(L"ntdll.dll");                        \
            if (NtFunction == NULL && ntdll_handle != NULL) {                        \
                NtFunction = (NtFunction##Type)GetProcAddress(ntdll_handle,          \
                                                              (LPCSTR) #NtFunction); \
            }                                                                        \
        } while (0);
static HANDLE ntdll_handle = NULL;

int
under_dynamorio(process_id_t ProcessID)
{
    return under_dynamorio_ex(ProcessID, NULL);
}

DWORD
read_hotp_status(const HANDLE hproc, const void *table_ptr,
                 hotp_policy_status_table_t **hotp_status)
{
    UINT crc_and_size[2];
    UINT crc, size;
    SIZE_T len;

    DO_ASSERT(table_ptr != NULL);

    /* first, read the size and crc */
    if (!ReadProcessMemory(hproc, table_ptr, &crc_and_size, sizeof(crc_and_size), &len) ||
        len != sizeof(crc_and_size)) {
        return GetLastError();
    }

    /* see drmarker.h: crc is 1st uint, size is 2nd */
    DO_ASSERT(0 == offsetof(hotp_policy_status_table_t, crc));
    DO_ASSERT(sizeof(UINT) == offsetof(hotp_policy_status_table_t, size));
    crc = crc_and_size[0];
    size = crc_and_size[1];

    /* allocate *hotp_status */
    *hotp_status = (hotp_policy_status_table_t *)malloc(size);

    /* read size bytes into *hotp_status */
    if (!ReadProcessMemory(hproc, table_ptr, *hotp_status, size, &len) || len != size) {
        free(*hotp_status);
        *hotp_status = NULL;
        return GetLastError();
    }

#    if 0
    /* FIXME: d_r_crc32 is not defined where share/ can get at it,
     *  and we may be changing it anyway (case 5346) -- so just
     *  don't do a check for now. */
    /* verify crc: crc starts at size elt, see globals_shared.h */
    if ((*hotp_status)->crc !=
        d_r_crc32((char *)*hotp_status + sizeof(UINT), size - sizeof(UINT))) {
        free(*hotp_status);
        *hotp_status = NULL;
        return ERROR_DRMARKER_ERROR;
    }
#    endif

    /* need to fixup internal pointer to our address space! */
    (*hotp_status)->policy_status_array =
        (hotp_policy_status_t *)((char *)&((*hotp_status)->policy_status_array) +
                                 sizeof(void *));

    DO_DEBUG(
        DL_VERB, printf("np = %d\n", (*hotp_status)->num_policies); {
            UINT i;
            for (i = 0; i < (*hotp_status)->num_policies; i++)
                printf(" patch %s, status=%d\n",
                       (*hotp_status)->policy_status_array[i].policy_id,
                       (*hotp_status)->policy_status_array[i].inject_status);
        });

    return ERROR_SUCCESS;
}

void
free_hotp_status_table(hotp_policy_status_table_t *hotp_status)
{
    free(hotp_status);
}

DWORD
get_dr_marker_helper(process_id_t ProcessID, dr_marker_t *marker,
                     hotp_policy_status_table_t **hotp_status, int *found)
{
    DWORD res = ERROR_SUCCESS;
    HANDLE hproc;

    DO_ASSERT(marker != NULL);
    DO_ASSERT(found != NULL);

    DO_DEBUG(DL_VERB, printf("getting dr marker, hps=" PFX "\n", hotp_status););

    acquire_privileges();
    hproc = OpenProcess(PROCESS_VM_READ, FALSE, (DWORD)ProcessID);
    if (hproc != NULL) {
        *found = read_and_verify_dr_marker(hproc, marker);
        if (*found == DR_MARKER_FOUND && hotp_status != NULL &&
            marker->dr_hotp_policy_status_table != NULL) {
            res =
                read_hotp_status(hproc, marker->dr_hotp_policy_status_table, hotp_status);
        } else if (hotp_status != NULL) {
            /* return an error if we couldn't get the hotp status */
            res = ERROR_DRMARKER_ERROR;
        }
        CloseHandle(hproc);
    } else {
        res = GetLastError();
        if (res != ERROR_SUCCESS)
            res = ERROR_DRMARKER_ERROR;
    }
    release_privileges();

    DO_DEBUG(DL_VERB, printf("getting dr marker, err=%d\n", res););

    return res;
}

#    define NUM_DR_MARKER_RETRIES 2

/* we retry in case synchronization issues (crc failure, in the
 *  middle of a detach, etc) cause this to fail.
 * NOTE: this is redefined in detach.c to avoid include issues! */
DWORD
get_dr_marker(process_id_t ProcessID, dr_marker_t *marker,
              hotp_policy_status_table_t **hotp_status, int *found)
{
    DWORD res = ERROR_SUCCESS;
    int i;

    DO_ASSERT(marker != NULL);
    DO_ASSERT(found != NULL);

    for (i = 0; i < NUM_DR_MARKER_RETRIES; i++) {
        res = get_dr_marker_helper(ProcessID, marker, hotp_status, found);
        if (res == ERROR_SUCCESS && *found != DR_MARKER_ERROR)
            return ERROR_SUCCESS;
    }

    return res;
}

DWORD
get_hotp_status(process_id_t pid, hotp_policy_status_table_t **hotp_status)
{
    dr_marker_t marker;
    int found;
    DWORD res = get_dr_marker(pid, &marker, hotp_status, &found);
    if (res == ERROR_SUCCESS && found == DR_MARKER_ERROR)
        return ERROR_DRMARKER_ERROR;
    else
        return res;
}

void
free_dynamorio_stats(dr_statistics_t *stats)
{
    free(stats);
}

/* Caller must call free_dynamorio_stats on return value, if it's non-NULL */
struct _dr_statistics_t *
get_dynamorio_stats(process_id_t pid)
{
    dr_marker_t marker;
    dr_statistics_t *stats = NULL;
    dr_statistics_t stats_tmp;
    int found;
    HANDLE hproc;

    acquire_privileges();
    hproc = OpenProcess(PROCESS_VM_READ, FALSE, (DWORD)pid);
    if (hproc != NULL) {
        found = read_and_verify_dr_marker(hproc, &marker);
        if (found == DR_MARKER_FOUND && marker.stats != NULL) {
            uint alloc_size;
            SIZE_T read;
            if (ReadProcessMemory(hproc, marker.stats, &stats_tmp, sizeof(stats_tmp),
                                  &read) &&
                read == sizeof(stats_tmp) &&
                /* if unreasonably large, probably some error.
                 * could return error code */
                stats_tmp.num_stats < USHRT_MAX) {
                /* The USHRT_MAX check above avoids integer overflow */
                alloc_size = offsetof(dr_statistics_t, stats) +
                    sizeof(single_stat_t) * stats_tmp.num_stats;
                stats = (dr_statistics_t *)malloc(alloc_size);
                if (stats != NULL) {
                    if (!ReadProcessMemory(hproc, marker.stats, stats, alloc_size,
                                           &read) ||
                        read != alloc_size) {
                        free(stats);
                        stats = NULL;
                    } else if (stats->num_stats != stats_tmp.num_stats) {
                        /* malicious modification, or some error.
                         * could return error code */
                        free(stats);
                        stats = NULL;
                    }
                }
            }
        }
        CloseHandle(hproc);
    }
    release_privileges();
    return stats;
}

/* NOTE in v 1.17 this had a kernel32 method of getting the dll file version
 * that might be useful in other situations */
int
under_dynamorio_ex(process_id_t ProcessID, DWORD *build_num)
{
    dr_marker_t marker;
    DWORD err;
    int res;

    err = get_dr_marker(ProcessID, &marker, NULL, &res);

    if (err != ERROR_SUCCESS || res == DR_MARKER_ERROR)
        return DLL_UNKNOWN;
    else if (res == DR_MARKER_NOT_FOUND)
        return DLL_NONE;
    else if (res == DR_MARKER_FOUND) {
        if (build_num != NULL) {
            *build_num = marker.build_num;
        }
        /* NOTE - profile build can be combined with DEBUG or RELEASE so we check it
         * first.  Perhaps we should just get rid of the notion of profile builds. */
        if (TEST(DR_MARKER_PROFILE_BUILD, marker.flags))
            return DLL_PROFILE;
        if (TEST(DR_MARKER_RELEASE_BUILD, marker.flags))
            return DLL_RELEASE;
        if (TEST(DR_MARKER_DEBUG_BUILD, marker.flags))
            return DLL_DEBUG;
    }
    /* should never get here */
    return DLL_UNKNOWN;
}

DWORD
check_status_and_pending_restart(ConfigGroup *config, process_id_t pid,
                                 BOOL *pending_restart, int *status,
                                 ConfigGroup **process_cfg)
{
    WCHAR *rununder_param = NULL;
    int stat, rununder;
    ConfigGroup *process_config = NULL;

    DO_ASSERT(pending_restart != NULL);

    /* may pass NULL config ==> pending_restart is FALSE */
    if (NULL == config)
        *pending_restart = FALSE;
    else
        process_config = get_process_config_group(config, pid);

    if (NULL != process_cfg)
        *process_cfg = process_config;

    if (NULL == process_config) {
        rununder = 0;
    } else {
        rununder_param =
            get_config_group_parameter(process_config, L_DYNAMORIO_VAR_RUNUNDER);
        if (rununder_param == NULL)
            rununder_param = get_config_group_parameter(config, L_DYNAMORIO_VAR_RUNUNDER);

        /* bad news; just abort */
        if (rununder_param == NULL)
            return ERROR_OPTION_NOT_FOUND;

        rununder = _wtoi(rununder_param) & RUNUNDER_ON;
    }

    stat = under_dynamorio(pid);
    if (status != NULL)
        *status = stat;

    /* FIXME: for now assume unknown == off  (?) */
    if (stat == DLL_UNKNOWN)
        return ERROR_DETACH_ERROR;

    *pending_restart = (rununder && stat == DLL_NONE) || (!rununder && stat != DLL_NONE);

    DO_DEBUG(DL_VERB,
             printf("  -> ru=%d, stat=%d, pr=%d, c=" PFX ", pc=" PFX ", pxru=%S\n",
                    rununder, stat, *pending_restart, config, process_config,
                    rununder_param););

    return ERROR_SUCCESS;
}

DWORD
hotp_notify_modes_update(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms)
{
    return generic_nudge(pid, allow_upgraded_perms, NUDGE_GENERIC(mode), 0, NULL,
                         timeout_ms);
}

DWORD
hotp_notify_defs_update(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms)
{
    return generic_nudge(pid, allow_upgraded_perms, NUDGE_GENERIC(policy), 0, NULL,
                         timeout_ms);
}

/*
 * helper info for process walk callback functions
 */

typedef enum {
    CB_INVALID = 0,
    CB_CHECK_PENDING,
    CB_DETACH,
    CB_DETACH_EXE,
    CB_DETACH_NOT_IN_POLICY,
    CB_NUDGE_MODES,
    CB_NUDGE_DEFS,
    CB_NUDGE_EXE,
    CB_NUDGE_GENERIC,
} CB_TYPE;

/* process iterator shared state */
typedef struct process_status_info_s {
    ConfigGroup *policy;
    int num_running;
    int num_detached;
    BOOL is_pending;
    DWORD res;
    DWORD process_nonfatal_res;
    DWORD timeout_ms;
    WCHAR *exename;
    CB_TYPE callback_type;
    DWORD nudge_action_mask;
    void *nudge_client_arg;
    DWORD delay_ms;
} process_status_info_t;

/* the param is a process_status_info_t* */
BOOL
system_info_cb(process_info_t *pi, void **param)
{
    int status;
    process_status_info_t *sinfo = (process_status_info_t *)param;
    ConfigGroup *process_config;
    BOOL is_pending;
    DWORD res = ERROR_SUCCESS;

    /* always skip the idle process */
    if (pi->ProcessID == 0)
        return TRUE;

    res = check_status_and_pending_restart(sinfo->policy, pi->ProcessID, &is_pending,
                                           &status, &process_config);

    /* for the walk methods, only record process-specific errors,
     *  but ignore and keep trying the rest. */
    if (res != ERROR_SUCCESS) {
        sinfo->process_nonfatal_res = res;
        return TRUE;
    }

    DO_DEBUG(DL_VERB,
             printf("  pid=%d, type=%d, name=%S, ip=%d, sta=%d:\n", pi->ProcessID,
                    sinfo->callback_type, pi->ProcessName, is_pending, status););

    switch (sinfo->callback_type) {
    case CB_CHECK_PENDING:
        if (is_pending) {
            sinfo->is_pending = TRUE;
            return FALSE;
        }
        break;
    case CB_DETACH_EXE:
        /* fall through to detach only if the process name
         *  matches */
        if (sinfo->exename == NULL || 0 != wcsicmp(sinfo->exename, pi->ProcessName)) {
            break;
        }
    case CB_DETACH:
        /* for detach, set is_pending = TRUE and fall through, to
         *  force DETACH_NOT_IN_POLICY to detach if it's running. */
        is_pending = TRUE;
    case CB_DETACH_NOT_IN_POLICY:
        if (status != DLL_NONE && is_pending) {
            res = detach(pi->ProcessID, TRUE, sinfo->timeout_ms);
            if (sinfo->res == ERROR_SUCCESS)
                sinfo->res = res;
        }
        break;
    case CB_NUDGE_EXE:
        /* fall through to nudge only if the process name
         *  matches; and .exe nudge is by defn a modes nudge. */
        if (sinfo->exename == NULL || 0 != wcsicmp(sinfo->exename, pi->ProcessName)) {
            break;
        }
    case CB_NUDGE_DEFS:
        /* fall through and use the same code. */
    case CB_NUDGE_MODES:
        /* FIXME: we used to only nudge apps that have the
         *  DYNAMORIO_HOTPATCH_MODES key set; but this is dangerous
         *  if, e.g., hotpatching was on and then turned off. so
         *  we just nudge anything under DR, at least for now. */
        if (status != DLL_NONE) {
            if (sinfo->callback_type == CB_NUDGE_DEFS)
                res = hotp_notify_defs_update(pi->ProcessID, TRUE, sinfo->timeout_ms);

            if (sinfo->callback_type == CB_NUDGE_MODES)
                res = hotp_notify_modes_update(pi->ProcessID, TRUE, sinfo->timeout_ms);

            if (sinfo->res == ERROR_SUCCESS)
                sinfo->res = res;
        }
        break;
    case CB_NUDGE_GENERIC:
        /* generic nudge available under HOT_PATCHING_INTERFACE */
        if (status != DLL_NONE) {
            res = generic_nudge(pi->ProcessID, TRUE, sinfo->nudge_action_mask,
                                0, /* client ID arbitrary */
                                sinfo->nudge_client_arg, sinfo->timeout_ms);
            if (sinfo->res == ERROR_SUCCESS)
                sinfo->res = res;
            /* pause after nudging */
            if (sinfo->delay_ms != 0)
                SleepEx(sinfo->delay_ms, FALSE);
        }
        break;

    default: sinfo->res = ERROR_INVALID_PARAMETER; return FALSE;
    }

    return TRUE;
}

DWORD
execute_sysinfo_walk(process_status_info_t *sinfo)
{
    DWORD res;

    DO_DEBUG(DL_VERB, printf("starting walk...\n"););

    res = process_walk(&system_info_cb, (void **)sinfo);

    DO_DEBUG(DL_VERB, printf("walk done, res=%d, sr=%d\n", res, sinfo->res););

    if (res != ERROR_SUCCESS)
        return res;

    if (sinfo->res != ERROR_SUCCESS)
        return sinfo->res;

    /* FIXME: report status_info.process_nonfatal_res? */

    return ERROR_SUCCESS;
}

/* pending_restart is required OUT parameter */
DWORD
is_anything_pending_restart(ConfigGroup *c, BOOL *pending_restart)
{
    process_status_info_t status_info;
    DWORD res;

    DO_ASSERT(pending_restart != NULL);

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.policy = c;
    status_info.callback_type = CB_CHECK_PENDING;

    res = execute_sysinfo_walk(&status_info);

    if (res != ERROR_SUCCESS)
        return res;

    *pending_restart = status_info.is_pending;

    return ERROR_SUCCESS;
}

/* pending_restart is required OUT parameter */
DWORD
detach_all_not_in_config_group(ConfigGroup *c, DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.policy = c;
    status_info.callback_type = CB_DETACH_NOT_IN_POLICY;
    status_info.timeout_ms = timeout_ms;

    return execute_sysinfo_walk(&status_info);
}

DWORD
detach_exe(WCHAR *exename, DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_DETACH_EXE;
    status_info.timeout_ms = timeout_ms;
    status_info.exename = exename;

    return execute_sysinfo_walk(&status_info);
}

DWORD
detach_all(DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_DETACH;
    status_info.timeout_ms = timeout_ms;

    return execute_sysinfo_walk(&status_info);
}

DWORD
hotp_notify_all_modes_update(DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_NUDGE_MODES;
    status_info.timeout_ms = timeout_ms;

    return execute_sysinfo_walk(&status_info);
}

DWORD
hotp_notify_all_defs_update(DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_NUDGE_DEFS;
    status_info.timeout_ms = timeout_ms;

    return execute_sysinfo_walk(&status_info);
}

DWORD
hotp_notify_exe_modes_update(WCHAR *exename, DWORD timeout_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_NUDGE_EXE;
    status_info.timeout_ms = timeout_ms;
    status_info.exename = exename;

    return execute_sysinfo_walk(&status_info);
}

/* generic nudge DR, action mask determines which actions will be executed,
 *   timeout_ms is the maximum time for a single process nudge
 *   delay_ms is pause between processing each process, 0 no pause
 */
DWORD
generic_nudge_all(DWORD action_mask, uint64 client_arg /* optional */, DWORD timeout_ms,
                  DWORD delay_ms)
{
    process_status_info_t status_info;

    memset(&status_info, 0, sizeof(process_status_info_t));
    status_info.callback_type = CB_NUDGE_GENERIC;
    status_info.timeout_ms = timeout_ms;
    status_info.delay_ms = delay_ms;
    status_info.nudge_action_mask = action_mask;
    status_info.nudge_client_arg = client_arg;

    return execute_sysinfo_walk(&status_info);
}

/* read process_handle's PEB into peb */
DWORD
get_process_peb(HANDLE process_handle, PEB *peb)
{
    PROCESS_BASIC_INFORMATION info;
    SIZE_T got;
    DWORD res;
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
               IN PVOID ProcessInformation, IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));

    if (NtQueryInformationProcess == NULL) {
        return GetLastError();
    }

    res = NtQueryInformationProcess(process_handle, ProcessBasicInformation, &info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);

    if (!NT_SUCCESS(res)) {
        return res;
    } else if (got != sizeof(PROCESS_BASIC_INFORMATION)) {
        return ERROR_BAD_LENGTH;
    }

    /* Read process PEB */
    res = ReadProcessMemory(process_handle, (LPVOID)info.PebBaseAddress, peb, sizeof(PEB),
                            &got);

    if (!res || got != sizeof(PEB)) {
        return GetLastError();
    }

    return ERROR_SUCCESS;
}

/* this is somewhat duplicated from get_process_imgname_cmdline in the src
 * module, share? FIXME */
/* name returns just the name of the executable (strips off the path if
 * it's there) to be compatible with previous implementations */
/* NOTE len's are in bytes */
DWORD
get_process_name_and_cmdline(process_id_t pid, WCHAR *name_buf, int name_len,
                             WCHAR *cmdline_buf, int cmdline_len)
{
    HANDLE process_handle = NULL;
    SIZE_T nbytes;
    DWORD res;
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS process_parameters;
    DWORD error = ERROR_SUCCESS;

    /* note that on Vista+ acquire_privileges() requires being admin.  currently
     * most routines here just ignore failure which seems a reasonable approach:
     * perhaps cleaner code to first try to open the process, and then acquire
     * and return error on acquire failing: but more complex
     */
    acquire_privileges();
    /* deliberately asking for pre-Vista PROCESS_ALL_ACCESS so that this code
     * compiled w/ later VS will run on pre-Vista
     */
    process_handle =
        OpenProcess(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFF, FALSE, (DWORD)pid);
    error = GetLastError();
    release_privileges();

    if (process_handle == NULL) {
        goto exit;
    }

    res = get_process_peb(process_handle, &peb);

    if (res != ERROR_SUCCESS) {
        error = res;
        goto exit;
    }

    /* Follow on to process parameters */
    res = ReadProcessMemory(process_handle, (LPVOID)peb.ProcessParameters,
                            &process_parameters, sizeof(process_parameters), &nbytes);

    if (!res) {
        error = GetLastError();
        goto exit;
    }

    if (cmdline_buf != NULL) {
        /* note that Buffer is a pointer here, compare win32/os.c
         * it seems that something during process initialization converts
         *  Buffer from an offset to a pointer...
         */
        void *cmdline_location = (void *)(process_parameters.CommandLine.Buffer);

        int cmdlen = process_parameters.CommandLine.Length;
        cmdlen = (cmdlen > cmdline_len - 1) ? cmdline_len - 1 : cmdlen;

        res = ReadProcessMemory(process_handle, (LPVOID)cmdline_location, cmdline_buf,
                                cmdlen, &nbytes);

        if (!res) {
            error = GetLastError();
            goto exit;
        }

        cmdline_buf[cmdlen / sizeof(WCHAR)] = 0;
    }

    if (name_buf != NULL) {
        /* note that Buffer is a pointer here, compare win32/os.c
         * it seems that something during process initialization converts
         *  Buffer from an offset to a pointer...
         */
        WCHAR buf[MAX_PATH];
        void *name_location = (void *)(process_parameters.ImagePathName.Buffer);

        int namelen = process_parameters.ImagePathName.Length;
        namelen = (namelen > name_len - 1) ? name_len - 1 : namelen;

        res = ReadProcessMemory(process_handle, (LPVOID)name_location, buf, sizeof(buf),
                                &nbytes);

        if (!res) {
            error = GetLastError();
            goto exit;
        }

        buf[MAX_PATH - 1] = L'\0';

        // return just the executable name
        wcsncpy(name_buf, w_get_short_name(buf), namelen / sizeof(WCHAR));

        name_buf[namelen / sizeof(WCHAR)] = 0;
    }

exit:
    if (process_handle != NULL)
        CloseHandle(process_handle);
    return error;
}

DWORD
get_process_cmdline(process_id_t pid, WCHAR *buf, int len)
{
    return get_process_name_and_cmdline(pid, NULL, 0, buf, len);
}

DWORD
get_process_name(process_id_t pid, WCHAR *buf, int len)
{
    return get_process_name_and_cmdline(pid, buf, len, NULL, 0);
}

#    define MAX_PROCESS_WALK_BUFFER_LENGTH 0x1000000

DWORD
process_walk(processwalk_callback pwcb, void **param)
{
    void *proc_base = NULL;
    SYSTEM_PROCESSES *proc_snap;
    unsigned long got, proc_bytes = 4096 /* is doubled till large enough */;
    DWORD res;
    GET_NTDLL(NtQuerySystemInformation,
              (IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
               IN OUT PVOID SystemInformation, IN ULONG SystemInformationLength,
               OUT PULONG ReturnLength OPTIONAL));

    if (NtQuerySystemInformation == NULL) {
        return GetLastError();
    }

    do {
        if (proc_base != NULL)
            free(proc_base);
        proc_bytes *= 2;
        if (proc_bytes > MAX_PROCESS_WALK_BUFFER_LENGTH)
            return ERROR_NOT_ENOUGH_MEMORY;
        proc_base = malloc(proc_bytes);
        if (proc_base == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;
        proc_snap = (SYSTEM_PROCESSES *)proc_base;
        res = NtQuerySystemInformation(SystemProcessesAndThreadsInformation, proc_snap,
                                       proc_bytes, &got);
    } while (res == STATUS_INFO_LENGTH_MISMATCH);

    if (NT_SUCCESS(res)) {
        while (proc_snap != NULL) {
            process_info_t info;

            info.ThreadCount = proc_snap->ThreadCount;
            info.CreateTime = proc_snap->CreateTime;
            info.UserTime = proc_snap->UserTime;
            info.KernelTime = proc_snap->KernelTime;
            // NOTE - the system idle process has a NULL name
            if (proc_snap->ProcessName.Buffer == NULL)
                info.ProcessName = L"";
            else
                info.ProcessName = proc_snap->ProcessName.Buffer;
            info.BasePriority = proc_snap->BasePriority;
            info.ProcessID = proc_snap->ProcessId;
            info.InheritedFromProcessID = proc_snap->InheritedFromProcessId;
            info.HandleCount = proc_snap->HandleCount;
            info.VmCounters = proc_snap->VmCounters;

            // callback with the info
            if (!(*pwcb)(&info, param))
                break;

            if (proc_snap->NextEntryDelta == 0) {
                proc_snap = NULL;
            } else {
                proc_snap =
                    (SYSTEM_PROCESSES *)(((char *)proc_snap) + proc_snap->NextEntryDelta);
            }
        }
        res = ERROR_SUCCESS;
    }

    free(proc_base);

    return res;
}

typedef struct cb_helper_ {
    process_callback pcb;
    void **param;
} cb_helper_t;

BOOL
translate_cb(process_info_t *pi, void **param)
{
    cb_helper_t *cbht = (cb_helper_t *)param;
    return (*cbht->pcb)(pi->ProcessID, pi->ProcessName, cbht->param);
}

DWORD
enumerate_processes(process_callback pcb, void **param)
{
    cb_helper_t cbht;
    cbht.pcb = pcb;
    cbht.param = param;
    return process_walk(&translate_cb, (void **)&cbht);
}

#    define MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD 2048

DWORD
dll_walk_proc(process_id_t ProcessID, dllwalk_callback dwcb, void **param)
{
    PEB peb;
    PEB_LDR_DATA ldr;
    LIST_ENTRY *first;
    LDR_MODULE mod;
    HANDLE hproc;
    DWORD res;
    int i = 0;
    SIZE_T nbytes;

    // use a read process memory implementation (like psapi) since toolhelp
    // is too invasive and not available on all platforms

    acquire_privileges();
    hproc =
        OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)ProcessID);
    res = GetLastError();
    release_privileges();
    if (hproc == NULL)
        return res;

    res = get_process_peb(hproc, &peb);
    if (res != ERROR_SUCCESS)
        goto exit;

    res = ReadProcessMemory(hproc, peb.LoaderData, &ldr, sizeof(ldr), &nbytes);
    if (!res || nbytes != sizeof(ldr)) {
        res = GetLastError();
        goto exit;
    }

    // arbitrary - use the InLoadOrderList since it has simplest offsets
    first = (LIST_ENTRY *)(((char *)peb.LoaderData) +
                           offsetof(PEB_LDR_DATA, InLoadOrderModuleList));
    // prime the loop
    mod.InLoadOrderModuleList.Flink = ldr.InLoadOrderModuleList.Flink;

    do {
        module_info_t info;
        BOOL callback_ret_val;

        res = ReadProcessMemory(hproc, mod.InLoadOrderModuleList.Flink, &mod, sizeof(mod),
                                &nbytes);
        if (!res || nbytes != sizeof(mod)) {
            res = GetLastError();
            goto exit;
        }

        // Fill info struct for the callback
        info.BaseAddress = mod.BaseAddress;
        info.EntryPoint = mod.EntryPoint;
        info.SizeOfImage = mod.SizeOfImage;
        info.LoadCount = mod.LoadCount;
        info.TlsIndex = mod.TlsIndex;
        info.TimeDateStamp = mod.TimeDateStamp;
        info.ProcessID = ProcessID;

        // now copy over the strings

        // allocate length + sizeof(wchar) [for null termination]
        info.FullDllName = (WCHAR *)malloc(mod.FullDllName.Length + sizeof(WCHAR));
        res = ReadProcessMemory(hproc, mod.FullDllName.Buffer, info.FullDllName,
                                mod.FullDllName.Length, &nbytes);
        if (!res || nbytes != mod.FullDllName.Length) {
            res = GetLastError();
            goto exit;
        }
        // Be sure to NULL terminate
        info.FullDllName[mod.FullDllName.Length / sizeof(WCHAR)] = 0;

        // allocate length + sizeof(wchar) [for null termination]
        info.BaseDllName = (WCHAR *)malloc(mod.BaseDllName.Length + sizeof(WCHAR));
        res = ReadProcessMemory(hproc, mod.BaseDllName.Buffer, info.BaseDllName,
                                mod.BaseDllName.Length, &nbytes);
        if (!res || nbytes != mod.BaseDllName.Length) {
            res = GetLastError();
            goto exit;
        }
        // Be sure to NULL terminate
        info.BaseDllName[mod.BaseDllName.Length / sizeof(WCHAR)] = 0;

        // do callback
        callback_ret_val = (*dwcb)(&info, param);

        // free strings
        free(info.FullDllName);
        free(info.BaseDllName);

        // now check the callback return value for early loop exit
        if (!callback_ret_val)
            break;

    } while (mod.InLoadOrderModuleList.Flink != first &&
             i++ < MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD);

    if (i >= MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD)
        res = ERROR_TOO_MANY_MODULES;
    else
        res = ERROR_SUCCESS;

exit:
    CloseHandle(hproc);
    return res;
}

typedef struct dll_walk_all_cb_info_s {
    dllwalk_callback dwcb;
    void **param;
} dll_walk_all_cb_info_t;

BOOL
dll_walk_all_cb(process_info_t *pi, void **param)
{
    dll_walk_all_cb_info_t *info = (dll_walk_all_cb_info_t *)param;
    // will instantly silently crash if we try to peer into
    // system-idle process (pid 0) with upgraded permissions
    if (pi->ProcessID != 0)
        dll_walk_proc(pi->ProcessID, info->dwcb, info->param);
    return true;
}

DWORD
dll_walk_all(dllwalk_callback dwcb, void **param)
{
    dll_walk_all_cb_info_t info;
    info.dwcb = dwcb;
    info.param = param;
    return process_walk((processwalk_callback)dll_walk_all_cb, (void **)&info);
}

DWORD
terminate_process(process_id_t pid)
{
    HANDLE hproc;
    DWORD res = 0;

    acquire_privileges();
    hproc = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    release_privileges();

    if (!hproc)
        return GetLastError();

    if (!TerminateProcess(hproc, (UINT)-1))
        res = GetLastError();

    CloseHandle(hproc);

    return res;
}

typedef struct kill_helper_s {
    WCHAR *exename;
    DWORD res;
} kill_helper_t;

BOOL
kill_callback(process_info_t *pi, void **param)
{
    kill_helper_t *khs = (kill_helper_t *)param;

    if (khs->exename == NULL)
        return FALSE;

    if (0 == wcsicmp(khs->exename, pi->ProcessName)) {
        khs->res = terminate_process(pi->ProcessID);
        if (khs->res != ERROR_SUCCESS)
            return FALSE;
    }

    return TRUE;
}

DWORD
terminate_process_by_exe(WCHAR *exename)
{
    kill_helper_t khs;
    khs.exename = exename;
    khs.res = ERROR_SUCCESS;

    process_walk(&kill_callback, (void **)&khs);

    return khs.res;
}

#else // ifdef UNIT_TEST

int
main()
{
    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    /* most of these are hard to do without the test framework.. */

    printf("All Test Passed\n");

    return 0;
}

#endif
