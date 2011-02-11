/* **********************************************************
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

/*
 * loader.c: custom private library loader for Windows
 *
 * original case: i#157
 *
 * unfinished/additional features:
 *
 * i#235: redirect more of ntdll for more transparent private libraries:
 * - in particular, redirect Ldr*, or at least kernel32!*W
 * - we'll redirect any additional routines as transparency issues come up
 *
 * i#350: no-dcontext try/except:749
 * - then we can check readability of everything more easily: today
 *   not checking everything in the name of performance
 *
 * i#233: advanced loader features:
 * - delay-load dlls
 * - bound imports
 * - import hint
 * - TLS (though expect only in .exe not .dll)
 *
 * i#234: earliest injection:
 * - use bootstrap loader w/ manual syscalls or ntdll binding to load DR
 *   itself with this private loader at very first APC point
 *
 * i#249: TLS/TEB/PEB isolation for private dll copies
 * - -private_peb uses a private PEB copy, but is limited in several respects:
 *   * uses a shallow copy
 *   * does not intercept private libs/client using NtQueryInformationProcess
 *     but kernel seems to just use TEB pointer anyway!
 *   * added dr_get_app_PEB() for client to get app PEB
 */

#include "../globals.h"
#include "../module_shared.h"
#include "ntdll.h"
#include "os_private.h"
#include "diagnost.h" /* to read systemroot reg key */

#ifdef X64
# define IMAGE_ORDINAL_FLAG IMAGE_ORDINAL_FLAG64
#else
# define IMAGE_ORDINAL_FLAG IMAGE_ORDINAL_FLAG32
#endif


/* Not persistent across code cache execution, so not protected */
DECLARE_NEVERPROT_VAR(static char modpath[MAXIMUM_PATH], {0});
DECLARE_NEVERPROT_VAR(static char forwmodpath[MAXIMUM_PATH], {0});

/* Written during initialization only */
static char systemroot[MAXIMUM_PATH];

/* PE entry points take 3 args */
typedef bool (WINAPI *dllmain_t)(HANDLE, DWORD, LPVOID);

/* forward decls */

static void
privload_init_search_paths(void);

static bool
privload_get_import_descriptor(privmod_t *mod, IMAGE_IMPORT_DESCRIPTOR **imports OUT,
                               app_pc *imports_end OUT);

static bool
privload_process_one_import(privmod_t *mod, privmod_t *impmod,
                            IMAGE_THUNK_DATA *lookup, app_pc *address);

static const char *
privload_map_name(const char *impname, privmod_t *immed_dep);

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent);

/* Redirection of ntdll routines that for transparency reasons we can't
 * point at the real ntdll.  If we get a lot of these should switch to
 * a hashtable.
 */
typedef struct _redirect_import_t {
    const char *name;
    app_pc func;
} redirect_import_t;


static app_pc
privload_redirect_imports(privmod_t *impmod, const char *name);

static bool WINAPI
redirect_ignore_arg4(void *arg1);

static bool WINAPI
redirect_ignore_arg8(void *arg1, void *arg2);

static bool WINAPI
redirect_ignore_arg12(void *arg1, void *arg2, void *arg3);

static void * WINAPI
redirect_RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size);

static void * WINAPI
redirect_RtlReAllocateHeap(HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size);

static bool WINAPI
redirect_RtlFreeHeap(HANDLE heap, ULONG flags, PVOID ptr);

static SIZE_T WINAPI
redirect_RtlSizeHeap(HANDLE heap, ULONG flags, PVOID ptr);

static bool WINAPI
redirect_RtlLockHeap(HANDLE heap);

static bool WINAPI
redirect_RtlUnlockHeap(HANDLE heap);

static void WINAPI
redirect_RtlFreeUnicodeString(UNICODE_STRING *string);

static void WINAPI
redirect_RtlFreeAnsiString(ANSI_STRING *string);

static void WINAPI
redirect_RtlFreeOemString(OEM_STRING *string);

static DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb);

static HMODULE WINAPI
redirect_GetModuleHandleA(const char *name);

static FARPROC WINAPI
redirect_GetProcAddress(app_pc modbase, const char *name);

static HMODULE WINAPI
redirect_LoadLibraryA(const char *name);

/* Since we can't easily have a 2nd copy of ntdll, our 2nd copy of kernel32,
 * etc. use the same ntdll as the app.  We then have to redirect ntdll imports
 * that use shared resources and could interfere with the app.  There is a LOT
 * of stuff to emulate to really be transparent: we're going to add it
 * incrementally as needed, now that we have the infrastructure.
 *
 * FIXME i#235: redirect the Ldr* routines, incl LdrGetProcedureAddress.  For
 * GetModuleHandle: why does kernel32 seem to do a lot of work?
 * BasepGetModuleHandleExW => RtlPcToFileHeader, RtlComputePrivatizedDllName_U
 * where should we intercept?  why isn't it calling LdrGetDllHandle{,Ex}?
 */
static const redirect_import_t redirect_ntdll[] = {
    /* kernel32 passes some of its routines to ntdll where they are
     * stored in function pointers.  xref PR 215408 where on x64 we had
     * issues w/ these not showing up b/c no longer in relocs.
     * kernel32!_BaseDllInitialize calls certain ntdll routines to
     * set up these callbacks:
     */
    /* LdrSetDllManifestProber has more args on win7: see redirect_ntdll_win7 */
    {"LdrSetDllManifestProber",           (app_pc)redirect_ignore_arg4},
    {"RtlSetThreadPoolStartFunc",         (app_pc)redirect_ignore_arg8},
    {"RtlSetUnhandledExceptionFilter",    (app_pc)redirect_ignore_arg4},
    /* Rtl*Heap routines:
     * The plan is to allow other Heaps to be created, and only redirect use of
     * PEB.ProcessHeap.  For now we'll leave the query, walk, enum, etc.  of
     * PEB.ProcessHeap pointing at the app's and focus on allocation.
     * There are many corner cases where we won't be transparent but we'll
     * incrementally add more redirection (i#235) and more transparency: have to
     * start somehwere.  Our biggest problems are ntdll routines that internally
     * allocate or free combined with the other of the pair from outside.
     */
    {"RtlAllocateHeap",                (app_pc)redirect_RtlAllocateHeap},
    {"RtlReAllocateHeap",              (app_pc)redirect_RtlReAllocateHeap},
    {"RtlFreeHeap",                    (app_pc)redirect_RtlFreeHeap},
    {"RtlSizeHeap",                    (app_pc)redirect_RtlSizeHeap},
    /* kernel32!LocalFree calls these */
    {"RtlLockHeap",                    (app_pc)redirect_RtlLockHeap},
    {"RtlUnlockHeap",                  (app_pc)redirect_RtlUnlockHeap},
    /* We don't redirect the creation but we avoid DR pointers being passed
     * to RtlFreeHeap and subsequent heap corruption by redirecting the frees,
     * since sometimes creation is by direct RtlAllocateHeap.
     */
    {"RtlFreeUnicodeString",           (app_pc)redirect_RtlFreeUnicodeString},
    {"RtlFreeAnsiString",              (app_pc)redirect_RtlFreeAnsiString},
    {"RtlFreeOemString",               (app_pc)redirect_RtlFreeOemString},
#if 0 /* FIXME i#235: redirect these: */
    {"RtlSetUserValueHeap",            (app_pc)redirect_RtlSetUserValueHeap},
    {"RtlGetUserInfoHeap",             (app_pc)redirect_RtlGetUserInfoHeap},
#endif
};
#define REDIRECT_NTDLL_NUM (sizeof(redirect_ntdll)/sizeof(redirect_ntdll[0]))

