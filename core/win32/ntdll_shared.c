/* **********************************************************
 * Copyright (c) 2010-2019 Google, Inc.  All rights reserved.
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
#    define snprintf _snprintf
#    include <stdio.h> /* _snprintf */
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

#ifndef X64
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
    if (ntcall == NULL) {
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
        /* The first call may not be during init so we have to unprot */
        if (dynamo_initialized)
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
#    endif
        ntcall = (NtWow64ReadVirtualMemory64_t)
#    ifdef NOT_DYNAMORIO_CORE
            GetProcAddress(GetModuleHandle("ntdll.dll"), "NtWow64ReadVirtualMemory64");
#    else
            d_r_get_proc_address(get_ntdll_base(), "NtWow64ReadVirtualMemory64");
#    endif
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
        if (dynamo_initialized)
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
#    endif
    }
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
#endif
