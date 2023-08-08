/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

// func_trace.cpp: module for recording function traces

#define NOMINMAX // Avoid windows.h messing up std::min.
#include "func_trace.h"

#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "dr_api.h"
#include "drmgr.h"
#include "drvector.h"
#include "options.h"
#include "hashtable.h"
#include "droption.h"
#include "drsyms.h"
#include "drwrap.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

// The expected pattern for a single_op_value is:
//     function_name|function_id|arguments_num
// where function_name can contain spaces (for instance, C++ namespace prefix)
#define PATTERN_SEPARATOR "|"

#define NOTIFY(level, ...)                     \
    do {                                       \
        if (op_verbose.get_value() >= (level)) \
            dr_fprintf(STDERR, __VA_ARGS__);   \
    } while (0)

static int func_trace_init_count;
static int tls_idx;

static func_trace_append_entry_vec_t append_entry_vec;
static drvector_t func_names;

static void *funcs_wrapped_lock;
/* These are protected by funcs_wrapped_lock. */
static drvector_t funcs_wrapped;
static int wrap_id;
/* We store the id+1 so that 0 can mean "not present". */
static hashtable_t pc2idplus1;

static std::string funcs_str, funcs_str_sep;
static ssize_t (*write_file_func)(file_t file, const void *data, size_t count);
static file_t funclist_fd;

/* The maximum supported length of a function name to trace.
 * We expect this to be longer than any C/C++ symbol we'll see.
 */
#define DRMEMTRACE_MAX_FUNC_NAME_LEN 2048

/* The maximum length of a line in #DRMEMTRACE_FUNCTION_LIST_FILENAME. */
#define DRMEMTRACE_MAX_QUALIFIED_FUNC_LEN (DRMEMTRACE_MAX_FUNC_NAME_LEN + 256)

typedef struct {
    char name[DRMEMTRACE_MAX_FUNC_NAME_LEN];
    int id;
    int arg_num;
    bool noret;
} func_metadata_t;

static func_metadata_t *
create_func_metadata(const char *name, int id, int arg_num, bool noret)
{
    func_metadata_t *f = (func_metadata_t *)dr_global_alloc(sizeof(func_metadata_t));
    strncpy(f->name, name, BUFFER_SIZE_ELEMENTS(f->name));
    f->id = id;
    f->arg_num = arg_num;
    f->noret = noret;
    return f;
}

static void
delete_func_metadata(func_metadata_t *f)
{
    dr_global_free((void *)f, sizeof(func_metadata_t));
}

static void
free_func_entry(void *entry)
{
    delete_func_metadata((func_metadata_t *)entry);
}

// NOTE: try to avoid invoking any code that could be traced by func_pre_hook
//       (e.g., STL, libc, etc.)
static void
func_pre_hook(void *wrapcxt, INOUT void **user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    void *pt_data = drmgr_get_tls_field(drcontext, tls_idx);
    func_trace_entry_vector_t *v = (func_trace_entry_vector_t *)pt_data;
    v->size = 0;
    size_t idx = (size_t)*user_data;
    func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs_wrapped, (uint)idx);
    uintptr_t retaddr = (uintptr_t)drwrap_get_retaddr(wrapcxt);
    uintptr_t f_id = (uintptr_t)f->id;

    v->entries[v->size++] = func_trace_entry_t(TRACE_MARKER_TYPE_FUNC_ID, f_id);
    v->entries[v->size++] = func_trace_entry_t(TRACE_MARKER_TYPE_FUNC_RETADDR, retaddr);
    for (int i = 0; i < f->arg_num; i++) {
        uintptr_t arg_i = (uintptr_t)drwrap_get_arg(wrapcxt, i);
        v->entries[v->size++] = func_trace_entry_t(TRACE_MARKER_TYPE_FUNC_ARG, arg_i);
    }
    append_entry_vec(drcontext, v);
}