/* For ntdll redirections that differ on Windows 7.  Takes precedence over
 * redirect_ntdll[].
 */
static const redirect_import_t redirect_ntdll_win7[] = {
    /* win7 increases the #args */
    {"LdrSetDllManifestProber",           (app_pc)redirect_ignore_arg12},
};
#define REDIRECT_NTDLL_WIN7_NUM \
    (sizeof(redirect_ntdll_win7)/sizeof(redirect_ntdll_win7[0]))

static const redirect_import_t redirect_kernel32[] = {
    /* To avoid the FlsCallback being interpreted */
    {"FlsAlloc",                       (app_pc)redirect_FlsAlloc},
    /* As an initial interception of loader queries, but simpler than
     * intercepting Ldr*: plus, needed to intercept FlsAlloc called by msvcrt
     * init routine.  Of course we're missing GetModuleHandle{W,ExA,ExW}
     * and LoadLibraryW.
     */
    {"GetModuleHandleA",               (app_pc)redirect_GetModuleHandleA},
    {"GetProcAddress",                 (app_pc)redirect_GetProcAddress},
    {"LoadLibraryA",                   (app_pc)redirect_LoadLibraryA},
};
#define REDIRECT_KERNEL32_NUM (sizeof(redirect_kernel32)/sizeof(redirect_kernel32[0]))

/* Support for running private FlsCallback routines natively */
typedef struct _fls_cb_t {
    PFLS_CALLBACK_FUNCTION cb;
    struct _fls_cb_t *next;
} fls_cb_t;

static fls_cb_t *fls_cb_list; /* in .data, so we have a permanent head node */
DECLARE_CXTSWPROT_VAR(static mutex_t privload_fls_lock,
                      INIT_LOCK_FREE(privload_fls_lock));

/* Rather than statically linking to real kernel32 we want to invoke
 * routines in the private kernel32
 */
static DWORD (WINAPI *priv_kernel32_FlsAlloc)(PFLS_CALLBACK_FUNCTION);
static HMODULE (WINAPI *priv_kernel32_GetModuleHandleA)(const char *);
static FARPROC (WINAPI *priv_kernel32_GetProcAddress)(HMODULE, const char *);
static HMODULE (WINAPI *priv_kernel32_LoadLibraryA)(const char *);

#ifdef CLIENT_INTERFACE
/* Isolate the app's PEB by making a copy for use by private libs (i#249) */
static PEB *private_peb;
/* Isolate TEB->FlsData: for first thread we need to copy before have dcontext */
static void *priv_fls_data;
/* Isolate TEB->ReservedForNtRpc: for first thread we need to copy before have dcontext */
static void *priv_nt_rpc;
/* Only swap peb and teb fields if we've loaded WinAPI libraries */
static bool loaded_windows_lib;
#endif

/***************************************************************************/

void 
os_loader_init_prologue(void)
{
    app_pc ntdll = get_ntdll_base();
    app_pc drdll = get_dynamorio_dll_start();
    app_pc user32 = (app_pc) get_module_handle(L"user32.dll");
    privmod_t *mod;

#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_peb)) {
        /* Isolate the app's PEB by making a copy for use by private libs (i#249).
         * We just do a shallow copy for now until we hit an issue w/ deeper fields
         * that are allocated at our init time.
         * Anything allocated by libraries after our init here will of
         * course get its own private deep copy.
         * We also do not intercept private libs calling NtQueryInformationProcess
         * to get info.PebBaseAddress: we assume they don't do that.  It's not
         * exposed in any WinAPI routine.
         */
        GET_NTDLL(RtlInitializeCriticalSection, (OUT RTL_CRITICAL_SECTION *crit));
        PEB *own_peb = get_own_peb();
        /* FIXME: does it need to be page-aligned? */
        private_peb = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, PEB, ACCT_OTHER, UNPROTECTED);
        memcpy(private_peb, own_peb, sizeof(*private_peb));
        /* We need priv libs to NOT use any locks that app code uses: else we'll
         * deadlock (classic transparency violation).
         * One concern here is that the real PEB points at ntdll!FastPebLock
         * but we assume nobody cares.
         */
        private_peb->FastPebLock = HEAP_TYPE_ALLOC
            (GLOBAL_DCONTEXT, RTL_CRITICAL_SECTION, ACCT_OTHER, UNPROTECTED);
        RtlInitializeCriticalSection(private_peb->FastPebLock);
        
        /* Start with empty values, regardless of what app libs did prior to us
         * taking over.  FIXME: if we ever have attach will have to verify this:
         * can priv libs always live in their own universe that starts empty?
         */
        private_peb->FlsListHead.Flink = (LIST_ENTRY *) &private_peb->FlsListHead;
        private_peb->FlsListHead.Blink = (LIST_ENTRY *) &private_peb->FlsListHead;
        private_peb->FlsCallback = NULL;
        swap_peb_pointer(NULL, true/*to priv*/);
        LOG(GLOBAL, LOG_LOADER, 2, "app peb="PFX"\n", own_peb);
        LOG(GLOBAL, LOG_LOADER, 2, "private peb="PFX"\n", private_peb);
        priv_fls_data = get_tls(FLS_DATA_TIB_OFFSET);
        priv_nt_rpc = get_tls(NT_RPC_TIB_OFFSET);
        LOG(GLOBAL, LOG_LOADER, 2, "initial thread TEB->FlsData="PFX"\n", priv_fls_data);
        LOG(GLOBAL, LOG_LOADER, 2, "initial thread TEB->ReservedForNtRpc="PFX"\n",
            priv_nt_rpc);
    }
#endif

    /* use permanent head node to avoid .data unprot */
    ASSERT(fls_cb_list == NULL);
    fls_cb_list = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fls_cb_t, ACCT_OTHER, PROTECTED);
    fls_cb_list->cb = NULL;
    fls_cb_list->next = NULL;

    acquire_recursive_lock(&privload_lock);
    privload_init_search_paths();
    /* We count on having at least one node that's never removed so we
     * don't have to unprot .data and write to modlist later
     */
    mod = privload_insert(NULL, ntdll, get_allocation_size(ntdll, NULL),
                          "ntdll.dll");
    mod->externally_loaded = true;
    /* Once we have earliest injection and load DR via this private loader
     * (i#234/PR 204587) we can remove this
     */
    mod = privload_insert(NULL, drdll, get_allocation_size(drdll, NULL),
                          DYNAMORIO_LIBRARY_NAME);
    mod->externally_loaded = true;

    /* FIXME i#235: loading a private user32.dll is problematic: it registers
     * callbacks that KiUserCallbackDispatcher invokes.  For now we do not
     * duplicate it.  If the app loads it dynamically later we will end up
     * duplicating but not worth checking for that.
     */
    if (user32 != NULL) {
        mod = privload_insert(NULL, user32, get_allocation_size(user32, NULL),
                              "user32.dll");
        mod->externally_loaded = true;
    }
}

