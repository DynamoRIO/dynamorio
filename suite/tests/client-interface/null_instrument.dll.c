/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

/* Test the -null_instrument_list option.
 */

#include "dr_api.h"
#include "client_tools.h"

#include <string.h>

static bool found_appdll_in_trace;

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    const char *name = dr_module_preferred_name(info);
    if (strstr(name, "appdll") != NULL) {
        /* Test setting the null instrument module list from the module load
         * event, since that's when clients will probably decide whether to
         * instrument or not.
         */
        bool success, should_instrument;
        should_instrument = dr_module_should_instrument(info->handle);
        ASSERT(should_instrument); /* By default, should be yes. */
        success =
            dr_module_set_should_instrument(info->handle, false /*!should_instrument*/);
        ASSERT(success);
        should_instrument = dr_module_should_instrument(info->handle);
        ASSERT(!should_instrument);
    }
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        app_pc pc = instr_get_app_pc(instr);
        module_data_t *mod = dr_lookup_module(pc);
        if (mod != NULL && strstr(dr_module_preferred_name(mod), "appdll") != NULL) {
            size_t modoffs = pc - mod->start;
            dr_fprintf(STDERR, "appdll pc appeared in bb event: 0x%08lx %s\n", modoffs,
                       dr_module_preferred_name(mod));
        }
        dr_free_module_data(mod);
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_trace(void *drcontext, void *tag, instrlist_t *trace, bool translating)
{
    instr_t *instr;
    for (instr = instrlist_first(trace); instr != NULL; instr = instr_get_next(instr)) {
        app_pc pc = instr_get_app_pc(instr);
        module_data_t *mod = dr_lookup_module(pc);
        if (mod != NULL && strstr(dr_module_preferred_name(mod), "appdll") != NULL) {
            found_appdll_in_trace = true;
        }
        dr_free_module_data(mod);
    }
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    uint64 enable_traces = 1;
    if (!dr_get_integer_option("enable_traces", &enable_traces))
        dr_fprintf(STDERR, "dr_get_integer_option failed\n");
    if (enable_traces != 0 && !found_appdll_in_trace)
        dr_fprintf(STDERR, "didn't find appdll in trace\n");
}

DR_EXPORT void
dr_init(client_id_t client_id)
{
    dr_register_module_load_event(event_module_load);
    dr_register_bb_event(event_bb);
    dr_register_trace_event(event_trace);
    dr_register_exit_event(event_exit);
}