// NOTE: try to avoid invoking any code that could be traced by func_post_hook
//       (e.g., STL, libc, etc.)
static void
func_post_hook(void *wrapcxt, void *user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    void *pt_data = drmgr_get_tls_field(drcontext, tls_idx);
    func_trace_entry_vector_t *v = (func_trace_entry_vector_t *)pt_data;
    v->size = 0;
    size_t idx = (size_t)user_data;
    func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs_wrapped, (uint)idx);
    uintptr_t retval = (uintptr_t)drwrap_get_retval(wrapcxt);
    uintptr_t f_id = (uintptr_t)f->id;
    DR_ASSERT(!f->noret); // Shouldn't come here if noret.

    v->entries[v->size++] = func_trace_entry_t(TRACE_MARKER_TYPE_FUNC_ID, f_id);
    v->entries[v->size++] = func_trace_entry_t(TRACE_MARKER_TYPE_FUNC_RETVAL, retval);
    append_entry_vec(drcontext, v);
}

static app_pc
get_pc_by_symbol(const module_data_t *mod, const char *symbol)
{
    if (mod == NULL || symbol == NULL)
        return NULL;

    // Try to find the symbol in the dynamic symbol table.
    app_pc pc = (app_pc)dr_get_proc_address(mod->handle, symbol);
    if (pc != NULL) {
        NOTIFY(2, "dr_get_proc_address found symbol %s at pc=" PFX "\n", symbol, pc);
        return pc;
    } else if (op_record_dynsym_only.get_value()) {
        NOTIFY(2, "Failed to find symbol %s in .dynsym; not recording it\n", symbol);
        return NULL;
    } else {
        // If failed to find the symbol in the dynamic symbol table, then we try to find
        // it in the module loaded by reading the module file in mod->full_path.
        // NOTE: mod->full_path could be invalid in the case where the original
        // module file is remapped and deleted (e.g. hugepage_text).
        // FIXME: find a way to find the PC of the symbol even if the original module file
        // is deleted.
        size_t offset;
        drsym_error_t err =
            drsym_lookup_symbol(mod->full_path, symbol, &offset, DRSYM_DEMANGLE);
        if (err != DRSYM_SUCCESS) {
            err =
                drsym_lookup_symbol(mod->full_path, symbol, &offset, DRSYM_LEAVE_MANGLED);
        }
        if (err == DRSYM_SUCCESS) {
            pc = mod->start + offset;
            NOTIFY(2, "drsym_lookup_symbol found symbol %s at pc=" PFX "\n", symbol, pc);
            return pc;
        } else {
            NOTIFY(2, "Failed to find symbol %s, drsym_error_t=%d\n", symbol, err);
            return NULL;
        }
    }
}

static inline const char *
get_module_basename(const module_data_t *mod)
{
    const char *mod_name = dr_module_preferred_name(mod);
    if (mod_name == nullptr) {
        const char *slash = strrchr(mod->full_path, '/');
#ifdef WINDOWS
        const char *bslash = strrchr(mod->full_path, '\\');
        if (bslash != nullptr && bslash > slash)
            slash = bslash;
#endif
        if (slash != nullptr)
            mod_name = slash + 1;
    }
    if (mod_name == nullptr)
        mod_name = "<unknown>";
    return mod_name;
}