void
os_loader_init_epilogue(void)
{
#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_peb) && !should_swap_peb_pointer()) {
        /* Not going to be swapping so restore permanently to app */
        swap_peb_pointer(NULL, false/*to app*/);
    }
#endif
    release_recursive_lock(&privload_lock);
}

void
os_loader_exit(void)
{
    mutex_lock(&privload_fls_lock);
    while (fls_cb_list != NULL) {
        fls_cb_t *cb = fls_cb_list;
        fls_cb_list = fls_cb_list->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, cb, fls_cb_t, ACCT_OTHER, PROTECTED);
    }
    mutex_unlock(&privload_fls_lock);
    DELETE_LOCK(privload_fls_lock);

#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_peb)) {
        if (should_swap_peb_pointer()) {
            /* Swap back so any further peb queries (e.g., reading env var
             * while reporting a leak) use a non-freed peb
             */
            swap_peb_pointer(NULL, false/*to app*/);
        }
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, private_peb->FastPebLock,
                       RTL_CRITICAL_SECTION, ACCT_OTHER, UNPROTECTED);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, private_peb, PEB, ACCT_OTHER, UNPROTECTED);
    }
#endif
}

void
os_loader_thread_init_prologue(dcontext_t *dcontext)
{
#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        if (!dynamo_initialized) {
            /* For first thread use cached pre-priv-lib value for app and
             * whatever value priv libs have set for priv
             */
            dcontext->priv_fls_data = get_tls(FLS_DATA_TIB_OFFSET);
            dcontext->priv_nt_rpc = get_tls(NT_RPC_TIB_OFFSET);
            dcontext->app_fls_data = NULL;
            dcontext->app_nt_rpc = NULL;
            set_tls(FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
            set_tls(NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
        } else {
            /* The real value will be set by swap_peb_pointer */
            dcontext->app_fls_data = NULL;
            dcontext->app_nt_rpc = NULL;
            /* We assume clearing out any non-NULL value for priv is safe */
            dcontext->priv_fls_data = NULL;
            dcontext->priv_nt_rpc = NULL;
        }
        LOG(THREAD, LOG_LOADER, 2, "app fls="PFX", priv fls="PFX"\n",
            dcontext->app_fls_data, dcontext->priv_fls_data);
        LOG(THREAD, LOG_LOADER, 2, "app rpc="PFX", priv rpc="PFX"\n",
            dcontext->app_nt_rpc, dcontext->priv_nt_rpc);
        /* For swapping teb fields (detach, reset i#25) we'll need to
         * know the teb base from another thread 
         */
        dcontext->teb_base = (byte *) get_tls(SELF_TIB_OFFSET);
        swap_peb_pointer(dcontext, true/*to priv*/);
    }
#endif
}

void
os_loader_thread_init_epilogue(dcontext_t *dcontext)
{
#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        /* For subsequent app threads, peb ptr will be swapped to priv
         * by transfer_to_dispatch(), and w/ FlsData swap we have to
         * properly nest.
         */
        if (dynamo_initialized/*later thread*/ && !IS_CLIENT_THREAD(dcontext))
            swap_peb_pointer(dcontext, false/*to app*/);
    }
#endif
}

void
os_loader_thread_exit(dcontext_t *dcontext)
{
    /* do nothing in Windows */
}

#ifdef CLIENT_INTERFACE
/* our copy of the PEB for isolation (i#249) */
PEB *
get_private_peb(void)
{
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(private_peb != NULL);
    return private_peb;
}

/* For performance reasons we avoid the swap if there are no private WinAPI libs:
 * we assume libs not in the system dir will not write to PEB or TEB fields we
 * care about (mainly Fls ones).
 */
bool
should_swap_peb_pointer(void)
{
    return INTERNAL_OPTION(private_peb) && loaded_windows_lib;
}

static void
set_loaded_windows_lib(void)
{
    if (!loaded_windows_lib) {
        if (!dynamo_initialized) {
            loaded_windows_lib = true;
            LOG(GLOBAL, LOG_LOADER, 1,
                "loaded a Windows system library => isolating PEB+TEB\n");
            /* attempt to catch init re-ordering (see comment below and i#338) */
            ASSERT(get_thread_private_dcontext() == NULL);
        } else {
            /* We've already emitted context switch code that does not swap peb/teb.
             * Basically we don't support this.  (Should really check for post-emit.)
             */
            ASSERT_NOT_REACHED();
        }
    }
}

static void *
get_teb_field(dcontext_t *dcontext, ushort offs)
{
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT) {
        /* get our own */
        return get_tls(offs);
    } else {
        byte *teb = dcontext->teb_base;
        return *((void **)(teb + offs));
    }
}

static void
set_teb_field(dcontext_t *dcontext, ushort offs, void *value)
{
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT) {
        /* set our own */
        set_tls(offs, value);
    } else {
        byte *teb = dcontext->teb_base;
        *((void **)(teb + offs)) = value;
    }
}

/* C version of preinsert_swap_peb() */
void
swap_peb_pointer(dcontext_t *dcontext, bool to_priv)
{
    PEB *tgt_peb = to_priv ? get_private_peb() : get_own_peb();
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(!dynamo_initialized || should_swap_peb_pointer());
    ASSERT(tgt_peb != NULL);
    set_teb_field(dcontext, PEB_TIB_OFFSET, (void *) tgt_peb);
    LOG(GLOBAL, LOG_LOADER, 2, "set teb->peb to "PFX"\n", tgt_peb);
    if (dcontext != NULL && dcontext != GLOBAL_DCONTEXT) {
        /* We also swap TEB->FlsData and TEB->ReservedForNtRpc */
        void *cur_fls = get_teb_field(dcontext, FLS_DATA_TIB_OFFSET);
        void *cur_rpc = get_teb_field(dcontext, NT_RPC_TIB_OFFSET);
        if (to_priv) {
            if (dcontext->priv_fls_data != cur_fls) { /* handle two calls in a row */
                dcontext->app_fls_data = cur_fls;
                set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->priv_fls_data);
            }
            if (dcontext->priv_nt_rpc != cur_rpc) { /* handle two calls in a row */
                dcontext->app_nt_rpc = cur_rpc;
                set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->priv_nt_rpc);
            }
        } else {
            if (dcontext->app_fls_data != cur_fls) { /* handle two calls in a row */
                dcontext->priv_fls_data = cur_fls;
                set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
            }
            if (dcontext->app_nt_rpc != cur_rpc) { /* handle two calls in a row */
                dcontext->priv_nt_rpc = cur_rpc;
                set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
            }
        }
        ASSERT(!is_dynamo_address(dcontext->app_fls_data));
        ASSERT(!is_dynamo_address(dcontext->app_nt_rpc));
        /* Once we have earier injection we should be able to assert
         * that priv_fls_data is either NULL or a DR address: but on
         * notepad w/ drinject it's neither: need to investigate.
         */
        LOG(THREAD, LOG_LOADER, 3, "app fls="PFX", priv fls="PFX"\n",
            dcontext->app_fls_data, dcontext->priv_fls_data);
        LOG(THREAD, LOG_LOADER, 3, "app rpc="PFX", priv rpc="PFX"\n",
            dcontext->app_nt_rpc, dcontext->priv_nt_rpc);
    }
}

