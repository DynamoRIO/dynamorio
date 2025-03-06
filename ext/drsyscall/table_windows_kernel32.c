/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Need this defined and to the latest to get the latest defines and types */
#define _WIN32_WINNT 0x0601 /* == _WIN32_WINNT_WIN7 */
#define WINVER _WIN32_WINNT

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"
#include "table_defines.h"

/***************************************************************************/
/* System calls with wrappers in kernel32.dll (on win7 these are duplicated
 * in kernelbase.dll as well but w/ the same syscall number)
 * Not all wrappers are exported: xref i#388.
 */

/* FIXME i#1089: fill in info on all the inlined args for all of
 * syscalls in this file.
 */

syscall_info_t syscall_kernel32_info[] = {
    /* wchar_t *locale DR_PARAM_OUT, size_t locale_sz (assuming size in bytes) */
    {{0,0},"NtWow64CsrBasepNlsGetUserInfo", OK, RNTST, 2,
     {
         {0, -1, W|CT, SYSARG_TYPE_CSTRING_WIDE},
     }
    },

    /* Takes a single param that's a pointer to a struct that has a PHANDLE at offset
     * 0x7c where the base of a new mmap is stored by the kernel.  We handle that by
     * waiting for RtlCreateActivationContext (i#352).  We don't know of any written
     * values in the rest of the struct or its total size so we ignore it for now and
     * use this entry to avoid "unknown syscall" warnings.
     *
     * XXX: there are 4+ wchar_t* input strings in the struct: should check them.
     */
    {{0,0},"NtWow64CsrBasepCreateActCtx", OK, RNTST, 1, },

    /* FIXME i#1091: add further kernel32 syscall info */
    {{0,0},"AddConsoleAliasInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"AllocConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"AttachConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"CloseConsoleHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ConnectConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ConsoleMenuControl", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"CreateConsoleScreenBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"DuplicateConsoleHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ExpungeConsoleCommandHistoryInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"FillConsoleOutput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"FlushConsoleInputBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"FreeConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GenerateConsoleCtrlEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleAliasExesInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleAliasExesLengthInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleAliasInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleAliasesInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleAliasesLengthInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCP", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCharType", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCommandHistoryInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCommandHistoryLengthInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCursorInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleCursorMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleDisplayMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleFontInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleFontSize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleHandleInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleHardwareState", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleInput", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(INPUT_RECORD)},
         {1, -3, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(INPUT_RECORD)},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"GetConsoleKeyboardLayoutNameWorker", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleLangId", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleNlsMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleOutputCP", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleProcessList", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleScreenBufferInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleSelectionInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleTitleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetCurrentConsoleFont", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetLargestConsoleWindowSize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetNumberOfConsoleFonts", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetNumberOfConsoleInputEvents", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetNumberOfConsoleMouseButtons", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"InvalidateConsoleDIBits", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBaseCheckRunApp", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBaseClientConnectToServer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBaseQueryModuleData", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepCreateProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepCreateThread", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepDefineDosDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepExitProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepGetProcessShutdownParam", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepGetTempFile", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepNlsCreateSection", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepNlsSetMultipleUserInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepNlsSetUserInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepNlsUpdateCacheCount", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepRefreshIniFileMapping", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepSetClientTimeZoneInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepSetProcessShutdownParam", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepSetTermsrvAppInstallMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtWow64CsrBasepSoundSentryNotification", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"OpenConsoleWInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ReadConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ReadConsoleOutputInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ReadConsoleOutputString", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"RegisterConsoleIMEInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"RegisterConsoleOS2", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"RegisterConsoleVDM", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ScrollConsoleScreenBufferInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleActiveScreenBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCP", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCommandHistoryMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCursorInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCursorMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleCursorPosition", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleDisplayMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleFont", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleHandleInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleHardwareState", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleIcon", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleKeyShortcuts", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleLocalEUDC", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleMenuClose", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleNlsMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleNumberOfCommandsInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleOS2OemFormat", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleOutputCPInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsolePaletteInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleScreenBufferSize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleTextAttribute", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleTitleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleWindowInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetLastConsoleEventActiveInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"ShowConsoleCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"UnregisterConsoleIMEInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"VerifyConsoleIoHandle", OK, DRSYS_TYPE_BOOL, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"WriteConsoleInputInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"WriteConsoleInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"WriteConsoleOutputInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"WriteConsoleOutputString", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Vista */
    /* XXX: add min OS version: but we have to distinguish the service packs! */
    {{0,0},"GetConsoleHistoryInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetConsoleScreenBufferInfoEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"GetCurrentConsoleFontEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"QueryConsoleIMEInternal", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleHistoryInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetConsoleScreenBufferInfoEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"SetCurrentConsoleFontEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Windows 8 */
    {{WIN8,0},"NtWow64ConsoleLaunchServerProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
};
#define NUM_KERNEL32_SYSCALLS \
    (sizeof(syscall_kernel32_info)/sizeof(syscall_kernel32_info[0]))

size_t
num_kernel32_syscalls(void)
{
    return NUM_KERNEL32_SYSCALLS;
}
