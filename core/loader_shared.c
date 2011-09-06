/* *******************************************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2009 Derek Bruening   All rights reserved.
 * *******************************************************************************/

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
 * loader_shared.c: os independent code of DR's custom private library loader
 *
 * original case: i#157
 */

#include "globals.h"
#include "module_shared.h"

#include <string.h>

/* ok to be in .data w/ no sentinel head node b/c never empties out
 * .ntdll always there for Windows, so no need to unprot.
 * XXX: Does it hold for Linux? 
 * It seems no library is must in Linux, even the loader. 
 * Maybe the linux-gate or we just create a fake one?
 */
static privmod_t *modlist; 


/* Recursive library load could happen:
 * Linux:   when load dependent library 
 * Windows: redirect_* can be invoked from private libray
 *          entry points.
 */
DECLARE_CXTSWPROT_VAR(recursive_lock_t privload_lock,
                      INIT_RECURSIVE_LOCK(privload_lock));
/* Protected by privload_lock */
#ifdef DEBUG
DECLARE_NEVERPROT_VAR(static uint privload_recurse_cnt, 0);
#endif


/* These are only written during init so ok to be in .data */
static privmod_t privmod_static[PRIVMOD_STATIC_NUM];
/* this marks end of privmod_static[] */
uint privmod_static_idx;

/* We store client paths here to use for locating libraries later
 * Unfortunately we can't use dynamic storage, and the paths are clobbered
 * immediately by instrument_load_client_libs, so we have max space here.
 */
char search_paths[SEARCH_PATHS_NUM][MAXIMUM_PATH];
/* this marks end of search_paths[] */
uint search_paths_idx;

/* Used for in_private_library() */
vm_area_vector_t *modlist_areas;

/* forward decls */
static bool
privload_load_finalize(privmod_t *privmod);

static bool
privload_has_thread_entry(void);

static bool
privload_modlist_initialized(void);

/***************************************************************************/

void
loader_init(void)
{
    uint i;
    privmod_t *mod;

    acquire_recursive_lock(&privload_lock);
    VMVECTOR_ALLOC_VECTOR(modlist_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE
                          /* protected by privload_lock */
                          | VECTOR_NO_LOCK,
                          modlist_areas);
    /* os specific loader initialization prologue before finalize the load */
    os_loader_init_prologue();

    /* Process client libs we loaded early but did not finalize */
    for (i = 0; i < privmod_static_idx; i++) {
        /* Transfer to real list so we can do normal processing */
        mod = privload_insert(NULL,
                              privmod_static[i].base,
                              privmod_static[i].size,
                              privmod_static[i].name,
                              privmod_static[i].path);
        LOG(GLOBAL, LOG_LOADER, 1, "%s: processing imports for %s\n",
            __FUNCTION__, mod->name);
        if (!privload_load_finalize(mod)) {
            CLIENT_ASSERT(false, "failure to process imports of client library");
#ifdef CLIENT_INTERFACE
            SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 3, 
                   get_application_name(), get_application_pid(),
                   "unable to process imports of client library");
#endif
        }
    }
    /* os specific loader initialization epilogue after finalize the load */
    os_loader_init_epilogue();
    /* FIXME i#338: call loader_thread_init here once get
     * loader_init called after dynamo_thread_init but in a way that
     * works with Windows
     */
    release_recursive_lock(&privload_lock);
}

void
loader_exit(void)
{
    /* We must unload for detach so can't leave them loaded */
    acquire_recursive_lock(&privload_lock);
    /* The list is kept in reverse-dependent order so we can unload from the
     * front without breaking dependencies.
     */
    while (modlist != NULL)
        privload_unload(modlist);
    /* os related loader finalization */
    os_loader_exit();
    vmvector_delete_vector(GLOBAL_DCONTEXT, modlist_areas);
    release_recursive_lock(&privload_lock);
    DELETE_RECURSIVE_LOCK(privload_lock);
}