/* Meant for use on detach only: restore app values and does not update
 * or swap private values.  Up to caller to synchronize w/ other thread.
 */
void
restore_peb_pointer_for_thread(dcontext_t *dcontext)
{
    PEB *tgt_peb = get_own_peb();
    ASSERT_NOT_TESTED();
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(!dynamo_initialized || should_swap_peb_pointer());
    ASSERT(tgt_peb != NULL);
    ASSERT(dcontext != NULL && dcontext->teb_base != NULL);
    set_teb_field(dcontext, PEB_TIB_OFFSET, (void *) tgt_peb);
    LOG(GLOBAL, LOG_LOADER, 2, "set teb->peb to "PFX"\n", tgt_peb);
    /* We also swap TEB->FlsData and TEB->ReservedForNtRpc */
    set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
    LOG(THREAD, LOG_LOADER, 3, "restored app fls to "PFX"\n", dcontext->app_fls_data);
    set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
    LOG(THREAD, LOG_LOADER, 3, "restored app fls to "PFX"\n", dcontext->app_nt_rpc);
}
#endif /* CLIENT_INTERFACE */

void
privload_add_areas(privmod_t *privmod)
{
    vmvector_add(modlist_areas, privmod->base, privmod->base + privmod->size,
                 (void *)privmod);
}

void
privload_remove_areas(privmod_t *privmod)
{
    vmvector_remove(modlist_areas, privmod->base, privmod->base + privmod->size);
}

void 
privload_unmap_file(privmod_t *mod)
{
    unmap_file(mod->base, mod->size);
}

bool
privload_unload_imports(privmod_t *mod)
{
    privmod_t *impmod;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    app_pc imports_end;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    if (!privload_get_import_descriptor(mod, &imports, &imports_end)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: error reading imports for %s\n",
            __FUNCTION__, mod->name);
        return false;
    }
    if (imports == NULL) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has no imports\n", __FUNCTION__, mod->name);
        return true;
    }

    while (imports->OriginalFirstThunk != 0) {
        const char *impname = (const char *) RVA_TO_VA(mod->base, imports->Name);
        impname = privload_map_name(impname, mod);
        impmod = privload_lookup(impname);
        /* If we hit an error in the middle of loading we may not have loaded
         * all imports for mod so impmod may not be found
         */
        if (impmod != NULL)
            privload_unload(impmod);
        imports++;
        ASSERT((app_pc)(imports+1) <= imports_end);
    }
    /* I used to ASSERT((app_pc)(imports+1) == imports_end) but kernel32 on win2k
     * has an extra 10 bytes in the dir->Size for unknown reasons so suppressing
     */
    return true;
}

/* if anything fails, undoes the mapping and returns NULL */
app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT)
{
    file_t fd;
    app_pc map;
    app_pc pref;
    byte *(*map_func)(file_t, size_t *, uint64, app_pc, uint, bool, bool, bool);
    bool (*unmap_func)(file_t, size_t);
    ASSERT(size != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    /* On win32 OS_EXECUTE is required to create a section w/ rwx
     * permissions, which is in turn required to map a view w/ rwx
     */
    fd = os_open(filename, OS_OPEN_READ | OS_EXECUTE |
                 /* we should allow renaming (xref PR 214399) as well as
                  * simultaneous read while holding the file handle
                  */
                 OS_SHARE_DELETE /* shared read is on by default */);
    if (fd == INVALID_FILE) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to open %s\n", __FUNCTION__, filename);
        return NULL;
    }

    /* The libs added prior to dynamo_heap_initialized are only client
     * libs, which we do not want on the DR-areas list to allow them
     * to have app execute from their .text.  We do want other
     * privately-loaded libs to be on the DR-areas list (though that
     * means that if we mess up and the app executes their code, we throw
     * an app exception: FIXME: should we raise a better error message?     
     */
    *size = 0; /* map at full size */
    if (dynamo_heap_initialized) {
        /* These hold the DR lock and update DR areas */
        map_func = map_file;
        unmap_func = unmap_file;
    } else {
        map_func = os_map_file;
        unmap_func = os_unmap_file;
    }
    /* On Windows, SEC_IMAGE => the kernel sets up the different segments w/
     * proper protections for us, all on this single map syscall
     */
    /* If libs should be in lower 2GB or 4GB, they should have a preferred base
     * there: here we simply pass NULL and let the kernel decide.
     */
    map = (*map_func)(fd, size, 0/*offs*/, NULL/*base*/,
                      /* Ask for max, then restrict pieces */
                      MEMPROT_READ|MEMPROT_WRITE|MEMPROT_EXEC,
                      /* case 9599: asking for COW commits pagefile space
                       * up front, so we map two separate views later: see below
                       */
                      true/*writes should not change file*/, true/*image*/,
                      false/*!fixed*/);
    os_close(fd); /* no longer needed */
    fd = INVALID_FILE;

    pref = get_module_preferred_base(map);
    if (pref != map) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: relocating from "PFX" to "PFX"\n",
            __FUNCTION__, pref, map);
        if (!module_file_relocatable(map)) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: module not relocatable\n", __FUNCTION__);
            (*unmap_func)(map, *size);
            return NULL;
        }
        if (!module_rebase(map, *size, (map - pref), true/*+w incremental*/)) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to relocate %s\n",
                __FUNCTION__, filename);
            (*unmap_func)(map, *size);
            return NULL;
        }
    }

    return map;
}

