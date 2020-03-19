/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* DRSyms DynamoRIO Extension */

/* Symbol lookup for Unix: code shared between Linux, Mac, and Cygwin */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "drsyms_obj.h"
#include "hashtable.h"

#include "dwarf.h"
#include "libdwarf.h"

#include <string.h> /* strlen */
#include <errno.h>
#include <stddef.h> /* offsetof */

#include "demangle.h"
#ifdef DRSYM_HAVE_LIBELFTC
#    include "libelftc.h"
#endif

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x)
#endif

/* For debugging */
static bool verbose = false;

typedef struct _dbg_module_t {
    file_t fd;
    size_t file_size;
    size_t map_size;
    void *map_base;
    void *obj_info;
    void *dwarf_info;
    drsym_debug_kind_t debug_kind;
    /* Sometimes we need to have both original and debuglink loaded.
     * mod_with_dwarf points at the one w/ DWARF info in that case,
     * while the primary mod has symtab+strtab.
     */
    struct _dbg_module_t *mod_with_dwarf;
#define SYMTABLE_HASH_BITS 12
    hashtable_t symtable;
} dbg_module_t;

/******************************************************************************
 * Forward declarations.
 */

static void
unload_module(dbg_module_t *mod);
static bool
follow_debuglink(const char *modpath, dbg_module_t *mod, const char *debuglink,
                 char debug_modpath[MAXIMUM_PATH]);
static void
drsym_free_hash_key(void *key);

/******************************************************************************
 * Module loading and unloading.
 */

