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
 *
 * i#234: earliest injection:
 * - use bootstrap loader w/ manual syscalls or ntdll binding to load DR
 *   itself with this private loader at very first APC point
 *
 * i#249: TLS/TEB/PEB isolation for private dll copies
 * - -private_peb uses a private PEB copy, but is limited in several respects:
 *   * uses a shallow copy
 *     (we should look at the fiber API to see the full list of fields to copy)
 *   * does not intercept private libs/client using NtQueryInformationProcess
 *     but kernel seems to just use TEB pointer anyway!
 *   * added dr_get_app_PEB() for client to get app PEB
 *
 * i#1299: improved isolation of user32.dll
 */

#include "../globals.h"
#include "../module_shared.h"
#include "ntdll.h"
#include "os_private.h"
#include "diagnost.h" /* to read systemroot reg key */
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "drwinapi/drwinapi.h"

#ifdef X64
#    define IMAGE_ORDINAL_FLAG IMAGE_ORDINAL_FLAG64
#else
#    define IMAGE_ORDINAL_FLAG IMAGE_ORDINAL_FLAG32
#endif

typedef struct _os_privmod_data_t {
    PIMAGE_TLS_CALLBACK *tls_callbacks;
    int tls_callback_count;
    int tls_idx;
    byte *tls_init_data;
    size_t tls_size;
} os_privmod_data_t;

static volatile int tls_next_idx;
static volatile int tls_array_count;
/* It is complex to realloc this; it is rarely used so we have a simple max. */
#define TLS_ARRAY_MAX_SIZE 32

/* Not persistent across code cache execution, so not protected.
 * Synchronized by privload_lock.
 */
DECLARE_NEVERPROT_VAR(static char modpath[MAXIMUM_PATH], { 0 });
DECLARE_NEVERPROT_VAR(static char forwmodpath[MAXIMUM_PATH], { 0 });

/* Written during initialization only */
static char systemroot[MAXIMUM_PATH];

static bool windbg_cmds_initialized;

/* PE entry points take 3 args */
typedef BOOL(WINAPI *dllmain_t)(HANDLE, DWORD, LPVOID);

/* forward decls */

static void
privload_init_search_paths(void);

static bool
privload_get_import_descriptor(privmod_t *mod, IMAGE_IMPORT_DESCRIPTOR **imports OUT,
                               app_pc *imports_end OUT);

static bool
privload_process_one_import(privmod_t *mod, privmod_t *impmod, IMAGE_THUNK_DATA *lookup,
                            app_pc *address);

static const char *
privload_map_name(const char *impname, privmod_t *immed_dep);

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent, bool reachable);

static void
privload_add_windbg_cmds_post_init(privmod_t *mod);

static app_pc
privload_redirect_imports(privmod_t *impmod, const char *name, privmod_t *importer);

static void
privload_add_windbg_cmds(void);

/* Isolate the app's PEB by making a copy for use by private libs (i#249) */
static PEB *private_peb;
static bool private_peb_initialized = false;
/* Isolate TEB->FlsData: for first thread we need to copy before have dcontext */
static void *pre_fls_data;
/* Isolate TEB->ReservedForNtRpc: for first thread we need to copy before have dcontext */
static void *pre_nt_rpc;
/* Isolate TEB->NlsCache: for first thread we need to copy before have dcontext */
static void *pre_nls_cache;
/* Isolate TEB->ThreadLocalStoragePointer. */
static void *pre_static_tls;
/* FIXME i#875: we do not have ntdll!RtlpFlsLock isolated.  Living w/ it for now. */

/* NtTickCount: not really a syscall, just reads KUSER_SHARED_DATA.
 * Redirects to RtlGetTickCount on Win2003+.
 * But, it's not present on XP (i#1195), so we have to dynamically
 * look it up.
 */
typedef ULONG_PTR(NTAPI *ntdll_NtTickCount_t)(void);
static ntdll_NtTickCount_t ntdll_NtTickCount;

/***************************************************************************/

HANDLE WINAPI
RtlCreateHeap(ULONG flags, void *base, size_t reserve_sz, size_t commit_sz, void *lock,
              void *params);

BOOL WINAPI
RtlDestroyHeap(HANDLE base);

void
os_loader_init_prologue(void)
{
    app_pc ntdll = get_ntdll_base();
    app_pc drdll = get_dynamorio_dll_start();
    app_pc user32 = NULL;
    privmod_t *mod;
    /* FIXME i#812: need to delay this for earliest injection */
    if (!dr_earliest_injected && !standalone_library) {
        user32 = (app_pc)get_module_handle(L"user32.dll");
    }

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
        GET_NTDLL(RtlInitializeCriticalSection, (OUT RTL_CRITICAL_SECTION * crit));
        PEB *own_peb = get_own_peb();
        /* FIXME: does it need to be page-aligned? */
        private_peb = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, PEB, ACCT_OTHER, UNPROTECTED);
        memcpy(private_peb, own_peb, sizeof(*private_peb));
        /* We need priv libs to NOT use any locks that app code uses: else we'll
         * deadlock (classic transparency violation).
         * One concern here is that the real PEB points at ntdll!FastPebLock
         * but we assume nobody cares.
         */
        private_peb->FastPebLock = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, RTL_CRITICAL_SECTION,
                                                   ACCT_OTHER, UNPROTECTED);
        if (!dr_earliest_injected) /* FIXME i#812: need to delay this */
            RtlInitializeCriticalSection(private_peb->FastPebLock);

        /* We can't redirect ntdll routines allocating memory internally,
         * but we can at least have them not affect the app's Heap.
         * We do this after the swap in case it affects some other peb field,
         * in which case it will match the RtlDestroyHeap.
         */
        if (dr_earliest_injected) { /* FIXME i#812: need to delay RtlCreateHeap */
            private_peb->ProcessHeap = own_peb->ProcessHeap;
        } else {
            private_peb->ProcessHeap =
                RtlCreateHeap(HEAP_GROWABLE | HEAP_CLASS_PRIVATE, NULL, 0, 0, NULL, NULL);
            if (private_peb->ProcessHeap == NULL) {
                SYSLOG_INTERNAL_ERROR("private default heap creation failed");
                /* fallback */
                private_peb->ProcessHeap = own_peb->ProcessHeap;
            }
        }

        if (get_os_version() >= WINDOWS_VERSION_2003) {
            /* FLS is supported in WinXP-64 or later */
            /* We need a custom setup for FLS structures */
            ntdll_redir_fls_init(own_peb, private_peb);
        }

        private_peb_initialized = true;
        swap_peb_pointer(NULL, true /*to priv*/);
        LOG(GLOBAL, LOG_LOADER, 2, "app peb=" PFX "\n", own_peb);
        LOG(GLOBAL, LOG_LOADER, 2, "private peb=" PFX "\n", private_peb);

        if (should_swap_teb_nonstack_fields()) {
            pre_fls_data = d_r_get_tls(FLS_DATA_TIB_OFFSET);
            pre_nt_rpc = d_r_get_tls(NT_RPC_TIB_OFFSET);
            pre_nls_cache = d_r_get_tls(NLS_CACHE_TIB_OFFSET);
            /* Clear state to separate priv from app.
             * XXX: if we attach or something it seems possible that ntdll or user32
             * or some other shared resource might set these and we want to share
             * the value between app and priv.  In that case we should not clear here
             * and should relax the asserts in d_r_dispatch and is_using_app_peb to
             * allow app==priv if both ==pre.
             */
            d_r_set_tls(FLS_DATA_TIB_OFFSET, NULL);
            d_r_set_tls(NT_RPC_TIB_OFFSET, NULL);
            d_r_set_tls(NLS_CACHE_TIB_OFFSET, NULL);
            LOG(GLOBAL, LOG_LOADER, 2, "initial thread TEB->FlsData=" PFX "\n",
                pre_fls_data);
            LOG(GLOBAL, LOG_LOADER, 2, "initial thread TEB->ReservedForNtRpc=" PFX "\n",
                pre_nt_rpc);
            LOG(GLOBAL, LOG_LOADER, 2, "initial thread TEB->NlsCache=" PFX "\n",
                pre_nls_cache);
        }
        /* FIX: i#6024: when kernelbase.dll has TLS or client which will lead to
         * crash. Solution is to always save pre_static_tls but do not set
         * ThreadLocalStoragePointer to NULL. If we set it to NULL, it will lead to
         * crash when application uses ThreadLocalStoragePointer (eg. __declspec(thread)
         * is used), but client or kernelbase.dll (Win11/Win22H2) or client doesn't
         * have TLS which will fail to restore TheadLocalStoragePointer later in
         * calls to should_swap_teb_static_tls() as tlx_next_idx is 0
         */
        pre_static_tls = d_r_get_tls(STATIC_TLS_TIB_OFFSET);
        LOG(GLOBAL, LOG_LOADER, 2,
            "initial thread TEB->ThreadLocalStoragePointer=" PFX "\n", pre_static_tls);
    }

    drwinapi_init();

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    privload_init_search_paths();
    /* We count on having at least one node that's never removed so we
     * don't have to unprot .data and write to modlist later
     */
    snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/system32/%s", systemroot,
             "ntdll.dll");
    NULL_TERMINATE_BUFFER(modpath);
    mod = privload_insert(NULL, ntdll, get_allocation_size(ntdll, NULL), "ntdll.dll",
                          modpath);
    mod->externally_loaded = true;
    /* FIXME i#234: Once we have earliest injection and load DR via this private loader
     * (i#234/PR 204587) we can remove this
     */
    mod = privload_insert(NULL, drdll, get_allocation_size(drdll, NULL),
                          DYNAMORIO_LIBRARY_NAME, get_dynamorio_library_path());
    mod->externally_loaded = true;

    /* Sometimes a privlib calls LoadLibrary to get a handle to the executable */
    mod = privload_insert(
        NULL, get_application_base(), get_application_end() - get_application_base() + 1,
        get_application_short_unqualified_name(), get_application_name());
    mod->externally_loaded = true;
#ifdef STATIC_LIBRARY
    /* Set up TLS and ensure the thread init functions are called.
     * loader_shared will not call privload_os_finalize() for externally_loaded,
     * but it will call privload_call_entry() for us.
     */
    mod->is_client = true;
    privload_os_finalize(mod);
#endif

    /* FIXME i#1299: loading a private user32.dll is problematic: it registers
     * callbacks that KiUserCallbackDispatcher invokes.  For now we do not
     * duplicate it.  If the app loads it dynamically later we will end up
     * duplicating but not worth checking for that.
     * If client private lib loads user32 when app does not statically depend
     * on it, we'll have a private copy and no app copy: this may cause
     * problems later but waiting to see.
     */
    if (user32 != NULL) {
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/system32/%s", systemroot,
                 "user32.dll");
        NULL_TERMINATE_BUFFER(modpath);
        mod = privload_insert(NULL, user32, get_allocation_size(user32, NULL),
                              "user32.dll", modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "adding app's user32.dll to privlib list\n");
        mod->externally_loaded = true;
    }

    /* i#1195: NtTickCount is only on 2K3+.  If we can't find it we use
     * the old KUSER_SHARED_DATA so make sure we're on an old OS.
     */
    ntdll_NtTickCount =
        (ntdll_NtTickCount_t)d_r_get_proc_address(get_ntdll_base(), "NtGetTickCount");
    ASSERT(ntdll_NtTickCount != NULL || get_os_version() <= WINDOWS_VERSION_XP);
}

void
os_loader_init_epilogue(void)
{
#ifndef STANDALONE_UNIT_TEST
    /* drmarker and the privlist are set up, so fill in the windbg commands (i#522) */
    privload_add_windbg_cmds();
#endif
}

void
os_loader_exit(void)
{
    drwinapi_exit();

    if (INTERNAL_OPTION(private_peb)) {
        /* Swap back so any further peb queries (e.g., reading env var
         * while reporting a leak) use a non-freed peb
         */
        swap_peb_pointer(NULL, false /*to app*/);
        /* we do have a dcontext */
        ASSERT(get_thread_private_dcontext != NULL);
        TRY_EXCEPT(get_thread_private_dcontext(),
                   { RtlDestroyHeap(private_peb->ProcessHeap); },
                   {
                       /* shouldn't crash, but does on security-win32/sd_tester,
                        * probably b/c it corrupts the heap: regardless we don't
                        * want DR reporting a crash on an ntdll address so we ignore.
                        */
                   });

        if (get_os_version() >= WINDOWS_VERSION_2003) {
            /* FLS is supported in WinXP-64 or later */
            ntdll_redir_fls_exit(private_peb);
        }

        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, private_peb->FastPebLock, RTL_CRITICAL_SECTION,
                       ACCT_OTHER, UNPROTECTED);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, private_peb, PEB, ACCT_OTHER, UNPROTECTED);
    }

    tls_next_idx = 0;
}

