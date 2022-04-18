/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.   All rights reserved.
 * Copyright (c) 2009-2010 Derek Bruening   All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* ntdll.dll redirection for custom private library loader */

#ifndef _NTDLL_REDIR_H_
#define _NTDLL_REDIR_H_ 1

#include "../../globals.h"

void
ntdll_redir_init(void);

void
ntdll_redir_exit(void);

app_pc
ntdll_redir_lookup(const char *name);

HANDLE WINAPI
redirect_RtlCreateHeap(ULONG flags, void *base, size_t reserve_sz, size_t commit_sz,
                       void *lock, void *params);

BOOL WINAPI
redirect_RtlDestroyHeap(HANDLE base);

void *WINAPI
redirect_RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size);

void *WINAPI
redirect_RtlReAllocateHeap(HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size);

BOOL WINAPI
redirect_RtlFreeHeap(HANDLE heap, ULONG flags, PVOID ptr);

SIZE_T WINAPI
redirect_RtlSizeHeap(HANDLE heap, ULONG flags, PVOID ptr);

BOOL WINAPI
redirect_RtlValidateHeap(HANDLE heap, DWORD flags, void *ptr);

BOOL WINAPI
redirect_RtlLockHeap(HANDLE heap);

BOOL WINAPI
redirect_RtlUnlockHeap(HANDLE heap);

BOOL WINAPI
redirect_RtlSetHeapInformation(HANDLE HeapHandle,
                               HEAP_INFORMATION_CLASS HeapInformationClass,
                               PVOID HeapInformation, SIZE_T HeapInformationLength);

void WINAPI
redirect_RtlFreeUnicodeString(UNICODE_STRING *string);

void WINAPI
redirect_RtlFreeAnsiString(ANSI_STRING *string);

void WINAPI
redirect_RtlFreeOemString(OEM_STRING *string);

/* A LIST_ENTRY is stored at the start of TEB.FlsData */
#define TEB_FLS_DATA_OFFS (sizeof(LIST_ENTRY) / sizeof(void *))

NTSTATUS NTAPI
redirect_RtlFlsAlloc(IN PFLS_CALLBACK_FUNCTION cb, OUT PDWORD index_out);

NTSTATUS NTAPI
redirect_RtlFlsFree(IN DWORD index);

NTSTATUS NTAPI
redirect_RtlProcessFlsData(IN PLIST_ENTRY fls_data);

NTSTATUS WINAPI
redirect_NtCreateFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PIO_STATUS_BLOCK io_status_block, PLARGE_INTEGER allocation_size,
                      ULONG file_attributes, ULONG share_access, ULONG create_disposition,
                      ULONG create_options, PVOID ea_buffer, ULONG ea_length);

NTSTATUS WINAPI
redirect_NtCreateKey(PHANDLE key_handle, ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes, ULONG title_index,
                     PUNICODE_STRING class, ULONG create_options, PULONG disposition);

NTSTATUS WINAPI
redirect_NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle,
                            PVOID *base_address, ULONG_PTR zero_bits, SIZE_T commit_size,
                            PLARGE_INTEGER section_offset, PSIZE_T view_size,
                            SECTION_INHERIT inherit_disposition, ULONG allocation_type,
                            ULONG win32_protect);

NTSTATUS WINAPI
redirect_NtOpenFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    PIO_STATUS_BLOCK io_status_block, ULONG share_access,
                    ULONG open_options);

NTSTATUS WINAPI
redirect_NtOpenKey(PHANDLE key_handle, ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes);

NTSTATUS WINAPI
redirect_NtOpenKeyEx(PHANDLE key_handle, ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes, ULONG open_options);

NTSTATUS WINAPI
redirect_NtOpenProcess(PHANDLE process_handle, ACCESS_MASK desired_access,
                       POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

NTSTATUS WINAPI
redirect_NtOpenProcessToken(HANDLE process_handle, ACCESS_MASK desired_access,
                            PHANDLE token_handle);

NTSTATUS WINAPI
redirect_NtOpenProcessTokenEx(HANDLE process_handle, ACCESS_MASK desired_access,
                              ULONG handle_attributes, PHANDLE token_handle);

NTSTATUS WINAPI
redirect_NtOpenThread(PHANDLE thread_handle, ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

NTSTATUS WINAPI
redirect_NtOpenThreadToken(HANDLE thread_handle, ACCESS_MASK desired_access,
                           BOOLEAN open_as_self, PHANDLE token_handle);

NTSTATUS WINAPI
redirect_NtOpenThreadTokenEx(HANDLE thread_handle, ACCESS_MASK desired_access,
                             BOOLEAN open_as_self, ULONG handle_attributes,
                             PHANDLE token_handle);

NTSTATUS WINAPI
redirect_NtQueryAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                               PFILE_BASIC_INFORMATION file_information);

NTSTATUS WINAPI
redirect_NtQueryFullAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                                   PFILE_NETWORK_OPEN_INFORMATION file_information);

NTSTATUS WINAPI
redirect_NtSetInformationFile(HANDLE file_handle, PIO_STATUS_BLOCK io_status_block,
                              PVOID file_information, ULONG length,
                              FILE_INFORMATION_CLASS file_information_class);

NTSTATUS WINAPI
redirect_NtSetInformationThread(HANDLE thread_handle,
                                THREADINFOCLASS thread_information_class,
                                PVOID thread_information,
                                ULONG thread_information_length);

NTSTATUS WINAPI
redirect_NtUnmapViewOfSection(HANDLE process_handle, PVOID base_address);

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSection(RTL_CRITICAL_SECTION *crit);

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSectionAndSpinCount(RTL_CRITICAL_SECTION *crit,
                                                  ULONG spincount);

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSectionEx(RTL_CRITICAL_SECTION *crit, ULONG spincount,
                                        ULONG flags);

NTSTATUS WINAPI
redirect_RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *crit);

NTSTATUS NTAPI
redirect_LdrGetProcedureAddress(IN HMODULE module_handle, IN PANSI_STRING func OPTIONAL,
                                IN WORD ordinal OPTIONAL, OUT PVOID *addr);

NTSTATUS NTAPI
redirect_LdrLoadDll(IN PWSTR path OPTIONAL, IN PULONG characteristics OPTIONAL,
                    IN PUNICODE_STRING name, OUT PVOID *handle);

/* This is exported by some kernel32.dll versions, but it's just forwarded
 * directly or there's a stub that calls the real impl in ntdll.
 */
PVOID NTAPI
redirect_RtlPcToFileHeader(__in PVOID PcValue, __out PVOID *BaseOfImage);

#endif /* _NTDLL_REDIR_H_ */
