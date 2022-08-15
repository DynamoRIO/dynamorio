/* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Illustrates use of the drstatecmp extension by a sample client.
 * This sample client introduces an instrumentation bug that is caught by
 * drstatecmp. This sample client also shows how to specify a user-defined
 * callback to be invoked on state comparison mismatches.
 */

#include "dr_api.h"
#include "drreg.h"
#include "drstatecmp.h"
#include <string.h>
#include <limits.h>

#define MINSERT instrlist_meta_preinsert

static int error_detected = 0;
static int global_count = 0;

static void
event_exit(void)
{
    drreg_exit();
    drstatecmp_exit();
    DR_ASSERT(error_detected == 1);
}

/* Invoked by drstatecmp when a state comparison fails. */
static void
error_callback(const char *msg, void *tag)
{
    error_detected = 1;
    DR_ASSERT_MSG(!strcmp(msg, "xflags"), msg);
}

static dr_emit_flags_t
event_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating, OUT void **user_data)
{

    bool *side_effect_free = (bool *)dr_thread_alloc(drcontext, sizeof(bool));
    *side_effect_free = drstatecmp_bb_checks_enabled(bb);
    *user_data = (void *)side_effect_free;
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_insert_instru(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                    bool for_trace, bool translating, void *user_data)
{
    if (!drmgr_is_last_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    bool side_effect_free = *(bool *)user_data;
    dr_thread_free(drcontext, user_data, sizeof(bool));
    /* Avoid clobbering of basic blocks with side-effects since such blocks are
     * not currently covered by drstatecmp.
     */
    if (!side_effect_free)
        return DR_EMIT_DEFAULT;

#ifdef X86
    // Instrumentation clobbering the arithmetic flags.
    MINSERT(bb, inst,
            INSTR_CREATE_add(drcontext, OPND_CREATE_ABSMEM(&global_count, OPSZ_4),
                             OPND_CREATE_INT_32OR8(1)));
#elif defined(AARCHXX)
    reg_id_t reg1, reg2;
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg1) != DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, bb, inst, NULL, &reg2) != DRREG_SUCCESS)
        return DR_EMIT_DEFAULT;

    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)&global_count,
                                     opnd_create_reg(reg1), bb, inst, NULL, NULL);
    MINSERT(
        bb, inst,
        XINST_CREATE_load(drcontext, opnd_create_reg(reg2), OPND_CREATE_MEMPTR(reg1, 0)));
    // Instrumentation clobbering the arithmetic flags.
    MINSERT(bb, inst,
            XINST_CREATE_add_s(drcontext, opnd_create_reg(reg2), OPND_CREATE_INT(1)));
    MINSERT(bb, inst,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg1, 0),
                               opnd_create_reg(reg2)));

    if (drreg_unreserve_register(drcontext, bb, inst, reg1) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, inst, reg2) != DRREG_SUCCESS)
        return DR_EMIT_DEFAULT;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    DR_ASSERT_MSG(false, "Not implemented on RISC-V");
    /* Marking as unused to silence -Wunused-variable. */
    (void)global_count;
#endif
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t drreg_ops = { sizeof(drreg_ops), 1 /*max slots needed: aflags*/,
                                  false };
    dr_set_client_name("DynamoRIO Sample Client 'statecmp'",
                       "http://dynamorio.org/issues");
    /* Make it easy to tell, by looking at log file, which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'statecmp' initializing\n");
    /* To enable state comparison checks by the drstatecmp library, a client simply needs
     * to initially invoke drstatecmp_init() and drstatecmp_exit() on exit.
     * The invocation of drstatecmp_init() registers callbacks that insert
     * machine state comparison checks in the code. A user-provided callback is
     * invoked (or assertions triggered if no callback specified) when there is any state
     * mismatch, indicating an instrumentation-induced clobbering. Invoking
     * drstatecmp_exit() unregisters the drstatecmp's callbacks and frees up the allocated
     * thread-local storage.
     */
    drstatecmp_options_t drstatecmp_ops = { error_callback };
    if (drstatecmp_init(&drstatecmp_ops) != DRSTATECMP_SUCCESS ||
        drreg_init(&drreg_ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    if (!drmgr_register_bb_instrumentation_event(event_analysis, event_insert_instru,
                                                 NULL))
        DR_ASSERT(false);

    dr_register_exit_event(event_exit);
}
