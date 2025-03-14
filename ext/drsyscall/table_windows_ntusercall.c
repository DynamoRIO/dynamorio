/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
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

#include "../wininc/ndk_extypes.h" /* required by ntuser.h */
#include "../wininc/ntuser.h"
#include "../wininc/ntuser_win8.h"

/* Secondary system calls for NtUserCall{No, One, Two}Param */
/* FIXME i#1094: the official return type is DWORD_PTR but it would be more useful
 * to give the actual types
 */
/* FIXME i#1153: Windows 8 added some syscalls we do not have details for */
/* FIXME i#1360: Windows 8.1 added some syscalls we do not have details for */
/* FIXME i#1750: Windows 10+ added some syscalls we do not have details for */

/* FIXME i#1089: fill in info on all the inlined args for all of
 * syscalls in this file.
 */

/* These entries require special handling since related sys numbers are not 0 through N.
 * We use special callback routine wingdi_get_secondary_syscall_num to return secondary
 * number using syscall name & primary number.
 */
syscall_info_t syscall_UserCallNoParam_info[] = {
    {{0,0},"NtUserCallNoParam.CREATEMENU", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.CREATEMENUPOPUP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.DISABLEPROCWNDGHSTING", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.MSQCLEARWAKEMASK", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.ALLOWFOREGNDACTIVATION", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.CREATESYSTEMTHREADS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.DESKTOPHASWATERMARK", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.DESTROY_CARET", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.GETDEVICECHANGEINFO", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.GETIMESHOWSTATUS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.GETINPUTDESKTOP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.GETMSESSAGEPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.GETREMOTEPROCID", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN8,0},"NtUserCallNoParam.GETUNPREDICTEDMESSAGEPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.HIDECURSORNOCAPTURE", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.LOADCURSANDICOS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN8,0},"NtUserCallNoParam.ISQUEUEATTACHED", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.PREPAREFORLOGOFF", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.RELEASECAPTURE", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.RESETDBLCLICK", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.ZAPACTIVEANDFOUS", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTECONSHDWSTOP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTEDISCONNECT", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTELOGOFF", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTENTSECURITY", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTESHDWSETUP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTESHDWSTOP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTEPASSTHRUENABLE", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTEPASSTHRUDISABLE", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.REMOTECONNECTSTATE", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.UPDATEPERUSERIMMENABLING", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,WIN13},"NtUserCallNoParam.USERPWRCALLOUTWORKER", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.WAKERITFORSHTDWN", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.INIT_MESSAGE_PUMP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.UNINIT_MESSAGE_PUMP", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{0,0},"NtUserCallNoParam.LOADUSERAPIHOOK", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN81,0},"NtUserCallNoParam.ENABLEMIPSHELLTHREAD", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN81,WIN13},"NtUserCallNoParam.ISMIPSHELLTHREADENABLED", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN8,0},"NtUserCallNoParam.DEFERREDDESKTOPROTATION", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {{WIN10,0},"NtUserCallNoParam.ENABLEPERMONITORMENUSCALING", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallNoParam.UNKNOWN", OK, DRSYS_TYPE_UNSIGNED_INT, 1, },
};

