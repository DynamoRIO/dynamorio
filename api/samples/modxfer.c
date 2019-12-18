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
 * modxfer.c
 *
 * - Reports the dynamic count of total number of instructions executed
 *   and the number of transfers between modules via indirect branches.
 * - This is different from modxfer_app2lib.c as it counts all transfers between
 *   any modules while modxfer.c only counts transfers between app and other
 *   modules, i.e., not counting transfers between two libraries.
 * - modxfer and modxfer_app2lib are to demonstrate how we can simplify the client
 *   and improve the performance if we collect less information (modxfer_app2lib).
 * - We assume that most of cross module transfers happen via indirect branches,
 *   and most of them are paired, so we only instrument indrect branches
 *   but not returns for better performance.
 * - It is possible a direct branch with DGC or self-mod jumps between modules,
 *   they are ignored for now. They can be handled differently from indirect
 *   branch since the src and target are known at instrumentation time.
 */

#include "utils.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"
#include <string.h>

#define MAX_NUM_MODULES 0x1000
#define UNKNOW_MODULE_IDX (MAX_NUM_MODULES - 1)

typedef struct _module_array_t {
    app_pc base;
    app_pc end;
    bool loaded;
    module_data_t *info;
} module_array_t;
/* the last slot is where all non-module address go */
static module_array_t mod_array[MAX_NUM_MODULES];

static uint64 ins_count; /* number of instructions executed in total */
static void *mod_lock;
static int num_mods;
static uint xfer_cnt[MAX_NUM_MODULES][MAX_NUM_MODULES];
static uint64 mod_cnt[MAX_NUM_MODULES];
static file_t logfile;

static bool
module_data_same(const module_data_t *d1, const module_data_t *d2)
{
    if (d1->start == d2->start && d1->end == d2->end &&
        d1->entry_point == d2->entry_point &&
#ifdef WINDOWS
        d1->checksum == d2->checksum && d1->timestamp == d2->timestamp &&
#endif
        /* treat two modules w/ no name (there are some) as different */
        dr_module_preferred_name(d1) != NULL && dr_module_preferred_name(d2) != NULL &&
        strcmp(dr_module_preferred_name(d1), dr_module_preferred_name(d2)) == 0)
        return true;
    return false;
}

/* Simple clean calls with two arguments will not be inlined, but the context
 * switch can be optimized for better performance.
 */
static void
mbr_update(app_pc instr_addr, app_pc target_addr)
{
    int i, j;
    /* XXX: this can be optimized by walking a tree instead */
    /* find the source module */
    for (i = 0; i < num_mods; i++) {
        if (mod_array[i].loaded && mod_array[i].base <= instr_addr &&
            mod_array[i].end > instr_addr)
            break;
    }
    /* if cannot find a module, put it to the last */
    if (i == num_mods)
        i = UNKNOW_MODULE_IDX;
    /* find the target module */
    /* quick check if it is the same module */
    if (i < UNKNOW_MODULE_IDX && mod_array[i].base <= target_addr &&
        mod_array[i].end > target_addr) {
        j = i;
    } else {
        for (j = 0; j < num_mods; j++) {
            if (mod_array[j].loaded && mod_array[j].base <= target_addr &&
                mod_array[j].end > target_addr)
                break;
        }
        /* if cannot find a module, put it to the last */
        if (j == num_mods)
            j = UNKNOW_MODULE_IDX;
    }
    /* this is a racy update, but should be ok to be a few number off */
    xfer_cnt[i][j]++;
}

static void
event_exit(void);

static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data);

static dr_emit_flags_t
event_insert_instrumentation(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                             bool for_trace, bool translating, void *user_data);

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded);

static void
event_module_unload(void *drcontext, const module_data_t *info);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* We need no drreg slots ourselves, but we initialize drreg as we call
     * drreg_restore_app_values(), required since drx_insert_counter_update()
     * uses drreg when drmgr is used.
     */
    drreg_options_t ops = { sizeof(ops) };
    dr_set_client_name("DynamoRIO Sample Client 'modxfer'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);
    drx_init();
    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_analyze_bb,
                                                 event_insert_instrumentation, NULL))
        DR_ASSERT(false);
    drmgr_register_module_load_event(event_module_load);
    drmgr_register_module_unload_event(event_module_unload);

    mod_lock = dr_mutex_create();

    logfile = log_file_open(id, NULL /* drcontext */, NULL /* path */, "modxfer",
#ifndef WINDOWS
                            DR_FILE_CLOSE_ON_FORK |
#endif
                                DR_FILE_ALLOW_LARGE);

    DR_ASSERT(logfile != INVALID_FILE);
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'modxfer' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client modxfer is running\n");
    }
#endif
}

