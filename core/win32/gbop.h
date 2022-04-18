/* **********************************************************
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2006-2007 Determina Corp. */

/*
 * gbop.h - GBOP hook definitions
 *
 * Plan to implement only for win32.
 *
 */

#ifndef GBOP_H
#define GBOP_H

/* Attempting to keep GBOP hook policies more flexbile, though the
 * right place will be in a hotpatch policy file.
 */

/* Support for gbop_include_set */
/* GBOP_DEFINE_HOOK and GBOP_DEFINE_HOOK_MODULE should be defined as
 * appropriate by users of hook definitions, note that module name for
 * each list is specified by the user since some may need different
 * variants of the name.
 */

/* note that we currently do only shallow GBOP checking therefore we
 * need all entry points even if they all eventually call a common
 * exported implementation that we also hook */

/* Creating processes, modifying files, or creating new network
 * connections are the primary behaviors that need to be watched.
 * More generic shellcodes will be stopped while trying to setup their
 * plumbing when the more generic ones use LoadLibrary/GetProcAddress.
 * To minimize overhead due to our checks they shouldn't be added to
 * hot routines, and should be best positioned to include the
 * initializing call of some facility, but not the more common methods:
 * e.g. hook socket() but not necessarily recv().
 */

/* KERNEL32.dll base */
/* likely targets for generic shellcode */
/* mostly in alphabetic order */
#define GBOP_DEFINE_KERNEL32_BASE_HOOKS(module) /* "KERNEL32.dll" */ \
    GBOP_DEFINE_HOOK(module, CreateFileA)                            \
    GBOP_DEFINE_HOOK(module, CreateFileW)                            \
    GBOP_DEFINE_HOOK(module, CreateProcessA)                         \
    GBOP_DEFINE_HOOK(module, CreateProcessInternalA)                 \
    GBOP_DEFINE_HOOK(module, CreateProcessInternalW)                 \
    GBOP_DEFINE_HOOK(module, CreateProcessW)                         \
    /* kernel32!CreateProcessInternalWSecure is a RET on XP SP2 */   \
    GBOP_DEFINE_HOOK(module, CreateRemoteThread)                     \
    GBOP_DEFINE_HOOK(module, CreateThread)                           \
    GBOP_DEFINE_HOOK(module, FreeLibrary)                            \
                                                                     \
    GBOP_DEFINE_HOOK(module, GetModuleHandleA)                       \
    GBOP_DEFINE_HOOK(module, GetModuleHandleW)                       \
    GBOP_DEFINE_HOOK(module, GetModuleHandleExW)                     \
    GBOP_DEFINE_HOOK(module, GetModuleHandleExA)                     \
                                                                     \
    GBOP_DEFINE_HOOK(module, GetProcAddress)                         \
    GBOP_DEFINE_HOOK(module, LoadLibraryA)                           \
    GBOP_DEFINE_HOOK(module, LoadLibraryExA)                         \
    GBOP_DEFINE_HOOK(module, LoadLibraryExW)                         \
    GBOP_DEFINE_HOOK(module, LoadLibraryW)                           \
    GBOP_DEFINE_HOOK(module, LoadModule)                             \
    /* wrapper around CreateProcess */                               \
                                                                     \
    GBOP_DEFINE_HOOK(module, OpenProcess)                            \
    GBOP_DEFINE_HOOK(module, VirtualAlloc)                           \
    GBOP_DEFINE_HOOK(module, VirtualAllocEx)                         \
    GBOP_DEFINE_HOOK(module, VirtualProtect)                         \
    GBOP_DEFINE_HOOK(module, VirtualProtectEx)                       \
    GBOP_DEFINE_HOOK(module, WinExec)                                \
    /* wrapper around CreateProcess */                               \
                                                                     \
    GBOP_DEFINE_HOOK(module, WriteProcessMemory)

/* KERNEL32.dll complete set */
/* adding the less likely to be used by a generic exploit for
 * completeness, yet short of KERNEL32!*
 */