static dbg_module_t *
load_module(const char *modpath)
{
    dbg_module_t *mod, *newmod = NULL;
    bool ok;
    const char *debuglink;
    uint64 file_size;

    /* static depth count to prevent stack overflow from circular .gnu_debuglink
     * sections.  We're protected by symbol_lock.
     */
    static int load_module_depth;

    if (load_module_depth >= 2) {
        NOTIFY("drsyms: Refusing to follow .gnu_debuglink more than 2 times.\n");
        return NULL;
    }
    load_module_depth++;

    NOTIFY("loading debug info for module %s\n", modpath);

    /* Alloc and zero the struct so it can be unloaded safely in case of error. */
    mod = dr_global_alloc(sizeof(*mod));
    memset(mod, 0, sizeof(*mod));

    mod->fd = dr_open_file(modpath, DR_FILE_READ);
    if (mod->fd == INVALID_FILE) {
        NOTIFY("%s: unable to open %s\n", __FUNCTION__, modpath);
        goto error;
    }

    ok = dr_file_size(mod->fd, &file_size);
    if (!ok) {
        NOTIFY("%s: unable to get file size %s\n", __FUNCTION__, modpath);
        goto error;
    }
    mod->file_size = (size_t)file_size; /* XXX: ignoring truncation */

    mod->map_size = mod->file_size;
    mod->map_base =
        dr_map_file(mod->fd, &mod->map_size, 0, NULL, DR_MEMPROT_READ, DR_MAP_PRIVATE);
    /* map_size can be larger than file_size */
    if (mod->map_base == NULL || mod->map_size < mod->file_size) {
        NOTIFY("%s: unable to map %s\n", __FUNCTION__, modpath);
        goto error;
    }

    /* We have to duplicate most of the strings we add ourselves, so we do not ask for
     * additional duplication and use a key freeing function.
     */
    hashtable_init_ex(&mod->symtable, SYMTABLE_HASH_BITS, HASH_STRING, false /*strdup*/,
                      false /*!synch*/, NULL, NULL, NULL);
    hashtable_config_t config;
    config.size = sizeof(config);
    config.resizable = true;
    config.resize_threshold = 70;
    config.free_key_func = drsym_free_hash_key;
    hashtable_configure(&mod->symtable, &config);

    /* We need to partially initialize in order to get the debug info */
    mod->obj_info = drsym_obj_mod_init_pre(mod->map_base, mod->file_size);
    if (mod->obj_info == NULL)
        goto error;
    /* Figure out what kind of debug info is available for this module.  */
    mod->debug_kind = drsym_obj_info_avail(mod->obj_info);

    /* If there is a .gnu_debuglink section, then all the debug info we care
     * about is in the file it points to (except maybe .symtab: see below).
     */
    debuglink = drsym_obj_debuglink_section(mod->obj_info, modpath);
    if (debuglink != NULL) {
        char debug_modpath[MAXIMUM_PATH];
        NOTIFY("%s: looking for debuglink %s\n", __FUNCTION__, debuglink);
        if (follow_debuglink(modpath, mod, debuglink, debug_modpath)) {
            NOTIFY("%s: loading debuglink %s\n", __FUNCTION__, debug_modpath);
            newmod = load_module(debug_modpath);
            if (newmod != NULL) {
                /* We expect that DWARF sections will all be in
                 * newmod, but .symtab may be empty in newmod and we
                 * may need to keep mod for that (i#642).
                 */
                NOTIFY("%s: followed debuglink to %s\n", __FUNCTION__, debug_modpath);
                if (!TESTANY(DRSYM_ELF_SYMTAB | DRSYM_PECOFF_SYMTAB,
                             newmod->debug_kind) &&
                    TESTANY(DRSYM_ELF_SYMTAB | DRSYM_PECOFF_SYMTAB, mod->debug_kind)) {
                    /* We need both */
                    mod->mod_with_dwarf = newmod;
                    mod->debug_kind |= newmod->debug_kind;
                } else {
                    /* Debuglink is all we need */
                    unload_module(mod);
                    mod = newmod;
                }
            } /* else stick with mod */
        }     /* else stick with mod */
    }
    if (newmod == NULL) {
        Dwarf_Debug dbg;
        /* If there is no .gnu_debuglink, initialize parsing. */
#ifdef WINDOWS
        /* i#1395: support switching to expots-only for MinGW, for which we
         * need an image mapping.  We don't need the file mapping anymore.
         */
        if (drsym_obj_remap_as_image(mod->obj_info)) {
            dr_unmap_file(mod->map_base, mod->map_size);
            mod->map_size = 0;
            mod->map_base = dr_map_file(mod->fd, &mod->map_size, 0, NULL, DR_MEMPROT_READ,
                                        DR_MAP_PRIVATE | DR_MAP_IMAGE);
            if (mod->map_base == NULL || mod->map_size < mod->file_size) {
                NOTIFY("%s: unable to map %s\n", __FUNCTION__, modpath);
                goto error;
            }
        }
#endif
        if (TEST(DRSYM_DWARF_LINE, mod->debug_kind) &&
            drsym_obj_dwarf_init(mod->obj_info, &dbg)) {
            mod->dwarf_info = drsym_dwarf_init(dbg);
        } else {
            NOTIFY("%s: failed to init DWARF for %s\n", __FUNCTION__, modpath);
            mod->dwarf_info = NULL;
        }
        if (!drsym_obj_mod_init_post(mod->obj_info, mod->map_base, mod->dwarf_info))
            goto error;
        if (mod->dwarf_info != NULL) {
            /* i#1433: obj_info->load_base is initialized in drsym_obj_mod_init_post */
            drsym_dwarf_set_load_base(mod->dwarf_info,
                                      drsym_obj_load_base(mod->obj_info));
        }
    }

    NOTIFY("%s: loaded %s\n", __FUNCTION__, modpath);
    load_module_depth--;
    return mod;

error:
    unload_module(mod);
    load_module_depth--;
    return NULL;
}

/* Construct a hybrid dbg_module_t that loads debug information from the
 * debuglink path.
 *
 * Gdb's search algorithm for finding debug info files is documented here:
 * http://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
 *
 * FIXME: We should allow the user to register additional search directories.
 * XXX: We may need to support the --build-id debug file mechanism documented at
 * the above URL, but for now, .gnu_debuglink seems to work for most Linux
 * systems.
 */
