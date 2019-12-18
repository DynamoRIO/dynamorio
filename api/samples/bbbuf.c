/* ******************************************************************************
 * Copyright (c) 2013-2015 Google, Inc.  All rights reserved.
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
 * bbbuf.c
 *
 * This sample demonstrates how to use a TLS field for per-thread profiling.
 * For each thread, we create a 64KB buffer with 64KB-aligned start address,
 * and store that into a TLS slot.
 * At the beginning of each basic block, we insert code to
 * - load the pointer from the TLS slot,
 * - store the starting pc of the basic block into the buffer,
 * - update the pointer by incrementing just the low 16 bits of the pointer
 *   so we will fill the buffer in a cyclical way.
 * This is all done via the fast circular buffer code provided by the drx_buf
 * extension.
 * This sample can be used for hot path profiling or debugging with execution
 * history.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"
#include <string.h>

/* drx_buf makes our work easy as it already has first-class support for the
 * fast circular buffer.
 */
static drx_buf_t *buf;

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    app_pc pc = dr_fragment_app_pc(tag);
    reg_id_t reg;
    /* We need a 2nd scratch reg for several operations on AArch32 and AArch64 only. */
    reg_id_t reg2 = DR_REG_NULL;

    /* By default drmgr enables auto-predication, which predicates all instructions with
     * the predicate of the current instruction on ARM.
     * We disable it here because we want to unconditionally execute the following
     * instrumentation.
     */
    drmgr_disable_auto_predication(drcontext, bb);

    /* We do all our work at the start of the block prior to the first instr */
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    /* We need a scratch register */
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg) != DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return DR_EMIT_DEFAULT;
    }

#ifdef AARCHXX
    /* We need a second register here, because the drx_buf routines need a scratch reg
     * for AArch32 and AArch64.
     */
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg2) != DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return DR_EMIT_DEFAULT;
    }
#endif

    /* load buffer pointer from TLS field */
    drx_buf_insert_load_buf_ptr(drcontext, buf, bb, inst, reg);

    /* store bb's start pc into the buffer */
    drx_buf_insert_buf_store(drcontext, buf, bb, inst, reg, reg2, OPND_CREATE_INTPTR(pc),
                             OPSZ_PTR, 0);

    /* Internally this will update the TLS buffer pointer by incrementing just the bottom
     * 16 bits of the pointer.
     */
    drx_buf_insert_update_buf_ptr(drcontext, buf, bb, inst, reg, reg2, sizeof(app_pc));

    if (drreg_unreserve_register(drcontext, bb, inst, reg) != DRREG_SUCCESS)
        DR_ASSERT(false);

#ifdef AARCHXX
    if (drreg_unreserve_register(drcontext, bb, inst, reg2) != DRREG_SUCCESS)
        DR_ASSERT(false);
#endif

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    byte *data;

    data = drx_buf_get_buffer_ptr(drcontext, buf);
    memset(data, 0, DRX_BUF_FAST_CIRCULAR_BUFSZ);
}

static void
event_exit(void)
{
    if (!drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);

    drx_buf_free(buf);
    drmgr_exit();
    drx_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    dr_set_client_name("DynamoRIO Sample Client 'bbbuf'", "http://dynamorio.org/issues");
    if (!drmgr_init() || !drx_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    buf = drx_buf_create_circular_buffer(DRX_BUF_FAST_CIRCULAR_BUFSZ);
    DR_ASSERT(buf);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
}