static void
instru_funcs_module_load(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (drcontext == NULL || mod == NULL)
        return;

    uint64 ms_start = dr_get_milliseconds();
    const char *mod_name = get_module_basename(mod);
    NOTIFY(2, "instru_funcs_module_load for %s\n", mod_name);
    // We need to go through all the functions to identify duplicates and adjust
    // arg counts before we can write to funclist.  We use this vector to remember
    // what to write.  We expect the common case to be zero entries since the
    // average app library probably has zero traced functions in it.
    drvector_t vec_pcs;
    drvector_init(&vec_pcs, 0, false, nullptr);
    for (size_t i = 0; i < func_names.entries; i++) {
        func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&func_names, (uint)i);
        app_pc f_pc = get_pc_by_symbol(mod, f->name);
        if (f_pc == NULL)
            continue;
        drvector_append(&vec_pcs, (void *)f_pc);
        dr_mutex_lock(funcs_wrapped_lock);
        int id = 0;
        int existing_id = (int)(ptr_int_t)hashtable_lookup(&pc2idplus1, (void *)f_pc);
        if (existing_id != 0) {
            // Another symbol mapping to the same pc is already wrapped.
            // The number of args will be the minimum count for all those registered,
            // since the code must be ignoring extra arguments.
            id = existing_id - 1 /*table stores +1*/;
            func_metadata_t *f_traced =
                (func_metadata_t *)drvector_get_entry(&funcs_wrapped, (uint)id);
            f_traced->arg_num = std::min(f->arg_num, f_traced->arg_num);
            NOTIFY(1, "Duplicate-pc hook: %s!%s == id %d; using min=%d args\n", mod_name,
                   f->name, id, f_traced->arg_num);
        } else {
            id = wrap_id++;
            drvector_append(&funcs_wrapped,
                            create_func_metadata(f->name, id, f->arg_num, f->noret));
            if (!hashtable_add(&pc2idplus1, (void *)f_pc, (void *)(ptr_int_t)(id + 1)))
                DR_ASSERT(false && "Failed to maintain pc2idplus1 internal hashtable");
        }
        // With the lock restrictions for calling drwrap_wrap_ex(), we can't hold a
        // a lock across this entire callback.  We release our lock during our
        // drwrap_wrap_ex() call.
        dr_mutex_unlock(funcs_wrapped_lock);
        if (existing_id != 0) {
            continue;
        }
        uint flags = 0;
        if (!f->noret && op_record_replace_retaddr.get_value())
            flags = DRWRAP_REPLACE_RETADDR;
        if (drwrap_wrap_ex(f_pc, func_pre_hook, f->noret ? nullptr : func_post_hook,
                           (void *)(ptr_uint_t)id, flags)) {
            NOTIFY(1, "Inserted hooks for %s!%s @%p == id %d\n", mod_name, f->name, f_pc,
                   id);
        } else {
            // We've ruled out two symbols mapping to the same pc, so this is some
            // unexpected, possibly severe error.
            NOTIFY(0, "Failed to insert hooks for %s!%s == id %d\n", mod_name, f->name,
                   id);
        }
    }
    // Now write out the traced functions.
    dr_mutex_lock(funcs_wrapped_lock);
    for (size_t i = 0; i < vec_pcs.entries; ++i) {
        app_pc f_pc = (app_pc)drvector_get_entry(&vec_pcs, (uint)i);
        int id = (int)(ptr_int_t)hashtable_lookup(&pc2idplus1, (void *)f_pc);
        DR_ASSERT(id != 0 && "Failed to maintain pc2idplus1 internal hashtable");
        --id; // Table stores +1.
        func_metadata_t *f_traced =
            (func_metadata_t *)drvector_get_entry(&funcs_wrapped, (uint)id);
        char qual[DRMEMTRACE_MAX_QUALIFIED_FUNC_LEN];
        int len = dr_snprintf(qual, BUFFER_SIZE_ELEMENTS(qual), "%d,%d,%p,%s%s!%s\n", id,
                              f_traced->arg_num, f_pc, f_traced->noret ? "noret," : "",
                              mod_name, f_traced->name);
        if (len < 0 || len == BUFFER_SIZE_ELEMENTS(qual)) {
            NOTIFY(0, "Qualified name is too long and was truncated: %s!%s\n", mod_name,
                   f_traced->name);
        }
        NULL_TERMINATE_BUFFER(qual);
        size_t sz = strlen(qual);
        if (write_file_func(funclist_fd, qual, sz) != static_cast<ssize_t>(sz))
            NOTIFY(0, "Failed to write to funclist file\n");
    }
    dr_mutex_unlock(funcs_wrapped_lock);
    drvector_delete(&vec_pcs);

    uint64 ms_elapsed = dr_get_milliseconds() - ms_start;
    NOTIFY((ms_elapsed > 10U) ? 1U : 2U,
           "Symbol queries for %s took " UINT64_FORMAT_STRING "ms\n", mod_name,
           ms_elapsed);
}

static void
instru_funcs_module_unload(void *drcontext, const module_data_t *mod)
{
    if (drcontext == NULL || mod == NULL)
        return;
    const char *mod_name = get_module_basename(mod);
    for (size_t i = 0; i < func_names.entries; i++) {
        func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&func_names, (uint)i);
        app_pc f_pc = get_pc_by_symbol(mod, f->name);
        if (f_pc == NULL)
            continue;
        // To support a different library with a to-trace symbol being mapped at the
        // same pc, we remove from pc2idplus1.  If the same library is re-loaded, we'll
        // give a new id to the same symbol in the new incarnation.
        hashtable_remove(&pc2idplus1, (void *)f_pc);
        if (drwrap_unwrap(f_pc, func_pre_hook, f->noret ? nullptr : func_post_hook)) {
            NOTIFY(1, "Removed hooks for %s!%s @%p\n", mod_name, f->name, f_pc);
        } else {
            NOTIFY(0, "Failed to remove hooks for %s!%s @%p\n", mod_name, f->name, f_pc);
        }
    }
}

