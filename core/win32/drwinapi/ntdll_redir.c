/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.   All rights reserved.
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

/* ntdll.dll redirection for custom private library loader */

#include "../../globals.h"
#include "../ntdll.h"
#include "drwinapi.h"
#include "drwinapi_private.h"
#include "ntdll_redir.h"

#ifndef WINDOWS
#    error Windows-only
#endif

/* We use a hashtale for faster lookups than a linear walk */
static strhash_table_t *ntdll_table;
static strhash_table_t *ntdll_win7_table;

/* Since we can't easily have a 2nd copy of ntdll, our 2nd copy of kernel32,
 * etc. use the same ntdll as the app.  We then have to redirect ntdll imports
 * that use shared resources and could interfere with the app.  There is a LOT
 * of stuff to emulate to really be transparent: we're going to add it
 * incrementally as needed, now that we have the infrastructure.
 *
 * FIXME i#235: redirect the rest of the Ldr* routines.  For
 * GetModuleHandle: why does kernel32 seem to do a lot of work?
 * BasepGetModuleHandleExW => RtlPcToFileHeader, RtlComputePrivatizedDllName_U
 * where should we intercept?  why isn't it calling LdrGetDllHandle{,Ex}?
 */
static const redirect_import_t redirect_ntdll[] = {
    { "LdrGetProcedureAddress", (app_pc)redirect_LdrGetProcedureAddress },
    { "LdrLoadDll", (app_pc)redirect_LdrLoadDll },
    { "RtlPcToFileHeader", (app_pc)redirect_RtlPcToFileHeader },
    /* kernel32 passes some of its routines to ntdll where they are
     * stored in function pointers.  xref PR 215408 where on x64 we had
     * issues w/ these not showing up b/c no longer in relocs.
     * kernel32!_BaseDllInitialize calls certain ntdll routines to
     * set up these callbacks:
     */
    /* LdrSetDllManifestProber has more args on win7: see redirect_ntdll_win7 */
    { "LdrSetDllManifestProber", (app_pc)redirect_ignore_arg4 },
    { "RtlSetThreadPoolStartFunc", (app_pc)redirect_ignore_arg8 },
    { "RtlSetUnhandledExceptionFilter", (app_pc)redirect_ignore_arg4 },
    /* avoid attempts to free on private heap allocs made earlier on app heap: */
    { "RtlCleanUpTEBLangLists", (app_pc)redirect_ignore_arg0 },
    /* Rtl*Heap routines:
     * We turn new Heap creation into essentially nops, and we redirect allocs
     * from PEB.ProcessHeap or a Heap whose creation we saw.
     * For now we'll leave the query, walk, enum, etc.  of
     * PEB.ProcessHeap pointing at the app's and focus on allocation.
     * There are many corner cases where we won't be transparent but we'll
     * incrementally add more redirection (i#235) and more transparency: have to
     * start somehwere.  Our biggest problems are ntdll routines that internally
     * allocate or free, esp when combined with the other of the pair from outside.
     */
    { "RtlCreateHeap", (app_pc)redirect_RtlCreateHeap },
    { "RtlDestroyHeap", (app_pc)redirect_RtlDestroyHeap },
    { "RtlAllocateHeap", (app_pc)redirect_RtlAllocateHeap },
    { "RtlReAllocateHeap", (app_pc)redirect_RtlReAllocateHeap },
    { "RtlFreeHeap", (app_pc)redirect_RtlFreeHeap },
    { "RtlSizeHeap", (app_pc)redirect_RtlSizeHeap },
    { "RtlValidateHeap", (app_pc)redirect_RtlValidateHeap },
    { "RtlSetHeapInformation", (app_pc)redirect_RtlSetHeapInformation },
    /* kernel32!LocalFree calls these */
    { "RtlLockHeap", (app_pc)redirect_RtlLockHeap },
    { "RtlUnlockHeap", (app_pc)redirect_RtlUnlockHeap },
    /* We redirect these to our implementations to avoid their internal
     * heap allocs that can end up mixing app and priv heap
     */
    { "RtlInitializeCriticalSection", (app_pc)redirect_RtlInitializeCriticalSection },
    { "RtlInitializeCriticalSectionAndSpinCount",
      (app_pc)redirect_RtlInitializeCriticalSectionAndSpinCount },
    { "RtlInitializeCriticalSectionEx", (app_pc)redirect_RtlInitializeCriticalSectionEx },
    { "RtlDeleteCriticalSection", (app_pc)redirect_RtlDeleteCriticalSection },
    /* We don't redirect the creation but we avoid DR pointers being passed
     * to RtlFreeHeap and subsequent heap corruption by redirecting the frees,
     * since sometimes creation is by direct RtlAllocateHeap.
     */
    { "RtlFreeUnicodeString", (app_pc)redirect_RtlFreeUnicodeString },
    { "RtlFreeAnsiString", (app_pc)redirect_RtlFreeAnsiString },
    { "RtlFreeOemString", (app_pc)redirect_RtlFreeOemString },
#if 0 /* FIXME i#235: redirect these: */
    {"RtlSetUserValueHeap",            (app_pc)redirect_RtlSetUserValueHeap},
    {"RtlGetUserInfoHeap",             (app_pc)redirect_RtlGetUserInfoHeap},
#endif
    /* DrM-i#1066: functions below are hooked by Chrome sandbox */
    { "NtCreateFile", (app_pc)redirect_NtCreateFile },
    { "ZwCreateFile", (app_pc)redirect_NtCreateFile },
    { "NtCreateKey", (app_pc)redirect_NtCreateKey },
    { "ZwCreateKey", (app_pc)redirect_NtCreateKey },
    { "NtMapViewOfSection", (app_pc)redirect_NtMapViewOfSection },
    { "ZwMapViewOfSection", (app_pc)redirect_NtMapViewOfSection },
    { "NtOpenFile", (app_pc)redirect_NtOpenFile },
    { "ZwOpenFile", (app_pc)redirect_NtOpenFile },
    { "NtOpenKey", (app_pc)redirect_NtOpenKey },
    { "ZwOpenKey", (app_pc)redirect_NtOpenKey },
    { "NtOpenKeyEx", (app_pc)redirect_NtOpenKeyEx },
    { "ZwOpenKeyEx", (app_pc)redirect_NtOpenKeyEx },
    { "NtOpenProcess", (app_pc)redirect_NtOpenProcess },
    { "ZwOpenProcess", (app_pc)redirect_NtOpenProcess },
    { "NtOpenProcessToken", (app_pc)redirect_NtOpenProcessToken },
    { "ZwOpenProcessToken", (app_pc)redirect_NtOpenProcessToken },
    { "NtOpenProcessTokenEx", (app_pc)redirect_NtOpenProcessTokenEx },
    { "ZwOpenProcessTokenEx", (app_pc)redirect_NtOpenProcessTokenEx },
    { "NtOpenThread", (app_pc)redirect_NtOpenThread },
    { "ZwOpenThread", (app_pc)redirect_NtOpenThread },
    { "NtOpenThreadToken", (app_pc)redirect_NtOpenThreadToken },
    { "ZwOpenThreadToken", (app_pc)redirect_NtOpenThreadToken },
    { "NtOpenThreadTokenEx", (app_pc)redirect_NtOpenThreadTokenEx },
    { "ZwOpenThreadTokenEx", (app_pc)redirect_NtOpenThreadTokenEx },
    { "NtQueryAttributesFile", (app_pc)redirect_NtQueryAttributesFile },
    { "ZwQueryAttributesFile", (app_pc)redirect_NtQueryAttributesFile },
    { "NtQueryFullAttributesFile", (app_pc)redirect_NtQueryFullAttributesFile },
    { "ZwQueryFullAttributesFile", (app_pc)redirect_NtQueryFullAttributesFile },
    { "NtSetInformationFile", (app_pc)redirect_NtSetInformationFile },
    { "ZwSetInformationFile", (app_pc)redirect_NtSetInformationFile },
    { "NtSetInformationThread", (app_pc)redirect_NtSetInformationThread },
    { "ZwSetInformationThread", (app_pc)redirect_NtSetInformationThread },
    { "NtUnmapViewOfSection", (app_pc)redirect_NtUnmapViewOfSection },
    { "ZwUnmapViewOfSection", (app_pc)redirect_NtUnmapViewOfSection },
    /* i#875: ensure we've isolated FLS */
    { "RtlFlsAlloc", (app_pc)redirect_RtlFlsAlloc },
    { "RtlFlsFree", (app_pc)redirect_RtlFlsFree },
    { "RtlProcessFlsData", (app_pc)redirect_RtlProcessFlsData },
};
#define REDIRECT_NTDLL_NUM (sizeof(redirect_ntdll) / sizeof(redirect_ntdll[0]))

