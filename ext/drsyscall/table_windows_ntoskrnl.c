/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"

#include "../wininc/ndk_dbgktypes.h"
#include "../wininc/ndk_iotypes.h"
#include "../wininc/ndk_extypes.h"
#include "../wininc/ndk_psfuncs.h"
#include "../wininc/ndk_ketypes.h"
#include "../wininc/ndk_lpctypes.h"
#include "../wininc/ndk_mmtypes.h"
#include "../wininc/afd_shared.h"
#include "../wininc/msafdlib.h"
#include "../wininc/winioctl.h"
#include "../wininc/tcpioctl.h"
#include "../wininc/iptypes_undocumented.h"
#include "../wininc/ntalpctyp.h"
#include "../wininc/wdm.h"
#include "../wininc/ntddk.h"
#include "../wininc/ntexapi.h"
#include "../wininc/ntifs.h"
#include "../wininc/ntmmapi.h"
#include "../wininc/tls.h"
#include "../wininc/ktmtypes.h"
#include "../wininc/winnt_recent.h"

#include "table_defines.h"

/* XXX i#97: add IIS syscalls.
 * FIXME i#98: fill in data on still-unknown recently-added Windows syscalls.
 * XXX i#99: my windows syscall data is missing 3 types of information:
 *   - some structs have variable-length data on the end
 *     e.g., PORT_MESSAGE which I do handle today w/ hardcoded support
 *   - some structs have optional fields that don't need to be defined
 *   - need to add post-syscall write size entries: I put in a handful.
 *     should look at all OUT params whose (requested) size comes from an IN param.
 *     e.g., NtQueryValueKey: should use IN param to check addressability, but
 *     DR_PARAM_OUT ResultLength for what was actually written to.
 *     The strategy for these is to use a double entry with the second typically
 *     using WI to indicate that the OUT size needs to be dereferenced (PR 408536).
 *     E.g.:
 *       {0,"NtQuerySecurityObject", 5, 2,-3,W, 2,-4,WI, 4,sizeof(ULONG),W, },
 */
/* Originally generated via:
 *  ./mksystable.pl < ../../win32lore/syscalls/nebbett/ntdll-fix.h | sort
 * (ntdll-fix.h has NTAPI, etc. added to NtNotifyChangeDirectoryFile)
 * Don't forget to re-add the #if 1 below after re-generating
 *
 * Updated version generated via:
 * ./mksystable.pl < ../../win32lore/syscalls/metasploit/metasploit-syscalls-fix.html | sort
 * metasploit-syscalls-fix.html has these changes:
 * - added IN/OUT to NtTranslateFilePath
 * - removed dups (in some cases not clear which alternative was better):
 *   > grep '^Nt' ../../win32lore/syscalls/metasploit/metasploit-syscalls.html | uniq -d
 *   NtAllocateUuids(
 *   NtOpenChannel(
 *   NtPlugPlayControl(
 *   NtReplyWaitSendChannel(
 *   NtSendWaitReplyChannel(
 *   NtSetContextChannel(
 * Also made manual additions for post-syscall write sizes
 * and to set arg size for 0-args syscalls to 0 (xref PR 534421)
 */

extern drsys_sysnum_t sysnum_CreateThread;
extern drsys_sysnum_t sysnum_CreateThreadEx;
extern drsys_sysnum_t sysnum_CreateUserProcess;
extern drsys_sysnum_t sysnum_DeviceIoControlFile;
extern drsys_sysnum_t sysnum_QueryInformationThread;
extern drsys_sysnum_t sysnum_QuerySystemInformation;
extern drsys_sysnum_t sysnum_QuerySystemInformationWow64;
extern drsys_sysnum_t sysnum_QuerySystemInformationEx;
extern drsys_sysnum_t sysnum_SetSystemInformation;
extern drsys_sysnum_t sysnum_SetInformationProcess;
extern drsys_sysnum_t sysnum_PowerInformation;
extern drsys_sysnum_t sysnum_QueryVirtualMemory;
extern drsys_sysnum_t sysnum_FsControlFile;
extern drsys_sysnum_t sysnum_TraceControl;


/* The secondary tables are large, so we separate them into their own file: */
extern syscall_QueryKey_info[];
extern syscall_EnumerateKey_info[];
extern syscall_EnumerateValueKey_info[];
extern syscall_QueryDirectoryFile_info[];
extern syscall_QueryEvent_info[];
extern syscall_QueryVolumeInformationFile_info[];
extern syscall_SetInformationFile_info[];
extern syscall_SetInformationKey_info[];
extern syscall_SetInformationObject_info[];
extern syscall_QueryInformationAtom_info[];
extern syscall_QueryInformationFile_info[];
extern syscall_QueryInformationPort_info[];
extern syscall_QueryIoCompletion_info[];
extern syscall_QueryMutant_info[];
extern syscall_SetVolumeInformationFile_info[];
extern syscall_AlpcQueryInformation_info[];
extern syscall_AlpcQueryInformationMessage_info[];
extern syscall_AlpcSetInformation_info[];
extern syscall_QueryInformationEnlistment_info[];
extern syscall_QueryInformationResourceManager_info[];
extern syscall_QueryInformationTransaction_info[];
extern syscall_QueryInformationTransactionManager_info[];
extern syscall_SetInformationEnlistment_info[];
extern syscall_SetInformationResourceManager_info[];
extern syscall_SetInformationTransaction_info[];
extern syscall_SetInformationTransactionManager_info[];
extern syscall_SetTimerEx_info[];

/* A non-SYSARG_INLINED type is by default DRSYS_TYPE_STRUCT, unless
 * a different type is specified with |HT.
 * So a truly unknown memory type must be explicitly marked DRSYS_TYPE_UNKNOWN.
 */