void
os_loader_thread_init_prologue(dcontext_t *dcontext)
{
    /* XXX i#4030: We do not support libraries loaded after init time.  We'd have
     * to go through and re-allocate all the thread arrays.
     * We detect this and abort in privload_os_finalize().
     */
    if (tls_next_idx > 0) {
        if (tls_array_count == 0)
            tls_array_count = tls_next_idx;
        ASSERT(tls_array_count == tls_next_idx);
        dcontext->priv_static_tls = HEAP_ARRAY_ALLOC(
            GLOBAL_DCONTEXT, byte *, tls_array_count, ACCT_OTHER, PROTECTED);
        memset(dcontext->priv_static_tls, 0, tls_array_count * sizeof(byte *));
        d_r_set_tls(STATIC_TLS_TIB_OFFSET, dcontext->priv_static_tls);
        LOG(THREAD, LOG_LOADER, 2, "set up TLS array @" PFX " for %d libraries\n",
            dcontext->priv_static_tls, tls_array_count);
    } else
        dcontext->priv_static_tls = NULL;

    if (INTERNAL_OPTION(private_peb)) {
        if (!dynamo_initialized) {
            /* For first thread use cached pre-priv-lib value for app and
             * whatever value priv libs have set for priv
             */
            dcontext->app_stack_limit = d_r_get_tls(BASE_STACK_TIB_OFFSET);
            dcontext->app_stack_base = d_r_get_tls(TOP_STACK_TIB_OFFSET);
            if (should_swap_teb_nonstack_fields()) {
                dcontext->priv_fls_data = d_r_get_tls(FLS_DATA_TIB_OFFSET);
                dcontext->priv_nt_rpc = d_r_get_tls(NT_RPC_TIB_OFFSET);
                dcontext->priv_nls_cache = d_r_get_tls(NLS_CACHE_TIB_OFFSET);
                dcontext->app_fls_data = pre_fls_data;
                dcontext->app_nt_rpc = pre_nt_rpc;
                dcontext->app_nls_cache = pre_nls_cache;
                d_r_set_tls(FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
                d_r_set_tls(NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
                d_r_set_tls(NLS_CACHE_TIB_OFFSET, dcontext->app_nls_cache);
            }
            if (should_swap_teb_static_tls()) {
                dcontext->app_static_tls = pre_static_tls;
                d_r_set_tls(STATIC_TLS_TIB_OFFSET, dcontext->app_static_tls);
            }
        } else {
            /* The real value will be set by swap_peb_pointer */
            dcontext->app_stack_limit = NULL;
            dcontext->app_stack_base = NULL;
            if (should_swap_teb_nonstack_fields()) {
                dcontext->app_fls_data = NULL;
                dcontext->app_nt_rpc = NULL;
                dcontext->app_nls_cache = NULL;
                /* We assume clearing out any non-NULL value for priv is safe */
                dcontext->priv_fls_data = NULL;
                dcontext->priv_nt_rpc = NULL;
                dcontext->priv_nls_cache = NULL;
            }
            if (should_swap_teb_static_tls()) {
                dcontext->app_static_tls = NULL;
            }
        }
        LOG(THREAD, LOG_LOADER, 2, "app stack limit=" PFX "\n",
            dcontext->app_stack_limit);
        LOG(THREAD, LOG_LOADER, 2, "app stack base=" PFX "\n", dcontext->app_stack_base);
        if (should_swap_teb_nonstack_fields()) {
            LOG(THREAD, LOG_LOADER, 2, "app fls=" PFX ", priv fls=" PFX "\n",
                dcontext->app_fls_data, dcontext->priv_fls_data);
            LOG(THREAD, LOG_LOADER, 2, "app rpc=" PFX ", priv rpc=" PFX "\n",
                dcontext->app_nt_rpc, dcontext->priv_nt_rpc);
            LOG(THREAD, LOG_LOADER, 2, "app nls_cache=" PFX ", priv nls_cache=" PFX "\n",
                dcontext->app_nls_cache, dcontext->priv_nls_cache);
        }
        if (should_swap_teb_static_tls()) {
            LOG(THREAD, LOG_LOADER, 2,
                "app static_tls=" PFX ", priv static_tls=" PFX "\n",
                dcontext->app_static_tls, dcontext->priv_static_tls);
        }
        /* For swapping teb fields (detach, reset i#25) we'll need to
         * know the teb base from another thread
         */
        dcontext->teb_base = (byte *)d_r_get_tls(SELF_TIB_OFFSET);
    }

    if (INTERNAL_OPTION(private_peb))
        swap_peb_pointer(dcontext, true /*to priv*/);
}

void
os_loader_thread_init_epilogue(dcontext_t *dcontext)
{
    /* For subsequent app threads, peb ptr will be swapped to priv
     * by transfer_to_dispatch(), and w/ FlsData swap we have to
     * properly nest.
     */
    if (dynamo_initialized /*later thread*/ && !IS_CLIENT_THREAD(dcontext))
        swap_peb_pointer(dcontext, false /*to app*/);
}

void
os_loader_thread_exit(dcontext_t *dcontext)
{
    /* i#3633: In case of windows 1903 if priv_fls_data ends up in internal list of
     * ntdll.dll, we have to unlink it manually. We do unlinking always on thread
     * exit.
     */
    ntdll_redir_fls_thread_exit(dcontext->priv_fls_data);
    if (tls_array_count > 0) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, dcontext->priv_static_tls, byte *,
                        tls_array_count, ACCT_OTHER, PROTECTED);
        dcontext->priv_static_tls = NULL;
    }
}

/* our copy of the PEB for isolation (i#249) */
PEB *
get_private_peb(void)
{
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(private_peb != NULL);
    return private_peb;
}

/* For performance reasons we avoid the swap of PEB if there's no client.
 * We'd like to do so if there are no private WinAPI libs
 * (we assume libs not in the system dir will not write to PEB or TEB fields we
 * care about (mainly Fls ones)), but kernel32 can be loaded in dr_client_main()
 * (which is after d_r_arch_init()) via dr_enable_console_printing(); plus,
 * a client could load kernel32 via dr_load_aux_library(), or a 3rd-party
 * priv lib could load anything at any time.  Xref i#984.
 * This does not indicate whether TEB fields should be swapped: we need
 * to swap TEB stack fields even with no client (DrM#1626, DrM#1723, i#1692).
 * That's covered by SWAP_TEB_STACK{LIMIT,BASE}.
 *
 * If we change this to only swap if there's no private WinAPI lib, we'll need
 * to figure out how to deal with the ordering problem from i#338 where arch_init
 * needs to know whether to emit swap routines, yet the private loader wants
 * a dcontext before invoking lib entry routines and so wants loader_init to be
 * *after* arch_init (or at least, after dynamo_thread_init).
 */
bool
should_swap_peb_pointer(void)
{
#ifdef STANDALONE_UNIT_TEST
    /* Our drwinapi tests require FLS isolation, etc. */
    return true;
#else
    return (INTERNAL_OPTION(private_peb) && CLIENTS_EXIST());
#endif
}

bool
should_swap_teb_nonstack_fields(void)
{
    return should_swap_peb_pointer();
}

bool
should_swap_teb_static_tls(void)
{
    /* XXX: Since today we do not support late-loaded priv libs w/ static TLS, we can
     * make the decision on whether to swap the TEB.ThreadLocalStoragePointer at
     * init time and avoid a dynamic check in our gencode in preinsert_swap_peb().
     */
    return tls_next_idx > 0 && should_swap_teb_nonstack_fields();
}

static void *
get_teb_field(dcontext_t *dcontext, ushort offs)
{
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT) {
        /* get our own */
        return d_r_get_tls(offs);
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
        d_r_set_tls(offs, value);
    } else {
        byte *teb = dcontext->teb_base;
        ASSERT(dcontext->teb_base != NULL);
        *((void **)(teb + offs)) = value;
    }
}

bool
is_using_app_peb(dcontext_t *dcontext)
{
    /* don't use get_own_peb() as we want what's actually pointed at by TEB */
    PEB *cur_peb = get_teb_field(dcontext, PEB_TIB_OFFSET);
    void *cur_stack_limit;
    void *cur_stack_base;
    void *cur_fls;
    void *cur_rpc;
    void *cur_nls_cache;
    void *cur_static_tls;
    if (!INTERNAL_OPTION(private_peb) || !private_peb_initialized)
        return true;
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    ASSERT(cur_peb != NULL);
    cur_stack_limit = get_teb_field(dcontext, BASE_STACK_TIB_OFFSET);
    cur_stack_base = get_teb_field(dcontext, TOP_STACK_TIB_OFFSET);
    if (!should_swap_peb_pointer() || !should_swap_teb_nonstack_fields()) {
        if (SWAP_TEB_STACKLIMIT())
            return cur_stack_limit != dcontext->dstack - DYNAMORIO_STACK_SIZE;
        if (SWAP_TEB_STACKBASE())
            return cur_stack_base != dcontext->dstack;
        return true;
    }
    cur_fls = get_teb_field(dcontext, FLS_DATA_TIB_OFFSET);
    cur_rpc = get_teb_field(dcontext, NT_RPC_TIB_OFFSET);
    cur_nls_cache = get_teb_field(dcontext, NLS_CACHE_TIB_OFFSET);
    cur_static_tls = get_teb_field(dcontext, STATIC_TLS_TIB_OFFSET);
    if (cur_peb == get_private_peb()) {
        /* won't nec equal the priv_ value since could have changed: but should
         * not have the app value!
         */
        ASSERT(!is_dynamo_address(dcontext->app_stack_limit) ||
               IS_CLIENT_THREAD(dcontext) || IS_CLIENT_THREAD_EXITING(dcontext));
        ASSERT(!is_dynamo_address((byte *)dcontext->app_stack_base - 1) ||
               IS_CLIENT_THREAD(dcontext) || IS_CLIENT_THREAD_EXITING(dcontext));
        ASSERT(cur_fls == NULL || cur_fls != dcontext->app_fls_data);
        ASSERT(cur_rpc == NULL || cur_rpc != dcontext->app_nt_rpc);
        ASSERT(cur_nls_cache == NULL || cur_nls_cache != dcontext->app_nls_cache);
        ASSERT(cur_static_tls == NULL || cur_static_tls != dcontext->app_static_tls);
        return false;
    } else {
        /* won't nec equal the app_ value since could have changed: but should
         * not have the priv value!
         */
        ASSERT(!is_dynamo_address(cur_stack_limit));
        ASSERT(!is_dynamo_address((byte *)cur_stack_base - 1));
        ASSERT(cur_fls == NULL || cur_fls != dcontext->priv_fls_data);
        ASSERT(cur_rpc == NULL || cur_rpc != dcontext->priv_nt_rpc);
        ASSERT(cur_static_tls == NULL || cur_static_tls != dcontext->priv_static_tls);
        return true;
    }
}

#ifdef DEBUG
static void
print_teb_fields(dcontext_t *dcontext, const char *reason)
{
    void *cur_stack_limit = get_teb_field(dcontext, BASE_STACK_TIB_OFFSET);
    byte *cur_stack_base = (byte *)get_teb_field(dcontext, TOP_STACK_TIB_OFFSET);
    void *cur_fls = get_teb_field(dcontext, FLS_DATA_TIB_OFFSET);
    void *cur_rpc = get_teb_field(dcontext, NT_RPC_TIB_OFFSET);
    void *cur_nls_cache = get_teb_field(dcontext, NLS_CACHE_TIB_OFFSET);
    void *cur_static_tls = get_teb_field(dcontext, STATIC_TLS_TIB_OFFSET);
    LOG(THREAD, LOG_LOADER, 1, "%s\n", reason);
    LOG(THREAD, LOG_LOADER, 3, "  cur stack_limit=" PFX ", app stack_limit=" PFX "\n",
        cur_stack_limit, dcontext->app_stack_limit);
    LOG(THREAD, LOG_LOADER, 3, "  cur stack_base=" PFX ", app stack_base=" PFX "\n",
        cur_stack_base, dcontext->app_stack_base);
    LOG(THREAD, LOG_LOADER, 3, "  cur fls=" PFX ", app fls=" PFX ", priv fls=" PFX "\n",
        cur_fls, dcontext->app_fls_data, dcontext->priv_fls_data);
    LOG(THREAD, LOG_LOADER, 3, "  cur rpc=" PFX ", app rpc=" PFX ", priv rpc=" PFX "\n",
        cur_rpc, dcontext->app_nt_rpc, dcontext->priv_nt_rpc);
    LOG(THREAD, LOG_LOADER, 3,
        "  cur nls_cache=" PFX ", app nls_cache=" PFX ", priv nls_cache=" PFX "\n",
        cur_nls_cache, dcontext->app_nls_cache, dcontext->priv_nls_cache);
    LOG(THREAD, LOG_LOADER, 3,
        "  cur static_tls=" PFX ", app static_tls=" PFX ", priv static_tls=" PFX "\n",
        cur_static_tls, dcontext->app_static_tls, dcontext->priv_static_tls);
}
#endif