#define GBOP_DEFINE_KERNEL32_MORE_HOOKS(module) /* "KERNEL32.dll" */ \
    GBOP_DEFINE_HOOK(module, CopyFileA)                              \
    GBOP_DEFINE_HOOK(module, CopyFileW)                              \
    GBOP_DEFINE_HOOK(module, CopyFileExA)                            \
    GBOP_DEFINE_HOOK(module, CopyFileExW)                            \
    GBOP_DEFINE_HOOK(module, CreatePipe)                             \
    GBOP_DEFINE_HOOK(module, CreateNamedPipeA)                       \
    GBOP_DEFINE_HOOK(module, CreateNamedPipeW)                       \
                                                                     \
    GBOP_DEFINE_HOOK(module, CreateDirectoryA) /* low risk */        \
    GBOP_DEFINE_HOOK(module, CreateDirectoryExA)                     \
    GBOP_DEFINE_HOOK(module, CreateDirectoryExW)                     \
    GBOP_DEFINE_HOOK(module, CreateDirectoryW)                       \
                                                                     \
    GBOP_DEFINE_HOOK(module, DeleteFileA)                            \
    GBOP_DEFINE_HOOK(module, DeleteFileW)                            \
                                                                     \
    GBOP_DEFINE_HOOK(module, ExitProcess)                            \
                                                                     \
    GBOP_DEFINE_HOOK(module, GetStartupInfoA)                        \
    GBOP_DEFINE_HOOK(module, GetStartupInfoW)                        \
                                                                     \
    GBOP_DEFINE_HOOK(module, LZCreateFileW)                          \
    /* may be used to overwrite a file */                            \
    /* note there is no LZCreateFileA version */                     \
    /* skipping LZOpenFile[AW] which open only compressed files */   \
                                                                     \
    GBOP_DEFINE_HOOK(module, MoveFileA)                              \
    GBOP_DEFINE_HOOK(module, MoveFileExA)                            \
    GBOP_DEFINE_HOOK(module, MoveFileExW)                            \
    GBOP_DEFINE_HOOK(module, MoveFileW)                              \
    GBOP_DEFINE_HOOK(module, MoveFileWithProgressA)                  \
    GBOP_DEFINE_HOOK(module, MoveFileWithProgressW)                  \
                                                                     \
    GBOP_DEFINE_HOOK(module, OpenFile)                               \
    GBOP_DEFINE_HOOK(module, OpenDataFile)                           \
                                                                     \
    GBOP_DEFINE_HOOK(module, PeekNamedPipe)                          \
    GBOP_DEFINE_HOOK(module, PrivCopyFileExW)                        \
                                                                     \
    /* skipping ReadFile, though hooked by others */                 \
    GBOP_DEFINE_HOOK(module, ReplaceFileA)                           \
    GBOP_DEFINE_HOOK(module, ReplaceFileW)                           \
    /* skipping kernel32!RemoveDirectoryW on an empty directory */   \
                                                                     \
    GBOP_DEFINE_HOOK(module, SetEndOfFile)                           \
    /* interesting if a handle is open */                            \
    GBOP_DEFINE_HOOK(module, WriteFile)                              \
    /* FIXME: performance, interesting if a handle is open */        \
    GBOP_DEFINE_HOOK(module, WriteFileEx)                            \
    /* FIXME: performance, interesting if a handle is open */        \
    /* KERNEL32 presumed to be complete */

#define GBOP_DEFINE_WININET_BASE_HOOKS(module) /* "WININET.dll" */ \
    GBOP_DEFINE_HOOK(module, FtpGetFileA)                          \
    /* risk of creating a local file */                            \
    GBOP_DEFINE_HOOK(module, InternetConnectA)                     \
    GBOP_DEFINE_HOOK(module, InternetConnectW)                     \
    GBOP_DEFINE_HOOK(module, InternetOpenA)                        \
    GBOP_DEFINE_HOOK(module, InternetOpenUrlA)                     \
    GBOP_DEFINE_HOOK(module, InternetOpenUrlW)                     \
    GBOP_DEFINE_HOOK(module, InternetOpenW)
/* InternetReadFile needs a handle created by the above */
/* WININET presumed to be complete */

#define GBOP_DEFINE_MSVCRT_BASE_HOOKS(module) /* "MSVCRT.dll" */ \
    GBOP_DEFINE_HOOK(module, system)

