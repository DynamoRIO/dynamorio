/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
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

/* Basic Block Duplicator API Sample:
 * hot_bbcount.c
 *
 * Reports the dynamic execution count of hot basic blocks.
 * Illustrates how to use drbbdup to create different versions of
 * basic block instrumentation.
 *
 * Two cases of instrumentation are set for a basic block. The first
 * version is executed when a basic block is cold. The basic block's hit
 * count is recorded by performing a clean call in this instrumentation case.
 * The second version is executed when the basic block has reached the appropriate
 * hit count (i.e., it is now considered hot). Code is inserted to count the
 * execution of the hot basic block similar to the bbcount client.
 */

#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drbbdup.h"
#include "drx.h"
#include "hashtable.h"

/* Start counting once a bb has been executed at least 1000 times. */
#define HIT_THRESHOLD 1000

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

/* we only have a global count */
static int global_count;

/* Global hash table to keep track of the hit count of cold basic blocks. */
static hashtable_t hit_count_table;
#define HASH_BITS 13

static reg_id_t tls_raw_reg;
static uint tls_raw_offset;

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "%10d hot basic block executions\n",
                      global_count);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    /* Delete hit count table. */
    hashtable_delete(&hit_count_table);

    dr_raw_tls_cfree(tls_raw_offset, 1);
    drbbdup_exit();
    drx_exit();
    drreg_exit();
    drmgr_exit();
}

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    /* Init hit count. */
    app_pc bb_pc = instr_get_app_pc(instrlist_first_app(bb));
    hashtable_lock(&hit_count_table);
    if (hashtable_lookup(&hit_count_table, bb_pc) == NULL) {
        /* If hit count is not mapped to this bb, then add a new count to the table. */
        uint *hit_count = dr_global_alloc(sizeof(uint));
        *hit_count = HIT_THRESHOLD;
        hashtable_add(&hit_count_table, bb_pc, hit_count);
    }
    hashtable_unlock(&hit_count_table);

    /* Enable duplication for all basic blocks. */
    *enable_dups = true;

    /* Disable dynamic handling. */
    *enable_dynamic_handling = false;

    /* Register the case encoding for counting the execution of hot basic blocks. */
    drbbdup_register_case_encoding(drbbdup_ctx, (uintptr_t) true /* hot */);

    /* Set the default case encoding for tracking the hit count of basic blocks. */
    return 0; /* cold */
}

static void
analyse_orig_bb(void *drcontext, void *tag, instrlist_t *bb, void *user_data,
                IN void **orig_analysis_data)
{
    /* Extract bb_pc and store it as analysis data. */
    app_pc *bb_pc = dr_thread_alloc(drcontext, sizeof(app_pc));
    *bb_pc = instr_get_app_pc(instrlist_first_app(bb));
    *orig_analysis_data = bb_pc;
}

static void
destroy_orig_analysis(void *drcontext, void *user_data, void *bb_pc)
{
    /* Destroy the orig analysis data, particularly the pc of the bb. */
    DR_ASSERT(bb_pc != NULL);
    dr_thread_free(drcontext, bb_pc, sizeof(app_pc));
}

static void
encode(app_pc bb_pc)
{
    bool is_hot;
    hashtable_lock(&hit_count_table);
    uint *hit_count = hashtable_lookup(&hit_count_table, bb_pc);
    DR_ASSERT_MSG(hit_count != NULL, "hit count must be present");
    is_hot = *hit_count == 0;
    hashtable_unlock(&hit_count_table);

    byte *base = dr_get_dr_segment_base(tls_raw_reg);
    uintptr_t *runtime_case = (uintptr_t *)(base + tls_raw_offset);
    *runtime_case = (uintptr_t)is_hot;
}

static void
insert_encode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
              void *user_data, void *orig_analysis_data)
{
    app_pc *bb_pc = (app_pc *)orig_analysis_data;
    dr_insert_clean_call(drcontext, bb, where, (void *)encode, false, 1,
                         OPND_CREATE_INTPTR(*bb_pc));
}

