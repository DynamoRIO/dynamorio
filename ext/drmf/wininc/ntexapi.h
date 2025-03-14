/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created to make information necessary for userspace
 ***   to call into the Windows kernel available to Dr. Memory.  It contains
 ***   only constants, structures, and macros, and thus, contains no
 ***   copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _NTEXAPI_H_
#define _NTEXAPI_H_ 1

#include "ndk_iotypes.h"

NTSTATUS NTAPI
NtEnumerateBootEntries(__out_bcount_opt(*BufferLength) PVOID Buffer,
                       __inout PULONG BufferLength);

NTSTATUS NTAPI
NtEnumerateDriverEntries(__out_bcount(*BufferLength) PVOID Buffer,
                         __inout PULONG BufferLength);

NTSTATUS NTAPI
NtEnumerateSystemEnvironmentValuesEx(__in ULONG InformationClass, __out PVOID Buffer,
                                     __inout PULONG BufferLength);

NTSTATUS NTAPI
NtQueryBootEntryOrder(__out_ecount_opt(*Count) PULONG Ids, __inout PULONG Count);

NTSTATUS NTAPI
NtQueryBootOptions(__out_bcount_opt(*BootOptionsLength) PBOOT_OPTIONS BootOptions,
                   __inout PULONG BootOptionsLength);

NTSTATUS NTAPI
NtQueryDriverEntryOrder(__out_ecount(*Count) PULONG Ids, __inout PULONG Count);

NTSTATUS NTAPI
NtQuerySystemEnvironmentValueEx(__in PUNICODE_STRING VariableName, __in LPGUID VendorGuid,
                                __out_bcount_opt(*ValueLength) PVOID Value,
                                __inout PULONG ValueLength, __out_opt PULONG Attributes);

NTSTATUS NTAPI
NtSetBootEntryOrder(__in_ecount(Count) PULONG Ids, __in ULONG Count);

NTSTATUS NTAPI
NtSetDriverEntryOrder(__in_ecount(Count) PULONG Ids, __in ULONG Count);

NTSTATUS
NTAPI
NtQuerySystemInformationEx(__in SYSTEM_INFORMATION_CLASS SystemInformationClass,
                           __in_bcount(QueryInformationLength) PVOID QueryInformation,
                           __in ULONG QueryInformationLength,
                           __out_bcount_opt(SystemInformationLength)
                               PVOID SystemInformation,
                           __in ULONG SystemInformationLength,
                           __out_opt PULONG ReturnLength);

NTSTATUS
NTAPI
NtInitializeNlsFiles(__out PVOID *BaseAddress, __out PLCID DefaultLocaleId,
                     __out PLARGE_INTEGER DefaultCasingTableSize);

NTSTATUS
NTAPI
NtAcquireCMFViewOwnership(__out PULONGLONG TimeStamp, __out PBOOLEAN tokenTaken,
                          __in BOOLEAN replaceExisting);

NTSTATUS
NTAPI
NtCreateProfileEx(__out PHANDLE ProfileHandle, __in_opt HANDLE Process,
                  __in PVOID ProfileBase, __in SIZE_T ProfileSize, __in ULONG BucketSize,
                  __in PULONG Buffer, __in ULONG BufferSize,
                  __in KPROFILE_SOURCE ProfileSource, __in ULONG GroupAffinityCount,
                  __in_opt PGROUP_AFFINITY GroupAffinity);

NTSTATUS
NTAPI
NtCreateWorkerFactory(__out PHANDLE WorkerFactoryHandleReturn,
                      __in ACCESS_MASK DesiredAccess,
                      __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
                      __in HANDLE CompletionPortHandle, __in HANDLE WorkerProcessHandle,
                      __in PVOID StartRoutine, __in_opt PVOID StartParameter,
                      __in_opt ULONG MaxThreadCount, __in_opt SIZE_T StackReserve,
                      __in_opt SIZE_T StackCommit);

NTSTATUS
NTAPI
NtFlushInstallUILanguage(__in LANGID InstallUILanguage, __in ULONG SetComittedFlag);

NTSTATUS
NTAPI
NtGetMUIRegistryInfo(__in ULONG Flags, __inout PULONG DataSize, __out PVOID Data);

NTSTATUS
NTAPI
NtGetNlsSectionPtr(__in ULONG SectionType, __in ULONG SectionData, __in PVOID ContextData,
                   __out PVOID *SectionPointer, __out PULONG SectionSize);

NTSTATUS
NTAPI
NtIsUILanguageComitted(VOID);

NTSTATUS
NTAPI
NtReleaseCMFViewOwnership(VOID);

