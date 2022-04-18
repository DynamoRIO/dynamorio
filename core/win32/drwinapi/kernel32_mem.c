/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.   All rights reserved.
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
#include "../os_private.h"

/* Value used to encrypt pointers.  Xor with a per-process magic value
 * seems plenty secure enough for private libs (this isn't affecting
 * the app).
 */
static ptr_uint_t magic_val;

/* Lock for the Local* routines.  We can't use redirect_RtlLockHeap
 * to mirror the real kernel32 b/c that's a nop.
 */
DECLARE_CXTSWPROT_VAR(static mutex_t localheap_lock,
                      INIT_LOCK_FREE(drwinapi_localheap_lock));

void
kernel32_redir_init_mem(void)
{
    magic_val = (ptr_uint_t)get_random_offset(POINTER_MAX);
}

void
kernel32_redir_exit_mem(void)
{
    DELETE_LOCK(localheap_lock);
}

PVOID
WINAPI
redirect_DecodePointer(__in_opt PVOID Ptr)
{
    return (PVOID)((ptr_uint_t)Ptr ^ magic_val);
}

PVOID
WINAPI
redirect_EncodePointer(__in_opt PVOID Ptr)
{
    return (PVOID)((ptr_uint_t)Ptr ^ magic_val);
}

HANDLE
WINAPI
redirect_GetProcessHeap(VOID)
{
    return get_private_peb()->ProcessHeap;
}

__bcount(dwBytes) LPVOID WINAPI
    redirect_HeapAlloc(__in HANDLE hHeap, __in DWORD dwFlags, __in SIZE_T dwBytes)
{
    return redirect_RtlAllocateHeap(hHeap, dwFlags, dwBytes);
}

SIZE_T
WINAPI
redirect_HeapCompact(__in HANDLE hHeap, __in DWORD dwFlags)
{
    /* We do not support compacting/coalescing here so we just return
     * a reasonably large size for the "largest committed free block".
     * We don't bother checking hHeap and forwarding: won't affect
     * correctness as the app can't rely on this value.
     */
    return 8 * 1024;
}

HANDLE
WINAPI
redirect_HeapCreate(__in DWORD flOptions, __in SIZE_T dwInitialSize,
                    __in SIZE_T dwMaximumSize)
{
    return redirect_RtlCreateHeap(flOptions | HEAP_CLASS_PRIVATE |
                                      (dwMaximumSize == 0 ? HEAP_GROWABLE : 0),
                                  NULL, dwMaximumSize, dwInitialSize, NULL, NULL);
}

BOOL WINAPI
redirect_HeapDestroy(__in HANDLE hHeap)
{
    return redirect_RtlDestroyHeap(hHeap);
}

BOOL WINAPI
redirect_HeapFree(__inout HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem)
{
    return redirect_RtlFreeHeap(hHeap, dwFlags, lpMem);
}

__bcount(dwBytes) LPVOID WINAPI
    redirect_HeapReAlloc(__inout HANDLE hHeap, __in DWORD dwFlags, __deref LPVOID lpMem,
                         __in SIZE_T dwBytes)
{
    return redirect_RtlReAllocateHeap(hHeap, dwFlags, lpMem, dwBytes);
}

BOOL WINAPI
redirect_HeapSetInformation(__in_opt HANDLE HeapHandle,
                            __in HEAP_INFORMATION_CLASS HeapInformationClass,
                            __in_bcount_opt(HeapInformationLength) PVOID HeapInformation,
                            __in SIZE_T HeapInformationLength)
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
redirect_HeapSize(__in HANDLE hHeap, __in DWORD dwFlags, __in LPCVOID lpMem)
{
    return redirect_RtlSizeHeap(hHeap, dwFlags, (PVOID)lpMem);
}

BOOL WINAPI
redirect_HeapValidate(__in HANDLE hHeap, __in DWORD dwFlags, __in_opt LPCVOID lpMem)
{
    return redirect_RtlValidateHeap(hHeap, dwFlags, (PVOID)lpMem);
}