static void
swap_peb_pointer_ex(dcontext_t *dcontext, bool to_priv, dr_state_flags_t flags)
{
    PEB *tgt_peb = to_priv ? get_private_peb() : get_own_peb();
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(private_peb_initialized);
    ASSERT(tgt_peb != NULL);
    if (TEST(DR_STATE_PEB, flags) && should_swap_peb_pointer()) {
        set_teb_field(dcontext, PEB_TIB_OFFSET, (void *)tgt_peb);
        LOG(THREAD, LOG_LOADER, 2, "set teb->peb to " PFX "\n", tgt_peb);
    }
    if (dcontext != NULL && dcontext != GLOBAL_DCONTEXT) {
        /* We preserve TEB->LastErrorValue and we swap TEB->FlsData,
         * TEB->ReservedForNtRpc, and TEB->NlsCache.
         */
        void *cur_stack_limit = get_teb_field(dcontext, BASE_STACK_TIB_OFFSET);
        byte *cur_stack_base = (byte *)get_teb_field(dcontext, TOP_STACK_TIB_OFFSET);
        void *cur_fls = NULL;
        void *cur_rpc = NULL;
        void *cur_nls_cache = NULL;
        void *cur_static_tls = NULL;
        if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_nonstack_fields()) {
            cur_fls = get_teb_field(dcontext, FLS_DATA_TIB_OFFSET);
            cur_rpc = get_teb_field(dcontext, NT_RPC_TIB_OFFSET);
            cur_nls_cache = get_teb_field(dcontext, NLS_CACHE_TIB_OFFSET);
        }
        if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_static_tls()) {
            cur_static_tls = get_teb_field(dcontext, STATIC_TLS_TIB_OFFSET);
        }
        DOLOG(3, LOG_LOADER, {
            print_teb_fields(dcontext, to_priv ? "pre swap to priv" : "pre swap to app");
        });
        if (to_priv) {
            if (TEST(DR_STATE_STACK_BOUNDS, flags) &&
                dynamo_initialized /* on app stack until init finished */) {
                if (SWAP_TEB_STACKLIMIT() &&
                    /* Handle two in a row, using an exact cmp b/c
                     * is_dynamo_address() is slow and needs locks (i#1832).
                     */
                    cur_stack_limit != dcontext->dstack - DYNAMORIO_STACK_SIZE) {
                    dcontext->app_stack_limit = cur_stack_limit;
                    set_teb_field(dcontext, BASE_STACK_TIB_OFFSET,
                                  dcontext->dstack - DYNAMORIO_STACK_SIZE);
                }
                if (SWAP_TEB_STACKBASE() &&
                    /* Handle two in a row, using an exact cmp b/c
                     * is_dynamo_address() is slow and needs locks (i#1832).
                     */
                    cur_stack_base != dcontext->dstack) {
                    dcontext->app_stack_base = cur_stack_base;
                    set_teb_field(dcontext, TOP_STACK_TIB_OFFSET, dcontext->dstack);
                }
            }
            if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_nonstack_fields()) {
                /* note: two calls in a row will clobber app_errno w/ wrong value! */
                dcontext->app_errno =
                    (int)(ptr_int_t)get_teb_field(dcontext, ERRNO_TIB_OFFSET);
                if (dcontext->priv_fls_data != cur_fls) { /* handle two calls in a row */
                    dcontext->app_fls_data = cur_fls;
                    set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->priv_fls_data);
                }
                if (dcontext->priv_nt_rpc != cur_rpc) { /* handle two calls in a row */
                    dcontext->app_nt_rpc = cur_rpc;
                    set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->priv_nt_rpc);
                }
                if (dcontext->priv_nls_cache != cur_nls_cache) { /* handle two in a row */
                    dcontext->app_nls_cache = cur_nls_cache;
                    set_teb_field(dcontext, NLS_CACHE_TIB_OFFSET,
                                  dcontext->priv_nls_cache);
                }
            }
            if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_static_tls()) {
                if (dcontext->priv_static_tls !=
                    cur_static_tls) { /* handle two in a row */
                    dcontext->app_static_tls = cur_static_tls;
                    set_teb_field(dcontext, STATIC_TLS_TIB_OFFSET,
                                  dcontext->priv_static_tls);
                }
            }
        } else {
            if (TEST(DR_STATE_STACK_BOUNDS, flags)) {
                if (SWAP_TEB_STACKLIMIT() &&
                    /* Handle two in a row, using an exact cmp b/c
                     * is_dynamo_address() is slow and needs locks (i#1832).
                     */
                    cur_stack_limit == dcontext->dstack - DYNAMORIO_STACK_SIZE) {
                    set_teb_field(dcontext, BASE_STACK_TIB_OFFSET,
                                  dcontext->app_stack_limit);
                }
                if (SWAP_TEB_STACKBASE() &&
                    /* Handle two in a row, using an exact cmp b/c
                     * is_dynamo_address() is slow and needs locks (i#1832).
                     */
                    cur_stack_base == dcontext->dstack) {
                    set_teb_field(dcontext, TOP_STACK_TIB_OFFSET,
                                  dcontext->app_stack_base);
                }
            }
            if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_nonstack_fields()) {
                /* two calls in a row should be fine */
                set_teb_field(dcontext, ERRNO_TIB_OFFSET,
                              (void *)(ptr_int_t)dcontext->app_errno);
                if (dcontext->app_fls_data != cur_fls) { /* handle two calls in a row */
                    dcontext->priv_fls_data = cur_fls;
                    set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
                }
                if (dcontext->app_nt_rpc != cur_rpc) { /* handle two calls in a row */
                    dcontext->priv_nt_rpc = cur_rpc;
                    set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
                }
                if (dcontext->app_nls_cache != cur_nls_cache) { /* handle two in a row */
                    dcontext->priv_nls_cache = cur_nls_cache;
                    set_teb_field(dcontext, NLS_CACHE_TIB_OFFSET,
                                  dcontext->app_nls_cache);
                }
            }
            if (TEST(DR_STATE_TEB_MISC, flags) && should_swap_teb_static_tls()) {
                if (dcontext->app_static_tls !=
                    cur_static_tls) { /* handle two in a row */
                    /* Unlike the other fields, we control this private one so we
                     * never set it from the TEB field.
                     */
                    set_teb_field(dcontext, STATIC_TLS_TIB_OFFSET,
                                  dcontext->app_static_tls);
                }
            }
        }
        ASSERT(!is_dynamo_address(dcontext->app_stack_limit) ||
               IS_CLIENT_THREAD(dcontext));
        ASSERT(!is_dynamo_address((byte *)dcontext->app_stack_base - 1) ||
               IS_CLIENT_THREAD(dcontext));
        if (should_swap_teb_nonstack_fields()) {
            ASSERT(!is_dynamo_address(dcontext->app_fls_data));
            ASSERT(!is_dynamo_address(dcontext->app_nt_rpc));
            ASSERT(!is_dynamo_address(dcontext->app_nls_cache));
        }
        if (should_swap_teb_static_tls()) {
            ASSERT(!is_dynamo_address(dcontext->app_static_tls));
        }
        /* Once we have earier injection we should be able to assert
         * that priv_fls_data is either NULL or a DR address: but on
         * notepad w/ drinject it's neither: need to investigate.
         */
        DOLOG(3, LOG_LOADER, {
            print_teb_fields(dcontext,
                             to_priv ? "post swap to priv" : "post swap to app");
        });
    }
}

/* C version of preinsert_swap_peb() */
void
swap_peb_pointer(dcontext_t *dcontext, bool to_priv)
{
    swap_peb_pointer_ex(dcontext, to_priv, DR_STATE_ALL);
}

/* Meant for use on detach only: restore app values and does not update
 * or swap private values.  Up to caller to synchronize w/ other thread.
 */
void
restore_peb_pointer_for_thread(dcontext_t *dcontext)
{
    PEB *tgt_peb = get_own_peb();
    ASSERT(INTERNAL_OPTION(private_peb));
    ASSERT(private_peb_initialized);
    ASSERT(tgt_peb != NULL);
    ASSERT(dcontext != NULL && dcontext->teb_base != NULL);
    if (should_swap_peb_pointer()) {
        set_teb_field(dcontext, PEB_TIB_OFFSET, (void *)tgt_peb);
        LOG(GLOBAL, LOG_LOADER, 2, "set teb->peb to " PFX "\n", tgt_peb);
    }
    if (should_swap_teb_nonstack_fields()) {
        set_teb_field(dcontext, ERRNO_TIB_OFFSET, (void *)(ptr_int_t)dcontext->app_errno);
        LOG(THREAD, LOG_LOADER, 3, "restored app errno to " PIFX "\n",
            dcontext->app_errno);
        /* We also swap TEB->FlsData and TEB->ReservedForNtRpc */
        set_teb_field(dcontext, FLS_DATA_TIB_OFFSET, dcontext->app_fls_data);
        LOG(THREAD, LOG_LOADER, 3, "restored app fls to " PFX "\n",
            dcontext->app_fls_data);
        set_teb_field(dcontext, NT_RPC_TIB_OFFSET, dcontext->app_nt_rpc);
        LOG(THREAD, LOG_LOADER, 3, "restored app fls to " PFX "\n", dcontext->app_nt_rpc);
        set_teb_field(dcontext, NLS_CACHE_TIB_OFFSET, dcontext->app_nls_cache);
        LOG(THREAD, LOG_LOADER, 3, "restored app nls_cache to " PFX "\n",
            dcontext->app_nls_cache);
    }
    if (should_swap_teb_static_tls()) {
        set_teb_field(dcontext, STATIC_TLS_TIB_OFFSET, dcontext->app_static_tls);
        LOG(THREAD, LOG_LOADER, 3, "restored app static_tls to " PFX "\n",
            dcontext->app_static_tls);
    }
}

void
loader_pre_client_thread_exit(dcontext_t *dcontext)
{
    /* See comments by SWAP_TEB_STACKLIMIT() on our overall strategy.
     * At thread exit, prior to running client or privlib code, we need to
     * remove references to the app stack, which could be freed now (if at
     * process exit or some other synchall from another thread).
     */
    if (SWAP_TEB_STACKLIMIT()) {
        set_teb_field(dcontext, BASE_STACK_TIB_OFFSET,
                      dcontext->dstack - DYNAMORIO_STACK_SIZE);
    }
}

void
check_app_stack_limit(dcontext_t *dcontext)
{
    /* DrMi#1723: while in priv cxt, client may have touched an app guard page
     * in another thread, which will result in no update to TEB.StackLimit.  We
     * try to recover here before any harm is done (although on the new Win8.1,
     * any fault will result in sudden death w/ a bad StackLimit: i#1676).
     */
    MEMORY_BASIC_INFORMATION mbi;
    byte *start_pc, *check_pc;
    size_t res;
    if (!SWAP_TEB_STACKLIMIT() && !SWAP_TEB_STACKBASE())
        return;
    if (SWAP_TEB_STACKLIMIT())
        start_pc = (byte *)dcontext->app_stack_limit;
    else
        start_pc = (byte *)get_teb_field(dcontext, BASE_STACK_TIB_OFFSET);
    ASSERT(start_pc > (byte *)(ptr_uint_t)PAGE_SIZE);
    check_pc = start_pc;
    do {
        check_pc -= PAGE_SIZE;
        res = query_virtual_memory(check_pc, &mbi, sizeof(mbi));
    } while (res == sizeof(mbi) && !TEST(PAGE_GUARD, mbi.Protect) &&
             check_pc > (byte *)(ptr_uint_t)PAGE_SIZE);
    if (res == sizeof(mbi) && TEST(PAGE_GUARD, mbi.Protect) &&
        check_pc + PAGE_SIZE < start_pc) {
        LOG(THREAD, LOG_LOADER, 2,
            "updated stored TEB.StackLimit from " PFX " to " PFX "\n", start_pc,
            check_pc + PAGE_SIZE);
        dcontext->app_stack_limit = check_pc + PAGE_SIZE;
        if (SWAP_TEB_STACKLIMIT())
            dcontext->app_stack_limit = check_pc + PAGE_SIZE;
        else
            set_teb_field(dcontext, BASE_STACK_TIB_OFFSET, check_pc + PAGE_SIZE);
    }
}

bool
os_should_swap_state(void)
{
    return SWAP_TEB_STACKLIMIT() || SWAP_TEB_STACKBASE() || should_swap_peb_pointer() ||
        should_swap_teb_nonstack_fields();
}

bool
os_using_app_state(dcontext_t *dcontext)
{
    return is_using_app_peb(dcontext);
    return true;
}

void
os_swap_context(dcontext_t *dcontext, bool to_app, dr_state_flags_t flags)
{
    /* i#249: swap PEB pointers */
    swap_peb_pointer_ex(dcontext, !to_app /*to priv*/, flags);
}

