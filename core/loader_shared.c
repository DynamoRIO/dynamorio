/* *******************************************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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
#ifdef CLIENT_INTERFACE
#    include "instrument.h" /* for instrument_client_lib_unloaded */
#endif

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
DECLARE_CXTSWPROT_VAR(recursive_lock_t privload_lock, INIT_RECURSIVE_LOCK(privload_lock));
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
                          VECTOR_SHARED |
                              VECTOR_NEVER_MERGE
                              /* protected by privload_lock */
                              | VECTOR_NO_LOCK,
                          modlist_areas);
    /* os specific loader initialization prologue before finalize the load */
    os_loader_init_prologue();

    /* Process client libs we loaded early but did not finalize */
    for (i = 0; i < privmod_static_idx; i++) {
        /* Transfer to real list so we can do normal processing */
        char name_copy[MAXIMUM_PATH];
        mod = privload_insert(NULL, privmod_static[i].base, privmod_static[i].size,
                              privmod_static[i].name, privmod_static[i].path);
        LOG(GLOBAL, LOG_LOADER, 1, "%s: processing imports for %s\n", __FUNCTION__,
            mod->name);
        /* save a copy for error msg, b/c mod will be unloaded (i#643) */
        snprintf(name_copy, BUFFER_SIZE_ELEMENTS(name_copy), "%s", mod->name);
        NULL_TERMINATE_BUFFER(name_copy);
        if (!privload_load_finalize(mod)) {
            mod = NULL; /* it's been unloaded! */
#ifdef CLIENT_INTERFACE
            SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 5, get_application_name(),
                   get_application_pid(), name_copy,
                   "\n\tUnable to locate imports of client library");
#endif
            os_terminate(NULL, TERMINATE_PROCESS);
            ASSERT_NOT_REACHED();
        }
    }
    /* os specific loader initialization epilogue after finalize the load */
    os_loader_init_epilogue();
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
             * holding privload_lock and the priv lib
             * DllMain may acquire the same lock that another thread acquired
             * in its app code before requesting a synchall (flush, exit).
             * FIXME i#875: we do not have ntdll!RtlpFlsLock isolated.
             * Living w/ it for now.  It should be unlikely for the app to
             * hold RtlpFlsLock and then acquire privload_lock: privload_lock
             * is used for import redirection but those don't apply within
             * ntdll.
             */
            ASSERT_OWN_NO_LOCKS();
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
    if (privload_has_thread_entry() &&
        /* Only call if we're cleaning up the currently executing thread, as
         * that's what the entry routine is going to do!  Calling on other
         * threads results in problems like double frees (i#969).  Exiting
         * another thread should only happen on process exit or forced thread
         * termination.  The former can technically continue (app could call
         * NtTerminateProcess(0) but then keep going) but we have never seen
         * that; and the latter doesn't do full native cleanups anyway.  Thus
         * we're not worried about leaks from not calling DLL_THREAD_EXIT.
         * (We can't check get_thread_private_dcontext() b/c it's already cleared.)
         */
        dcontext->owning_thread == d_r_get_thread_id()) {
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

/* Given a path-less name, locates and loads a private library for DR's client.
 * Will also accept a full path.
 */
app_pc
locate_and_load_private_library(const char *name, bool reachable)
{
    DODEBUG(privload_recurse_cnt = 0;);
    return privload_load_private_library(name, reachable);
}

/* Load private library for DR's client.  Must be passed a full path. */
app_pc
load_private_library(const char *filename, bool reachable)
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
        privmod = privload_load(filename, NULL, reachable);
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
    if (mod != NULL) {
        res = true; /* don't care if refcount hit 0 or not */
        privload_unload(mod);
    }
    release_recursive_lock(&privload_lock);
    return res;
}

bool
in_private_library(app_pc pc)
{
    return vmvector_overlap(modlist_areas, pc, pc + 1);
}

/* Caseless and "separator agnostic" (i#1869) */
static int
pathcmp(const char *left, const char *right)
{
    size_t i;
    for (i = 0; left[i] != '\0' || right[i] != '\0'; i++) {
        int l = tolower(left[i]);
        int r = tolower(right[i]);
        if (l == '/')
            l = '\\';
        if (r == '/')
            r = '\\';
        if (l < r)
            return -1;
        if (l > r)
            return 1;
    }
    return 0;
}