bool
privload_process_imports(privmod_t *mod)
{
    privmod_t *impmod;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    app_pc iat, imports_end;
    uint orig_prot;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    if (!privload_get_import_descriptor(mod, &imports, &imports_end)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: error reading imports for %s\n",
            __FUNCTION__, mod->name);
        return false;
    }
    if (imports == NULL) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has no imports\n", __FUNCTION__, mod->name);
        return true;
    }

    /* If we later have other uses, turn this into a general import iterator
     * in module.c.  For now we're the only use so not worth the effort.
     */
    while (imports->OriginalFirstThunk != 0) {
        IMAGE_THUNK_DATA *lookup;
        IMAGE_THUNK_DATA *address;
        const char *impname = (const char *) RVA_TO_VA(mod->base, imports->Name);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s imports from %s\n", __FUNCTION__,
            mod->name, impname);
        impname = privload_map_name(impname, mod);

        /* FIXME i#233: support bound imports: for now ignoring */
        if (imports->TimeDateStamp == -1) {
            /* Imports are bound via "new bind": need to walk
             * IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT =>
             * IMAGE_BOUND_IMPORT_DESCRIPTOR
             */
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has new bind imports\n",
                __FUNCTION__, mod->name);
        } else if (imports->TimeDateStamp != 0) {
            /* Imports are bound via "old bind" */
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has old bind imports\n",
                __FUNCTION__, mod->name);
        }

        impmod = privload_lookup(impname);
        if (impmod == NULL) {
            impmod = privload_locate_and_load(impname, mod);
            if (impmod == NULL) {
                LOG(GLOBAL, LOG_LOADER, 1, "%s: unable to load import lib %s\n",
                    __FUNCTION__, impname);
                return false;
            }
        } else
            impmod->ref_count++;

        /* walk the lookup table and address table in lockstep */
        /* FIXME: should check readability: if had no-dcontext try (i#350) could just
         * do try/except around whole thing
         */
        lookup = (IMAGE_THUNK_DATA *) RVA_TO_VA(mod->base, imports->OriginalFirstThunk);
        address = (IMAGE_THUNK_DATA *) RVA_TO_VA(mod->base, imports->FirstThunk);
        iat = (app_pc) address;
        if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                    PAGE_READWRITE, &orig_prot))
            return false;
        while (lookup->u1.Function != 0) {
            if (!privload_process_one_import(mod, impmod, lookup, (app_pc *)address)) {
                LOG(GLOBAL, LOG_LOADER, 1, "%s: error processing imports\n",
                    __FUNCTION__);
                return false;
            }
            lookup++;
            address++;
            if (PAGE_START(address) != PAGE_START(iat)) {
                if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                            orig_prot, &orig_prot))
                    return false;
                iat = (app_pc) address;
                if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                            PAGE_READWRITE, &orig_prot))
                    return false;
            }
        }
        if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                    orig_prot, &orig_prot))
            return false;

        imports++;
        ASSERT((app_pc)(imports+1) <= imports_end);
    }
    /* I used to ASSERT((app_pc)(imports+1) == imports_end) but kernel32 on win2k
     * has an extra 10 bytes in the dir->Size for unknown reasons so suppressing
     */

    /* FIXME i#233: support delay-load: IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT */

    return true;
}

static bool
privload_get_import_descriptor(privmod_t *mod, IMAGE_IMPORT_DESCRIPTOR **imports OUT,
                               app_pc *imports_end OUT)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *) mod->base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *) (mod->base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir;
    ASSERT(is_readable_pe_base(mod->base));
    ASSERT(dos->e_magic == IMAGE_DOS_SIGNATURE);
    ASSERT(nt != NULL && nt->Signature == IMAGE_NT_SIGNATURE);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    ASSERT(imports != NULL);

    dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (dir == NULL || dir->Size <= 0) {
        *imports = NULL;
        return true;
    }
    *imports = (IMAGE_IMPORT_DESCRIPTOR *) RVA_TO_VA(mod->base, dir->VirtualAddress);
    ASSERT_CURIOSITY(dir->Size >= sizeof(IMAGE_IMPORT_DESCRIPTOR));
    if (!is_readable_without_exception((app_pc)*imports, dir->Size)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has unreadable imports: partial map?\n",
            __FUNCTION__, mod->name);
        return false;
    }
    if (imports_end != NULL)
        *imports_end = mod->base + dir->VirtualAddress + dir->Size;
    return true;
}

static bool
privload_process_one_import(privmod_t *mod, privmod_t *impmod,
                            IMAGE_THUNK_DATA *lookup, app_pc *address)
{
    app_pc dst = NULL;
    const char *forwarder;
    generic_func_t func;
    /* Set to first-level names for use below in case no forwarder */
    privmod_t *forwmod = impmod;
    privmod_t *last_forwmod = NULL;
    const char *forwfunc = NULL;
    const char *impfunc = NULL;
    const char *forwpath = NULL;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (TEST(IMAGE_ORDINAL_FLAG, lookup->u1.Function)) {
        /* XXX: for 64-bit this is a 64-bit type: should we widen through
         * get_proc_address_by_ordinal()?
         */
        DWORD ord = (DWORD) (lookup->u1.AddressOfData & ~(IMAGE_ORDINAL_FLAG));
        func = get_proc_address_by_ordinal(impmod->base, ord, &forwarder);
        impfunc = "<ordinal>";
    } else {
        /* import by name */
        IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)
            RVA_TO_VA(mod->base, (lookup->u1.AddressOfData & ~(IMAGE_ORDINAL_FLAG)));
        /* FIXME optimization i#233:
         * - try name->Hint first 
         * - build hashtables for quick lookup instead of repeatedly walking
         *   export tables
         */
        /* expensive to check is_readable for name: really we want no-dcxt try (i#350) */
        func = get_proc_address_ex(impmod->base, (const char *) name->Name, &forwarder);
        /* set to first-level names for use below in case no forwarder */
        forwfunc = (const char *) name->Name;
        impfunc = forwfunc;
    }
    /* loop to handle sequence of forwarders */
    while (func == NULL) {
        if (forwarder == NULL) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: import %s not found in %s\n",
                __FUNCTION__, impfunc, impmod->name);
            return false;
        }
        forwfunc = strchr(forwarder, '.') + 1;
        if (forwfunc - forwarder + strlen("dll") >=
            BUFFER_SIZE_ELEMENTS(forwmodpath)) {
            ASSERT_NOT_REACHED();
            LOG(GLOBAL, LOG_LOADER, 1, "%s: import string %s too long\n",
                __FUNCTION__, forwarder);
            return false;
        }
        /* we use static buffer: may be clobbered by recursion below */
        snprintf(forwmodpath, forwfunc - forwarder, "%s", forwarder);
        snprintf(forwmodpath + (forwfunc - forwarder), strlen("dll"), "dll");
        forwmodpath[forwfunc - 1/*'.'*/ - forwarder + strlen(".dll")] = '\0';
        LOG(GLOBAL, LOG_LOADER, 2, "\tforwarder %s => %s %s\n",
            forwarder, forwmodpath, forwfunc);
        last_forwmod = forwmod;
        forwpath = privload_map_name(forwmodpath,
                                     last_forwmod == NULL ? mod : last_forwmod);
        forwmod = privload_lookup(forwpath);
        if (forwmod == NULL) {
            /* don't use forwmodpath past here: recursion may clobber it */
            /* XXX: should inc ref count: but then need to walk individual imports
             * and dec on unload.  For now risking it.
             */
            forwmod = privload_locate_and_load(forwpath, mod);
            if (forwmod == NULL) {
                LOG(GLOBAL, LOG_LOADER, 1, "%s: unable to load forworder for %s\n"
                    __FUNCTION__, forwarder);
                return false;
            }
        }
        /* should be listed as import; don't want to inc ref count on each forw */
        func = get_proc_address_ex(forwmod->base, forwfunc, &forwarder);
    }
    /* write result into IAT */
    LOG(GLOBAL, LOG_LOADER, 2, "\timport %s @ "PFX" => IAT "PFX"\n",
        impfunc, func, address);
    if (forwfunc != NULL) {
        /* XXX i#233: support redirecting when imported by ordinal */
        dst = privload_redirect_imports(forwmod, forwfunc);
    }
    if (dst == NULL)
        dst = (app_pc) func;
    *address = dst;
    return true;
}

