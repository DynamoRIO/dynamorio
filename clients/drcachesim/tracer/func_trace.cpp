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

/* func_trace.cpp: code for recording function traces */

#include <vector>
#include <string>
#include "dr_api.h"
#include "drsyms.h"
#include "drwrap.h"
#include "drmgr.h"
#include "trace_entry.h"
#include "../common/options.h"
#include "func_trace.h"

typedef struct {
    std::string name;
    int id;
    int arg_num;
} func_metadata_t;

/* expected pattern "<function_name>:<function_id>:<arguments_num>" */
#define PATTERN_SEPARATOR ":"


static int func_trace_init_count;

static func_trace_append_entry_t append_entry;
static std::vector<func_metadata_t> funcs;


static void
func_pre_hook(void *wrapcxt, INOUT void **user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    func_metadata_t &f = funcs[(size_t)*user_data];
    app_pc retaddr = drwrap_get_retaddr(wrapcxt);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ID, (uintptr_t)f.id);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_RETADDR, (uintptr_t)retaddr);
    for (int i = 0; i < f.arg_num; i++) {
        uintptr_t arg_i = (uintptr_t)drwrap_get_arg(wrapcxt, i);
        append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ARG, arg_i);
    }
}

static void
func_post_hook(void *wrapcxt, void *user_data)
{
    void *drcontext = drwrap_get_drcontext(wrapcxt);
    if (drcontext == NULL)
        return;

    func_metadata_t &f = funcs[(size_t)user_data];
    uintptr_t retval = (uintptr_t)drwrap_get_retval(wrapcxt);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_ID, (uintptr_t)f.id);
    append_entry(drcontext, TRACE_MARKER_TYPE_FUNC_RETVAL, retval);
}

static void
instru_funcs_module_load(void *drcontext, const module_data_t *mod, bool loaded)
{
    size_t offset;
    drsym_error_t result;
    app_pc func_pc;

    if (drcontext == NULL || mod == NULL)
        return;
    dr_log(NULL, DR_LOG_ALL, 1, "instru_funcs_module_load start=%p, mod->full_path=%s\n",
           mod->start, mod->full_path);

    for (size_t i = 0; i < funcs.size(); i++) {
        auto &f = funcs[i];
        result = drsym_lookup_symbol(mod->full_path, f.name.c_str(), &offset,
                                     DRSYM_DEMANGLE);
        dr_log(NULL, DR_LOG_ALL, 1,
               "func_name=%s, func_id=%d, func_arg_num=%d, result=%d, offset=%d\n",
               f.name.c_str(), f.id, f.arg_num, result, offset);

        if (result == DRSYM_SUCCESS) {
            func_pc = mod->start + offset;
            drwrap_wrap_ex(func_pc, func_pre_hook, func_post_hook, (void *)i, 0);
        }
    }
}

static std::vector<std::string> split_by(std::string s, std::string sep)
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

DR_EXPORT
bool
func_trace_init(func_trace_append_entry_t append_entry_)
{
    if (op_record_function.get_value().empty())
        return false;
    if (dr_atomic_add32_return_sum(&func_trace_init_count, 1) > 1)
        return true;

    append_entry = append_entry_;
    funcs.clear();

    std::string value = op_record_function.get_value();
    std::string sep = " ";  // XXX: use separator in droption when the PR comes in
    for (auto &single_op_value : split_by(value, sep)) {
        auto items = split_by(single_op_value, PATTERN_SEPARATOR);
        DR_ASSERT(items.size() == 3);
        funcs.push_back({items[0], atoi(items[1].c_str()), atoi(items[2].c_str())});
    }

    if (!(drsym_init(0) == DRSYM_SUCCESS) ||
        !drwrap_init() ||
        !drmgr_register_module_load_event(instru_funcs_module_load))
        DR_ASSERT(false);

    return true;
}

DR_EXPORT
void
func_trace_exit()
{
    if (dr_atomic_add32_return_sum(&func_trace_init_count, -1) != 0)
        return;

    funcs.clear();

    if (!drmgr_unregister_module_load_event(instru_funcs_module_load) ||
        !(drsym_exit()== DRSYM_SUCCESS))
        DR_ASSERT(false);
    drwrap_exit();
}