static bool
follow_debuglink(const char *modpath, dbg_module_t *mod, const char *debuglink,
                 char debug_modpath[MAXIMUM_PATH])
{
    char mod_dir[MAXIMUM_PATH];
    char *s, *last_slash = NULL;

    /* For non-GNU we might be handed an absolute path */
    if (debuglink[0] == '/' && dr_file_exists(debuglink)) {
        strncpy(debug_modpath, debuglink, MAXIMUM_PATH);
        debug_modpath[MAXIMUM_PATH - 1] = '\0';
        return true;
    }

    /* Get the module's directory. */
    strncpy(mod_dir, modpath, MAXIMUM_PATH);
    NULL_TERMINATE_BUFFER(mod_dir);
    /* XXX: we want DR's double_strrchr */
    for (s = mod_dir; *s != '\0'; s++) {
        if (*s == '/' IF_WINDOWS(|| *s == '\\'))
            last_slash = s;
    }
    if (last_slash != NULL)
        *last_slash = '\0';

    /* 1. Check /usr/lib/debug/.build-id/xx/$debuglink */
    const char *build_id = drsym_obj_build_id(mod->obj_info);
    NOTIFY("%s: build id is %s\n", __FUNCTION__, build_id == NULL ? "<null>" : build_id);
    if (build_id != NULL && build_id[0] != '\0') {
        dr_snprintf(debug_modpath, MAXIMUM_PATH, "%s/.build-id/%c%c/%s",
                    drsym_obj_debug_path(), build_id[0], build_id[1], debuglink);
        debug_modpath[MAXIMUM_PATH - 1] = '\0';
        NOTIFY("%s: looking for %s\n", __FUNCTION__, debug_modpath);
        if (dr_file_exists(debug_modpath))
            return true;
    }

    /* 2. Check $mod_dir/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH, "%s/%s", mod_dir, debuglink);
    debug_modpath[MAXIMUM_PATH - 1] = '\0';
    NOTIFY("%s: looking for %s\n", __FUNCTION__, debug_modpath);
    /* If debuglink is the basename of modpath, this can point to the same file.
     * Infinite recursion is prevented with a depth check, but we would fail to
     * test the other paths, so we check here if these paths resolve to the same
     * file, ignoring hard and soft links and other path quirks.
     */
    if (dr_file_exists(debug_modpath) && !drsym_obj_same_file(modpath, debug_modpath))
        return true;

    /* 3. Check $mod_dir/.debug/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH, "%s/.debug/%s", mod_dir, debuglink);
    debug_modpath[MAXIMUM_PATH - 1] = '\0';
    NOTIFY("%s: looking for %s\n", __FUNCTION__, debug_modpath);
    if (dr_file_exists(debug_modpath))
        return true;

    /* 4. Check /usr/lib/debug/$mod_dir/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH, "%s/%s/%s", drsym_obj_debug_path(), mod_dir,
                debuglink);
    debug_modpath[MAXIMUM_PATH - 1] = '\0';
    NOTIFY("%s: looking for %s\n", __FUNCTION__, debug_modpath);
    if (dr_file_exists(debug_modpath))
        return true;

    /* We couldn't find the debug file, so we make do with the original module
     * instead.  We'll still find exports in .dynsym.
     */
    return false;
}

/* Free all resources associated with the debug module and the object itself.
 */
static void
unload_module(dbg_module_t *mod)
{
    if (mod->dwarf_info != NULL)
        drsym_dwarf_exit(mod->dwarf_info);
    if (mod->obj_info != NULL)
        drsym_obj_mod_exit(mod->obj_info);
    if (mod->symtable.table != NULL)
        hashtable_delete(&mod->symtable);
    if (mod->map_base != NULL)
        dr_unmap_file(mod->map_base, mod->map_size);
    if (mod->fd != INVALID_FILE)
        dr_close_file(mod->fd);
    if (mod->mod_with_dwarf != NULL)
        unload_module(mod->mod_with_dwarf);
    dr_global_free(mod, sizeof(*mod));
}

/******************************************************************************
 * Symbol table parsing
 */