static void
privload_alloc_opd(privmod_t *privmod)
{
    privmod->os_privmod_data =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, os_privmod_data_t, ACCT_OTHER, PROTECTED);
    memset(privmod->os_privmod_data, 0, sizeof(os_privmod_data_t));
}

static void
privload_free_opd(privmod_t *privmod)
{
    os_privmod_data_t *opd = privmod->os_privmod_data;
    if (opd->tls_callbacks != NULL) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, opd->tls_callbacks, PIMAGE_TLS_CALLBACK,
                        opd->tls_callback_count, ACCT_OTHER, PROTECTED);
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, privmod->os_privmod_data, os_privmod_data_t,
                   ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = NULL;
}

void
privload_add_areas(privmod_t *privmod)
{
    vmvector_add(modlist_areas, privmod->base, privmod->base + privmod->size,
                 (void *)privmod);
    privload_alloc_opd(privmod);
}

void
privload_remove_areas(privmod_t *privmod)
{
    privload_free_opd(privmod);
    vmvector_remove(modlist_areas, privmod->base, privmod->base + privmod->size);
}

void
privload_unmap_file(privmod_t *mod)
{
    d_r_unmap_file(mod->base, mod->size);
}

bool
privload_unload_imports(privmod_t *mod)
{
    privmod_t *impmod;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    app_pc imports_end;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    if (!privload_get_import_descriptor(mod, &imports, &imports_end)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: error reading imports for %s\n", __FUNCTION__,
            mod->name);
        return false;
    }
    if (imports == NULL) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has no imports\n", __FUNCTION__, mod->name);
        return true;
    }

    while (imports->OriginalFirstThunk != 0) {
        const char *impname = (const char *)RVA_TO_VA(mod->base, imports->Name);
        impname = privload_map_name(impname, mod);
        impmod = privload_lookup(impname);
        /* If we hit an error in the middle of loading we may not have loaded
         * all imports for mod so impmod may not be found
         */
        if (impmod != NULL)
            privload_unload(impmod);
        imports++;
        ASSERT((app_pc)(imports + 1) <= imports_end);
    }
    /* I used to ASSERT((app_pc)(imports+1) == imports_end) but kernel32 on win2k
     * has an extra 10 bytes in the dir->Size for unknown reasons so suppressing
     */
    return true;
}

/* if anything fails, undoes the mapping and returns NULL */
app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT, modload_flags_t flags)
{
    file_t fd;
    app_pc map;
    app_pc pref;
    byte *(*map_func)(file_t, size_t *, uint64, app_pc, uint, map_flags_t);
    bool (*unmap_func)(file_t, size_t);
    ASSERT(size != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(!TEST(MODLOAD_NOT_PRIVLIB, flags), &privload_lock);

    /* On win32 OS_EXECUTE is required to create a section w/ rwx
     * permissions, which is in turn required to map a view w/ rwx
     */
    fd = os_open(filename,
                 OS_OPEN_READ | OS_EXECUTE |
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
    if (dynamo_heap_initialized && !standalone_library) {
        /* These hold the DR lock and update DR areas */
        map_func = d_r_map_file;
        unmap_func = d_r_unmap_file;
    } else {
        map_func = os_map_file;
        unmap_func = os_unmap_file;
    }
    /* On Windows, SEC_IMAGE => the kernel sets up the different segments w/
     * proper protections for us, all on this single map syscall.
     * Thus, we ignore MODLOAD_SKIP_*.
     */
    /* First map: let kernel pick base.
     * Even for a lib that needs to be reachable, we'd need to read or map
     * the headers to find the preferred base anyway, and the common
     * case will be a client lib with a preferred base in the low 2GB which
     * will need no re-mapping.
     */
    map = (*map_func)(fd, size, 0 /*offs*/, NULL /*base*/,
                      /* Ask for max, then restrict pieces */
                      MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC,
                      MAP_FILE_COPY_ON_WRITE /*writes should not change file*/ |
                          MAP_FILE_IMAGE /*image*/);
    if (map != NULL && IF_X64_ELSE(module_is_32bit(map), module_is_64bit(map))) {
        /* XXX i#828: we may eventually support mixed-mode clients.
         * Xref dr_load_aux_x64_library() and load_library_64().
         */
        /* XXX i#147: Should we try some path substs like s/lib32/lib64/?
         * Maybe it's better to error out to avoid loading some unintended lib.
         */
        SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_WRONG_BITWIDTH, 3, get_application_name(),
               get_application_pid(), filename);
        return NULL;
    }
#ifdef X64
    if (TEST(MODLOAD_REACHABLE, flags)) {
        bool reloc = module_file_relocatable(map);
        (*unmap_func)(map, *size);
        map = NULL;
        if (!reloc) {
            os_close(fd);
            return NULL; /* failed */
        }
        /* Re-map with MAP_FILE_REACHABLE */
        map = (*map_func)(fd, size, 0 /*offs*/, NULL,
                          MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC,
                          MAP_FILE_COPY_ON_WRITE /*writes should not change file*/ |
                              MAP_FILE_IMAGE /*image*/ | MAP_FILE_REACHABLE);
        DOCHECK(1, {
            if (map != NULL) {
                byte *region_start = NULL;
                byte *region_end = NULL;
                vmcode_get_reachable_region(&region_start, &region_end);
                ASSERT(map >= region_start && map + *size <= region_end);
            }
        });
    }
#endif
    os_close(fd); /* no longer needed */
    fd = INVALID_FILE;
    if (map == NULL)
        return NULL; /* failed */

    pref = get_module_preferred_base(map);
    if (pref != map) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: relocating from " PFX " to " PFX "\n",
            __FUNCTION__, pref, map);
        if (!module_file_relocatable(map)) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: module not relocatable\n", __FUNCTION__);
            (*unmap_func)(map, *size);
            return NULL;
        }
        if (!module_rebase(map, *size, (map - pref), true /*+w incremental*/)) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to relocate %s\n", __FUNCTION__,
                filename);
            (*unmap_func)(map, *size);
            return NULL;
        }
    }

    return map;
}

static privmod_t *
privload_lookup_locate_and_load(const char *name, privmod_t *name_dependent,
                                privmod_t *load_dependent, bool inc_refcnt,
                                bool reachable)
{
    privmod_t *newmod = NULL;
    const char *toload = name;
    const char *sep = double_strrchr(name, DIRSEP, ALT_DIRSEP);
    size_t rootlen = strlen(systemroot);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (systemroot[0] != '\0' && strncasecmp(name, systemroot, rootlen) == 0 &&
        name[rootlen] != '\0' &&
        strncasecmp(name + rootlen + 1, "sys", strlen("sys")) == 0) {
        /* We want to have system32 match syswow64 for WOW64.
         * We get that effect via dropping the path if it's anywhere
         * under systemroot/sys*.
         * XXX: I'm assuming we don't need to match different paths in general that
         * map to the same file via symlink or junction.  Xref i#1295.
         */
        LOG(GLOBAL, LOG_LOADER, 2, "%s: loading %s instead of %s\n", __FUNCTION__,
            sep + 1, name);
        name = sep + 1;
    }
    if (double_strrchr(name, DIRSEP, ALT_DIRSEP) == NULL) {
        /* only do this mapping when a basename, not a full path, is specified */
        toload = privload_map_name(name, name_dependent);
    }
    newmod = privload_lookup(toload);
    if (newmod == NULL)
        newmod = privload_locate_and_load(toload, load_dependent, reachable);
    else if (inc_refcnt)
        newmod->ref_count++;
    return newmod;
}

/* Does a search for the name, whereas load_private_library() assumes you're
 * passing it the whole path (i#486).
 */
app_pc
privload_load_private_library(const char *name, bool reachable)
{
    privmod_t *newmod;
    app_pc res = NULL;
    acquire_recursive_lock(&privload_lock);
    newmod =
        privload_lookup_locate_and_load(name, NULL, NULL, true /*inc refcnt*/, reachable);
    if (newmod != NULL)
        res = newmod->base;
    release_recursive_lock(&privload_lock);
    return res;
}

void
privload_load_finalized(privmod_t *mod)
{
    DODEBUG({
        if (d_r_get_proc_address(mod->base, DR_DISALLOW_UNSAFE_STATIC_NAME) != NULL)
            disallow_unsafe_static_calls = true;
    });
    if (windbg_cmds_initialized) /* we added libs loaded at init time already */
        privload_add_windbg_cmds_post_init(mod);
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
        LOG(GLOBAL, LOG_LOADER, 2, "%s: error reading imports for %s\n", __FUNCTION__,
            mod->name);
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
        const char *impname = (const char *)RVA_TO_VA(mod->base, imports->Name);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s imports from %s\n", __FUNCTION__, mod->name,
            impname);

        /* FIXME i#233: support bound imports: for now ignoring */
        if (imports->TimeDateStamp == -1) {
            /* Imports are bound via "new bind": need to walk
             * IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT =>
             * IMAGE_BOUND_IMPORT_DESCRIPTOR
             */
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has new bind imports\n", __FUNCTION__,
                mod->name);
        } else if (imports->TimeDateStamp != 0) {
            /* Imports are bound via "old bind" */
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has old bind imports\n", __FUNCTION__,
                mod->name);
        }

        impmod = privload_lookup_locate_and_load(impname, mod, mod, true /*inc refcnt*/,
                                                 false /*=> true if in client/ext dir*/);
        if (impmod == NULL) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: unable to load import lib %s\n", __FUNCTION__,
                impname);
            return false;
        }
        /* i#852: identify all libs that import from DR as client libs */
        if (impmod->base == get_dynamorio_dll_start())
            mod->is_client = true;

        /* walk the lookup table and address table in lockstep */
        /* FIXME: should check readability: if had no-dcontext try (i#350) could just
         * do try/except around whole thing
         */
        lookup = (IMAGE_THUNK_DATA *)RVA_TO_VA(mod->base, imports->OriginalFirstThunk);
        address = (IMAGE_THUNK_DATA *)RVA_TO_VA(mod->base, imports->FirstThunk);
        iat = (app_pc)address;
        if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE, PAGE_READWRITE,
                                    &orig_prot))
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
                if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE, orig_prot,
                                            &orig_prot))
                    return false;
                iat = (app_pc)address;
                if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                            PAGE_READWRITE, &orig_prot))
                    return false;
            }
        }
        if (!protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE, orig_prot,
                                    &orig_prot))
            return false;

        imports++;
        ASSERT((app_pc)(imports + 1) <= imports_end);
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
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mod->base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(mod->base + dos->e_lfanew);
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
    *imports = (IMAGE_IMPORT_DESCRIPTOR *)RVA_TO_VA(mod->base, dir->VirtualAddress);
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
privload_process_one_import(privmod_t *mod, privmod_t *impmod, IMAGE_THUNK_DATA *lookup,
                            app_pc *address)
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
        DWORD ord = (DWORD)(lookup->u1.AddressOfData & ~(IMAGE_ORDINAL_FLAG));
        func = get_proc_address_by_ordinal(impmod->base, ord, &forwarder);
        impfunc = "<ordinal>";
    } else {
        /* import by name */
        IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)RVA_TO_VA(
            mod->base, (lookup->u1.AddressOfData & ~(IMAGE_ORDINAL_FLAG)));
        /* FIXME optimization i#233:
         * - try name->Hint first
         * - build hashtables for quick lookup instead of repeatedly walking
         *   export tables
         */
        /* expensive to check is_readable for name: really we want no-dcxt try (i#350) */
        func = get_proc_address_ex(impmod->base, (const char *)name->Name, &forwarder);
        /* set to first-level names for use below in case no forwarder */
        forwfunc = (const char *)name->Name;
        impfunc = forwfunc;
    }
    /* loop to handle sequence of forwarders */
    while (func == NULL) {
        if (forwarder == NULL) {
            /* there's a syslog in loader_init() but we want to provide the symbol */
            char msg[MAXIMUM_PATH * 2];
            snprintf(msg, BUFFER_SIZE_ELEMENTS(msg), "import %s not found in ",
                     impfunc); /* name is subsequent arg */
            NULL_TERMINATE_BUFFER(msg);
            SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4, get_application_name(),
                   get_application_pid(), msg, impmod->name);
            LOG(GLOBAL, LOG_LOADER, 1, "%s: import %s not found in %s\n", __FUNCTION__,
                impfunc, impmod->name);
            return false;
        }
        forwfunc = strchr(forwarder, '.') + 1;
        /* XXX: forwarder string constraints are not documented and
         * all I've seen look like this: "NTDLL.RtlAllocateHeap".
         * so I've never seen a full filename or path.
         * but there could still be extra dots somewhere: watch for them.
         */
        if (forwfunc == (char *)(ptr_int_t)1 || strchr(forwfunc + 1, '.') != NULL) {
            CLIENT_ASSERT(false, "unexpected forwarder string");
            return false;
        }
        if (forwfunc - forwarder + strlen("dll") >= BUFFER_SIZE_ELEMENTS(forwmodpath)) {
            ASSERT_NOT_REACHED();
            LOG(GLOBAL, LOG_LOADER, 1, "%s: import string %s too long\n", __FUNCTION__,
                forwarder);
            return false;
        }
        /* we use static buffer: may be clobbered by recursion below */
        snprintf(forwmodpath, forwfunc - forwarder, "%s", forwarder);
        snprintf(forwmodpath + (forwfunc - forwarder), strlen("dll"), "dll");
        forwmodpath[forwfunc - 1 /*'.'*/ - forwarder + strlen(".dll")] = '\0';
        LOG(GLOBAL, LOG_LOADER, 2, "\tforwarder %s => %s %s\n", forwarder, forwmodpath,
            forwfunc);
        last_forwmod = forwmod;
        /* don't use forwmodpath past here: recursion may clobber it */
        /* XXX: should inc ref count: but then need to walk individual imports
         * and dec on unload.  For now risking it.
         */
        forwmod = privload_lookup_locate_and_load(
            forwmodpath, last_forwmod == NULL ? mod : last_forwmod, mod,
            false /*!inc refcnt*/, false /*=> true if in client/ext dir*/);
        if (forwmod == NULL) {
            LOG(GLOBAL, LOG_LOADER, 1,
                "%s: unable to load forwarder for %s\n" __FUNCTION__, forwarder);
            return false;
        }
        /* XXX i#1870: we've seen ordinals listed as "libname.#nnn",
         * e.g. "SHUNIMPL.#210".  We should add support for that.
         */
        /* should be listed as import; don't want to inc ref count on each forw */
        func = get_proc_address_ex(forwmod->base, forwfunc, &forwarder);
    }
    /* write result into IAT */
    LOG(GLOBAL, LOG_LOADER, 2, "\timport %s @ " PFX " => IAT " PFX "\n", impfunc, func,
        address);
    if (forwfunc != NULL) {
        /* XXX i#233: support redirecting when imported by ordinal */
        dst = privload_redirect_imports(forwmod, forwfunc, mod);
        DOLOG(2, LOG_LOADER, {
            if (dst != NULL)
                LOG(GLOBAL, LOG_LOADER, 2, "\tredirect => " PFX "\n", dst);
        });
    }
    if (dst == NULL)
        dst = (app_pc)func;
    *address = dst;
    return true;
}