BOOL WINAPI
redirect_HeapWalk(__in HANDLE hHeap, __inout LPPROCESS_HEAP_ENTRY lpEntry)
{
    /* XXX: what msvcrt routine really depends on this?  Should be
     * used primarily for debugging, right?
     */
    set_last_error(ERROR_NO_MORE_ITEMS);
    return FALSE;
}

/***************************************************************************
 * Local heap
 *
 * Although our target of {msvcp*,msvcr*,dbghelp} only uses Local{Alloc,Free}
 * we must implement the full set in case a privlib calls another routine.
 *
 * We use a custom header and we synchronize with localheap_lock.  We
 * return a pointer beyond the header to support LMEM_FIXED where
 * handle==pointer and local_header_t.alloc==NULL.  For LMEM_MOVEABLE,
 * we start out with an inlined alloc, but if it's resized we store
 * the separate alloc in local_header_t.alloc.  We use a header on the
 * separate alloc (with flags==LMEM_INVALID_HANDLE to distinguish, and
 * with alloc==original header) so we can map back to the handle for
 * LocalHandle.  On LocalDiscard() we keep a 0-sized alloc pointed at
 * by local_header_t.alloc.
 */

typedef struct _local_header_t {
    ushort lock_count;
    ushort flags;
    struct _local_header_t *alloc;
} local_header_t;

static inline local_header_t *
local_header_from_handle(HLOCAL handle)
{
    return ((local_header_t *)handle) - 1;
}

