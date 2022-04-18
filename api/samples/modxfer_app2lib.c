/* ******************************************************************************
 * Copyright (c) 2013-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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
 * modxfer_app2lib.c
 *
 * - Reports the dynamic count of total number of instructions executed in
 *   application executable and other libraries, and the number of transfers
 *   between app and other libraries.
 * - Illustrates how to perform performant clean calls.
 * - Demonstrates effect of clean call optimization and auto-inlining with
 *   different -opt_cleancall values.
 * - Illustrates how to perform different instrumentation on different modules.
 */

#include "dr_api.h"
#include "drmgr.h"

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox("%s", msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

static uint64 app_count; /* number of instructions executed in app */
static uint64 lib_count; /* number of instructions executed in libs */
static uint app2lib;     /* number of transfers (calls/jmps) from app to lib */
static uint lib2app;     /* number of transfers (calls/jmps )from lib to app */
static app_pc app_base;
static app_pc app_end;

/* Simple clean calls that will each be automatically inlined because it has only
 * one argument and contains no calls to other functions.
 */
static void
app_update(uint num_instrs)
{
    app_count += num_instrs;
}
static void
lib_update(uint num_instrs)
{
    lib_count += num_instrs;
}

/* Simple clean calls with two arguments will not be inlined, but the context
 * switch can be optimized for better performance.
 */
static void
app_mbr(app_pc instr_addr, app_pc target_addr)
{
    /* update count if target is not in app */
    if (target_addr >= app_end || target_addr < app_base)
        app2lib++;
}

static void
lib_mbr(app_pc instr_addr, app_pc target_addr)
{
    /* update count if target is in app */
    if (target_addr >= app_base && target_addr < app_end)
        lib2app++;
}

static void
event_exit(void);

static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data);

static dr_emit_flags_t
event_insert_instrumentation(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                             bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *appmod;
    dr_set_client_name("DynamoRIO Sample Client 'modxfer_app2lib'",
                       "http://dynamorio.org/issues");
    appmod = dr_get_main_module();
    DR_ASSERT(appmod != NULL);
    app_base = appmod->start;
    app_end = appmod->end;
    dr_free_module_data(appmod);

    if (!drmgr_init())
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_analyze_bb,
                                                 event_insert_instrumentation, NULL))
        DR_ASSERT(false);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'modxfer_app2lib' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client modxfer_app2lib is running\n");
    }
#endif
}

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    uint64 total_count = app_count + lib_count;
    /* We only instrument indirect calls/jmps, and assume that
     * there would be a return paired with indirect calls/jmps.
     */
    uint64 total_xfer = (app2lib + lib2app);
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "\t%10llu instructions executed\n"
                      "\t%10llu (%2.3f%%) in app\n"
                      "\t%10llu (%2.3f%%) in lib,\n"
                      "\t%10llu (%2.3f%%) call/jmp between app and lib\n"
                      "\t%10u app call/jmp to lib\n"
                      "\t%10u lib call/jmp to app\n",
                      total_count, app_count, 100 * (float)app_count / total_count,
                      lib_count, 100 * (float)lib_count / total_count, total_xfer,
                      100 * (float)total_xfer / total_count, app2lib, lib2app);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    if (!drmgr_unregister_bb_instrumentation_event(event_analyze_bb))
        DR_ASSERT(false);
    drmgr_exit();
}

/* This event is passed the instruction list for the whole bb. */
static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data)
{
    /* Count the instructions and pass the result to event_insert_instrumentation. */
    instr_t *instr;
    uint num_instrs;
    for (instr = instrlist_first_app(bb), num_instrs = 0; instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
    }
    *(uint *)user_data = num_instrs;
    return DR_EMIT_DEFAULT;
}

/* This event is called separately for each individual instruction in the bb. */
static dr_emit_flags_t
event_insert_instrumentation(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                             bool for_trace, bool translating, void *user_data)
{
    bool bb_in_app;
    if (dr_fragment_app_pc(tag) >= app_base && dr_fragment_app_pc(tag) < app_end)
        bb_in_app = true;
    else
        bb_in_app = false;

    if (drmgr_is_first_instr(drcontext, instr)) {
        uint num_instrs = (uint)(ptr_uint_t)user_data;
        dr_insert_clean_call(drcontext, bb, instr,
                             (void *)(bb_in_app ? app_update : lib_update),
                             false /* save fpstate */, 1, OPND_CREATE_INT32(num_instrs));
    }

    if (instr_is_mbr(instr) && !instr_is_return(instr)) {
        /* Assuming most of the transfers between app and lib are paired, we
         * instrument indirect branches but not returns for better performance.
         */
        dr_insert_mbr_instrumentation(
            drcontext, bb, instr, (void *)(bb_in_app ? app_mbr : lib_mbr), SPILL_SLOT_1);
    }

    return DR_EMIT_DEFAULT;
}