bool
privload_call_entry(privmod_t *privmod, uint reason)
{
    app_pc entry = get_module_entry(privmod->base);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* get_module_entry adds base => returns base instead of NULL */
    if (entry != NULL && entry != privmod->base) {
        dllmain_t func = (dllmain_t) convert_data_to_function(entry);
        bool res;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: calling %s entry "PFX" for %d\n",
            __FUNCTION__, privmod->name, entry, reason);
        res = (*func)((HANDLE)privmod->base, reason, NULL);
        if (!res && get_os_version() >= WINDOWS_VERSION_7 &&
            str_case_prefix(privmod->name, "kernel32")) {
            /* i#364: win7 _BaseDllInitialize fails to initialize a new console
             * (0xc0000041 (3221225537) - The NtConnectPort request is refused)
             * which we ignore for now.  DR always had trouble writing to the
             * console anyway (xref i#261).
             */
            LOG(GLOBAL, LOG_LOADER, 2,
                "%s: ignoring failure of kernel32!_BaseDllInitialize\n", __FUNCTION__);
            res = true;    
        }
        return res;
    }
    return true;
}

/* Map API-set pseudo-dlls to real dlls.
 * In Windows 7, dlls now import from pseudo-dlls that split up the
 * API.  They are all named
 * "API-MS-Win-<category>-<component>-L<layer>-<version>.dll".
 * There is no such file: instead the loader uses a table in the special
 * apisetschema.dll that is mapped into every process to map
 * from the particular pseudo-dll to a real dll.
 */
static const char *
map_api_set_dll(const char *name, privmod_t *dependent)
{
    /* Ideally we would read apisetschema.dll ourselves.
     * It seems to be mapped in at 0x00040000.
     * But this is simpler than trying to parse that dll's table.
     * We ignore the version suffix ("-1-0", e.g.).
     */
    if (str_case_prefix(name, "API-MS-Win-Core-Console-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-DateTime-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-DelayLoad-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Debug-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-ErrorHandling-L1")) {
        /* This one includes {,Set}UnhandledExceptionFilter which are only in
         * kernel32, but kernel32 itself imports GetLastError, etc.  which must come
         * from kernelbase to avoid infinite loop.  XXX: what does apisetschema say?
         * dependent on what's imported?
         */
        if (str_case_prefix(dependent->name, "kernel32"))
            return "kernelbase.dll";
        else
            return "kernel32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-Fibers-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-File-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Handle-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Heap-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Interlocked-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-IO-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Localization-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-LocalRegistry-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-LibraryLoader-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Memory-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Misc-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-NamedPipe-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-ProcessEnvironment-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-ProcessThreads-L1")) {
        /* This one includes CreateProcessAsUserW which is only in
         * kernel32, but kernel32 itself imports from here and its must come
         * from kernelbase to avoid infinite loop.  XXX: see above: seeming
         * more and more like it depends on what's imported.
         */
        if (str_case_prefix(dependent->name, "kernel32"))
            return "kernelbase.dll";
        else
            return "kernel32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-Profile-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-RTLSupport-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-String-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Synch-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-SysInfo-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-ThreadPool-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-XState-L1"))
        return "ntdll.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Util-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Security-Base-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Security-LSALookup-L1"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Security-SDDL-L1"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Service-Core-L1"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Service-Management-L1"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Service-Management-L2"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Service-Winsvc-L1"))
        return "sechost.dll";
    else {
        SYSLOG_INTERNAL_WARNING("unknown API-MS-Win pseudo-dll %s", name);
        /* good guess */
        return "kernelbase.dll";
    }
}

/* If walking forwarder chain, immed_dep should be most recent walked.
 * Else should be regular dependent.
 */
static const char *
privload_map_name(const char *impname, privmod_t *immed_dep)
{
    /* 0) on Windows 7, the API-set pseudo-dlls map to real dlls */
    if (get_os_version() >= WINDOWS_VERSION_7 &&
        str_case_prefix(impname, "API-MS-Win-")) {
        IF_DEBUG(const char *apiname = impname;)
        /* We need immediate dependent to avoid infinite chain when hit
         * kernel32 OpenProcessToken forwarder which needs to forward
         * to kernelbase
         */
        impname = map_api_set_dll(impname, immed_dep);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: mapped API-set dll %s to %s\n",
            __FUNCTION__, apiname, impname);
        return impname;
    }
    return impname;
}

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent)
{
    privmod_t *mod = NULL;
    uint i;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* The ntdll!Ldr loader searches in this order:
     *   1) exe dir
     *   2) cur dir
     *   3) system dir
     *   4) windows dir
     *   5) dirs on PATH
     * We modify "exe dir" to be "client lib dir".
     * we do not support cur dir.
     * we additionally support loading from the Extensions dir
     * (i#277/PR 540817, added to search_paths in privload_init_search_paths()).
     */

    /* 1) client lib dir(s) and Extensions dir */
    for (i = 0; i < search_paths_idx; i++) {
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/%s",
                 search_paths[i], impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false/*!is_dir*/)) {
            mod = privload_load(modpath, dependent);
            /* if fails to load, don't keep searching: that seems the most
             * reasonable semantics.  we could keep searching: then should
             * relax the privload_recurse_cnt curiosity b/c won't be reset
             * in between if many copies of same lib fail to load.
             */
            return mod;
        }
    }
    /* 2) cur dir: we do not support */
    if (systemroot[0] != '\0') {
        /* 3) system dir */
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/system32/%s",
                 systemroot, impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false/*!is_dir*/)) {
#ifdef CLIENT_INTERFACE
            set_loaded_windows_lib();
#endif
            mod = privload_load(modpath, dependent);
            return mod; /* if fails to load, don't keep searching */
        }
        /* 4) windows dir */
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/%s",
                 systemroot, impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false/*!is_dir*/)) {
#ifdef CLIENT_INTERFACE
            set_loaded_windows_lib();
#endif
            mod = privload_load(modpath, dependent);
            return mod; /* if fails to load, don't keep searching */
        }
    }
    /* 5) dirs on PATH: FIXME: not supported yet */
    return mod;
}

/* Although privload_init_paths will be called in both Linux and Windows,
 * it is only called from os_loader_init_prologue, so it is ok to keep it
 * local. Instead, we extract the shared code of adding ext path into
 * privload_add_drext_path().
 */
