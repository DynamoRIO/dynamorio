/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

/* Counts the number of dynamic div instruction for which the
 * divisor is a power of 2 (these are cases where div could be
 * strength reduced to a simple shift).  Demonstrates callout
 * based profiling with live operand values. */

#include "dr_api.h"
#include "drmgr.h"
#include "drdbg.h"
#include <string.h>

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'

static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag,
                                             instrlist_t *bb, instr_t *instr,
                                             bool for_trace, bool translating,
                                             void *user_data);
static void exit_event(void);

/* Runtime option: If set, only count instructions in the application itself */
static bool only_from_app = false;
/* Application module */
static app_pc exe_start;
static int xor_count = 0;
static void *count_mutex; /* for multithread support */

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    int i;
    dr_set_client_name("DynamoRIO Sample Client 'div'", "http://dynamorio.org/issues");
    /* Options */
    for (i = 1/*skip client*/; i < argc; i++) {
        if (strcmp(argv[i], "-only_from_app") == 0) {
            only_from_app = true;
        } else {
            dr_fprintf(STDERR, "UNRECOGNIZED OPTION: \"%s\"\n", argv[i]);
            DR_ASSERT_MSG(false, "invalid option");
        }
    }
    if (!drmgr_init())
        DR_ASSERT(false);
    dr_register_exit_event(exit_event);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
    count_mutex = dr_mutex_create();

    /* Get main module address */
    if (only_from_app) {
        module_data_t *exe = dr_get_main_module();
        if (exe != NULL)
            exe_start = exe->start;
        dr_free_module_data(exe);
    }

}

static void
exit_event(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %d non-zeroing xor instructions\n",
                      xor_count);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    dr_mutex_destroy(count_mutex);
    drmgr_exit();
}

static void
callback(app_pc addr)
{
    /* instead of a lock could use atomic operations to
     * increment the counters */
    dr_mutex_lock(count_mutex);

    xor_count++;

    dr_mutex_unlock(count_mutex);

    drdbg_api_break(addr);
}

static dr_emit_flags_t
event_app_instruction(void* drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* if find div, insert a clean call to our instrumentation routine */
    if (instr_get_opcode(instr) == OP_xor) {
        // Check if non-zeroing
        opnd_t a = instr_get_dst(instr, 0);
        opnd_t b = instr_get_src(instr, 0);
        if (opnd_is_reg(a) && opnd_is_reg(b) && opnd_get_reg(a) != opnd_get_reg(b)) {
            /* Only count in app BBs */
            if (only_from_app) {
                module_data_t *mod = dr_lookup_module(instr_get_app_pc(instr));
mod++;
                //if (mod != NULL) {
                //    bool from_exe = (mod->start == exe_start);
                //    dr_free_module_data(mod);
                //    if (!from_exe) {
                //        return DR_EMIT_DEFAULT;
                //    }
                //}
            }
            dr_insert_clean_call(drcontext, bb, instr, (void *)callback,
                                 false /*no fp save*/, 1,
                                 OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
        }
    }
    return DR_EMIT_DEFAULT;
}
