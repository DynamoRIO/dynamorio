/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created from a ProcessHacker header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

 /* from phlib/include/ntioapi.h */

#ifndef __PHLIB_NTIOAPI_H
#define __PHLIB_NTIOAPI_H

NTSTATUS NTAPI
NtDisableLastKnownGood(
    VOID
    );

NTSTATUS NTAPI
NtEnableLastKnownGood(
    VOID
    );

NTSTATUS NTAPI
NtCancelSynchronousIoFile(
    __in HANDLE ThreadHandle,
    __in_opt PIO_STATUS_BLOCK IoRequestToCancel,
    __out PIO_STATUS_BLOCK IoStatusBlock
    );

NTSTATUS NTAPI
NtSetIoCompletion(
    __in HANDLE IoCompletionHandle,
    __in ULONG CompletionKey,
    __in_opt PVOID CompletionValue,
    __in NTSTATUS IoStatus,
    __in ULONG_PTR IoStatusInformation
    );

NTSTATUS NTAPI
NtSetIoCompletionEx(
    __in HANDLE IoCompletionHandle,
    __in HANDLE IoCompletionReserveHandle,
    __in ULONG CompletionKey,
    __in_opt PVOID CompletionValue,
    __in NTSTATUS IoStatus,
    __in ULONG_PTR IoStatusInformation
    );

NTSTATUS NTAPI
NtRemoveIoCompletionEx(
    __in HANDLE IoCompletionHandle,
    __out_ecount(Count) FILE_IO_COMPLETION_INFORMATION IoCompletionInformation,
    __in ULONG Count,
    __out PVOID NumEntriesRemoved,
    __in_opt PLARGE_INTEGER Timeout,
    __in BOOLEAN Alertable
    );

/* The params for this syscall differ from Process Hacker
 * and MSDN description. We put it here manually according to
 * our own research and ntifs.h ver. Windows 8.1.
 */
NTSTATUS NTAPI
NtFlushBuffersFileEx(
    _In_ HANDLE FileHandle,
    _In_ ULONG Flags,
    _In_reads_bytes_(ParametersSize) PVOID Parameters,
    _In_ ULONG ParametersSize,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock
    );


typedef enum _IO_SESSION_STATE {
    IoSessionStateCreated,
    IoSessionStateInitialized,
    IoSessionStateConnected,
    IoSessionStateDisconnected,
    IoSessionStateDisconnectedLoggedOn,
    IoSessionStateLoggedOn,
    IoSessionStateLoggedOff,
    IoSessionStateTerminated,
    IoSessionStateMax
} IO_SESSION_STATE;

NTAPI
NtOpenSession(
    __out PHANDLE SessionHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS
NTAPI
NtNotifyChangeSession(
    __in HANDLE SessionHandle,
    __in ULONG IoStateSequence,
    __in PVOID Reserved,
    __in ULONG Action,
    __in IO_SESSION_STATE IoState,
    __in IO_SESSION_STATE IoState2,
    __in PVOID Buffer,
    __in ULONG BufferSize
    );


NTSTATUS
NTAPI
NtAssociateWaitCompletionPacket(
    _In_ HANDLE WaitCompletionPacketHandle,
    _In_ HANDLE IoCompletionHandle,
    _In_ HANDLE TargetObjectHandle,
    _In_opt_ PVOID KeyContext,
    _In_opt_ PVOID ApcContext,
    _In_ NTSTATUS IoStatus,
    _In_ ULONG_PTR IoStatusInformation,
    _Out_opt_ PBOOLEAN AlreadySignaled
    );

#endif /* __PHLIB_NTIOAPI_H */

/* EOF */