bool
privload_call_entry(dcontext_t *dcontext, privmod_t *privmod, uint reason)
{
    LOG(THREAD, LOG_LOADER, 3, "%s for %s: reason=%d\n", __FUNCTION__, privmod->name,
        reason);
    bool return_val = true;
    bool call_routines = true;
    if (reason == DLL_THREAD_EXIT) {
        /* Only call privlib routines if we're cleaning up the currently executing
         * thread, as that's what the routines are going to do!  Calling on other
         * threads results in problems like double frees (i#969).  Exiting another
         * thread should only happen on process exit or forced thread termination.
         * The former can technically continue (app could call NtTerminateProcess(0)
         * but then keep going) but we have never seen that; and the latter doesn't
         * do full native cleanups anyway.  Thus we're not worried about leaks from
         * not calling DLL_THREAD_EXIT.  (We can't check
         * get_thread_private_dcontext() b/c it's already cleared.)
         */
        call_routines = dcontext->owning_thread == d_r_get_thread_id();
    }
    /* First, call the static-TLS callbacks. */
    os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;
    /* We must set up the thread's TLS first.
     * For the first thread's we need to set it up before the process init call
     * (which means we need loader_init() to be *after* dynamo_thread_init()).
     */
    if (opd->tls_size > 0 && (reason == DLL_PROCESS_INIT || reason == DLL_THREAD_INIT)) {
        /* Set up the TLS. */
        ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
        byte **tls_array = (byte **)dcontext->priv_static_tls;
        ASSERT(tls_array_count > opd->tls_idx && tls_array != NULL);
        if (tls_array[opd->tls_idx] != NULL) {
            /* Initial thread already set up by process-init call. */
            ASSERT(reason == DLL_THREAD_INIT);
        } else {
            byte *data = global_heap_alloc(opd->tls_size HEAPACCT(ACCT_OTHER));
            memcpy(data, opd->tls_init_data, opd->tls_size);
            LOG(THREAD, LOG_LOADER, 2,
                "Initialized %zd bytes @" PFX " for TLS idx=%d in %s\n", opd->tls_size,
                data, opd->tls_idx, privmod->name);
            tls_array[opd->tls_idx] = data;
        }
    }
    for (int i = 0; call_routines && i < opd->tls_callback_count; ++i) {
        LOG(GLOBAL, LOG_LOADER, 2,
            "%s: calling TLS callback #%d for %s @" PFX " for reason %d\n", __FUNCTION__,
            i, privmod->name, opd->tls_callbacks[i], reason);
        TRY_EXCEPT_ALLOW_NO_DCONTEXT(
            dcontext, { (*opd->tls_callbacks[i])((HANDLE)privmod->base, reason, NULL); },
            { /* EXCEPT */
              SYSLOG_INTERNAL_WARNING("TLS callback " PFX " in %s crashed!",
                                      opd->tls_callbacks[i], privmod->name);
            });
    }
    /* Then, call the module entry point. */
    app_pc entry = get_module_entry(privmod->base);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* get_module_entry adds base => returns base instead of NULL */
    if (call_routines && entry != NULL && entry != privmod->base &&
        /* We call the TLS routines for the externally_loaded executable for
         * static DR, but we never want to call the main entry for externally_loaded..
         */
        !privmod->externally_loaded) {
        dllmain_t func = (dllmain_t)convert_data_to_function(entry);
        BOOL res = FALSE;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: calling %s entry " PFX " for %d\n", __FUNCTION__,
            privmod->name, entry, reason);

        if (get_os_version() >= WINDOWS_VERSION_8 &&
            str_case_prefix(privmod->name, "kernelbase")) {
            /* XXX i#915: win8 kernelbase entry fails on initial csrss setup.
             * Xref i#364, i#440.
             * We can ignore and continue for at least small apps, and
             * so far we have not seen problems on larger apps.
             */
        }

        TRY_EXCEPT_ALLOW_NO_DCONTEXT(
            dcontext, { res = (*func)((HANDLE)privmod->base, reason, NULL); },
            { /* EXCEPT */
              LOG(GLOBAL, LOG_LOADER, 1, "%s: %s init routine " PFX " crashed!\n",
                  __FUNCTION__, privmod->name, func);
              res = FALSE;
            });

        if (!res && get_os_version() >= WINDOWS_VERSION_7 &&
            /* i#364: win7 _BaseDllInitialize fails to initialize a new console
             * (0xc0000041 (3221225537) - The NtConnectPort request is refused)
             * which we ignore for now.  DR always had trouble writing to the
             * console anyway (xref i#261).
             * Update: for i#440, this should now succeed, but we leave this
             * in place just in case.
             */
            (str_case_prefix(privmod->name, "kernel") ||
             /* i#2221: combase's entry fails on win10.  So far ignoring it
              * hasn't cause any problems with simple clients.
              */
             str_case_prefix(privmod->name, "combase"))) {
            LOG(GLOBAL, LOG_LOADER, 1, "%s: ignoring failure of %s entry\n", __FUNCTION__,
                privmod->name);
            res = TRUE;
        }
        return_val = CAST_TO_bool(res);
    }
    if (reason == DLL_THREAD_EXIT && opd->tls_size > 0) {
        /* Free the TLS. */
        ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
        byte **tls_array = (byte **)dcontext->priv_static_tls;
        ASSERT(tls_array_count > opd->tls_idx && tls_array != NULL);
        LOG(THREAD, LOG_LOADER, 2, "Freeing %zd bytes @" PFX " for TLS idx=%d in %s\n",
            opd->tls_size, tls_array[opd->tls_idx], opd->tls_idx, privmod->name);
        global_heap_free(tls_array[opd->tls_idx], opd->tls_size HEAPACCT(ACCT_OTHER));
        tls_array[opd->tls_idx] = NULL;
    }
    if (reason == DLL_PROCESS_EXIT && privmod->externally_loaded) {
        /* This happens for the executable for static DR, for which
         * privload_remove_areas() will not be called.
         */
        privload_free_opd(privmod);
    }
    return return_val;
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
    if (str_case_prefix(name, "API-MS-Win-Core-APIQuery-L1") ||
        str_case_prefix(name, "API-MS-Win-Core-CRT-L1"))
        return "ntdll.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Console-L1"))
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
        if (dependent != NULL && str_case_prefix(dependent->name, "kernel32"))
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
    else if (str_case_prefix(name, "API-MS-Win-Core-ProcessThreads-L1") ||
             str_case_prefix(name, "API-MS-Win-Core-AppInit-L1-1")) {
        /* This one includes CreateProcessAsUserW which is only in
         * kernel32, but kernel32 itself imports from here and its must come
         * from kernelbase to avoid infinite loop.  XXX: see above: seeming
         * more and more like it depends on what's imported.
         */
        if (dependent != NULL && str_case_prefix(dependent->name, "kernel32"))
            return "kernelbase.dll";
        else
            return "kernel32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-Profile-L1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-RTLSupport-L1")) {
        if (get_os_version() >= WINDOWS_VERSION_8 ||
            (dependent != NULL && str_case_prefix(dependent->name, "kernel")))
            return "ntdll.dll";
        else
            return "kernel32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-String-L1"))
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
    /**************************************************/
    /* Added in Win8 */
    else if (str_case_prefix(name, "API-MS-Win-Core-Kernel32-Legacy-L1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Appcompat-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-BEM-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Comm-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Console-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-CRT-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Job-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Localization-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Localization-Private-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Namespace-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Normalization-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-ProcessTopology-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Psapi-Ansi-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Psapi-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Psapi-Obsolete-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Realtime-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Registry-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-SideBySide-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-String-Obsolete-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-SystemTopology-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Threadpool-Legacy-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Threadpool-Private-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Timezone-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-WOW64-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-WindowsErrorReporting-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Appcontainer-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Base-Private-L1-1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-File-L2-1")) {
        /* i#2658: In kernel32 on win7 but kernelbase on win8+. */
        if (get_os_version() <= WINDOWS_VERSION_7)
            return "kernel32.dll";
        else
            return "kernelbase.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-Heap-Obsolete-L1-1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Service-Private-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Audit-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Capability-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Credentials-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Security-Credentials-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Security-LSAPolicy-L1"))
        return "sechost.dll";
    else if (str_case_prefix(name, "API-MS-Win-Eventing-Controller-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Eventing-Consumer-L1-1")) {
        /* i#1528: moved to sechost.dll on win8.1 */
        if (get_os_version() >= WINDOWS_VERSION_8_1)
            return "sechost.dll";
        else
            return "kernelbase.dll";
    }
    /**************************************************/
    /* Added in Win8.1 */
    else if (str_case_prefix(name, "API-MS-Win-Core-ProcessTopology-L1-2") ||
             str_case_prefix(name, "API-MS-Win-Core-XState-L2-1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-Registry-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Eventing-ClassicProvider-L1-1"))
        return "advapi32.dll";
    /**************************************************/
    /* Added in Win10 (some may be 8.1 too) */
    else if (str_case_prefix(name, "API-MS-Win-Core-Console-L2-2") ||
             str_case_prefix(name, "API-MS-Win-Core-Console-L3-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Console-L3-2") ||
             str_case_prefix(name, "API-MS-Win-Core-Enclave-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Fibers-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Heap-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-LargeInteger-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-LibraryLoader-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Localization-Obsolete-L1-2") ||
             str_case_prefix(name, "API-MS-Win-Core-Localization-Obsolete-L1-3") ||
             str_case_prefix(name, "API-MS-Win-Core-Path-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Pcw-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-PerfCounters-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-ProcessSnapshot-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Psm-Key-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Quirks-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-RegistryUserSpecific-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-SHLWAPI-Legacy-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-SHLWAPI-Obsolete-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-SHLWAPI-Obsolete-L1-2") ||
             str_case_prefix(name, "API-MS-Win-Core-String-L2-1") ||
             str_case_prefix(name, "API-MS-Win-Core-StringAnsi-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-URL-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Version-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Version-Private-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-VersionAnsi-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Eventing-Provider-L1-1"))
        return "kernelbase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-PrivateProfile-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Atoms-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Core-Job-L2-1") ||
             str_case_prefix(name, "API-MS-Win-DownLevel-Kernel32-L2-1"))
        return "kernel32.dll";
    else if (str_case_prefix(name, "API-MS-Win-Core-WinRT-Error-L1-1"))
        return "combase.dll";
    else if (str_case_prefix(name, "API-MS-Win-Appmodel-Runtime-L1-1") ||
             str_case_prefix(name, "API-MS-Win-Appmodel-State-L1-2"))
        return "kernel.appcore.dll";
    else if (str_case_prefix(name, "API-MS-Win-GDI-")) {
        /* We've seen many different GDI-* */
        return "gdi32full.dll";
    } else if (str_case_prefix(name, "API-MS-Win-CRT-")) {
        /* We've seen CRT-{String,Runtime,Private} */
        return "ucrtbase.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-COM-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Core-COM-Private-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Core-COM-Private-L1-2") ||
               str_case_prefix(name, "API-MS-Win-Core-Com-MidlProxyStub-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Core-WinRT-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Core-WinRT-String-L1-1")) {
        return "combase.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Core-Kernel32-Private-L1-1")) {
        return "kernel32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Shell-Shellcom-L1-1")) {
        return "kernelbase.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Stateseparation-Helpers-L1-1")) {
        return "kernelbase.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Shell-ShellFolders-L1-1")) {
        /* Moved to windows.storage.dll on Windows 10 */
        if (get_os_version() >= WINDOWS_VERSION_10) {
            return "windows.storage.dll";
        } else {
            return "kernelbase.dll";
        }
    } else if (str_case_prefix(name, "API-MS-Win-Devices-Config-L1-1")) {
        return "cfgmgr32.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Shcore-")) {
        /* There are a lot of these, and they all seem to redirect to shcore.dll */
        return "shcore.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Storage-Exports-Internal-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Storage-Exports-External-L1-1")) {
        return "windows.storage.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Security-Cryptoapi-L1-1")) {
        return "cryptsp.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Shlwapi-WinRT-Storage-L1-1") ||
               str_case_prefix(name, "API-MS-Win-Shlwapi-WinRT-IE-L1-1")) {
        return "shlwapi.dll";
    } else if (str_case_prefix(name, "API-MS-Win-Power-Base-L1-1")) {
        return "powrprof.dll";
    } else if (str_case_prefix(name, "Ext-MS-Win-NtUser-") ||
               str_case_prefix(name, "API-MS-Win-RtCore-NtUser-")) {
        return "user32.dll";
    } else {
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
        (str_case_prefix(impname, "API-MS-Win-") ||
         str_case_prefix(impname, "Ext-MS-Win-"))) {
        IF_DEBUG(const char *apiname = impname;)
        /* We need immediate dependent to avoid infinite chain when hit
         * kernel32 OpenProcessToken forwarder which needs to forward
         * to kernelbase
         */
        impname = map_api_set_dll(impname, immed_dep);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: mapped API-set dll %s to %s\n", __FUNCTION__,
            apiname, impname);
        return impname;
    }
    return impname;
}

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent, bool reachable)
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

    /* We may be passed a full path.  We check for a separator to avoid finding
     * the library w/ just a basename (i#1768) which results in us not having
     * the path in our data structures.
     */
    if (double_strrchr(impname, DIRSEP, ALT_DIRSEP) != NULL &&
        os_file_exists(impname, false /*!is_dir*/)) {
        mod = privload_load(impname, dependent, reachable);
        return mod; /* if fails to load, don't keep searching */
    }

    /* 1) client lib dir(s) and Extensions dir */
    for (i = 0; i < search_paths_idx; i++) {
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/%s", search_paths[i],
                 impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false /*!is_dir*/)) {
            mod = privload_load(modpath, dependent, true /*always reachable*/);
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
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/system32/%s", systemroot,
                 impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false /*!is_dir*/)) {
            mod = privload_load(modpath, dependent, reachable);
            return mod; /* if fails to load, don't keep searching */
        }
        /* 4) windows dir */
        snprintf(modpath, BUFFER_SIZE_ELEMENTS(modpath), "%s/%s", systemroot, impname);
        NULL_TERMINATE_BUFFER(modpath);
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, modpath);
        if (os_file_exists(modpath, false /*!is_dir*/)) {
            mod = privload_load(modpath, dependent, reachable);
            return mod; /* if fails to load, don't keep searching */
        }
    }
    /* 5) dirs on PATH: FIXME: not supported yet */

    if (mod == NULL) {
        /* There's a SYSLOG in loader_init(), but we want the name of the missing
         * library.  If we end up using this loading code for cases where we
         * expect failure, we could switch to a global missing_lib[] that we
         * can write to and have loader_init() use to add to its message.
         */
        SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4, get_application_name(),
               get_application_pid(), impname, "\n\tCannot find library");
    }
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
    value_result = reg_query_value(DIAGNOSTICS_OS_REG_KEY, DIAGNOSTICS_SYSTEMROOT_REG_KEY,
                                   KeyValueFullInformation, &diagnostic_value_info,
                                   sizeof(diagnostic_value_info), 0);
    if (value_result == REG_QUERY_SUCCESS) {
        snprintf(systemroot, BUFFER_SIZE_ELEMENTS(systemroot), "%S",
                 (wchar_t *)(diagnostic_value_info.NameAndData +
                             diagnostic_value_info.DataOffset -
                             DECREMENT_FOR_DATA_OFFSET));
        NULL_TERMINATE_BUFFER(systemroot);
    } else
        ASSERT_NOT_REACHED();
}