void
loader_thread_init(dcontext_t *dcontext)
{
    privmod_t *mod;

    if (modlist == NULL) {
#ifdef WINDOWS 
        /* FIXME i#338: once restore order this will become nop */
        /* os specific thread initilization prologue for loader with no lock */
        os_loader_thread_init_prologue(dcontext);
        /* os specific thread initilization epilogue for loader with no lock */
        os_loader_thread_init_epilogue(dcontext);
#endif /* WINDOWS */
    } else {
        /* os specific thread initilization prologue for loader with no lock */
        os_loader_thread_init_prologue(dcontext);
        if (privload_has_thread_entry()) {
            /* We rely on lock isolation to prevent deadlock while we're here
             * holding privload_lock and thread_initexit_lock and the priv lib
             * DllMain may acquire the same lock that another thread acquired
             * in its app code before requesting a synchall (flush, exit)
             */
            acquire_recursive_lock(&privload_lock);
            /* Walk forward and call independent libs last.
             * We do notify priv libs of client threads.
             */
            for (mod = modlist; mod != NULL; mod = mod->next) {
                if (!mod->externally_loaded)
                    privload_call_entry(mod, DLL_THREAD_INIT);
            }
            release_recursive_lock(&privload_lock);
        }
        /* os specific thread initilization epilogue for loader with no lock */
        os_loader_thread_init_epilogue(dcontext);
    }
}

void
loader_thread_exit(dcontext_t *dcontext)
{
    privmod_t *mod;
    /* assuming context swap have happened when entered DR */
    if (privload_has_thread_entry()) {
        acquire_recursive_lock(&privload_lock);
        /* Walk forward and call independent libs last */
         for (mod = modlist; mod != NULL; mod = mod->next) {
            if (!mod->externally_loaded)
                privload_call_entry(mod, DLL_THREAD_EXIT);
        }
        release_recursive_lock(&privload_lock);
    }
    /* os specific thread exit for loader, holding no lock */
    os_loader_thread_exit(dcontext);
}

/* load private library for DR's client */
app_pc
load_private_library(const char *filename)
{
    app_pc res = NULL;
    privmod_t *privmod;

    /* Simpler to lock up front than to unmap on race.  All helper routines
     * assume the lock is held.
     */
    acquire_recursive_lock(&privload_lock);

    privmod = privload_lookup(filename);
    /* XXX: If the private lib has been loaded, shall we increase the counter 
     * or report error?
     */
    if (privmod == NULL) {
        DODEBUG(privload_recurse_cnt = 0;);
        privmod = privload_load(filename, NULL);
    }

    if (privmod != NULL)
        res = privmod->base;
    release_recursive_lock(&privload_lock);
    return res;
}

bool
unload_private_library(app_pc modbase)
{
    privmod_t *mod;
    bool res = false;
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base(modbase);
    if (mod != NULL)
        res = privload_unload(mod);
    release_recursive_lock(&privload_lock);
    return res;
}

bool
in_private_library(app_pc pc)
{
    return vmvector_overlap(modlist_areas, pc, pc+1);
}


/* Lookup the private loaded library by name */
privmod_t *
privload_lookup(const char *name)
{
    privmod_t *mod;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (name == NULL || name[0] == '\0')
        return NULL;
    if (!privload_modlist_initialized()) {
        uint i;
        for (i = 0; i < privmod_static_idx; i++) {
            mod = &privmod_static[i];
            if (strcasecmp(name, mod->name) == 0)
                return mod;
        }
    } else {
        for (mod = modlist; mod != NULL; mod = mod->next) {
            if (strcasecmp(name, mod->name) == 0)
                return mod;
        }
    }
    return NULL;
}

/* Lookup the private loaded library by base */
privmod_t *
privload_lookup_by_base(app_pc modbase)
{
    privmod_t *mod;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (!privload_modlist_initialized()) {
        uint i;
        for (i = 0; i < privmod_static_idx; i++) {
            if (privmod_static[i].base == modbase)
                return &privmod_static[i];
        }
    } else {
        for (mod = modlist; mod != NULL; mod = mod->next) {
            if (modbase == mod->base)
                return mod;
        }
    }
    return NULL;
}

/* Insert privmod after *after
 * name is assumed to be in immutable persistent storage.
 * a copy of path is made.
 */