static drsym_error_t
symsearch_symtab(dbg_module_t *mod, drsym_enumerate_cb callback,
                 drsym_enumerate_ex_cb callback_ex, size_t info_size, void *data,
                 uint flags)
{
    int num_syms;
    int i;
    bool keep_searching = true;
    size_t name_buf_size = 1024; /* C++ symbols can be quite long. */
    drsym_error_t res = DRSYM_SUCCESS;
    drsym_info_t *out;

    num_syms = drsym_obj_num_symbols(mod->obj_info);
    if (num_syms == 0)
        return DRSYM_ERROR;

    out = (drsym_info_t *)dr_global_alloc(info_size);
    out->name = (char *)dr_global_alloc(name_buf_size);
    out->struct_size = info_size;
    out->debug_kind = mod->debug_kind;
    out->type_id = 0; /* NYI */
    /* We don't have line info (see below) */
    out->file = NULL;
    out->file_size = 0;
    out->file_available_size = 0;
    /* Fields beyond name require compatibility checks */
    if (out->struct_size > offsetof(drsym_info_t, flags)) {
        /* Remove unsupported flags */
        out->flags = flags & ~(UNSUPPORTED_NONPDB_FLAGS);
    }

    for (i = 0; keep_searching && i < num_syms; i++) {
        const char *mangled = drsym_obj_symbol_name(mod->obj_info, i);
        const char *unmangled = mangled; /* Points at mangled or symbol_buf. */
        size_t modoffs = 0;
        if (mangled == NULL) {
            res = DRSYM_ERROR;
            break;
        }

        if (callback_ex != NULL) {
            res =
                drsym_obj_symbol_offs(mod->obj_info, i, &out->start_offs, &out->end_offs);
        } else
            res = drsym_obj_symbol_offs(mod->obj_info, i, &modoffs, NULL);
        /* Skip imports and missing symbols. */
        if (res == DRSYM_ERROR_SYMBOL_NOT_FOUND ||
            (callback_ex == NULL && modoffs == 0) || mangled[0] == '\0') {
            res = DRSYM_SUCCESS; /* if go off end of loop */
            continue;
        }
        if (res != DRSYM_SUCCESS)
            break;

        if (TESTANY(DRSYM_DEMANGLE | DRSYM_DEMANGLE_FULL, flags)) {
            size_t len;
            /* Resize until it's big enough. */
            while ((len = drsym_demangle_symbol(out->name, name_buf_size, mangled,
                                                flags)) > name_buf_size) {
                dr_global_free(out->name, name_buf_size);
                name_buf_size = len;
                out->name = (char *)dr_global_alloc(name_buf_size);
            }
            if (len != 0) {
                /* Success. */
                unmangled = out->name;
            }
        } else if (callback_ex != NULL) {
            strncpy(out->name, unmangled, name_buf_size);
            out->name[name_buf_size - 1] = '\0';
        }

        if (callback_ex != NULL) {
            out->name_size = name_buf_size;
            out->name_available_size = strlen(out->name);
            /* We can't get line information w/o doing a separate addr lookup
             * which may not be the same symbol as this one (not 1-to-1)
             */
            keep_searching = callback_ex(out, DRSYM_ERROR_LINE_NOT_AVAILABLE, data);
        } else
            keep_searching = callback(unmangled, modoffs, data);
    }

    dr_global_free(out->name, name_buf_size);
    dr_global_free(out, info_size);

    return res;
}

static drsym_error_t
addrsearch_symtab(dbg_module_t *mod, size_t modoffs, drsym_info_t *info INOUT, uint flags)
{
    const char *symbol;
    size_t name_len = 0;
    uint idx;
    drsym_error_t res = drsym_obj_addrsearch_symtab(mod->obj_info, modoffs, &idx);

    if (res != DRSYM_SUCCESS)
        return res;

    symbol = drsym_obj_symbol_name(mod->obj_info, idx);
    if (symbol == NULL)
        return DRSYM_ERROR;

    if (TEST(DRSYM_DEMANGLE, flags) && info->name != NULL) {
        name_len = drsym_demangle_symbol(info->name, info->name_size, symbol, flags);
    }
    if (name_len == 0) {
        /* Demangling either failed or was not requested. */
        name_len = strlen(symbol) + 1;
        if (info->name != NULL) {
            strncpy(info->name, symbol, info->name_size);
            info->name[info->name_size - 1] = '\0';
        }
    }

    info->name_available_size = name_len;

    return drsym_obj_symbol_offs(mod->obj_info, idx, &info->start_offs, &info->end_offs);
}

