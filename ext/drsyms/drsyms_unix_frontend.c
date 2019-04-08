/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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

/* Symbol lookup front-end for Linux and Mac
 *
 * For symbol and address lookup and enumeration we use a combination of libelf
 * and libdwarf.  All symbol and address lookup is dealt with by parsing the
 * .symtab section, which points to symbols in the .strtab section.  To get line
 * number information, we have to go the extra mile and use libdwarf to dig
 * through the .debug_line section, which was added in DWARF2.  We don't support
 * STABS or any other form of line number debug information.
 */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "hashtable.h"

/* Guards our internal state and libdwarf's modifications of mod->dbg.
 * We use a recursive lock to allow queries to be called from enumerate callbacks.
 */
static void *symbol_lock;

/* We have to restrict operations when operating in a nested query from a callback */
static bool recursive_context;

/* Hashtable for mapping module paths to dbg_module_t*. */
#define MODTABLE_HASH_BITS 8
static hashtable_t modtable;

/* Sideline server support */
static int shmid;

#define IS_SIDELINE (shmid != 0)

/******************************************************************************
 * Linux lookup layer
 */

static void *
lookup_or_load(const char *modpath)
{
    void *mod = hashtable_lookup(&modtable, (void *)modpath);
    if (mod == NULL) {
        mod = drsym_unix_load(modpath);
        if (mod != NULL) {
            hashtable_add(&modtable, (void *)modpath, mod);
        }
    }
    return mod;
}

static drsym_error_t
drsym_enumerate_symbols_local(const char *modpath, drsym_enumerate_cb callback,
                              drsym_enumerate_ex_cb callback_ex, size_t info_size,
                              void *data, uint flags)
{
    void *mod;
    drsym_error_t r;

    if (modpath == NULL || (callback == NULL && callback_ex == NULL))
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    recursive_context = true;
    r = drsym_unix_enumerate_symbols(mod, callback, callback_ex, info_size, data, flags);
    recursive_context = false;

    dr_recurlock_unlock(symbol_lock);
    return r;
}

static drsym_error_t
drsym_lookup_symbol_local(const char *modpath, const char *symbol, size_t *modoffs OUT,
                          uint flags)
{
    void *mod;
    drsym_error_t r;

    if (modpath == NULL || symbol == NULL || modoffs == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    r = drsym_unix_lookup_symbol(mod, symbol, modoffs, flags);

    dr_recurlock_unlock(symbol_lock);
    return r;
}

static drsym_error_t
drsym_lookup_address_local(const char *modpath, size_t modoffs, drsym_info_t *out INOUT,
                           uint flags)
{
    void *mod;
    drsym_error_t r;

    if (modpath == NULL || out == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* If we add fields in the future we would dispatch on out->struct_size */
    if (out->struct_size != sizeof(*out))
        return DRSYM_ERROR_INVALID_SIZE;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    r = drsym_unix_lookup_address(mod, modoffs, out, flags);

    dr_recurlock_unlock(symbol_lock);
    return r;
}

static drsym_error_t
drsym_enumerate_lines_local(const char *modpath, drsym_enumerate_lines_cb callback,
                            void *data)
{
    void *mod;
    drsym_error_t res;

    if (modpath == NULL || callback == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    res = drsym_unix_enumerate_lines(mod, callback, data);

    dr_recurlock_unlock(symbol_lock);
    return res;
}

/******************************************************************************
 * Exports.
 */

static int drsyms_init_count;

DR_EXPORT
drsym_error_t
drsym_init(int shmid_in)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drsyms_init_count, 1);
    if (count > 1)
        return true;

    shmid = shmid_in;

    symbol_lock = dr_recurlock_create();

    drsym_unix_init();

    if (IS_SIDELINE) {
        /* FIXME NYI i#446: establish connection with sideline server via shared
         * memory specified by shmid
         */
    } else {
        hashtable_init_ex(&modtable, MODTABLE_HASH_BITS, HASH_STRING, true /*strdup*/,
                          false /*!synch: using symbol_lock*/,
                          (generic_func_t)drsym_unix_unload, NULL, NULL);
    }
    return DRSYM_SUCCESS;
}

DR_EXPORT
drsym_error_t
drsym_exit(void)
{
    drsym_error_t res = DRSYM_SUCCESS;
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drsyms_init_count, -1);
    if (count > 0)
        return res;
    if (count < 0)
        return DRSYM_ERROR;

    drsym_unix_exit();
    if (IS_SIDELINE) {
        /* FIXME NYI i#446 */
    }
    hashtable_delete(&modtable);
    dr_recurlock_destroy(symbol_lock);
    return res;
}

DR_EXPORT
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *out INOUT,
                     uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_address_local(modpath, modoffs, out, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs OUT,
                    uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_symbol_local(modpath, symbol, modoffs, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data,
                        uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_symbols_local(modpath, callback, NULL,
                                             sizeof(drsym_info_t), data, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_symbols_ex(const char *modpath, drsym_enumerate_ex_cb callback,
                           size_t info_size, void *data, uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_symbols_local(modpath, NULL, callback, info_size, data,
                                             flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_get_type(const char *modpath, size_t modoffs, uint levels_to_expand, char *buf,
               size_t buf_sz, drsym_type_t **type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

DR_EXPORT
drsym_error_t
drsym_get_func_type(const char *modpath, size_t modoffs, char *buf, size_t buf_sz,
                    drsym_func_type_t **func_type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

DR_EXPORT
drsym_error_t
drsym_expand_type(const char *modpath, uint type_id, uint levels_to_expand, char *buf,
                  size_t buf_sz, drsym_type_t **expanded_type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

DR_EXPORT
size_t
drsym_demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled, uint flags)
{
    return drsym_unix_demangle_symbol(dst, dst_sz, mangled, flags);
}

DR_EXPORT
drsym_error_t
drsym_get_module_debug_kind(const char *modpath, drsym_debug_kind_t *kind OUT)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        void *mod;
        drsym_error_t r;

        if (modpath == NULL || kind == NULL)
            return DRSYM_ERROR_INVALID_PARAMETER;

        dr_recurlock_lock(symbol_lock);
        mod = lookup_or_load(modpath);
        r = drsym_unix_get_module_debug_kind(mod, kind);
        dr_recurlock_unlock(symbol_lock);
        return r;
    }
}

DR_EXPORT
drsym_error_t
drsym_module_has_symbols(const char *modpath)
{
    drsym_debug_kind_t kind;
    drsym_error_t r = drsym_get_module_debug_kind(modpath, &kind);
    if (r == DRSYM_SUCCESS && !TEST(DRSYM_SYMBOLS, kind))
        r = DRSYM_ERROR;
    return r;
}

DR_EXPORT
drsym_error_t
drsym_free_resources(const char *modpath)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        bool found;

        if (modpath == NULL)
            return DRSYM_ERROR_INVALID_PARAMETER;

        /* unsafe to free during iteration */
        if (recursive_context)
            return DRSYM_ERROR_RECURSIVE;

        dr_recurlock_lock(symbol_lock);
        found = hashtable_remove(&modtable, (void *)modpath);
        dr_recurlock_unlock(symbol_lock);

        return (found ? DRSYM_SUCCESS : DRSYM_ERROR);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_lines(const char *modpath, drsym_enumerate_lines_cb callback, void *data)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_lines_local(modpath, callback, data);
    }
}
