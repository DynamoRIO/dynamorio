/* **********************************************************
 * Copyright (c) 2021-2023 Google, Inc.  All rights reserved.
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

/* Illustrates using the drcallstack extension.
 *
 * The drcallstack extension only supports Linux in this release.
 * This sample wraps malloc and every time it's called it symbolizes and
 * prints the callstack.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drcallstack.h"
#include "drsyms.h"
#include "droption.h"
#include <string>

namespace dynamorio {
namespace samples {
namespace {

using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::droption_t;

static droption_t<std::string> trace_function(
    DROPTION_SCOPE_CLIENT, "trace_function", "malloc", "Name of function to trace",
    "The name of the function to wrap and print callstacks on every call.");

static void
print_qualified_function_name(app_pc pc)
{
    module_data_t *mod = dr_lookup_module(pc);
    if (mod == NULL) {
        // If we end up in assembly code or generated code we'll likely never
        // get out again without stack scanning or frame pointer walking or
        // other strategies not yet part of drcallstack.
        dr_fprintf(STDERR, "  <unknown module> @%p\n", pc);
        return;
    }
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
    dr_fprintf(STDERR, "  %s!%s\n", dr_module_preferred_name(mod), func);
    dr_free_module_data(mod);
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    dr_fprintf(STDERR, "%s called from:\n", trace_function.get_value().c_str());
    // Get the context.  The pc field is set by drwrap to the wrapped function
    // entry point.
    dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);
    // Walk the callstack.
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
    // The return value DRCALLSTACK_NO_MORE_FRAMES indicates a complete callstack.
    // Anything else indicates some kind of unwind info error.
    // If this code were used inside a larger tool it would be up to that tool
    // whether to record or act on the callstack quality.
    res = drcallstack_cleanup_walk(walk);
    DR_ASSERT(res == DRCALLSTACK_SUCCESS);
}

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    size_t modoffs;
    drsym_error_t sym_res = drsym_lookup_symbol(
        mod->full_path, trace_function.get_value().c_str(), &modoffs, DRSYM_DEMANGLE);
    if (sym_res == DRSYM_SUCCESS) {
        app_pc towrap = mod->start + modoffs;
        bool ok = drwrap_wrap(towrap, wrap_pre, NULL);
        DR_ASSERT(ok);
        dr_fprintf(STDERR, "wrapping %s!%s\n", mod->full_path,
                   trace_function.get_value().c_str());
    }
}

static void
module_unload_event(void *drcontext, const module_data_t *mod)
{
    size_t modoffs;
    drsym_error_t sym_res = drsym_lookup_symbol(
        mod->full_path, trace_function.get_value().c_str(), &modoffs, DRSYM_DEMANGLE);
    if (sym_res == DRSYM_SUCCESS) {
        app_pc towrap = mod->start + modoffs;
        bool ok = drwrap_unwrap(towrap, wrap_pre, NULL);
        DR_ASSERT(ok);
    }
}

static void
event_exit()
{
    drmgr_register_module_unload_event(module_unload_event);
    drcallstack_exit();
    drwrap_exit();
    drsym_exit();
}

} // namespace
} // namespace samples
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'callstack'",
                       "http://dynamorio.org/issues");
    // Parse our option.
    if (!dynamorio::droption::droption_parser_t::parse_argv(
            dynamorio::droption::DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL))
        DR_ASSERT(false);
    drcallstack_options_t ops = {
        sizeof(ops),
    };
    // Initialize the libraries we're using.
    if (!drwrap_init() || drcallstack_init(&ops) != DRCALLSTACK_SUCCESS ||
        drsym_init(0) != DRSYM_SUCCESS ||
        !drmgr_register_module_load_event(dynamorio::samples::module_load_event))
        DR_ASSERT(false);
    dr_register_exit_event(dynamorio::samples::event_exit);
    // Improve performance as we only need basic wrapping support.
    drwrap_set_global_flags(
        static_cast<drwrap_global_flags_t>(DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS));
}