/******************************************************************************
 * Hashtable building for symbol lookup.
 *
 * We use __wrap_malloc because our demangled strings have larger capacities
 * than strlen() will find (see drsym_demangle_helper() where we start with a
 * 1024-byte buffer).
 * XXX: Should DR export a malloc-matching heap API that does not start with
 * underscores for cleaner usage?  Most non-static-DR usage though can just
 * use malloc().  Here we want to support static DR.
 *
 * We do not want to pay the cost of hashtable_t doing a further
 * strdup on our just-allocated strings so we free keys ourselves.
 */

static void
drsym_free_hash_key(void *key)
{
    __wrap_free(key);
}

static char *
drsym_dup_string_until_char(const char *sym, char stop)
{
    /* We skip the first char to avoid empty strings. */
    const char *found = strchr(sym + 1, stop);
    if (found != NULL) {
        size_t no_found_sz = found - sym;
        char *no_found = (char *)__wrap_malloc(no_found_sz + 1);
        dr_snprintf(no_found, no_found_sz, "%s", sym);
        no_found[no_found_sz] = '\0';
        return no_found;
    }
    return NULL;
}

static char *
drsym_demangle_helper(const char *sym, uint flags)
{
    /* We look for "_Z" to avoid the copy of the same symbol for non-mangled cases. */
    if (sym[0] != '_' || sym[1] != 'Z')
        return NULL;
    size_t len;
    size_t buf_size = 1024;
    char *buf = (char *)__wrap_malloc(buf_size);
    /* Resize until it's big enough. */
    while ((len = drsym_demangle_symbol(buf, buf_size, sym, flags)) > buf_size) {
        __wrap_free(buf);
        buf_size = len;
        buf = (char *)__wrap_malloc(buf_size);
    }
    if (len <= 0) {
        return NULL;
    }
    return buf;
}

static bool
drsym_add_hash_entry(dbg_module_t *mod, const char *copy, size_t modoffs)
{
    if (hashtable_add(&mod->symtable, (void *)copy, (void *)modoffs)) {
        NOTIFY("%s: added %s\n", __FUNCTION__, copy);
        return true;
    }
    drsym_free_hash_key((void *)copy);
    return false;
}

/* Symbol enumeration callback for doing a single lookup.
 */
static bool
drsym_fill_symtable_cb(const char *sym, size_t modoffs, void *data INOUT)
{
    dbg_module_t *mod = (dbg_module_t *)data;
    size_t len = strlen(sym);
    char *copy = __wrap_malloc(len + 1);
    dr_snprintf(copy, len, "%s", sym);
    copy[len] = '\0';
    if (!drsym_add_hash_entry(mod, copy, modoffs))
        return true;

    /* We match the name part of versioned symbols: so "foo" will match
     * "foo@@GLIBC_2.1".  If there are multiple, a user who wants one in
     * particular needs to include the version name in the search target.
     */
    char *toadd = drsym_dup_string_until_char(sym, '@');
    if (toadd != NULL)
        drsym_add_hash_entry(mod, toadd, modoffs);

    /* Add the demanglings. */
    toadd = drsym_demangle_helper(sym, DRSYM_DEMANGLE);
    if (toadd == NULL)
        return true;
    if (!drsym_add_hash_entry(mod, toadd, modoffs))
        return true;

    /* Add a version without parameters to allow the user to ignore overloads.
     * XXX: This is not a great heuristic as there are cases with parentheses for
     * namespaces or other parts of the type, such as:
     *   Foo::(anonymous namespace)::bar()
     *   std::function<int(int)>::foo().
     * If nobody is relying on this maybe we should just remove it?
     */
    char *noparen = drsym_dup_string_until_char(toadd, '(');
    if (noparen != NULL)
        drsym_add_hash_entry(mod, noparen, modoffs);

#ifdef DRSYM_HAVE_LIBELFTC
    toadd = drsym_demangle_helper(sym, DRSYM_DEMANGLE_FULL);
    if (toadd != NULL)
        drsym_add_hash_entry(mod, toadd, modoffs);
#endif

    return true;
}

