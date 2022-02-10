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

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data);
static void
exit_event(void);

static int div_count = 0, div_p2_count = 0;
static void *count_mutex; /* for multithread support */

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
}

static void
exit_event(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %d div instructions\n"
                      "  of which %d were powers of 2\n",
                      div_count, div_p2_count);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    dr_mutex_destroy(count_mutex);
    drmgr_exit();
}

static void
callback(app_pc addr, uint divisor)
{
    /* instead of a lock could use atomic operations to
     * increment the counters */
    dr_mutex_lock(count_mutex);

    div_count++;

    /* check for power of 2 or zero */
    if ((divisor & (divisor - 1)) == 0)
        div_p2_count++;

    dr_mutex_unlock(count_mutex);
}

/* If instr is unsigned division, return true and set *opnd to divisor. */
static bool
instr_is_div(instr_t *instr, OUT opnd_t *opnd)
{
    int opc = instr_get_opcode(instr);
#if defined(X86)
    if (opc == OP_div) {
        *opnd = instr_get_src(instr, 0); /* divisor is 1st src */
        return true;
    }
#elif defined(AARCHXX)
    if (opc == OP_udiv) {
        *opnd = instr_get_src(instr, 1); /* divisor is 2nd src */
        return true;
    }
#else
#    error NYI
#endif
    return false;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* if find div, insert a clean call to our instrumentation routine */
    opnd_t opnd;
    if (instr_is_div(instr, &opnd)) {
        dr_insert_clean_call(drcontext, bb, instr, (void *)callback, false /*no fp save*/,
                             2, OPND_CREATE_INTPTR(instr_get_app_pc(instr)), opnd);
    }
    return DR_EMIT_DEFAULT;
}