/* i#440: win7 private kernel32 tries to init the console w/ csrss and
 * not only gets back an error, but csrss then refuses any future
 * console ops from either DR or the app itself!
 * Our workaround here is to disable the ConnectConsoleInternal call
 * from the private kernel32.  We leave the rest alone, and it
 * seems to work: or at least, printing to the console works for
 * both the app and the private kernel32.
 */
static bool
privload_disable_console_init(privmod_t *mod)
{
    app_pc pc, entry, prev_pc, protect;
    instr_t instr;
    dcontext_t *dcontext = GLOBAL_DCONTEXT;
    bool prev_marks_call = false;
    bool success = false;
    app_pc push1 = NULL, push2 = NULL, push3 = NULL;
    uint orig_prot, count = 0;
    static const uint MAX_DECODE = IF_X64_ELSE(1200, 1024);
    static const uint MAX_INSTR_COUNT = 1024;

    ASSERT(mod != NULL);
    ASSERT(strcasecmp(mod->name, "kernel32.dll") == 0);
    /* win8 does not need this fix (i#911) */
    if (get_os_version() != WINDOWS_VERSION_7)
        return true; /* nothing to do */

    /* We want to turn the call to ConnectConsoleInternal from ConDllInitialize,
     * which is called from the entry routine _BaseDllInitialize,
     * into a nop.  Unfortunately none of these are exported.  We rely on the
     * fact that the 1st param to the first ConDllInitialize call (not actually
     * the one we care about, but same callee) is 2 (DLL_THREAD_ATTACH):
     *    kernel32!_BaseDllInitialize+0x8a:
     *    53               push    ebx
     *    6a02             push    0x2
     *    e83c000000       call    kernel32!ConDllInitialize (75043683)
     */
    instr_init(dcontext, &instr);
    entry = get_module_entry(mod->base);
    for (pc = entry; pc < entry + MAX_DECODE;) {
        if (count++ > MAX_INSTR_COUNT)
            break; /* bail */
        instr_reset(dcontext, &instr);
        prev_pc = pc;
        pc = decode(dcontext, pc, &instr);
        if (!instr_valid(&instr) || instr_is_return(&instr))
            break; /* bail */
        /* follow direct jumps.  MAX_INSTR_COUNT avoids infinite loop on backward jmp. */
        if (instr_is_ubr(&instr)) {
            pc = opnd_get_pc(instr_get_target(&instr));
            continue;
        }
#ifdef X64
        /* For x64 we don't have a very good way to identify.  There is no
         * ConDllInitialize, but the call to ConnectConsoleInternal from
         * BaseDllInitialize is preceded by a rip-rel mov rax to memory.
         */
        if (instr_get_opcode(&instr) == OP_mov_st &&
            opnd_is_reg(instr_get_src(&instr, 0)) &&
            opnd_get_reg(instr_get_src(&instr, 0)) == REG_RAX &&
            opnd_is_rel_addr(instr_get_dst(&instr, 0))) {
            prev_marks_call = true;
            continue;
        }
#else
        if (instr_get_opcode(&instr) == OP_push_imm &&
            opnd_get_immed_int(instr_get_src(&instr, 0)) == DLL_THREAD_ATTACH) {
            prev_marks_call = true;
            continue;
        }
#endif
        if (prev_marks_call && instr_get_opcode(&instr) == OP_call) {
            /* For 32-bit we need to continue scanning.  For 64-bit we're there. */
#ifdef X64
            protect = prev_pc;
#else
            app_pc tgt = opnd_get_pc(instr_get_target(&instr));
            uint prev_lea = 0;
            bool first_jcc = false;
            /* Now we're in ConDllInitialize.  The call to ConnectConsoleInternal
             * has several lea's in front of it:
             *
             *   8d85d7f9ffff     lea     eax,[ebp-0x629]
             *   50               push    eax
             *   8d85d8f9ffff     lea     eax,[ebp-0x628]
             *   50               push    eax
             *   56               push    esi
             *   e84c000000    call KERNEL32_620000!ConnectConsoleInternal (00636582)
             *
             * Unfortunately ConDllInitialize is not straight-line code.
             * For now we follow the first je which is a little fragile.
             * XXX: build full CFG.
             */
            for (pc = tgt; pc < tgt + MAX_DECODE;) {
                if (count++ > MAX_INSTR_COUNT)
                    break; /* bail */
                instr_reset(dcontext, &instr);
                prev_pc = pc;
                pc = decode(dcontext, pc, &instr);
                if (!instr_valid(&instr) || instr_is_return(&instr))
                    break; /* bail */
                if (!first_jcc && instr_is_cbr(&instr)) {
                    /* See above: a fragile hack to get to rest of routine */
                    tgt = opnd_get_pc(instr_get_target(&instr));
                    pc = tgt;
                    first_jcc = true;
                    continue;
                }
                /* Follow direct jumps, which is required on win7x86 (i#556c#5).
                 * MAX_INSTR_COUNT avoids infinite loop on backward jmp.
                 */
                if (instr_is_ubr(&instr)) {
                    pc = opnd_get_pc(instr_get_target(&instr));
                    continue;
                }
                if (instr_get_opcode(&instr) == OP_lea &&
                    opnd_is_base_disp(instr_get_src(&instr, 0)) &&
                    opnd_get_disp(instr_get_src(&instr, 0)) < -0x400) {
                    prev_lea++;
                    continue;
                }
                if (prev_lea >= 2 && instr_get_opcode(&instr) == OP_call) {
                    protect = push1;
#endif
            /* found a call preceded by a large lea and maybe some pushes.
             * replace the call:
             *   e84c000000    call KERNEL32_620000!ConnectConsoleInternal
             * =>
             *   b801000000    mov eax,0x1
             * and change the 3 pushes to nops (this is stdcall).
             */
            /* 2 pages in case our code crosses a page */
            if (!protect_virtual_memory((void *)PAGE_START(protect), PAGE_SIZE * 2,
                                        PAGE_READWRITE, &orig_prot))
                break; /* bail */
            if (push1 != NULL)
                *push1 = RAW_OPCODE_nop;
            if (push2 != NULL)
                *push2 = RAW_OPCODE_nop;
            if (push3 != NULL)
                *push3 = RAW_OPCODE_nop;
            *(prev_pc) = MOV_IMM2XAX_OPCODE;
            *((uint *)(prev_pc + 1)) = 1;
            protect_virtual_memory((void *)PAGE_START(protect), PAGE_SIZE * 2, orig_prot,
                                   &orig_prot);
            success = true;
            break; /* done */
#ifndef X64
        }
        if (prev_lea > 0) {
            if (instr_get_opcode(&instr) == OP_push) {
                if (push1 != NULL) {
                    if (push2 != NULL)
                        push3 = prev_pc;
                    else
                        push2 = prev_pc;
                } else
                    push1 = prev_pc;
            } else {
                push1 = push2 = push3 = NULL;
                prev_lea = 0;
            }
        }
    }
    break; /* bailed, or done */
#endif
}
prev_marks_call = false;
}
instr_free(dcontext, &instr);
return success;
}

/* GUI apps are initialized without a console. To enable writing to the console
 * we attach to the parent's console.
 * XXX: if an app attempts to create/attach to a console w/o first freeing itself
 * from this console, it will fail since a process can only associate w/ one console.
 * The solution here would be to monitor such attempts by the app and free the console
 * that is setup here.
 */
typedef BOOL(WINAPI *kernel32_AttachConsole_t)(IN DWORD);
static kernel32_AttachConsole_t kernel32_AttachConsole;

bool
privload_attach_parent_console(app_pc app_kernel32)
{
    ASSERT(app_kernel32 != NULL);
    if (kernel32_AttachConsole == NULL) {
        kernel32_AttachConsole =
            (kernel32_AttachConsole_t)d_r_get_proc_address(app_kernel32, "AttachConsole");
    }
    if (kernel32_AttachConsole != NULL) {
        if (kernel32_AttachConsole(ATTACH_PARENT_PROCESS) != 0)
            return true;
    }
    return false;
}

