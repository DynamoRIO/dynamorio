/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

#include "client_tools.h"

#include <string.h>

static bool verbose = false;

#ifdef WINDOWS
static bool found_ordinal_import = false;
#    define LIB_TO_LOOK_FOR "COMDLG32.dll"
#else
#    define LIB_TO_LOOK_FOR "libclient.modules.appdll.so"
#endif

#define INFO(msg, ...)                              \
    do {                                            \
        if (verbose) {                              \
            dr_fprintf(STDERR, msg, ##__VA_ARGS__); \
        }                                           \
    } while (0)

/* Only compare the start of the string to avoid caring about LoadLibraryA vs
 * LoadLibraryW on Windows.
 */
static const char load_library_symbol[] = IF_WINDOWS_ELSE("LoadLibrary", "dlopen");

bool
string_match(const char *str1, const char *str2)
{
    if (str1 == NULL || str2 == NULL)
        return false;

    while (*str1 == *str2) {
        if (*str1 == '\0')
            return true;
        str1++;
        str2++;
    }

    return false;
}

static void
module_load_event(void *dcontext, const module_data_t *data, bool loaded)
{
    /* It's easier to simply print all module loads and unloads, but
     * it appears that loading a module like advapi32.dll causes
     * different modules to load on different windows versions.  Even
     * worse, they seem to be loaded in a different order for
     * different runs.  For the sake of making this test robust, I'll
     * just look for the module in question.
     */
    /* Test i#138 */
    dr_symbol_import_iterator_t *sym_iter;
    dr_symbol_export_iterator_t *exp_iter;
    bool found_sym = false;

    if (data->full_path == NULL || data->full_path[0] == '\0') {
        dr_fprintf(STDERR, "ERROR: full_path empty for %s\n",
                   dr_module_preferred_name(data));
    }
#ifdef WINDOWS
    /* We do not expect \\server-style paths for this test */
    else if (data->full_path[0] == '\\' || data->full_path[1] != ':') {
        dr_fprintf(STDERR, "ERROR: full_path is not in DOS format: %s\n",
                   data->full_path);
    }
#endif
    if (string_match(data->names.module_name, LIB_TO_LOOK_FOR))
        dr_fprintf(STDERR, "LOADED MODULE: %s\n", data->names.module_name);

#ifdef UNIX
    /* Sanity checks on the preferred_base field. */
    if (data->preferred_base != 0) {
        module_data_t *main_mod = dr_get_main_module();
        if (main_mod->start != data->start &&
            /* DR and the client have non-0 preferred bases. */
            strstr(dr_module_preferred_name(data), "dynamorio") == NULL &&
            dr_get_client_base(0) != data->start) {
            dr_fprintf(STDERR, "ERROR: expected 0 preferred base for regular .so\n");
        }
        dr_free_module_data(main_mod);
    }
#else
    if (data->preferred_base == 0) {
        dr_fprintf(STDERR, "ERROR: expected non-zero preferred_base for %s\n",
                   dr_module_preferred_name(data));
    }
#endif

#ifdef WINDOWS
    /* Test iterating symbols imported from a specific module.  The typical use
     * case is probably going to be looking for a specific module, like ntdll,
     * and checking which symbols are used.
     */
    {
        dr_module_import_iterator_t *mod_iter;
        INFO("iterating imports for module %s\n", data->full_path);
        mod_iter = dr_module_import_iterator_start(data->handle);
        while (dr_module_import_iterator_hasnext(mod_iter)) {
            dr_module_import_t *mod_import = dr_module_import_iterator_next(mod_iter);
            INFO("import module: %s\n", mod_import->modname);
            sym_iter = dr_symbol_import_iterator_start(data->handle,
                                                       mod_import->module_import_desc);
            while (dr_symbol_import_iterator_hasnext(sym_iter)) {
                dr_symbol_import_t *sym_import = dr_symbol_import_iterator_next(sym_iter);
                if (strcmp(mod_import->modname, sym_import->modname) != 0) {
                    dr_fprintf(STDERR, "ERROR: modname mismatch: %s vs %s\n",
                               mod_import->modname, sym_import->name);
                }
                if (sym_import->by_ordinal) {
                    found_ordinal_import = true;
                    INFO("%s imports %s!Ordinal%d\n", dr_module_preferred_name(data),
                         sym_import->modname, sym_import->ordinal);
                } else {
                    INFO("%s imports %s!%s\n", dr_module_preferred_name(data),
                         sym_import->modname, sym_import->name);
                }
            }
            dr_symbol_import_iterator_stop(sym_iter);
        }
        dr_module_import_iterator_stop(mod_iter);
    }
#else  /* UNIX */
    /* Linux has no module import iterator, just symbols. */
    sym_iter = dr_symbol_import_iterator_start(data->handle, NULL);
    while (dr_symbol_import_iterator_hasnext(sym_iter)) {
        dr_symbol_import_t *sym_import = dr_symbol_import_iterator_next(sym_iter);
        INFO("%s imports %s\n", dr_module_preferred_name(data), sym_import->name);
    }
    dr_symbol_import_iterator_stop(sym_iter);
#endif /* WINDOWS */

    exp_iter = dr_symbol_export_iterator_start(data->handle);
    while (dr_symbol_export_iterator_hasnext(exp_iter)) {
        dr_symbol_export_t *sym = dr_symbol_export_iterator_next(exp_iter);
        INFO("%s exports %s @" PFX " forward=%s ordinal=%d indirect=%d code=%d\n",
             dr_module_preferred_name(data), sym->name, sym->addr,
             sym->forward == NULL ? "\"\"" : sym->forward, sym->ordinal,
             sym->is_indirect_code, sym->is_code);
        if (strcmp(sym->name, "decode_next_pc") == 0)
            found_sym = true;
    }
    dr_symbol_export_iterator_stop(exp_iter);
    if (strstr(dr_module_preferred_name(data), "dynamorio") != NULL && !found_sym)
        dr_fprintf(STDERR, "failed to find a DR export\n");
}

static void
module_unload_event(void *dcontext, const module_data_t *data)
{
    if (string_match(data->names.module_name, LIB_TO_LOOK_FOR))
        dr_fprintf(STDERR, "UNLOADED MODULE: %s\n", data->names.module_name);
}

static void
test_aux_lib(client_id_t id)
{
    const char *auxname =
        IF_WINDOWS_ELSE("client.modules.appdll.dll", "libclient.modules.appdll.so");
    char buf[MAXIMUM_PATH];
    char path[MAXIMUM_PATH];
    dr_auxlib_handle_t lib;
    char *sep;
    if (dr_snprintf(path, sizeof(path) / sizeof(path[0]), "%s", dr_get_client_path(id)) <
        0) {
        dr_fprintf(STDERR, "ERROR printing to buffer\n");
        return;
    }
    sep = path;
    while (*sep != '\0')
        sep++;
    while (sep > path && *sep != '/' IF_WINDOWS(&&*sep != '\\'))
        sep--;
    *sep = '\0';
    if (dr_snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%s/%s", path, auxname) < 0) {
        dr_fprintf(STDERR, "ERROR printing to buffer\n");
        return;
    }
    /* test loading an auxiliary library: just use another client lib */
    lib = dr_load_aux_library(buf, NULL, NULL);
    if (lib != NULL) {
        dr_auxlib_routine_ptr_t func = dr_lookup_aux_library_routine(lib, "foo_export");
        if (func != NULL) {
            if (!dr_memory_is_in_client((byte *)func)) {
                dr_fprintf(STDERR, "ERROR: aux lib " PFX " not considered client\n",
                           func);
            }
        } else
            dr_fprintf(STDERR, "ERROR: unable to find foo_export\n");
    } else
        dr_fprintf(STDERR, "ERROR: unable to load %s\n", buf);
    if (!dr_unload_aux_library(lib))
        dr_fprintf(STDERR, "ERROR: unable to unload %s\n", buf);
}

#ifdef WINDOWS
/* Module import iterator is Windows-only. */
static bool
module_imports_from_kernel_star(module_handle_t mod)
{
    bool found_module = false;
    dr_module_import_iterator_t *mod_iter = dr_module_import_iterator_start(mod);
    while (dr_module_import_iterator_hasnext(mod_iter)) {
        /* The exe probably imports from kernel32. */
        dr_module_import_t *mod_import = dr_module_import_iterator_next(mod_iter);
        if (_strnicmp(mod_import->modname, "KERNEL", 6) == 0) {
            found_module = true;
        }
    }
    dr_module_import_iterator_stop(mod_iter);
    return found_module;
}
#endif /* WINDOWS */

static void
exit_event(void)
{
#ifdef WINDOWS
    dr_os_version_info_t info;
    info.size = sizeof(info);
    if (dr_get_os_version(&info) && info.version >= DR_WINDOWS_VERSION_7 &&
        !found_ordinal_import) {
        dr_fprintf(STDERR, "ERROR: Failed to find ordinal imports on Win7+\n");
    }
#endif /* WINDOWS */
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    if (id != 0)
        dr_fprintf(STDERR, "ERROR: We assume the id is 0.\n");
    dr_symbol_import_iterator_t *sym_iter;
    bool found_symbol = false;
    module_data_t *main_mod = dr_get_main_module();
    module_handle_t mod_handle = main_mod->handle;
    if (strstr(dr_module_preferred_name(main_mod), "client.modules") == NULL) {
        dr_fprintf(STDERR, "ERROR: Main module has the wrong name\n");
    }
    dr_free_module_data(main_mod);

#ifdef WINDOWS
    if (!module_imports_from_kernel_star(mod_handle)) {
        dr_fprintf(STDERR, "ERROR: didn't find imported module KERNEL*.dll\n");
    }
#endif /* WINDOWS */

    /* Test iterating all symbols by looking for a symbol that we know is
     * imported.
     */
    sym_iter = dr_symbol_import_iterator_start(mod_handle, NULL);
    while (dr_symbol_import_iterator_hasnext(sym_iter)) {
        dr_symbol_import_t *sym_import = dr_symbol_import_iterator_next(sym_iter);
        if (strncmp(sym_import->name, load_library_symbol, strlen(load_library_symbol)) ==
            0) {
            found_symbol = true;
        }
    }
    dr_symbol_import_iterator_stop(sym_iter);

    if (!found_symbol) {
        dr_fprintf(STDERR, "ERROR: didn't find imported symbol %s\n",
                   load_library_symbol);
    }

    dr_register_module_load_event(module_load_event);
    dr_register_module_unload_event(module_unload_event);
    dr_register_exit_event(exit_event);
    test_aux_lib(id);
}

/* TODO - add more module interface tests. */
