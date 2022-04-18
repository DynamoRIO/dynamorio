/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.   All rights reserved.
 * Copyright (c) 2009-2010 Derek Bruening   All rights reserved.
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

/* kernel32 and kernelbase redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "drwinapi.h"
#include "drwinapi_private.h"

#ifndef WINDOWS
#    error Windows-only
#endif

/* We use a hashtale for faster lookups than a linear walk */
static strhash_table_t *kernel32_table;

static const redirect_import_t redirect_kernel32[] = {
    /* Process and thread-related routines */
    { "GetCurrentProcess", (app_pc)redirect_GetCurrentProcess },
    { "GetCurrentProcessId", (app_pc)redirect_GetCurrentProcessId },
    { "GetCurrentThread", (app_pc)redirect_GetCurrentThread },
    { "GetCurrentThreadId", (app_pc)redirect_GetCurrentThreadId },
    { "ExitProcess", (app_pc)redirect_ExitProcess },
    /* To avoid the FlsCallback being interpreted, and to avoid hangs
     * and other issues with Fls locks (i#875).
     */
    { "FlsAlloc", (app_pc)redirect_FlsAlloc },
    { "FlsFree", (app_pc)redirect_FlsFree },
    { "FlsGetValue", (app_pc)redirect_FlsGetValue },
    { "FlsSetValue", (app_pc)redirect_FlsSetValue },

    /* Library routines */
    /* As an initial interception of loader queries, but simpler than
     * intercepting Ldr*: plus, needed to intercept FlsAlloc called by msvcrt
     * init routine.
     * XXX i#235: redirect GetModuleHandle{ExA,ExW} as well
     */
    { "GetModuleHandleA", (app_pc)redirect_GetModuleHandleA },
    { "GetModuleHandleW", (app_pc)redirect_GetModuleHandleW },
    { "GetProcAddress", (app_pc)redirect_GetProcAddress },
    { "LoadLibraryA", (app_pc)redirect_LoadLibraryA },
    { "LoadLibraryW", (app_pc)redirect_LoadLibraryW },
    { "LoadLibraryExA", (app_pc)redirect_LoadLibraryExA },
    { "LoadLibraryExW", (app_pc)redirect_LoadLibraryExW },
    { "FreeLibrary", (app_pc)redirect_FreeLibrary },
    { "GetModuleFileNameA", (app_pc)redirect_GetModuleFileNameA },
    { "GetModuleFileNameW", (app_pc)redirect_GetModuleFileNameW },

    /* Memory-related routines */
    { "DecodePointer", (app_pc)redirect_DecodePointer },
    { "EncodePointer", (app_pc)redirect_EncodePointer },
    { "GetProcessHeap", (app_pc)redirect_GetProcessHeap },
    { "HeapAlloc", (app_pc)redirect_HeapAlloc },
    { "HeapCompact", (app_pc)redirect_HeapCompact },
    { "HeapCreate", (app_pc)redirect_HeapCreate },
    { "HeapDestroy", (app_pc)redirect_HeapDestroy },
    { "HeapFree", (app_pc)redirect_HeapFree },
    { "HeapReAlloc", (app_pc)redirect_HeapReAlloc },
    { "HeapSetInformation ", (app_pc)redirect_HeapSetInformation },
    { "HeapSize", (app_pc)redirect_HeapSize },
    { "HeapValidate", (app_pc)redirect_HeapValidate },
    { "HeapWalk", (app_pc)redirect_HeapWalk },
    { "IsBadReadPtr", (app_pc)redirect_IsBadReadPtr },
    { "LocalAlloc", (app_pc)redirect_LocalAlloc },
    { "LocalFree", (app_pc)redirect_LocalFree },
    { "LocalReAlloc", (app_pc)redirect_LocalReAlloc },
    { "LocalLock", (app_pc)redirect_LocalLock },
    { "LocalHandle", (app_pc)redirect_LocalHandle },
    { "LocalUnlock", (app_pc)redirect_LocalUnlock },
    { "LocalSize", (app_pc)redirect_LocalSize },
    { "LocalFlags", (app_pc)redirect_LocalFlags },
    { "ReadProcessMemory", (app_pc)redirect_ReadProcessMemory },
    { "VirtualAlloc", (app_pc)redirect_VirtualAlloc },
    { "VirtualFree", (app_pc)redirect_VirtualFree },
    { "VirtualProtect", (app_pc)redirect_VirtualProtect },
    { "VirtualQuery", (app_pc)redirect_VirtualQuery },
    { "VirtualQueryEx", (app_pc)redirect_VirtualQueryEx },

    /* File-related routines */
    { "CreateDirectoryA", (app_pc)redirect_CreateDirectoryA },
    { "CreateDirectoryW", (app_pc)redirect_CreateDirectoryW },
    { "RemoveDirectoryA", (app_pc)redirect_RemoveDirectoryA },
    { "RemoveDirectoryW", (app_pc)redirect_RemoveDirectoryW },
    { "GetCurrentDirectoryA", (app_pc)redirect_GetCurrentDirectoryA },
    { "GetCurrentDirectoryW", (app_pc)redirect_GetCurrentDirectoryW },
    { "SetCurrentDirectoryA", (app_pc)redirect_SetCurrentDirectoryA },
    { "SetCurrentDirectoryW", (app_pc)redirect_SetCurrentDirectoryW },
    { "CreateFileA", (app_pc)redirect_CreateFileA },
    { "CreateFileW", (app_pc)redirect_CreateFileW },
    { "DeleteFileA", (app_pc)redirect_DeleteFileA },
    { "DeleteFileW", (app_pc)redirect_DeleteFileW },
    { "ReadFile", (app_pc)redirect_ReadFile },
#if 0 /* FIXME i#1089: disabling until we have console support */
    {"WriteFile",                      (app_pc)redirect_WriteFile},
#endif
    { "CreateFileMappingA", (app_pc)redirect_CreateFileMappingA },
    { "CreateFileMappingW", (app_pc)redirect_CreateFileMappingW },
    { "MapViewOfFile", (app_pc)redirect_MapViewOfFile },
    { "MapViewOfFileEx", (app_pc)redirect_MapViewOfFileEx },
    { "UnmapViewOfFile", (app_pc)redirect_UnmapViewOfFile },
    { "FlushViewOfFile", (app_pc)redirect_FlushViewOfFile },
    { "CreatePipe", (app_pc)redirect_CreatePipe },
    { "DeviceIoControl", (app_pc)redirect_DeviceIoControl },
    { "CloseHandle", (app_pc)redirect_CloseHandle },
    { "DuplicateHandle", (app_pc)redirect_DuplicateHandle },
    { "FileTimeToLocalFileTime", (app_pc)redirect_FileTimeToLocalFileTime },
    { "LocalFileTimeToFileTime", (app_pc)redirect_LocalFileTimeToFileTime },
    { "FileTimeToSystemTime", (app_pc)redirect_FileTimeToSystemTime },
    { "SystemTimeToFileTime", (app_pc)redirect_SystemTimeToFileTime },
    { "GetSystemTimeAsFileTime", (app_pc)redirect_GetSystemTimeAsFileTime },
    { "GetFileTime", (app_pc)redirect_GetFileTime },
    { "SetFileTime", (app_pc)redirect_SetFileTime },
    { "FindClose", (app_pc)redirect_FindClose },
    { "FindFirstFileA", (app_pc)redirect_FindFirstFileA },
    { "FindFirstFileW", (app_pc)redirect_FindFirstFileW },
    { "FindNextFileA", (app_pc)redirect_FindNextFileA },
    { "FindNextFileW", (app_pc)redirect_FindNextFileW },
    { "FlushFileBuffers", (app_pc)redirect_FlushFileBuffers },
    { "GetDiskFreeSpaceA", (app_pc)redirect_GetDiskFreeSpaceA },
    { "GetDiskFreeSpaceW", (app_pc)redirect_GetDiskFreeSpaceW },
    { "GetDriveTypeA", (app_pc)redirect_GetDriveTypeA },
    { "GetDriveTypeW", (app_pc)redirect_GetDriveTypeW },
    { "GetFileAttributesA", (app_pc)redirect_GetFileAttributesA },
    { "GetFileAttributesW", (app_pc)redirect_GetFileAttributesW },
    { "GetFileInformationByHandle", (app_pc)redirect_GetFileInformationByHandle },
    { "GetFileSize", (app_pc)redirect_GetFileSize },
    { "GetFileType", (app_pc)redirect_GetFileType },
    /* skipped a few in alpha order, to focus on those invoked by dbghelp */
    { "GetStdHandle", (app_pc)redirect_GetStdHandle },

    /* Synchronization routines */
    { "InitializeCriticalSectionAndSpinCount",
      (app_pc)redirect_InitializeCriticalSectionAndSpinCount },
    { "InitializeCriticalSectionEx", (app_pc)redirect_InitializeCriticalSectionEx },
    { "DeleteCriticalSection", (app_pc)redirect_DeleteCriticalSection },
    { "EnterCriticalSection", (app_pc)redirect_EnterCriticalSection },
    { "LeaveCriticalSection", (app_pc)redirect_LeaveCriticalSection },
    { "InterlockedCompareExchange ", (app_pc)redirect_InterlockedCompareExchange },
    { "InterlockedDecrement", (app_pc)redirect_InterlockedDecrement },
    { "InterlockedExchange", (app_pc)redirect_InterlockedExchange },
    { "InterlockedIncrement", (app_pc)redirect_InterlockedIncrement },
    { "WaitForSingleObject", (app_pc)redirect_WaitForSingleObject },

    /* Miscellaneous routines */
    { "GetLastError", (app_pc)redirect_GetLastError },
    { "SetLastError", (app_pc)redirect_SetLastError },
};
#define REDIRECT_KERNEL32_NUM (sizeof(redirect_kernel32) / sizeof(redirect_kernel32[0]))

