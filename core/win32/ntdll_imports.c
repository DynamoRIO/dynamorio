/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* i#894: Win8 WDK ntdll.lib does not list Ki routines so we make our own .lib.
 * Since they're stdcall we need to both list them in .def and to have stubs
 * in an .obj file.
 */
/* i#1011: to support old Windows, only functions on win-2K+ can be added */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ntdll_types.h"

#pragma warning(disable : 4100) /* unreferenced formal parameter */

#ifdef __cplusplus
extern "C" {
#endif

#define NTEXPORT __declspec(dllexport)

/***************************************************************************
 * Ki
 */

NTEXPORT void NTAPI
KiUserApcDispatcher(void *Unknown1, void *Unknown2, void *Unknown3, void *ContextStart,
                    void *ContextBody)
{
}

NTEXPORT void NTAPI
KiUserCallbackDispatcher(void *Unknown1, void *Unknown2, void *Unknown3)
{
}

NTEXPORT void NTAPI
KiUserExceptionDispatcher(void *Unknown1, void *Unknown2)
{
}

NTEXPORT void NTAPI
KiRaiseUserExceptionDispatcher(void)
{
}

/***************************************************************************
 * Nt
 */

NTEXPORT NTSTATUS NTAPI
NtAllocateVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                        IN ULONG ZeroBits, IN OUT PULONG AllocationSize,
                        IN ULONG AllocationType, IN ULONG Protect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryVirtualMemory(IN HANDLE ProcessHandle, IN const void *BaseAddress,
                     IN MEMORY_INFORMATION_CLASS MemoryInformationClass,
                     OUT PVOID MemoryInformation, IN SIZE_T MemoryInformationLength,
                     OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFreeVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                    IN OUT PSIZE_T FreeSize, IN ULONG FreeType)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtProtectVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                       IN OUT PULONG ProtectSize, IN ULONG NewProtect,
                       OUT PULONG OldProtect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateFile(OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
             IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock,
             IN PLARGE_INTEGER AllocationSize OPTIONAL, IN ULONG FileAttributes,
             IN ULONG ShareAccess, IN ULONG CreateDisposition, IN ULONG CreateOptions,
             IN PVOID EaBuffer OPTIONAL, IN ULONG EaLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWriteVirtualMemory(IN HANDLE ProcessHandle, IN PVOID BaseAddress, IN const void *Buffer,
                     IN SIZE_T BufferLength, OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtGetContextThread(IN HANDLE ThreadHandle, OUT PCONTEXT Context)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetContextThread(IN HANDLE ThreadHandle, IN PCONTEXT Context)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSuspendThread(IN HANDLE ThreadHandle, OUT PULONG PreviousSuspendCount OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtResumeThread(IN HANDLE ThreadHandle, OUT PULONG PreviousSuspendCount OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTerminateThread(IN HANDLE ThreadHandle OPTIONAL, IN NTSTATUS ExitStatus)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtClose(IN HANDLE Handle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDuplicateObject(IN HANDLE SourceProcessHandle, IN HANDLE SourceHandle,
                  IN HANDLE TargetProcessHandle, OUT PHANDLE TargetHandle OPTIONAL,
                  IN ACCESS_MASK DesiredAcess, IN ULONG Atrributes, IN ULONG options_t)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateSection(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes,
                IN PLARGE_INTEGER SectionSize OPTIONAL, IN ULONG Protect,
                IN ULONG Attributes, IN HANDLE FileHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenSection(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
              IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtMapViewOfSection(IN HANDLE SectionHandle, IN HANDLE ProcessHandle,
                   IN OUT PVOID *BaseAddress, IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize,
                   IN OUT PLARGE_INTEGER SectionOffset OPTIONAL, IN OUT PSIZE_T ViewSize,
                   IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType,
                   IN ULONG Protect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtUnmapViewOfSection(IN HANDLE ProcessHandle, IN PVOID BaseAddress)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCallbackReturn(IN PVOID Result OPTIONAL, IN ULONG ResultLength, IN NTSTATUS Status)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTestAlert(void)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtContinue(IN PCONTEXT Context, IN BOOLEAN TestAlert)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationThread(IN HANDLE ThreadHandle,
                         IN THREADINFOCLASS ThreadInformationClass,
                         OUT PVOID ThreadInformation, IN ULONG ThreadInformationLength,
                         OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationProcess(IN HANDLE ProcessHandle,
                          IN PROCESSINFOCLASS ProcessInformationClass,
                          OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
                          OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationFile(IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock,
                       OUT PVOID FileInformation, IN ULONG FileInformationLength,
                       IN FILE_INFORMATION_CLASS FileInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationFile(IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock,
                     IN PVOID FileInformation, IN ULONG FileInformationLength,
                     IN FILE_INFORMATION_CLASS FileInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySection(IN HANDLE SectionHandle,
               IN SECTION_INFORMATION_CLASS SectionInformationClass,
               OUT PVOID SectionInformation, IN ULONG SectionInformationLength,
               OUT PULONG ResultLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenFile(OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
           IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock,
           IN ULONG ShareAccess, IN ULONG OpenOptions)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenThreadToken(IN HANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
                  IN BOOLEAN OpenAsSelf, OUT PHANDLE TokenHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenProcessToken(IN HANDLE ProcessToken, IN ACCESS_MASK DesiredAccess,
                   OUT PHANDLE TokenHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationToken(IN HANDLE TokenHandle,
                        IN TOKEN_INFORMATION_CLASS TokenInformationClass,
                        OUT PVOID TokenInformation, IN ULONG TokenInformationLength,
                        OUT PULONG ReturnLength)
{
    return STATUS_SUCCESS;
}

/* clang-format off */ /* (work around clang-format bug) */
NTEXPORT NTSTATUS NTAPI
NtYieldExecution(VOID)
/* clang-format on */
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySystemInformation(IN SYSTEM_INFORMATION_CLASS info_class, OUT PVOID info,
                         IN ULONG info_size, OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenProcess(OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess,
              IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationThread(IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInformationClass,
                       IN PVOID ThreadInformation, IN ULONG ThreadInformationLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtReadVirtualMemory(IN HANDLE ProcessHandle, IN const void *BaseAddress, OUT PVOID Buffer,
                    IN SIZE_T BufferLength, OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateTimer(OUT PHANDLE TimerHandle, IN ACCESS_MASK DesiredAccess,
              IN POBJECT_ATTRIBUTES ObjectAttributes, IN DWORD TimerType /* TIMER_TYPE */)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetTimer(IN HANDLE TimerHandle, IN PLARGE_INTEGER DueTime,
           IN PVOID TimerApcRoutine, /* PTIMER_APC_ROUTINE */
           IN PVOID TimerContext, IN BOOLEAN Resume, IN LONG Period,
           OUT PBOOLEAN PreviousState)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDelayExecution(IN BOOLEAN Alertable, IN PLARGE_INTEGER Interval)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryObject(IN HANDLE ObjectHandle, IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
              OUT PVOID ObjectInformation, IN ULONG ObjectInformationLength,
              OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryFullAttributesFile(IN POBJECT_ATTRIBUTES attributes,
                          OUT PFILE_NETWORK_OPEN_INFORMATION info)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateKey(OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess,
            IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG TitleIndex,
            IN PUNICODE_STRING Class OPTIONAL, IN ULONG CreateOptions,
            OUT PULONG Disposition OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenKey(OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess,
          IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetValueKey(IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName,
              IN ULONG TitleIndex OPTIONAL, IN ULONG Type, IN PVOID Data,
              IN ULONG DataSize)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeleteKey(IN HANDLE KeyHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryValueKey(IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName,
                IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                OUT PVOID KeyValueInformation, IN ULONG Length, OUT PULONG ResultLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushKey(IN HANDLE KeyHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtEnumerateKey(IN HANDLE hkey, IN ULONG index, IN KEY_INFORMATION_CLASS info_class,
               OUT PVOID key_info, IN ULONG key_info_size, OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtEnumerateValueKey(IN HANDLE hKey, IN ULONG index,
                    IN KEY_VALUE_INFORMATION_CLASS info_class, OUT PVOID key_info,
                    IN ULONG key_info_size, OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySystemTime(IN PLARGE_INTEGER SystemTime)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeleteFile(IN POBJECT_ATTRIBUTES ObjectAttributes)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushBuffersFile(IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtReadFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
           IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
           OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID Buffer, IN ULONG Length,
           IN PLARGE_INTEGER ByteOffset OPTIONAL, IN PULONG Key OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWriteFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
            IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
            OUT PIO_STATUS_BLOCK IoStatusBlock,
            IN const void *Buffer, /* PVOID, but need const */
            IN ULONG Length, IN PLARGE_INTEGER ByteOffset OPTIONAL,
            IN PULONG Key OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateIoCompletion(OUT PHANDLE IoCompletionHandle, IN ACCESS_MASK DesiredAccess,
                     IN POBJECT_ATTRIBUTES ObjectAttributes,
                     IN ULONG NumberOfConcurrentThreads)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtRaiseHardError(IN NTSTATUS ErrorStatus, IN ULONG NumberOfArguments,
                 /* FIXME: ReactOS claims this is a PUNICODE_STRING */
                 IN ULONG UnicodeStringArgumentsMask, IN PVOID Arguments,
                 IN ULONG MessageBoxType, /* HARDERROR_RESPONSE_OPTION */
                 OUT PULONG MessageBoxResult)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtRaiseException(IN PEXCEPTION_RECORD ExceptionRecord, IN PCONTEXT Context,
                 IN BOOLEAN SearchFrames)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateEvent(OUT PHANDLE EventHandle, IN ACCESS_MASK DesiredAccess,
              IN POBJECT_ATTRIBUTES ObjectAttributes, IN EVENT_TYPE EventType,
              IN BOOLEAN InitialState)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWaitForSingleObject(IN HANDLE ObjectHandle, IN BOOLEAN Alertable,
                      IN PLARGE_INTEGER TimeOut)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetEvent(IN HANDLE EventHandle, OUT PLONG PreviousState OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtClearEvent(IN HANDLE EventHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSignalAndWaitForSingleObject(IN HANDLE ObjectToSignal, IN HANDLE WaitableObject,
                               IN BOOLEAN Alertable, IN PLARGE_INTEGER Time OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryPerformanceCounter(OUT PLARGE_INTEGER PerformanceCount,
                          OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFsControlFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
                IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
                OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG FsControlCode,
                IN PVOID InputBuffer OPTIONAL, IN ULONG InputBufferLength,
                OUT PVOID OutputBuffer OPTIONAL, IN ULONG OutputBufferLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCancelIoFile(IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateProfile(OUT PHANDLE ProfileHandle, IN HANDLE ProcessHandle, IN PVOID Base,
                IN ULONG Size, IN ULONG BucketShift, IN PULONG Buffer,
                IN ULONG BufferLength, IN KPROFILE_SOURCE Source, IN ULONG ProcessorMask)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetIntervalProfile(IN ULONG Interval, IN KPROFILE_SOURCE Source)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryIntervalProfile(IN KPROFILE_SOURCE Source, OUT PULONG Interval)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtStartProfile(IN HANDLE ProfileHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtStopProfile(IN HANDLE ProfileHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateProcess(OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes,
                IN HANDLE InheritFromProcessHandle, IN BOOLEAN InheritHandles,
                IN HANDLE SectionHandle OPTIONAL, IN HANDLE DebugPort OPTIONAL,
                IN HANDLE ExceptionPort OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTerminateProcess(IN HANDLE ProcessHandle OPTIONAL, IN NTSTATUS ExitStatus)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationProcess(IN HANDLE hprocess, IN PROCESSINFOCLASS class, IN OUT void *info,
                        IN ULONG info_len)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateDirectoryObject(OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
                        IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenDirectoryObject(OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
                      IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenSymbolicLinkObject(OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
                         IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySymbolicLinkObject(IN HANDLE DirectoryHandle, IN OUT PUNICODE_STRING TargetName,
                          OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
ZwAreMappedFilesTheSame(IN PVOID Address1, IN PVOID Address2)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryVolumeInformationFile(IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock,
                             OUT PVOID FsInformation, IN ULONG Length,
                             IN FS_INFORMATION_CLASS FsInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySecurityObject(IN HANDLE Handle, IN SECURITY_INFORMATION RequestedInformation,
                      OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
                      IN ULONG SecurityDescriptorLength, OUT PULONG ReturnLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueueApcThread(IN HANDLE ThreadHandle, IN PKNORMAL_ROUTINE ApcRoutine,
                 IN PVOID ApcContext OPTIONAL, IN PVOID Argument1 OPTIONAL,
                 IN PVOID Argument2 OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateThread(OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
               OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext,
               IN PUSER_STACK UserStack, IN BOOLEAN CreateSuspended)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenThread(OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
             IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushInstructionCache(IN HANDLE ProcessHandle, IN PVOID BaseAddress OPTIONAL,
                        IN SIZE_T FlushSize)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                      PFILE_BASIC_INFORMATION file_information)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateNamedPipeFile(OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
                      IN POBJECT_ATTRIBUTES ObjectAttributes,
                      OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG ShareAccess,
                      IN ULONG CreateDisposition, IN ULONG CreateOptions,
                      IN BOOLEAN TypeMessage, IN BOOLEAN ReadmodeMessage,
                      IN BOOLEAN Nonblocking, IN ULONG MaxInstances,
                      IN ULONG InBufferSize, IN ULONG OutBufferSize,
                      IN PLARGE_INTEGER DefaultTimeout OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeviceIoControlFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
                      IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                      IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock,
                      IN ULONG IoControlCode, IN PVOID InputBuffer OPTIONAL,
                      IN ULONG InputBufferLength, OUT PVOID OutputBuffer OPTIONAL,
                      IN ULONG OutputBufferLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryDirectoryFile(IN HANDLE FileHandle, IN HANDLE Event OPTIONAL,
                     IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL,
                     OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID FileInformation,
                     IN ULONG FileInformationLength,
                     IN FILE_INFORMATION_CLASS FileInformationClass,
                     IN BOOLEAN ReturnSingleEntry, IN PUNICODE_STRING FileName OPTIONAL,
                     IN BOOLEAN RestartScan)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushVirtualMemory(IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                     IN OUT PULONG_PTR FlushSize, OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationJobObject(IN HANDLE JobHandle,
                            IN JOBOBJECTINFOCLASS JobInformationClass,
                            OUT PVOID JobInformation, IN ULONG JobInformationLength,
                            OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * RTL
 */

NTEXPORT NTSTATUS NTAPI
RtlInitializeCriticalSection(OUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlEnterCriticalSection(IN OUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlLeaveCriticalSection(IN OUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlTryEnterCriticalSection(IN OUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlInitUnicodeString(IN OUT PUNICODE_STRING DestinationString, IN PCWSTR SourceString)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlInitAnsiString(IN OUT PANSI_STRING DestinationString, IN PCSTR SourceString)
{
    return STATUS_SUCCESS;
}

NTEXPORT void NTAPI
RtlFreeUnicodeString(UNICODE_STRING *string)
{
}

NTEXPORT void NTAPI
RtlFreeAnsiString(ANSI_STRING *string)
{
}

NTEXPORT void NTAPI
RtlFreeOemString(OEM_STRING *string)
{
}

NTEXPORT NTSTATUS NTAPI
RtlQueryEnvironmentVariable_U(PWSTR Environment, PUNICODE_STRING Name,
                              PUNICODE_STRING Value)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlConvertSidToUnicodeString(OUT PUNICODE_STRING UnicodeString, IN PSID Sid,
                             BOOLEAN AllocateDestinationString)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlCreateProcessParameters(
    OUT PRTL_USER_PROCESS_PARAMETERS *ProcessParameters, IN PUNICODE_STRING ImageFile,
    IN PUNICODE_STRING DllPath OPTIONAL, IN PUNICODE_STRING CurrentDirectory OPTIONAL,
    IN PUNICODE_STRING CommandLine OPTIONAL, IN ULONG CreationFlags,
    IN PUNICODE_STRING WindowTitle OPTIONAL, IN PUNICODE_STRING Desktop OPTIONAL,
    IN PUNICODE_STRING Reserved OPTIONAL, IN PUNICODE_STRING Reserved2 OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlDestroyProcessParameters(IN PRTL_USER_PROCESS_PARAMETERS ProcessParameters)

{
    return STATUS_SUCCESS;
}

NTEXPORT HANDLE NTAPI
RtlCreateHeap(ULONG flags, void *base, size_t reserve_sz, size_t commit_sz, void *lock,
              void *params)
{
    return INVALID_HANDLE_VALUE;
}

NTEXPORT BOOL NTAPI
RtlDestroyHeap(HANDLE base)
{
    return STATUS_SUCCESS;
}

NTEXPORT void *NTAPI
RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size)
{
    return NULL;
}

NTEXPORT void *NTAPI
RtlReAllocateHeap(HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size)
{
    return NULL;
}

NTEXPORT BOOL NTAPI
RtlFreeHeap(HANDLE heap, ULONG flags, PVOID ptr)
{
    return FALSE;
}

NTEXPORT SIZE_T NTAPI
RtlSizeHeap(HANDLE heap, ULONG flags, PVOID ptr)
{
    return 0;
}

NTEXPORT BOOL NTAPI
RtlValidateHeap(HANDLE heap, DWORD flags, void *ptr)
{
    return FALSE;
}

NTEXPORT BOOL NTAPI
RtlLockHeap(HANDLE heap)
{
    return FALSE;
}

NTEXPORT BOOL NTAPI
RtlUnlockHeap(HANDLE heap)
{
    return FALSE;
}

NTEXPORT NTSTATUS NTAPI
RtlGetProcessHeaps(IN ULONG count, OUT HANDLE *Heaps)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlWalkHeap(IN HANDLE Heap, OUT PVOID Info)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * Ldr
 */

NTEXPORT NTSTATUS NTAPI
LdrLoadDll(IN PCWSTR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL,
           IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrUnloadDll(IN HANDLE ModuleHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrGetProcedureAddress(IN HANDLE ModuleHandle, IN PANSI_STRING ProcedureName OPTIONAL,
                       IN ULONG Ordinal OPTIONAL, OUT FARPROC *ProcedureAddress)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrGetDllHandle(IN PCWSTR PathToFile OPTIONAL, IN ULONG Unused OPTIONAL,
                IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * Csr
 */

NTEXPORT NTSTATUS NTAPI
CsrClientCallServer(IN OUT PVOID Message, IN PVOID unknown, IN ULONG Opcode,
                    IN ULONG Size)
{
    return STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif
