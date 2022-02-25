/* **********************************************************
 * Copyright (c) 2020-2022 Google, Inc.  All rights reserved.
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

/* Tests drreg when the user performs end of basic block restoration. */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include <string.h> /* memcpy */

#ifdef X86
#    define TEST_REG DR_REG_XDI
#else
#    define TEST_REG DR_REG_R5
#endif

static int tls_idx = -1;

/* No locks performed on this flag. Single-thread is assumed. */
static bool performed_check = false;

static void
event_exit(void);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void *user_data);
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed */, false };
    drmgr_priority_t priority = { sizeof(priority), "my_priority", NULL, NULL, 0 };
    bool ok;
    drreg_status_t res;

    dr_set_client_name("DynamoRIO Sample Client 'drreg-end-restore'",
                       "http://dynamorio.org/issues");

    drmgr_init();
    res = drreg_init(&ops);
    CHECK(res == DRREG_SUCCESS, "drreg init failed");
    dr_register_exit_event(event_exit);

    ok = drmgr_register_bb_instrumentation_ex_event(event_bb_app2app, event_bb_analysis,
                                                    event_bb_insert, NULL, &priority);
    CHECK(ok, "drmgr register bb failed");

    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx != -1, "tls registration failed");
}

static void
event_exit(void)
{
    drmgr_unregister_tls_field(tls_idx);

    bool ok = drmgr_unregister_bb_instrumentation_ex_event(
        event_bb_app2app, event_bb_analysis, event_bb_insert, NULL);
    CHECK(ok, "drmgr unregister bb failed");
    CHECK(performed_check, "check was not performed");
    drreg_exit();
    drmgr_exit();
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, OUT void **user_data)
{
    drreg_status_t res =
        drreg_set_bb_properties(drcontext, DRREG_USER_RESTORES_AT_BB_END);
    CHECK(res == DRREG_SUCCESS, "failed to set property");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void *user_data)
{
    drreg_status_t res =
        drreg_set_bb_properties(drcontext, DRREG_USER_RESTORES_AT_BB_END);
    CHECK(res == DRREG_SUCCESS, "failed to set property");
    return DR_EMIT_DEFAULT;
}

static reg_t
get_val_from_ctx(void *drcontext, reg_id_t reg_id)
{
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    return reg_get_value(reg_id, &mcontext);
}

static void
set_reg_val()
{
    void *drcontext = dr_get_current_drcontext();
    void *val = (void *)get_val_from_ctx(drcontext, TEST_REG);
    drmgr_set_tls_field(drcontext, tls_idx, val);
}

static void
check_reg_val()
{
    void *drcontext = dr_get_current_drcontext();
    reg_t val = get_val_from_ctx(drcontext, TEST_REG);
    reg_t orig_val = (reg_t)drmgr_get_tls_field(drcontext, tls_idx);
    CHECK(val == orig_val, "restoration failed");
    performed_check = true;
}

static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                bool for_trace, bool translating, void *user_data)
{
    drreg_status_t res;
    reg_id_t reg;
    drvector_t allowed;
    bool is_dead;

    res = drreg_is_register_dead(drcontext, TEST_REG, instr, &is_dead);
    CHECK(res == DRREG_SUCCESS, "failed to check whether reg is dead");

    if (!is_dead) {
        res = drreg_init_and_fill_vector(&allowed, false);
        CHECK(res == DRREG_SUCCESS, "failed to init vector");

        res = drreg_set_vector_entry(&allowed, TEST_REG, true);
        CHECK(res == DRREG_SUCCESS, "failed to set entry in vector");

        res = drreg_restore_all(drcontext, bb, instr);
        CHECK(res == DRREG_SUCCESS, "failed to restore all");

        dr_insert_clean_call_ex(drcontext, bb, instr, set_reg_val,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);

        res = drreg_reserve_aflags(drcontext, bb, instr);
        CHECK(res == DRREG_SUCCESS, "failed to reserve flags");

        res = drreg_reserve_register(drcontext, bb, instr, &allowed, &reg);
        CHECK(res == DRREG_SUCCESS, "failed to reserve");
        CHECK(reg == TEST_REG, "reg reservation failed");

        instrlist_insert_mov_immed_ptrsz(drcontext, 0, opnd_create_reg(reg), bb, instr,
                                         NULL, NULL);

        res = drreg_unreserve_register(drcontext, bb, instr, reg);
        CHECK(res == DRREG_SUCCESS, "failed to unreserve reg");

        res = drreg_unreserve_aflags(drcontext, bb, instr);
        CHECK(res == DRREG_SUCCESS, "failed to unreserve flags");

        res = drreg_restore_all(drcontext, bb, instr);
        CHECK(res == DRREG_SUCCESS, "failed to restore all");

        dr_insert_clean_call_ex(drcontext, bb, instr, check_reg_val,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);

        drvector_delete(&allowed);
    }

    return DR_EMIT_DEFAULT;
}
