/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * ntdll_shared.h
 * Routines for calling Windows system calls via the ntdll.dll wrappers,
 * meant for use beyond just the core DR library.
 */

#ifndef _NTDLL_SHARED_H_
#define _NTDLL_SHARED_H_ 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ntdll_types.h"

bool
read_remote_memory_maybe64(HANDLE process, uint64 base, void *buffer,
                           size_t buffer_length, size_t *bytes_read);

bool
write_remote_memory_maybe64(HANDLE process, uint64 base, void *buffer,
                            size_t buffer_length, size_t *bytes_written);

#ifndef X64
typedef struct _PROCESS_BASIC_INFORMATION64 {
    NTSTATUS ExitStatus;
    uint64 PebBaseAddress;
    uint64 AffinityMask;
    KPRIORITY BasePriority;
    uint64 UniqueProcessId;
    uint64 InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION64;

/* Returns raw NTSTATUS. */
NTSTATUS
nt_wow64_read_virtual_memory64(HANDLE process, uint64 base, void *buffer,
                               size_t buffer_length, size_t *bytes_read);

/* Returns raw NTSTATUS. */
NTSTATUS
nt_wow64_write_virtual_memory64(HANDLE process, uint64 base, void *buffer,
                                size_t buffer_length, size_t *bytes_read);

/* Returns raw NTSTATUS. */
NTSTATUS
nt_wow64_query_info_process64(HANDLE process, PROCESS_BASIC_INFORMATION64 *info);

#endif

#endif /* _NTDLL_SHARED_H_ */
