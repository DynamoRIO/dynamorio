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

/* kernel32 and kernelbase redirection routines.
 * We initially target the union of the imports of C++ apps, msvcrt,
 * and dbghelp.
 */

#ifndef _KERNEL32_REDIR_H_
#define _KERNEL32_REDIR_H_ 1

#ifdef _WIN32_WINNT
#    error Must include kernel32_redir.h before globals.h
#endif

#define _WIN32_WINNT 0x0600 /* for FILE_INFO_BY_HANDLE_CLASS */
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#undef _WIN32_WINNT
#undef WIN32_LEAN_AND_MEAN

#include "../../globals.h"
#include "../../module_shared.h"

/**********************************************************************
 * support older VS versions
 */

#ifndef __drv_interlocked
#    define __drv_interlocked /* nothing */
#endif

#if _MSC_VER <= 1400 /* VS2005- */
enum {
    HeapEnableTerminationOnCorruption = 1,
};

/* wincon.h */
typedef struct _CONSOLE_READCONSOLE_CONTROL {
    ULONG nLength;
    ULONG nInitialChars;
    ULONG dwCtrlWakeupMask;
    ULONG dwControlKeyState;
} CONSOLE_READCONSOLE_CONTROL, *PCONSOLE_READCONSOLE_CONTROL;

/* winbase.h */
typedef enum _FILE_INFO_BY_HANDLE_CLASS {
    FileBasicInfo,
    FileStandardInfo,
    FileNameInfo,
    FileRenameInfo,
    FileDispositionInfo,
    FileAllocationInfo,
    FileEndOfFileInfo,
    FileStreamInfo,
    FileCompressionInfo,
    FileAttributeTagInfo,
    FileIdBothDirectoryInfo,
    FileIdBothDirectoryRestartInfo,
    FileIoPriorityHintInfo,
    FileRemoteProtocolInfo,
    MaximumFileInfoByHandleClass
} FILE_INFO_BY_HANDLE_CLASS,
    *PFILE_INFO_BY_HANDLE_CLASS;
#endif

#if defined(X64) && !defined(UNWIND_HISTORY_TABLE_SIZE)
#    define UNWIND_HISTORY_TABLE_SIZE 12

typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
    DWORD64 ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
} UNWIND_HISTORY_TABLE_ENTRY, *PUNWIND_HISTORY_TABLE_ENTRY;