/* For ntdll redirections that differ on Windows 7.  Takes precedence over
 * redirect_ntdll[].
 */
static const redirect_import_t redirect_ntdll_win7[] = {
    /* win7 increases the #args */
    { "LdrSetDllManifestProber", (app_pc)redirect_ignore_arg12 },
};
#define REDIRECT_NTDLL_WIN7_NUM \
    (sizeof(redirect_ntdll_win7) / sizeof(redirect_ntdll_win7[0]))

void
ntdll_redir_init(void)
{
    uint i;
    ntdll_table =
        strhash_hash_create(GLOBAL_DCONTEXT, hashtable_num_bits(REDIRECT_NTDLL_NUM * 2),
                            80 /* load factor: not perf-critical, plus static */,
                            HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                            NULL _IF_DEBUG("ntdll redirection table"));
    TABLE_RWLOCK(ntdll_table, write, lock);
    for (i = 0; i < REDIRECT_NTDLL_NUM; i++) {
        strhash_hash_add(GLOBAL_DCONTEXT, ntdll_table, redirect_ntdll[i].name,
                         (void *)redirect_ntdll[i].func);
    }
    TABLE_RWLOCK(ntdll_table, write, unlock);

    if (get_os_version() >= WINDOWS_VERSION_7) {
        ntdll_win7_table =
            strhash_hash_create(GLOBAL_DCONTEXT, REDIRECT_NTDLL_WIN7_NUM * 2,
                                80 /* load factor: not perf-critical, plus static */,
                                HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                                NULL _IF_DEBUG("ntdll win7 redirection table"));
        TABLE_RWLOCK(ntdll_win7_table, write, lock);
        for (i = 0; i < REDIRECT_NTDLL_WIN7_NUM; i++) {
            strhash_hash_add(GLOBAL_DCONTEXT, ntdll_win7_table,
                             redirect_ntdll_win7[i].name,
                             (void *)redirect_ntdll_win7[i].func);
        }
        TABLE_RWLOCK(ntdll_win7_table, write, unlock);
    }
}