syscall_info_t syscall_ntdll_info[] = {
    /***************************************************/
    /* Base set from Windows NT, Windows 2000, and Windows XP */
    {{0,0},"NtAcceptConnectPort", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(PORT_VIEW), R|W},
         {5, sizeof(REMOTE_PORT_VIEW), R|W},
     }
    },
    {{0,0},"NtAccessCheck", OK, RNTST, 8,
     {
         {0, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(GENERIC_MAPPING), R},
         {4, sizeof(PRIVILEGE_SET), W},
         {5, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtAccessCheckAndAuditAlarm", OK, RNTST, 11,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {5, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(GENERIC_MAPPING), R},
         {7, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {8, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
         {10, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAccessCheckByType", OK, RNTST, 11,
     {
         {0, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {1, sizeof(SID), R},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(OBJECT_TYPE_LIST), R},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(GENERIC_MAPPING), R},
         {7, sizeof(PRIVILEGE_SET), R},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtAccessCheckByTypeAndAuditAlarm", OK, RNTST, 16,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {5, sizeof(SID), R},
         {6, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(AUDIT_EVENT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(OBJECT_TYPE_LIST), R},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(GENERIC_MAPPING), R},
         {12, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {13, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {14, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
         {15, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAccessCheckByTypeResultList", OK, RNTST, 11,
     {
         {0, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {1, sizeof(SID), R},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(OBJECT_TYPE_LIST), R},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(GENERIC_MAPPING), R},
         {7, sizeof(PRIVILEGE_SET), R},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtAccessCheckByTypeResultListAndAuditAlarm", OK, RNTST, 16,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {5, sizeof(SID), R},
         {6, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(AUDIT_EVENT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(OBJECT_TYPE_LIST), R},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(GENERIC_MAPPING), R},
         {12, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {13, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {14, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
         {15, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAccessCheckByTypeResultListAndAuditAlarmByHandle", OK, RNTST, 17,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {5, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {6, sizeof(SID), R},
         {7, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(AUDIT_EVENT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {10, sizeof(OBJECT_TYPE_LIST), R},
         {11, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {12, sizeof(GENERIC_MAPPING), R},
         {13, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {14, sizeof(ACCESS_MASK), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {15, sizeof(NTSTATUS), W|HT, DRSYS_TYPE_NTSTATUS},
         {16, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAddAtom", OK, RNTST, 3,
     {
         {0, -1, R|HT, DRSYS_TYPE_CWSTRING},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ATOM), W|HT, DRSYS_TYPE_ATOM},
     }
    },
    {{0,0},"NtAddBootEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtAddDriverEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtAdjustGroupsToken", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(TOKEN_GROUPS), R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, W},
         {4, -5, WI},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAdjustPrivilegesToken", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(TOKEN_PRIVILEGES), R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, W},
         {4, -5, WI},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAlertResumeThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAlertThread", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtAllocateLocallyUniqueId", OK, RNTST, 1,
     {
         {0, sizeof(LUID), W},
     }
    },
    {{0,0},"NtAllocateUserPhysicalPages", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAllocateUuids", OK, RNTST, 4,
     {
         {0, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(UCHAR), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAllocateVirtualMemory", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "MEM_COMMIT"},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "PAGE_NOACCESS"},
     }
    },
    {{0,0},"NtApphelpCacheControl", OK, RNTST, 2,
     {
         {0, sizeof(APPHELPCACHESERVICECLASS), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtAreMappedFilesTheSame", OK, RNTST, 2,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
     }
    },
    {{0,0},"NtAssignProcessToJobObject", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCallbackReturn", OK, RNTST, 3,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,WINVISTA},"NtCancelDeviceWakeupRequest", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCancelIoFile", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{0,0},"NtCancelTimer", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtClearEvent", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtClose", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCloseObjectAuditAlarm", OK, RNTST, 3,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCompactKeys", OK, RNTST, 2,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCompareTokens", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCompleteConnectPort", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCompressKey", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* Arg#4 is IN OUT for Nebbett, but not for Metasploit.
     * Arg#6 is of a user-defined format and since IN/OUT but w/ only one
     * capacity/size on IN can easily have capacity be larger than IN size:
     * xref i#494.  Be on the lookout for other false positives.
     */
    {{0,0},"NtConnectPort", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(SECURITY_QUALITY_OF_SERVICE), R|CT, SYSARG_TYPE_SECURITY_QOS},
         {3, sizeof(PORT_VIEW), R|W},
         {4, sizeof(REMOTE_PORT_VIEW), W},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, -7, R|WI},
         {7, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtContinue", OK, RNTST, 2,
     {
         {0, sizeof(CONTEXT), R|CT, SYSARG_TYPE_CONTEXT},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,WINXP},"NtCreateChannel", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtCreateDebugObject", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCreateDirectoryObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtCreateEvent", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(EVENT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCreateEventPair", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtCreateFile", OK, RNTST, 11,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_ATTRIBUTE_READONLY"},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_SHARE_READ"},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_SUPERSEDE"},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_DIRECTORY_FILE"},
         {9, -10, R},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateIoCompletion", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateJobObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtCreateJobSet", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(JOB_SET_ARRAY), R},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateKey", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "REG_OPTION_RESERVED"},
         {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateKeyedEvent", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateMailslotFile", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtCreateMutant", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCreateNamedPipeFile", OK, RNTST, 14,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {9, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {12, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {13, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtCreatePagingFile", OK, RNTST, 4,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {2, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreatePort", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateProcess", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {7, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCreateProcessEx", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {7, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateProfile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(KPROFILE_SOURCE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateSection", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "PAGE_NOACCESS"},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "SEC_FILE"},
         {6, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCreateSemaphore", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtCreateSymbolicLinkObject", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtCreateThread", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(CLIENT_ID), W},
         {5, sizeof(CONTEXT), R|CT, SYSARG_TYPE_CONTEXT},
         {6, sizeof(USER_STACK), R},
         {7, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }, &sysnum_CreateThread
    },
    {{0,0},"NtCreateThreadEx", OK, RNTST, 11,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(PTHREAD_START_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {6, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* 10 is handled manually */
     }, &sysnum_CreateThreadEx
    },
    {{0,0},"NtCreateTimer", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(TIMER_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtCreateToken", OK, RNTST, 13,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(TOKEN_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {4, sizeof(LUID), R},
         {5, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {6, sizeof(TOKEN_USER), R},
         {7, sizeof(TOKEN_GROUPS), R},
         {8, sizeof(TOKEN_PRIVILEGES), R},
         {9, sizeof(TOKEN_OWNER), R},
         {10, sizeof(TOKEN_PRIMARY_GROUP), R},
         {11, sizeof(TOKEN_DEFAULT_DACL), R},
         {12, sizeof(TOKEN_SOURCE), R},
     }
    },
    {{0,0},"NtCreateUserProcess", OK, RNTST, 11,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {5, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {8, sizeof(RTL_USER_PROCESS_PARAMETERS), R},
         /*XXX i#98: arg 9 is in/out but not completely known*/
         {10, sizeof(create_proc_thread_info_t), R/*rest handled manually*/, },
     }, &sysnum_CreateUserProcess
    },
    {{0,0},"NtCreateWaitablePort", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtDebugActiveProcess", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtDebugContinue", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CLIENT_ID), R},
         {2, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtDelayExecution", OK, RNTST, 2,
     {
         {0, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtDeleteAtom", OK, RNTST, 1,
     {
         {0, sizeof(ATOM), SYSARG_INLINED, DRSYS_TYPE_ATOM},
     }
    },
    {{0,0},"NtDeleteBootEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtDeleteDriverEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtDeleteFile", OK, RNTST, 1,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtDeleteKey", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtDeleteObjectAuditAlarm", OK, RNTST, 3,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtDeleteValueKey", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtDeviceIoControlFile", UNKNOWN/*to do param cmp for unknown ioctl codes*/, RNTST, 10,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /*param6 handled manually*/
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, -9, W},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_DeviceIoControlFile
    },
    {{0,0},"NtDisplayString", OK, RNTST, 1,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtDuplicateObject", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {4, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "OBJ_INHERIT"},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "DUPLICATE_CLOSE_SOURCE"},
     }
    },
    {{0,0},"NtDuplicateToken", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(TOKEN_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {5, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtEnumerateBootEntries", OK, RNTST, 2,
     {
         {0, -1, WI},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtEnumerateDriverEntries", OK, RNTST, 2,
     {
         {0, -1, WI},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtEnumerateKey", OK|SYSINFO_SECONDARY_TABLE, RNTST, 6,
     {
         {2,}
     }, (drsys_sysnum_t *)syscall_EnumerateKey_info
    },
    {{0,0},"NtEnumerateSystemEnvironmentValuesEx", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, WI},
         {2, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtEnumerateValueKey", OK|SYSINFO_SECONDARY_TABLE, RNTST, 6,
     {
         {2,}
     }, (drsys_sysnum_t *)syscall_EnumerateValueKey_info
    },
    {{0,0},"NtExtendSection", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtFilterToken", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(TOKEN_GROUPS), R},
         {3, sizeof(TOKEN_PRIVILEGES), R},
         {4, sizeof(TOKEN_GROUPS), R},
         {5, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtFindAtom", OK, RNTST, 3,
     {
         {0, -1, R|HT, DRSYS_TYPE_CWSTRING},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ATOM), W|HT, DRSYS_TYPE_ATOM},
     }
    },
    {{0,0},"NtFlushBuffersFile", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{0,0},"NtFlushInstructionCache", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtFlushKey", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtFlushVirtualMemory", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG_PTR), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{0,0},"NtFlushWriteBuffer", OK, RNTST, 0, },
    {{0,0},"NtFreeUserPhysicalPages", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtFreeVirtualMemory", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "MEM_COMMIT"},
     }
    },
    {{0,0},"NtFsControlFile", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* The "{6, -7, R}" param can have padding inside it and is special-cased */
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, -9, W},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_FsControlFile
    },
    {{0,0},"NtGetContextThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CONTEXT), W|CT, SYSARG_TYPE_CONTEXT},
     }
    },
    {{0,0},"NtGetCurrentProcessorNumber", OK, RNTST, 0, },
    {{0,0},"NtGetDevicePowerState", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DEVICE_POWER_STATE), W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,WIN7},"NtGetPlugPlayEvent", OK, RNTST, 4,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, W},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* BufferEntries is #elements, not #bytes */
    {{0,0},"NtGetWriteWatch", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -5, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(void*)},
         {5, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtImpersonateAnonymousToken", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtImpersonateClientOfPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,0},"NtImpersonateThread", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(SECURITY_QUALITY_OF_SERVICE), R|CT, SYSARG_TYPE_SECURITY_QOS},
     }
    },
    {{0,0},"NtInitializeRegistry", OK, RNTST, 1,
     {
         {0, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtInitiatePowerAction", OK, RNTST, 4,
     {
         {0, sizeof(POWER_ACTION), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(SYSTEM_POWER_STATE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtIsProcessInJob", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtIsSystemResumeAutomatic", OK, RNTST, 0, },
    {{0,WINXP},"NtListenChannel", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CHANNEL_MESSAGE), W},
     }
    },
    {{0,0},"NtListenPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), W|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,0},"NtLoadDriver", OK, RNTST, 1,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtLoadKey", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtLoadKey2", OK, RNTST, 3,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtLoadKeyEx", OK, RNTST, 4,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtLockFile", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {6, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {9, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtLockProductActivationKeys", OK, RNTST, 2,
     {
         {0, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtLockRegistryKey", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtLockVirtualMemory", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtMakePermanentObject", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtMakeTemporaryObject", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtMapCMFModule", OK, RNTST, 6,
     {
         /* XXX DRi#415 not all known */
         {4, sizeof(PVOID), W|HT, DRSYS_TYPE_POINTER},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtMapUserPhysicalPages", OK, RNTST, 3,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {1, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtMapUserPhysicalPagesScatter", OK, RNTST, 3,
     {
         {0, sizeof(PVOID), R|HT, DRSYS_TYPE_POINTER},
         {1, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtMapViewOfSection", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(LARGE_INTEGER), R|W|HT, DRSYS_TYPE_LARGE_INTEGER},
         {6, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(SECTION_INHERIT), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "MEM_COMMIT"},
         {9, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "PAGE_NOACCESS"},
     }
    },
    {{0,0},"NtModifyBootEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtModifyDriverEntry", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtNotifyChangeDirectoryFile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(FILE_NOTIFY_INFORMATION), W},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtNotifyChangeKey", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "REG_NOTIFY_CHANGE_NAME"},
         {6, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {7, -8, R},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtNotifyChangeMultipleKeys", OK, RNTST, 12,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {6, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {9, -10, R},
         {10, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {11, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,WINXP},"NtOpenChannel", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenDirectoryObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenEvent", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenEventPair", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenFile", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_SHARE_READ"},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "FILE_DIRECTORY_FILE"},
     }
    },
    {{0,0},"NtOpenIoCompletion", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenJobObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenKey", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenKeyEx", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "REG_OPTION_RESERVED"},
     }
    },
    {{0,0},"NtOpenKeyedEvent", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenMutant", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenObjectAuditAlarm", OK, RNTST, 12,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         /* XXX: not a regular HANDLE?  ditto NtAccessCheck* */
         {1, sizeof(PVOID), R|HT, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
         {5, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {6, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(PRIVILEGE_SET), R},
         {9, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {10, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {11, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtOpenProcess", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(CLIENT_ID), R},
     }
    },
    {{0,0},"NtOpenProcessToken", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenProcessTokenEx", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "OBJ_INHERIT"},
         {3, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenSection", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenSemaphore", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenSymbolicLinkObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenThread", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(CLIENT_ID), R},
     }
    },
    {{0,0},"NtOpenThreadToken", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenThreadTokenEx", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "OBJ_INHERIT"},
         {4, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenTimer", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtPlugPlayControl", OK, RNTST, 4,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, W},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtPowerInformation", OK, RNTST, 5,
     {
         {0, sizeof(POWER_INFORMATION_LEVEL), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         /* Some info classes do not need to define every field of the input buffer
          * (i#1247), necessitating special-casing instead of listing "{1, -2, R}" here.
          * We still list an entry (w/ default struct type) for non-memarg iterator.
          */
         {1, -2, SYSARG_NON_MEMARG, },
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_PowerInformation
    },
    {{0,0},"NtPrivilegeCheck", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PRIVILEGE_SET), R},
         {2, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtPrivilegedServiceAuditAlarm", OK, RNTST, 5,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(PRIVILEGE_SET), R},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtPrivilegeObjectAuditAlarm", OK, RNTST, 6,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(PRIVILEGE_SET), R},
         {5, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtProtectVirtualMemory", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "PAGE_NOACCESS"},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtPulseEvent", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryAttributesFile", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(FILE_BASIC_INFORMATION), W},
     }
    },
    {{0,0},"NtQueryBootEntryOrder", OK, RNTST, 2,
     {
         {0, -1, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryBootOptions", OK, RNTST, 2,
     {
         {0, -1, WI},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT}
     }
    },
    {{0,0},"NtQueryDebugFilterState", OK, RNTST, 2,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryDefaultLocale", OK, RNTST, 2,
     {
         {0, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {1, sizeof(LCID), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryDefaultUILanguage", OK, RNTST, 1,
     {
         {0, sizeof(LANGID), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryDirectoryFile", OK|SYSINFO_SECONDARY_TABLE, RNTST, 11,
     {
         {7,}
     }, (drsys_sysnum_t *)syscall_QueryDirectoryFile_info
    },
    {{0,0},"NtQueryDirectoryObject", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W},
         {1, -6, WI},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryDriverEntryOrder", OK, RNTST, 2,
     {
         {0, -1, WI|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryEaFile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(FILE_FULL_EA_INFORMATION), W},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(FILE_GET_EA_INFORMATION), R},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtQueryEvent", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryEvent_info
    },
    {{0,0},"NtQueryFullAttributesFile", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(FILE_NETWORK_OPEN_INFORMATION), W},
     }
    },
    {{0,0},"NtQueryInformationAtom", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryInformationAtom_info
    },
    {{0,0},"NtQueryInformationFile", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {4,}
     }, (drsys_sysnum_t *)syscall_QueryInformationFile_info
    },
    {{0,0},"NtQueryInformationJobObject", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(JOBOBJECTINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryInformationPort", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryInformationPort_info
    },
    {{0,0},"NtQueryInformationProcess", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PROCESSINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryInformationThread", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(THREADINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_QueryInformationThread
    },
    {{0,0},"NtQueryInformationToken", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(TOKEN_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryInstallUILanguage", OK, RNTST, 1,
     {
         {0, sizeof(LANGID), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryIntervalProfile", OK, RNTST, 2,
     {
         {0, sizeof(KPROFILE_SOURCE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryIoCompletion", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryIoCompletion_info
    },
    {{0,0},"NtQueryKey", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
       {1,}
     }, (drsys_sysnum_t *)syscall_QueryKey_info
    },
    {{0,0},"NtQueryMultipleValueKey", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(KEY_VALUE_ENTRY), R|W},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, WI},
         {4, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryMutant", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryMutant_info
    },
    {{0,0},"NtQueryObject", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,WINNT},"NtQueryOleDirectoryFile", OK, RNTST, 11,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, -6, W},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(FILE_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {9, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {10, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtQueryOpenSubKeys", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryOpenSubKeysEx", OK, RNTST, 4,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryPerformanceCounter", OK, RNTST, 2,
     {
         {0, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
         {1, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtQueryPortInformationProcess", OK, RNTST, 0, },
    {{0,0},"NtQueryQuotaInformationFile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(FILE_USER_QUOTA_INFORMATION), W},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(FILE_QUOTA_LIST_INFORMATION), R},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(SID), R},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtQuerySection", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SECTION_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQuerySecurityObject", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SECURITY_INFORMATION), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "OWNER_SECURITY_INFORMATION"},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQuerySemaphore", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SEMAPHORE_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* No double entry for 3rd param needed b/c the written size is in
     * .Length of the UNICODE_STRING as well as returned in the param:
     */
    {{0,0},"NtQuerySymbolicLinkObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), W|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQuerySystemEnvironmentValue", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 4,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, -2, W},
         {1, -3, WI},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQuerySystemEnvironmentValueEx", OK, RNTST, 5,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(GUID), R},
         {2, -3, WI},
         {3, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* One info class reads data, which is special-cased */
    {{0,0},"NtQuerySystemInformation", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 4,
     {
         {0, sizeof(SYSTEM_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -2, W},
         {1, -3, WI},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_QuerySystemInformation
    },
    {{0,0},"NtQuerySystemTime", OK, RNTST, 1,
     {
         {0, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtQueryTimer", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(TIMER_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W},
         {2, -4, WI},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryTimerResolution", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryValueKey", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(KEY_VALUE_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, -4, W},
         {3, -5, WI},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryVirtualMemory", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {2, sizeof(MEMORY_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, -4, W},
         {3, -5, WI},
         {4, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(SIZE_T), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_QueryVirtualMemory
    },
    {{0,0},"NtQueryVolumeInformationFile", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
       {4,}
     }, (drsys_sysnum_t *)syscall_QueryVolumeInformationFile_info
    },
    {{0,0},"NtQueueApcThread", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PKNORMAL_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
     }
    },
    {{0,0},"NtRaiseException", OK, RNTST, 3,
     {
         {0, sizeof(EXCEPTION_RECORD), R|CT, SYSARG_TYPE_EXCEPTION_RECORD},
         {1, sizeof(CONTEXT), R|CT, SYSARG_TYPE_CONTEXT},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtRaiseHardError", OK, RNTST, 6,
     {
         {0, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG_PTR), R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReadFile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, -6, W|HT, DRSYS_TYPE_VOID},
         {5, -4,(W|IO)|HT, DRSYS_TYPE_VOID},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReadFileScatter", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(FILE_SEGMENT_ELEMENT), R},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReadRequestData", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W},
         {3, -5, WI},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReadVirtualMemory", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, -3, W|HT, DRSYS_TYPE_VOID},
         {2, -4, WI|HT, DRSYS_TYPE_VOID},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtRegisterThreadTerminatePort", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtReleaseKeyedEvent", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtReleaseMutant", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReleaseSemaphore", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(LONG), W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtRemoveIoCompletion", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(void*), W|HT, DRSYS_TYPE_VOID},/* see i#1536 */
         {3, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtRemoveProcessDebug", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtRenameKey", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtReplaceKey", OK, RNTST, 3,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtReplyPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,0},"NtReplyWaitReceivePort", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), W|HT, DRSYS_TYPE_UNKNOWN /* XXX: what type is this? */},
         {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(PORT_MESSAGE), W|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,0},"NtReplyWaitReceivePortEx", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), W|HT, DRSYS_TYPE_UNKNOWN /* XXX: what type is this? */},
         {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(PORT_MESSAGE), W|CT, SYSARG_TYPE_PORT_MESSAGE},
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtReplyWaitReplyPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|W|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,WINXP},"NtReplyWaitSendChannel", OK, RNTST, 3,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(CHANNEL_MESSAGE), W},
     }
    },
    {{0,WINVISTA},"NtRequestDeviceWakeup", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtRequestPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,0},"NtRequestWaitReplyPort", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
#if 1
         /* FIXME PR 406356: suppressing undefined read I see on every app at process
          * termination on w2k3 vm (though not on wow64 laptop) where the last 16
          * bytes are not filled in (so only length and type are).  Length indicates
          * there is data afterward which we try to handle specially.
          */
         {1, 8, R},
#else
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
#endif
         {2, sizeof(PORT_MESSAGE), W|CT, SYSARG_TYPE_PORT_MESSAGE},
     }
    },
    {{0,WINVISTA},"NtRequestWakeupLatency", OK, RNTST, 1,
     {
         {0, sizeof(LATENCY_TIME), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtResetEvent", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtResetWriteWatch", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtRestoreKey", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtResumeProcess", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtResumeThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSaveKey", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSaveKeyEx", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSaveMergedKeys", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSecureConnectPort", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(SECURITY_QUALITY_OF_SERVICE), R|CT, SYSARG_TYPE_SECURITY_QOS},
         {3, sizeof(PORT_VIEW), R|W},
         {4, sizeof(SID), R},
         {5, sizeof(REMOTE_PORT_VIEW), R|W},
         {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {7, -8, R|WI},
         {8, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,WINXP},"NtSendWaitReplyChannel", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(CHANNEL_MESSAGE), W},
     }
    },
    {{0,0},"NtSetBootEntryOrder", OK, RNTST, 2,
     {
         {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetBootOptions", OK, RNTST, 2,
     {
         {0, sizeof(BOOT_OPTIONS), R},
     }
    },
    {{0,WINXP},"NtSetContextChannel", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSetContextThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CONTEXT), R|CT, SYSARG_TYPE_CONTEXT},
     }
    },
    {{0,0},"NtSetDebugFilterState", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtSetDefaultHardErrorPort", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSetDefaultLocale", OK, RNTST, 2,
     {
         {0, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {1, sizeof(LCID), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetDefaultUILanguage", OK, RNTST, 1,
     {
         {0, sizeof(LANGID), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetEaFile", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(FILE_FULL_EA_INFORMATION), R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetEvent", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetEventBoostPriority", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSetHighEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSetHighWaitLowEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,WINNT},"NtSetHighWaitLowThread", OK, RNTST, 0},
    {{0,0},"NtSetInformationDebugObject", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(DEBUGOBJECTINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetInformationFile", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
       {4,}
     }, (drsys_sysnum_t *)syscall_SetInformationFile_info
    },
    {{0,0},"NtSetInformationJobObject", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(JOBOBJECTINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetInformationKey", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
       {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationKey_info
    },
    {{0,0},"NtSetInformationObject", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
       {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationObject_info
    },
    {{0,0},"NtSetInformationProcess", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PROCESSINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         /* Some info classes have part of the passed-in size as OUT (i#1228),
          * necessitating special-casing instead of listing "{2, -3, R}" here.
          * We still list an entry (w/ default struct type) for non-memarg iterator.
          */
         {2, -3, SYSARG_NON_MEMARG, },
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_SetInformationProcess
    },
    {{0,0},"NtSetInformationThread", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(THREADINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetInformationToken", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(TOKEN_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetIntervalProfile", OK, RNTST, 2,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(KPROFILE_SOURCE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtSetIoCompletion", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* XXX i#1536: We fill it as inlined VOID* based on our own research
          * but different sources describe this arg in different ways.
          */
         {2, sizeof(void*), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {3, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetLdtEntries", OK, RNTST, 4,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(LDT_ENTRY), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(LDT_ENTRY), SYSARG_INLINED, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtSetLowEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSetLowWaitHighEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,WINNT},"NtSetLowWaitHighThread", OK, RNTST, 0, },
    {{0,0},"NtSetQuotaInformationFile", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(FILE_USER_QUOTA_INFORMATION), R},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetSecurityObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(SECURITY_INFORMATION), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(SECURITY_DESCRIPTOR), R|CT, SYSARG_TYPE_SECURITY_DESCRIPTOR},
     }
    },
    {{0,0},"NtSetSystemEnvironmentValue", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtSetSystemEnvironmentValueEx", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(GUID), R},
     }
    },
    /* Some info classes write data as well, which is special-cased */
    {{0,0},"NtSetSystemInformation", OK, RNTST, 3,
     {
         {0, sizeof(SYSTEM_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -2, R},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_SetSystemInformation
    },
    {{0,0},"NtSetSystemPowerState", OK, RNTST, 3,
     {
         {0, sizeof(POWER_ACTION), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(SYSTEM_POWER_STATE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetSystemTime", OK, RNTST, 2,
     {
         {0, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {1, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtSetThreadExecutionState", OK, RNTST, 2,
     {
         {0, sizeof(EXECUTION_STATE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(EXECUTION_STATE), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetTimer", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {2, sizeof(PTIMER_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {4, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {5, sizeof(LONG), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {6, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtSetTimerResolution", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetUuidSeed", OK, RNTST, 1,
     {
         {0, sizeof(UCHAR), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetValueKey", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "REG_NONE"},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSetVolumeInformationFile", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {4,}
     }, (drsys_sysnum_t *)syscall_SetVolumeInformationFile_info
    },
    {{0,0},"NtShutdownSystem", OK, RNTST, 1,
     {
         {0, sizeof(SHUTDOWN_ACTION), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtSignalAndWaitForSingleObject", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtStartProfile", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtStopProfile", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSuspendProcess", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtSuspendThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtSystemDebugControl", OK, RNTST, 6,
     {
         {0, sizeof(SYSDBG_COMMAND), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, R},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W},
         {3, -5, WI},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtTerminateJobObject", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtTerminateProcess", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtTerminateThread", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtTestAlert", OK, RNTST, 0},
    /* unlike TraceEvent API routine, syscall takes size+flags as
     * separate params, and struct observed to be all uninit, so we
     * assume struct is all OUT
     */
    {{0,0},"NtTraceEvent", OK, RNTST, 4,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(EVENT_TRACE_HEADER), W},
     }
    },
    {{0,0},"NtTranslateFilePath", OK, RNTST, 4,
     {
         {0, sizeof(FILE_PATH), R},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(FILE_PATH), W},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUnloadDriver", OK, RNTST, 1,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtUnloadKey", OK, RNTST, 1,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtUnloadKey2", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtUnloadKeyEx", OK, RNTST, 2,
     {
         {0, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtUnlockFile", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {3, sizeof(ULARGE_INTEGER), R|HT, DRSYS_TYPE_ULARGE_INTEGER},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUnlockVirtualMemory", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtUnmapViewOfSection", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtVdmControl", OK, RNTST, 2,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,WINNT},"NtW32Call", OK, RNTST, 5,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* FIXME: de-ref w/o corresponding R to check definedness: but not enough
          * info to understand exactly what's going on here
          */
         {3, -4, WI|HT, DRSYS_TYPE_VOID},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWaitForDebugEvent", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {3, sizeof(DBGUI_WAIT_STATE_CHANGE), W},
     }
    },
    {{0,0},"NtWaitForKeyedEvent", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {3, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtWaitForMultipleObjects", OK, RNTST, 5,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HANDLE), R|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(WAIT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtWaitForMultipleObjects32", OK, RNTST, 5,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(HANDLE), R|HT, DRSYS_TYPE_HANDLE},
         {2, sizeof(WAIT_TYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtWaitForSingleObject", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
         {2, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtWaitHighEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtWaitLowEventPair", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtWriteFile", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, -6, R|HT, DRSYS_TYPE_VOID},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWriteFileGather", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PIO_APC_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {5, sizeof(FILE_SEGMENT_ELEMENT), R},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {8, sizeof(ULONG), R|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWriteRequestData", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, R},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWriteVirtualMemory", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {2, -3, R|HT, DRSYS_TYPE_VOID},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtYieldExecution", OK, RNTST, 0, },

    /***************************************************/
    /* added in Windows 2003 */
    {{0,0},"NtSetDriverEntryOrder", OK, RNTST, 2,
     {
        {0, -1, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(ULONG)},
        {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },

    /* FIXME i#1089: fill in info on all the inlined args for the
     * syscalls below here.
     */

    /***************************************************/
    /* added in Windows XP64 WOW64 */
    {{0,0},"NtWow64CsrClientConnectToServer", UNKNOWN, RNTST, 5, },
    {{0,0},"NtWow64CsrNewThread", OK, RNTST, 0, },
    {{0,0},"NtWow64CsrIdentifyAlertableThread", OK, RNTST, 0, },
    {{0,0},"NtWow64CsrClientCallServer", UNKNOWN, RNTST, 4, },
    {{0,0},"NtWow64CsrAllocateCaptureBuffer", OK, RNTST, 2, },
    {{0,0},"NtWow64CsrFreeCaptureBuffer", OK, RNTST, 1, },
    {{0,0},"NtWow64CsrAllocateMessagePointer", UNKNOWN, RNTST, 3, },
    {{0,0},"NtWow64CsrCaptureMessageBuffer", UNKNOWN, RNTST, 4, },
    {{0,0},"NtWow64CsrCaptureMessageString", UNKNOWN, RNTST, 5, },
    {{0,0},"NtWow64CsrSetPriorityClass", OK, RNTST, 2,
     {
        {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWow64CsrGetProcessId", OK, RNTST, 0, },
    {{0,0},"NtWow64DebuggerCall", OK, RNTST, 5, },
    /* args seem to be identical to NtQuerySystemInformation */
    {{0,0},"NtWow64GetNativeSystemInformation", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 4,
     {
         {0, sizeof(SYSTEM_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -2, W},
         {1, -3, WI},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_QuerySystemInformationWow64
    },
    {{0,0},"NtWow64QueryInformationProcess64", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 5,
     {
        {2, -3, W},
        {2, -4, WI},
        {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWow64ReadVirtualMemory64", UNKNOWN, RNTST, 7, },
    {{0,WIN10},"NtWow64QueryVirtualMemory64", UNKNOWN, RNTST, 8, },

    /***************************************************/
    /* added in Windows Vista SP0 */
    /* XXX: add min OS version: but we have to distinguish the service packs! */
    {{0,0},"NtAcquireCMFViewOwnership", OK, RNTST, 3,
     {
         {0, sizeof(ULONGLONG), W|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
         {2, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAlpcAcceptConnectPort", OK, RNTST, 9,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {4, sizeof(ALPC_PORT_ATTRIBUTES), R|CT, SYSARG_TYPE_ALPC_PORT_ATTRIBUTES},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {6, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {7, sizeof(ALPC_MESSAGE_ATTRIBUTES), R|W|HT, DRSYS_TYPE_STRUCT},
         {8, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtAlpcCancelMessage", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ALPC_CONTEXT_ATTRIBUTES), R|CT, SYSARG_TYPE_ALPC_CONTEXT_ATTRIBUTES},
     }
    },
    {{0,0},"NtAlpcConnectPort", OK, RNTST, 11,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ALPC_PORT_ATTRIBUTES), R|CT, SYSARG_TYPE_ALPC_PORT_ATTRIBUTES},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT, "ALPC_SYNC_CONNECTION"},
         {5, sizeof(SID), R|HT, DRSYS_TYPE_STRUCT},
         {6, -7, WI|HT, SYSARG_TYPE_PORT_MESSAGE},
         {7, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(ALPC_MESSAGE_ATTRIBUTES), R|W|HT, DRSYS_TYPE_STRUCT},
         {9, sizeof(ALPC_MESSAGE_ATTRIBUTES), R|W|HT, DRSYS_TYPE_STRUCT},
         {10, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtAlpcCreatePort", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {2, sizeof(ALPC_PORT_ATTRIBUTES), R|CT, SYSARG_TYPE_ALPC_PORT_ATTRIBUTES},
     }
    },
    {{0,0},"NtAlpcCreatePortSection", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAlpcCreateResourceReserve", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtAlpcCreateSectionView", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ALPC_DATA_VIEW), R|W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtAlpcCreateSecurityContext", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ALPC_SECURITY_ATTRIBUTES), R|W|CT, SYSARG_TYPE_ALPC_SECURITY_ATTRIBUTES},
     }
    },
    {{0,0},"NtAlpcDeletePortSection", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtAlpcDeleteResourceReserve", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* XXX: ok for shadowing purposes, but we should look at tracking
     * the allocation once we understand NtAlpcCreateSectionView
     */
    {{0,0},"NtAlpcDeleteSectionView", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtAlpcDeleteSecurityContext", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtAlpcDisconnectPort", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtAlpcImpersonateClientOfPort", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtAlpcOpenSenderProcess", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtAlpcOpenSenderThread", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtAlpcQueryInformation", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_AlpcQueryInformation_info
    },
    {{0,0},"NtAlpcQueryInformationMessage", OK|SYSINFO_SECONDARY_TABLE, RNTST, 6,
     {
         {2,}
     }, (drsys_sysnum_t *)syscall_AlpcQueryInformationMessage_info
    },
    {{0,0},"NtAlpcRevokeSecurityContext", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    /* FIXME i#98:
     * + #2 should be {2, sizeof(PORT_MESSAGE), R|CT, SYSARG_TYPE_PORT_MESSAGE}
     * + #4 should be {4, -5, R|WI|HT, SYSARG_TYPE_PORT_MESSAGE}
     * The issue is w/ synchronous calls where the same PORT_MESSAGE buffer is used
     * for both receive/send.
     */
    {{0,0},"NtAlpcSendWaitReceivePort", UNKNOWN, RNTST, 8,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PORT_MESSAGE), SYSARG_NON_MEMARG, SYSARG_TYPE_PORT_MESSAGE},
         {3, sizeof(ALPC_MESSAGE_ATTRIBUTES), R|W|CT, DRSYS_TYPE_ALPC_MESSAGE_ATTRIBUTES},
         {4, -5, WI|HT, SYSARG_TYPE_PORT_MESSAGE},
         {5, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ALPC_MESSAGE_ATTRIBUTES), R|W|CT, DRSYS_TYPE_ALPC_MESSAGE_ATTRIBUTES},
         {7, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtAlpcSetInformation", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_AlpcSetInformation_info
    },
    {{0,0},"NtCancelIoFileEx", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), R|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{0,0},"NtCancelSynchronousIoFile", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(IO_STATUS_BLOCK), R|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
         {2, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{0,0},"NtClearAllSavepointsTransaction", UNKNOWN, RNTST, 1, },
    {{0,0},"NtClearSavepointTransaction", UNKNOWN, RNTST, 2, },
    {{0,0},"NtCommitComplete", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtCommitEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtCommitTransaction", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtCreateEnlistment", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(NOTIFICATION_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtCreateKeyTransacted", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {7, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreatePrivateNamespace", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtCreateResourceManager", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtCreateTransaction", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {9, sizeof(UNICODE_STRING), R|HT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{0,0},"NtCreateTransactionManager", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtCreateWorkerFactory", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {4, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {6, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {8, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtDeletePrivateNamespace", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtEnumerateTransactionObject", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(KTMOBJECT_TYPE), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|W|HT, DRSYS_TYPE_STRUCT},
         {2, -4, WI|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtFlushInstallUILanguage", OK, RNTST, 2,
     {
         {0, sizeof(LANGID), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtFlushProcessWriteBuffers", OK, RNTST, 0, },
    {{0,0},"NtFreezeRegistry", OK, RNTST, 1,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtFreezeTransactions", OK, RNTST, 2,
     {
         {0, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtGetMUIRegistryInfo", OK, RNTST, 3,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, -1, WI|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtGetNextProcess", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGetNextThread", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtGetNlsSectionPtr", OK, RNTST, 5,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {3, sizeof(PVOID), W|HT, DRSYS_TYPE_POINTER},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtGetNotificationResourceManager", OK, RNTST, 7,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|HT, DRSYS_TYPE_STRUCT},
         {1, -4, WI|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtInitializeNlsFiles", OK, RNTST, 3,
     {
         {0, sizeof(PVOID), W|HT, DRSYS_TYPE_POINTER},
         {1, sizeof(LCID), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(LARGE_INTEGER), W|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtIsUILanguageComitted", OK, RNTST, 0, },
    {{0,0},"NtListTransactions", UNKNOWN, RNTST, 3, },
    {{0,0},"NtMarshallTransaction", UNKNOWN, RNTST, 6, },
    {{0,0},"NtOpenEnlistment", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenKeyTransacted", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenPrivateNamespace", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtOpenResourceManager", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenSession", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{0,0},"NtOpenTransaction", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtOpenTransactionManager", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
         {3, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {4, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtPrepareComplete", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtPrepareEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtPrePrepareComplete", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtPrePrepareEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtPropagationComplete", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtPropagationFailed", OK, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
     }
    },
    {{0,0},"NtPullTransaction", UNKNOWN, RNTST, 7, },
    {{0,0},"NtQueryInformationEnlistment", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryInformationEnlistment_info
    },
    {{0,0},"NtQueryInformationResourceManager", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryInformationResourceManager_info
    },
    {{0,0},"NtQueryInformationTransaction", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_QueryInformationTransaction_info
    },
    {{0,0},"NtQueryInformationTransactionManager", OK|SYSINFO_SECONDARY_TABLE, RNTST, 5,
     {
         {1,}
     },  (drsys_sysnum_t *)syscall_QueryInformationTransactionManager_info
    },
    {{0,0},"NtQueryInformationWorkerFactory", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WORKERFACTORYINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, W|HT, DRSYS_TYPE_STRUCT},
         {2, -4, WI|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtQueryLicenseValue", UNKNOWN, RNTST, 5, },
    {{0,0},"NtReadOnlyEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtRecoverEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
     }
    },
    {{0,0},"NtRecoverResourceManager", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtRecoverTransactionManager", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtRegisterProtocolAddressInformation", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(CRM_PROTOCOL_ID), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtReleaseCMFViewOwnership", OK, RNTST, 0, },
    {{0,0},"NtReleaseWorkerFactoryWorker", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{0,0},"NtRemoveIoCompletionEx", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(FILE_IO_COMPLETION_INFORMATION)},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(void*), W|HT, DRSYS_TYPE_VOID},/* see i#1536 */
         {4, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {5, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtRollbackComplete", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     },
    },
    {{0,0},"NtRollbackEnlistment", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtRollbackSavepointTransaction", UNKNOWN, RNTST, 2, },
    {{0,0},"NtRollbackTransaction", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), SYSARG_INLINED, DRSYS_TYPE_BOOL},
     }
    },
    {{0,0},"NtRollforwardTransactionManager", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtSavepointComplete", UNKNOWN, RNTST, 2, },
    {{0,0},"NtSavepointTransaction", UNKNOWN, RNTST, 3, },
    {{0,0},"NtSetInformationEnlistment", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationEnlistment_info
    },
    {{0,0},"NtSetInformationResourceManager", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationResourceManager_info
    },
    {{0,0},"NtSetInformationTransaction", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationTransaction_info
    },
    {{0,0},"NtSetInformationTransactionManager", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1,}
     }, (drsys_sysnum_t *)syscall_SetInformationTransactionManager_info
    },
    {{0,0},"NtSetInformationWorkerFactory", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(WORKERFACTORYINFOCLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtShutdownWorkerFactory", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LONG), R|W|HT, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{0,0},"NtSinglePhaseReject", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
     }
    },
    {{0,0},"NtStartTm", OK, RNTST, 0, },
    {{0,0},"NtThawRegistry", OK, RNTST, 0, },
    {{0,0},"NtThawTransactions", OK, RNTST, 0, },
    {{0,0},"NtTraceControl", OK, RNTST, 6,
     {
         {0, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* The "{1, -2, R|HT, DRSYS_TYPE_STRUCT}" entry is specially handled */
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W|HT, DRSYS_TYPE_STRUCT},
         {3, -5, WI|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_TraceControl
    },
    {{0,WIN7},"NtWaitForWorkViaWorkerFactory", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(FILE_IO_COMPLETION_INFORMATION), W|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{WIN8,WIN8},"NtWaitForWorkViaWorkerFactory", UNKNOWN, RNTST, 4, },
    {{WIN81,0},  "NtWaitForWorkViaWorkerFactory", UNKNOWN, RNTST, 5, },
    {{0,0},"NtWorkerFactoryWorkerReady", OK, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },

    /***************************************************/
    /* added in Windows Vista SP1 */
    /* XXX: add min OS version: but we have to distinguish the service packs! */
    {{0,0},"NtRenameTransactionManager", OK, RNTST, 2,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(GUID), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{0,0},"NtReplacePartitionUnit", OK, RNTST, 3,
     {
         {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {1, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWow64CsrVerifyRegion", OK, RNTST, 2, },
    {{0,0},"NtWow64WriteVirtualMemory64", OK, RNTST, 7,
     {
        {6, sizeof(ULONGLONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{0,0},"NtWow64CallFunction64", OK, RNTST, 7,
     {
        {3, -2, R},
        {5, -4, W},
        {6, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },

    /***************************************************/
    /* added in Windows 7 */
    {{WIN7,0},"NtAllocateReserveObject", OK, RNTST, 3,
     {
        {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
        {1, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
     }
    },
    {{WIN7,0},"NtCreateProfileEx", OK, RNTST, 10,
     {
         {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {3, sizeof(SIZE_T), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, -6, R|HT, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(KPROFILE_SOURCE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {8, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {9, sizeof(GROUP_AFFINITY), R|HT, DRSYS_TYPE_STRUCT},
     }
    },
    {{WIN7,0},"NtDisableLastKnownGood", OK, RNTST, 0, },
    {{WIN7,0},"NtDrawText", OK, RNTST, 1,
     {
        {0, sizeof(UNICODE_STRING), R|CT, SYSARG_TYPE_UNICODE_STRING},
     }
    },
    {{WIN7,0},"NtEnableLastKnownGood", OK, RNTST, 0, },
    {{WIN7,0},"NtNotifyChangeSession", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, -7, R},
         {7, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN7,0},"NtOpenKeyTransactedEx", OK, RNTST, 5,
     {
        {0, sizeof(HANDLE), W|HT, DRSYS_TYPE_HANDLE},
        {1, sizeof(ACCESS_MASK), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
        {2, sizeof(OBJECT_ATTRIBUTES), R|CT, SYSARG_TYPE_OBJECT_ATTRIBUTES},
        {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
        {4, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },
    {{WIN7,0},"NtQuerySecurityAttributesToken", UNKNOWN, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         /* XXX i#1537: Arg requires special handler function. */
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W|HT, DRSYS_TYPE_STRUCT},
         {3, -5, WI|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* One info class reads data, which is special-cased */
    {{WIN7,0},"NtQuerySystemInformationEx", OK|SYSINFO_RET_SMALL_WRITE_LAST, RNTST, 6,
     {
         {0, sizeof(SYSTEM_INFORMATION_CLASS), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, -2, R},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, W},
         {3, -5, WI},
         {4, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }, &sysnum_QuerySystemInformationEx
    },
    {{WIN7,0},"NtQueueApcThreadEx", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(PKNORMAL_ROUTINE), SYSARG_INLINED, DRSYS_TYPE_FUNCTION},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {5, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
     }
    },
    {{WIN7,0},"NtSerializeBoot", OK, RNTST, 0, },
    {{WIN7,0},"NtSetIoCompletionEx", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(void*), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
         {5, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN7,0},"NtSetTimerEx", OK|SYSINFO_SECONDARY_TABLE, RNTST, 4,
     {
         {1, }
     }, (drsys_sysnum_t *)syscall_SetTimerEx_info
    },
    {{WIN7,0},"NtUmsThreadYield", OK, RNTST, 1,
     {
         {0, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
     }
    },
    {{WIN7,0},"NtWow64GetCurrentProcessorNumberEx", OK, RNTST, 1,
     {
        {0, sizeof(PROCESSOR_NUMBER), W},
     }
    },
    {{WIN7,WIN7},"NtWow64InterlockedPopEntrySList", OK, RNTST, 1,
     {
        {0, sizeof(SLIST_HEADER), R|W},
     }
    },

    /***************************************************/
    /* Added in Windows 8 */
    /* FIXME i#1153: fill in details */
    {{WIN8,0},"NtAddAtomEx", UNKNOWN, RNTST, 4, },
    {{WIN8,0},"NtAdjustTokenClaimsAndDeviceGroups", UNKNOWN, RNTST, 16, },
    {{WIN8,0},"NtAlertThreadByThreadId", UNKNOWN, RNTST, 1, },
    {{WIN8,0},"NtAlpcConnectPortEx", UNKNOWN, RNTST, 11, },
    {{WIN8,0},"NtAssociateWaitCompletionPacket", OK, RNTST, 8,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {2, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {3, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {5, sizeof(NTSTATUS), SYSARG_INLINED, DRSYS_TYPE_NTSTATUS},
         {6, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {7, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{WIN8,0},"NtCancelWaitCompletionPacket", UNKNOWN, RNTST, 2, },
    {{WIN8,0},"NtCreateDirectoryObjectEx", UNKNOWN, RNTST, 5, },
    {{WIN8,0},"NtCreateIRTimer", UNKNOWN, RNTST, 2, },
    {{WIN8,0},"NtCreateLowBoxToken", UNKNOWN, RNTST, 9, },
    {{WIN8,0},"NtCreateTokenEx", UNKNOWN, RNTST, 17, },
    {{WIN8,0},"NtCreateWaitCompletionPacket", UNKNOWN, RNTST, 3, },
    {{WIN8,0},"NtCreateWnfStateName", UNKNOWN, RNTST, 7, },
    {{WIN8,0},"NtDeleteWnfStateData", UNKNOWN, RNTST, 2, },
    {{WIN8,0},"NtDeleteWnfStateName", UNKNOWN, RNTST, 1, },
    {{WIN8,0},"NtFilterBootOption", UNKNOWN, RNTST, 5, },
    {{WIN8,0},"NtFilterTokenEx", UNKNOWN, RNTST, 14, },
    {{WIN8,0},"NtFlushBuffersFileEx", OK, RNTST, 5,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, -3, R|HT, DRSYS_TYPE_STRUCT},
         {3, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, sizeof(IO_STATUS_BLOCK), W|HT, DRSYS_TYPE_IO_STATUS_BLOCK},
     }
    },
    {{WIN8,0},"NtGetCachedSigningLevel", UNKNOWN, RNTST, 6, },
    {{WIN8,0},"NtQueryWnfStateData", OK, RNTST, 6,
     {
         {0, sizeof(WNF_STATE_NAME), R|HT, DRSYS_TYPE_STRUCT},
         {1, sizeof(WNF_TYPE_ID), R|HT, DRSYS_TYPE_STRUCT},
         {2, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {3, sizeof(WNF_CHANGE_STAMP), W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {4, -5, WI},
         {5, sizeof(ULONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN8,0},"NtQueryWnfStateNameInformation", UNKNOWN, RNTST, 5, },
    {{WIN8,0},"NtSetCachedSigningLevel", UNKNOWN, RNTST, 5, },
    {{WIN8,0},"NtSetInformationVirtualMemory", OK, RNTST, 6,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(ULONG_PTR), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(MEMORY_RANGE_ENTRY)},
         {4, -5, R},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN8,0},"NtSetIRTimer", UNKNOWN, RNTST, 2, },
    {{WIN8,0},"NtSubscribeWnfStateChange", UNKNOWN, RNTST, 4, },
    {{WIN8,0},"NtUnmapViewOfSectionEx", UNKNOWN, RNTST, 3,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_UNKNOWN},
         /* FIXME i#1153: what is the 3rd arg?  Observed to be 0. */
     }
    },
    {{WIN8,0},"NtUnsubscribeWnfStateChange", UNKNOWN, RNTST, 1, },
    {{WIN8,0},"NtUpdateWnfStateData", OK, RNTST, 7,
     {
         {0, sizeof(WNF_STATE_NAME), R|HT, DRSYS_TYPE_STRUCT},
         {1, -2, R},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(WNF_TYPE_ID), R|HT, DRSYS_TYPE_STRUCT},
         {4, sizeof(PVOID), SYSARG_INLINED, DRSYS_TYPE_VOID},
         {5, sizeof(WNF_CHANGE_STAMP), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(LOGICAL), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{WIN8,0},"NtWaitForAlertByThreadId", UNKNOWN, RNTST, 2, },
    {{WIN8,WIN8},"NtWaitForWnfNotifications", UNKNOWN, RNTST, 2, },
    {{WIN8,0},"NtWow64AllocateVirtualMemory64", UNKNOWN, RNTST, 7,
     {
         /* XXX: I'm asuming the base and size pointers point at 64-bit values */
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(ULONGLONG), R|W|HT, DRSYS_TYPE_POINTER},
         {2, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         /* FIXME i#1153: what is 4th arg?  Top of ZeroBits?*/
         {4, sizeof(ULONGLONG), R|W|HT, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {6, sizeof(ULONG), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },

    /***************************************************/
    /* Added in Windows 8.1 */
    /* FIXME i#1360: fill in details */
    {{WIN81,0},"NtCancelTimer2", OK, RNTST, 2,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(BOOLEAN), W|HT, DRSYS_TYPE_BOOL},
     }
    },
    {{WIN81,0},"NtCreateTimer2", UNKNOWN, RNTST, 5, },
    {{WIN81,0},"NtGetCompleteWnfStateSubscription", UNKNOWN, RNTST, 6, },
    {{WIN81,0},"NtSetTimer2", OK, RNTST, 4,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
         {1, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {2, sizeof(LARGE_INTEGER), R|HT, DRSYS_TYPE_LARGE_INTEGER},
         {3, sizeof(T2_SET_PARAMETERS), R|CT, SYSARG_TYPE_T2_SET_PARAMETERS},
     }
    },
    {{WIN81,0},"NtSetWnfProcessNotificationEvent", UNKNOWN, RNTST, 1,
     {
         {0, sizeof(HANDLE), SYSARG_INLINED, DRSYS_TYPE_HANDLE},
     }
    },

    /***************************************************/
    /* Added in Windows 10 */
    /* FIXME i#1750: fill in details */
    {{WIN10,0},"NtAlpcImpersonateClientContainerOfPort", UNKNOWN, RNTST, 3, },
    {{WIN10,0},"NtCompareObjects", UNKNOWN, RNTST, 2, },
    {{WIN10,0},"NtCreatePartition", UNKNOWN, RNTST, 5, },
    {{WIN10,0},"NtGetCurrentProcessorNumberEx", UNKNOWN, RNTST, 1, },
    {{WIN10,0},"NtManagePartition", UNKNOWN, RNTST, 5, },
    {{WIN10,0},"NtOpenPartition", UNKNOWN, RNTST, 3, },
    {{WIN10,0},"NtRevertContainerImpersonation", UNKNOWN, RNTST, 0, },
    {{WIN10,0},"NtSetInformationSymbolicLink", UNKNOWN, RNTST, 4, },
    {{WIN10,0},"NtWow64IsProcessorFeaturePresent", OK, RNTST, 1,
     {
         {0, sizeof(DWORD), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    /* Added in Windows 10 1511 */
    /* FIXME i#1750: fill in details */
    {{WIN11,0},"NtCreateEnclave", UNKNOWN, RNTST, 9, },
    {{WIN11,0},"NtInitializeEnclave", UNKNOWN, RNTST, 5, },
    {{WIN11,0},"NtLoadEnclaveData", UNKNOWN, RNTST, 9, },
    /* Added in Windows 10 1607 */
    /* FIXME i#1750: fill in details */
    {{WIN12,0},"NtCommitRegistryTransaction", UNKNOWN, RNTST, 2, },
    {{WIN12,0},"NtCreateRegistryTransaction", UNKNOWN, RNTST, 4, },
    {{WIN12,0},"NtOpenRegistryTransaction", UNKNOWN, RNTST, 3, },
    {{WIN12,0},"NtQuerySecurityPolicy", UNKNOWN, RNTST, 6, },
    {{WIN12,0},"NtRollbackRegistryTransaction", UNKNOWN, RNTST, 2, },
    {{WIN12,0},"NtSetCachedSigningLevel2", UNKNOWN, RNTST, 6, },
    /* Added in Windows 10 1703 */
    /* FIXME i#1750: fill in details */
    {{WIN13,0},"NtAcquireProcessActivityReference", UNKNOWN, RNTST, 3, },
    {{WIN13,0},"NtCompareSigningLevels", UNKNOWN, RNTST, 2, },
    {{WIN13,0},"NtConvertBetweenAuxiliaryCounterAndPerformanceCounter,NONE,", UNKNOWN, RNTST, 1, },
    {{WIN13,0},"NtLoadHotPatch", UNKNOWN, RNTST, 2, },
    {{WIN13,0},"NtQueryAuxiliaryCounterFrequency", UNKNOWN, RNTST, 1, },
    {{WIN13,0},"NtQueryInformationByName", UNKNOWN, RNTST, 5, },
    /* Added in Windows 10 1709 */
    /* FIXME i#1750: fill in details */
    {{WIN14,0},"NtCallEnclave", UNKNOWN, RNTST, 4, },
    {{WIN14,0},"NtNotifyChangeDirectoryFileEx", UNKNOWN, RNTST, 10, },
    {{WIN14,0},"NtQueryDirectoryFileEx", UNKNOWN, RNTST, 10, },
    {{WIN14,0},"NtTerminateEnclave", UNKNOWN, RNTST, 2, },
    /* Added in Windows 10 1803 */
    /* FIXME i#1750: fill in details */
    {{WIN15,0},"NtAllocateVirtualMemoryEx", UNKNOWN, RNTST, 7, },
    {{WIN15,0},"NtMapViewOfSectionEx", UNKNOWN, RNTST, 9, },
};

#define NUM_NTDLL_SYSCALLS \
    (sizeof(syscall_ntdll_info)/sizeof(syscall_ntdll_info[0]))

size_t
num_ntdll_syscalls(void)
{
    return NUM_NTDLL_SYSCALLS;
}