privmod_t *
privload_insert(privmod_t *after, app_pc base, size_t size, const char *name,
                const char *path)
{
    privmod_t *mod;
    /* We load client libs before heap is initialized so we use a
     * static array of initial privmod_t structs until we can fully
     * load and create proper list entries.
     */

    if (privload_modlist_initialized())
        mod = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, privmod_t, ACCT_OTHER, PROTECTED);
    else {
        /* temporarily use array */
        if (privmod_static_idx >= PRIVMOD_STATIC_NUM) {
            ASSERT_NOT_REACHED();
            return NULL;
        }
        mod = &privmod_static[privmod_static_idx];
        ++privmod_static_idx;
        ++search_paths_idx;
    }
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    mod->base = base;
    mod->size = size;
    mod->name = name;
    strncpy(mod->path, path, BUFFER_SIZE_ELEMENTS(mod->path));
    NULL_TERMINATE_BUFFER(mod->path);
    /* i#489 DT_SONAME is optional and name passed in could be NULL.
     * If so, we get libname from path instead.
     */
    if (IF_LINUX_ELSE(mod->name == NULL, false)) {
        mod->name = double_strrchr(mod->path, DIRSEP, ALT_DIRSEP);
        if (mod->name == NULL)
            mod->name = mod->path;
    }
    mod->ref_count = 1;
    mod->externally_loaded = false;
    /* do not add non-heap struct to list: in init() we'll move array to list */
    if (privload_modlist_initialized()) {
        if (after == NULL) {
            mod->next = modlist;
            mod->prev = NULL;
            ASSERT(!DATASEC_PROTECTED(DATASEC_RARELY_PROT));
            modlist = mod;
        } else {
            /* we insert after dependent libs so we can unload in forward order */
            mod->prev = after;
            mod->next = after->next;
            if (after->next != NULL)
                after->next->prev = mod;
            after->next = mod;
        }
    }
    return (void *)mod;
}

privmod_t *
privload_load(const char *filename, privmod_t *dependent)
{
    app_pc map;
    size_t size;
    privmod_t *privmod;

    /* i#350: it would be nice to have no-dcontext try/except support:
     * then we could wrap the whole load process, like ntdll!Ldr does
     */
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    DODEBUG({
        /* we have limited stack but we don't expect deep recursion */
        privload_recurse_cnt++;
        ASSERT_CURIOSITY(privload_recurse_cnt < 20); /* win7 dbghelp gets to 12 */
    });

    LOG(GLOBAL, LOG_LOADER, 2, "%s: loading %s\n", __FUNCTION__, filename);

    map = privload_map_and_relocate(filename, &size);
    if (map == NULL) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to map %s\n", __FUNCTION__, filename);
        return NULL;
    }

    /* Keep a copy of the lib path for use in searching: we'll strdup in loader_init.
     * This needs to come before privload_insert which will inc search_paths_idx.
     */
    if (!privload_modlist_initialized()) {
        const char *end = double_strrchr(filename, DIRSEP, ALT_DIRSEP);
        if (end != NULL &&
            end - filename < BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx])) {
            snprintf(search_paths[search_paths_idx], end - filename, "%s",
                     filename);
            NULL_TERMINATE_BUFFER(search_paths[search_paths_idx]);
        } else
            ASSERT_NOT_REACHED(); /* should never have client lib path so big */
    }

    /* Add to list before processing imports in case of mutually dependent libs */
    /* Since we control when unmapped, we can use orig export name string and
     * don't need strdup
     */
    /* Add after its dependent to preserve forward-can-unload order */
    privmod = privload_insert(dependent, map, size, get_shared_lib_name(map),
                              filename);

    /* If no heap yet, we'll call finalize later in loader_init() */
    if (privmod != NULL && privload_modlist_initialized()) {
        if (!privload_load_finalize(privmod))
            return NULL;
    }
    return privmod;
}

