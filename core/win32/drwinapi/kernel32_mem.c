/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll memory-related redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "ntdll_redir.h"
#include "../ntdll.h"
#include "drwinapi_private.h"

/* Value used to encrypt pointers.  Xor with a per-process magic value
 * seems plenty secure enough for private libs (this isn't affecting
 * the app).
 */
static ptr_uint_t magic_val;

void
kernel32_redir_init_mem(void)
{
    magic_val = (ptr_uint_t) get_random_offset(POINTER_MAX);
}

void
kernel32_redir_exit_mem(void)
{
    /* nothing */
}

PVOID
WINAPI
redirect_DecodePointer(
    __in_opt PVOID Ptr
    )
{
    return (PVOID) ((ptr_uint_t)Ptr ^ magic_val);
}

PVOID
WINAPI
redirect_EncodePointer (
    __in_opt PVOID Ptr
    )
{
    return (PVOID) ((ptr_uint_t)Ptr ^ magic_val);
}

HANDLE
WINAPI
redirect_GetProcessHeap(VOID)
{
    return get_private_peb()->ProcessHeap;
}

__bcount(dwBytes)
LPVOID
WINAPI
redirect_HeapAlloc(
    __in HANDLE hHeap,
    __in DWORD dwFlags,
    __in SIZE_T dwBytes
    )
{
    return redirect_RtlAllocateHeap(hHeap, dwFlags, dwBytes);
}

SIZE_T
WINAPI
redirect_HeapCompact(
    __in HANDLE hHeap,
    __in DWORD dwFlags
    )
{
    /* We do not support compacting/coalescing here so we just return
     * a reasonably large size for the "largest committed free block".
     * We don't bother checking hHeap and forwarding: won't affect
     * correctness as the app can't rely on this value.
     */
    return 8*1024;
}

HANDLE
WINAPI
redirect_HeapCreate(
    __in DWORD flOptions,
    __in SIZE_T dwInitialSize,
    __in SIZE_T dwMaximumSize
    )
{
    return redirect_RtlCreateHeap(flOptions | HEAP_CLASS_PRIVATE |
                                  (dwMaximumSize == 0 ? HEAP_GROWABLE : 0),
                                  NULL, dwMaximumSize, dwInitialSize, NULL, NULL);
}

BOOL
WINAPI
redirect_HeapDestroy(
    __in HANDLE hHeap
    )
{
    return redirect_RtlDestroyHeap(hHeap);
}

BOOL
WINAPI
redirect_HeapFree(
    __inout HANDLE hHeap,
    __in    DWORD dwFlags,
    __deref LPVOID lpMem
    )
{
    return redirect_RtlFreeHeap(hHeap, dwFlags, lpMem);
}

__bcount(dwBytes)
LPVOID
WINAPI
redirect_HeapReAlloc(
    __inout HANDLE hHeap,
    __in    DWORD dwFlags,
    __deref LPVOID lpMem,
    __in    SIZE_T dwBytes
    )
{
    return redirect_RtlReAllocateHeap(hHeap, dwFlags, lpMem, dwBytes);
}

BOOL
WINAPI
redirect_HeapSetInformation (
    __in_opt HANDLE HeapHandle,
    __in HEAP_INFORMATION_CLASS HeapInformationClass,
    __in_bcount_opt(HeapInformationLength) PVOID HeapInformation,
    __in SIZE_T HeapInformationLength
    )
{
    if (HeapInformationClass == HeapCompatibilityInformation) {
        if (HeapInformationLength != sizeof(ULONG) || HeapInformation == NULL)
            return FALSE;
        /* We just ignore LFH requests */
        return TRUE;
    } else if (HeapInformationClass == HeapEnableTerminationOnCorruption) {
        if (HeapInformationLength != 0 || HeapInformation != NULL)
            return FALSE;
        /* We just ignore */
        return TRUE;
    }
    return FALSE;
}

SIZE_T
WINAPI
redirect_HeapSize(
    __in HANDLE hHeap,
    __in DWORD dwFlags,
    __in LPCVOID lpMem
    )
{
    return redirect_RtlSizeHeap(hHeap, dwFlags, (PVOID) lpMem);
}

BOOL
WINAPI
redirect_HeapValidate(
    __in     HANDLE hHeap,
    __in     DWORD dwFlags,
    __in_opt LPCVOID lpMem
    )
{
    return redirect_RtlValidateHeap(hHeap, dwFlags, (PVOID) lpMem);
}

BOOL
WINAPI
redirect_HeapWalk(
    __in    HANDLE hHeap,
    __inout LPPROCESS_HEAP_ENTRY lpEntry
    )
{
    /* XXX: what msvcrt routine really depends on this?  Should be
     * used primarily for debugging, right?
     */
    set_last_error(ERROR_NO_MORE_ITEMS);
    return FALSE;
}

/***************************************************************************
 * System calls
 */

BOOL
WINAPI
redirect_IsBadReadPtr(
    __in_opt CONST VOID *lp,
    __in     UINT_PTR ucb
    )
{
    if (ucb == 0)
        return FALSE;
    return !is_readable_without_exception((const byte *)lp, ucb);
}

