/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * opcode_count.c
 *
 * Reports the dynamic execution count of all instructions with a particular opcode.
 * Illustrates how to to use drmgr to register opcode events and inline locked
 * counter code.
 */

#include <cstdint>  /* for intptr */
#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"
#include "droption.h"

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

static droption_t<int>
    opcode(DROPTION_SCOPE_CLIENT, "opcode", OP_add, "The opcode to count",
           "The opcode to consider when counting the number of times "
           "the instruction is executed. Default opcode is set to add.");

static uintptr_t global_opcode_count = 0;
static uintptr_t global_total_count = 0;

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]), "%u/%u instructions executed.",
                      global_opcode_count, global_total_count);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    drx_exit();
    drreg_exit();
    drmgr_exit();
}

static dr_emit_flags_t
event_opcode_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                         bool for_trace, bool translating, void *user_data)
{
    /* Insert code to update the counter for tracking the number of executed instructions
     * having the specified opcode.
     */
    drx_insert_counter_update(
        drcontext, bb, inst,
        /* We're using drmgr, so these slots
         * here won't be used: drreg's slots will be.
         */
        static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
        IF_AARCHXX_(static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1)) &
            global_opcode_count,
        1,
        /* TODO i#4215: DRX_COUNTER_LOCK is not yet supported on ARM. */
        IF_X86_ELSE(DRX_COUNTER_LOCK, 0));

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    intptr_t bb_size = (intptr_t)drx_instrlist_app_size(bb);
    *user_data = (void *)bb_size;
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    /* By default drmgr enables auto-predication, which predicates all instructions with
     * the predicate of the current instruction on ARM.
     * We disable it here because we want to unconditionally execute the following
     * instrumentation.
     */
    drmgr_disable_auto_predication(drcontext, bb);
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    intptr_t bb_size = (intptr_t)user_data;

    drx_insert_counter_update(
        drcontext, bb, inst,
        /* We're using drmgr, so these slots
         * here won't be used: drreg's slots will be.
         */
        static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
        IF_AARCHXX_(static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1)) &
            global_total_count,
        (int)bb_size,
        /* TODO i#4215: DRX_COUNTER_LOCK is not yet supported on ARM. */
        IF_X86_ELSE(DRX_COUNTER_LOCK, 0));

    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* Parse command-line options. */
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL))
        DR_ASSERT(false);

    /* Get opcode and check if valid. */
    int valid_opcode = opcode.get_value();
    if (valid_opcode < OP_FIRST || valid_opcode > OP_LAST) {
#ifdef SHOW_RESULTS
        dr_fprintf(STDERR, "Error: give a valid opcode as a parameter.\n");
        dr_abort();
#endif
    }

    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed: aflags*/, false };
    dr_set_client_name("DynamoRIO Sample Client 'opcode_count'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || !drx_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* Register opcode event. */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_opcode_instrumentation_event(event_opcode_instruction,
                                                     valid_opcode, NULL, NULL) ||
        !drmgr_register_bb_instrumentation_event(event_bb_analysis, event_app_instruction,
                                                 NULL))
        DR_ASSERT(false);

    /* Make it easy to tell, by looking at log file, which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'opcode_count' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* Ask for best-effort printing to cmd window. This must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client opcode_count is running and considering opcode: %d.\n",
                   valid_opcode);
    }
#endif
}