bool
privload_unload(privmod_t *privmod)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    ASSERT(privload_modlist_initialized());
    ASSERT(privmod->ref_count > 0);
    privmod->ref_count--;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: %s refcount => %d\n", __FUNCTION__,
        privmod->name, privmod->ref_count);
    if (privmod->ref_count == 0) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: unloading %s @ "PFX"\n", __FUNCTION__,
            privmod->name, privmod->base);
        if (privmod->prev == NULL) {
            ASSERT(!DATASEC_PROTECTED(DATASEC_RARELY_PROT));
            modlist = privmod->next;
        } else
            privmod->prev->next = privmod->next;
        if (privmod->next != NULL)
            privmod->next->prev = privmod->prev;
        if (!privmod->externally_loaded) {
            privload_call_entry(privmod, DLL_PROCESS_EXIT);
            /* this routine may modify modlist, but we're done with it */
            privload_unload_imports(privmod);
            privload_remove_areas(privmod);
            /* unmap_file removes from DR areas and calls unmap_file().
             * It's ok to call this for client libs: ok to remove what's not there.
             */
            privload_unmap_file(privmod);
        }
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, privmod, privmod_t, ACCT_OTHER, PROTECTED);
        return true;
    }
    return false;
}

#ifdef X64
# define LIB_SUBDIR "lib64"
#else
# define LIB_SUBDIR "lib32"
#endif
#define EXT_SUBDIR "ext"
void
privload_add_drext_path(void)
{
    const char *path, *mid, *end;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    /* We support loading from the Extensions dir.  We locate it by
     * assuming dynamorio.dll is in <prefix>/lib{32,64}/{debug,release}/
     * and that the Extensions are in <prefix>/ext/lib{32,64}/{debug,release}/
     * Xref i#277/PR 540817.
     * FIXME: this does not work from a build dir: only using exports!
     */
    path = get_dynamorio_library_path();
    mid = strstr(path, LIB_SUBDIR);
    if (mid != NULL && 
        search_paths_idx < SEARCH_PATHS_NUM && 
        (strlen(path)+strlen(EXT_SUBDIR)+1/*sep*/) <
        BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx])) {
        char *s = search_paths[search_paths_idx];
        snprintf(s, mid - path, "%s", path);
        s += (mid - path);
        snprintf(s, strlen(EXT_SUBDIR)+1/*sep*/, "%s%c", EXT_SUBDIR, DIRSEP);
        s += strlen(EXT_SUBDIR)+1/*sep*/;
        end = double_strrchr(path, DIRSEP, ALT_DIRSEP);
        if (end != NULL && search_paths_idx < SEARCH_PATHS_NUM) {
            snprintf(s, end - mid, "%s", mid);
            NULL_TERMINATE_BUFFER(search_paths[search_paths_idx]);
            LOG(GLOBAL, LOG_LOADER, 1, "%s: added Extension search dir %s\n",
                __FUNCTION__, search_paths[search_paths_idx]);
            search_paths_idx++;
        }
    }
}


/* most uses should call privload_load() instead
 * if it fails, unloads
 */
static bool
privload_load_finalize(privmod_t *privmod)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    ASSERT(!privmod->externally_loaded);

    privload_add_areas(privmod);

    privload_redirect_setup(privmod);

    if (!privload_process_imports(privmod)) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to process imports %s\n",
            __FUNCTION__, privmod->name);
        privload_unload(privmod);
        return false;
    }

    /* FIXME: not supporting TLS today in Windows: 
     * covered by i#233, but we don't expect to see it for dlls, only exes
     */

    if (!privload_call_entry(privmod, DLL_PROCESS_INIT)) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: entry routine failed\n", __FUNCTION__);
        privload_unload(privmod);
        return false;
    }

    LOG(GLOBAL, LOG_LOADER, 1, "%s: loaded %s @ "PFX"-"PFX" from %s\n", __FUNCTION__,
        privmod->name, privmod->base, privmod->base + privmod->size, privmod->path);
    return true;
}

static bool
privload_has_thread_entry(void)
{
    return IF_LINUX_ELSE(false, true);
}

static bool
privload_modlist_initialized(void)
{
    return dynamo_heap_initialized;
}

privmod_t *
privload_next_module(privmod_t *mod)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    return mod->next;
}

privmod_t *
privload_first_module(void)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    return modlist;
}