HLOCAL
WINAPI
redirect_LocalAlloc(__in UINT uFlags, __in SIZE_T uBytes)
{
    HLOCAL res = NULL;
    local_header_t *hdr;
    HANDLE heap = redirect_GetProcessHeap();
    /* for back-compat, LocalAlloc asks for +x */
    uint rtl_flags = HEAP_NO_SERIALIZE | HEAP_CREATE_ENABLE_EXECUTE;
    if (TEST(LMEM_ZEROINIT, uFlags))
        rtl_flags |= HEAP_ZERO_MEMORY;

    /* flags should be in ushort range */
    if ((uFlags & 0xffff0000) != 0) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* No lock is needed as the lock is to synchronize w/ other Local* routines
     * accessing the same object, and this object has not been returned yet.
     */
    hdr = (local_header_t *)redirect_RtlAllocateHeap(heap, rtl_flags,
                                                     uBytes + sizeof(local_header_t));
    if (hdr == NULL) {
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    res = (HLOCAL)(hdr + 1); /* even for LMEM_MOVEABLE we return the usable mem */
    hdr->lock_count = 0;
    hdr->flags = (ushort)uFlags; /* we checked for truncation above */
    hdr->alloc = NULL;
    return res;
}

HLOCAL
WINAPI
redirect_LocalFree(__deref HLOCAL hMem)
{
    HANDLE heap = redirect_GetProcessHeap();
    local_header_t *hdr;
    if (hMem == NULL)
        return NULL;
    hdr = local_header_from_handle(hMem);
    d_r_mutex_lock(&localheap_lock);
    /* XXX: supposed to raise debug msg + bp if freeing locked object */
    if (hdr->alloc != NULL) {
        ASSERT(TEST(LMEM_MOVEABLE, hdr->flags));
        redirect_RtlFreeHeap(heap, HEAP_NO_SERIALIZE, hdr->alloc);
    }
    redirect_RtlFreeHeap(heap, HEAP_NO_SERIALIZE, hdr);
    d_r_mutex_unlock(&localheap_lock);
    return NULL;
}

HLOCAL
WINAPI
redirect_LocalReAlloc(__in HLOCAL hMem, __in SIZE_T uBytes, __in UINT uFlags)
{
    HLOCAL res = NULL;
    HANDLE heap = redirect_GetProcessHeap();
    local_header_t *hdr = local_header_from_handle(hMem);
    uint rtl_flags = HEAP_NO_SERIALIZE | HEAP_CREATE_ENABLE_EXECUTE;
    if (TEST(LMEM_ZEROINIT, uFlags))
        rtl_flags |= HEAP_ZERO_MEMORY;
    d_r_mutex_lock(&localheap_lock);
    if (TEST(LMEM_MODIFY, uFlags)) {
        /* no realloc, just update flags */
        if ((uFlags & 0xffff0000) != 0 || /* flags should be in ushort range */
            /* we don't allow turning moveable w/ sep alloc into fixed */
            (!TEST(LMEM_MOVEABLE, uFlags) && hdr->alloc != NULL)) {
            set_last_error(ERROR_INVALID_PARAMETER);
            res = NULL;
            goto redirect_LocalReAlloc_done;
        }
        hdr->flags = (ushort)uFlags;
        res = hMem;
    } else {
        /* if fixed or locked and LMEM_MOVEABLE is not specified, must realloc in-place */
        if (!TEST(LMEM_MOVEABLE, uFlags) &&
            (!TEST(LMEM_MOVEABLE, hdr->flags) || hdr->lock_count > 0))
            rtl_flags |= HEAP_REALLOC_IN_PLACE_ONLY;
        else if (TEST(LMEM_MOVEABLE, hdr->flags) && hdr->alloc == NULL) {
            size_t copy_sz =
                redirect_RtlSizeHeap(heap, 0, (byte *)hdr) - sizeof(local_header_t);
            copy_sz = MIN(copy_sz, uBytes);
            hdr->alloc = redirect_RtlAllocateHeap(heap, rtl_flags,
                                                  uBytes + sizeof(local_header_t));
            if (hdr->alloc == NULL) {
                set_last_error(ERROR_NOT_ENOUGH_MEMORY);
                res = NULL;
                goto redirect_LocalReAlloc_done;
            }
            hdr->alloc->flags = LMEM_INVALID_HANDLE; /* mark as sep alloc */
            hdr->alloc->alloc = hdr;                 /* backpointer */
            memcpy(hdr->alloc + 1, hMem, copy_sz);
            res = hMem;
        }
        if (res == NULL) {
            local_header_t *newmem;
            if (hdr->alloc != NULL) {
                newmem = (local_header_t *)redirect_RtlReAllocateHeap(
                    heap, rtl_flags, hdr->alloc, uBytes + sizeof(local_header_t));
                if (newmem == NULL) {
                    set_last_error(ERROR_NOT_ENOUGH_MEMORY);
                    res = NULL;
                    goto redirect_LocalReAlloc_done;
                }
                hdr->alloc = newmem;
                ASSERT(hdr->alloc->flags == LMEM_INVALID_HANDLE);
                ASSERT(hdr->alloc->alloc == hdr);
                res = hMem;
            } else {
                newmem = (local_header_t *)redirect_RtlReAllocateHeap(
                    heap, rtl_flags, hdr, uBytes + sizeof(local_header_t));
                if (newmem == NULL) {
                    set_last_error(ERROR_NOT_ENOUGH_MEMORY);
                    res = NULL;
                    goto redirect_LocalReAlloc_done;
                }
                res = (HLOCAL)(newmem + 1);
            }
        }
    }
redirect_LocalReAlloc_done:
    d_r_mutex_unlock(&localheap_lock);
    return res;
}

LPVOID
WINAPI
redirect_LocalLock(__in HLOCAL hMem)
{
    LPVOID res = NULL;
    local_header_t *hdr = local_header_from_handle(hMem);
    d_r_mutex_lock(&localheap_lock);
    if (TEST(LMEM_MOVEABLE, hdr->flags))
        hdr->lock_count++;
    if (hdr->alloc != NULL)
        res = (LPVOID)(hdr->alloc + 1);
    else
        res = (LPVOID)hMem;
    d_r_mutex_unlock(&localheap_lock);
    return res;
}

HLOCAL
WINAPI
redirect_LocalHandle(__in LPCVOID pMem)
{
    local_header_t *hdr = local_header_from_handle((PVOID)pMem);
    if (TEST(LMEM_INVALID_HANDLE, hdr->flags)) {
        /* separate alloc stores the original header */
        hdr = hdr->alloc;
    }
    return (HLOCAL)(hdr + 1);
}

BOOL WINAPI
redirect_LocalUnlock(__in HLOCAL hMem)
{
    BOOL res = FALSE;
    local_header_t *hdr = local_header_from_handle(hMem);
    d_r_mutex_lock(&localheap_lock);
    if (hdr->lock_count == 0) {
        res = FALSE;
        set_last_error(ERROR_NOT_LOCKED);
    } else {
        hdr->lock_count--;
        if (hdr->lock_count == 0) {
            res = FALSE;
            set_last_error(NO_ERROR);
        } else
            res = TRUE;
    }
    d_r_mutex_unlock(&localheap_lock);
    return res;
}

SIZE_T
WINAPI
redirect_LocalSize(__in HLOCAL hMem)
{
    SIZE_T res = 0;
    local_header_t *hdr = local_header_from_handle(hMem);
    HANDLE heap = redirect_GetProcessHeap();
    d_r_mutex_lock(&localheap_lock);
    if (hdr->alloc != NULL) {
        ASSERT(TEST(LMEM_MOVEABLE, hdr->flags));
        res = redirect_RtlSizeHeap(heap, 0, (byte *)hdr->alloc);
    } else
        res = redirect_RtlSizeHeap(heap, 0, (byte *)hdr);
    if (res != 0) {
        ASSERT(res >= sizeof(local_header_t));
        res -= sizeof(local_header_t);
    }
    d_r_mutex_unlock(&localheap_lock);
    return res;
}

UINT WINAPI
redirect_LocalFlags(__in HLOCAL hMem)
{
    UINT res = 0;
    local_header_t *hdr = local_header_from_handle(hMem);
    HANDLE heap = redirect_GetProcessHeap();
    d_r_mutex_lock(&localheap_lock);
    res |= (hdr->lock_count & LMEM_LOCKCOUNT);
    if ((hdr->alloc != NULL &&
         redirect_RtlSizeHeap(heap, 0, (byte *)hdr->alloc) == sizeof(local_header_t)) ||
        (hdr->alloc == NULL &&
         redirect_RtlSizeHeap(heap, 0, (byte *)hdr) == sizeof(local_header_t)))
        res |= LMEM_DISCARDABLE;
    d_r_mutex_unlock(&localheap_lock);
    return res;
}

/***************************************************************************
 * System calls
 */

BOOL WINAPI
redirect_IsBadReadPtr(__in_opt CONST VOID *lp, __in UINT_PTR ucb)
{
    if (ucb == 0)
        return FALSE;
    return !is_readable_without_exception((const byte *)lp, ucb);
}

BOOL WINAPI
redirect_ReadProcessMemory(__in HANDLE hProcess, __in LPCVOID lpBaseAddress,
                           __out_bcount_part(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
                           __in SIZE_T nSize, __out_opt SIZE_T *lpNumberOfBytesRead)
{
    size_t bytes_read;
    NTSTATUS res = nt_raw_read_virtual_memory(hProcess, (void *)lpBaseAddress, lpBuffer,
                                              nSize, &bytes_read);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return FALSE;
    }
    if (lpNumberOfBytesRead != NULL)
        *lpNumberOfBytesRead = bytes_read;
    return TRUE;
}

__bcount(dwSize) LPVOID WINAPI
    redirect_VirtualAlloc(__in_opt LPVOID lpAddress, __in SIZE_T dwSize,
                          __in DWORD flAllocationType, __in DWORD flProtect)
{
    /* XXX: are MEM_* values beyond MEM_RESERVE and MEM_COMMIT passed to the kernel? */
    PVOID base = lpAddress;
    NTSTATUS res;
    if (TEST(MEM_COMMIT, flAllocationType) &&
        /* Any overlap when asking for MEM_RESERVE (even when combined w/ MEM_COMMIT)
         * will fail anyway, so we only have to worry about overlap on plain MEM_COMMIT
         */
        !TEST(MEM_RESERVE, flAllocationType) && lpAddress != NULL) {
        /* i#1175: NtAllocateVirtualMemory can modify prot on existing pages */
        if (!app_memory_pre_alloc(get_thread_private_dcontext(), lpAddress, dwSize,
                                  osprot_to_memprot(flProtect), false, true /*update*/,
                                  false /*!image*/)) {
            set_last_error(ERROR_INVALID_ADDRESS);
            return NULL;
        }
    }
    res = nt_allocate_virtual_memory(&base, dwSize, flProtect, flAllocationType);
    if (!NT_SUCCESS(res)) {
        set_last_error(ntstatus_to_last_error(res));
        return NULL;
    }
    if (NT_SUCCESS(res)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s => " PFX "-" PFX "\n", __FUNCTION__, lpAddress,
            (app_pc)lpAddress + dwSize);
    }
    return base;
}

