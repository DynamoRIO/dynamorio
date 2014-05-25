/* ******************************************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox("%s", msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'

static uint64 app_count;   /* number of instructions executed in app */
static uint64 lib_count;   /* number of instructions executed in libs */
static uint   app2lib;     /* number of transfers (calls/jmps) from app to lib */
static uint   lib2app;     /* number of transfers (calls/jmps )from lib to app */
static app_pc app_base;
static app_pc app_end;

/* Simple clean calls that will each be automatically inlined because it has only
 * one argument and contains no calls to other functions.
 */
static void app_update(uint num_instrs) { app_count += num_instrs; }
static void lib_update(uint num_instrs) { lib_count += num_instrs; }

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

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);

DR_EXPORT void
dr_init(client_id_t id)
{
    module_data_t *appmod  = dr_get_main_module();

    DR_ASSERT(appmod != NULL);
    app_base = appmod->start;
    app_end  = appmod->end;
    dr_free_module_data(appmod);

    /* register events */
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'modxfer_app2lib' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
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
    uint64 total_xfer  = (app2lib + lib2app);
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "\t%10llu instructions executed\n"
                      "\t%10llu (%2.3f%%) in app\n"
                      "\t%10llu (%2.3f%%) in lib,\n"
                      "\t%10llu (%2.3f%%) call/jmp between app and lib\n"
                      "\t%10u app call/jmp to lib\n"
                      "\t%10u lib call/jmp to app\n",
                      total_count,
                      app_count,  100*(float)app_count/total_count,
                      lib_count,  100*(float)lib_count/total_count,
                      total_xfer, 100*(float)total_xfer/total_count,
                      app2lib, lib2app);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr, *mbr = NULL;
    uint num_instrs;
    bool bb_in_app;

#ifdef VERBOSE
    dr_printf("in dynamorio_basic_block(tag="PFX")\n", tag);
# ifdef VERBOSE_VERBOSE
    instrlist_disassemble(drcontext, tag, bb, STDOUT);
# endif
#endif

    for (instr  = instrlist_first(bb), num_instrs = 0;
         instr != NULL;
         instr = instr_get_next(instr)) {
        /* only care about app instr */
        if (!instr_ok_to_mangle(instr))
            continue;
        num_instrs++;
        /* Assuming most of the transfers between app and lib are paired, we
         * instrument indirect branches but not returns for better performance.
         */
        if (instr_is_mbr(instr) && !instr_is_return(instr))
            mbr = instr;
    }

    if (dr_fragment_app_pc(tag) >= app_base &&
        dr_fragment_app_pc(tag) <  app_end)
        bb_in_app = true;
    else
        bb_in_app = false;
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb),
                         (void *)(bb_in_app ? app_update : lib_update),
                         false /* save fpstate */, 1,
                         OPND_CREATE_INT32(num_instrs));
    if (mbr != NULL) {
        dr_insert_mbr_instrumentation(drcontext, bb, mbr,
                                      (void *)(bb_in_app ? app_mbr : lib_mbr),
                                      SPILL_SLOT_1);
    }

#if defined(VERBOSE) && defined(VERBOSE_VERBOSE)
    dr_printf("Finished instrumenting dynamorio_basic_block(tag="PFX")\n", tag);
    instrlist_disassemble(drcontext, tag, bb, STDOUT);
#endif
    return DR_EMIT_DEFAULT;
}