static void
register_hit(app_pc bb_pc)
{
    /* Decrement the hit count. Once it reaches zero,
     * basic block is considered as hot.
     */
    hashtable_lock(&hit_count_table);
    uint *hit_count = hashtable_lookup(&hit_count_table, bb_pc);
    DR_ASSERT_MSG(hit_count != NULL, "hit count must be present");
    DR_ASSERT_MSG(hit_count > 0, "bb cannot be hot");
    (*hit_count)--;
    hashtable_unlock(&hit_count_table);
}

static void
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, uintptr_t encoding, void *user_data,
                 void *orig_analysis_data, void *analysis_data)
{
    bool is_start;

    /* By default drmgr enables auto-predication, which predicates all instructions with
     * the predicate of the current instruction on ARM.
     * We disable it here because we want to unconditionally execute the following
     * instrumentation.
     */
    drmgr_disable_auto_predication(drcontext, bb);

    /* Determine whether the instr is the first instruction in the
     * currently considered basic block copy.
     */
    drbbdup_is_first_instr(drcontext, instr, &is_start);
    if (is_start) {
        /* Check if hot case. */
        if (encoding == 1) {
            /* racy update on the counter for better performance */
            drx_insert_counter_update(drcontext, bb, where /*insert always at where */,
                                      /* We're using drmgr, so these slots
                                       * here won't be used: drreg's slots will be.
                                       */
                                      SPILL_SLOT_MAX + 1, &global_count, 1, 0);

        } else {
            /* Basic block is cold. Therefore insert clean call to mark the hit. */
            app_pc *bb_pc = (app_pc *)orig_analysis_data;
            dr_insert_clean_call(drcontext, bb, where /* insert always at where */,
                                 (void *)register_hit, false, 1,
                                 OPND_CREATE_INTPTR(*bb_pc));
        }
    }
}

static void
destroy_hit_count(void *entry)
{
    uint *hit_count = (uint *)entry;
    dr_global_free(hit_count, sizeof(uint));
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t drreg_ops = { sizeof(drreg_ops), 1 /* max slots needed: aflags */,
                                  false };
    dr_set_client_name("DynamoRIO Sample Client 'hot_bbcount'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || !drx_init() || drreg_init(&drreg_ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);

    hashtable_init_ex(&hit_count_table, HASH_BITS, HASH_INTPTR, false /*!strdup*/,
                      false /*synchronization is external*/, destroy_hit_count, NULL,
                      NULL);

    /* Initialise an addressable TLS slot to contain the runtime case encoding. */
    if (!dr_raw_tls_calloc(&tls_raw_reg, &tls_raw_offset, 1 /* num of slots */, 0))
        DR_ASSERT(false);

    /* Initialise drbbdup. Essentially, drbbdup requires
     * the client to pass a set of call-back functions and
     * the memory operand that the dispatcher will use
     * to load the current runtime case encoding.
     */
    drbbdup_options_t drbbdup_ops = { 0 };
    drbbdup_ops.struct_size = sizeof(drbbdup_options_t);
    drbbdup_ops.set_up_bb_dups = set_up_bb_dups;
    drbbdup_ops.insert_encode = insert_encode;
    drbbdup_ops.analyze_orig = analyse_orig_bb;
    drbbdup_ops.destroy_orig_analysis = destroy_orig_analysis;
    drbbdup_ops.instrument_instr = instrument_instr;
    /* The operand referring to memory storing the current runtime case encoding. */
    drbbdup_ops.runtime_case_opnd =
        dr_raw_tls_opnd(dr_get_current_drcontext(), tls_raw_reg, tls_raw_offset);
    drbbdup_ops.non_default_case_limit = 1; /* Only one additional copy is needed. */
    drbbdup_ops.is_stat_enabled = false;

    if (drbbdup_init(&drbbdup_ops) != DRBBDUP_SUCCESS)
        DR_ASSERT(false);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'hot_bbcount' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client hot_bbcount is running\n");
    }
#endif
}