/* i#556: A process can be associated with only one console, which is why the call to
 * ConnectConsoleInternal is nop'd out for win7. With the call disabled priv kernel32
 * does not initialize globals needed for console support. This routine will share the
 * real kernel32's ConsoleLpcHandle, ConsolePortHeap, and ConsolePortMemoryRemoteDelta
 * with private kernel32. This will enable console support for 32-bit kernel and
 * 64-bit apps.
 */
typedef BOOL(WINAPI *kernel32_FreeConsole_t)(VOID);
static kernel32_FreeConsole_t kernel32_FreeConsole;

bool
privload_console_share(app_pc priv_kernel32, app_pc app_kernel32)
{
    app_pc pc;
    instr_t instr;
    dcontext_t *dcontext = GLOBAL_DCONTEXT;
    bool success = false;
    size_t console_handle_diff, console_heap_diff, console_delta_diff;
    app_pc console_handle = NULL, console_heap = NULL, console_delta = NULL;
    app_pc get_console_cp;
    BOOL status = false;
    static const uint MAX_DECODE = 1024;

    ASSERT(app_kernel32 != NULL);
    if (get_own_peb()->ImageSubsystem != IMAGE_SUBSYSTEM_WINDOWS_CUI) {
        /* On win8+, if private kernelbase is loaded after calling dr_using_console, its
         * init routine will call ConsoleInitalize and check if the app is a console. If
         * it's not a console, it will close all handles with ConsoleCloseIfConsoleHandle.
         * To enable console printing for gui apps, we simply detach and reattach.
         */
        if (get_os_version() >= WINDOWS_VERSION_8) {
            kernel32_FreeConsole =
                (kernel32_FreeConsole_t)d_r_get_proc_address(app_kernel32, "FreeConsole");
            if (kernel32_FreeConsole != NULL) {
                if (kernel32_FreeConsole() == 0)
                    return false;
            }
        }
        if (privload_attach_parent_console(app_kernel32) == false)
            return false;
    }
    /* xref i#440: Noping out the call to ConsoleConnectInternal is enough to get console
     * support for wow64. We have this check after in case of GUI app.
     */
    if (is_wow64_process(NT_CURRENT_PROCESS)) {
        return true;
    }

    /* Below here is win7-specific */
    if (get_os_version() != WINDOWS_VERSION_7)
        return true;
    get_console_cp = (app_pc)d_r_get_proc_address(app_kernel32, "GetConsoleCP");
    ASSERT(get_console_cp != NULL);
    /* No exported routines directly reference the globals. The easiest and shortest
     * path is through GetConsoleCP, where we look for a call to ConsoleClientCallServer
     * which then references ConsoleLpcHandle and ConsolePortMemoryRemoteDelta.
     * ConsolePortHeap seems to always precede the remote delta global by the size of a
     * pointer in memory. Tested on win7 x86 and x64.
     */
    instr_init(dcontext, &instr);
    for (pc = get_console_cp; pc < get_console_cp + MAX_DECODE;) {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        if (!instr_valid(&instr) || instr_is_return(&instr))
            break; /* bail */
        if (instr_get_opcode(&instr) == OP_call) {
            app_pc tgt = opnd_get_pc(instr_get_target(&instr));
            /* Now we're in ConsoleClientCallServer. ConsoleLpcHandle is referenced
             * right away w/ a cmp. From there, we fall through on first je and then
             * follow a jnz where ConsolePortMemoryRemoteDelta is referenced after.
             */
            for (pc = tgt; pc < tgt + MAX_DECODE;) {
                instr_reset(dcontext, &instr);
                pc = decode(dcontext, pc, &instr);
                if (!instr_valid(&instr) || instr_is_return(&instr))
                    break; /* bail */
                /* kernel32!ConsoleClientCallServer:
                 * 8bff            mov     edi,edi
                 * 55              push    ebp
                 * 8bec            mov     ebp,esp
                 * 833da067a47500  cmp     dword ptr [kernel32!ConsoleLpcHandle],0
                 * 0f8415a1feff    je      kernel32!ConsoleClientCallServer+0xe
                 */
                if (instr_get_opcode(&instr) == OP_cmp &&
#ifdef X64
                    opnd_is_rel_addr(instr_get_src(&instr, 0)) &&
#else
                            opnd_is_abs_addr(instr_get_src(&instr, 0)) &&
#endif
                    opnd_is_immed_int(instr_get_src(&instr, 1)) &&
                    opnd_get_immed_int(instr_get_src(&instr, 1)) == 0) {
                    console_handle = (app_pc)opnd_get_addr(instr_get_src(&instr, 0));
                    continue;
                }
                if (instr_get_opcode(&instr) == OP_jnz) {
                    pc = opnd_get_pc(instr_get_target(&instr));
                    /* First instruction following the jnz is a mov which references
                     * ConsolePortMemoryRemoteDelta as src.
                     *
                     * 85ff         test edi,edi
                     * 0f8564710000 jne kernel32!ConsoleClientCallServer+0x49 (759dbcb4)
                     *
                     * kernel32!ConsoleClientCallServer+0x49:
                     * a18465a475 mov eax,dword ptr[kernel32!ConsolePortMemoryRemoteDelta]
                     */
                    instr_reset(dcontext, &instr);
                    pc = decode(dcontext, pc, &instr);
                    if (!instr_valid(&instr))
                        break; /* bail */
                    if (instr_get_opcode(&instr) == OP_mov_ld &&
#ifdef X64
                        opnd_is_rel_addr(instr_get_src(&instr, 0))) {
#else
                                opnd_is_abs_addr(instr_get_src(&instr, 0))) {
#endif
                        console_delta = (app_pc)opnd_get_addr(instr_get_src(&instr, 0));
                        success = true;
                    }
                    break; /* done */
                }
            }
            break; /* bailed, or done */
        }
    }
    /* If we have successfully retrieved the addr to each global from app's kernel32, we
     * now share the values with private kernel32.
     * XXX: right now we calculate delta from base of kernel32 to each global. We should
     * add a checksum to ensure app's kernel32 is same as private kernel32.
     */
    if (success) {
        console_handle_diff = console_handle - app_kernel32;
        console_delta_diff = console_delta - app_kernel32;
        console_heap_diff = console_delta_diff - sizeof(PVOID);

        if (!safe_write_ex(priv_kernel32 + console_handle_diff, sizeof(PHANDLE),
                           console_handle, NULL) ||
            !safe_write_ex(priv_kernel32 + console_delta_diff, sizeof(ULONG_PTR),
                           console_delta, NULL) ||
            !safe_write_ex(priv_kernel32 + console_heap_diff, sizeof(PVOID),
                           app_kernel32 + console_heap_diff, NULL))
            success = false;
    }
    instr_free(dcontext, &instr);
    return success;
}

/* Rather than statically linking to real kernel32 we want to invoke
 * routines in the private kernel32
 */
void
privload_redirect_setup(privmod_t *mod)
{
    drwinapi_onload(mod);

    if (strcasecmp(mod->name, "kernel32.dll") == 0) {
        if (privload_disable_console_init(mod))
            LOG(GLOBAL, LOG_LOADER, 2, "%s: fixed console setup\n", __FUNCTION__);
        else /* we want to know about it: may well happen in future version of dll */
            SYSLOG_INTERNAL_WARNING("unable to fix console setup");
    }
}

/* XXX i#1298: we may also want to support redirecting routines via hook instead
 * of only via imports, to handle cases where priv libs call into their own
 * routines.
 */
static app_pc
privload_redirect_imports(privmod_t *impmod, const char *name, privmod_t *importer)
{
    return drwinapi_redirect_imports(impmod, name, importer);
}

/* Handles a private-library callback called from interpreted app code.
 * This should no longer happen now that we fully isolate the PEB and FLS,
 * but I'm leaving the mechanism in case we need it in the future.
 */
bool
private_lib_handle_cb(dcontext_t *dcontext, app_pc pc)
{
    return true;
}

/***************************************************************************
 * SECURITY COOKIE
 */

#ifdef X64
#    define SECURITY_COOKIE_INITIAL 0x00002B992DDFA232
#else
#    define SECURITY_COOKIE_INITIAL 0xBB40E64E
#endif
#define SECURITY_COOKIE_16BIT_INITIAL 0xBB40

static ULONG_PTR
get_tick_count(void)
{
    if (ntdll_NtTickCount != NULL) {
        return (*ntdll_NtTickCount)();
    } else {
        /* Pre-win2k3, there is no ntdll!NtTickCount, and kernel32!GetTickCount
         * does a simple computation from KUSER_SHARED_DATA.
         */
        KUSER_SHARED_DATA *kud = (KUSER_SHARED_DATA *)KUSER_SHARED_DATA_ADDRESS;
        ULONG64 val = (ULONG64)kud->TickCountLowDeprecated * kud->TickCountMultiplier;
        return (ULONG_PTR)(val >> 18);
    }
}

static bool
privload_set_security_cookie(privmod_t *mod)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mod->base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(mod->base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_LOAD_CONFIG_DIRECTORY *config;
    ptr_uint_t *cookie_ptr;
    ptr_uint_t cookie;
    uint64 time100ns;
    LARGE_INTEGER perfctr;

    ASSERT(is_readable_pe_base(mod->base));
    ASSERT(dos->e_magic == IMAGE_DOS_SIGNATURE);
    ASSERT(nt != NULL && nt->Signature == IMAGE_NT_SIGNATURE);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG;
    if (dir == NULL || dir->Size <= 0)
        return false;
    config = (IMAGE_LOAD_CONFIG_DIRECTORY *)RVA_TO_VA(mod->base, dir->VirtualAddress);
    if (dir->Size < offsetof(IMAGE_LOAD_CONFIG_DIRECTORY, SecurityCookie) +
            sizeof(config->SecurityCookie)) {
        ASSERT_CURIOSITY(false && "IMAGE_LOAD_CONFIG_DIRECTORY too small");
        return false;
    }
    cookie_ptr = (ptr_uint_t *)config->SecurityCookie;
    if ((byte *)cookie_ptr < mod->base || (byte *)cookie_ptr >= mod->base + mod->size) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s has out-of-bounds cookie @" PFX "\n",
            __FUNCTION__, mod->name, cookie_ptr);
        return false;
    }
    LOG(GLOBAL, LOG_LOADER, 2,
        "%s: %s dirsz=" PIFX " configsz=" PIFX " init cookie=" PFX "\n", __FUNCTION__,
        mod->name, dir->Size, config->Size, *cookie_ptr);
    if (*cookie_ptr != SECURITY_COOKIE_INITIAL &&
        *cookie_ptr != SECURITY_COOKIE_16BIT_INITIAL) {
        /* I'm assuming a cookie should either be the magic value, or zero if
         * no cookie is desired?  Let's have a curiosity to find if there are
         * any other values:
         */
        ASSERT_CURIOSITY(*cookie_ptr == 0);
        return true;
    }

    /* Generate a new cookie using:
     *   SystemTimeHigh ^ SystemTimeLow ^ ProcessId ^ ThreadId ^ TickCount ^
     *   PerformanceCounterHigh ^ PerformanceCounterLow
     */
    time100ns = query_time_100ns();
    /* 64-bit seems to sign-extend so we use ptr_int_t */
    cookie = (ptr_int_t)(time100ns >> 32) ^ (ptr_int_t)time100ns;
    cookie ^= get_process_id();
    cookie ^= d_r_get_thread_id();
    cookie ^= get_tick_count();
    nt_query_performance_counter(&perfctr, NULL);
#ifdef X64
    cookie ^= perfctr.QuadPart;
#else
            cookie ^= perfctr.LowPart;
            cookie ^= perfctr.HighPart;
#endif

    if (*cookie_ptr == SECURITY_COOKIE_16BIT_INITIAL)
        cookie &= 0xffff; /* only want low 16 bits */
    if (cookie == SECURITY_COOKIE_INITIAL || cookie == SECURITY_COOKIE_16BIT_INITIAL) {
        /* If it happens to match, make it not match */
        cookie--;
    }
#ifdef X64
    /* Apparently the top 16 bits should always be 0.
     * XXX: is my algorithm wrong for x64 above?
     */
    cookie &= 0x0000ffffffffffff;
#endif
    LOG(GLOBAL, LOG_LOADER, 2, "  new cookie value: " PFX "\n", cookie);

    *cookie_ptr = cookie;
    return true;
}

