/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a ProcessHacker header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* from phlib/include/ntpsapi.h */

#ifndef __PHLIB_NTPSAPI_H
#define __PHLIB_NTPSAPI_H

typedef DWORD MEMORY_RESERVE_TYPE;
typedef PVOID PPS_APC_ROUTINE;

/**************************************************
 * Syscalls added in Win7
 */
NTSTATUS NTAPI
NtAllocateReserveObject(
    __out PHANDLE MemoryReserveHandle,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in MEMORY_RESERVE_TYPE Type
    );

NTSTATUS
NTAPI
NtGetNextProcess(
    __in HANDLE ProcessHandle,
    __in ACCESS_MASK DesiredAccess,
    __in ULONG HandleAttributes,
    __in ULONG Flags,
    __out PHANDLE NewProcessHandle
    );

NTSTATUS
NTAPI
NtGetNextThread(
    __in HANDLE ProcessHandle,
    __in HANDLE ThreadHandle,
    __in ACCESS_MASK DesiredAccess,
    __in ULONG HandleAttributes,
    __in ULONG Flags,
    __out PHANDLE NewThreadHandle
    );

NTSTATUS
NTAPI
NtQueueApcThreadEx(
    __in HANDLE ThreadHandle,
    __in_opt HANDLE UserApcReserveHandle,
    __in PPS_APC_ROUTINE ApcRoutine,
    __in_opt PVOID ApcArgument1,
    __in_opt PVOID ApcArgument2,
    __in_opt PVOID ApcArgument3
    );

NTSTATUS
NTAPI
NtUmsThreadYield(
    __in  PVOID SchedulerParam
    );

/**************************************************
 * NtQueryThreadInformation
 */

typedef struct _THREAD_TEB_INFORMATION {
    PVOID OutputBuffer;
    ULONG TebOffset;
    ULONG BytesToRead;
} THREAD_TEB_INFORMATION, *PTHREAD_TEB_INFORMATION;

#endif /* __PHLIB_NTPSAPI_H */

/* EOF */