void
ntdll_redir_exit(void)
{
    strhash_hash_destroy(GLOBAL_DCONTEXT, ntdll_table);
    if (ntdll_win7_table != NULL) {
        strhash_hash_destroy(GLOBAL_DCONTEXT, ntdll_win7_table);
    }
}

app_pc
ntdll_redir_lookup(const char *name)
{
    app_pc res;
    if (ntdll_win7_table != NULL) {
        TABLE_RWLOCK(ntdll_win7_table, read, lock);
        res = strhash_hash_lookup(GLOBAL_DCONTEXT, ntdll_win7_table, name);
        TABLE_RWLOCK(ntdll_win7_table, read, unlock);
        if (res != NULL)
            return res;
    }
    TABLE_RWLOCK(ntdll_table, read, lock);
    res = strhash_hash_lookup(GLOBAL_DCONTEXT, ntdll_table, name);
    TABLE_RWLOCK(ntdll_table, read, unlock);
    return res;
}

/****************************************************************************
 * Rtl*Heap redirection
 *
 * We only redirect for PEB.ProcessHeap or heaps whose creation we saw
 * (e.g., private kernel32!_crtheap).
 * See comments at top of file and i#235 for adding further redirection.
 */

HANDLE WINAPI
RtlCreateHeap(ULONG flags, void *base, size_t reserve_sz, size_t commit_sz, void *lock,
              void *params);

BOOL WINAPI
RtlDestroyHeap(HANDLE base);

HANDLE WINAPI
redirect_RtlCreateHeap(ULONG flags, void *base, size_t reserve_sz, size_t commit_sz,
                       void *lock, void *params)
{
    if (INTERNAL_OPTION(privlib_privheap)) {
        /* We don't want to waste space by letting a Heap be created
         * and not used so we nop this.  We need to return something
         * here, and distinguish a nop-ed from real in Destroy, so we
         * allocate a token block.
         */
        LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX "\n", __FUNCTION__, base);
        return (HANDLE)global_heap_alloc(1 HEAPACCT(ACCT_LIBDUP));
    } else
        return RtlCreateHeap(flags, base, reserve_sz, commit_sz, lock, params);
}

bool
redirect_heap_call(HANDLE heap)
{
    ASSERT(!dynamo_initialized || dynamo_exited || standalone_library ||
           get_thread_private_dcontext() == NULL /*thread exiting*/ ||
           !os_using_app_state(get_thread_private_dcontext()));
    if (!INTERNAL_OPTION(privlib_privheap))
        return false;
    /* either default heap, or one whose creation we intercepted */
    return (
        /* check both current and private: should be same, but
         * handle case where didn't swap
         */
        heap == get_private_peb()->ProcessHeap ||
        heap == get_peb(NT_CURRENT_PROCESS)->ProcessHeap ||
        is_dynamo_address((byte *)heap));
}

BOOL WINAPI
redirect_RtlDestroyHeap(HANDLE base)
{
    if (redirect_heap_call(base)) {
        /* XXX i#: need to iterate over all blocks in the heap and free them:
         * would have to keep a list of blocks.
         * For now assume all private heaps practice individual dealloc
         * instead of whole-pool-free.
         */
        LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX "\n", __FUNCTION__, base);
        global_heap_free((byte *)base, 1 HEAPACCT(ACCT_LIBDUP));
        return TRUE;
    } else
        return RtlDestroyHeap(base);
}

void *WINAPI
RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size);

void *
wrapped_dr_alloc(ULONG flags, SIZE_T size)
{
    /* HeapAlloc returns 16-byte-aligned for 64-bit and 8-byte-aligned for 32-bit.
     * We use redirect_malloc() to get that alignment.
     */
    void *mem = redirect_malloc(size);
    if (mem == NULL) {
        /* TODO: support HEAP_GENERATE_EXCEPTIONS (xref PR 406742).
         * redirect_malloc() already did a CLIENT_ASSERT too: we'd want to remove that.
         */
        ASSERT_NOT_REACHED();
        return NULL;
    }
    if (TEST(HEAP_ZERO_MEMORY, flags))
        memset(mem, 0, size);
    return mem;
}

void
wrapped_dr_free(byte *ptr)
{
    redirect_free(ptr);
}

static inline size_t
wrapped_dr_size(byte *ptr)
{
    return redirect_malloc_requested_size(ptr);
}

void *WINAPI
redirect_RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size)
{
    if (redirect_heap_call(heap)) {
        void *mem = wrapped_dr_alloc(flags, size);
        LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX " " PIFX "\n", __FUNCTION__, mem, size);
        return mem;
    } else {
        void *res = RtlAllocateHeap(heap, flags, size);
        LOG(GLOBAL, LOG_LOADER, 2, "native %s " PFX " " PIFX "\n", __FUNCTION__, res,
            size);
        return res;
    }
}

void *WINAPI
RtlReAllocateHeap(HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size);

