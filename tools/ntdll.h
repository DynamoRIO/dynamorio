/* **********************************************************
 * Copyright (c) 2004-2007 VMware, Inc.  All rights reserved.
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

/* Shared ntdll functionality for tools */

#ifndef NTDLL_H
#define NTDLL_H

#include "win32/ntdll.h"
#undef GET_NTDLL

/* ntdll.dll interface
 * cleaner to use ntdll.lib but too lazy to add to build process
 *
 * WARNING: the Native API is an undocumented API and
 * could change without warning with a new version of Windows.
 */
static HANDLE ntdll_handle = NULL;

#define GET_PROC_ADDR(func, type, name)                                \
    do {                                                               \
        if (ntdll_handle == NULL)                                      \
            ntdll_handle = GetModuleHandle((LPCTSTR) _T("ntdll.dll")); \
        if (func == NULL) {                                            \
            assert(ntdll_handle != NULL);                              \
            func = (type)GetProcAddress(ntdll_handle, (LPCSTR)name);   \
            assert(func != NULL);                                      \
        }                                                              \
    } while (0)

/* A wrapper to define kernel entry point in a static function */
/* In C use only at the end of a block prologue! */
#define GET_NTDLL(NtFunction, type)              \
    typedef int(WINAPI * NtFunction##Type) type; \
    static NtFunction##Type NtFunction;          \
    GET_PROC_ADDR(NtFunction, NtFunction##Type, #NtFunction);

static LONGLONG
get_system_time()
{
    int i, len = 0;
    LARGE_INTEGER time;
    GET_NTDLL(NtQuerySystemTime, (OUT PLARGE_INTEGER time));
    i = NtQuerySystemTime(&time);
    if (i != 0) /* function failed */
        return 0;
    return (LONGLONG)time.QuadPart;
}

static unsigned long
get_uptime()
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    NTSTATUS res;
    GET_NTDLL(NtQueryPerformanceCounter,
              (OUT PLARGE_INTEGER PerformanceCount,
               OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL));
    res = NtQueryPerformanceCounter(&counter, &frequency);
    if (!NT_SUCCESS(res) || frequency.QuadPart == 0)
        return 0;
    return (unsigned long)(counter.QuadPart * 1000 / (frequency.QuadPart));
}

static uint
get_system_load(bool sampled)
{
    int i, len = 0;
    SYSTEM_PROCESSOR_TIMES times;
    LONGLONG clock1, clock2;
    LONGLONG idle1, idle2;
    int idle_load = 0;
    GET_NTDLL(NtQuerySystemInformation,
              (IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
               OUT PVOID SystemInformation, IN ULONG SystemInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQuerySystemInformation(SystemProcessorTimes, &times, sizeof(times), &len);
    if (i != 0) /* function failed */
        return -1;
    /* return length not trustworthy, according to Nebbett, so we don't test it */
    /* actually cannot get load from kernel:
     * can only get idle time since boot, so to get average over
     * an interval must take samples yourself -- this is what taskmgr and
     * perfmon do (perfmon help even says so)
     */
    clock1 = get_system_time();
    idle1 = times.IdleTime.QuadPart;
    /* if sampling, use the clock and idle from prev iteration, otherwise average load
     * over 100 ms */
    if (sampled) {
        static LONGLONG prev_clock = 0, prev_idle = 0;
        if (clock1 > prev_clock) {
            idle_load = (int)((100 * (idle1 - prev_idle)) / (clock1 - prev_clock));
            prev_clock = clock1;
            prev_idle = idle1;
        }
    } else {
        Sleep(100);
        i = NtQuerySystemInformation(SystemProcessorTimes, &times, sizeof(times), &len);
        if (i != 0) /* function failed */
            return -1;
        idle2 = times.IdleTime.QuadPart;
        clock2 = get_system_time();
        if (clock2 > clock1)
            idle_load = (int)((100 * (idle2 - idle1)) / (clock2 - clock1));
    }

    idle_load = (idle_load >= 100) ? 100 : idle_load;

    /* we want %CPU == 100 - idle load */
    return 100 - idle_load;
}

static BOOL
get_system_performance_info(SYSTEM_PERFORMANCE_INFORMATION *sperf_info)
{
    int i, len = 0;
    GET_NTDLL(NtQuerySystemInformation,
              (IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
               OUT PVOID SystemInformation, IN ULONG SystemInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQuerySystemInformation(SystemPerformanceInformation, sperf_info,
                                 sizeof(*sperf_info), &len);
    return (i == 0);
}

/* returns both %CPU and %user time */
static int
get_process_load_ex(HANDLE h, int *cpu, int *user)
{
    KERNEL_USER_TIMES times;
    LONGLONG scheduled_time;
    LONGLONG wallclock_time;
    int i, len = 0;
    /* could share w/ other process info routines... */
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
               OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQueryInformationProcess((HANDLE)h, ProcessTimes, &times, sizeof(times), &len);
    if (i != 0) /* function failed */
        return 0;
    /* return length not trustworthy, according to Nebbett, so we don't test it */
    scheduled_time = times.UserTime.QuadPart + times.KernelTime.QuadPart;
    if (cpu != NULL) {
        /* %CPU == (scheduled time) / (wall clock time) */
        wallclock_time = get_system_time() - times.CreateTime.QuadPart;
        if (wallclock_time == (LONGLONG)0)
            *cpu = 0;
        else
            *cpu = (int)((100 * scheduled_time) / wallclock_time);
    }
    if (user != NULL) {
        /* %user == (user time) / (scheduled time) */
        if (scheduled_time == (LONGLONG)0)
            *user = 0;
        else
            *user = (int)((100 * times.UserTime.QuadPart) / scheduled_time);
    }
    return 1;
}

static bool
get_process_mem_stats(HANDLE h, VM_COUNTERS *info)
{
    int i, len = 0;
    /* could share w/ other process info routines... */
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
               OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQueryInformationProcess((HANDLE)h, ProcessVmCounters, info, sizeof(VM_COUNTERS),
                                  &len);
    if (i != 0) {
        /* function failed */
        memset(info, 0, sizeof(VM_COUNTERS));
    } else
        assert(len == sizeof(VM_COUNTERS));
    return true;
}

#endif /* NTDLL_H */