/* FIXME: case 8006 currently unused */
#define GBOP_DEFINE_MSVCRT_MORE_HOOKS(module) /* "MSVCRT.dll" */ \
    GBOP_DEFINE_HOOK(module, _execl)                             \
    GBOP_DEFINE_HOOK(module, _execle)                            \
    GBOP_DEFINE_HOOK(module, _execlp)                            \
    GBOP_DEFINE_HOOK(module, _execlpe)                           \
    GBOP_DEFINE_HOOK(module, _execv)                             \
    GBOP_DEFINE_HOOK(module, _execve)                            \
    GBOP_DEFINE_HOOK(module, _execvp)                            \
    GBOP_DEFINE_HOOK(module, _execvpe)                           \
                                                                 \
    GBOP_DEFINE_HOOK(module, _getdllprocaddr)                    \
    GBOP_DEFINE_HOOK(module, _loaddll)                           \
    GBOP_DEFINE_HOOK(module, _popen /* process+pipe open */)     \
                                                                 \
    GBOP_DEFINE_HOOK(module, _spawnl)                            \
    GBOP_DEFINE_HOOK(module, _spawnle)                           \
    GBOP_DEFINE_HOOK(module, _spawnlp)                           \
    GBOP_DEFINE_HOOK(module, _spawnlpe)                          \
    GBOP_DEFINE_HOOK(module, _spawnv)                            \
    GBOP_DEFINE_HOOK(module, _spawnve)                           \
    GBOP_DEFINE_HOOK(module, _spawnvp)                           \
    GBOP_DEFINE_HOOK(module, _spawnvpe)                          \
                                                                 \
    GBOP_DEFINE_HOOK(module, _wexecl)                            \
    GBOP_DEFINE_HOOK(module, _wexecle)                           \
    GBOP_DEFINE_HOOK(module, _wexeclp)                           \
    GBOP_DEFINE_HOOK(module, _wexeclpe)                          \
    GBOP_DEFINE_HOOK(module, _wexecv)                            \
    GBOP_DEFINE_HOOK(module, _wexecve)                           \
    GBOP_DEFINE_HOOK(module, _wexecvp)                           \
    GBOP_DEFINE_HOOK(module, _wexecvpe)                          \
    GBOP_DEFINE_HOOK(module, _wspawnl)                           \
    GBOP_DEFINE_HOOK(module, _wspawnle)                          \
    GBOP_DEFINE_HOOK(module, _wspawnlp)                          \
    GBOP_DEFINE_HOOK(module, _wspawnlpe)                         \
    GBOP_DEFINE_HOOK(module, _wspawnv)                           \
    GBOP_DEFINE_HOOK(module, _wspawnve)                          \
    GBOP_DEFINE_HOOK(module, _wspawnvp)                          \
    GBOP_DEFINE_HOOK(module, _wspawnvpe)                         \
    GBOP_DEFINE_HOOK(module, _wsystem)                           \
    /* FIXME: more file creation related to add */

#define GBOP_DEFINE_WS2_32_BASE_HOOKS(module) /* "WS2_32.dll" */ \
    GBOP_DEFINE_HOOK(module, WSASocketA)                         \
    GBOP_DEFINE_HOOK(module, WSASocketW)                         \
    GBOP_DEFINE_HOOK(module, bind)                               \
    GBOP_DEFINE_HOOK(module, getpeername)                        \
    GBOP_DEFINE_HOOK(module, socket)
/* most operations create a new socket(), or reuse one */

/* FIXME: case 8006 currently unused in default gbop_include_set */
#define GBOP_DEFINE_WS2_32_MORE_HOOKS(module) /* "WS2_32.dll" */ \
    GBOP_DEFINE_HOOK(module, connect)                            \
    GBOP_DEFINE_HOOK(module, listen)                             \
    GBOP_DEFINE_HOOK(module, WSAConnect)                         \
    /* not adding the send family: WSASend, WSASendTo, send, sendto */

/* note that WSOCK32.dll is WinSock 1.1 and WS2_32.DLL is WinSock 2,
 * yet all interesting routines that we hook of identical names are
 * simply forwarded from WSOCK32.dll to WS2_32.DLL so our hooks there
 * are sufficient.
 */

#define GBOP_DEFINE_USER32_BASE_HOOKS(module) /* "USER32.dll" */ \
    GBOP_DEFINE_HOOK(module, MessageBoxIndirectA)                \
    GBOP_DEFINE_HOOK(module, MessageBoxA)                        \
    GBOP_DEFINE_HOOK(module, MessageBoxExW)                      \
    GBOP_DEFINE_HOOK(module, MessageBoxExA)                      \
    GBOP_DEFINE_HOOK(module, MessageBoxTimeoutW)                 \
    GBOP_DEFINE_HOOK(module, MessageBoxTimeoutA)                 \
    GBOP_DEFINE_HOOK(module, MessageBoxIndirectW)                \
    GBOP_DEFINE_HOOK(module, MessageBoxW)                        \
    GBOP_DEFINE_HOOK(module, ExitWindowsEx)
/* USER32 presumed to be complete */

