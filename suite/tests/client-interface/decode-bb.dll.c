/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

static void *last_trace_mutex;
static void *last_trace_tag;
static instrlist_t *last_trace_ilist;
static void *last_trace_drcontext;

static void
verify_identical(instrlist_t *il1, instrlist_t *il2, app_pc label)
{
    instr_t *i = instrlist_first(il1);
    instr_t *j = instrlist_first(il2);

    while (i != NULL && j != NULL) {
        app_pc i_addr = instr_get_app_pc(i);
        app_pc j_addr = instr_get_app_pc(j);

        if (i_addr != j_addr || !instr_same(i, j)) {
            break;
        }

        i = instr_get_next(i);
        j = instr_get_next(j);
    }

    if (i != NULL || j != NULL) {
        dr_fprintf(STDERR, "ERROR: mismatch in block at " PFX "\n", label);
    }
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (!translating) {
        app_pc pc = dr_fragment_app_pc(tag);

        instrlist_t *copy = decode_as_bb(drcontext, pc);
        verify_identical(bb, copy, pc);
        instrlist_clear_and_destroy(drcontext, copy);

        dr_mutex_lock(last_trace_mutex);
        if (last_trace_tag != NULL) {
            pc = dr_fragment_app_pc(last_trace_tag);
            if (pc != NULL) {
                copy = decode_trace(drcontext, pc);
                if (copy != NULL) {
                    verify_identical(last_trace_ilist, copy, pc);
                    instrlist_clear_and_destroy(drcontext, copy);
                }
            }
            instrlist_clear_and_destroy(last_trace_drcontext, last_trace_ilist);
            last_trace_tag = NULL;
        }
        dr_mutex_unlock(last_trace_mutex);
    }

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
trace_event(void *drcontext, void *tag, instrlist_t *trace, bool translating)
{
    instr_t *i;
    for (i = instrlist_first(trace); i != NULL; i = instr_get_next(i)) {
        if (instr_get_app_pc(i) == NULL) {
            dr_fprintf(STDERR,
                       "ERROR: app pc not available for all trace instrs " PFX "\n",
                       dr_fragment_app_pc(tag));
        }
    }

    if (!translating) {
        /* we can't call decode_trace() until after it's emitted, so remember the tag
         * and call it in the next bb hook
         */
        dr_mutex_lock(last_trace_mutex);
        if (last_trace_tag != NULL)
            instrlist_clear_and_destroy(last_trace_drcontext, last_trace_ilist);
        last_trace_tag = tag;
        last_trace_ilist = instrlist_clone(drcontext, trace);
        last_trace_drcontext = drcontext;
        dr_mutex_unlock(last_trace_mutex);
    }

    return DR_EMIT_DEFAULT;
}

static void
dr_thread_exit(void *drcontext)
{
    dr_mutex_lock(last_trace_mutex);
    if (last_trace_tag != NULL && last_trace_drcontext == drcontext) {
        instrlist_clear_and_destroy(last_trace_drcontext, last_trace_ilist);
        last_trace_tag = NULL;
    }
    dr_mutex_unlock(last_trace_mutex);
}

static void
dr_exit(void)
{
    dr_mutex_destroy(last_trace_mutex);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    last_trace_mutex = dr_mutex_create();
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_bb_event(bb_event);
    dr_register_trace_event(trace_event);
    dr_register_thread_exit_event(dr_thread_exit);
    dr_register_exit_event(dr_exit);
}