dr_emit_flags_t
func_trace_enabled_instrument_event(void *drcontext, void *tag, instrlist_t *bb,
                                    instr_t *instr, instr_t *where, bool for_trace,
                                    bool translating, void *user_data)
{
    if (funcs_str.empty())
        return DR_EMIT_DEFAULT;
    return drwrap_invoke_insert(drcontext, tag, bb, instr, where, for_trace, translating,
                                user_data);
}

dr_emit_flags_t
func_trace_disabled_instrument_event(void *drcontext, void *tag, instrlist_t *bb,
                                     instr_t *instr, instr_t *where, bool for_trace,
                                     bool translating, void *user_data)
{
    if (funcs_str.empty())
        return DR_EMIT_DEFAULT;
    return drwrap_invoke_insert_cleanup_only(drcontext, tag, bb, instr, where, for_trace,
                                             translating, user_data);
}

static std::vector<std::string>
split_by(std::string s, std::string sep)
{
    size_t pos;
    std::vector<std::string> vec;
    do {
        pos = s.find(sep);
        vec.push_back(s.substr(0, pos));
        s.erase(0, pos + sep.length());
    } while (pos != std::string::npos);
    return vec;
}

static void
init_funcs_str_and_sep()
{
    if (op_record_heap.get_value())
        funcs_str = op_record_heap_value.get_value();
    else
        funcs_str = "";
    funcs_str_sep = op_record_function.get_value_separator();
    DR_ASSERT(funcs_str_sep == op_record_heap_value.get_value_separator());
    std::string op_value = op_record_function.get_value();
    if (!funcs_str.empty() && !op_value.empty())
        funcs_str += funcs_str_sep;
    funcs_str += op_value;
}

// XXX: The reason we reserve a buffer/vector here for later append_entry_vec use is
// because we want to reduce the overhead of pre/post function hook by grouping several
// calls to append_entry into one. This makes the code less cleaner, but for now it is
// needed to put down the overhead of instrumenting function under certain threshold for
// some large application. This optimization would become negligible when we have a better
// way to improve the overall performance. At that time, we can remove this code and
// get back to the way of calling append_entry for each function trace entry.
static void
event_thread_init(void *drcontext)
{
    void *data = dr_thread_alloc(drcontext, sizeof(func_trace_entry_vector_t));
    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);
}

static void
event_thread_exit(void *drcontext)
{
    void *data = drmgr_get_tls_field(drcontext, tls_idx);
    dr_thread_free(drcontext, data, sizeof(func_trace_entry_vector_t));
}

