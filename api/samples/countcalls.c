/* **********************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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
 * countcalls.c
 *
 * Reports the dynamic execution count for direct calls, indirect
 * calls, and returns in the target application.  Illustrates how to
 * perform performant inline increments and use per-thread data
 * structures.
 */

#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

/* keep separate counters for each thread, in this thread-local data
 * structure:
 */
typedef struct {
    int num_direct_calls;
    int num_indirect_calls;
    int num_returns;
} per_thread_t;

static int tls_idx;

/* keep a global count as well */
static per_thread_t global_count = { 0 };

static void
event_exit(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);
static dr_emit_flags_t
event_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                  bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    dr_set_client_name("DynamoRIO Sample Client 'countcalls'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);
    /* register events */
    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_instruction, NULL);
    tls_idx = drmgr_register_tls_field();

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'countcalls' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client countcalls is running\n");
    }
#endif
}

static void
display_results(per_thread_t *data, const char *thread_note)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "%sInstrumentation results:\n"
                      "  saw %d direct calls\n"
                      "  saw %d indirect calls\n"
                      "  saw %d returns\n",
                      thread_note, data->num_direct_calls, data->num_indirect_calls,
                      data->num_returns);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
}

static void
event_exit(void)
{
    display_results(&global_count, "");
    if (!drmgr_unregister_bb_insertion_event(event_instruction) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);
    drmgr_exit();
}

static void
event_thread_init(void *drcontext)
{
    /* create an instance of our data structure for this thread */
    per_thread_t *data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, data);
    data->num_direct_calls = 0;
    data->num_indirect_calls = 0;
    data->num_returns = 0;
    dr_log(drcontext, DR_LOG_ALL, 1, "countcalls: set up for thread " TIDFMT "\n",
           dr_get_thread_id(drcontext));
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    char msg[512];
    int len;

    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]), "Thread %d exited - ",
                      dr_get_thread_id(drcontext));
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);

    /* display thread private counts data */
    display_results(data, msg);

    /* clean up memory */
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
insert_counter_update(void *drcontext, instrlist_t *bb, instr_t *where, int offset)
{
    /* Since the inc instruction clobbers 5 of the arithmetic eflags,
     * we have to save them around the inc. We could be more efficient
     * by not bothering to save the overflow flag and constructing our
     * own sequence of instructions to save the other 5 flags (using
     * lahf).
     */
    if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return;
    }

    /* Increment the global counter using the lock prefix to make it atomic
     * across threads. It would be cheaper to aggregate the thread counters
     * in the exit events, but this sample is intended to illustrate inserted
     * instrumentation.
     */
    instrlist_meta_preinsert(
        bb, where,
        LOCK(INSTR_CREATE_inc(
            drcontext, OPND_CREATE_ABSMEM(((byte *)&global_count) + offset, OPSZ_4))));

    /* Increment the thread private counter. */
    if (dr_using_all_private_caches()) {
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        /* private caches - we can use an absolute address */
        instrlist_meta_preinsert(
            bb, where,
            INSTR_CREATE_inc(drcontext,
                             OPND_CREATE_ABSMEM(((byte *)&data) + offset, OPSZ_4)));
    } else {
        /* shared caches - we must indirect via thread local storage */
        reg_id_t scratch;
        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch) != DRREG_SUCCESS)
            DR_ASSERT(false);
        drmgr_insert_read_tls_field(drcontext, tls_idx, bb, where, scratch);
        instrlist_meta_preinsert(
            bb, where, INSTR_CREATE_inc(drcontext, OPND_CREATE_MEM32(scratch, offset)));
        if (drreg_unreserve_register(drcontext, bb, where, scratch) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }

    if (drreg_unreserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
        DR_ASSERT(false); /* cannot recover */
}

static dr_emit_flags_t
event_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                  bool for_trace, bool translating, void *user_data)
{
    /* ignore tool-inserted instrumentation */
    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;

    /* instrument calls and returns -- ignore far calls/rets */
    if (instr_is_call_direct(instr)) {
        insert_counter_update(drcontext, bb, instr,
                              offsetof(per_thread_t, num_direct_calls));
    } else if (instr_is_call_indirect(instr)) {
        insert_counter_update(drcontext, bb, instr,
                              offsetof(per_thread_t, num_indirect_calls));
    } else if (instr_is_return(instr)) {
        insert_counter_update(drcontext, bb, instr, offsetof(per_thread_t, num_returns));
    }

    return DR_EMIT_DEFAULT;
}