typedef struct _UNWIND_HISTORY_TABLE {
    DWORD Count;
    BYTE LocalHint;
    BYTE GlobalHint;
    BYTE Search;
    BYTE Once;
    DWORD64 LowAddress;
    DWORD64 HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;

typedef struct _KNONVOLATILE_CONTEXT_POINTERS {
    union {
        PM128A FloatingContext[16];
        struct {
            PM128A Xmm0;
            PM128A Xmm1;
            PM128A Xmm2;
            PM128A Xmm3;
            PM128A Xmm4;
            PM128A Xmm5;
            PM128A Xmm6;
            PM128A Xmm7;
            PM128A Xmm8;
            PM128A Xmm9;
            PM128A Xmm10;
            PM128A Xmm11;
            PM128A Xmm12;
            PM128A Xmm13;
            PM128A Xmm14;
            PM128A Xmm15;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;

    union {
        PDWORD64 IntegerContext[16];
        struct {
            PDWORD64 Rax;
            PDWORD64 Rcx;
            PDWORD64 Rdx;
            PDWORD64 Rbx;
            PDWORD64 Rsp;
            PDWORD64 Rbp;
            PDWORD64 Rsi;
            PDWORD64 Rdi;
            PDWORD64 R8;
            PDWORD64 R9;
            PDWORD64 R10;
            PDWORD64 R11;
            PDWORD64 R12;
            PDWORD64 R13;
            PDWORD64 R14;
            PDWORD64 R15;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME2;

} KNONVOLATILE_CONTEXT_POINTERS, *PKNONVOLATILE_CONTEXT_POINTERS;
#endif

#ifndef RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO
#    define RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO 0x01000000
#endif

/***************************************************************************/

void
kernel32_redir_init(void);

void
kernel32_redir_exit(void);

void
kernel32_redir_onload(privmod_t *mod);

app_pc
kernel32_redir_lookup(const char *name);

/* Note that we keep the declarations for these redirect_* routines
 * matching the Windows header style, rather than DR's internal style,
 * to make it easier to compare to Windows headers and docs.
 */

/***************************************************************************
 * Processes and threads
 */

void
kernel32_redir_init_proc(void);

void
kernel32_redir_exit_proc(void);

void
kernel32_redir_onload_proc(privmod_t *mod, strhash_table_t *kernel32_table);

DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb);

HANDLE
WINAPI
redirect_GetCurrentProcess(VOID);

DWORD
WINAPI
redirect_GetCurrentProcessId(VOID);

__out HANDLE WINAPI redirect_GetCurrentThread(VOID);

DWORD
WINAPI
redirect_GetCurrentThreadId(VOID);

BOOL WINAPI
redirect_CreateProcessA(__in_opt LPCSTR lpApplicationName,
                        __inout_opt LPSTR lpCommandLine,
                        __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
                        __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
                        __in BOOL bInheritHandles, __in DWORD dwCreationFlags,
                        __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
                        __in LPSTARTUPINFOA lpStartupInfo,
                        __out LPPROCESS_INFORMATION lpProcessInformation);

BOOL WINAPI
redirect_CreateProcessW(__in_opt LPCWSTR lpApplicationName,
                        __inout_opt LPWSTR lpCommandLine,
                        __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
                        __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
                        __in BOOL bInheritHandles, __in DWORD dwCreationFlags,
                        __in_opt LPVOID lpEnvironment,
                        __in_opt LPCWSTR lpCurrentDirectory,
                        __in LPSTARTUPINFOW lpStartupInfo,
                        __out LPPROCESS_INFORMATION lpProcessInformation);

__out HANDLE WINAPI
redirect_CreateThread(__in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
                      __in SIZE_T dwStackSize, __in LPTHREAD_START_ROUTINE lpStartAddress,
                      __in_opt LPVOID lpParameter, __in DWORD dwCreationFlags,
                      __out_opt LPDWORD lpThreadId);

DECLSPEC_NORETURN
VOID WINAPI
redirect_ExitProcess(__in UINT uExitCode);

DECLSPEC_NORETURN
VOID WINAPI
redirect_ExitThread(__in DWORD dwExitCode);

VOID WINAPI
redirect_FatalAppExitA(__in UINT uAction, __in LPCSTR lpMessageText);

DWORD
WINAPI
redirect_FlsAlloc(__in_opt PFLS_CALLBACK_FUNCTION lpCallback);

BOOL WINAPI
redirect_FlsFree(__in DWORD dwFlsIndex);

PVOID
WINAPI
redirect_FlsGetValue(__in DWORD dwFlsIndex);

BOOL WINAPI
redirect_FlsSetValue(__in DWORD dwFlsIndex, __in_opt PVOID lpFlsData);

__out LPSTR WINAPI redirect_GetCommandLineA(VOID);

__out LPWSTR WINAPI redirect_GetCommandLineW(VOID);

BOOL WINAPI
redirect_GetExitCodeProcess(__in HANDLE hProcess, __out LPDWORD lpExitCode);

VOID WINAPI
redirect_GetStartupInfoA(__out LPSTARTUPINFOA lpStartupInfo);

VOID WINAPI
redirect_GetStartupInfoW(__out LPSTARTUPINFOW lpStartupInfo);

BOOL WINAPI
redirect_GetThreadContext(__in HANDLE hThread, __inout LPCONTEXT lpContext);

int WINAPI
redirect_GetThreadPriority(__in HANDLE hThread);

BOOL WINAPI
redirect_GetThreadSelectorEntry(__in HANDLE hThread, __in DWORD dwSelector,
                                __out LPLDT_ENTRY lpSelectorEntry);

BOOL WINAPI
redirect_GetThreadTimes(__in HANDLE hThread, __out LPFILETIME lpCreationTime,
                        __out LPFILETIME lpExitTime, __out LPFILETIME lpKernelTime,
                        __out LPFILETIME lpUserTime);

__out HANDLE WINAPI
redirect_OpenProcess(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle,
                     __in DWORD dwProcessId);

DWORD
WINAPI
redirect_ResumeThread(__in HANDLE hThread);

VOID NTAPI
redirect_RtlCaptureContext(__out PCONTEXT ContextRecord);

DWORD
WINAPI
redirect_SuspendThread(__in HANDLE hThread);

BOOL WINAPI
redirect_TerminateProcess(__in HANDLE hProcess, __in UINT uExitCode);

/* XXX i#1229: if DR watches this syscall we should also redirect it */
BOOL WINAPI
redirect_TerminateJobObject(__in HANDLE hJob, __in UINT uExitCode);

BOOL WINAPI
redirect_TerminateThread(__in HANDLE hThread, __in DWORD dwExitCode);

DWORD
WINAPI
redirect_TlsAlloc(VOID);

BOOL WINAPI
redirect_TlsFree(__in DWORD dwTlsIndex);

LPVOID
WINAPI
redirect_TlsGetValue(__in DWORD dwTlsIndex);

BOOL WINAPI
redirect_TlsSetValue(__in DWORD dwTlsIndex, __in_opt LPVOID lpTlsValue);

/***************************************************************************
 * Libraries
 */

void
kernel32_redir_onload_lib(privmod_t *mod);

FARPROC
WINAPI
redirect_DelayLoadFailureHook(__in LPCSTR pszDllName, __in LPCSTR pszProcName);

BOOL WINAPI
redirect_FreeLibrary(__in HMODULE hLibModule);

DWORD
WINAPI
redirect_GetModuleFileNameA(__in_opt HMODULE hModule,
                            __out_ecount_part(nSize, return +1) LPCH lpFilename,
                            __in DWORD nSize);

DWORD
WINAPI
redirect_GetModuleFileNameW(__in_opt HMODULE hModule,
                            __out_ecount_part(nSize, return +1) LPWCH lpFilename,
                            __in DWORD nSize);

__out HMODULE WINAPI
redirect_GetModuleHandleA(__in_opt LPCSTR lpModuleName);

__out HMODULE WINAPI
redirect_GetModuleHandleW(__in_opt LPCWSTR lpModuleName);

FARPROC
WINAPI
redirect_GetProcAddress(__in HMODULE hModule, __in LPCSTR lpProcName);

__out HMODULE WINAPI
redirect_LoadLibraryA(__in LPCSTR lpLibFileName);

__out HMODULE WINAPI
redirect_LoadLibraryW(__in LPCWSTR lpLibFileName);

__out HMODULE WINAPI
redirect_LoadLibraryExA(__in LPCSTR lpLibFileName, __reserved HANDLE hFile,
                        __in DWORD dwFlags);

__out HMODULE WINAPI
redirect_LoadLibraryExW(__in LPCWSTR lpLibFileName, __reserved HANDLE hFile,
                        __in DWORD dwFlags);

/***************************************************************************
 * Memory
 */

void
kernel32_redir_init_mem(void);

void
kernel32_redir_exit_mem(void);

PVOID
WINAPI
redirect_DecodePointer(__in_opt PVOID Ptr);

PVOID
WINAPI
redirect_EncodePointer(__in_opt PVOID Ptr);

HANDLE
WINAPI
redirect_GetProcessHeap(VOID);

__bcount(dwBytes) LPVOID WINAPI
    redirect_HeapAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __in SIZE_T dwBytes);

SIZE_T
WINAPI
redirect_HeapCompact(__in HANDLE hHeap, __in DWORD dwFlags);

HANDLE
WINAPI
redirect_HeapCreate(__in DWORD flOptions, __in SIZE_T dwInitialSize,
                    __in SIZE_T dwMaximumSize);

BOOL WINAPI
redirect_HeapDestroy(__in HANDLE hHeap);

BOOL WINAPI
redirect_HeapFree(__inout HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem);

__bcount(dwBytes) LPVOID WINAPI
    redirect_HeapReAlloc(__inout HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem,
                         __in SIZE_T dwBytes);

BOOL WINAPI
redirect_HeapSetInformation(__in_opt HANDLE HeapHandle,
                            __in HEAP_INFORMATION_CLASS HeapInformationClass,
                            __in_bcount_opt(HeapInformationLength) PVOID HeapInformation,
                            __in SIZE_T HeapInformationLength);

SIZE_T
WINAPI
redirect_HeapSize(__in HANDLE hHeap, __in DWORD dwFlags, __in LPCVOID lpMem);

BOOL WINAPI
redirect_HeapValidate(__in HANDLE hHeap, __in DWORD dwFlags, __in_opt LPCVOID lpMem);

BOOL WINAPI
redirect_HeapWalk(__in HANDLE hHeap, __inout LPPROCESS_HEAP_ENTRY lpEntry);

BOOL WINAPI
redirect_IsBadReadPtr(__in_opt CONST VOID *lp, __in UINT_PTR ucb);

HLOCAL
WINAPI
redirect_LocalAlloc(__in UINT uFlags, __in SIZE_T uBytes);

HLOCAL
WINAPI
redirect_LocalFree(__deref HLOCAL hMem);

/* These next 6 Local* routines are not part of the set we're directly
 * targeting, but we have to provide a complete Local* replacement.
 */
HLOCAL
WINAPI
redirect_LocalReAlloc(__in HLOCAL hMem, __in SIZE_T uBytes, __in UINT uFlags);

LPVOID
WINAPI
redirect_LocalLock(__in HLOCAL hMem);

HLOCAL
WINAPI
redirect_LocalHandle(__in LPCVOID pMem);

BOOL WINAPI
redirect_LocalUnlock(__in HLOCAL hMem);

SIZE_T
WINAPI
redirect_LocalSize(__in HLOCAL hMem);

UINT WINAPI
redirect_LocalFlags(__in HLOCAL hMem);

BOOL WINAPI
redirect_ReadProcessMemory(__in HANDLE hProcess, __in LPCVOID lpBaseAddress,
                           __out_bcount_part(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
                           __in SIZE_T nSize, __out_opt SIZE_T *lpNumberOfBytesRead);

__bcount(dwSize) LPVOID WINAPI
    redirect_VirtualAlloc(__in_opt LPVOID lpAddress, __in SIZE_T dwSize,
                          __in DWORD flAllocationType, __in DWORD flProtect);

BOOL WINAPI
redirect_VirtualFree(__in LPVOID lpAddress, __in SIZE_T dwSize, __in DWORD dwFreeType);

BOOL WINAPI
redirect_VirtualProtect(__in LPVOID lpAddress, __in SIZE_T dwSize,
                        __in DWORD flNewProtect, __out PDWORD lpflOldProtect);

SIZE_T
WINAPI
redirect_VirtualQuery(__in_opt LPCVOID lpAddress,
                      __out_bcount_part(dwLength, return )
                          PMEMORY_BASIC_INFORMATION lpBuffer,
                      __in SIZE_T dwLength);

SIZE_T
WINAPI
redirect_VirtualQueryEx(__in HANDLE hProcess, __in_opt LPCVOID lpAddress,
                        __out_bcount_part(dwLength, return )
                            PMEMORY_BASIC_INFORMATION lpBuffer,
                        __in SIZE_T dwLength);

/***************************************************************************
 * Files
 */

void
kernel32_redir_init_file(void);

void
kernel32_redir_exit_file(void);

void
kernel32_redir_onload_file(privmod_t *mod);

BOOL WINAPI
redirect_CreateDirectoryA(__in LPCSTR lpPathName,
                          __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes);

BOOL WINAPI
redirect_CreateDirectoryW(__in LPCWSTR lpPathName,
                          __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes);

BOOL WINAPI
redirect_RemoveDirectoryA(__in LPCSTR lpPathName);

BOOL WINAPI
redirect_RemoveDirectoryW(__in LPCWSTR lpPathName);

DWORD
WINAPI
redirect_GetCurrentDirectoryA(__in DWORD nBufferLength,
                              __out_ecount_part_opt(nBufferLength, return +1)
                                  LPSTR lpBuffer);

DWORD
WINAPI
redirect_GetCurrentDirectoryW(__in DWORD nBufferLength,
                              __out_ecount_part_opt(nBufferLength, return +1)
                                  LPWSTR lpBuffer);

BOOL WINAPI
redirect_SetCurrentDirectoryA(__in LPCSTR lpPathName);

BOOL WINAPI
redirect_SetCurrentDirectoryW(__in LPCWSTR lpPathName);

HANDLE
WINAPI
redirect_CreateFileA(__in LPCSTR lpFileName, __in DWORD dwDesiredAccess,
                     __in DWORD dwShareMode,
                     __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     __in DWORD dwCreationDisposition, __in DWORD dwFlagsAndAttributes,
                     __in_opt HANDLE hTemplateFile);

__out HANDLE WINAPI
redirect_CreateFileW(__in LPCWSTR lpFileName, __in DWORD dwDesiredAccess,
                     __in DWORD dwShareMode,
                     __in_opt LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     __in DWORD dwCreationDisposition, __in DWORD dwFlagsAndAttributes,
                     __in_opt HANDLE hTemplateFile);

BOOL WINAPI
redirect_DeleteFileA(__in LPCSTR lpFileName);

BOOL WINAPI
redirect_DeleteFileW(__in LPCWSTR lpFileName);

BOOL WINAPI
redirect_ReadFile(__in HANDLE hFile,
                  __out_bcount_part(nNumberOfBytesToRead, *lpNumberOfBytesRead)
                      LPVOID lpBuffer,
                  __in DWORD nNumberOfBytesToRead, __out_opt LPDWORD lpNumberOfBytesRead,
                  __inout_opt LPOVERLAPPED lpOverlapped);

BOOL WINAPI
redirect_WriteFile(__in HANDLE hFile, __in_bcount(nNumberOfBytesToWrite) LPCVOID lpBuffer,
                   __in DWORD nNumberOfBytesToWrite,
                   __out_opt LPDWORD lpNumberOfBytesWritten,
                   __inout_opt LPOVERLAPPED lpOverlapped);

HANDLE
WINAPI
redirect_CreateFileMappingA(__in HANDLE hFile,
                            __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                            __in DWORD flProtect, __in DWORD dwMaximumSizeHigh,
                            __in DWORD dwMaximumSizeLow, __in_opt LPCSTR lpName);

HANDLE
WINAPI
redirect_CreateFileMappingW(__in HANDLE hFile,
                            __in_opt LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                            __in DWORD flProtect, __in DWORD dwMaximumSizeHigh,
                            __in DWORD dwMaximumSizeLow, __in_opt LPCWSTR lpName);

LPVOID
WINAPI
redirect_MapViewOfFile(__in HANDLE hFileMappingObject, __in DWORD dwDesiredAccess,
                       __in DWORD dwFileOffsetHigh, __in DWORD dwFileOffsetLow,
                       __in SIZE_T dwNumberOfBytesToMap);

LPVOID
WINAPI
redirect_MapViewOfFileEx(__in HANDLE hFileMappingObject, __in DWORD dwDesiredAccess,
                         __in DWORD dwFileOffsetHigh, __in DWORD dwFileOffsetLow,
                         __in SIZE_T dwNumberOfBytesToMap, __in_opt LPVOID lpBaseAddress);

BOOL WINAPI
redirect_UnmapViewOfFile(__in LPCVOID lpBaseAddress);

BOOL WINAPI
redirect_FlushViewOfFile(__in LPCVOID lpBaseAddress, __in SIZE_T dwNumberOfBytesToFlush);

BOOL WINAPI
redirect_CreatePipe(__out_ecount_full(1) PHANDLE hReadPipe,
                    __out_ecount_full(1) PHANDLE hWritePipe,
                    __in_opt LPSECURITY_ATTRIBUTES lpPipeAttributes, __in DWORD nSize);

BOOL WINAPI
redirect_DeviceIoControl(__in HANDLE hDevice, __in DWORD dwIoControlCode,
                         __in_bcount_opt(nInBufferSize) LPVOID lpInBuffer,
                         __in DWORD nInBufferSize,
                         __out_bcount_part_opt(nOutBufferSize, *lpBytesReturned)
                             LPVOID lpOutBuffer,
                         __in DWORD nOutBufferSize, __out_opt LPDWORD lpBytesReturned,
                         __inout_opt LPOVERLAPPED lpOverlapped);

BOOL WINAPI
redirect_CloseHandle(__in HANDLE hObject);

BOOL WINAPI
redirect_DuplicateHandle(__in HANDLE hSourceProcessHandle, __in HANDLE hSourceHandle,
                         __in HANDLE hTargetProcessHandle,
                         __deref_out LPHANDLE lpTargetHandle, __in DWORD dwDesiredAccess,
                         __in BOOL bInheritHandle, __in DWORD dwOptions);

BOOL WINAPI
redirect_FileTimeToLocalFileTime(__in CONST FILETIME *lpFileTime,
                                 __out LPFILETIME lpLocalFileTime);

BOOL WINAPI
redirect_LocalFileTimeToFileTime(__in CONST FILETIME *lpLocalFileTime,
                                 __out LPFILETIME lpFileTime);

BOOL WINAPI
redirect_FileTimeToSystemTime(__in CONST FILETIME *lpFileTime,
                              __out LPSYSTEMTIME lpSystemTime);

BOOL WINAPI
redirect_SystemTimeToFileTime(__in CONST SYSTEMTIME *lpSystemTime,
                              __out LPFILETIME lpFileTime);

VOID WINAPI
redirect_GetSystemTimeAsFileTime(__out LPFILETIME lpSystemTimeAsFileTime);

BOOL WINAPI
redirect_GetFileTime(__in HANDLE hFile, __out_opt LPFILETIME lpCreationTime,
                     __out_opt LPFILETIME lpLastAccessTime,
                     __out_opt LPFILETIME lpLastWriteTime);

BOOL WINAPI
redirect_SetFileTime(__in HANDLE hFile, __in_opt CONST FILETIME *lpCreationTime,
                     __in_opt CONST FILETIME *lpLastAccessTime,
                     __in_opt CONST FILETIME *lpLastWriteTime);

BOOL WINAPI
redirect_FindClose(__inout HANDLE hFindFile);

HANDLE
WINAPI
redirect_FindFirstFileA(__in LPCSTR lpFileName, __out LPWIN32_FIND_DATAA lpFindFileData);

HANDLE
WINAPI
redirect_FindFirstFileW(__in LPCWSTR lpFileName, __out LPWIN32_FIND_DATAW lpFindFileData);

BOOL WINAPI
redirect_FindNextFileA(__in HANDLE hFindFile, __out LPWIN32_FIND_DATAA lpFindFileData);

BOOL WINAPI
redirect_FindNextFileW(__in HANDLE hFindFile, __out LPWIN32_FIND_DATAW lpFindFileData);

BOOL WINAPI
redirect_FlushFileBuffers(__in HANDLE hFile);

BOOL WINAPI
redirect_GetDiskFreeSpaceA(__in_opt LPCSTR lpRootPathName,
                           __out_opt LPDWORD lpSectorsPerCluster,
                           __out_opt LPDWORD lpBytesPerSector,
                           __out_opt LPDWORD lpNumberOfFreeClusters,
                           __out_opt LPDWORD lpTotalNumberOfClusters);

BOOL WINAPI
redirect_GetDiskFreeSpaceW(__in_opt LPCWSTR lpRootPathName,
                           __out_opt LPDWORD lpSectorsPerCluster,
                           __out_opt LPDWORD lpBytesPerSector,
                           __out_opt LPDWORD lpNumberOfFreeClusters,
                           __out_opt LPDWORD lpTotalNumberOfClusters);

UINT WINAPI
redirect_GetDriveTypeA(__in_opt LPCSTR lpRootPathName);

UINT WINAPI
redirect_GetDriveTypeW(__in_opt LPCWSTR lpRootPathName);

DWORD
WINAPI
redirect_GetFileAttributesA(__in LPCSTR lpFileName);

DWORD
WINAPI
redirect_GetFileAttributesW(__in LPCWSTR lpFileName);

BOOL WINAPI
redirect_GetFileInformationByHandle(__in HANDLE hFile,
                                    __out LPBY_HANDLE_FILE_INFORMATION lpFileInformation);

DWORD
WINAPI
redirect_GetFileSize(__in HANDLE hFile, __out_opt LPDWORD lpFileSizeHigh);

DWORD
WINAPI
redirect_GetFileType(__in HANDLE hFile);

/* XXX: when implemented, use in redirect_SetCurrentDirectoryW() */
DWORD
WINAPI
redirect_GetFullPathNameA(__in LPCSTR lpFileName, __in DWORD nBufferLength,
                          __out_ecount_part_opt(nBufferLength, return +1) LPSTR lpBuffer,
                          __deref_opt_out LPSTR *lpFilePart);

DWORD
WINAPI
redirect_GetFullPathNameW(__in LPCWSTR lpFileName, __in DWORD nBufferLength,
                          __out_ecount_part_opt(nBufferLength, return +1) LPWSTR lpBuffer,
                          __deref_opt_out LPWSTR *lpFilePart);

HANDLE
WINAPI
redirect_GetStdHandle(__in DWORD nStdHandle);

DWORD
WINAPI
redirect_GetLogicalDrives(VOID);

BOOL WINAPI
redirect_LockFile(__in HANDLE hFile, __in DWORD dwFileOffsetLow,
                  __in DWORD dwFileOffsetHigh, __in DWORD nNumberOfBytesToLockLow,
                  __in DWORD nNumberOfBytesToLockHigh);

BOOL WINAPI
redirect_PeekConsoleInputA(IN HANDLE hConsoleInput, OUT PINPUT_RECORD lpBuffer,
                           IN DWORD nLength, OUT LPDWORD lpNumberOfEventsRead);

BOOL WINAPI
redirect_PeekNamedPipe(__in HANDLE hNamedPipe,
                       __out_bcount_part_opt(nBufferSize, *lpBytesRead) LPVOID lpBuffer,
                       __in DWORD nBufferSize, __out_opt LPDWORD lpBytesRead,
                       __out_opt LPDWORD lpTotalBytesAvail,
                       __out_opt LPDWORD lpBytesLeftThisMessage);

BOOL WINAPI
redirect_ReadConsoleInputA(IN HANDLE hConsoleInput, OUT PINPUT_RECORD lpBuffer,
                           IN DWORD nLength, OUT LPDWORD lpNumberOfEventsRead);

BOOL WINAPI
redirect_ReadConsoleInputW(IN HANDLE hConsoleInput, OUT PINPUT_RECORD lpBuffer,
                           IN DWORD nLength, OUT LPDWORD lpNumberOfEventsRead);

BOOL WINAPI
redirect_ReadConsoleW(__in HANDLE hConsoleInput,
                      __out_ecount_part(nNumberOfCharsToRead, *lpNumberOfCharsRead)
                          LPVOID lpBuffer,
                      __in DWORD nNumberOfCharsToRead, __out LPDWORD lpNumberOfCharsRead,
                      __in_opt PCONSOLE_READCONSOLE_CONTROL pInputControl);

BOOL WINAPI
redirect_SetEndOfFile(__in HANDLE hFile);

BOOL WINAPI
redirect_SetFileAttributesA(__in LPCSTR lpFileName, __in DWORD dwFileAttributes);

BOOL WINAPI
redirect_SetFileAttributesW(__in LPCWSTR lpFileName, __in DWORD dwFileAttributes);

BOOL WINAPI
redirect_SetFileInformationByHandle(__in HANDLE hFile,
                                    __in FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
                                    __in_bcount(dwBufferSize) LPVOID lpFileInformation,
                                    __in DWORD dwBufferSize);

/* XXX: once we have redirect_SetFilePointer, replace SetFilePointer in unit test */
DWORD
WINAPI
redirect_SetFilePointer(__in HANDLE hFile, __in LONG lDistanceToMove,
                        __in_opt PLONG lpDistanceToMoveHigh, __in DWORD dwMoveMethod);

BOOL WINAPI
redirect_SetStdHandle(__in DWORD nStdHandle, __in HANDLE hHandle);

BOOL WINAPI
redirect_UnlockFile(__in HANDLE hFile, __in DWORD dwFileOffsetLow,
                    __in DWORD dwFileOffsetHigh, __in DWORD nNumberOfBytesToUnlockLow,
                    __in DWORD nNumberOfBytesToUnlockHigh);

BOOL WINAPI
redirect_WriteConsoleA(IN HANDLE hConsoleOutput, IN CONST VOID *lpBuffer,
                       IN DWORD nNumberOfCharsToWrite, OUT LPDWORD lpNumberOfCharsWritten,
                       IN LPVOID lpReserved);

BOOL WINAPI
redirect_WriteConsoleW(IN HANDLE hConsoleOutput, IN CONST VOID *lpBuffer,
                       IN DWORD nNumberOfCharsToWrite, OUT LPDWORD lpNumberOfCharsWritten,
                       IN LPVOID lpReserved);

/***************************************************************************
 * Synchronization
 */

VOID WINAPI
redirect_InitializeCriticalSection(__out LPCRITICAL_SECTION lpCriticalSection);

BOOL WINAPI
redirect_InitializeCriticalSectionAndSpinCount(__out LPCRITICAL_SECTION lpCriticalSection,
                                               __in DWORD dwSpinCount);

BOOL WINAPI
redirect_InitializeCriticalSectionEx(__out LPCRITICAL_SECTION lpCriticalSection,
                                     __in DWORD dwSpinCount, __in DWORD Flags);

VOID WINAPI
redirect_DeleteCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection);

VOID WINAPI
redirect_EnterCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection);

VOID WINAPI
redirect_LeaveCriticalSection(__inout LPCRITICAL_SECTION lpCriticalSection);

LONG WINAPI
redirect_InterlockedCompareExchange(__inout __drv_interlocked LONG volatile *Destination,
                                    __in LONG ExChange, __in LONG Comperand);

LONG WINAPI
redirect_InterlockedDecrement(__inout __drv_interlocked LONG volatile *Addend);

LONG WINAPI
redirect_InterlockedExchange(__inout __drv_interlocked LONG volatile *Target,
                             __in LONG Value);

LONG WINAPI
redirect_InterlockedIncrement(__inout __drv_interlocked LONG volatile *Addend);

DWORD
WINAPI
redirect_WaitForSingleObject(__in HANDLE hHandle, __in DWORD dwMilliseconds);

/***************************************************************************
 * Miscellaneous
 */

DWORD
WINAPI
redirect_GetLastError(VOID);

VOID WINAPI
redirect_SetLastError(__in DWORD dwErrCode);

BOOL WINAPI
redirect_Beep(__in DWORD dwFreq, __in DWORD dwDuration);

int WINAPI
redirect_CompareStringA(__in LCID Locale, __in DWORD dwCmpFlags, __in LPCSTR lpString1,
                        __in int cchCount1, __in LPCSTR lpString2, __in int cchCount2);

int WINAPI
redirect_CompareStringW(__in LCID Locale, __in DWORD dwCmpFlags, __in LPCWSTR lpString1,
                        __in int cchCount1, __in LPCWSTR lpString2, __in int cchCount2);

VOID WINAPI redirect_DebugBreak(VOID);

BOOL WINAPI
redirect_EnumSystemLocalesA(__in LOCALE_ENUMPROCA lpLocaleEnumProc, __in DWORD dwFlags);

DWORD
WINAPI
redirect_FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                        DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize,
                        va_list *Arguments);

BOOL WINAPI
redirect_FreeEnvironmentStringsA(__in __nullnullterminated LPCH);

BOOL WINAPI
redirect_FreeEnvironmentStringsW(__in __nullnullterminated LPWCH);

UINT WINAPI
redirect_GetACP(void);

UINT WINAPI redirect_GetConsoleCP(VOID);

BOOL WINAPI
redirect_SetConsoleCP(IN UINT wCodePageID);

BOOL WINAPI
redirect_GetConsoleMode(IN HANDLE hConsoleHandle, OUT LPDWORD lpMode);

UINT WINAPI redirect_GetConsoleOutputCP(VOID);

BOOL WINAPI
redirect_GetCPInfo(__in UINT CodePage, __out LPCPINFO lpCPInfo);

int WINAPI
redirect_GetDateFormatA(__in LCID Locale, __in DWORD dwFlags,
                        __in_opt CONST SYSTEMTIME *lpDate, __in_opt LPCSTR lpFormat,
                        __out_ecount_opt(cchDate) LPSTR lpDateStr, __in int cchDate);

int WINAPI
redirect_GetDateFormatW(__in LCID Locale, __in DWORD dwFlags,
                        __in_opt CONST SYSTEMTIME *lpDate, __in_opt LPCWSTR lpFormat,
                        __out_ecount_opt(cchDate) LPWSTR lpDateStr, __in int cchDate);

__nullnullterminated LPCH WINAPI redirect_GetEnvironmentStrings(VOID);

__out __nullnullterminated LPWCH WINAPI redirect_GetEnvironmentStringsW(VOID);

int WINAPI
redirect_GetLocaleInfoA(__in LCID Locale, __in LCTYPE LCType,
                        __out_ecount_opt(cchData) LPSTR lpLCData, __in int cchData);

int WINAPI
redirect_GetLocaleInfoW(__in LCID Locale, __in LCTYPE LCType,
                        __out_ecount_opt(cchData) LPWSTR lpLCData, __in int cchData);

VOID WINAPI
redirect_GetLocalTime(__out LPSYSTEMTIME lpSystemTime);

BOOL WINAPI
redirect_GetNumberOfConsoleInputEvents(IN HANDLE hConsoleInput,
                                       OUT LPDWORD lpNumberOfEvents);

UINT WINAPI
redirect_GetOEMCP(void);

DWORD
WINAPI
redirect_GetPriorityClass(__in HANDLE hProcess);

BOOL WINAPI
redirect_GetStringTypeA(__in LCID Locale, __in DWORD dwInfoType, __in LPCSTR lpSrcStr,
                        __in int cchSrc, __out LPWORD lpCharType);

BOOL WINAPI
redirect_GetStringTypeW(__in DWORD dwInfoType, __in LPCWSTR lpSrcStr, __in int cchSrc,
                        __out LPWORD lpCharType);

VOID WINAPI
redirect_GetSystemInfo(__out LPSYSTEM_INFO lpSystemInfo);

int WINAPI
redirect_GetTimeFormatA(__in LCID Locale, __in DWORD dwFlags,
                        __in_opt CONST SYSTEMTIME *lpTime, __in_opt LPCSTR lpFormat,
                        __out_ecount_opt(cchTime) LPSTR lpTimeStr, __in int cchTime);

int WINAPI
redirect_GetTimeFormatW(__in LCID Locale, __in DWORD dwFlags,
                        __in_opt CONST SYSTEMTIME *lpTime, __in_opt LPCWSTR lpFormat,
                        __out_ecount_opt(cchTime) LPWSTR lpTimeStr, __in int cchTime);

DWORD
WINAPI
redirect_GetTimeZoneInformation(__out LPTIME_ZONE_INFORMATION lpTimeZoneInformation);

LCID WINAPI
redirect_GetUserDefaultLCID(void);

DWORD
WINAPI
redirect_GetVersion(VOID);

BOOL WINAPI
redirect_GetVersionExA(__inout LPOSVERSIONINFOA lpVersionInformation);

BOOL WINAPI
redirect_GetVersionExW(__inout LPOSVERSIONINFOW lpVersionInformation);

BOOL WINAPI
redirect_IsDBCSLeadByte(__in BYTE TestChar);

BOOL WINAPI redirect_IsDebuggerPresent(VOID);

BOOL WINAPI
redirect_IsValidCodePage(__in UINT CodePage);

BOOL WINAPI
redirect_IsValidLocale(__in LCID Locale, __in DWORD dwFlags);

int WINAPI
redirect_LCMapStringA(__in LCID Locale, __in DWORD dwMapFlags, __in LPCSTR lpSrcStr,
                      __in int cchSrc, __out_ecount_opt(cchDest) LPSTR lpDestStr,
                      __in int cchDest);

int WINAPI
redirect_LCMapStringW(__in LCID Locale, __in DWORD dwMapFlags, __in LPCWSTR lpSrcStr,
                      __in int cchSrc, __out_ecount_opt(cchDest) LPWSTR lpDestStr,
                      __in int cchDest);

int WINAPI
redirect_lstrlenA(__in LPCSTR lpString);

int WINAPI
redirect_MultiByteToWideChar(__in UINT CodePage, __in DWORD dwFlags,
                             __in LPCSTR lpMultiByteStr, __in int cbMultiByte,
                             __out_ecount_opt(cchWideChar) LPWSTR lpWideCharStr,
                             __in int cchWideChar);

VOID WINAPI
redirect_OutputDebugStringA(__in LPCSTR lpOutputString);

VOID WINAPI
redirect_OutputDebugStringW(__in LPCWSTR lpOutputString);

BOOL WINAPI
redirect_QueryPerformanceCounter(__out LARGE_INTEGER *lpPerformanceCount);

VOID WINAPI
redirect_RaiseException(__in DWORD dwExceptionCode, __in DWORD dwExceptionFlags,
                        __in DWORD nNumberOfArguments,
                        __in_ecount_opt(nNumberOfArguments) CONST ULONG_PTR *lpArguments);

VOID NTAPI
redirect_RtlGetNtVersionNumbers(LPDWORD major, LPDWORD minor, LPDWORD build);

#ifdef X64
PRUNTIME_FUNCTION
NTAPI
redirect_RtlLookupFunctionEntry(__in DWORD64 ControlPc, __out PDWORD64 ImageBase,
                                __inout_opt PUNWIND_HISTORY_TABLE HistoryTable);
#endif

VOID NTAPI
redirect_RtlUnwind(__in_opt PVOID TargetFrame, __in_opt PVOID TargetIp,
                   __in_opt PEXCEPTION_RECORD ExceptionRecord, __in PVOID ReturnValue);

#ifdef X64
VOID NTAPI
redirect_RtlUnwindEx(__in_opt PVOID TargetFrame, __in_opt PVOID TargetIp,
                     __in_opt PEXCEPTION_RECORD ExceptionRecord, __in PVOID ReturnValue,
                     __in PCONTEXT ContextRecord,
                     __in_opt PUNWIND_HISTORY_TABLE HistoryTable);

ULONGLONG
NTAPI
redirect_RtlVirtualUnwind(__in DWORD HandlerType, __in DWORD64 ImageBase,
                          __in DWORD64 ControlPc, __in PRUNTIME_FUNCTION FunctionEntry,
                          __inout PCONTEXT ContextRecord, __out PVOID *HandlerData,
                          __out PDWORD64 EstablisherFrame,
                          __inout_opt PKNONVOLATILE_CONTEXT_POINTERS ContextPointers);
#endif

BOOL WINAPI
redirect_SetConsoleCtrlHandler(IN PHANDLER_ROUTINE HandlerRoutine, IN BOOL Add);

BOOL WINAPI
redirect_SetConsoleMode(IN HANDLE hConsoleHandle, IN DWORD dwMode);

BOOL WINAPI
redirect_SetEnvironmentVariableA(__in LPCSTR lpName, __in_opt LPCSTR lpValue);

BOOL WINAPI
redirect_SetEnvironmentVariableW(__in LPCWSTR lpName, __in_opt LPCWSTR lpValue);

UINT WINAPI
redirect_SetErrorMode(__in UINT uMode);

UINT WINAPI
redirect_SetHandleCount(__in UINT uNumber);

BOOL WINAPI
redirect_SetLocalTime(__in CONST SYSTEMTIME *lpSystemTime);

BOOL WINAPI
redirect_SetThreadStackGuarantee(__inout PULONG StackSizeInBytes);

LPTOP_LEVEL_EXCEPTION_FILTER
WINAPI
redirect_SetUnhandledExceptionFilter(
    __in LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

VOID WINAPI
redirect_Sleep(__in DWORD dwMilliseconds);

LONG WINAPI
redirect_UnhandledExceptionFilter(__in struct _EXCEPTION_POINTERS *ExceptionInfo);

int WINAPI
redirect_WideCharToMultiByte(__in UINT CodePage, __in DWORD dwFlags,
                             __in_opt LPCWSTR lpWideCharStr, __in int cchWideChar,
                             __out_bcount_opt(cbMultiByte) LPSTR lpMultiByteStr,
                             __in int cbMultiByte, __in_opt LPCSTR lpDefaultChar,
                             __out_opt LPBOOL lpUsedDefaultChar);

#endif /* _KERNEL32_REDIR_H_ */