void
kernel32_redir_init(void)
{
    uint i;
    kernel32_table = strhash_hash_create(
        GLOBAL_DCONTEXT, hashtable_num_bits(REDIRECT_KERNEL32_NUM * 2),
        80 /* load factor: not perf-critical, plus static */,
        HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
        NULL _IF_DEBUG("kernel32 redirection table"));
    TABLE_RWLOCK(kernel32_table, write, lock);
    for (i = 0; i < REDIRECT_KERNEL32_NUM; i++) {
        strhash_hash_add(GLOBAL_DCONTEXT, kernel32_table, redirect_kernel32[i].name,
                         (void *)redirect_kernel32[i].func);
    }
    TABLE_RWLOCK(kernel32_table, write, unlock);

    kernel32_redir_init_proc();
    kernel32_redir_init_mem();
    kernel32_redir_init_file();
}

void
kernel32_redir_exit(void)
{
    kernel32_redir_exit_file();
    kernel32_redir_exit_mem();
    kernel32_redir_exit_proc();

    strhash_hash_destroy(GLOBAL_DCONTEXT, kernel32_table);
}

void
kernel32_redir_onload(privmod_t *mod)
{
    if (!dynamo_initialized)
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);

    /* Rather than statically linking to real kernel32 we want to invoke
     * routines in the private kernel32 so we look them up here.
     */

    /* We give sub-modules a chance to remove entries from the table (i#1385) */
    TABLE_RWLOCK(kernel32_table, write, lock);
    kernel32_redir_onload_proc(mod, kernel32_table);
    kernel32_redir_onload_lib(mod);
    kernel32_redir_onload_file(mod);
    TABLE_RWLOCK(kernel32_table, write, unlock);

    if (!dynamo_initialized)
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

/* We assume the caller has already ruled out kernel32 calling into kernelbase,
 * which we do not want to redirect.
 */
app_pc
kernel32_redir_lookup(const char *name)
{
    app_pc res;
    TABLE_RWLOCK(kernel32_table, read, lock);
    res = strhash_hash_lookup(GLOBAL_DCONTEXT, kernel32_table, name);
    TABLE_RWLOCK(kernel32_table, read, unlock);
    return res;
}