void *WINAPI
redirect_RtlReAllocateHeap(HANDLE heap, ULONG flags, byte *ptr, SIZE_T size)
{
    /* FIXME i#235: on x64 using dbghelp, SymLoadModule64 calls
     * kernel32!CreateFileW which calls
     * ntdll!RtlDosPathNameToRelativeNtPathName_U_WithStatus which calls
     * ntdll!RtlpDosPathNameToRelativeNtPathName_Ustr which directly calls
     * RtlAllocateHeap and passes peb->ProcessHeap: but then it's
     * kernel32!CreateFileW that calls RtlFreeHeap, so we end up seeing just a
     * free with no corresponding alloc.  For now we handle by letting non-DR
     * addresses go natively.  Xref the opposite problem with
     * RtlFreeUnicodeString, handled below.
     */
    if (ptr == NULL) /* unlike realloc(), HeapReAlloc fails on NULL */
        return NULL;
    if (redirect_heap_call(heap) && is_dynamo_address(ptr)) {
        byte *buf = NULL;
        if (TEST(HEAP_REALLOC_IN_PLACE_ONLY, flags)) {
            ASSERT_NOT_IMPLEMENTED(false);
            return NULL;
        }
        /* RtlReAllocateHeap does re-alloc 0-sized */
        LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX " " PIFX "\n", __FUNCTION__, ptr, size);
        buf = redirect_RtlAllocateHeap(heap, flags, size);
        if (buf != NULL) {
            size_t old_size = wrapped_dr_size(ptr);
            size_t min_size = MIN(old_size, size);
            memcpy(buf, ptr, min_size);
            redirect_RtlFreeHeap(heap, flags, ptr);
        }
        return (void *)buf;
    } else {
        void *res = RtlReAllocateHeap(heap, flags, ptr, size);
        LOG(GLOBAL, LOG_LOADER, 2, "native %s " PFX " " PIFX "\n", __FUNCTION__, res,
            size);
        return res;
    }
}

BOOL WINAPI
RtlFreeHeap(HANDLE heap, ULONG flags, PVOID ptr);

SIZE_T WINAPI
RtlSizeHeap(HANDLE heap, ULONG flags, PVOID ptr);

BOOL WINAPI
RtlValidateHeap(HANDLE heap, DWORD flags, void *ptr);

BOOL WINAPI
RtlLockHeap(HANDLE heap);

BOOL WINAPI
RtlUnlockHeap(HANDLE heap);

BOOL WINAPI
redirect_RtlFreeHeap(HANDLE heap, ULONG flags, byte *ptr)
{
    if (redirect_heap_call(heap) && is_dynamo_address(ptr) /*see above*/) {
        ASSERT(INTERNAL_OPTION(privlib_privheap));
        if (ptr != NULL) {
            LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX "\n", __FUNCTION__, ptr);
            wrapped_dr_free(ptr);
            return TRUE;
        } else
            return FALSE;
    } else {
        LOG(GLOBAL, LOG_LOADER, 2, "native %s " PFX " " PIFX "\n", __FUNCTION__, ptr,
            (ptr == NULL ? 0 : RtlSizeHeap(heap, flags, ptr)));
        return RtlFreeHeap(heap, flags, ptr);
    }
}

SIZE_T WINAPI
redirect_RtlSizeHeap(HANDLE heap, ULONG flags, byte *ptr)
{
    if (redirect_heap_call(heap) && is_dynamo_address(ptr) /*see above*/) {
        ASSERT(INTERNAL_OPTION(privlib_privheap));
        if (ptr != NULL)
            return wrapped_dr_size(ptr);
        else
            return 0;
    } else {
        return RtlSizeHeap(heap, flags, ptr);
    }
}

BOOL WINAPI
redirect_RtlValidateHeap(HANDLE heap, DWORD flags, void *ptr)
{
    if (redirect_heap_call(heap)) {
        /* nop: we assume no caller expects false */
        return TRUE;
    } else {
        return RtlValidateHeap(heap, flags, ptr);
    }
}

/* These are called by LocalFree, passing kernel32!BaseHeap == peb->ProcessHeap */
BOOL WINAPI
redirect_RtlLockHeap(HANDLE heap)
{
    /* If the main heap, we assume any subsequent alloc/free will be through
     * DR heap, so we nop this
     */
    if (redirect_heap_call(heap)) {
        /* nop */
        return TRUE;
    } else {
        return RtlLockHeap(heap);
    }
}

BOOL WINAPI
redirect_RtlUnlockHeap(HANDLE heap)
{
    /* If the main heap, we assume any prior alloc/free was through
     * DR heap, so we nop this
     */
    if (redirect_heap_call(heap)) {
        /* nop */
        return TRUE;
    } else {
        return RtlUnlockHeap(heap);
    }
}

BOOL WINAPI
redirect_RtlSetHeapInformation(HANDLE HeapHandle,
                               HEAP_INFORMATION_CLASS HeapInformationClass,
                               PVOID HeapInformation, SIZE_T HeapInformationLength)
{
    /* xref DrMem i#280.
     * The only options are HeapEnableTerminationOnCorruption and
     * HeapCompatibilityInformation LFH, neither of which we want.
     * Running this routine causes problems on Win7 (i#709).
     */
    return TRUE;
}

void WINAPI
RtlFreeUnicodeString(UNICODE_STRING *string);

void WINAPI
redirect_RtlFreeUnicodeString(UNICODE_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeUnicodeString(string);
}

void WINAPI
RtlFreeAnsiString(ANSI_STRING *string);

void WINAPI
redirect_RtlFreeAnsiString(ANSI_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeAnsiString(string);
}

void WINAPI
RtlFreeOemString(OEM_STRING *string);

void WINAPI
redirect_RtlFreeOemString(OEM_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeOemString(string);
}

