/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
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
 * ntdll_shared.c
 * Routines for calling Windows system calls via the ntdll.dll wrappers,
 * meant for sharing beyond the core DR library.
 * Xref i#1409 on splitting files up and eliminating the NOT_DYNAMORIO_CORE
 * ifdefs that today select portions of files.
 */
#include "configure.h"
#ifdef NOT_DYNAMORIO_CORE
#    include "globals_shared.h"
#    define ASSERT(x)
#    define ASSERT_CURIOSITY(x)
#    define ASSERT_NOT_REACHED()
#    define ASSERT_NOT_IMPLEMENTED(x)
#    define DODEBUG(x)
#    define DOCHECK(n, x)
#    define DEBUG_DECLARE(x)
#    pragma warning(disable : 4210) // nonstd extension: function given file scope
#    pragma warning(disable : 4204) // nonstd extension: non-constant aggregate
                                    // initializer
#    define INVALID_FILE INVALID_HANDLE_VALUE
#    define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002L)
#else
/* we include globals.h mainly for ASSERT, even though we're
 * used by preinject.
 * preinject just defines its own d_r_internal_error!
 */
#    include "../globals.h"
#    include "../module_shared.h"
#endif

/* We have to hack away things we use here that won't work for non-core */
#if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
#    undef ASSERT_OWN_NO_LOCKS
#    define ASSERT_OWN_NO_LOCKS() /* who cares if not the core */
#endif

#include "ntdll_shared.h"

/* In ntdll.c which is linked everywhere ntdll_shared.c is these days. */
bool
nt_read_virtual_memory(HANDLE process, const void *base, void *buffer,
                       size_t buffer_length, size_t *bytes_read);

bool
nt_write_virtual_memory(HANDLE process, void *base, const void *buffer,
                        size_t buffer_length, size_t *bytes_written);

#ifndef X64 /* Around most of the rest of the file. */

#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
#        define UNPROT_IF_INIT()                                                  \
            do {                                                                  \
                /* The first call may not be during init so we have to unprot. */ \
                if (dynamo_initialized) {                                         \
                    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);                  \
                }                                                                 \
            } while (0)
#        define PROT_IF_INIT()                                                    \
            do {                                                                  \
                /* The first call may not be during init so we have to unprot. */ \
                if (dynamo_initialized) {                                         \
                    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);                    \
                }                                                                 \
            } while (0)
#    else
#        define PROT_IF_INIT()   /* Nothing. */
#        define UNPROT_IF_INIT() /* Nothing. */
#    endif

#    ifdef NOT_DYNAMORIO_CORE
#        define GET_PROC_ADDR(name) GetProcAddress(GetModuleHandle("ntdll.dll"), name)
#    else
#        define GET_PROC_ADDR(name) d_r_get_proc_address(get_ntdll_base(), name)
#    endif

