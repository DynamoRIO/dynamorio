/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
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
#include <stddef.h> /* offsetof */

#include "../wininc/ndk_extypes.h" /* required by ntuser.h */
#include "../wininc/ntuser.h"
#include "../wininc/ntuser_win8.h"
#include "../wininc/ntuser_ex.h"

/***************************************************************************/
/* System calls with wrappers in user32.dll.
 * Not all wrappers are exported: xref i#388.
 *
 * Initially obtained via mksystable.pl on ntuser.h.
 * That version was checked in separately to track manual changes.
 *
 * When adding new entries, use the NtUser prefix.
 * When we try to find the wrapper via symbol lookup we try with
 * and without the prefix.
 *
 * Unresolved issues are marked w/ FIXME in the table.
 */

/* FIXME i#1089: fill in info on all the inlined args for all of
 * syscalls in this file.
 */

/* FIXME i#1093: figure out the failure codes for all the int and uint return values */

extern drsys_sysnum_t sysnum_UserSystemParametersInfo;
extern drsys_sysnum_t sysnum_UserMenuInfo;
extern drsys_sysnum_t sysnum_UserMenuItemInfo;
extern drsys_sysnum_t sysnum_UserGetAltTabInfo;
extern drsys_sysnum_t sysnum_UserGetRawInputBuffer;
extern drsys_sysnum_t sysnum_UserGetRawInputData;
extern drsys_sysnum_t sysnum_UserGetRawInputDeviceInfo;
extern drsys_sysnum_t sysnum_UserTrackMouseEvent;
extern drsys_sysnum_t sysnum_UserLoadKeyboardLayoutEx;
extern drsys_sysnum_t sysnum_UserCreateWindowStation;
extern drsys_sysnum_t sysnum_UserMessageCall;
extern drsys_sysnum_t sysnum_UserCreateAcceleratorTable;
extern drsys_sysnum_t sysnum_UserCopyAcceleratorTable;
extern drsys_sysnum_t sysnum_UserSetScrollInfo;

extern syscall_info_t syscall_UserCallNoParam_info[];
extern syscall_info_t syscall_UserCallOneParam_info[];
extern syscall_info_t syscall_UserCallTwoParam_info[];
extern syscall_info_t syscall_UserCallHwnd_info[];
extern syscall_info_t syscall_UserCallHwndLock_info[];
extern syscall_info_t syscall_UserCallHwndOpt_info[];
extern syscall_info_t syscall_UserCallHwndParam_info[];
extern syscall_info_t syscall_UserCallHwndParamLock_info[];