/****************************************************************************
 * Rtl*CriticalSection redirection
 */

#ifndef RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO
#    define RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO 0x1000000
#endif
#ifndef RTL_CRITICAL_SECTION_FLAG_STATIC_INIT
#    define RTL_CRITICAL_SECTION_FLAG_STATIC_INIT 0x4000000
#endif

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSection(RTL_CRITICAL_SECTION *crit)
{
    return redirect_RtlInitializeCriticalSectionEx(crit, 0, 0);
}

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSectionAndSpinCount(RTL_CRITICAL_SECTION *crit,
                                                  ULONG spincount)
{
    return redirect_RtlInitializeCriticalSectionEx(crit, spincount, 0);
}

NTSTATUS WINAPI
redirect_RtlInitializeCriticalSectionEx(RTL_CRITICAL_SECTION *crit, ULONG spincount,
                                        ULONG flags)
{
    /* We cannot allow ntdll!RtlpAllocateDebugInfo to be called as it
     * uses its own free list RtlCriticalSectionDebugSList which is
     * shared w/ the app and can result in mixing app and private heap
     * objects but with the wrong Heap handle, leading to crashes
     * (xref Dr. Memory i#333).
     */
    LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX "\n", __FUNCTION__, crit);
    ASSERT(get_own_teb()->ProcessEnvironmentBlock == get_private_peb() ||
           standalone_library);
    if (crit == NULL)
        return STATUS_INVALID_PARAMETER;
    if (TEST(RTL_CRITICAL_SECTION_FLAG_STATIC_INIT, flags)) {
        /* We're supposed to use a memory pool but it's not
         * clear whether it really matters so we ignore this flag.
         */
        LOG(GLOBAL, LOG_LOADER, 2, "%s: ignoring static-init flag\n", __FUNCTION__);
    }

    memset(crit, 0, sizeof(*crit));
    crit->LockCount = -1;
    if (get_own_peb()->NumberOfProcessors < 2)
        crit->SpinCount = 0;
    else
        crit->SpinCount = (spincount & ~0x80000000);

    if (TEST(RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO, flags))
        crit->DebugInfo = NULL;
    else {
        crit->DebugInfo = wrapped_dr_alloc(0, sizeof(*crit->DebugInfo));
    }
    if (crit->DebugInfo != NULL) {
        memset(crit->DebugInfo, 0, sizeof(*crit->DebugInfo));
        crit->DebugInfo->CriticalSection = crit;
        crit->DebugInfo->ProcessLocksList.Blink = &(crit->DebugInfo->ProcessLocksList);
        crit->DebugInfo->ProcessLocksList.Flink = &(crit->DebugInfo->ProcessLocksList);
    }

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI
redirect_RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *crit)
{
    GET_NTDLL(RtlDeleteCriticalSection, (RTL_CRITICAL_SECTION * crit));
    LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX "\n", __FUNCTION__, crit);
    ASSERT(get_own_teb()->ProcessEnvironmentBlock == get_private_peb() ||
           standalone_library);
    if (crit == NULL)
        return STATUS_INVALID_PARAMETER;
    if (crit->DebugInfo != NULL) {
        if (is_dynamo_address((byte *)crit->DebugInfo))
            wrapped_dr_free((byte *)crit->DebugInfo);
        else {
            /* somehow a critsec created elsewhere is being destroyed here! */
            ASSERT_NOT_REACHED();
            return RtlDeleteCriticalSection(crit);
        }
    }
    close_handle(crit->LockSemaphore);
    memset(crit, 0, sizeof(*crit));
    crit->LockCount = -1;
    return STATUS_SUCCESS;
}

/****************************************************************************
 * DrM-i#1066: redirect some syscalls from ntdll
 */
NTSTATUS WINAPI
redirect_NtCreateFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PIO_STATUS_BLOCK io_status_block, PLARGE_INTEGER allocation_size,
                      ULONG file_attributes, ULONG share_access, ULONG create_disposition,
                      ULONG create_options, PVOID ea_buffer, ULONG ea_length)
{
    return nt_raw_CreateFile(file_handle, desired_access, object_attributes,
                             io_status_block, allocation_size, file_attributes,
                             share_access, create_disposition, create_options, ea_buffer,
                             ea_length);
}

NTSTATUS WINAPI
redirect_NtCreateKey(PHANDLE key_handle, ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes, ULONG title_index,
                     PUNICODE_STRING class, ULONG create_options, PULONG disposition)
{
    return nt_raw_CreateKey(key_handle, desired_access, object_attributes, title_index,
                            class, create_options, disposition);
}

NTSTATUS WINAPI
redirect_NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle,
                            PVOID *base_address, ULONG_PTR zero_bits, SIZE_T commit_size,
                            PLARGE_INTEGER section_offset, PSIZE_T view_size,
                            SECTION_INHERIT inherit_disposition, ULONG allocation_type,
                            ULONG win32_protect)
{
    NTSTATUS res = nt_raw_MapViewOfSection(
        section_handle, process_handle, base_address, zero_bits, commit_size,
        section_offset, view_size, inherit_disposition, allocation_type, win32_protect);
    if (NT_SUCCESS(res)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s => " PFX "-" PFX "\n", __FUNCTION__,
            *(byte **)base_address, *view_size);
    }
    return res;
}

