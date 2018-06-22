/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

// func_trace.cpp: brief Module for recording function traces

#include <string>
#include <vector>
#include "dr_api.h"
#include "drsyms.h"
#include "drwrap.h"
#include "drmgr.h"
#include "drvector.h"
#include "trace_entry.h"
#include "../common/options.h"
#include "func_trace.h"

// The expected pattern for a single_op_value is:
//     function_name|function_id|arguments_num
// where function_name can contain spaces (for instance, C++ namespace prefix)
#define PATTERN_SEPARATOR "|"

static int func_trace_init_count;

static func_trace_append_entry_t append_entry;
static process_fatal_t process_fatal;
static drvector_t funcs;

typedef struct {
    char name[2048];  // probably the maximum length of C/C++ symbol
    int id;
    int arg_num;
} func_metadata_t;

static inline size_t
get_safe_size(size_t dst_size, size_t src_size)
{
    return src_size < dst_size ? src_size : dst_size - 1;
}

static func_metadata_t *
create_func_metadata(std::string name, int id, int arg_num)
{
    func_metadata_t *f =
        (func_metadata_t *)dr_global_alloc(sizeof(func_metadata_t));
    size_t safe_size = get_safe_size(sizeof(f->name), name.size());
    strncpy(f->name, name.c_str(), safe_size);
    f->name[safe_size] = '\0';
    f->id = id;
    f->arg_num = arg_num;
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
//       (e.g., stl, libc, etc.)
static void
func_pre_hook(void *wrapcxt, INOUT void **user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    size_t idx = (size_t)*user_data;
    func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs, idx);
    app_pc retaddr = drwrap_get_retaddr(wrapcxt);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ID, (uintptr_t)f->id);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_RETADDR, (uintptr_t)retaddr);
    for (int i = 0; i < f->arg_num; i++) {
        uintptr_t arg_i = (uintptr_t)drwrap_get_arg(wrapcxt, i);
        append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ARG, arg_i);
    }
}

// NOTE: try to avoid invoking any code that could be traced by func_post_hook
//       (e.g., stl, libc, etc.)
static void
func_post_hook(void *wrapcxt, void *user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    size_t idx = (size_t)user_data;
    func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs, idx);
    uintptr_t retval = (uintptr_t)drwrap_get_retval(wrapcxt);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ID, (uintptr_t)f->id);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_RETVAL, retval);
}

static void
instru_funcs_module_load(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (drcontext == NULL || mod == NULL)
        return;
    dr_log(NULL, DR_LOG_ALL, 1,
           "instru_funcs_module_load start=%p, mod->full_path=%s\n",
           mod->start, mod->full_path);

    for (size_t i = 0; i < funcs.entries; i++) {
        func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs, i);
        size_t offset;
        if (drsym_lookup_symbol(mod->full_path, f->name, &offset,
                                DRSYM_DEMANGLE) == DRSYM_SUCCESS) {
            dr_log(NULL, DR_LOG_ALL, 1,
                   "Found and trace func name=%s, id=%d, arg_num=%d\n",
                   f->name, f->id, f->arg_num);
            app_pc f_pc = mod->start + offset;
            drwrap_wrap_ex(f_pc, func_pre_hook, func_post_hook, (void *)i, 0);
        }
    }
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
    }
    while (pos != std::string::npos);
    return vec;
}

static bool
id_existed(int id)
{
    for (uint i = 0; i < funcs.entries; i++) {
        func_metadata_t *f = (func_metadata_t *)drvector_get_entry(&funcs, i);
        if (id == f->id)
            return true;
    }
    return false;
}

static void
get_funcs_str_and_sep(std::string &func_str, std::string &sep)
{
    func_str = op_record_function.get_value();
    sep = op_record_function.get_value_separator();
    if (op_record_heap.get_value()) {
        DR_ASSERT(sep == op_record_heap_value.get_value_separator());
        func_str += func_str.empty() ?
            op_record_heap_value.get_value() :
            sep + op_record_heap_value.get_value();
    }
}

DR_EXPORT
bool
func_trace_init(func_trace_append_entry_t append_entry_,
                process_fatal_t process_fatal_)
{
    if (dr_atomic_add32_return_sum(&func_trace_init_count, 1) > 1)
        return true;

    std::string funcs_str, sep;
    get_funcs_str_and_sep(funcs_str, sep);
    // If there is no function specified to trace,
    // then the whole func_trace module doesn't have to do anything.
    if (funcs_str.empty())
        return true;

    if (!drvector_init(&funcs, 32, false, free_func_entry)) {
        DR_ASSERT(false);
        goto failed;
    }

    append_entry = append_entry_;
    process_fatal = process_fatal_;

    for (auto &single_op_value : split_by(funcs_str, sep)) {
        auto items = split_by(single_op_value, PATTERN_SEPARATOR);
        if (items.size() != 3) {
            process_fatal("Usage error: -record_function or -record_heap_value"
                          "was not passed a triplet");
        }
        std::string name = items[0];
        int id = atoi(items[1].c_str());
        int arg_num = atoi(items[2].c_str());
        if (id_existed(id)) {
            process_fatal("Usage error: duplicated function id in"
                          "-record_function or -record_heap_value");
        }
        dr_log(NULL, DR_LOG_ALL, 1, "Trace func name=%s, id=%d, arg_num=%d\n",
               name.c_str(), id, arg_num);
        drvector_append(&funcs, create_func_metadata(name, id, arg_num));
    }

    if (!(drsym_init(0) == DRSYM_SUCCESS)) {
        DR_ASSERT(false);
        goto failed;
    }
    if (!drwrap_init()) {
        DR_ASSERT(false);
        goto failed;
    }
    if (!drmgr_register_module_load_event(instru_funcs_module_load)) {
        DR_ASSERT(false);
        goto failed;
    }

    return true;
failed:
    func_trace_exit();
    return false;
}

DR_EXPORT
void
func_trace_exit()
{
    if (dr_atomic_add32_return_sum(&func_trace_init_count, -1) != 0)
        return;

    std::string funcs_str, sep;
    get_funcs_str_and_sep(funcs_str, sep);
    if (funcs_str.empty())
        return;
    if (!drvector_delete(&funcs))
        DR_ASSERT(false);
    if (!drmgr_unregister_module_load_event(instru_funcs_module_load) ||
        !(drsym_exit() == DRSYM_SUCCESS))
        DR_ASSERT(false);
    drwrap_exit();
}
