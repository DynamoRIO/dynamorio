/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Tests the drcallstack extension. */

#include "dr_api.h"
#include "drcallstack.h"
#include "drsyms.h"
#include "drwrap.h"
#include "client_tools.h"
#include "string.h"

static void
print_qualified_function_name(app_pc pc)
{
    module_data_t *mod = dr_lookup_module(pc);
    DR_ASSERT(mod != NULL);
    drsym_info_t sym_info;
#define MAX_FUNC_LEN 1024
    char name[MAX_FUNC_LEN];
    char file[MAXIMUM_PATH];
    sym_info.struct_size = sizeof(sym_info);
    sym_info.name = name;
    sym_info.name_size = MAX_FUNC_LEN;
    sym_info.file = file;
    sym_info.file_size = MAXIMUM_PATH;
    const char *func = "<unknown>";
    drsym_error_t sym_res =
        drsym_lookup_address(mod->full_path, pc - mod->start, &sym_info, DRSYM_DEMANGLE);
    if (sym_res == DRSYM_SUCCESS)
        func = sym_info.name;
    dr_fprintf(STDERR, "%s!%s\n", dr_module_preferred_name(mod), func);
    dr_free_module_data(mod);
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);
    drcallstack_walk_t *walk;
    drcallstack_status_t res = drcallstack_init_walk(mc, &walk);
    DR_ASSERT(res == DRCALLSTACK_SUCCESS);
    drcallstack_frame_t frame = {
        sizeof(frame),
    };
    int count = 0;
    print_qualified_function_name(drwrap_get_func(wrapcxt));
    do {
        res = drcallstack_next_frame(walk, &frame);
        if (res != DRCALLSTACK_SUCCESS)
            break;
        print_qualified_function_name(frame.pc);
        ++count;
    } while (res == DRCALLSTACK_SUCCESS);
    DR_ASSERT(res == DRCALLSTACK_NO_MORE_FRAMES);
    res = drcallstack_cleanup_walk(walk);
    DR_ASSERT(res == DRCALLSTACK_SUCCESS);
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
    /* Nothing yet. */
}

static void
event_exit(void)
{
    drcallstack_exit();
    drwrap_exit();
    drsym_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drcallstack_options_t ops = {
        sizeof(ops),
    };
    if (!drwrap_init() || drcallstack_init(&ops) != DRCALLSTACK_SUCCESS ||
        drsym_init(0) != DRSYM_SUCCESS)
        DR_ASSERT(false);
    dr_register_exit_event(event_exit);

    /* Ensure callstacks work w/o a full mcontext. */
    drwrap_set_global_flags(DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS);

    module_data_t *exe = dr_get_main_module();
    size_t modoffs;
    drsym_error_t sym_res =
        drsym_lookup_symbol(exe->full_path, "qux", &modoffs, DRSYM_DEMANGLE);
    DR_ASSERT(sym_res == DRSYM_SUCCESS);
    app_pc towrap = exe->start + modoffs;
    bool ok = drwrap_wrap(towrap, wrap_pre, wrap_post);
    dr_free_module_data(exe);
}