NTSTATUS
NTAPI
NtReleaseWorkerFactoryWorker(__in HANDLE WorkerFactoryHandle);

NTSTATUS
NTAPI
NtWorkerFactoryWorkerReady(__in HANDLE WorkerFactoryHandle);

NTSTATUS
NTAPI
NtQueryInformationWorkerFactory(__in HANDLE WorkerFactoryHandle,
                                __in WORKERFACTORYINFOCLASS WorkerFactoryInformationClass,
                                __out_bcount(WorkerFactoryInformationLength)
                                    PVOID WorkerFactoryInformation,
                                __in ULONG WorkerFactoryInformationLength,
                                __out_opt PULONG ReturnLength);

NTSTATUS
NTAPI
NtSetInformationWorkerFactory(__in HANDLE WorkerFactoryHandle,
                              __in WORKERFACTORYINFOCLASS WorkerFactoryInformationClass,
                              __in_bcount(WorkerFactoryInformationLength)
                                  PVOID WorkerFactoryInformation,
                              __in ULONG WorkerFactoryInformationLength);

NTSTATUS
NTAPI
NtWaitForWorkViaWorkerFactory(__in HANDLE WorkerFactoryHandle,
                              __out FILE_IO_COMPLETION_INFORMATION *MiniPacket);

NTSTATUS
NTAPI
NtShutdownWorkerFactory(__in HANDLE WorkerFactoryHandle,
                        __inout volatile LONG *PendingWorkerCount);

NTSTATUS
NTAPI
NtSetTimerEx(__in HANDLE TimerHandle,
             __in TIMER_SET_INFORMATION_CLASS TimerSetInformationClass,
             __inout_bcount_opt(TimerSetInformationLength) PVOID TimerSetInformation,
             __in ULONG TimerSetInformationLength);

NTSTATUS
NTAPI
NtCancelTimer2(__in HANDLE TimerHandle, __out_opt PBOOLEAN CurrentState);

NTSTATUS
NTAPI
NtSetTimer2(__in HANDLE TimerHandle, __in PLARGE_INTEGER DueTime,
            __in_opt PLARGE_INTEGER Period, __in PT2_SET_PARAMETERS Parameters);

typedef struct _WNF_STATE_NAME {
    ULONG Data[2];
} WNF_STATE_NAME, *PWNF_STATE_NAME;

typedef const WNF_STATE_NAME *PCWNF_STATE_NAME;

typedef enum _WNF_STATE_NAME_LIFETIME {
    WnfWellKnownStateName,
    WnfPermanentStateName,
    WnfPersistentStateName,
    WnfTemporaryStateName
} WNF_STATE_NAME_LIFETIME;

typedef enum _WNF_STATE_NAME_INFORMATION {
    WnfInfoStateNameExist,
    WnfInfoSubscribersPresent,
    WnfInfoIsQuiescent
} WNF_STATE_NAME_INFORMATION;

typedef enum _WNF_DATA_SCOPE {
    WnfDataScopeSystem,
    WnfDataScopeSession,
    WnfDataScopeUser,
    WnfDataScopeProcess
} WNF_DATA_SCOPE;

typedef struct _WNF_TYPE_ID {
    GUID TypeId;
} WNF_TYPE_ID, *PWNF_TYPE_ID;

typedef const WNF_TYPE_ID *PCWNF_TYPE_ID;

typedef ULONG WNF_CHANGE_STAMP, *PWNF_CHANGE_STAMP;

typedef ULONG LOGICAL;
typedef ULONG *PLOGICAL;

NTSTATUS
NTAPI
NtQueryWnfStateData(_In_ PCWNF_STATE_NAME StateName, _In_opt_ PCWNF_TYPE_ID TypeId,
                    _In_opt_ const VOID *ExplicitScope,
                    _Out_ PWNF_CHANGE_STAMP ChangeStamp,
                    _Out_writes_bytes_to_opt_(*BufferSize, *BufferSize) PVOID Buffer,
                    _Inout_ PULONG BufferSize);

NTSTATUS
NTAPI
NtUpdateWnfStateData(_In_ PCWNF_STATE_NAME StateName,
                     _In_reads_bytes_opt_(Length) const VOID *Buffer,
                     _In_opt_ ULONG Length, _In_opt_ PCWNF_TYPE_ID TypeId,
                     _In_opt_ const PVOID ExplicitScope,
                     _In_ WNF_CHANGE_STAMP MatchingChangeStamp, _In_ LOGICAL CheckStamp);

#endif /* _NTEXAPI_H_ 1 */