/******************************************************************************
 * Exports
 */

void
drsym_unix_init(void)
{
    drsym_obj_init();
}

void
drsym_unix_exit(void)
{
    /* nothing */
}

void *
drsym_unix_load(const char *modpath)
{
    return load_module(modpath);
}

void
drsym_unix_unload(void *mod_in)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    unload_module(mod);
}

drsym_error_t
drsym_unix_enumerate_symbols(void *mod_in, drsym_enumerate_cb callback,
                             drsym_enumerate_ex_cb callback_ex, size_t info_size,
                             void *data, uint flags)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    if (info_size != sizeof(drsym_info_t))
        return DRSYM_ERROR_INVALID_SIZE;
    return symsearch_symtab(mod, callback, callback_ex, info_size, data, flags);
}

drsym_error_t
drsym_unix_lookup_symbol(void *mod_in, const char *symbol, size_t *modoffs OUT,
                         uint flags)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    const char *sym_no_mod;

    if (symbol == NULL) {
        sym_no_mod = NULL;
    } else {
        /* Ignore the module portion of the match string.  We search the module
         * specified by modpath.
         *
         * FIXME #574: Change the interface for both Linux and Windows
         * implementations to not include the module name.
         */
        sym_no_mod = strchr(symbol, '!');
        if (sym_no_mod != NULL) {
            sym_no_mod++;
        } else {
            sym_no_mod = symbol;
        }
    }

    *modoffs = 0;

    if (!TEST(DRSYM_SYMBOLS, mod->debug_kind)) {
        /* XXX i#883: we have no symbols and we're just looking at exports so we
         * should do a fast hashtable lookup instead of a linear walk.
         * DR already has the code for this, accessible via dr_get_proc_address(),
         * except that interface will only work for online (i.e., non-standalone)
         * use.
         */
    }

    if (*modoffs == 0) {
        if (mod->symtable.entries == 0) {
            /* Initialize the hashtable. */
            symsearch_symtab(mod, drsym_fill_symtable_cb, NULL, sizeof(drsym_info_t), mod,
                             DRSYM_LEAVE_MANGLED);
        }
        *modoffs = (size_t)hashtable_lookup(&mod->symtable, (void *)sym_no_mod);
    }
    if (*modoffs == 0)
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    return DRSYM_SUCCESS;
}

drsym_error_t
drsym_unix_lookup_address(void *mod_in, size_t modoffs, drsym_info_t *out INOUT,
                          uint flags)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    drsym_error_t r = addrsearch_symtab(mod, modoffs, out, flags);

    /* If we did find an address for the symbol, go look for its line number
     * information.
     */
    if (r == DRSYM_SUCCESS) {
        /* Search through .debug_line for line and file information.  We always
         * report success even if we only get partial line information we at
         * least have the name of the function.
         */
        dbg_module_t *mod4line = mod;
        if (mod->mod_with_dwarf != NULL)
            mod4line = mod->mod_with_dwarf;
        if (mod4line->dwarf_info == NULL ||
            !drsym_dwarf_search_addr2line(
                mod4line->dwarf_info,
                (Dwarf_Addr)(ptr_uint_t)(drsym_obj_load_base(mod->obj_info) + modoffs),
                out)) {
            r = DRSYM_ERROR_LINE_NOT_AVAILABLE;
        }
    }

    out->debug_kind = mod->debug_kind;
    /* Fields beyond name require compatibility checks */
    if (out->struct_size > offsetof(drsym_info_t, flags)) {
        /* Remove unsupported flags */
        out->flags = flags & ~(UNSUPPORTED_NONPDB_FLAGS);
    }
    return r;
}