void
privload_os_finalize(privmod_t *mod)
{
    /* Libraries built with /GS require us to set
     * IMAGE_LOAD_CONFIG_DIRECTORY.SecurityCookie (i#1093)
     */
    if (!mod->externally_loaded)
        privload_set_security_cookie(mod);

#ifdef STANDALONE_UNIT_TEST
    /* No static TLS support for later-loaded libs used in the test. */
    return;
#endif

    /* Static TLS support. */
    os_privmod_data_t *opd = (os_privmod_data_t *)mod->os_privmod_data;
    if (opd == NULL) {
        /* This happens for the executable for static DR. */
        ASSERT(mod->externally_loaded);
        privload_alloc_opd(mod);
        opd = (os_privmod_data_t *)mod->os_privmod_data;
    }
    void **callbacks;
    int *index;
    byte *data_start;
    byte *data_end;
    if (!module_get_tls_info(mod->base, &callbacks, &index, &data_start, &data_end))
        return;
    LOG(GLOBAL, LOG_LOADER, 2, "%s has static TLS\n", mod->name);

    if (callbacks == NULL || *callbacks == NULL) {
        opd->tls_callbacks = NULL;
        opd->tls_callback_count = 0;
    } else {
        void **cb = callbacks;
        while (*cb != NULL)
            cb++;
        opd->tls_callback_count = (int)(cb - callbacks);
        ASSERT(opd->tls_callback_count > 0);
        opd->tls_callbacks = (PIMAGE_TLS_CALLBACK *)convert_data_to_function(
            HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, PIMAGE_TLS_CALLBACK,
                             opd->tls_callback_count, ACCT_OTHER, PROTECTED));
        memcpy(opd->tls_callbacks, callbacks, (cb - callbacks) * sizeof(*cb));
    }
    LOG(GLOBAL, LOG_LOADER, 2, "%s has %d TLS callbacks\n", mod->name,
        opd->tls_callback_count);

    opd->tls_size = data_end - data_start;
    opd->tls_init_data = data_start;
    LOG(GLOBAL, LOG_LOADER, 2, "%s has %d TLS bytes (init: " PFX ")\n", mod->name,
        opd->tls_size, opd->tls_init_data);

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    opd->tls_idx = tls_next_idx++;
    if (opd->tls_idx >= TLS_ARRAY_MAX_SIZE ||
        (tls_array_count > 0 && opd->tls_idx >= tls_array_count)) {
        /* XXX: It is not easy to resize for all threads.  We do not support for now.
         * If we do add support, we'll have to turn should_swap_teb_static_tls() into
         * a dynamic check in our generated code.
         */
        REPORT_FATAL_ERROR_AND_EXIT(
            PRIVATE_LIBRARY_TLS_LIMIT_CROSSED, 3, get_application_name(),
            get_application_pid(),
            (opd->tls_idx >= tls_array_count)
                ? "Late-loaded libraries with static TLS not supported"
                : "Too many libaries with static TLS");
    }
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    LOG(GLOBAL, LOG_LOADER, 2, "TLS index for %s is %d\n", mod->name, opd->tls_idx);
    *index = opd->tls_idx;
}

/***************************************************************************/
#ifdef WINDOWS
/* i#522: windbg commands for viewing symbols for private libs */

static bool
add_mod_to_drmarker(dr_marker_t *marker, const char *path, const char *modname,
                    byte *base, size_t *sofar)
{
    const char *last_dir = double_strrchr(path, DIRSEP, ALT_DIRSEP);
    int res;
    if (last_dir == NULL) {
        SYSLOG_INTERNAL_WARNING_ONCE("drmarker windbg cmd: invalid path");
        return false;
    }
    /* We have to use .block{} b/c .sympath eats up all chars until the end
     * of the "line", which for as /c is the entire command output, but it
     * does stop at the }.
     * Sample:
     *   .block{.sympath+ c:\src\dr\git\build_x86_dbg\api\samples\bin};
     *   .reload bbcount.dll=74ad0000;.echo "Loaded bbcount.dll";
     *
     */
#    define WINDBG_ADD_PATH ".block{.sympath+ "
    if (*sofar + strlen(WINDBG_ADD_PATH) + (last_dir - path) < WINDBG_CMD_MAX_LEN) {
        res = _snprintf(marker->windbg_cmds + *sofar,
                        strlen(WINDBG_ADD_PATH) + last_dir - path, "%s%s",
                        WINDBG_ADD_PATH, path);
        ASSERT(res == -1);
        *sofar += strlen(WINDBG_ADD_PATH) + last_dir - path;
        return print_to_buffer(marker->windbg_cmds, WINDBG_CMD_MAX_LEN, sofar,
                               /* XXX i#631: for 64-bit, windbg fails to successfully
                                * load (has start==end) so we use /i as workaround
                                */
                               "};\n.reload /i %s=" PFMT ";.echo \"Loaded %s\";\n",
                               modname, base, modname);
    } else {
        SYSLOG_INTERNAL_WARNING_ONCE("drmarker windbg cmds out of space");
        return false;
    }
}

static void
privload_add_windbg_cmds(void)
{
    /* i#522: print windbg commands to locate DR and priv libs */
    dr_marker_t *marker = get_drmarker();
    size_t sofar;
    privmod_t *mod;
    sofar = strlen(marker->windbg_cmds);

    /* dynamorio.dll is in the list on windows right now.
     * if not we'd add with get_dynamorio_library_path(), DYNAMORIO_LIBRARY_NAME,
     * and marker->dr_base_addr
     */

    /* XXX: currently only adding those on the list at init time here
     * and later loaded in privload_add_windbg_cmds_post_init(): ignoring
     * unloaded modules.
     */

    acquire_recursive_lock(&privload_lock);
    for (mod = privload_first_module(); mod != NULL; mod = privload_next_module(mod)) {
        /* user32 and ntdll are not private */
        if (strcasecmp(mod->name, "user32.dll") != 0 &&
            strcasecmp(mod->name, "ntdll.dll") != 0) {
            if (!add_mod_to_drmarker(marker, mod->path, mod->name, mod->base, &sofar))
                break;
        }
    }
    windbg_cmds_initialized = true;
    release_recursive_lock(&privload_lock);
}

static void
privload_add_windbg_cmds_post_init(privmod_t *mod)
{
    /* i#522: print windbg commands to locate DR and priv libs */
    dr_marker_t *marker = get_drmarker();
    size_t sofar;
    acquire_recursive_lock(&privload_lock);
    /* privload_lock is our synch mechanism for drmarker windbg field */
    sofar = strlen(marker->windbg_cmds);
    if (dynamo_initialized) {
        set_protection((byte *)marker, sizeof(*marker),
                       MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC);
    }
    add_mod_to_drmarker(marker, mod->path, mod->name, mod->base, &sofar);
    if (dynamo_initialized) {
        set_protection((byte *)marker, sizeof(*marker), MEMPROT_READ | MEMPROT_EXEC);
    }
    release_recursive_lock(&privload_lock);
}

/***************************************************************************/
/* early injection bootstrapping
 *
 * dynamorio.dll has been mapped in by the parent but its imports are
 * not set up.  We do that here.  We can't make any library calls
 * since those require imports.  We could try to share code with
 * privload_get_import_descriptor(), privload_process_imports(),
 * privload_process_one_import(), and get_proc_address_ex(), but IMHO
 * the code duplication is worth the simplicity of not having a param
 * or sthg that is checked on every LOG or ASSERT.
 */

typedef NTSTATUS(NTAPI *nt_protect_t)(IN HANDLE, IN OUT PVOID *, IN OUT PSIZE_T, IN ULONG,
                                      OUT PULONG);
static nt_protect_t bootstrap_ProtectVirtualMemory;

/* exported for earliest_inject_init() */
bool
bootstrap_protect_virtual_memory(void *base, size_t size, uint prot, uint *old_prot)
{
    NTSTATUS res;
    SIZE_T sz = size;
    if (bootstrap_ProtectVirtualMemory == NULL)
        return false;
    res = bootstrap_ProtectVirtualMemory(NT_CURRENT_PROCESS, &base, &sz, prot,
                                         (ULONG *)old_prot);
    return (NT_SUCCESS(res) && sz == size);
}

static char
bootstrap_tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

static int
bootstrap_strcmp(const char *s1, const char *s2, bool ignore_case)
{
    char c1, c2;
    while (true) {
        if (*s1 == '\0') {
            if (*s2 == '\0')
                return 0;
            return -1;
        } else if (*s2 == '\0')
            return 1;
        c1 = (char)*s1;
        c2 = (char)*s2;
        if (ignore_case) {
            c1 = bootstrap_tolower(c1);
            c2 = bootstrap_tolower(c2);
        }
        if (c1 != c2) {
            if (c1 < c2)
                return -1;
            else
                return 1;
        }
        s1++;
        s2++;
    }
}

/* Does not handle forwarders!  Assumed to be called on ntdll only. */
static generic_func_t
privload_bootstrap_get_export(byte *modbase, const char *name)
{
    size_t exports_size;
    uint i;
    IMAGE_EXPORT_DIRECTORY *exports;
    PULONG functions; /* array of RVAs */
    PUSHORT ordinals;
    PULONG fnames;       /* array of RVAs */
    uint ord = UINT_MAX; /* the ordinal to use */
    app_pc func;
    bool match = false;

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)modbase;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(modbase + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *expdir;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt == NULL ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return NULL;
    expdir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_EXPORT;
    if (expdir == NULL || expdir->Size <= 0)
        return NULL;
    exports = (IMAGE_EXPORT_DIRECTORY *)(modbase + expdir->VirtualAddress);
    exports_size = expdir->Size;
    if (exports == NULL || exports->NumberOfNames == 0 || exports->AddressOfNames == 0)
        return NULL;

    functions = (PULONG)(modbase + exports->AddressOfFunctions);
    ordinals = (PUSHORT)(modbase + exports->AddressOfNameOrdinals);
    fnames = (PULONG)(modbase + exports->AddressOfNames);

    for (i = 0; i < exports->NumberOfNames; i++) {
        char *export_name = (char *)(modbase + fnames[i]);
        match = (bootstrap_strcmp(name, export_name, false) == 0);
        if (match) {
            /* we have a match */
            ord = ordinals[i];
            break;
        }
    }
    if (!match || ord >= exports->NumberOfFunctions)
        return NULL;
    func = (app_pc)(modbase + functions[ord]);
    if (func == modbase)
        return NULL;
    if (func >= (app_pc)exports && func < (app_pc)exports + exports_size) {
        /* forwarded */
        return NULL;
    }
    /* get around warnings converting app_pc to generic_func_t */
    return convert_data_to_function(func);
}

bool
privload_bootstrap_dynamorio_imports(byte *dr_base, byte *ntdll_base)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)dr_base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(dr_base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    byte *iat, *imports_end;
    uint orig_prot;
    generic_func_t func;

    /* first, get the one library routine we require */
    bootstrap_ProtectVirtualMemory =
        (nt_protect_t)privload_bootstrap_get_export(ntdll_base, "NtProtectVirtualMemory");
    if (bootstrap_ProtectVirtualMemory == NULL)
        return false;

    /* get import descriptor (modeled on privload_get_import_descriptor()) */
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt == NULL ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (dir == NULL || dir->Size <= 0)
        return false;
    imports = (IMAGE_IMPORT_DESCRIPTOR *)RVA_TO_VA(dr_base, dir->VirtualAddress);
    imports_end = dr_base + dir->VirtualAddress + dir->Size;

    /* walk imports (modeled on privload_process_imports()) */
    while (imports->OriginalFirstThunk != 0) {
        IMAGE_THUNK_DATA *lookup;
        IMAGE_THUNK_DATA *address;
        const char *impname = (const char *)RVA_TO_VA(dr_base, imports->Name);
        if (bootstrap_strcmp(impname, "ntdll.dll", true) != 0)
            return false; /* should only import from ntdll */
        /* DR shouldn't have bound imports so ignoring TimeDateStamp */

        /* walk the lookup table and address table in lockstep */
        lookup = (IMAGE_THUNK_DATA *)RVA_TO_VA(dr_base, imports->OriginalFirstThunk);
        address = (IMAGE_THUNK_DATA *)RVA_TO_VA(dr_base, imports->FirstThunk);
        iat = (app_pc)address;
        if (!bootstrap_protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                              PAGE_READWRITE, &orig_prot))
            return false;
        while (lookup->u1.Function != 0) {
            IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)RVA_TO_VA(
                dr_base, (lookup->u1.AddressOfData & ~(IMAGE_ORDINAL_FLAG)));
            if (TEST(IMAGE_ORDINAL_FLAG, lookup->u1.Function))
                return false; /* no ordinal support */

            func = privload_bootstrap_get_export(ntdll_base, (const char *)name->Name);
            if (func == NULL)
                return false;
            *(byte **)address = (byte *)func;

            lookup++;
            address++;
            if (PAGE_START(address) != PAGE_START(iat)) {
                if (!bootstrap_protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                                      orig_prot, &orig_prot))
                    return false;
                iat = (app_pc)address;
                if (!bootstrap_protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                                      PAGE_READWRITE, &orig_prot))
                    return false;
            }
        }
        if (!bootstrap_protect_virtual_memory((void *)PAGE_START(iat), PAGE_SIZE,
                                              orig_prot, &orig_prot))
            return false;

        imports++;
        if ((byte *)(imports + 1) > imports_end)
            return false;
    }

    return true;
}

#endif /* WINDOWS */
