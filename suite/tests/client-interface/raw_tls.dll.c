/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
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

/* Tests that raw TLS slots are initialised to zero. */

#include "dr_api.h"
#include "drmgr.h"

static bool thread_init_called = false;
static bool insert_called = false;
static reg_id_t tls_raw_reg;
static uint tls_raw_base;

static void **
get_tls_addr(int slot_idx)
{
    byte *base = dr_get_dr_segment_base(tls_raw_reg);
    byte *addr = (byte *)(base + tls_raw_base + slot_idx * sizeof(void *));
    return *((void **)addr);
}

static void
check(void)
{
    if (get_tls_addr(0) != NULL || get_tls_addr(1) != NULL || get_tls_addr(2) != NULL ||
        get_tls_addr(3) != NULL)
        dr_fprintf(STDERR, "raw TLS should be NULL\n");
}

static dr_emit_flags_t
insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace,
       bool translating, void *user_data)
{
    insert_called = true;
    check();

    if (drmgr_is_first_instr(drcontext, instr))
        dr_insert_clean_call(drcontext, bb, instr, check, false, 0);

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    thread_init_called = true;
    check();
}

static void
event_exit()
{
    if (!drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_bb_insertion_event(insert))
        dr_fprintf(STDERR, "error\n");

    if (!insert_called || !thread_init_called)
        dr_fprintf(STDERR, "not called\n");

    dr_raw_tls_cfree(tls_raw_base, 4);

    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmgr_init();

    dr_register_exit_event(event_exit);

    dr_raw_tls_calloc(&(tls_raw_reg), &(tls_raw_base), 4, 0);

    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_bb_instrumentation_event(NULL, insert, NULL))
        dr_fprintf(STDERR, "error\n");
}