NTSTATUS WINAPI
redirect_NtOpenFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    PIO_STATUS_BLOCK io_status_block, ULONG share_access,
                    ULONG open_options)
{
    return nt_raw_OpenFile(file_handle, desired_access, object_attributes,
                           io_status_block, share_access, open_options);
}

NTSTATUS WINAPI
redirect_NtOpenKey(PHANDLE key_handle, ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes)
{
    return nt_raw_OpenKey(key_handle, desired_access, object_attributes);
}

NTSTATUS WINAPI
redirect_NtOpenKeyEx(PHANDLE key_handle, ACCESS_MASK desired_access,
                     POBJECT_ATTRIBUTES object_attributes, ULONG open_options)
{
    return nt_raw_OpenKeyEx(key_handle, desired_access, object_attributes, open_options);
}

NTSTATUS WINAPI
redirect_NtOpenProcess(PHANDLE process_handle, ACCESS_MASK desired_access,
                       POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id)
{
    return nt_raw_OpenProcess(process_handle, desired_access, object_attributes,
                              client_id);
}

NTSTATUS WINAPI
redirect_NtOpenProcessToken(HANDLE process_handle, ACCESS_MASK desired_access,
                            PHANDLE token_handle)
{
    return nt_raw_OpenProcessToken(process_handle, desired_access, token_handle);
}

NTSTATUS WINAPI
redirect_NtOpenProcessTokenEx(HANDLE process_handle, ACCESS_MASK desired_access,
                              ULONG handle_attributes, PHANDLE token_handle)
{
    return nt_raw_OpenProcessTokenEx(process_handle, desired_access, handle_attributes,
                                     token_handle);
}

NTSTATUS WINAPI
redirect_NtOpenThread(PHANDLE thread_handle, ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id)
{
    return nt_raw_OpenThread(thread_handle, desired_access, object_attributes, client_id);
}

NTSTATUS WINAPI
redirect_NtOpenThreadToken(HANDLE thread_handle, ACCESS_MASK desired_access,
                           BOOLEAN open_as_self, PHANDLE token_handle)
{
    return nt_raw_OpenThreadToken(thread_handle, desired_access, open_as_self,
                                  token_handle);
}

NTSTATUS WINAPI
redirect_NtOpenThreadTokenEx(HANDLE thread_handle, ACCESS_MASK desired_access,
                             BOOLEAN open_as_self, ULONG handle_attributes,
                             PHANDLE token_handle)
{
    return nt_raw_OpenThreadTokenEx(thread_handle, desired_access, open_as_self,
                                    handle_attributes, token_handle);
}

NTSTATUS WINAPI
redirect_NtQueryAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                               PFILE_BASIC_INFORMATION file_information)
{
    return nt_raw_QueryAttributesFile(object_attributes, file_information);
}

NTSTATUS WINAPI
redirect_NtQueryFullAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                                   PFILE_NETWORK_OPEN_INFORMATION file_information)
{
    return nt_raw_QueryFullAttributesFile(object_attributes, file_information);
}

NTSTATUS WINAPI
redirect_NtSetInformationFile(HANDLE file_handle, PIO_STATUS_BLOCK io_status_block,
                              PVOID file_information, ULONG length,
                              FILE_INFORMATION_CLASS file_information_class)
{
    return nt_raw_SetInformationFile(file_handle, io_status_block, file_information,
                                     length, file_information_class);
}

NTSTATUS WINAPI
redirect_NtSetInformationThread(HANDLE thread_handle,
                                THREADINFOCLASS thread_information_class,
                                PVOID thread_information, ULONG thread_information_length)
{
    return nt_raw_SetInformationThread(thread_handle, thread_information_class,
                                       thread_information, thread_information_length);
}

NTSTATUS WINAPI
redirect_NtUnmapViewOfSection(HANDLE process_handle, PVOID base_address)
{
    return nt_raw_UnmapViewOfSection(process_handle, base_address);
}

NTSTATUS NTAPI
redirect_LdrGetProcedureAddress(IN HMODULE modbase, IN PANSI_STRING func OPTIONAL,
                                IN WORD ordinal OPTIONAL, OUT PVOID *addr)
{
    /* We ignore ordinal.  Our target is private kernel32's GetProcAddress
     * invoked dynamically so we didn't redirect it directly, and it
     * passes 0 for ordinal.
     */
    if (func == NULL || func->Buffer == NULL)
        return STATUS_INVALID_PARAMETER;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX "%s\n", __FUNCTION__, modbase, func->Buffer);
    /* redirect_GetProcAddress invokes the app kernel32 version if it fails, trying
     * to handle forwarder corner cases or sthg.  We don't bother here.
     */
    if (!drwinapi_redirect_getprocaddr((app_pc)modbase, func->Buffer, (app_pc *)addr))
        return STATUS_UNSUCCESSFUL;
    else
        return STATUS_SUCCESS;
}