bool
func_trace_init(func_trace_append_entry_vec_t append_entry_vec_,
                ssize_t (*write_file)(file_t file, const void *data, size_t count),
                file_t funclist_file)
{
    // Online is not supported as we have no mechanism to pass the funclist_file
    // data to the simulator.
    if (!op_offline.get_value())
        return false;

    if (append_entry_vec_ == NULL)
        return false;

    if (dr_atomic_add32_return_sum(&func_trace_init_count, 1) > 1)
        return true;

    init_funcs_str_and_sep();
    // If there is no function specified to trace,
    // then the whole func_trace module doesn't have to do anything.
    if (funcs_str.empty())
        return true;

    write_file_func = write_file;
    funclist_fd = funclist_file;
    funcs_wrapped_lock = dr_mutex_create();
    wrap_id = 0;

    auto op_values = split_by(funcs_str, funcs_str_sep);
    std::set<std::string> existing_names;
    if (!drvector_init(&func_names, (uint)op_values.size(), false, free_func_entry) ||
        !drvector_init(&funcs_wrapped, (uint)op_values.size(), false, free_func_entry)) {
        DR_ASSERT(false);
        goto failed;
    }
    append_entry_vec = append_entry_vec_;

    for (auto &single_op_value : op_values) {
        auto items = split_by(single_op_value, PATTERN_SEPARATOR);
        if (items.size() < 2 || items.size() > 3) {
            NOTIFY(0,
                   "Error: -record_function or -record_heap_value only takes 2"
                   " or 3 fields for each function: %s\n",
                   funcs_str.c_str());
            return false;
        }
        std::string name = items[0];
        int arg_num = atoi(items[1].c_str());
        if (name.empty()) {
            NOTIFY(0, "Error: -record_function name should not be empty");
            return false;
        }
        if (existing_names.find(name) != existing_names.end()) {
            NOTIFY(0,
                   "Warning: duplicated function name %s in -record_function or"
                   " -record_heap_value %s\n",
                   name.c_str(), funcs_str.c_str());
            continue;
        }
        if (name.size() > DRMEMTRACE_MAX_FUNC_NAME_LEN - 1 /*newline*/) {
            NOTIFY(0, "The function name %s should not be larger than %d\n", name.c_str(),
                   DRMEMTRACE_MAX_FUNC_NAME_LEN - 1);
            return false;
        }
        if (arg_num > MAX_FUNC_TRACE_ENTRY_VEC_CAP - 2) {
            NOTIFY(0, "The arg_num of the function %s should not be larger than %d\n",
                   funcs_str.c_str(), MAX_FUNC_TRACE_ENTRY_VEC_CAP - 2);
            return false;
        }
        bool noret = false;
        if (items.size() == 3) {
            if (items[2] == "noret") {
                noret = true;
            } else {
                NOTIFY(0, "Unknown optional flag: %s\n", items[2].c_str());
                return false;
            }
        }

        dr_log(NULL, DR_LOG_ALL, 1, "Trace func name=%s, arg_num=%d\n", name.c_str(),
               arg_num);
        existing_names.insert(name);
        drvector_append(&func_names,
                        create_func_metadata(name.c_str(), 0, arg_num, noret));
    }

    hashtable_init_ex(&pc2idplus1, 8, HASH_INTPTR, false /*!strdup*/, false /*!synch*/,
                      nullptr, nullptr, nullptr);

    if (!op_record_dynsym_only.get_value()) {
        if (!(drsym_init(0) == DRSYM_SUCCESS)) {
            DR_ASSERT(false);
            goto failed;
        }
    }
    /* For multi-instrumentation cases with drbbdup, we need the drwrap inverted
     * control mode where we invoke its instrumentation handlers.
     */
    if (!drwrap_set_global_flags(DRWRAP_INVERT_CONTROL) || !drwrap_init()) {
        DR_ASSERT(false);
        goto failed;
    }

    drwrap_set_global_flags(DRWRAP_NO_FRILLS);
    drwrap_set_global_flags(DRWRAP_FAST_CLEANCALLS);
    drwrap_set_global_flags(DRWRAP_SAFE_READ_RETADDR);
    drwrap_set_global_flags(DRWRAP_SAFE_READ_ARGS);

    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit)) {
        DR_ASSERT(false);
        goto failed;
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        DR_ASSERT(false);
        goto failed;
    }

    if (!drmgr_register_module_load_event(instru_funcs_module_load) ||
        !drmgr_register_module_unload_event(instru_funcs_module_unload)) {
        DR_ASSERT(false);
        goto failed;
    }

    return true;
failed:
    func_trace_exit();
    return false;
}

void
func_trace_exit()
{
    if (dr_atomic_add32_return_sum(&func_trace_init_count, -1) != 0)
        return;

    if (funcs_str.empty())
        return;
    /* Clear for re-attach. */
    funcs_str.clear();
    funcs_str_sep.clear();
    hashtable_delete(&pc2idplus1);
    if (!drvector_delete(&funcs_wrapped) || !drvector_delete(&func_names))
        DR_ASSERT(false);
    dr_mutex_destroy(funcs_wrapped_lock);
    if (!drmgr_unregister_module_load_event(instru_funcs_module_load) ||
        !drmgr_unregister_module_unload_event(instru_funcs_module_unload) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx))
        DR_ASSERT(false);
    if (!op_record_dynsym_only.get_value()) {
        if (drsym_exit() != DRSYM_SUCCESS)
            DR_ASSERT(false);
    }
    drwrap_exit();
}

} // namespace drmemtrace
} // namespace dynamorio