/* Lookup the private loaded library either by basename or by path */
privmod_t *
privload_lookup(const char *name)
{
    privmod_t *mod;
    bool by_path;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (name == NULL || name[0] == '\0')
        return NULL;
    by_path = IF_WINDOWS_ELSE(double_strrchr(name, DIRSEP, ALT_DIRSEP),
                              strrchr(name, DIRSEP)) != NULL;
    if (!privload_modlist_initialized()) {
        uint i;
        for (i = 0; i < privmod_static_idx; i++) {
            mod = &privmod_static[i];
            if ((by_path && pathcmp(name, mod->path) == 0) ||
                (!by_path && strcasecmp(name, mod->name) == 0))
                return mod;
        }
    } else {
        for (mod = modlist; mod != NULL; mod = mod->next) {
            if ((by_path && pathcmp(name, mod->path) == 0) ||
                (!by_path && strcasecmp(name, mod->name) == 0))
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

/* Lookup the private loaded library by base */
privmod_t *
privload_lookup_by_pc(app_pc pc)
{
    privmod_t *mod;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    if (!privload_modlist_initialized()) {
        uint i;
        for (i = 0; i < privmod_static_idx; i++) {
            if (pc >= privmod_static[i].base &&
                pc < privmod_static[i].base + privmod_static[i].size)
                return &privmod_static[i];
        }
    } else {
        for (mod = modlist; mod != NULL; mod = mod->next) {
            if (pc >= mod->base && pc < mod->base + mod->size)
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
    mod->os_privmod_data = NULL; /* filled in later */
    NULL_TERMINATE_BUFFER(mod->path);
    /* i#489 DT_SONAME is optional and name passed in could be NULL.
     * If so, we get libname from path instead.
     */
    if (IF_UNIX_ELSE(mod->name == NULL, false)) {
        mod->name = double_strrchr(mod->path, DIRSEP, ALT_DIRSEP);
        if (mod->name == NULL)
            mod->name = mod->path;
    }
    mod->ref_count = 1;
    mod->externally_loaded = false;
#ifdef CLIENT_INTERFACE
    mod->is_client = false; /* up to caller to set later */
#endif
    /* do not add non-heap struct to list: in init() we'll move array to list */
    if (privload_modlist_initialized()) {
        if (after == NULL) {
            bool prot = DATASEC_PROTECTED(DATASEC_RARELY_PROT);
            mod->next = modlist;
            mod->prev = NULL;
            if (prot)
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            if (modlist != NULL)
                modlist->prev = mod;
            modlist = mod;
            if (prot)
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
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

static bool
privload_search_path_exists(const char *path, size_t len)
{
    uint i;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    for (i = 0; i < search_paths_idx; i++) {
        if (IF_UNIX_ELSE(strncmp, strncasecmp)(search_paths[i], path, len) == 0)
            return true;
    }
    return false;
}

/* i#955: we support a <basename>.drpath text file listing search paths.
 * XXX i#1078: should we support something like DT_RPATH's $ORIGIN for relative
 * entries in this file?
 */
static void
privload_read_drpath_file(const char *libname)
{
    char path[MAXIMUM_PATH];
    char *end = strrchr(libname, '.');
    if (end == NULL)
        return;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%.*s.%s", end - libname, libname,
             DR_RPATH_SUFFIX);
    NULL_TERMINATE_BUFFER(path);
    LOG(GLOBAL, LOG_LOADER, 3, "%s: looking for %s\n", __FUNCTION__, path);
    if (os_file_exists(path, false /*!is_dir*/)) {
        /* Easiest to parse by mapping.  It's a newline-separated list of
         * paths.  We support carriage returns as well.
         */
        file_t f = os_open(path, OS_OPEN_READ);
        char *map;
        size_t map_size;
        uint64 file_size;
        if (f != INVALID_FILE && os_get_file_size_by_handle(f, &file_size)) {
            LOG(GLOBAL, LOG_LOADER, 2, "%s: reading %s\n", __FUNCTION__, path);
            ASSERT_TRUNCATE(map_size, size_t, file_size);
            map_size = (size_t)file_size;
            map = (char *)os_map_file(f, &map_size, 0, NULL, MEMPROT_READ, 0);
            if (map != NULL && map_size >= file_size) {
                const char *s = (char *)map;
                const char *nl;
                while (s < map + file_size && search_paths_idx < SEARCH_PATHS_NUM) {
                    for (nl = s; nl < map + file_size && *nl != '\r' && *nl != '\n';
                         nl++) {
                    }
                    if (nl == s)
                        break;
                    if (!privload_search_path_exists(s, nl - s)) {
                        snprintf(search_paths[search_paths_idx],
                                 BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx]),
                                 "%.*s", nl - s, s);
                        NULL_TERMINATE_BUFFER(search_paths[search_paths_idx]);
                        LOG(GLOBAL, LOG_LOADER, 1, "%s: added search dir \"%s\"\n",
                            __FUNCTION__, search_paths[search_paths_idx]);
                        search_paths_idx++;
                    }
                    s = nl + 1;
                    while (s < map + file_size && (*s == '\r' || *s == '\n'))
                        s++;
                }
                os_unmap_file((byte *)map, map_size);
            }
            os_close(f);
        }
    }
}

privmod_t *
privload_load(const char *filename, privmod_t *dependent, bool client)
{
    app_pc map;
    size_t size;
    privmod_t *privmod;

    /* i#350: it would be nice to have no-dcontext try/except support:
     * then we could wrap the whole load process, like ntdll!Ldr does
     */
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    DOCHECK(1, {
        /* we have limited stack but we don't expect deep recursion */
        privload_recurse_cnt++;
        ASSERT_CURIOSITY(privload_recurse_cnt < 20); /* win7 dbghelp gets to 12 */
    });

    LOG(GLOBAL, LOG_LOADER, 2, "%s: loading %s\n", __FUNCTION__, filename);

    map = privload_map_and_relocate(filename, &size, client ? MODLOAD_REACHABLE : 0);
    if (map == NULL) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to map %s\n", __FUNCTION__, filename);
        return NULL;
    }

    /* i#955: support a <basename>.drpath file for search paths */
    privload_read_drpath_file(filename);

    /* For direct client libs (not dependent libs),
     * keep a copy of the lib path for use in searching: we'll strdup in loader_init.
     * This needs to come before privload_insert which will inc search_paths_idx.
     * There should be very few of these (normally just 1), so we don't call
     * privload_search_path_exists() (which would require refactoring when the
     * search_paths_idx increment happens).
     */
    if (!privload_modlist_initialized()) {
        const char *end = double_strrchr(filename, DIRSEP, ALT_DIRSEP);
        ASSERT(search_paths_idx < SEARCH_PATHS_NUM);
        if (end != NULL &&
            end - filename < BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx])) {
            snprintf(search_paths[search_paths_idx], end - filename, "%s", filename);
            NULL_TERMINATE_BUFFER(search_paths[search_paths_idx]);
        } else
            ASSERT_NOT_REACHED(); /* should never have client lib path so big */
    }

    /* Add to list before processing imports in case of mutually dependent libs */
    /* Since we control when unmapped, we can use orig export name string and
     * don't need strdup
     */
    /* Add after its dependent to preserve forward-can-unload order */
    privmod = privload_insert(dependent, map, size, get_shared_lib_name(map), filename);

    /* If no heap yet, we'll call finalize later in loader_init() */
    if (privmod != NULL && privload_modlist_initialized()) {
        if (!privload_load_finalize(privmod))
            return NULL;
    }
#ifdef CLIENT_INTERFACE
    if (privmod->is_client)
        instrument_client_lib_loaded(privmod->base, privmod->base + privmod->size);
#endif
    return privmod;
}

bool
privload_unload(privmod_t *privmod)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    ASSERT(privload_modlist_initialized());
    ASSERT(privmod->ref_count > 0);
    privmod->ref_count--;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: %s refcount => %d\n", __FUNCTION__, privmod->name,
        privmod->ref_count);
    if (privmod->ref_count == 0) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: unloading %s @ " PFX "\n", __FUNCTION__,
            privmod->name, privmod->base);