NTSTATUS NTAPI
redirect_LdrLoadDll(IN PWSTR path OPTIONAL, IN PULONG characteristics OPTIONAL,
                    IN PUNICODE_STRING name, OUT PVOID *handle)
{
    app_pc res = NULL;
    char buf[MAXIMUM_PATH];
    if (name == NULL || name->Buffer == NULL ||
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%S", name->Buffer) < 0)
        return STATUS_INVALID_PARAMETER;
    NULL_TERMINATE_BUFFER(buf);
    res = locate_and_load_private_library(buf, false /*!reachable*/);
    if (res == NULL) {
        /* XXX: should we call the app's ntdll routine?  Xref similar discussions
         * in other redirection routines, to try and handle corner cases our own
         * routines don't support.  But seems best to fail for now.
         */
        return STATUS_UNSUCCESSFUL;
    } else {
        *handle = (PVOID)res;
        return STATUS_SUCCESS;
    }
}

PVOID NTAPI
redirect_RtlPcToFileHeader(__in PVOID PcValue, __out PVOID *BaseOfImage)
{
    PVOID res = NULL;
    privmod_t *mod;
    if (BaseOfImage == NULL) {
        /* The real thing seems to just crash, but we can be more robust */
        set_last_error(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_pc(PcValue);
    if (mod != NULL)
        res = mod->base;
    release_recursive_lock(&privload_lock);
    *BaseOfImage = res;
    LOG(GLOBAL, LOG_LOADER, 2, "%s " PFX " => " PFX "\n", __FUNCTION__, PcValue, res);
    return res;
}

/***************************************************************************
 * i#875: FLS isolation
 */

#ifndef FLS_MAX_COUNT
/* The max is 4096 on Win10-1909 (and probably earlier) but we do not try to emulate
 * that maximum since we're using the limited FlsBitmapBits in the PEB still.
 */
#    define FLS_MAX_COUNT 128
#endif

void
ntdll_redir_fls_init(PEB *app_peb, PEB *private_peb)
{
    /* FLS is supported in WinXP-64 or later */
    ASSERT(get_os_version() >= WINDOWS_VERSION_2003);

    /* i#3633: Implement FLS isolation for Win10 1903+ where FLS data is no longer
     * in the PEB. It is now SparePointers/Ulongs. We will use them as PEB->Fls*.
     */

    /* We need a deep copy of FLS structures */
    private_peb->FlsBitmap =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, RTL_BITMAP, ACCT_LIBDUP, UNPROTECTED);
    if (app_peb->FlsBitmap == NULL)
        private_peb->FlsBitmap->SizeOfBitMap = FLS_MAX_COUNT;
    else
        private_peb->FlsBitmap->SizeOfBitMap = app_peb->FlsBitmap->SizeOfBitMap;
    private_peb->FlsBitmap->BitMapBuffer = (LPBYTE)&private_peb->FlsBitmapBits;
    memset(private_peb->FlsBitmapBits, 0, sizeof(private_peb->FlsBitmapBits));

    private_peb->FlsHighIndex = 0;

    /* We initialize this to zero (required for redirect_RtlProcessFlsData),
     * and we assume no pre-existing entries (just like we start FlsList* empty).
     */
    private_peb->FlsCallback = HEAP_ARRAY_ALLOC_MEMSET(
        GLOBAL_DCONTEXT, PVOID, private_peb->FlsBitmap->SizeOfBitMap, ACCT_LIBDUP,
        UNPROTECTED, 0);

    /* Start with empty values, regardless of what app libs did prior to us
     * taking over.  FIXME: if we ever have attach will have to verify this:
     * can priv libs always live in their own universe that starts empty?
     */
    private_peb->FlsListHead.Flink = (LIST_ENTRY *)&private_peb->FlsListHead;
    private_peb->FlsListHead.Blink = (LIST_ENTRY *)&private_peb->FlsListHead;
}

void
ntdll_redir_fls_exit(PEB *private_peb)
{
    /* FLS is supported in WinXP-64 or later */
    ASSERT(get_os_version() >= WINDOWS_VERSION_2003);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, private_peb->FlsCallback, PVOID,
                    private_peb->FlsBitmap->SizeOfBitMap, ACCT_LIBDUP, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, private_peb->FlsBitmap, RTL_BITMAP, ACCT_LIBDUP,
                   UNPROTECTED);
}

/* i#3633: Fix Windows 1903 issue. FLS is not held inside of PEB but in private
 * variables inside of ntdll.dll. In case that FLS slips, and priv_fls_data ends up
 * in ntdll.dll internal struct, we will perform unlinking to prevent crashes which
 * can happen if priv_fls_data remains inside of ntdll.dll.
 */
void
ntdll_redir_fls_thread_exit(PPVOID fls_data_ptr)
{
    PEB *peb = get_private_peb();
    LIST_ENTRY *fls_data;
    NTSTATUS res;
    if (fls_data_ptr == NULL)
        return;

    res = RtlEnterCriticalSection(peb->FastPebLock);
    if (!NT_SUCCESS(res))
        return;

    fls_data = (LIST_ENTRY *)fls_data_ptr;

    fls_data->Flink->Blink = fls_data->Blink;
    fls_data->Blink->Flink = fls_data->Flink;

    RtlLeaveCriticalSection(peb->FastPebLock);
    return;
}

NTSTATUS NTAPI
redirect_RtlFlsAlloc(IN PFLS_CALLBACK_FUNCTION cb, OUT PDWORD index_out)
{
    PEB *peb = get_private_peb();
    DWORD index;
    NTSTATUS res;
    /* FLS is supported in WinXP-64 or later */
    ASSERT(get_os_version() >= WINDOWS_VERSION_2003);
    /* We avoid the synchronization done normally (RtlAcquireSRWLockExclusive
     * on RtlpFlsLock) and instead use the private PEB lock to keep things
     * isolated.
     */
    res = RtlEnterCriticalSection(peb->FastPebLock);
    if (!NT_SUCCESS(res))
        return res;

    index = bitmap_find_free_sequence(peb->FlsBitmap->BitMapBuffer,
                                      peb->FlsBitmap->SizeOfBitMap, 1,
                                      false /*!top_down*/, 0, 0 /*no alignment*/);
    if (index == -1) {
        res = STATUS_NO_MEMORY; /* observed in real ntdll */
    } else {
        *index_out = index;
        bitmap_mark_taken_sequence(peb->FlsBitmap->BitMapBuffer,
                                   peb->FlsBitmap->SizeOfBitMap, index, index + 1);
        if (index > peb->FlsHighIndex)
            peb->FlsHighIndex = index;
        peb->FlsCallback[index] = (PVOID)cb;
    }

    res = RtlLeaveCriticalSection(peb->FastPebLock);
    if (!NT_SUCCESS(res))
        return res;
    else
        return STATUS_SUCCESS;
}

NTSTATUS NTAPI
redirect_RtlFlsFree(IN DWORD index)
{
    PEB *peb = get_private_peb();
    TEB *teb = get_own_teb();
    NTSTATUS res;
    /* FLS is supported in WinXP-64 or later */
    ASSERT(get_os_version() >= WINDOWS_VERSION_2003);

    if (index >= peb->FlsBitmap->SizeOfBitMap)
        return STATUS_INVALID_PARAMETER;

    res = RtlEnterCriticalSection(peb->FastPebLock);
    if (!NT_SUCCESS(res))
        return res;

    bitmap_mark_freed_sequence(peb->FlsBitmap->BitMapBuffer, peb->FlsBitmap->SizeOfBitMap,
                               index, 1);
    /* Call the cb, if the slot value is non-NULL */
    if (peb->FlsCallback[index] != NULL &&
        teb->FlsData[index + TEB_FLS_DATA_OFFS] != NULL) {
        PFLS_CALLBACK_FUNCTION func =
            (PFLS_CALLBACK_FUNCTION)convert_data_to_function(peb->FlsCallback[index]);
        (*func)(teb->FlsData[index + TEB_FLS_DATA_OFFS]);
    }
    peb->FlsCallback[index] = NULL;
    /* Not bothering to figure out whether we can reduce peb->FlsHighIndex */

    res = RtlLeaveCriticalSection(peb->FastPebLock);
    if (!NT_SUCCESS(res))
        return res;
    else
        return STATUS_SUCCESS;
}

NTSTATUS NTAPI
redirect_RtlProcessFlsData(IN PLIST_ENTRY fls_data)
{
    PEB *peb = get_private_peb();
    TEB *teb = get_own_teb();
    /* FlsData is a LIST_ENTRY with as payload an array of void* values.
     * If that changes we'll need to change TEB_FLS_DATA_OFFS.
     */
    size_t fls_data_sz =
        sizeof(LIST_ENTRY) + sizeof(void *) * peb->FlsBitmap->SizeOfBitMap;
    /* FLS is supported in WinXP-64 or later */
    ASSERT(get_os_version() >= WINDOWS_VERSION_2003);
    if (fls_data == NULL) {
        NTSTATUS res;
        LIST_ENTRY *tmp;
        ASSERT(teb->FlsData == NULL); /* we're installing for the current fiber */
        res = RtlEnterCriticalSection(peb->FastPebLock);
        if (!NT_SUCCESS(res))
            return res;
        teb->FlsData = global_heap_alloc(fls_data_sz HEAPACCT(ACCT_LIBDUP));
        memset(teb->FlsData, 0, fls_data_sz);

        /* From observation, a new FlsData is appended to the whole-process
         * doubly-linked circular list with a permanent head entry at
         * PEB.FlsListHead.
         */
        tmp = peb->FlsListHead.Blink;
        peb->FlsListHead.Blink = (PVOID)teb->FlsData;
        ((LIST_ENTRY *)teb->FlsData)->Flink = &peb->FlsListHead;
        ((LIST_ENTRY *)teb->FlsData)->Blink = tmp;
        tmp->Flink = (PVOID)teb->FlsData;

        res = RtlLeaveCriticalSection(peb->FastPebLock);
        if (!NT_SUCCESS(res)) {
            global_heap_free(teb->FlsData, fls_data_sz HEAPACCT(ACCT_LIBDUP));
            return res;
        }
    } else {
        /* MSDN says "FlsCallback is called on fiber deletion, thread exit, and
         * when an FLS index is freed".  We expect priv lib code that we don't
         * redirect to call this routine for the first two.
         */
        uint i;
        for (i = 0; i < peb->FlsHighIndex; i++) {
            /* Only call it if the slot value is non-NULL */
            if (peb->FlsCallback[i] != NULL &&
                teb->FlsData[i + TEB_FLS_DATA_OFFS] != NULL) {
                PFLS_CALLBACK_FUNCTION func =
                    (PFLS_CALLBACK_FUNCTION)convert_data_to_function(peb->FlsCallback[i]);
                (*func)(((PPVOID)(fls_data + 1))[i]);
            }
        }

        fls_data->Flink->Blink = fls_data->Blink;
        fls_data->Blink->Flink = fls_data->Flink;

        global_heap_free(fls_data, fls_data_sz HEAPACCT(ACCT_LIBDUP));
    }
    return STATUS_SUCCESS;
}

/* XXX: unfortunately we don't yet have a nice way to add unit tests for
 * library lookup and other redirections that aren't as isolated as
 * file or synch operations.
 */