syscall_info_t syscall_user32_info[] = {
    {{0,0},"NtUserActivateKeyboardLayout", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserAlterWindowStyle", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserAssociateInputContext", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserAttachThreadInput", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserBeginPaint", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PAINTSTRUCT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserBitBltSysBmp", OK, SYSARG_TYPE_BOOL32, 8,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserBlockInput", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserBuildHimcList", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(HIMC)},
         {3, sizeof(UINT), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,WIN7},"NtUserBuildHwndList", OK, RNTST, 7,
     {
         {0, sizeof(HDESK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -6, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(HWND)},
         {6, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN8,0},"NtUserBuildHwndList", OK, RNTST, 8,
     {
         {0, sizeof(HDESK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* i#1153: size of buffer seems to be a separate inline param inserted
          * at 5th position.
          */
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, -5, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(HWND)},
         {7, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserBuildMenuItemList", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserBuildNameList", OK, RNTST, 4,
     {
         {0, sizeof(HWINSTA), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|HT, DRSYS_TYPE_STRUCT},
         {2, -3, WI|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserBuildPropList", OK, RNTST, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(USER_PROP_LIST_ENTRY)},
         {2, -3, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(USER_PROP_LIST_ENTRY)},
         {3, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCalcMenuBar", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* i#389: NtUserCall* take in a code and perform a variety of tasks */
    {{0,0},"NtUserCallHwnd", OK|SYSINFO_SECONDARY_TABLE, SYSARG_TYPE_UINT32, 2,
     {
         {1,}  /* code is param #1 */
     }, (drsys_sysnum_t*)syscall_UserCallHwnd_info
    },
    {{0,0},"NtUserCallHwndLock", OK|SYSINFO_SECONDARY_TABLE, SYSARG_TYPE_BOOL32, 2,
     {
         {1,}  /* code is param #1 */
     }, (drsys_sysnum_t*)syscall_UserCallHwndLock_info
    },
    {{0,0},"NtUserCallHwndOpt", OK|SYSINFO_SECONDARY_TABLE, DRSYS_TYPE_HANDLE, 2,
     {
         {1,}  /* code is param #1 */
     }, (drsys_sysnum_t*)syscall_UserCallHwndOpt_info
    },
    {{0,0},"NtUserCallHwndParam", OK|SYSINFO_SECONDARY_TABLE, SYSARG_TYPE_UINT32, 3,
     {
         {2,}  /* code is param #2 */
     }, (drsys_sysnum_t*)syscall_UserCallHwndParam_info
    },
    {{0,0},"NtUserCallHwndParamLock", OK|SYSINFO_SECONDARY_TABLE, SYSARG_TYPE_UINT32, 3,
     {
         {2,}  /* code is param #2 */
     }, (drsys_sysnum_t*)syscall_UserCallHwndParamLock_info
    },
    {{0,0},"NtUserCallMsgFilter", UNKNOWN, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(MSG), R|W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserCallNextHookEx", UNKNOWN, DRSYS_TYPE_SIGNED_INT, 4,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(WPARAM), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LPARAM), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserCallNoParam", OK|SYSINFO_SECONDARY_TABLE, DRSYS_TYPE_UNSIGNED_INT, 1,
     {
         {0,}  /* code is param #0 */
     }, (drsys_sysnum_t*)syscall_UserCallNoParam_info
    },
    {{0,0},"NtUserCallOneParam", OK|SYSINFO_SECONDARY_TABLE, DRSYS_TYPE_UNSIGNED_INT, 2,
     {
         {1,}  /* code is param #1 */
     }, (drsys_sysnum_t*)syscall_UserCallOneParam_info
    },
    {{0,0},"NtUserCallTwoParam", OK|SYSINFO_SECONDARY_TABLE, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {2,}  /* code is param #2 */
     }, (drsys_sysnum_t*)syscall_UserCallTwoParam_info
    },
    {{0,0},"NtUserChangeClipboardChain", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserChangeDisplaySettings", OK, SYSARG_TYPE_SINT32, 5,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(DEVMODEW)/*really var-len*/, R|CT, SYSARG_TYPE_DEVMODEW},
         {2, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -5, W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserCheckDesktopByThreadId", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCheckImeHotKey", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCheckMenuItem", OK|SYSINFO_RET_MINUS1_FAIL, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCheckWindowThreadDesktop", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserChildWindowFromPointEx", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserClipCursor", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserCloseClipboard", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserCloseDesktop", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDESK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserCloseWindowStation", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWINSTA), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserConsoleControl", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserConvertMemHandle", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, -1, R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCopyAcceleratorTable", OK|SYSINFO_RET_ZERO_FAIL, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HACCEL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         /* special-cased b/c ACCEL has padding */
         {1, -2, SYSARG_NON_MEMARG|SYSARG_SIZE_IN_ELEMENTS, sizeof(ACCEL)},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserCopyAcceleratorTable,
    },
    {{0,0},"NtUserCountClipboardFormats", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserCreateAcceleratorTable", OK, DRSYS_TYPE_HANDLE, 2,
     {
         /* special-cased b/c ACCEL has padding */
         {0, -1, SYSARG_NON_MEMARG|SYSARG_SIZE_IN_ELEMENTS, sizeof(ACCEL)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserCreateAcceleratorTable,
    },
    {{0,0},"NtUserCreateCaret", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HBITMAP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserCreateDesktop", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(DEVMODEW)/*really var-len*/, R|CT, SYSARG_TYPE_DEVMODEW},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCreateInputContext", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCreateLocalMemHandle", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserCreateWindowEx", OK, DRSYS_TYPE_HANDLE, 15,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(LARGE_STRING), R|CT, SYSARG_TYPE_LARGE_STRING},
         {2, sizeof(LARGE_STRING), R|CT, SYSARG_TYPE_LARGE_STRING},
         {3, sizeof(LARGE_STRING), R|CT, SYSARG_TYPE_LARGE_STRING},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {10, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {11, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {12, sizeof(LPVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {13, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {14, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtUserCreateWindowStation", OK, DRSYS_TYPE_HANDLE, 7,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserCreateWindowStation
    },
    {{0,0},"NtUserCtxDisplayIOCtl", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDdeGetQualityOfService", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(SECURITY_QUALITY_OF_SERVICE), W|CT, SYSARG_TYPE_SECURITY_QOS},
     }
    },
    {{0,0},"NtUserDdeInitialize", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDdeSetQualityOfService", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SECURITY_QUALITY_OF_SERVICE), R|CT, SYSARG_TYPE_SECURITY_QOS},
         {2, sizeof(SECURITY_QUALITY_OF_SERVICE), W|CT, SYSARG_TYPE_SECURITY_QOS},
     }
    },
    {{0,0},"NtUserDefSetText", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_STRING), R|CT, SYSARG_TYPE_LARGE_STRING},
     }
    },
    {{0,0},"NtUserDeferWindowPos", OK, DRSYS_TYPE_HANDLE, 8,
     {
         {0, sizeof(HDWP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {7, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDeleteMenu", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDestroyAcceleratorTable", OK, SYSARG_TYPE_BOOL8, 1,
     {
         {0, sizeof(HACCEL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserDestroyCursor", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserDestroyInputContext", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDestroyMenu", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserDestroyWindow", OK, SYSARG_TYPE_BOOL8, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserDisableThreadIme", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDispatchMessage", OK, DRSYS_TYPE_SIGNED_INT, 1,
     {
         {0, sizeof(MSG), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserDragDetect", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(POINT), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserDragObject", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(HCURSOR), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserDrawAnimatedRects", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserDrawCaption", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDrawCaptionTemp", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(HFONT), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(HICON), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {6, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDrawIconEx", OK, SYSARG_TYPE_BOOL32, 11,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(HICON), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {8, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {10, sizeof(DRAWICONEXDATA), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserDrawMenuBarTemp", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(HFONT), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserEmptyClipboard", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserEnableMenuItem", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserEnableScrollBar", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserEndDeferWindowPosEx", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HDWP), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserEndMenu", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserEndPaint", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PAINTSTRUCT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserEnumDisplayDevices", OK, RNTST, 4,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, SYSARG_SIZE_IN_FIELD, W, offsetof(DISPLAY_DEVICEW, cb)},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserEnumDisplayMonitors", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(MONITORENUMPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(LPARAM), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserEnumDisplaySettings", OK, RNTST, 4,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DEVMODEW)/*really var-len*/, W|CT, SYSARG_TYPE_DEVMODEW},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserEvent", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserExcludeUpdateRgn", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserFillWindow", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(HBRUSH), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserFindExistingCursorIcon", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(HMODULE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRSRC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserFindWindowEx", OK, DRSYS_TYPE_HANDLE, 5,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserFlashWindowEx", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, SYSARG_SIZE_IN_FIELD, R, offsetof(FLASHWINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetAltTabInfo", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ALTTABINFO), R|HT, DRSYS_TYPE_STRUCT},
         /* The buffer is ansi or unicode so memarg and non-memarg are special-cased. */
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }, &sysnum_UserGetAltTabInfo
    },
    {{0,0},"NtUserGetAncestor", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetAppImeLevel", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetAsyncKeyState", OK, SYSARG_TYPE_SINT16, 1,
     {
         {0, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetAtomName", OK|SYSINFO_RET_ZERO_FAIL, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(ATOM), SYSARG_INLINED, DRSYS_TYPE_ATOM},
         {1, sizeof(UNICODE_STRING), W|CT, SYSARG_TYPE_UNICODE_STRING_NOLEN/*i#490*/},
     }
    },
    {{0,0},"NtUserGetCPD", OK, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(GETCPD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetCaretBlinkTime", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserGetCaretPos", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(POINT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserGetClassInfo", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(WNDCLASSEXW), W|CT, SYSARG_TYPE_WNDCLASSEXW},
         {3, sizeof(PWSTR)/*pointer to existing string (ansi or unicode) is copied*/, W,},
         {4, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetClassInfoEx", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(WNDCLASSEXW), W|CT, SYSARG_TYPE_WNDCLASSEXW},
         {3, sizeof(PWSTR)/*pointer to existing string (ansi or unicode) is copied*/, W,},
         {4, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    /* XXX: Filled in based on ROS and should verify correct. */
    {{0,0},"NtUserGetClassLong", OK, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetClassName", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(UNICODE_STRING), W|CT, SYSARG_TYPE_UNICODE_STRING_NOLEN/*i#490*/},
     }
    },
    {{0,0},"NtUserGetClipCursor", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserGetClipboardData", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(GETCLIPBDATA), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserGetClipboardFormatName", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, RET, W|SYSARG_SIZE_IN_ELEMENTS|SYSARG_SIZE_PLUS_1, sizeof(wchar_t)},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetClipboardOwner", OK, DRSYS_TYPE_HANDLE, 0, },
    {{0,0},"NtUserGetClipboardSequenceNumber", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserGetClipboardViewer", OK, DRSYS_TYPE_HANDLE, 0, },
    {{0,0},"NtUserGetComboBoxInfo", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, SYSARG_SIZE_IN_FIELD, W, offsetof(COMBOBOXINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetControlBrush", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetControlColor", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetCursorFrameInfo", OK, DRSYS_TYPE_HANDLE, 4,
     {
         {0, sizeof(HCURSOR), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(INT), W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetCursorInfo", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, SYSARG_SIZE_IN_FIELD, W, offsetof(CURSORINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetDC", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserGetDCEx", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetDoubleClickTime", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserGetForegroundWindow", OK, DRSYS_TYPE_HANDLE, 0, },
    {{0,0},"NtUserGetGUIThreadInfo", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, SYSARG_SIZE_IN_FIELD, W, offsetof(GUITHREADINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetGuiResources", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetIconInfo", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ICONINFO), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(UNICODE_STRING), W|CT, SYSARG_TYPE_UNICODE_STRING_NOLEN/*i#490*/},
         {3, sizeof(UNICODE_STRING), W|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetIconSize", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LONG), W|HT, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(LONG), W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetImeHotKey", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* FIXME i#487: 1st param is OUT but shape is unknown. 2nd param seems to be an
     * info class, but not fully known.
     */
    {{0,0},"NtUserGetImeInfoEx", UNKNOWN|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetInternalWindowPos", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(POINT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserGetKeyNameText", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, RET, W|SYSARG_SIZE_IN_ELEMENTS|SYSARG_SIZE_PLUS_1, sizeof(wchar_t)},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetKeyState", OK, SYSARG_TYPE_SINT16, 1,
     {
         {0, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetKeyboardLayout", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetKeyboardLayoutList", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -0, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(HKL)},
         {1, RET, W|SYSARG_NO_WRITE_IF_COUNT_0|SYSARG_SIZE_IN_ELEMENTS, sizeof(HKL)},
     }
    },
    {{0,0},"NtUserGetKeyboardLayoutName", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, KL_NAMELENGTH*sizeof(wchar_t), W|CT, SYSARG_TYPE_CSTRING_WIDE},
     }
    },
    {{0,0},"NtUserGetKeyboardState", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, USER_KEYBOARD_STATE_SIZE, W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetKeyboardType", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetLastInputInfo", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, SYSARG_SIZE_IN_FIELD, W, offsetof(LASTINPUTINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetLayeredWindowAttributes", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORREF), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BYTE), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetListBoxInfo", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserGetMenuBarInfo", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, SYSARG_SIZE_IN_FIELD, W, offsetof(MENUBARINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetMenuDefaultItem", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetMenuIndex", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserGetMenuItemRect", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserGetMessage", OK, RNTST, 4,
     {
         {0, sizeof(MSG), W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetMinMaxInfo", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {1, sizeof(MINMAXINFO), W,},
     }
    },
    {{0,0},"NtUserGetMonitorInfo", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HMONITOR), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, SYSARG_SIZE_IN_FIELD, W, offsetof(MONITORINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetMouseMovePointsEx", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -0, R|HT, DRSYS_TYPE_STRUCT},
         {2, -3, W|SYSARG_SIZE_IN_ELEMENTS, -0},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetObjectInformation", OK|SYSINFO_RET_SMALL_WRITE_LAST, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, W|HT, DRSYS_TYPE_STRUCT},
         {2, -4, WI|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetOpenClipboardWindow", OK, DRSYS_TYPE_HANDLE, 0, },
    {{0,0},"NtUserGetPriorityClipboardFormat", OK, SYSARG_TYPE_SINT32, 2,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(UINT)},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserGetProcessWindowStation", OK, DRSYS_TYPE_HANDLE, 0, },
    {{0,0},"NtUserGetRawInputBuffer", OK, SYSARG_TYPE_UINT32, 3,
     {
        /* param #0 has both mem and non-memarg handled in code */
         {1, sizeof(UINT), SYSARG_NON_MEMARG, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, /*special-cased; FIXME: i#485: see handler*/ &sysnum_UserGetRawInputBuffer
    },
    {{0,0},"NtUserGetRawInputData", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(HRAWINPUT), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, WI|HT, DRSYS_TYPE_STRUCT},
         {2, RET, W},
         /*arg 3 is R or W => special-cased*/
         {3, sizeof(UINT), SYSARG_NON_MEMARG, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserGetRawInputData
    },
    {{0,0},"NtUserGetRawInputDeviceInfo", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_NON_MEMARG, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserGetRawInputDeviceInfo
    },
    {{0,0},"NtUserGetRawInputDeviceList", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, -1, WI|SYSARG_SIZE_IN_ELEMENTS, -2},
         /*really not written when #0!=NULL but harmless; ditto below and probably elsewhere in table*/
         {1, sizeof(UINT), R|W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetRegisteredRawInputDevices", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, -1, WI|SYSARG_SIZE_IN_ELEMENTS, -2},
         {1, sizeof(UINT), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetScrollBarInfo", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, SYSARG_SIZE_IN_FIELD, W, offsetof(SCROLLBARINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetSystemMenu", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetThreadDesktop", OK|SYSINFO_REQUIRES_PREFIX, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* XXX i#487: on WOW64 XP and Vista (but not win7) this makes a 0x2xxx syscall
     * instead of invoking NtUserGetThreadDesktop: is it really different?
     */
    {{0,0},"GetThreadDesktop", OK, RNTST, 2, },
    {{0,0},"NtUserGetThreadState", OK, DRSYS_TYPE_UNSIGNED_INT, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserGetTitleBarInfo", OK, SYSARG_TYPE_BOOL8, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, SYSARG_SIZE_IN_FIELD, W, offsetof(TITLEBARINFO, cbSize)},
     }
    },
    {{0,0},"NtUserGetUpdateRect", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetUpdateRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserGetWOWClass", OK, DRSYS_TYPE_POINTER, 2,
     {
         {0, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtUserGetWindowDC", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserGetWindowPlacement", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, SYSARG_SIZE_IN_FIELD, W, offsetof(WINDOWPLACEMENT, length)},
     }
    },
    {{0,0},"NtUserHardErrorControl", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserHideCaret", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserHiliteMenuItem", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserImpersonateDdeClientWindow", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserInitTask", OK, SYSARG_TYPE_UINT32, 12,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserInitialize", OK, RNTST, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* FIXME i#487: not sure whether these are arrays and if so how long they are */
    {{0,0},"NtUserInitializeClientPfnArrays", UNKNOWN, RNTST, 4,
     {
         {0, sizeof(PFNCLIENT), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(PFNCLIENT), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(PFNCLIENTWORKER), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserInternalGetWindowText", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {1, 0, W|CT, SYSARG_TYPE_CSTRING_WIDE},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserInvalidateRect", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserInvalidateRgn", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserIsClipboardFormatAvailable", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserKillTimer", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserLoadKeyboardLayoutEx", OK, DRSYS_TYPE_HANDLE, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserLoadKeyboardLayoutEx
    },
    {{0,0},"NtUserLockWindowStation", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWINSTA), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserLockWindowUpdate", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserLockWorkStation", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserMNDragLeave", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserMNDragOver", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMapVirtualKeyEx", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserMenuInfo", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(MENUINFO), SYSARG_NON_MEMARG, DRSYS_TYPE_STRUCT},/*can be R or W*/
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }, &sysnum_UserMenuInfo
    },
    {{0,0},"NtUserMenuItemFromPoint", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMenuItemInfo", OK, SYSARG_TYPE_BOOL32, 5,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(MENUITEMINFO), SYSARG_NON_MEMARG, DRSYS_TYPE_STRUCT},/*can be R or W*/
         {4, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }, &sysnum_UserMenuItemInfo
    },
    /* i#1249: NtUserMessageCall has a lot of sub-actions based on both 2nd
     * param and 6th param.  However, enough are identical for our purposes that
     * we handle in code.  That's based on an early examination: if more and
     * more need special handling we may want to switch to a secondary table(s).
     * The return value is an LRESULT.
     */
    {{0,0},"NtUserMessageCall", OK, SYSARG_TYPE_SINT32, 7,
     {
         {0, sizeof(HANDLE),  SYSARG_INLINED,    DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT),    SYSARG_INLINED,    DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(WPARAM),  SYSARG_INLINED,    DRSYS_TYPE_UNSIGNED_INT},
         /* For some WM_ codes this is a pointer: special-cased.
          * XXX: non-memarg client would want secondary table(s)!
          */
         {3, sizeof(LPARAM),  SYSARG_INLINED,    DRSYS_TYPE_SIGNED_INT},
         /* 4th param is sometimes IN and sometimes OUT so we special-case it.
          * XXX: however, now that we know the syscall return is
          * LRESULT (i#1752), and this param always seems to be NULL,
          * we may need to revisit what type it really is.
          */
         {4, sizeof(LRESULT), SYSARG_NON_MEMARG, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD),   SYSARG_INLINED,    DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(BOOL),    SYSARG_INLINED,    DRSYS_TYPE_BOOL},
     }, &sysnum_UserMessageCall
    },
    {{0,0},"NtUserMinMaximize", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserModifyUserStartupInfoFlags", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMonitorFromPoint", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(POINT), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMonitorFromRect", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMonitorFromWindow", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserMoveWindow", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserNotifyIMEStatus", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserNotifyProcessCreate", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserNotifyWinEvent", OK, DRSYS_TYPE_VOID, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserOpenClipboard", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserOpenDesktop", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserOpenInputDesktop", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserOpenWindowStation", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserPaintDesktop", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserPaintMenuBar", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserPeekMessage", OK, RNTST, 5,
     {
         {0, sizeof(MSG), W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserPostMessage", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(WPARAM), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(LPARAM), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserPostThreadMessage", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(WPARAM), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(LPARAM), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserPrintWindow", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* FIXME i#487: lots of pointers inside USERCONNECT */
    {{0,0},"NtUserProcessConnect", UNKNOWN, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(USERCONNECT), W|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserQueryInformationThread", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserQueryInputContext", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserQuerySendMessage", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserQueryUserCounters", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserQueryWindow", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRealChildWindowFromPoint", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserRealInternalGetMessage", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(MSG), W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserRealWaitMessageEx", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRedrawWindow", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterClassExWOW", OK|SYSINFO_RET_ZERO_FAIL, DRSYS_TYPE_ATOM, 7,
     {
         {0, sizeof(WNDCLASSEXW), R|CT, SYSARG_TYPE_WNDCLASSEXW},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(CLSMENUNAME), R|CT, SYSARG_TYPE_CLSMENUNAME},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterHotKey", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterRawInputDevices", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, -2},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterTasklist", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterUserApiHook", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRegisterWindowMessage", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtUserRemoteConnect", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRemoteRedrawRectangle", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRemoteRedrawScreen", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserRemoteStopScreenUpdates", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserRemoveMenu", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserRemoveProp", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ATOM), SYSARG_INLINED, DRSYS_TYPE_ATOM},
     }
    },
    {{0,0},"NtUserResolveDesktop", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserResolveDesktopForWOW", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* FIXME i#487: not sure whether #2 is in or out */
    {{0,0},"NtUserSBGetParms", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(SBDATA), W|HT, DRSYS_TYPE_STRUCT},
         {3, SYSARG_SIZE_IN_FIELD, W, offsetof(SCROLLINFO, cbSize)},
     }
    },
    {{0,0},"NtUserScrollDC", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserScrollWindowEx", OK, SYSARG_TYPE_UINT32, 8,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(RECT), W|HT, DRSYS_TYPE_STRUCT},
         {7, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSelectPalette", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HPALETTE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSendInput", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -0, R|SYSARG_SIZE_IN_ELEMENTS, -2},
         {2, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserSetActiveWindow", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetAppImeLevel", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetCapture", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetClassLong", OK, DRSYS_TYPE_UNSIGNED_INT, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSetClassWord", OK, SYSARG_TYPE_UINT16, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(WORD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserSetClipboardData", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(SETCLIPBDATA), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserSetClipboardViewer", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetConsoleReserveKeys", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetCursor", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HCURSOR), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetCursorContents", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ICONINFO), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserSetCursorIconData", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), R|HT, DRSYS_TYPE_BOOL},
         {2, sizeof(POINT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(HMODULE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(HRSRC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(HRSRC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetDbgTag", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetFocus", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetImeHotKey", OK, SYSARG_TYPE_UINT32, 5,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetImeInfoEx", OK|SYSINFO_IMM32_DLL, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetImeOwnerWindow", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetInformationProcess", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetInformationThread", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(USERTHREADINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetInternalWindowPos", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(POINT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserSetKeyboardState", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, USER_KEYBOARD_STATE_SIZE, R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetLayeredWindowAttributes", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BYTE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetLogonNotifyWindow", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetMenu", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSetMenuContextHelpId", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetMenuDefaultItem", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetMenuFlagRtoL", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetObjectInformation", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetParent", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetProcessWindowStation", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWINSTA), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetProp", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ATOM), SYSARG_INLINED, DRSYS_TYPE_ATOM},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetRipFlags", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetScrollBarInfo", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(SETSCROLLBARINFO), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserSetScrollInfo", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         /* Special-cased b/c some fields are ignored (i#1299) */
         {2, SYSARG_SIZE_IN_FIELD, SYSARG_NON_MEMARG, offsetof(SCROLLINFO, cbSize)},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }, &sysnum_UserSetScrollInfo,
    },
    {{0,0},"NtUserSetShellWindowEx", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetSysColors", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -0, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(INT)},
         {2, -0, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(COLORREF)},
         {3, sizeof(FLONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetSystemCursor", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HCURSOR), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetSystemMenu", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetSystemTimer", OK, DRSYS_TYPE_UNSIGNED_INT, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(TIMERPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
     }
    },
    {{0,0},"NtUserSetThreadDesktop", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDESK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSetThreadLayoutHandles", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetThreadState", OK, SYSARG_TYPE_UINT32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetTimer", OK, DRSYS_TYPE_UNSIGNED_INT, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(TIMERPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
     }
    },
    {{0,0},"NtUserSetWinEventHook", OK, DRSYS_TYPE_HANDLE, 8,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HMODULE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(WINEVENTPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetWindowFNID", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WORD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserSetWindowLong", OK, SYSARG_TYPE_SINT32, 4,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSetWindowPlacement", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, SYSARG_SIZE_IN_FIELD, R, offsetof(WINDOWPLACEMENT, length)},
     }
    },
    {{0,0},"NtUserSetWindowPos", OK, SYSARG_TYPE_BOOL32, 7,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetWindowRgn", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HRGN), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSetWindowStationUser", OK, SYSARG_TYPE_UINT32, 4,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserSetWindowWord", OK, SYSARG_TYPE_UINT16, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(INT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(WORD), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserSetWindowsHookAW", OK, DRSYS_TYPE_HANDLE, 3,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(HOOKPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserSetWindowsHookEx", OK, DRSYS_TYPE_HANDLE, 6,
     {
         {0, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(HOOKPROC), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {5, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserShowCaret", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserShowScrollBar", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserShowWindow", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserShowWindowAsync", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserSoundSentry", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserSwitchDesktop", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HDESK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserSystemParametersInfo", OK, SYSARG_TYPE_BOOL32, 4,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, SYSARG_NON_MEMARG, DRSYS_TYPE_STRUCT},
         {3, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_UserSystemParametersInfo
    },
    {{0,0},"NtUserTestForInteractiveUser", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* there is a pointer in MENUINFO but it's user-defined */
    {{0,0},"NtUserThunkedMenuInfo", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(MENUINFO), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserThunkedMenuItemInfo", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, 0, R|CT, SYSARG_TYPE_MENUITEMINFOW},
         {5, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtUserToUnicodeEx", OK, SYSARG_TYPE_SINT32, 7,
     {
         {0, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, 0x100*sizeof(BYTE), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(wchar_t)},
         {4, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserTrackMouseEvent", OK, SYSARG_TYPE_BOOL32, 1,
     {
         /* Memarg and non-memarg are both special-cased */
         {0,},
     }, &sysnum_UserTrackMouseEvent
    },
    {{0,0},"NtUserTrackPopupMenuEx", OK, SYSARG_TYPE_BOOL32, 6,
     {
         {0, sizeof(HMENU), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, SYSARG_SIZE_IN_FIELD, R, offsetof(TPMPARAMS, cbSize)},
     }
    },
    {{0,0},"NtUserTranslateAccelerator", OK, SYSARG_TYPE_SINT32, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HACCEL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(MSG), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserTranslateMessage", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(MSG), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(UINT), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserUnhookWinEvent", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWINEVENTHOOK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserUnhookWindowsHookEx", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HHOOK), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserUnloadKeyboardLayout", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUserUnlockWindowStation", OK, SYSARG_TYPE_BOOL32, 1,
     {
         {0, sizeof(HWINSTA), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* FIXME i#487: CLSMENUNAME format is not fully known */
    {{0,0},"NtUserUnregisterClass", UNKNOWN, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(HINSTANCE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(CLSMENUNAME), W|CT, SYSARG_TYPE_CLSMENUNAME},
     }
    },
    {{0,0},"NtUserUnregisterHotKey", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserUnregisterUserApiHook", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserUpdateInputContext", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserUpdateInstance", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserUpdateLayeredWindow", OK, SYSARG_TYPE_BOOL32, 10,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(POINT), R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(SIZE), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(POINT), R|HT, DRSYS_TYPE_STRUCT},
         {6, sizeof(COLORREF), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(BLENDFUNCTION), R|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserUpdatePerUserSystemParameters", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserUserHandleGrantAccess", OK, SYSARG_TYPE_BOOL32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserValidateHandleSecure", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserValidateRect", OK, SYSARG_TYPE_BOOL32, 2,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(RECT), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserValidateTimerCallback", OK, RNTST, 3,
     {
         {0, sizeof(HWND), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WPARAM), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LPARAM), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserVkKeyScanEx", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(WCHAR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HKL), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserWaitForInputIdle", OK, SYSARG_TYPE_UINT32, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOL), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUserWaitForMsgAndEvent", OK, SYSARG_TYPE_UINT32, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserWaitMessage", OK, SYSARG_TYPE_BOOL32, 0, },
    {{0,0},"NtUserWin32PoolAllocationStats", OK, SYSARG_TYPE_UINT32, 6,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserWindowFromPhysicalPoint", OK, DRSYS_TYPE_HANDLE, 1,
     {
         {0, sizeof(POINT), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtUserWindowFromPoint", OK, DRSYS_TYPE_HANDLE, 2,
     {
         {0, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtUserYieldTask", OK, SYSARG_TYPE_UINT32, 0, },
    {{0,0},"NtUserUserConnectToServer", OK, RNTST, 3,
     {
         {0, 0, R|CT, SYSARG_TYPE_CSTRING_WIDE},
         {1, -2, WI},
         {2, sizeof(ULONG), R|W},
     }
    },

    /***************************************************/
    /* FIXME i#1095: fill in the unknown info, esp Vista+ */
    {{0,0},"NtUserCallUserpExitWindowsEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserCallUserpRegisterLogonProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,WIN8},"NtUserDeviceEventWorker", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserEndTask", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserLogon", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserRegisterServicesProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Vista */
    /* XXX: add min OS version: but we have to distinguish the service packs! */
    /* XXX: NtUserGetProp's return value should match GetProp == HANDLE, but it
     * returns -1 and pointer-looking values in addition to NULL and 2, so I'm
     * not sure what it is -- the type may vary.
     */
    {{0,0},"NtUserGetProp", OK, DRSYS_TYPE_UNKNOWN, 2, },
    {{0,0},"NtUserAddClipboardFormatListener", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserCheckAccessForIntegrityLevel", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserCreateDesktopEx", UNKNOWN, DRSYS_TYPE_HANDLE, 6,
     {
         /* we figured some out but don't know others */
         {3, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUserDoSoundConnect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserDoSoundDisconnect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserDwmGetDxRgn", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserDwmHintDxUpdate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserDwmStartRedirection", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserDwmStopRedirection", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserEndTouchOperation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserFrostCrashedWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserGetUpdatedClipboardFormats", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserGetWindowMinimizeRect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserGetWindowRgnEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserGhostWindowFromHungWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserHungWindowFromGhostWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserInternalGetWindowIcon", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserLogicalToPhysicalPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserOpenThreadDesktop", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserPaintMonitor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserPhysicalToLogicalPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserRegisterErrorReportingDialog", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserRegisterSessionPort", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserRemoveClipboardFormatListener", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserSetMirrorRendering", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,WIN8}, "NtUserSetProcessDPIAware", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN81,0},"NtUserSetProcessDPIAwareness", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{0,0},"NtUserSetWindowRgnEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserShowSystemCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserShutdownBlockReasonCreate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserShutdownBlockReasonDestroy", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserShutdownBlockReasonQuery", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserUnregisterSessionPort", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{0,0},"NtUserUpdateWindowTransform", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Win7 */
    {{WIN7,0},"NtUserCalculatePopupWindowPosition", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserChangeWindowMessageFilterEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserDesktopHasWatermarkText", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserDisplayConfigGetDeviceInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserDisplayConfigSetDeviceInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetDisplayConfigBufferSizes", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetGestureConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetGestureExtArgs", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetGestureInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetInputLocaleInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetTopLevelWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetTouchInputInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetWindowCompositionAttribute", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetWindowCompositionInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserGetWindowDisplayAffinity", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserHwndQueryRedirectionInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserHwndSetRedirectionInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserInjectGesture", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserIsTopLevelWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserIsTouchWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserMagControl", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserMagGetContextInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserMagSetContextInformation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserManageGestureHandlerWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserModifyWindowTouchCapability", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserQueryDisplayConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSendTouchInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSetChildWindowNoActivate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSetDisplayConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSetGestureConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSetWindowCompositionAttribute", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSetWindowDisplayAffinity", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDestroyLogicalSurfaceBinding", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxBindSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxGetSwapChainStats", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxOpenSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxQuerySwapChainBindingStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxReleaseSwapChain", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxReportPendingBindingsToDwm", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxSetSwapChainBindingStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmDxSetSwapChainStats", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN7,0},"NtUserSfmGetLogicalSurfaceBinding", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

    /***************************************************/
    /* Added in Win8 */
    /* FIXME i#1153: fill in details */
    {{WIN8,0},"NtUserAcquireIAMKey", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserAutoPromoteMouseInPointer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserCanBrokerForceForeground", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserCheckProcessForClipboardAccess", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserCheckProcessSession", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,WIN8}, "NtUserCreateDCompositionHwndTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN81,0},"NtUserCreateDCompositionHwndTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserDeferWindowPosAndBand", UNKNOWN, DRSYS_TYPE_UNKNOWN, 10, },
    {{WIN8,0},"NtUserDelegateCapturePointers", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserDelegateInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 6, },
    {{WIN8,0},"NtUserDestroyDCompositionHwndTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserDisableImmersiveOwner", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserDisableProcessWindowFiltering", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserDiscardPointerFrameMessages", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserDwmGetRemoteSessionOcclusionEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserDwmGetRemoteSessionOcclusionState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserDwmValidateWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserEnableIAMAccess", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserEnableMouseInPointer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserEnableMouseInputForCursorSuppression", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserGetAutoRotationState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserGetCIMSSM", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserGetClipboardAccessToken", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetCurrentInputMessageSource", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserGetDesktopID", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetDisplayAutoRotationPreferencesByProcessId", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserGetDisplayAutoRotationPreferences", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,WIN8},"NtUserGetGlobalIMEStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetPointerCursorId", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetPointerDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetPointerDeviceCursors", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserGetPointerDeviceProperties", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserGetPointerDeviceRects", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserGetPointerDevices", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetPointerInfoList", UNKNOWN, DRSYS_TYPE_UNKNOWN, 8, },
    {{WIN8,0},"NtUserGetPointerType", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserGetProcessUIContextInformation", OK, DRSYS_TYPE_BOOL, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PROCESS_UI_CONTEXT), W,},
     }
    },
    {{WIN8,0},"NtUserGetQueueEventStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserGetRawPointerDeviceData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN8,0},"NtUserGetTouchValidationStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserGetWindowBand", OK, DRSYS_TYPE_BOOL, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DWORD), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN8,0},"NtUserGetWindowFeedbackSetting", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN8,0},"NtUserHandleDelegatedInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserHidePointerContactVisualization", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserInitializeTouchInjection", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserInjectTouchInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserInternalClipCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserIsMouseInPointerEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserIsMouseInputEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,WIN8}, "NtUserLayoutCompleted", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN81,0},"NtUserLayoutCompleted", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserPromotePointer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserQueryBSDRWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserRegisterBSDRWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserRegisterEdgy", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserRegisterPointerDeviceNotifications", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserRegisterPointerInputTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,0},"NtUserRegisterTouchHitTestingWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserSendEventMessage", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN8,0},"NtUserSetActiveProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetAutoRotation", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetBrokeredForeground", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetCalibrationData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN8,0},"NtUserSetDisplayAutoRotationPreferences", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetDisplayMapping", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserSetFallbackForeground", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserSetImmersiveBackgroundWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetProcessRestrictionExemption", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN8,0},"NtUserSetProcessUIAccessZorder", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserSetThreadInputBlocked", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserSetWindowBand", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN8,WIN8}, "NtUserSetWindowCompositionTransition", UNKNOWN, DRSYS_TYPE_UNKNOWN, 6, },
    {{WIN81,0},"NtUserSetWindowCompositionTransition", UNKNOWN, DRSYS_TYPE_UNKNOWN, 7, },
    {{WIN8,0},"NtUserSetWindowFeedbackSetting", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN8,0},"NtUserSignalRedirectionStartComplete", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN8,0},"NtUserSlicerControl", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN8,0},"NtUserUndelegateInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserUpdateDefaultDesktopThumbnail", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN8,0},"NtUserWaitAvailableMessageEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN8,0},"NtUserWaitForRedirectionStartComplete", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },

    /***************************************************/
    /* Added in Windows 8.1 */
    /* FIXME i#1360: fill in details */
    {{WIN81,0},"NtUserClearForeground", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN81,0},"NtUserCompositionInputSinkLuidFromPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserEnableTouchPad", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtUserGetCursorDims", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtUserGetDpiForMonitor", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN81,0},"NtUserGetHimetricScaleFactorFromPixelLocation", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN81,0},"NtUserGetOwnerTransformedMonitorRect", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN81,0},"NtUserGetPhysicalDeviceRect", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserGetPointerInputTransform", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN81,0},"NtUserGetPrecisionTouchPadConfiguration", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtUserGetProcessDpiAwareness", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserLinkDpiCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN81,0},"NtUserLogicalToPerMonitorDPIPhysicalPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserPerMonitorDPIPhysicalToLogicalPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserRegisterTouchPadCapable", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtUserReportInertia", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN81,0},"NtUserSetActivationFilter", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN81,0},"NtUserSetPrecisionTouchPadConfiguration", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN81,0},"NtUserTransformPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN81,0},"NtUserTransformRect", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN81,0},"NtUserUpdateWindowInputSinkHints", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },

    /***************************************************/
    /* Added in Windows 10 */
    /* FIXME i#1750: fill in details */
    {{WIN10,0},"NtCreateImplicitCompositionInputSink", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtDCompositionCapturePointer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtDCompositionDuplicateSwapchainHandleToDwm", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtDCompositionEnableDDASupport", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN10,0},"NtDCompositionEnableMMCSS", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtDCompositionGetAnimationTime", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtDCompositionRegisterVirtualDesktopVisual", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtDCompositionSetChannelCallbackId", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtDCompositionSetResourceCallbackId", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtDCompositionSetVisualInputSink", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtDCompositionUpdatePointerCapture", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtDesktopCaptureBits", UNKNOWN, DRSYS_TYPE_UNKNOWN, 8, },
    {{WIN10,0},"NtHWCursorUpdatePointer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtQueryCompositionInputIsImplicit", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtQueryCompositionInputQueueAndTransform", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtQueryCompositionInputSinkViewId", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtRIMAddInputObserver", UNKNOWN, DRSYS_TYPE_UNKNOWN, 7, },
    {{WIN10,0},"NtRIMGetDevicePreparsedDataLockfree", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtRIMObserveNextInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtRIMRemoveInputObserver", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtRIMUpdateInputObserverRegistration", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtSetCompositionSurfaceAnalogExclusive", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtTokenManagerConfirmOutstandingAnalogToken", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN10,0},"NtTokenManagerGetAnalogExclusiveSurfaceUpdates", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN10,0},"NtTokenManagerGetAnalogExclusiveTokenEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtTokenManagerOpenSectionAndEvents", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtUserDwmKernelShutdown", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN10,0},"NtUserDwmKernelStartup", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN10,0},"NtUserEnableChildWindowDpiMessage", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserGetDManipHookInitFunction", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserGetDpiMetrics", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserGetPointerFrameArrivalTimes", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserInitializeInputDeviceInjection", UNKNOWN, DRSYS_TYPE_UNKNOWN, 7, },
    {{WIN10,0},"NtUserInitializePointerDeviceInjection", UNKNOWN, DRSYS_TYPE_UNKNOWN, 5, },
    {{WIN10,0},"NtUserInjectDeviceInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserInjectKeyboardInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserInjectMouseInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserInjectPointerInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserIsChildWindowDpiMessageEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtUserIsWindowBroadcastingDpiToChildren", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtUserNavigateFocus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserRegisterDManipHook", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtUserRegisterManipulationThread", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN10,0},"NtUserRegisterShellPTPListener", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserReleaseDwmHitTestWaiters", UNKNOWN, DRSYS_TYPE_UNKNOWN, 0, },
    {{WIN10,0},"NtUserSetActiveProcessForMonitor", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserSetCoreWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtUserSetCoreWindowPartner", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserSetFeatureReportResponse", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserSetManipulationInputTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 4, },
    {{WIN10,0},"NtUserSetWindowArrangement", UNKNOWN, DRSYS_TYPE_UNKNOWN, 3, },
    {{WIN10,0},"NtUserSetWindowShowState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 2, },
    {{WIN10,0},"NtVisualCaptureBits", UNKNOWN, DRSYS_TYPE_UNKNOWN, 9, },
    /* Added in Windows 10 1511 */
    /* FIXME i#1750: fill in details */
    {{WIN11,0},"NtCompositionSetDropTarget", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtDCompositionAttachMouseWheelToHwnd", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtSetCompositionSurfaceBufferCompositionModeAndOrientation", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtUserRemoveInjectionDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN11,0},"NtUserUpdateWindowTrackingInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    /* Added in Windows 10 1607 */
    /* FIXME i#1750: fill in details */
    {{WIN12,0},"NtDCompositionProcessChannelBatchBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtQueryCompositionSurfaceHDRMetaData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtSetCompositionSurfaceDirectFlipState", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtSetCompositionSurfaceHDRMetaData", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserAcquireInteractiveControlBackgroundAccess", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserBroadcastThemeChangeEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserEnableNonClientDpiScaling", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserGetInteractiveControlDeviceInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserGetInteractiveControlInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserGetProcessDpiAwarenessContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserGetQueueStatusReadonly", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserInheritWindowMonitor", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserInteractiveControlQueryUsage", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserIsNonClientDpiScalingEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSendInteractiveControlHapticsReport", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSetInteractiveControlFocus", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSetInteractiveCtrlRotationAngle", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSetProcessDpiAwarenessContext", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSetProcessInteractionFlags", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    {{WIN12,0},"NtUserSystemParametersInfoForDpi", UNKNOWN, DRSYS_TYPE_UNKNOWN, 1, },
    /* Added in Windows 10 1703 */
    /* FIXME i#1750: fill in details */
    {{WIN13,0},"NtDCompositionCommitSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtDCompositionCreateSharedVisualHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtDCompositionSetChildRootVisual", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITActivateInputProcessing", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITBindInputTypeToMonitors", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITCoreMsgKGetConnectionHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITCoreMsgKOpenConnectionTo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITCoreMsgKSend", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITDeactivateInputProcessing", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITDisableMouseIntercept", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITEnableMouseIntercept", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITSetInputCallbacks", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITSynthesizeMouseInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITSynthesizeMouseWheel", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITSynthesizeTouchInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITUpdateInputGlobals", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtMITWaitForMultipleObjectsEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMAreSiblingDevices", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMDeviceIoControl", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMFreeInputBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMGetDevicePreparsedData", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMGetDeviceProperties", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMGetDevicePropertiesLockfree", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMGetPhysicalDeviceRect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMGetSourceProcessId", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMOnPnpNotification", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMOnTimerNotification", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMReadInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMRegisterForInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMSetTestModeStatus", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtRIMUnregisterForInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtSetCompositionSurfaceBufferUsage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserBeginLayoutUpdate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserCompositionInputSinkViewInstanceIdFromPoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserConfirmResizeCommit", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserEnableResizeLayoutSynchronization", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserEnableWindowGDIScaledDpiMessage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserEnableWindowResizeOptimization", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserFunctionalizeDisplayConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserGetInteractiveCtrlSupportedWaveforms", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserGetResizeDCompositionSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserInitializeGenericHidInjection", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserInitializePointerDeviceInjectionEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserInjectGenericHidInput", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserIsResizeLayoutSynchronizationEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserIsWindowGDIScaledDpiMessageEnabled", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserLockCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserProcessInkFeedbackCommand", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN13,0},"NtUserSetDialogControlDpiChangeBehavior", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    /* Added in Windows 10 1709 */
    /* FIXME i#1750: fill in details */
    {{WIN14,0},"NtDWMBindCursorToOutputConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtDWMCommitInputSystemOutputConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtDWMSetCursorOrientation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtDWMSetInputSystemOutputConfig", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtFlipObjectAddPoolBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtFlipObjectCreate", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtFlipObjectOpen", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtFlipObjectRemovePoolBuffer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtMITGetCursorUpdateHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtRIMEnableMonitorMappingForDevice", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserCreateEmptyCursorObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserGetActiveProcessesDpis", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserGetCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserGetDpiForCurrentProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserGetHDevName", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserMsgWaitForMultipleObjectsEx", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserReleaseDC", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserSetCursorPos", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserSetDesktopColorTransform", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserSetTargetForResourceBrokering", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserShowCursor", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserStopAndEndInertia", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserWOWCleanup", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN14,0},"NtUserWindowFromDC", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    /* Added in Windows 10 1803 */
    /* FIXME i#1750: fill in details */
    {{WIN15,0},"NtCloseCompositionInputSink", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDCompositionCreateSynchronizationObject", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDCompositionGetBatchId", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDCompositionSuspendAnimations", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkGetProcessList", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkRegisterVailProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkSubmitPresentBltToHwQueue", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkVailConnect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkVailDisconnect", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtDxgkVailPromoteCompositionSurface", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtEnableOneCoreTransformMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectAddContent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerAcquirePresent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerAdjustUsageReference", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerBeginProcessPresent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerEndProcessPresent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerPostMessage", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectConsumerQueryBufferInfo", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectDisconnectEndpoint", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectQueryBufferAvailableEvent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectQueryEndpointConnected", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectQueryNextMessageToProducer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectReadNextMessageToProducer", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectRemoveContent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtFlipObjectSetContent", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtIsOneCoreTransformMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtMITDispatchCompletion", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtMITSetInputDelegationMode", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtMITSetLastInputRecipient", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtMapVisualRelativePoints", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtTokenManagerCreateFlipObjectReturnTokenHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtTokenManagerCreateFlipObjectTokenHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserCreatePalmRejectionDelayZone", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserCreateWindowGroup", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserDeleteWindowGroup", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserDestroyPalmRejectionDelayZone", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserEnableSoftwareCursorForScreenCapture", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserEnableWindowGroupPolicy", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserGetMonitorBrightness", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserGetOemBitmapSize", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserGetSystemDpiForProcess", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserGetWindowGroupId", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserGetWindowProcessHandle", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserLogicalToPhysicalDpiPointForWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserPhysicalToLogicalDpiPointForWindow", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserRequestMoveSizeOperation", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserSetBridgeWindowChild", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserSetDimUndimTransitionTime", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserSetMonitorBrightness", UNKNOWN, DRSYS_TYPE_UNKNOWN, },
    {{WIN15,0},"NtUserSetWindowGroup", UNKNOWN, DRSYS_TYPE_UNKNOWN, },

};
#define NUM_USER32_SYSCALLS \
    (sizeof(syscall_user32_info)/sizeof(syscall_user32_info[0]))

size_t
num_user32_syscalls(void)
{
    return NUM_USER32_SYSCALLS;
}