BOOL
WINAPI
redirect_ReadProcessMemory(
    __in      HANDLE hProcess,
    __in      LPCVOID lpBaseAddress,
    __out_bcount_part(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
    __in      SIZE_T nSize,
    __out_opt SIZE_T * lpNumberOfBytesRead
    )
{
    size_t bytes_read;
    NTSTATUS res = nt_raw_read_virtual_memory(hProcess, (void *) lpBaseAddress, lpBuffer,
                                              nSize, &bytes_read);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    if (lpNumberOfBytesRead != NULL)
        *lpNumberOfBytesRead = bytes_read;
    return TRUE;
}

__bcount(dwSize)
LPVOID
WINAPI
redirect_VirtualAlloc(
    __in_opt LPVOID lpAddress,
    __in     SIZE_T dwSize,
    __in     DWORD flAllocationType,
    __in     DWORD flProtect
    )
{
    /* XXX: are MEM_* values beyond MEM_RESERVE and MEM_COMMIT passed to the kernel? */
    PVOID base = lpAddress;
    NTSTATUS res =
        nt_allocate_virtual_memory(&base, dwSize, flProtect, flAllocationType);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return NULL;
    }
    return base;
}

BOOL
WINAPI
redirect_VirtualFree(
    __in LPVOID lpAddress,
    __in SIZE_T dwSize,
    __in DWORD dwFreeType
    )
{
    NTSTATUS res;
    if (TEST(MEM_DECOMMIT, dwFreeType))
        res = nt_decommit_virtual_memory(lpAddress, dwSize);
    else {
        if (dwSize != 0) {
            set_last_error(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        res = nt_free_virtual_memory(lpAddress);
    }
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    return TRUE;
}

BOOL
WINAPI
redirect_VirtualProtect(
    __in  LPVOID lpAddress,
    __in  SIZE_T dwSize,
    __in  DWORD flNewProtect,
    __out PDWORD lpflOldProtect
    )
{
    return (BOOL)
        protect_virtual_memory(lpAddress, dwSize, flNewProtect, (uint *)lpflOldProtect);
}

SIZE_T
WINAPI
redirect_VirtualQuery(
    __in_opt LPCVOID lpAddress,
    __out_bcount_part(dwLength, return) PMEMORY_BASIC_INFORMATION lpBuffer,
    __in     SIZE_T dwLength
    )
{
    return redirect_VirtualQueryEx(NT_CURRENT_PROCESS, lpAddress, lpBuffer, dwLength);
}

SIZE_T
WINAPI
redirect_VirtualQueryEx(
    __in     HANDLE hProcess,
    __in_opt LPCVOID lpAddress,
    __out_bcount_part(dwLength, return) PMEMORY_BASIC_INFORMATION lpBuffer,
    __in     SIZE_T dwLength
    )
{
    size_t got;
    app_pc page = (app_pc) PAGE_START(lpAddress);
    NTSTATUS res = nt_remote_query_virtual_memory
        (hProcess, page, lpBuffer, dwLength, &got);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return 0;
    }
    return got;
}


#ifdef STANDALONE_UNIT_TEST
static void
test_heap(void)
{
    HANDLE heap;
    PVOID temp;
    BOOL ok;
    /* For now we're just ensuring we exercise these.
     * XXX: add more corner cases.
     */
    heap = redirect_HeapCreate(0, 0, 0);
    EXPECT(heap != NULL, true);
    temp = redirect_HeapAlloc(heap, 0, 32);
    EXPECT(temp != NULL, true);
    EXPECT(redirect_HeapSize(heap, 0, temp) >= 32, true);
    EXPECT(redirect_HeapValidate(heap, 0, temp), true);
    EXPECT(redirect_HeapCompact(heap, 0) > 0, true);
    temp = redirect_HeapReAlloc(heap, 0, temp, 64);
    EXPECT(temp != NULL, true);
    EXPECT(redirect_IsBadReadPtr(temp, 64), false);
    ok = redirect_HeapFree(heap, 0, temp);
    EXPECT(ok, true);
    ok = redirect_HeapDestroy(heap);
    EXPECT(ok, true);
}

static void
test_syscalls(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD dw;
    SIZE_T sz;
    BOOL ok;
    PVOID temp =
        redirect_VirtualAlloc(0, PAGE_SIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    EXPECT(temp != NULL, true);
    sz = redirect_VirtualQuery((char *)temp + PAGE_SIZE/2, &mbi, sizeof(mbi));
    EXPECT(sz == sizeof(mbi), true);
    EXPECT(mbi.BaseAddress == temp, true);
    EXPECT(mbi.AllocationBase == temp, true);
    EXPECT(mbi.AllocationProtect == PAGE_READWRITE, true);

    EXPECT(redirect_VirtualProtect(temp, PAGE_SIZE/2, PAGE_READONLY, &dw), true);
    sz = redirect_VirtualQuery((char *)temp + PAGE_SIZE/4, &mbi, sizeof(mbi));
    EXPECT(sz == sizeof(mbi), true);
    EXPECT(mbi.BaseAddress == temp, true);
    EXPECT(mbi.AllocationBase == temp, true);
    EXPECT(mbi.AllocationProtect == PAGE_READWRITE, true);

    ok = redirect_VirtualFree(temp, 0, MEM_RELEASE);
    EXPECT(ok, true);
}

void
unit_test_drwinapi_kernel32_mem(void)
{
    PVOID temp, ran;

    print_file(STDERR, "testing drwinapi kernel32 memory-related routines\n");

    ran = (PVOID) get_random_offset(POINTER_MAX);
    temp = redirect_EncodePointer(ran);
    EXPECT(temp != ran, true);
    EXPECT(redirect_DecodePointer(temp) == ran, true);

    test_heap();

    test_syscalls();
}
#endif