static void
privload_init_search_paths(void)
{
    reg_query_value_result_t value_result;
    DIAGNOSTICS_KEY_VALUE_FULL_INFORMATION diagnostic_value_info;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    privload_add_drext_path();
    /* Get SystemRoot from CurrentVersion reg key */
    value_result = reg_query_value(DIAGNOSTICS_OS_REG_KEY, 
                                   DIAGNOSTICS_SYSTEMROOT_REG_KEY,
                                   KeyValueFullInformation,
                                   &diagnostic_value_info, 
                                   sizeof(diagnostic_value_info), 0);    
    if (value_result == REG_QUERY_SUCCESS) {
        snprintf(systemroot, BUFFER_SIZE_ELEMENTS(systemroot), "%S",
                 (wchar_t*)(diagnostic_value_info.NameAndData  + 
                            diagnostic_value_info.DataOffset - 
                            DECREMENT_FOR_DATA_OFFSET));
        NULL_TERMINATE_BUFFER(systemroot);
    } else
        ASSERT_NOT_REACHED();
}

/* Rather than statically linking to real kernel32 we want to invoke
 * routines in the private kernel32
 */
void
privload_redirect_setup(privmod_t *mod)
{
    if (strcasecmp(mod->name, "kernel32.dll") == 0) {
        if (!dynamo_initialized)
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        priv_kernel32_FlsAlloc = (DWORD (WINAPI *)(PFLS_CALLBACK_FUNCTION))
            get_proc_address_ex(mod->base, "FlsAlloc", NULL);
        priv_kernel32_GetModuleHandleA = (HMODULE (WINAPI *)(const char *))
            get_proc_address_ex(mod->base, "GetModuleHandleA", NULL);
        priv_kernel32_GetProcAddress = (FARPROC (WINAPI *)(HMODULE, const char *))
            get_proc_address_ex(mod->base, "GetProcAddress", NULL);
        priv_kernel32_LoadLibraryA = (HMODULE (WINAPI *)(const char *))
            get_proc_address_ex(mod->base, "LoadLibraryA", NULL);
        if (!dynamo_initialized)
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
}

static app_pc
privload_redirect_imports(privmod_t *impmod, const char *name)
{
    uint i;
    if (strcasecmp(impmod->name, "ntdll.dll") == 0) {
        if (get_os_version() >= WINDOWS_VERSION_7) {
            for (i = 0; i < REDIRECT_NTDLL_WIN7_NUM; i++) {
                if (strcasecmp(name, redirect_ntdll_win7[i].name) == 0) {
                    return redirect_ntdll_win7[i].func;
                }
            }
        }
        for (i = 0; i < REDIRECT_NTDLL_NUM; i++) {
            if (strcasecmp(name, redirect_ntdll[i].name) == 0) {
                return redirect_ntdll[i].func;
            }
        }
    } else if (strcasecmp(impmod->name, "kernel32.dll") == 0) {
        for (i = 0; i < REDIRECT_KERNEL32_NUM; i++) {
            if (strcasecmp(name, redirect_kernel32[i].name) == 0) {
                return redirect_kernel32[i].func;
            }
        }
    }
    return NULL;
}

static bool WINAPI
redirect_ignore_arg4(void *arg1)
{
    return true;
}

static bool WINAPI
redirect_ignore_arg8(void *arg1, void *arg2)
{
    return true;
}

static bool WINAPI
redirect_ignore_arg12(void *arg1, void *arg2, void *arg3)
{
    return true;
}

/****************************************************************************
 * Rtl*Heap redirection
 *
 * We only redirect for PEB.ProcessHeap.  See comments at top of file
 * and i#235 for adding further redirection.
 */

void * WINAPI RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size);

static void * WINAPI
redirect_RtlAllocateHeap(HANDLE heap, ULONG flags, SIZE_T size)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
        heap == peb->ProcessHeap) {
        byte *mem;
        ASSERT(sizeof(size_t) >= HEAP_ALIGNMENT);
        size += sizeof(size_t);
        mem = global_heap_alloc(size HEAPACCT(ACCT_LIBDUP));
        if (mem == NULL) {
            /* FIXME: support HEAP_GENERATE_EXCEPTIONS (xref PR 406742) */
            ASSERT_NOT_REACHED();
            return NULL;
        }
        *((size_t *)mem) = size;
        if (TEST(HEAP_ZERO_MEMORY, flags))
            memset(mem + sizeof(size_t), 0, size - sizeof(size_t));
        LOG(GLOBAL, LOG_LOADER, 2, "%s "PFX" "PIFX"\n", __FUNCTION__,
            mem + sizeof(size_t), size);
        return (void *) (mem + sizeof(size_t));
    } else {
        void *res = RtlAllocateHeap(heap, flags, size);
        LOG(GLOBAL, LOG_LOADER, 2, "native %s "PFX" "PIFX"\n", __FUNCTION__,
            res, size);
        return res;
    }
}

void * WINAPI RtlReAllocateHeap(HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size);

static void * WINAPI
redirect_RtlReAllocateHeap(HANDLE heap, ULONG flags, byte *ptr, SIZE_T size)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
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
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
        heap == peb->ProcessHeap && (is_dynamo_address(ptr) || ptr == NULL)) {
        byte *buf = NULL;
        /* RtlReAllocateHeap does re-alloc 0-sized */
        LOG(GLOBAL, LOG_LOADER, 2, "%s "PFX" "PIFX"\n", __FUNCTION__, ptr, size);
        buf = redirect_RtlAllocateHeap(heap, flags, size);
        if (buf != NULL) {
            size_t old_size = *((size_t *)(ptr - sizeof(size_t)));
            size_t min_size = MIN(old_size, size);
            memcpy(buf, ptr, min_size);
        }
        redirect_RtlFreeHeap(heap, flags, ptr);
        return (void *) buf;
    } else {
        void *res = RtlReAllocateHeap(heap, flags, ptr, size);
        LOG(GLOBAL, LOG_LOADER, 2, "native %s "PFX" "PIFX"\n", __FUNCTION__,
            res, size);
        return res;
    }
}

bool WINAPI RtlFreeHeap(HANDLE heap, ULONG flags, PVOID ptr);

SIZE_T WINAPI RtlSizeHeap(HANDLE heap, ULONG flags, PVOID ptr);

bool WINAPI RtlLockHeap(HANDLE heap);

bool WINAPI RtlUnlockHeap(HANDLE heap);

static bool WINAPI
redirect_RtlFreeHeap(HANDLE heap, ULONG flags, byte *ptr)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
    if (heap == peb->ProcessHeap && is_dynamo_address(ptr)/*see above*/) {
        ASSERT(IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true));
        if (ptr != NULL) {
            LOG(GLOBAL, LOG_LOADER, 2, "%s "PFX"\n", __FUNCTION__, ptr);
            ptr -= sizeof(size_t);
            global_heap_free(ptr, *((size_t *)ptr) HEAPACCT(ACCT_LIBDUP));
            return true;
        } else
            return false;
    } else {
        LOG(GLOBAL, LOG_LOADER, 2, "native %s "PFX" "PIFX"\n", __FUNCTION__,
            ptr, (ptr == NULL ? 0 : RtlSizeHeap(heap, flags, ptr)));
        return RtlFreeHeap(heap, flags, ptr);
    }
}

static SIZE_T WINAPI
redirect_RtlSizeHeap(HANDLE heap, ULONG flags, byte *ptr)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
    if (heap == peb->ProcessHeap && is_dynamo_address(ptr)/*see above*/) {
        ASSERT(IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true));
        if (ptr != NULL)
            return *((size_t *)(ptr - sizeof(size_t)));
        else
            return 0;
    } else {
        return RtlSizeHeap(heap, flags, ptr);
    }
}