drsym_error_t
drsym_unix_enumerate_lines(void *mod_in, drsym_enumerate_lines_cb callback, void *data)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    dbg_module_t *mod4line = mod;
    if (mod->mod_with_dwarf != NULL)
        mod4line = mod->mod_with_dwarf;
    if (mod4line->dwarf_info != NULL)
        return drsym_dwarf_enumerate_lines(mod4line->dwarf_info, callback, data);
    else
        return DRSYM_ERROR_LINE_NOT_AVAILABLE;
}

drsym_error_t
drsym_unix_get_type(void *mod_in, size_t modoffs, uint levels_to_expand, char *buf,
                    size_t buf_sz, drsym_type_t **type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

drsym_error_t
drsym_unix_get_func_type(void *mod_in, size_t modoffs, char *buf, size_t buf_sz,
                         drsym_func_type_t **func_type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

drsym_error_t
drsym_unix_expand_type(const char *modpath, uint type_id, uint levels_to_expand,
                       char *buf, size_t buf_sz, drsym_type_t **expanded_type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

size_t
drsym_unix_demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled, uint flags)
{
    if (!TEST(DRSYM_DEMANGLE_FULL, flags)) {
        /* The demangle.cc implementation is fast and replaces template args
         * with <> and omits parameters.  Use it if the user doesn't
         * want either of those.  Its return value always follows our
         * conventions.
         */
        int len = Demangle(mangled, dst, (int)dst_sz, DEMANGLE_DEFAULT);
        if (len > 0) {
            return len; /* Success or truncation. */
        }
    } else {
#ifdef DRSYM_HAVE_LIBELFTC
        /* If the user wants template arguments or overloads, we use the
         * libelftc demangler which is slower, but can properly demangle
         * template arguments.
         */

        /* libelftc code use fp ops so we have to save fp state (i#756) */
        byte fp_raw[DR_FPSTATE_BUF_SIZE + DR_FPSTATE_ALIGN];
        byte *fp_align = (byte *)ALIGN_FORWARD(fp_raw, DR_FPSTATE_ALIGN);

        proc_save_fpstate(fp_align);
        int status = elftc_demangle(mangled, dst, dst_sz, ELFTC_DEM_GNU3);
        proc_restore_fpstate(fp_align);

#    ifdef WINDOWS
        /* Our libelftc returns the # of chars needed, and copies the truncated
         * unmangled name, but it returns -1 on error.
         */
        if (status <= 0)
            return 0;
        if (status > 0)
            return status;
#    else
        /* XXX: let's make the same change to the libelftc we're using here */
        if (status == 0) {
            return strlen(dst) + 1;
        } else if (errno == ENAMETOOLONG) {
            /* FIXME: libelftc actually doesn't copy the output into dst and
             * truncate it, so we do the next best thing and put the truncated
             * mangled name in there.
             */
            strncpy(dst, mangled, dst_sz);
            dst[dst_sz - 1] = '\0';
            /* FIXME: This return value is made up and may not be large enough.
             * It will work eventually if the caller reallocates their buffer
             * and retries in a loop, or if they just want to detect truncation.
             */
            return dst_sz * 2;
        }
#    endif
#endif /* DRSYM_HAVE_LIBELFTC */
    }

    /* If the demangling failed, copy the mangled symbol into the output. */
    strncpy(dst, mangled, dst_sz);
    dst[dst_sz - 1] = '\0';
    return 0;
}

drsym_error_t
drsym_unix_get_module_debug_kind(void *mod_in, drsym_debug_kind_t *kind OUT)
{
    dbg_module_t *mod = (dbg_module_t *)mod_in;
    if (mod != NULL) {
        *kind = mod->debug_kind;
        return DRSYM_SUCCESS;
    } else {
        return DRSYM_ERROR_LOAD_FAILED;
    }
}