#    define INIT_NTWOW64_FUNCPTR(var, name)           \
        do {                                          \
            if (ntcall == NULL) {                     \
                UNPROT_IF_INIT();                     \
                var = (name##_t)GET_PROC_ADDR(#name); \
                PROT_IF_INIT();                       \
            }                                         \
        } while (0)

NTSTATUS
nt_wow64_read_virtual_memory64(HANDLE process, uint64 base, void *buffer,
                               size_t buffer_length, size_t *bytes_read)
{
    /* This syscall was added in 2003 so we can't statically link. */
    typedef NTSTATUS(NTAPI * NtWow64ReadVirtualMemory64_t)(
        HANDLE ProcessHandle, IN PVOID64 BaseAddress, OUT PVOID Buffer,
        IN ULONGLONG BufferSize, OUT PULONGLONG NumberOfBytesRead);
    static NtWow64ReadVirtualMemory64_t ntcall;
    NTSTATUS res;
    INIT_NTWOW64_FUNCPTR(ntcall, NtWow64ReadVirtualMemory64);
    if (ntcall == NULL) {
        /* We do not need to fall back to NtReadVirtualMemory, b/c
         * NtWow64ReadVirtualMemory64 was added in xp64==2003 and so should
         * always exist if we are in a WOW64 process: and we should only be
         * called from a WOW64 process.
         */
        ASSERT_NOT_REACHED();
        res = STATUS_NOT_IMPLEMENTED;
    } else {
        uint64 len;
        res = ntcall(process, (PVOID64)base, buffer, (ULONGLONG)buffer_length, &len);
        if (bytes_read != NULL)
            *bytes_read = (size_t)len;
    }
    return res;
}

NTSTATUS
nt_wow64_write_virtual_memory64(HANDLE process, uint64 base, void *buffer,
                                size_t buffer_length, size_t *bytes_written)
{
    /* Just like nt_wow64_read_virtual_memory64, we dynamically acquire. */
    typedef NTSTATUS(NTAPI * NtWow64WriteVirtualMemory64_t)(
        HANDLE ProcessHandle, IN PVOID64 BaseAddress, IN PVOID Buffer,
        IN ULONGLONG BufferSize, OUT PULONGLONG NumberOfBytesWritten);
    static NtWow64WriteVirtualMemory64_t ntcall;
    NTSTATUS res;
    INIT_NTWOW64_FUNCPTR(ntcall, NtWow64WriteVirtualMemory64);
    if (ntcall == NULL) {
        ASSERT_NOT_REACHED();
        res = STATUS_NOT_IMPLEMENTED;
    } else {
        uint64 len;
        res = ntcall(process, (PVOID64)base, buffer, (ULONGLONG)buffer_length, &len);
        if (bytes_written != NULL)
            *bytes_written = (size_t)len;
    }
    return res;
}

NTSTATUS
nt_wow64_query_info_process64(HANDLE process, PROCESS_BASIC_INFORMATION64 *info)
{
    /* Just like nt_wow64_read_virtual_memory64, we dynamically acquire. */
    typedef NTSTATUS(NTAPI * NtWow64QueryInformationProcess64_t)(
        HANDLE ProcessHandle, IN PROCESSINFOCLASS InfoClass, OUT PVOID Buffer,
        IN ULONG BufferSize, OUT PULONG NumberOfBytesRead);
    static NtWow64QueryInformationProcess64_t ntcall;
    NTSTATUS res;
    INIT_NTWOW64_FUNCPTR(ntcall, NtWow64QueryInformationProcess64);
    if (ntcall == NULL) {
        ASSERT_NOT_REACHED();
        res = STATUS_NOT_IMPLEMENTED;
    } else {
        ULONG got;
        res = ntcall(process, ProcessBasicInformation, info, sizeof(*info), &got);
        ASSERT(!NT_SUCCESS(res) || got == sizeof(PROCESS_BASIC_INFORMATION64));
    }
    return res;
}

#endif /* !X64 */

#ifndef NOT_DYNAMORIO_CORE
bool
read_remote_memory_maybe64(HANDLE process, uint64 base, void *buffer,
                           size_t buffer_length, size_t *bytes_read)
{
#    ifdef X64
    return nt_read_virtual_memory(process, (LPVOID)base, buffer, buffer_length,
                                  bytes_read);
#    else
    return NT_SUCCESS(
        nt_wow64_read_virtual_memory64(process, base, buffer, buffer_length, bytes_read));
#    endif
}

bool
write_remote_memory_maybe64(HANDLE process, uint64 base, void *buffer,
                            size_t buffer_length, size_t *bytes_read)
{
#    ifdef X64
    return nt_write_virtual_memory(process, (LPVOID)base, buffer, buffer_length,
                                   bytes_read);
#    else
    return NT_SUCCESS(nt_wow64_write_virtual_memory64(process, base, buffer,
                                                      buffer_length, bytes_read));
#    endif
}
#endif
