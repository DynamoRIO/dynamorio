/* **********************************************************
 * Copyright (c) 2012-2023 Google, Inc.  All rights reserved.
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

#ifndef DR_PARAM_IN
#    define DR_PARAM_IN
#endif
#ifndef DR_PARAM_OUT
#    define DR_PARAM_OUT
#endif
#ifndef DR_PARAM_INOUT
#    define DR_PARAM_INOUT
#endif

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
NtAllocateVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle,
                        DR_PARAM_INOUT PVOID *BaseAddress, DR_PARAM_IN ULONG ZeroBits,
                        DR_PARAM_INOUT PULONG AllocationSize,
                        DR_PARAM_IN ULONG AllocationType, DR_PARAM_IN ULONG Protect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle,
                     DR_PARAM_IN const void *BaseAddress,
                     DR_PARAM_IN MEMORY_INFORMATION_CLASS MemoryInformationClass,
                     DR_PARAM_OUT PVOID MemoryInformation,
                     DR_PARAM_IN SIZE_T MemoryInformationLength,
                     DR_PARAM_OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFreeVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_INOUT PVOID *BaseAddress,
                    DR_PARAM_INOUT PSIZE_T FreeSize, DR_PARAM_IN ULONG FreeType)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtProtectVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle,
                       DR_PARAM_INOUT PVOID *BaseAddress,
                       DR_PARAM_INOUT PULONG ProtectSize, DR_PARAM_IN ULONG NewProtect,
                       DR_PARAM_OUT PULONG OldProtect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateFile(DR_PARAM_OUT PHANDLE FileHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
             DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
             DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
             DR_PARAM_IN PLARGE_INTEGER AllocationSize OPTIONAL,
             DR_PARAM_IN ULONG FileAttributes, DR_PARAM_IN ULONG ShareAccess,
             DR_PARAM_IN ULONG CreateDisposition, DR_PARAM_IN ULONG CreateOptions,
             DR_PARAM_IN PVOID EaBuffer OPTIONAL, DR_PARAM_IN ULONG EaLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWriteVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_IN PVOID BaseAddress,
                     DR_PARAM_IN const void *Buffer, DR_PARAM_IN SIZE_T BufferLength,
                     DR_PARAM_OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtGetContextThread(DR_PARAM_IN HANDLE ThreadHandle, DR_PARAM_OUT PCONTEXT Context)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetContextThread(DR_PARAM_IN HANDLE ThreadHandle, DR_PARAM_IN PCONTEXT Context)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSuspendThread(DR_PARAM_IN HANDLE ThreadHandle,
                DR_PARAM_OUT PULONG PreviousSuspendCount OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtResumeThread(DR_PARAM_IN HANDLE ThreadHandle,
               DR_PARAM_OUT PULONG PreviousSuspendCount OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTerminateThread(DR_PARAM_IN HANDLE ThreadHandle OPTIONAL,
                  DR_PARAM_IN NTSTATUS ExitStatus)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtClose(DR_PARAM_IN HANDLE Handle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDuplicateObject(DR_PARAM_IN HANDLE SourceProcessHandle, DR_PARAM_IN HANDLE SourceHandle,
                  DR_PARAM_IN HANDLE TargetProcessHandle,
                  DR_PARAM_OUT PHANDLE TargetHandle OPTIONAL,
                  DR_PARAM_IN ACCESS_MASK DesiredAcess, DR_PARAM_IN ULONG Atrributes,
                  DR_PARAM_IN ULONG options_t)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateSection(DR_PARAM_OUT PHANDLE SectionHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
                DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
                DR_PARAM_IN PLARGE_INTEGER SectionSize OPTIONAL,
                DR_PARAM_IN ULONG Protect, DR_PARAM_IN ULONG Attributes,
                DR_PARAM_IN HANDLE FileHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenSection(DR_PARAM_OUT PHANDLE SectionHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
              DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtMapViewOfSection(DR_PARAM_IN HANDLE SectionHandle, DR_PARAM_IN HANDLE ProcessHandle,
                   DR_PARAM_INOUT PVOID *BaseAddress, DR_PARAM_IN ULONG_PTR ZeroBits,
                   DR_PARAM_IN SIZE_T CommitSize,
                   DR_PARAM_INOUT PLARGE_INTEGER SectionOffset OPTIONAL,
                   DR_PARAM_INOUT PSIZE_T ViewSize,
                   DR_PARAM_IN SECTION_INHERIT InheritDisposition,
                   DR_PARAM_IN ULONG AllocationType, DR_PARAM_IN ULONG Protect)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtUnmapViewOfSection(DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_IN PVOID BaseAddress)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCallbackReturn(DR_PARAM_IN PVOID Result OPTIONAL, DR_PARAM_IN ULONG ResultLength,
                 DR_PARAM_IN NTSTATUS Status)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTestAlert(void)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtContinue(DR_PARAM_IN PCONTEXT Context, DR_PARAM_IN BOOLEAN TestAlert)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationThread(DR_PARAM_IN HANDLE ThreadHandle,
                         DR_PARAM_IN THREADINFOCLASS ThreadInformationClass,
                         DR_PARAM_OUT PVOID ThreadInformation,
                         DR_PARAM_IN ULONG ThreadInformationLength,
                         DR_PARAM_OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationProcess(DR_PARAM_IN HANDLE ProcessHandle,
                          DR_PARAM_IN PROCESSINFOCLASS ProcessInformationClass,
                          DR_PARAM_OUT PVOID ProcessInformation,
                          DR_PARAM_IN ULONG ProcessInformationLength,
                          DR_PARAM_OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationFile(DR_PARAM_IN HANDLE FileHandle,
                       DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                       DR_PARAM_OUT PVOID FileInformation,
                       DR_PARAM_IN ULONG FileInformationLength,
                       DR_PARAM_IN FILE_INFORMATION_CLASS FileInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationFile(DR_PARAM_IN HANDLE FileHandle,
                     DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                     DR_PARAM_IN PVOID FileInformation,
                     DR_PARAM_IN ULONG FileInformationLength,
                     DR_PARAM_IN FILE_INFORMATION_CLASS FileInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySection(DR_PARAM_IN HANDLE SectionHandle,
               DR_PARAM_IN SECTION_INFORMATION_CLASS SectionInformationClass,
               DR_PARAM_OUT PVOID SectionInformation,
               DR_PARAM_IN ULONG SectionInformationLength,
               DR_PARAM_OUT PULONG ResultLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenFile(DR_PARAM_OUT PHANDLE FileHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
           DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
           DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock, DR_PARAM_IN ULONG ShareAccess,
           DR_PARAM_IN ULONG OpenOptions)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenThreadToken(DR_PARAM_IN HANDLE ThreadHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
                  DR_PARAM_IN BOOLEAN OpenAsSelf, DR_PARAM_OUT PHANDLE TokenHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenProcessToken(DR_PARAM_IN HANDLE ProcessToken, DR_PARAM_IN ACCESS_MASK DesiredAccess,
                   DR_PARAM_OUT PHANDLE TokenHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationToken(DR_PARAM_IN HANDLE TokenHandle,
                        DR_PARAM_IN TOKEN_INFORMATION_CLASS TokenInformationClass,
                        DR_PARAM_OUT PVOID TokenInformation,
                        DR_PARAM_IN ULONG TokenInformationLength,
                        DR_PARAM_OUT PULONG ReturnLength)
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
NtQuerySystemInformation(DR_PARAM_IN SYSTEM_INFORMATION_CLASS info_class,
                         DR_PARAM_OUT PVOID info, DR_PARAM_IN ULONG info_size,
                         DR_PARAM_OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenProcess(DR_PARAM_OUT PHANDLE ProcessHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
              DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
              DR_PARAM_IN PCLIENT_ID ClientId)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationThread(DR_PARAM_IN HANDLE ThreadHandle,
                       DR_PARAM_IN THREADINFOCLASS ThreadInformationClass,
                       DR_PARAM_IN PVOID ThreadInformation,
                       DR_PARAM_IN ULONG ThreadInformationLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtReadVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_IN const void *BaseAddress,
                    DR_PARAM_OUT PVOID Buffer, DR_PARAM_IN SIZE_T BufferLength,
                    DR_PARAM_OUT PSIZE_T ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateTimer(DR_PARAM_OUT PHANDLE TimerHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
              DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
              DR_PARAM_IN DWORD TimerType /* TIMER_TYPE */)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetTimer(DR_PARAM_IN HANDLE TimerHandle, DR_PARAM_IN PLARGE_INTEGER DueTime,
           DR_PARAM_IN PVOID TimerApcRoutine, /* PTIMER_APC_ROUTINE */
           DR_PARAM_IN PVOID TimerContext, DR_PARAM_IN BOOLEAN Resume,
           DR_PARAM_IN LONG Period, DR_PARAM_OUT PBOOLEAN PreviousState)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDelayExecution(DR_PARAM_IN BOOLEAN Alertable, DR_PARAM_IN PLARGE_INTEGER Interval)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryObject(DR_PARAM_IN HANDLE ObjectHandle,
              DR_PARAM_IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
              DR_PARAM_OUT PVOID ObjectInformation,
              DR_PARAM_IN ULONG ObjectInformationLength,
              DR_PARAM_OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryFullAttributesFile(DR_PARAM_IN POBJECT_ATTRIBUTES attributes,
                          DR_PARAM_OUT PFILE_NETWORK_OPEN_INFORMATION info)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateKey(DR_PARAM_OUT PHANDLE KeyHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
            DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes, DR_PARAM_IN ULONG TitleIndex,
            DR_PARAM_IN PUNICODE_STRING Class OPTIONAL, DR_PARAM_IN ULONG CreateOptions,
            DR_PARAM_OUT PULONG Disposition OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenKey(DR_PARAM_OUT PHANDLE KeyHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
          DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetValueKey(DR_PARAM_IN HANDLE KeyHandle, DR_PARAM_IN PUNICODE_STRING ValueName,
              DR_PARAM_IN ULONG TitleIndex OPTIONAL, DR_PARAM_IN ULONG Type,
              DR_PARAM_IN PVOID Data, DR_PARAM_IN ULONG DataSize)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeleteKey(DR_PARAM_IN HANDLE KeyHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryValueKey(DR_PARAM_IN HANDLE KeyHandle, DR_PARAM_IN PUNICODE_STRING ValueName,
                DR_PARAM_IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                DR_PARAM_OUT PVOID KeyValueInformation, DR_PARAM_IN ULONG Length,
                DR_PARAM_OUT PULONG ResultLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushKey(DR_PARAM_IN HANDLE KeyHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtEnumerateKey(DR_PARAM_IN HANDLE hkey, DR_PARAM_IN ULONG index,
               DR_PARAM_IN KEY_INFORMATION_CLASS info_class, DR_PARAM_OUT PVOID key_info,
               DR_PARAM_IN ULONG key_info_size, DR_PARAM_OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtEnumerateValueKey(DR_PARAM_IN HANDLE hKey, DR_PARAM_IN ULONG index,
                    DR_PARAM_IN KEY_VALUE_INFORMATION_CLASS info_class,
                    DR_PARAM_OUT PVOID key_info, DR_PARAM_IN ULONG key_info_size,
                    DR_PARAM_OUT PULONG bytes_received)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySystemTime(DR_PARAM_IN PLARGE_INTEGER SystemTime)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeleteFile(DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushBuffersFile(DR_PARAM_IN HANDLE FileHandle,
                   DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtReadFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_IN HANDLE Event OPTIONAL,
           DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
           DR_PARAM_IN PVOID ApcContext OPTIONAL,
           DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock, DR_PARAM_OUT PVOID Buffer,
           DR_PARAM_IN ULONG Length, DR_PARAM_IN PLARGE_INTEGER ByteOffset OPTIONAL,
           DR_PARAM_IN PULONG Key OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWriteFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_IN HANDLE Event OPTIONAL,
            DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
            DR_PARAM_IN PVOID ApcContext OPTIONAL,
            DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
            DR_PARAM_IN const void *Buffer, /* PVOID, but need const */
            DR_PARAM_IN ULONG Length, DR_PARAM_IN PLARGE_INTEGER ByteOffset OPTIONAL,
            DR_PARAM_IN PULONG Key OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateIoCompletion(DR_PARAM_OUT PHANDLE IoCompletionHandle,
                     DR_PARAM_IN ACCESS_MASK DesiredAccess,
                     DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
                     DR_PARAM_IN ULONG NumberOfConcurrentThreads)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtRaiseHardError(DR_PARAM_IN NTSTATUS ErrorStatus, DR_PARAM_IN ULONG NumberOfArguments,
                 /* FIXME: ReactOS claims this is a PUNICODE_STRING */
                 DR_PARAM_IN ULONG UnicodeStringArgumentsMask,
                 DR_PARAM_IN PVOID Arguments,
                 DR_PARAM_IN ULONG MessageBoxType, /* HARDERROR_RESPONSE_OPTION */
                 DR_PARAM_OUT PULONG MessageBoxResult)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtRaiseException(DR_PARAM_IN PEXCEPTION_RECORD ExceptionRecord,
                 DR_PARAM_IN PCONTEXT Context, DR_PARAM_IN BOOLEAN SearchFrames)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateEvent(DR_PARAM_OUT PHANDLE EventHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
              DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
              DR_PARAM_IN EVENT_TYPE EventType, DR_PARAM_IN BOOLEAN InitialState)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtWaitForSingleObject(DR_PARAM_IN HANDLE ObjectHandle, DR_PARAM_IN BOOLEAN Alertable,
                      DR_PARAM_IN PLARGE_INTEGER TimeOut)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetEvent(DR_PARAM_IN HANDLE EventHandle, DR_PARAM_OUT PLONG PreviousState OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtClearEvent(DR_PARAM_IN HANDLE EventHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSignalAndWaitForSingleObject(DR_PARAM_IN HANDLE ObjectToSignal,
                               DR_PARAM_IN HANDLE WaitableObject,
                               DR_PARAM_IN BOOLEAN Alertable,
                               DR_PARAM_IN PLARGE_INTEGER Time OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryPerformanceCounter(DR_PARAM_OUT PLARGE_INTEGER PerformanceCount,
                          DR_PARAM_OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFsControlFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_IN HANDLE Event OPTIONAL,
                DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                DR_PARAM_IN PVOID ApcContext OPTIONAL,
                DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                DR_PARAM_IN ULONG FsControlCode, DR_PARAM_IN PVOID InputBuffer OPTIONAL,
                DR_PARAM_IN ULONG InputBufferLength,
                DR_PARAM_OUT PVOID OutputBuffer OPTIONAL,
                DR_PARAM_IN ULONG OutputBufferLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCancelIoFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateProfile(DR_PARAM_OUT PHANDLE ProfileHandle, DR_PARAM_IN HANDLE ProcessHandle,
                DR_PARAM_IN PVOID Base, DR_PARAM_IN ULONG Size,
                DR_PARAM_IN ULONG BucketShift, DR_PARAM_IN PULONG Buffer,
                DR_PARAM_IN ULONG BufferLength, DR_PARAM_IN KPROFILE_SOURCE Source,
                DR_PARAM_IN ULONG ProcessorMask)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetIntervalProfile(DR_PARAM_IN ULONG Interval, DR_PARAM_IN KPROFILE_SOURCE Source)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryIntervalProfile(DR_PARAM_IN KPROFILE_SOURCE Source, DR_PARAM_OUT PULONG Interval)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtStartProfile(DR_PARAM_IN HANDLE ProfileHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtStopProfile(DR_PARAM_IN HANDLE ProfileHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateProcess(DR_PARAM_OUT PHANDLE ProcessHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
                DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
                DR_PARAM_IN HANDLE InheritFromProcessHandle,
                DR_PARAM_IN BOOLEAN InheritHandles,
                DR_PARAM_IN HANDLE SectionHandle OPTIONAL,
                DR_PARAM_IN HANDLE DebugPort OPTIONAL,
                DR_PARAM_IN HANDLE ExceptionPort OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtTerminateProcess(DR_PARAM_IN HANDLE ProcessHandle OPTIONAL,
                   DR_PARAM_IN NTSTATUS ExitStatus)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtSetInformationProcess(DR_PARAM_IN HANDLE hprocess, DR_PARAM_IN PROCESSINFOCLASS class,
                        DR_PARAM_INOUT void *info, DR_PARAM_IN ULONG info_len)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateDirectoryObject(DR_PARAM_OUT PHANDLE DirectoryHandle,
                        DR_PARAM_IN ACCESS_MASK DesiredAccess,
                        DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenDirectoryObject(DR_PARAM_OUT PHANDLE DirectoryHandle,
                      DR_PARAM_IN ACCESS_MASK DesiredAccess,
                      DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenSymbolicLinkObject(DR_PARAM_OUT PHANDLE DirectoryHandle,
                         DR_PARAM_IN ACCESS_MASK DesiredAccess,
                         DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySymbolicLinkObject(DR_PARAM_IN HANDLE DirectoryHandle,
                          DR_PARAM_INOUT PUNICODE_STRING TargetName,
                          DR_PARAM_OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
ZwAreMappedFilesTheSame(DR_PARAM_IN PVOID Address1, DR_PARAM_IN PVOID Address2)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryVolumeInformationFile(DR_PARAM_IN HANDLE FileHandle,
                             DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                             DR_PARAM_OUT PVOID FsInformation, DR_PARAM_IN ULONG Length,
                             DR_PARAM_IN FS_INFORMATION_CLASS FsInformationClass)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQuerySecurityObject(DR_PARAM_IN HANDLE Handle,
                      DR_PARAM_IN SECURITY_INFORMATION RequestedInformation,
                      DR_PARAM_OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
                      DR_PARAM_IN ULONG SecurityDescriptorLength,
                      DR_PARAM_OUT PULONG ReturnLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueueApcThread(DR_PARAM_IN HANDLE ThreadHandle, DR_PARAM_IN PKNORMAL_ROUTINE ApcRoutine,
                 DR_PARAM_IN PVOID ApcContext OPTIONAL,
                 DR_PARAM_IN PVOID Argument1 OPTIONAL,
                 DR_PARAM_IN PVOID Argument2 OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtCreateThread(DR_PARAM_OUT PHANDLE ThreadHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
               DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
               DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_OUT PCLIENT_ID ClientId,
               DR_PARAM_IN PCONTEXT ThreadContext, DR_PARAM_IN PUSER_STACK UserStack,
               DR_PARAM_IN BOOLEAN CreateSuspended)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtOpenThread(DR_PARAM_OUT PHANDLE ThreadHandle, DR_PARAM_IN ACCESS_MASK DesiredAccess,
             DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
             DR_PARAM_IN PCLIENT_ID ClientId)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushInstructionCache(DR_PARAM_IN HANDLE ProcessHandle,
                        DR_PARAM_IN PVOID BaseAddress OPTIONAL,
                        DR_PARAM_IN SIZE_T FlushSize)
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
NtCreateNamedPipeFile(DR_PARAM_OUT PHANDLE FileHandle,
                      DR_PARAM_IN ACCESS_MASK DesiredAccess,
                      DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
                      DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                      DR_PARAM_IN ULONG ShareAccess, DR_PARAM_IN ULONG CreateDisposition,
                      DR_PARAM_IN ULONG CreateOptions, DR_PARAM_IN BOOLEAN TypeMessage,
                      DR_PARAM_IN BOOLEAN ReadmodeMessage,
                      DR_PARAM_IN BOOLEAN Nonblocking, DR_PARAM_IN ULONG MaxInstances,
                      DR_PARAM_IN ULONG InBufferSize, DR_PARAM_IN ULONG OutBufferSize,
                      DR_PARAM_IN PLARGE_INTEGER DefaultTimeout OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtDeviceIoControlFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_IN HANDLE Event OPTIONAL,
                      DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                      DR_PARAM_IN PVOID ApcContext OPTIONAL,
                      DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                      DR_PARAM_IN ULONG IoControlCode,
                      DR_PARAM_IN PVOID InputBuffer OPTIONAL,
                      DR_PARAM_IN ULONG InputBufferLength,
                      DR_PARAM_OUT PVOID OutputBuffer OPTIONAL,
                      DR_PARAM_IN ULONG OutputBufferLength)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryDirectoryFile(DR_PARAM_IN HANDLE FileHandle, DR_PARAM_IN HANDLE Event OPTIONAL,
                     DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                     DR_PARAM_IN PVOID ApcContext OPTIONAL,
                     DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
                     DR_PARAM_OUT PVOID FileInformation,
                     DR_PARAM_IN ULONG FileInformationLength,
                     DR_PARAM_IN FILE_INFORMATION_CLASS FileInformationClass,
                     DR_PARAM_IN BOOLEAN ReturnSingleEntry,
                     DR_PARAM_IN PUNICODE_STRING FileName OPTIONAL,
                     DR_PARAM_IN BOOLEAN RestartScan)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtFlushVirtualMemory(DR_PARAM_IN HANDLE ProcessHandle, DR_PARAM_INOUT PVOID *BaseAddress,
                     DR_PARAM_INOUT PULONG_PTR FlushSize,
                     DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
NtQueryInformationJobObject(DR_PARAM_IN HANDLE JobHandle,
                            DR_PARAM_IN JOBOBJECTINFOCLASS JobInformationClass,
                            DR_PARAM_OUT PVOID JobInformation,
                            DR_PARAM_IN ULONG JobInformationLength,
                            DR_PARAM_OUT PULONG ReturnLength OPTIONAL)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * RTL
 */

NTEXPORT NTSTATUS NTAPI
RtlInitializeCriticalSection(DR_PARAM_OUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlEnterCriticalSection(DR_PARAM_INOUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlLeaveCriticalSection(DR_PARAM_INOUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlTryEnterCriticalSection(DR_PARAM_INOUT RTL_CRITICAL_SECTION *crit)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlInitUnicodeString(DR_PARAM_INOUT PUNICODE_STRING DestinationString,
                     DR_PARAM_IN PCWSTR SourceString)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlInitAnsiString(DR_PARAM_INOUT PANSI_STRING DestinationString,
                  DR_PARAM_IN PCSTR SourceString)
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
RtlConvertSidToUnicodeString(DR_PARAM_OUT PUNICODE_STRING UnicodeString,
                             DR_PARAM_IN PSID Sid, BOOLEAN AllocateDestinationString)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlCreateProcessParameters(DR_PARAM_OUT PRTL_USER_PROCESS_PARAMETERS *ProcessParameters,
                           DR_PARAM_IN PUNICODE_STRING ImageFile,
                           DR_PARAM_IN PUNICODE_STRING DllPath OPTIONAL,
                           DR_PARAM_IN PUNICODE_STRING CurrentDirectory OPTIONAL,
                           DR_PARAM_IN PUNICODE_STRING CommandLine OPTIONAL,
                           DR_PARAM_IN ULONG CreationFlags,
                           DR_PARAM_IN PUNICODE_STRING WindowTitle OPTIONAL,
                           DR_PARAM_IN PUNICODE_STRING Desktop OPTIONAL,
                           DR_PARAM_IN PUNICODE_STRING Reserved OPTIONAL,
                           DR_PARAM_IN PUNICODE_STRING Reserved2 OPTIONAL)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlDestroyProcessParameters(DR_PARAM_IN PRTL_USER_PROCESS_PARAMETERS ProcessParameters)

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
RtlGetProcessHeaps(DR_PARAM_IN ULONG count, DR_PARAM_OUT HANDLE *Heaps)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
RtlWalkHeap(DR_PARAM_IN HANDLE Heap, DR_PARAM_OUT PVOID Info)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * Ldr
 */

NTEXPORT NTSTATUS NTAPI
LdrLoadDll(DR_PARAM_IN PCWSTR PathToFile OPTIONAL, DR_PARAM_IN PULONG Flags OPTIONAL,
           DR_PARAM_IN PUNICODE_STRING ModuleFileName, DR_PARAM_OUT PHANDLE ModuleHandle)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrUnloadDll(DR_PARAM_IN HANDLE ModuleHandle)

{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrGetProcedureAddress(DR_PARAM_IN HANDLE ModuleHandle,
                       DR_PARAM_IN PANSI_STRING ProcedureName OPTIONAL,
                       DR_PARAM_IN ULONG Ordinal OPTIONAL,
                       DR_PARAM_OUT FARPROC *ProcedureAddress)
{
    return STATUS_SUCCESS;
}

NTEXPORT NTSTATUS NTAPI
LdrGetDllHandle(DR_PARAM_IN PCWSTR PathToFile OPTIONAL, DR_PARAM_IN ULONG Unused OPTIONAL,
                DR_PARAM_IN PUNICODE_STRING ModuleFileName,
                DR_PARAM_OUT PHANDLE ModuleHandle)
{
    return STATUS_SUCCESS;
}

/***************************************************************************
 * Csr
 */

NTEXPORT NTSTATUS NTAPI
CsrClientCallServer(DR_PARAM_INOUT PVOID Message, DR_PARAM_IN PVOID unknown,
                    DR_PARAM_IN ULONG Opcode, DR_PARAM_IN ULONG Size)
{
    return STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif
