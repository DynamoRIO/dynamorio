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

static app_pc trace_pc = NULL;
static void *count_mutex; /* for multithread support */

drdbg_status_t
cmd_handler(char *buf, ssize_t len, char **outbuf, ssize_t *outlen)
{
    dr_fprintf(STDERR, "HANDLER CALLED!! %s\n", buf);
    if (!strncmp("itrace", buf, strlen("itrace"))) {
        trace_pc = (app_pc)strtoul(buf+strlen("itrace "), NULL, 16);
        dr_fprintf(STDERR, "trace_pc: "PIFX"\n", trace_pc);
        return DRDBG_SUCCESS;
    }
    return DRDBG_ERROR;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'div'", "http://dynamorio.org/issues");

    if (!drmgr_init())
        DR_ASSERT(false);
    dr_register_exit_event(exit_event);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
    count_mutex = dr_mutex_create();

    /* Register command handler with drdbg */
    drdbg_api_register_cmd(cmd_handler);
}

static void
exit_event(void)
{
    dr_mutex_destroy(count_mutex);
    drmgr_exit();
}

static void
callback(app_pc pc)
{
    void *drcontext = dr_get_current_drcontext();
    instr_t instr;
    opnd_t opnd;
    dr_mcontext_t mc;

    dr_mutex_lock(count_mutex);

    /* get context */
    mc.flags = DR_MC_INTEGER|DR_MC_CONTROL;
    mc.size = sizeof(dr_mcontext_t);
    dr_get_mcontext(drcontext, &mc);

    /* Decode instruction */
    instr_init(drcontext, &instr);
    decode(drcontext, pc, &instr);

    /* Print disassembly */
    instr_disassemble(drcontext, &instr, 2);
    dr_fprintf(STDERR, "\n");

    /* Print argument resolutions */
    opnd = instr_get_src(&instr, 0);
    if (opnd_is_reg(opnd)) {
        dr_fprintf(STDERR, "\t%s: %p", get_register_name(opnd_get_reg(opnd)),
                                         reg_get_value(opnd_get_reg(opnd), &mc));
    }
    opnd = instr_get_dst(&instr, 0);
    if (opnd_is_reg(opnd)) {
        dr_fprintf(STDERR, "\t%s: %p", get_register_name(opnd_get_reg(opnd)),
                                         reg_get_value(opnd_get_reg(opnd), &mc));
    }
    dr_fprintf(STDERR, "\n");

    dr_mutex_unlock(count_mutex);
}

static dr_emit_flags_t
event_app_instruction(void* drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* if the right pc, insert a clean call to our instrumentation routine */
    if (instr_get_app_pc(instr) == trace_pc) {
        dr_fprintf(STDERR, "Inserting clean call for "PIFX"\n", instr_get_app_pc(instr));
        dr_insert_clean_call(drcontext, bb, instr, (void *)callback,
                             false /*no fp save*/, 1,
                             OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
    }
    return DR_EMIT_DEFAULT;
}