static void
event_exit(void)
{
    int i;
    char msg[512];
    int len;
    int j;
    uint64 xmod_xfer = 0;
    uint64 self_xfer = 0;
    for (i = 0; i < num_mods; i++) {
        dr_fprintf(logfile, "module %3d: %s\n", i,
                   dr_module_preferred_name(mod_array[i].info) == NULL
                       ? "<unknown>"
                       : dr_module_preferred_name(mod_array[i].info));
        dr_fprintf(logfile, "%20llu instruction executed\n", mod_cnt[i]);
    }
    if (mod_cnt[UNKNOW_MODULE_IDX] != 0) {
        dr_fprintf(logfile, "unknown modules:\n%20llu instruction executed\n",
                   mod_cnt[UNKNOW_MODULE_IDX]);
    }
    for (i = 0; i < MAX_NUM_MODULES; i++) {
        for (j = 0; j < num_mods; j++) {
            if (xfer_cnt[i][j] != 0) {
                dr_fprintf(logfile, "mod %3d => mod %3d: %8u\n", i, j, xfer_cnt[i][j]);
                if (i == j)
                    self_xfer += xfer_cnt[i][j];
                else
                    xmod_xfer += xfer_cnt[i][j];
            }
        }
    }
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "\t%10llu instructions executed\n"
                      "\t%10llu (%2.3f%%) cross module indirect branches\n"
                      "\t%10llu (%2.3f%%) intra-module indirect branches\n",
                      ins_count, xmod_xfer, 100 * (float)xmod_xfer / ins_count, self_xfer,
                      100 * (float)self_xfer / ins_count);
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(msg);
#ifdef SHOW_RESULTS
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    dr_fprintf(logfile, "%s\n", msg);
    dr_mutex_lock(mod_lock);
    for (i = 0; i < num_mods; i++) {
        DR_ASSERT(mod_array[i].info != NULL);
        dr_free_module_data(mod_array[i].info);
    }
    dr_mutex_unlock(mod_lock);
    dr_mutex_destroy(mod_lock);
    log_file_close(logfile);
    drx_exit();
    if (!drmgr_unregister_bb_instrumentation_event(event_analyze_bb) ||
        !drmgr_unregister_module_load_event(event_module_load) ||
        !drmgr_unregister_module_unload_event(event_module_unload) ||
        drreg_exit() != DRREG_SUCCESS)
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
    if (drmgr_is_first_instr(drcontext, instr)) {
        uint num_instrs = (uint)(ptr_uint_t)user_data;
        int i;
        app_pc bb_addr = dr_fragment_app_pc(tag);
        for (i = 0; i < num_mods; i++) {
            if (mod_array[i].loaded && mod_array[i].base <= bb_addr &&
                mod_array[i].end > bb_addr)
                break;
        }
        if (i == num_mods)
            i = UNKNOW_MODULE_IDX;
        /* We pass SPILL_SLOT_MAX+1 as drx will use drreg for spilling. */
        drx_insert_counter_update(drcontext, bb, instr, SPILL_SLOT_MAX + 1,
                                  (void *)&mod_cnt[i], num_instrs, DRX_COUNTER_64BIT);
        drx_insert_counter_update(drcontext, bb, instr, SPILL_SLOT_MAX + 1,
                                  (void *)&ins_count, num_instrs, DRX_COUNTER_64BIT);
    }

    if (instr_is_mbr(instr) && !instr_is_return(instr)) {
        /* Assuming most of the transfers between modules are paired, we
         * instrument indirect branches but not returns for better performance.
         * We assume that most cross module transfers happens via indirect
         * branches.
         * Direct branch with DGC or self-modify may also cross modules, but
         * it should be ok to ignore, and we can handle them more efficiently.
         */
        /* dr_insert_mbr_instrumentation is going to read app values, so we need a
         * drreg lazy restore "barrier" here.
         */
        drreg_status_t res =
            drreg_restore_app_values(drcontext, bb, instr, instr_get_target(instr), NULL);
        DR_ASSERT(res == DRREG_SUCCESS || res == DRREG_ERROR_NO_APP_VALUE);
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (void *)mbr_update,
                                      SPILL_SLOT_1);
    }

    return DR_EMIT_DEFAULT;
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    int i;
    dr_mutex_lock(mod_lock);
    for (i = 0; i < num_mods; i++) {
        /* check if it is the same as any unloaded module */
        if (!mod_array[i].loaded && module_data_same(mod_array[i].info, info)) {
            mod_array[i].loaded = true;
            break;
        }
    }
    if (i == num_mods) {
        /* new module */
        mod_array[i].base = info->start;
        mod_array[i].end = info->end;
        mod_array[i].loaded = true;
        mod_array[i].info = dr_copy_module_data(info);
        num_mods++;
    }
    DR_ASSERT(num_mods < UNKNOW_MODULE_IDX);
    dr_mutex_unlock(mod_lock);
}

static void
event_module_unload(void *drcontext, const module_data_t *info)
{
    int i;
    dr_mutex_lock(mod_lock);
    for (i = 0; i < num_mods; i++) {
        if (mod_array[i].loaded && module_data_same(mod_array[i].info, info)) {
            /* Some module might be repeatedly loaded and unloaded, so instead
             * of clearing out the array entry, we keep the data for possible
             * reuse.
             */
            mod_array[i].loaded = false;
            break;
        }
    }
    DR_ASSERT(i < num_mods);
    dr_mutex_unlock(mod_lock);
}