syscall_info_t syscall_UserCallOneParam_info[] = {
    {{0,0},"NtUserCallOneParam.BEGINDEFERWNDPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*int count.  allocates memory but in the kernel*/},
    {{0,0},"NtUserCallOneParam.GETSENDMSGRECVR", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,WIN13},"NtUserCallOneParam.WINDOWFROMDC", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*HDC*/},
    {{0,0},"NtUserCallOneParam.ALLOWSETFOREGND", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,WIN13},"NtUserCallOneParam.CREATEEMPTYCUROBJECT", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*unused*/},
    {{0,0},"NtUserCallOneParam.CREATESYSTEMTHREADS", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*UINT*/},
    {{0,0},"NtUserCallOneParam.CSDDEUNINITIALIZE", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.DIRECTEDYIELD", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.ENUMCLIPBOARDFORMATS", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*UINT*/},
    {{0,0},"NtUserCallOneParam.GETCURSORPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 2,
     {
         {0, sizeof(POINTL), W},
     }
    },
    {{WIN10,0},"NtUserCallOneParam.FORCEFOCUSBASEDMOUSEWHEELROUTING", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.GETINPUTEVENT", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*DWORD*/},
    {{0,0},"NtUserCallOneParam.GETKEYBOARDLAYOUT", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*DWORD*/},
    {{0,0},"NtUserCallOneParam.GETKEYBOARDTYPE", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*DWORD*/},
    {{0,0},"NtUserCallOneParam.GETPROCDEFLAYOUT", OK, DRSYS_TYPE_UNSIGNED_INT, 2,
     {
         {0, sizeof(DWORD), W},
     }
    },
    {{0,0},"NtUserCallOneParam.GETQUEUESTATUS", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*DWORD*/},
    {{0,0},"NtUserCallOneParam.GETWINSTAINFO", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.HANDLESYSTHRDCREATFAIL", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.LOCKFOREGNDWINDOW", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.LOADFONTS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.MAPDEKTOPOBJECT", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.MESSAGEBEEP", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*LPARAM*/},
    {{0,0},"NtUserCallOneParam.PLAYEVENTSOUND", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.POSTQUITMESSAGE", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*int exit code*/},
    {{0,0},"NtUserCallOneParam.PREPAREFORLOGOFF", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.REALIZEPALETTE", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*HDC*/},
    {{0,0},"NtUserCallOneParam.REGISTERLPK", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.REGISTERSYSTEMTHREAD", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.REMOTERECONNECT", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.REMOTETHINWIRESTATUS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,WIN13},"NtUserCallOneParam.RELEASEDC", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*HDC*/
     {
         {0,}
     }
    },
    {{0,0},"NtUserCallOneParam.REMOTENOTIFY", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.REPLYMESSAGE", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*LRESULT*/},
    {{0,0},"NtUserCallOneParam.SETCARETBLINKTIME", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*UINT*/},
    {{0,0},"NtUserCallOneParam.SETDBLCLICKTIME", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.SETIMESHOWSTATUS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.SETMESSAGEEXTRAINFO", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*LPARAM*/},
    {{0,0},"NtUserCallOneParam.SETPROCDEFLAYOUT", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*DWORD for PROCESSINFO.dwLayout*/},
    {{0,0},"NtUserCallOneParam.SETWATERMARKSTRINGS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,WIN13},"NtUserCallOneParam.SHOWCURSOR", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*BOOL*/},
    {{0,0},"NtUserCallOneParam.SHOWSTARTGLASS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.SWAPMOUSEBUTTON", OK, DRSYS_TYPE_UNSIGNED_INT, 2, /*BOOL*/},

    {{0,0},"NtUserCallOneParam.WOWMODULEUNLOAD", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{0,0},"NtUserCallOneParam.UNKNOWNA", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN8,0},"NtUserCallOneParam.DWMLOCKSCREENUPDATES", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN8,0},"NtUserCallOneParam.ENABLESESSIONFORMMCSS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN8,0},"NtUserCallOneParam.UNKNOWNB", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN8,0},"NtUserCallOneParam.ISTHREADMESSAGEQUEUEATTACHED", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN81,0},"NtUserCallOneParam.POSTUIACTIONS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN12,0},"NtUserCallOneParam.SETINPUTSERVICESTATE", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN14,0},"NtUserCallOneParam.GETDPIDEPENDENTMETRIC", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {{WIN13,0},"NtUserCallOneParam.FORCEENABLENUMPADTRANSLATION", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallOneParam.UNKNOWN", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 2, },
};

syscall_info_t syscall_UserCallHwnd_info[] = {
    {{0,0},"NtUserCallHwnd.DEREGISTERSHELLHOOKWINDOW", OK, SYSARG_TYPE_UINT32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwnd.DWP_GETENABLEDPOPUP", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
    {{0,0},"NtUserCallHwnd.GETWNDCONTEXTHLPID", OK, SYSARG_TYPE_UINT32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwnd.REGISTERSHELLHOOKWINDOW", OK, SYSARG_TYPE_UINT32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwnd.UNKNOWNA", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
    {{WIN10,0},"NtUserCallHwnd.UNKNOWNB", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
    {{WIN12,0},"NtUserCallHwnd.UNKNOWNC", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
    {{WIN13,0},"NtUserCallHwnd.UNKNOWND", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallHwnd.UNKNOWN", UNKNOWN, SYSARG_TYPE_UINT32, 2, },
};

syscall_info_t syscall_UserCallHwndOpt_info[] = {
    {{0,0},"NtUserCallHwndOpt.SETPROGMANWINDOW", OK, SYSARG_TYPE_UINT32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndOpt.SETTASKMANWINDOW", OK, SYSARG_TYPE_UINT32, 2, /*HWND*/},
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallHwndOpt.UNKNOWN", UNKNOWN, SYSARG_TYPE_UINT32, 2, /*HWND*/},
};

syscall_info_t syscall_UserCallHwndParam_info[] = {
    {{0,0},"NtUserCallHwndParam.GETCLASSICOCUR", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{0,0},"NtUserCallHwndParam.CLEARWINDOWSTATE", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{0,0},"NtUserCallHwndParam.KILLSYSTEMTIMER", OK, SYSARG_TYPE_UINT32, 3, /*HWND, timer id*/},
    {{WIN13,0},"NtUserCallHwndParam.NOTIFYOVERLAYWINDOW", OK, SYSARG_TYPE_UINT32, 3,},
    {{WIN13,0},"NtUserCallHwndParam.REGISTERKBDCORRECTION", OK, SYSARG_TYPE_UINT32, 3,},
    {{0,0},"NtUserCallHwndParam.SETDIALOGPOINTER", OK, SYSARG_TYPE_UINT32, 3, /*HWND, BOOL*/ },
    {{0,0},"NtUserCallHwndParam.SETVISIBLE", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{0,0},"NtUserCallHwndParam.SETWNDCONTEXTHLPID", OK, SYSARG_TYPE_UINT32, 3, /*HWND, HANDLE*/},
    {{WIN81,0},"NtUserCallHwndParam.UNKNOWNA", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{0,0},"NtUserCallHwndParam.SETWINDOWSTATE", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{WIN10,0},"NtUserCallHwndParam.UNKNOWNB", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{WIN10,0},"NtUserCallHwndParam.REGISTERWINDOWARRANGEMENTCALLOUT", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {{WIN11,0},"NtUserCallHwndParam.ENABLEMODERNAPPWINDOWKBDINTERCEPT", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallHwndParam.UNKNOWN", UNKNOWN, SYSARG_TYPE_UINT32, 3, },
};

syscall_info_t syscall_UserCallHwndLock_info[] = {
    /* XXX: confirm the rest: assuming for now all just take HWND */
    {{0,0},"NtUserCallHwndLock.WINDOWHASSHADOW", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.ARRANGEICONICWINDOWS", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.DRAWMENUBAR", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.CHECKIMESHOWSTATUSINTHRD", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.GETSYSMENUHANDLE", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.REDRAWFRAME", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.REDRAWFRAMEANDHOOK", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.SETDLGSYSMENU", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.SETFOREGROUNDWINDOW", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.SETSYSMENU", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.UPDATECKIENTRECT", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.UPDATEWINDOW", OK, SYSARG_TYPE_BOOL32, 2, /*HWND*/},
    {{0,0},"NtUserCallHwndLock.SETACTIVEIMMERSIVEWINDOW", UNKNOWN, SYSARG_TYPE_BOOL32, 2, },
    {{WIN10,0},"NtUserCallHwndLock.GETWINDOWTRACKINFOASYNC", UNKNOWN, SYSARG_TYPE_BOOL32, 2, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallHwndLock.UNKNOWN", UNKNOWN, SYSARG_TYPE_BOOL32, 2, },
};

syscall_info_t syscall_UserCallTwoParam_info[] = {
    {{WIN81,0},"NtUserCallTwoParam.UNKNOWNA", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.CHANGEWNDMSGFILTER", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.GETCURSORPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {0, sizeof(POINTL), W},}/*other param is hardcoded as 0x1*/},
    /* XXX i#996: not 100% sure there's not more nuanced behavior to
     * this syscall.  First param looks like flags and 3rd looks like
     * size of buffer.
     */
    {{0,0},"NtUserCallTwoParam.GETHDEVNAME", OK, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {1, -2, W},
     }
    },
    {{0,0},"NtUserCallTwoParam.INITANSIOEM", OK, DRSYS_TYPE_UNSIGNED_INT, 3,
     {
         {1,0, W|CT, SYSARG_TYPE_CSTRING_WIDE},
     }
    },
    {{0,0},"NtUserCallTwoParam.NLSSENDIMENOTIFY", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.REGISTERGHSTWND", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.REGISTERLOGONPROCESS", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*HANDLE, BOOL*/},
    {{0,0},"NtUserCallTwoParam.REGISTERSYSTEMTHREAD", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.REGISTERSBLFROSTWND", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.REGISTERUSERHUNGAPPHANDLERS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.SHADOWCLEANUP", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.REMOTESHADOWSTART", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.SETCARETPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*int, int*/},
    {{0,WIN13},"NtUserCallTwoParam.SETCURSORPOS", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*int, int*/},
    {{WIN14,0},"NtUserCallTwoParam.SETTHREADQUEUEMERGESETTING", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.SETPHYSCURSORPOS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallTwoParam.UNHOOKWINDOWSHOOK", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*int, HOOKPROC*/},
    {{0,0},"NtUserCallTwoParam.WOWCLEANUP", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{WIN10,0},"NtUserCallTwoParam.ENABLESHELLWINDOWMGT", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{WIN11,0},"NtUserCallTwoParam.SCALESYSTEMMETRICFORDPI", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallTwoParam.UNKNOWN", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
};

syscall_info_t syscall_UserCallHwndParamLock_info[] = {
    {{WIN8,0},"NtUserCallHwndParamLock.UNKNOWNA", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallHwndParamLock.ENABLEWINDOW", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*HWND, BOOL*/},
    {{WIN10,0},"NtUserCallHwndParamLock.SETMODERNAPPWINDOW", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallHwndParamLock.REDRAWTITLE", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallHwndParamLock.SHOWOWNEDPOPUPS", OK, DRSYS_TYPE_UNSIGNED_INT, 3, /*HWND, BOOL*/},
    {{0,0},"NtUserCallHwndParamLock.SWITCHTOTHISWINDOW", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallHwndParamLock.UPDATEWINDOWS", UNKNOWN, DRSYS_TYPE_UNSIGNED_INT, 3, },
    {{0,0},"NtUserCallHwndParamLock.VALIDATERGN", OK, SYSARG_TYPE_UINT32, 3, /*HWND, HRGN*/},
    {SECONDARY_TABLE_ENTRY_MAX_NUMBER},
    {{0,0},"NtUserCallHwndParamLock.UNKNOWN", UNKNOWN, SYSARG_TYPE_UINT32, 3, /*HWND, HRGN*/},
};