BOOL WINAPI
redirect_VirtualFree(__in LPVOID lpAddress, __in SIZE_T dwSize, __in DWORD dwFreeType)
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

BOOL WINAPI
redirect_VirtualProtect(__in LPVOID lpAddress, __in SIZE_T dwSize,
                        __in DWORD flNewProtect, __out PDWORD lpflOldProtect)
{
#ifndef STANDALONE_UNIT_TEST
    if (!dynamo_vm_area_overlap((byte *)lpAddress, ((byte *)lpAddress) + dwSize)) {
        uint new_prot = osprot_to_memprot(flNewProtect);
        uint mod_prot = osprot_to_memprot(new_prot);
        uint old_prot;
        uint res = app_memory_protection_change(get_thread_private_dcontext(), lpAddress,
                                                dwSize, new_prot, &mod_prot, &old_prot,
                                                false /*!image*/);
        if (res == PRETEND_APP_MEM_PROT_CHANGE) {
            if (lpflOldProtect != NULL)
                *lpflOldProtect = memprot_to_osprot(old_prot);
            return TRUE;
        } else if (res == FAIL_APP_MEM_PROT_CHANGE)
            return FALSE;
        else if (res == SUBSET_APP_MEM_PROT_CHANGE)
            flNewProtect = memprot_to_osprot(mod_prot);
        else
            ASSERT(res == DO_APP_MEM_PROT_CHANGE);
    }
#endif
    return (BOOL)protect_virtual_memory(lpAddress, dwSize, flNewProtect,
                                        (uint *)lpflOldProtect);
}

