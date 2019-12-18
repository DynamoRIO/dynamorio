/* **********************************************************
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

/*
 *
 * processes.h
 *
 */

#ifndef _DETERMINA_PROCESSES_H_
#define _DETERMINA_PROCESSES_H_

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "dr_stats.h" // from src tree

/* dup KPRIORITY and VM_COUNTERS from src/win32/ntdll.h, in order
 *  to have process_info_s self-contained.
 * (so that, eg, src/ module is not necessary to build nodemgr.) */
#ifndef _NTDLL_H_

typedef LONG KPRIORITY;

/* format of data returned by QueryInformationProcess ProcessVmCounters */
typedef struct _VM_COUNTERS {
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} VM_COUNTERS;

#endif

typedef struct process_info_s {
    ULONG ThreadCount;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    WCHAR *ProcessName;
    KPRIORITY BasePriority;
    ULONG ProcessID;
    ULONG InheritedFromProcessID;
    ULONG HandleCount;
    VM_COUNTERS VmCounters;
} process_info_t;

typedef struct module_info_s {
    PVOID BaseAddress;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    // strings are NULL terminated, non persistent and in this address space
    WCHAR *FullDllName; // dll name with path (i.e. "c:\win\sys32\foo.dll")
    WCHAR *BaseDllName; // just dll name (i.e. "foo.dll")
    SHORT LoadCount;
    SHORT TlsIndex;
    ULONG TimeDateStamp;
    process_id_t ProcessID; // ID of Process this module is loaded into
} module_info_t;

/* for process and dll callback routines; the callback should
 *  return FALSE to abort the walk */
typedef BOOL (*processwalk_callback)(process_info_t *pi, void **param);

typedef BOOL (*dllwalk_callback)(module_info_t *mi, void **param);

/*
 * generic process information methods
 */

DWORD
dll_walk_proc(process_id_t ProcessID, dllwalk_callback dwcb, void **param);

DWORD
dll_walk_all(dllwalk_callback dwcb, void **param);

DWORD
process_walk(processwalk_callback pwcb, void **param);

DWORD
get_process_name_and_cmdline(process_id_t pid, WCHAR *name_buf, int name_len,
                             WCHAR *cmdline_buf, int cmdline_len);

DWORD
get_process_name(process_id_t pid, WCHAR *buf, int len);

DWORD
get_process_cmdline(process_id_t pid, WCHAR *buf, int len);

/*
 * heavy-duty kill
 */

DWORD
terminate_process(process_id_t pid);

DWORD
terminate_process_by_exe(WCHAR *exename);

/*
 * DR process status / drmarker functions
 */

int
under_dynamorio(process_id_t ProcessID);

int
under_dynamorio_ex(process_id_t ProcessID, DWORD *build_num);

/* pending_restart is required OUT parameter.
 * status is optional OUT parameter -- if not NULL, will have the status
 *  of the process after successful return.
 * process_cfg is optional OUT parameter -- if not NULL, will have a pointer
 *  to the config group in the config for this process, if available.
 * for internal convenience, may pass NULL for config, in which case
 *  status is the only well-defined output parameter. */
DWORD
check_status_and_pending_restart(ConfigGroup *config, process_id_t pid,
                                 BOOL *pending_restart, int *status,
                                 ConfigGroup **process_cfg);

/* pending_restart is required OUT parameter */
DWORD
is_anything_pending_restart(ConfigGroup *c, BOOL *pending_restart);

/* hotpatch status: the returned pointer must be freed with
 *  free_hotp_status_table below. */
DWORD
get_hotp_status(process_id_t pid, hotp_policy_status_table_t **hotp_status);

/* if a hotp_status table was requested, it must be freed. */
void
free_hotp_status_table(hotp_policy_status_table_t *hotp_status);

dr_statistics_t *
get_dynamorio_stats(process_id_t pid);

void
free_dynamorio_stats(dr_statistics_t *stats);

/*
 * Detach and nudge functions
 */

/* Loads the specified dll in process pid and waits on the loading thread, does
 * not free the dll once the loading thread exits. Usual usage is for the
 * loaded dll to do something in its DllMain. If you do not want the dll to
 * stay loaded its DllMain should return false. To unload a dll from a process
 * later, inject another dll whose dll main unloads that dll and then returns
 * false. If loading_thread != NULL returns a handle to the loading thread (dll
 * could call FreeLibraryAndExitThread on itself in its dll main to return a
 * value out via the exit code). inject_dll provides no way to pass arguments
 * in to the dll. */
DWORD
inject_dll(process_id_t pid, const WCHAR *dll_name, BOOL allow_upgraded_perms,
           DWORD timeout_ms, PHANDLE loading_thread);
/* pending_restart is required OUT parameter */
DWORD
detach_all_not_in_config_group(ConfigGroup *c, DWORD timeout_ms);

/* generic nudge DR, action mask determines which actions will be executed */
DWORD
generic_nudge(process_id_t pid, BOOL allow_upgraded_perms, DWORD action_mask,
              client_id_t id /* optional */, uint64 client_arg /* optional */,
              DWORD timeout_ms);

/* generic nudge DR, action mask determines which actions will be executed,
 *   timeout_ms is the maximum time for a single process nudge
 *   delay_ms is pause between processing each process, 0 no pause
 */
DWORD
generic_nudge_all(DWORD action_mask, uint64 client_arg /* optional */, DWORD timeout_ms,
                  DWORD delay_ms);

#ifdef __cplusplus
}
#endif

#endif //_NM_PROCESSES_H_
