/* **********************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * prefetch.c
 *
 * Performs a code-stream compatibility modification.  Programs optimized for AMD
 * processors are likely to use the prefetch and prefetchw instructions (originally
 * added as part of the 3D!now extensions).  However, these opcodes cause illegal
 * instruction faults on most Intel processors.  Here we detect if we are on an Intel
 * processor and translate (in this case remove) prefetch instructions so the app can
 * run.
 */

#include "dr_api.h"
#include "drmgr.h"

static void
event_exit(void);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating);

static void *count_mutex; /* for multithread support */
static int prefetches_removed = 0, prefetchws_removed = 0;

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'prefetch'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init())
        DR_ASSERT(false);
    dr_register_exit_event(event_exit);
    /* Only need to remove prefetches for Intel processors. */
    if (proc_get_vendor() == VENDOR_INTEL) {
        /* We may remove app instructions, so we should register app2app event
         * instead of instrumentation event.
         */
        if (!drmgr_register_bb_app2app_event(event_bb_app2app, NULL))
            DR_ASSERT(false);
    }

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'prefetch' initializing\n");

    /* must create the counting mutex */
    count_mutex = dr_mutex_create();
}

static void
event_exit(void)
{
    dr_log(NULL, DR_LOG_ALL, 1, "Removed %d prefetches and %d prefetchws.\n",
           prefetches_removed, prefetchws_removed);
    dr_mutex_destroy(count_mutex);
    drmgr_exit();
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    instr_t *instr, *next_instr;
    int opcode;

    /* look for and remove prefetch[w] instructions */
    for (instr = instrlist_first_app(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next_app(instr);
        opcode = instr_get_opcode(instr);
        if (opcode == OP_prefetch || opcode == OP_prefetchw) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
            if (!translating) {
                /* count_mutex protects the counter values */
                dr_mutex_lock(count_mutex);
                if (opcode == OP_prefetch)
                    prefetches_removed++;
                else
                    prefetchws_removed++;
                dr_mutex_unlock(count_mutex);
            }
        }
    }
    return DR_EMIT_DEFAULT;
}