SIZE_T
WINAPI
redirect_VirtualQuery(__in_opt LPCVOID lpAddress,
                      __out_bcount_part(dwLength, return )
                          PMEMORY_BASIC_INFORMATION lpBuffer,
                      __in SIZE_T dwLength)
{
    return redirect_VirtualQueryEx(NT_CURRENT_PROCESS, lpAddress, lpBuffer, dwLength);
}

SIZE_T
WINAPI
redirect_VirtualQueryEx(__in HANDLE hProcess, __in_opt LPCVOID lpAddress,
                        __out_bcount_part(dwLength, return )
                            PMEMORY_BASIC_INFORMATION lpBuffer,
                        __in SIZE_T dwLength)
{
    size_t got;
    app_pc page = (app_pc)PAGE_START(lpAddress);
    NTSTATUS res =
        nt_remote_query_virtual_memory(hProcess, page, lpBuffer, dwLength, &got);
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
test_local(void)
{
    HLOCAL loc;
    PVOID p;
    BOOL ok;

    /**************************************************/
    /* test fixed */
    loc = redirect_LocalAlloc(LMEM_ZEROINIT, 6);
    EXPECT(*(int *)loc == 0, true);
    EXPECT(redirect_LocalSize(loc), 6); /* *Size() returns requested, not padded */

    loc = redirect_LocalReAlloc(loc, 26, LMEM_MOVEABLE | LMEM_ZEROINIT);
    EXPECT(*(int *)loc == 0, true);
    EXPECT(redirect_LocalSize(loc), 26);
    EXPECT(TEST(LMEM_DISCARDABLE, redirect_LocalFlags(loc)), false);

    /* locking should do nothing since fixed */
    EXPECT(redirect_LocalLock(loc) == (LPVOID)loc, true);
    EXPECT(redirect_LocalLock(loc) == (LPVOID)loc, true);
    EXPECT(redirect_LocalUnlock(loc), FALSE);
    EXPECT(get_last_error(), ERROR_NOT_LOCKED);

    loc = redirect_LocalReAlloc(loc, 0, LMEM_MOVEABLE | LMEM_ZEROINIT);
    EXPECT(redirect_LocalSize(loc), 0);
    EXPECT(TEST(LMEM_DISCARDABLE, redirect_LocalFlags(loc)), true);

    /* test LMEM_MODIFY */
    loc = redirect_LocalReAlloc(loc, 0 /*ignored*/, LMEM_MODIFY | LMEM_MOVEABLE);
    EXPECT(loc != NULL, true);
    /* locking should now do something */
    EXPECT(redirect_LocalLock(loc) != NULL, true);
    EXPECT(redirect_LocalLock(loc) != NULL, true);
    EXPECT(redirect_LocalUnlock(loc), TRUE);
    EXPECT(redirect_LocalUnlock(loc), FALSE);
    EXPECT(get_last_error(), NO_ERROR);

    loc = redirect_LocalFree(loc);
    EXPECT(loc == NULL, true);

    /**************************************************/
    /* test moveable */
    loc = redirect_LocalAlloc(LMEM_ZEROINIT | LMEM_MOVEABLE, 6);
    EXPECT(loc != NULL, true);
    p = redirect_LocalLock(loc);
    EXPECT(p != NULL, true);
    EXPECT(*(int *)p == 0, true);
    EXPECT(redirect_LocalSize(loc), 6);
    EXPECT(TEST(LMEM_DISCARDABLE, redirect_LocalFlags(loc)), false);

    ok = redirect_LocalUnlock(loc);
    EXPECT(ok, FALSE);
    EXPECT(get_last_error(), NO_ERROR);
    *(int *)p = 42;
    loc = redirect_LocalReAlloc(loc, 126, LMEM_MOVEABLE | LMEM_ZEROINIT);
    EXPECT(loc != NULL, true);
    EXPECT(redirect_LocalSize(loc), 126);
    p = redirect_LocalLock(p);
    EXPECT(p != NULL, true);
    EXPECT(*(int *)p == 42, true);
    ok = redirect_LocalUnlock(loc);
    EXPECT(ok, FALSE);
    EXPECT(get_last_error(), NO_ERROR);

    loc = redirect_LocalReAlloc(loc, 0, LMEM_MOVEABLE | LMEM_ZEROINIT);
    EXPECT(redirect_LocalSize(loc), 0);
    EXPECT(TEST(LMEM_DISCARDABLE, redirect_LocalFlags(loc)), true);
    loc = redirect_LocalFree(loc);
    EXPECT(loc == NULL, true);
}

static void
test_syscalls(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD dw;
    SIZE_T sz;
    BOOL ok;
    PVOID temp =
        redirect_VirtualAlloc(0, PAGE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    EXPECT(temp != NULL, true);
    sz = redirect_VirtualQuery((char *)temp + PAGE_SIZE / 2, &mbi, sizeof(mbi));
    EXPECT(sz == sizeof(mbi), true);
    EXPECT(mbi.BaseAddress == temp, true);
    EXPECT(mbi.AllocationBase == temp, true);
    EXPECT(mbi.AllocationProtect == PAGE_READWRITE, true);

    EXPECT(redirect_VirtualProtect(temp, PAGE_SIZE / 2, PAGE_READONLY, &dw), true);
    sz = redirect_VirtualQuery((char *)temp + PAGE_SIZE / 4, &mbi, sizeof(mbi));
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

    ran = (PVOID)get_random_offset(POINTER_MAX);
    temp = redirect_EncodePointer(ran);
    EXPECT(temp != ran, true);
    EXPECT(redirect_DecodePointer(temp) == ran, true);

    test_heap();

    test_local();

    test_syscalls();
}
#endif