/* These are called by LocalFree, passing kernel32!BaseHeap == peb->ProcessHeap */
static bool WINAPI
redirect_RtlLockHeap(HANDLE heap)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
    /* If the main heap, we assume any subsequent alloc/free will be through
     * DR heap, so we nop this
     */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
        heap == peb->ProcessHeap) {
        /* nop */
        return true;
    } else {
        return RtlLockHeap(heap);
    }
}

static bool WINAPI
redirect_RtlUnlockHeap(HANDLE heap)
{
    PEB *peb = get_peb(NT_CURRENT_PROCESS);
    /* If the main heap, we assume any prior alloc/free was through
     * DR heap, so we nop this
     */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
        heap == peb->ProcessHeap) {
        /* nop */
        return true;
    } else {
        return RtlUnlockHeap(heap);
    }
}

void WINAPI RtlFreeUnicodeString(UNICODE_STRING *string);

static void WINAPI
redirect_RtlFreeUnicodeString(UNICODE_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeUnicodeString(string);
}

void WINAPI RtlFreeAnsiString(ANSI_STRING *string);

static void WINAPI
redirect_RtlFreeAnsiString(ANSI_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeAnsiString(string);
}

void WINAPI RtlFreeOemString(OEM_STRING *string);

static void WINAPI
redirect_RtlFreeOemString(OEM_STRING *string)
{
    if (is_dynamo_address((app_pc)string->Buffer)) {
        PEB *peb = get_peb(NT_CURRENT_PROCESS);
        redirect_RtlFreeHeap(peb->ProcessHeap, 0, (byte *)string->Buffer);
        memset(string, 0, sizeof(*string));
    } else
        RtlFreeOemString(string);
}

/* Handles a private-library FLS callback called from interpreted app code */
bool
private_lib_handle_cb(dcontext_t *dcontext, app_pc pc)
{
    fls_cb_t *e, *prev;
    bool redirected = false;
    mutex_lock(&privload_fls_lock);
    for (e = fls_cb_list, prev = NULL; e != NULL; prev = e, e = e->next) {
        LOG(GLOBAL, LOG_LOADER, 2,
            "%s: comparing cb "PFX" to pc "PFX"\n",
            __FUNCTION__, e->cb, pc);
        if (e->cb != NULL/*head node*/ && (app_pc)e->cb == pc) {
            dr_mcontext_t *mc = get_mcontext(dcontext);
            void *arg = NULL;
            app_pc retaddr;
            redirected = true;
            /* Extract the retaddr and the arg to the callback */
            if (!safe_read((app_pc)mc->xsp, sizeof(retaddr), &retaddr)) {
                /* in debug we'd assert in vmareas anyway */
                ASSERT_NOT_REACHED();
                redirected = false; /* in release we'll interpret the routine I guess */
            }
#ifdef X64
            arg = (void *) mc->xcx;
#else
            if (!safe_read((app_pc)mc->xsp + XSP_SZ, sizeof(arg), &arg))
                ASSERT_NOT_REACHED(); /* we'll still redirect and call w/ NULL */
#endif
            if (redirected) {
                LOG(GLOBAL, LOG_LOADER, 2,
                    "%s: native call to FLS cb "PFX", redirect to "PFX"\n",
                    __FUNCTION__, pc, retaddr);
                (*e->cb)(arg);
                /* This is stdcall so clean up the retaddr + param */
                mc->xsp += XSP_SZ IF_NOT_X64(+ sizeof(arg));
                /* Now we interpret from the retaddr */
                dcontext->next_tag = retaddr;
            }
            /* If we knew the reason for this call we would know whether to remove
             * from the list: for thread exit, leave entry, but for FlsExit, remove.
             * Since we don't know we just leave it.
             */
            break;
        }
    }
    mutex_unlock(&privload_fls_lock);
    return redirected;
}

static DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb)
{
    ASSERT(priv_kernel32_FlsAlloc != NULL);
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privlib_privheap), true) &&
        in_private_library((app_pc)cb)) {
        fls_cb_t *entry = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fls_cb_t,
                                          ACCT_OTHER, PROTECTED);
        mutex_lock(&privload_fls_lock);
        entry->cb = cb;
        /* we have a permanent head node to avoid .data unprot */
        entry->next = fls_cb_list->next;
        fls_cb_list->next = entry;
        mutex_unlock(&privload_fls_lock);
        /* ensure on DR areas list: won't already be only for client lib */
        dynamo_vm_areas_lock();
        if (!is_dynamo_address((app_pc)cb)) {
            add_dynamo_vm_area((app_pc)cb, ((app_pc)cb)+1, MEMPROT_READ|MEMPROT_EXEC,
                               true _IF_DEBUG("fls cb in private lib"));
            /* we do not ever remove: not worth refcount effort, and probably
             * good to catch future executions
             */
        }
        dynamo_vm_areas_unlock();
        LOG(GLOBAL, LOG_LOADER, 2, "%s: cb="PFX"\n", __FUNCTION__, cb);
    }
    return (*priv_kernel32_FlsAlloc)(cb);
}

/* Eventually we should intercept at the Ldr level but that takes more work
 * so we initially just intercept here.  This is also needed to intercept
 * FlsAlloc located dynamically by msvcrt init.
 */
static HMODULE WINAPI
redirect_GetModuleHandleA(const char *name)
{
    privmod_t *mod;
    app_pc res = NULL;
    ASSERT(priv_kernel32_GetModuleHandleA != NULL);
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup(name);
    if (mod != NULL) {
        res = mod->base;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s => "PFX"\n", __FUNCTION__, name, res);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL)
        return (*priv_kernel32_GetModuleHandleA)(name);
    else
        return (HMODULE) res;
}

static FARPROC WINAPI
redirect_GetProcAddress(app_pc modbase, const char *name)
{
    privmod_t *mod;
    app_pc res = NULL;
    ASSERT(priv_kernel32_GetProcAddress != NULL);
    LOG(GLOBAL, LOG_LOADER, 2, "%s: "PFX"%s\n", __FUNCTION__, modbase, name);
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base(modbase);
    if (mod != NULL) {
        const char *forwarder;
        res = privload_redirect_imports(mod, name);
        /* I assume GetProcAddress returns NULL for forwarded exports? */
        if (res == NULL)
            res = (app_pc) get_proc_address_ex(modbase, name, &forwarder);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s => "PFX"\n", __FUNCTION__, name, res);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL)
        return (*priv_kernel32_GetProcAddress)((HMODULE)modbase, name);
    else
        return (FARPROC) convert_data_to_function(res);
}

static HMODULE WINAPI
redirect_LoadLibraryA(const char *name)
{
    app_pc res = NULL;
    ASSERT(priv_kernel32_LoadLibraryA != NULL);
    res = load_private_library(name);
    if (res == NULL)
        return (*priv_kernel32_LoadLibraryA)(name);
    else
        return (HMODULE) res;
}