/* FIXME: case 8006 currently unused in default gbop_include_set */
#define GBOP_DEFINE_SHELL32_BASE_HOOKS(module) /* "SHELL32.dll" */   \
    GBOP_DEFINE_HOOK(module, RealShellExecuteA)                      \
    GBOP_DEFINE_HOOK(module, RealShellExecuteExA)                    \
    GBOP_DEFINE_HOOK(module, RealShellExecuteExW)                    \
    GBOP_DEFINE_HOOK(module, RealShellExecuteW)                      \
    GBOP_DEFINE_HOOK(module, SHCreateDirectory)                      \
    GBOP_DEFINE_HOOK(module, SHCreateDirectoryExA)                   \
    GBOP_DEFINE_HOOK(module, SHCreateProcessAsUserW)                 \
    /* SHFileOperation is an alias for SHFileOperationA on XP SP2 */ \
    GBOP_DEFINE_HOOK(module, SHFileOperationA)                       \
    GBOP_DEFINE_HOOK(module, SHFileOperationW)                       \
    GBOP_DEFINE_HOOK(module, ShellExec_RunDLLA)                      \
    GBOP_DEFINE_HOOK(module, ShellExec_RunDLLW)                      \
    GBOP_DEFINE_HOOK(module, ShellExecuteA)                          \
    GBOP_DEFINE_HOOK(module, ShellExecuteExA)                        \
    GBOP_DEFINE_HOOK(module, ShellExecuteExW)                        \
    GBOP_DEFINE_HOOK(module, ShellExecuteW)                          \
    GBOP_DEFINE_HOOK(module, WOWShellExecute)

/* caution these shouldn't overlap with hooks that DR normally has. */
/* Fortunately hotp_only doesn't allow duplicate hooks so it will
 * detect any such overlap. */
#define GBOP_DEFINE_NTDLL_MORE_HOOKS(module) /* "ntdll.dll" */ \
    GBOP_DEFINE_HOOK(module, LdrGetDllHandle)                  \
    GBOP_DEFINE_HOOK(module, LdrGetDllHandleEx)                \
    GBOP_DEFINE_HOOK(module, LdrGetProcedureAddress)           \
    GBOP_DEFINE_HOOK(module, NtCreateFile)                     \
    GBOP_DEFINE_HOOK(module, NtCreateKey)                      \
    GBOP_DEFINE_HOOK(module, NtCreateToken)                    \
    GBOP_DEFINE_HOOK(module, NtDeleteKey)                      \
    GBOP_DEFINE_HOOK(module, NtDeleteValueKey)                 \
    GBOP_DEFINE_HOOK(module, NtSetValueKey)                    \
    GBOP_DEFINE_HOOK(module, NtShutdownSystem)                 \
    GBOP_DEFINE_HOOK(module, NtWriteVirtualMemory)
/* FIXME: case 8006 have to add a complete set for NTDLL.DLL */

/* FIXME: case 8006, need to add ADVAPI32 wrappers as
 * ADVAPI32!CreateProcessAsUser*
 */

/* FIXME: I am not sure if this will scale to the number of modules -
 * since there is a limit on the macro size (at least in cl) */
/* all users are expected to define appropriately
 * GBOP_DEFINE_HOOK_MODULE and GBOP_DEFINE_HOOK
 */
#define GBOP_ALL_HOOKS                                   \
    /* counting hooks that we have without hotp */       \
    /* GBOP_SET_NTDLL_BASE         0x1 */                \
    GBOP_DEFINE_HOOK_MODULE(KERNEL32, BASE) /* 0x2 */    \
    GBOP_DEFINE_HOOK_MODULE(MSVCRT, BASE)   /* 0x4 */    \
    GBOP_DEFINE_HOOK_MODULE(WS2_32, BASE)   /* 0x8 */    \
    GBOP_DEFINE_HOOK_MODULE(WININET, BASE)  /*0x10 */    \
    /* case 8006: gbop_include_set defaults stop here */ \
    GBOP_DEFINE_HOOK_MODULE(USER32, BASE)  /* 0x20 */    \
    GBOP_DEFINE_HOOK_MODULE(SHELL32, BASE) /* 0x40 */    \
                                                         \
    GBOP_DEFINE_HOOK_MODULE(NTDLL, MORE)    /* 0x100 */  \
    GBOP_DEFINE_HOOK_MODULE(KERNEL32, MORE) /*0x200 */   \
    GBOP_DEFINE_HOOK_MODULE(MSVCRT, MORE)   /* 0x400 */  \
    GBOP_DEFINE_HOOK_MODULE(WS2_32, MORE)   /*0x800 */   \
    /* FIXME: need GBOP_DEFINE_NTDLL_MORE_HOOKS */

enum {
    GBOP_SET_NTDLL_BASE = 0x1,
    /* note that all other flags used in -gbop_set have the same bit
     * position equal to their order in GBOP_ALL_HOOKS above
     */
};

#endif /* GBOP_H */