#ifdef CLIENT_INTERFACE
        if (privmod->is_client)
            instrument_client_lib_unloaded(privmod->base, privmod->base + privmod->size);
#endif
        if (privmod->prev == NULL) {
            bool prot = DATASEC_PROTECTED(DATASEC_RARELY_PROT);
            if (prot)
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            modlist = privmod->next;
            if (prot)
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        } else
            privmod->prev->next = privmod->next;
        if (privmod->next != NULL)
            privmod->next->prev = privmod->prev;
        if (!privmod->externally_loaded) {
            privload_call_entry(privmod, DLL_PROCESS_EXIT);
            /* this routine may modify modlist, but we're done with it */
            privload_unload_imports(privmod);
            privload_remove_areas(privmod);
            /* unmap_file removes from DR areas and calls d_r_unmap_file().
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
#    define LIB_SUBDIR "lib64"
#else
#    define LIB_SUBDIR "lib32"
#endif
#define EXT_SUBDIR "ext"
#define DRMF_SUBDIR "drmemory/drmf"

static void
privload_add_subdir_path(const char *subdir)
{
    const char *path, *mid, *end;
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    /* We support loading from various subdirs of the DR package.  We
     * locate these by assuming dynamorio.dll is in
     * <prefix>/lib{32,64}/{debug,release}/ and searching backward for
     * that lib{32,64} part.  We assume that "subdir" is followed
     * by the same /lib{32,64}/{debug,release}/.
     * XXX: this does not work from a build dir: only using exports!
     */
    path = get_dynamorio_library_path();
    mid = strstr(path, LIB_SUBDIR);
    if (mid != NULL && search_paths_idx < SEARCH_PATHS_NUM &&
        (strlen(path) + strlen(subdir) + 1 /*sep*/) <
            BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx])) {
        char *s = search_paths[search_paths_idx];
        snprintf(s, mid - path, "%s", path);
        s += (mid - path);
        snprintf(s, strlen(subdir) + 1 /*sep*/, "%s%c", subdir, DIRSEP);
        s += strlen(subdir) + 1 /*sep*/;
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

void
privload_add_drext_path(void)
{
    /* We support loading from the Extensions dir:
     * <prefix>/ext/lib{32,64}/{debug,release}/
     * Xref i#277/PR 540817.
     */
    privload_add_subdir_path(EXT_SUBDIR);

    /* We also support loading from a co-located DRMF package. */
    privload_add_subdir_path(DRMF_SUBDIR);
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
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to process imports %s\n", __FUNCTION__,
            privmod->name);
        privload_unload(privmod);
        return false;
    }

    privload_os_finalize(privmod);

    if (!privload_call_entry(privmod, DLL_PROCESS_INIT)) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: entry routine failed\n", __FUNCTION__);
        privload_unload(privmod);
        return false;
    }

    privload_load_finalized(privmod);

    LOG(GLOBAL, LOG_LOADER, 1, "%s: loaded %s @ " PFX "-" PFX " from %s\n", __FUNCTION__,
        privmod->name, privmod->base, privmod->base + privmod->size, privmod->path);
    return true;
}

static bool
privload_has_thread_entry(void)
{
    return IF_UNIX_ELSE(false, true);
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

/* returns whether they all fit */
bool
privload_print_modules(bool path, bool lock, char *buf, size_t bufsz, size_t *sofar)
{
    privmod_t *mod;
    if (lock)
        acquire_recursive_lock(&privload_lock);
    for (mod = modlist; mod != NULL; mod = mod->next) {
        if (!mod->externally_loaded) {
            if (!print_to_buffer(buf, bufsz, sofar, "%s=" PFX "\n",
                                 path ? mod->path : mod->name, mod->base)) {
                if (lock)
                    release_recursive_lock(&privload_lock);
                return false;
            }
        }
    }
    if (lock)
        release_recursive_lock(&privload_lock);
    return true;
}
