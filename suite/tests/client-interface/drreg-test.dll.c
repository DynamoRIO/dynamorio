/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

/* Tests the drreg extension */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "client_tools.h"
#include "drreg-test-shared.h"
#include <string.h> /* memset */

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

#define MAGIC_VAL 0xabcd

static dr_emit_flags_t
event_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating, OUT void **user_data)
{
    instr_t *inst;
    bool prev_was_mov_const = false;
    ptr_int_t val1, val2;
    *user_data = NULL;
    /* Look for duplicate mov immediates telling us which subtest we're in */
    for (inst = instrlist_first_app(bb); inst != NULL; inst = instr_get_next_app(inst)) {
        if (instr_is_mov_constant(inst, prev_was_mov_const ? &val2 : &val1)) {
            if (prev_was_mov_const && val1 == val2 &&
                val1 != 0 && /* rule out xor w/ self */
                opnd_is_reg(instr_get_dst(inst, 0)) &&
                opnd_get_reg(instr_get_dst(inst, 0)) == TEST_REG) {
                *user_data = (void *)val1;
                instrlist_meta_postinsert(bb, inst, INSTR_CREATE_label(drcontext));
            } else
                prev_was_mov_const = true;
        } else
            prev_was_mov_const = false;
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, OUT void *user_data)
{
    return DR_EMIT_DEFAULT;
}

static void
check_const_eq(ptr_int_t reg, ptr_int_t val)
{
    CHECK(reg == val, "register value not preserved");
}

static void
check_const_ne(ptr_int_t reg, ptr_int_t val)
{
    CHECK(reg != val, "register value not preserved");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t reg, random = IF_X86_ELSE(DR_REG_XDI, DR_REG_R5);
    drreg_status_t res;
    drvector_t allowed;
    ptr_int_t subtest = (ptr_int_t)user_data;
    drreg_reserve_info_t info = {
        sizeof(info),
    };

    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, TEST_REG, true);

    if (subtest == 0) {
        uint flags;
        bool dead;
        /* Local tests */
        res = drreg_reserve_register(drcontext, bb, inst, NULL, &reg);
        CHECK(res == DRREG_SUCCESS, "default reserve should always work");
        dr_log(drcontext, DR_LOG_ALL, 3, "drreg at " PFX " scratch=%s\n",
               instr_get_app_pc(inst), get_register_name(reg));
        /* test restore app value back to reg */
        res = drreg_get_app_value(drcontext, bb, inst, reg, reg);
        CHECK(res == DRREG_SUCCESS || res == DRREG_ERROR_NO_APP_VALUE,
              "restore app value could only fail on dead reg");
        /* test get stolen reg to reg */
        if (dr_get_stolen_reg() != REG_NULL) {
            res = drreg_get_app_value(drcontext, bb, inst, dr_get_stolen_reg(), reg);
            CHECK(res == DRREG_SUCCESS, "get stolen reg app value should always work");
        }
        /* test get random reg to reg */
        res = drreg_get_app_value(drcontext, bb, inst, random, reg);
        CHECK(res == DRREG_SUCCESS || res == DRREG_ERROR_NO_APP_VALUE,
              "get random reg app value should only fail on dead reg");
        if (res == DRREG_ERROR_NO_APP_VALUE) {
            bool dead;
            res = drreg_is_register_dead(drcontext, random, inst, &dead);
            CHECK(res == DRREG_SUCCESS && dead, "get app val should only fail when dead");
        }
        /* test restore of opnd app values */
        res = drreg_restore_app_values(drcontext, bb, inst, opnd_create_reg(reg), NULL);
        CHECK(res == DRREG_SUCCESS || res == DRREG_ERROR_NO_APP_VALUE,
              "restore app values could only fail on dead reg");
        /* test restore of opnd app values with stolen reg */
        if (dr_get_stolen_reg() != REG_NULL) {
            reg_id_t swap = DR_REG_NULL;
            res = drreg_restore_app_values(drcontext, bb, inst,
                                           opnd_create_reg(dr_get_stolen_reg()), &swap);
            CHECK(res == DRREG_SUCCESS || res == DRREG_ERROR_NO_APP_VALUE,
                  "restore app values could only fail on dead reg");
            if (swap != DR_REG_NULL)
                res = drreg_unreserve_register(drcontext, bb, inst, swap);
            CHECK(res == DRREG_SUCCESS, "unreserve of swap reg should not fail");
        }
        /* query tests */
        res = drreg_aflags_liveness(drcontext, inst, &flags);
        CHECK(res == DRREG_SUCCESS, "query of aflags should work");
        res = drreg_are_aflags_dead(drcontext, inst, &dead);
        CHECK(res == DRREG_SUCCESS, "query of aflags should work");
        CHECK((dead && !TESTANY(EFLAGS_READ_ARITH, flags)) ||
                  (!dead && TESTANY(EFLAGS_READ_ARITH, flags)),
              "liveness inconsistency");
        res = drreg_unreserve_register(drcontext, bb, inst, reg);
        CHECK(res == DRREG_SUCCESS, "default unreserve should always work");

        res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
        CHECK(res == DRREG_SUCCESS && reg == TEST_REG, "only 1 choice");
        res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
        CHECK(res == DRREG_ERROR_REG_CONFLICT, "still reserved");
        opnd_t opnd = opnd_create_null();
        res = drreg_reservation_info(drcontext, reg, &opnd, NULL, NULL);
        CHECK(res == DRREG_SUCCESS && opnd_is_memory_reference(opnd),
              "slot info should succeed");
        res = drreg_reservation_info_ex(drcontext, reg, &info);
        CHECK(res == DRREG_SUCCESS && opnd_is_memory_reference(info.opnd) &&
                  info.reserved,
              "slot info_ex unexpected result");
        /* test stateless restore when live */
        drreg_statelessly_restore_app_value(drcontext, bb, reg, inst, inst, NULL, NULL);

        res = drreg_unreserve_register(drcontext, bb, inst, reg);
        CHECK(res == DRREG_SUCCESS, "unreserve should work");

        /* test stateless restore when lazily unrestored */
        drreg_statelessly_restore_app_value(drcontext, bb, reg, inst, inst, NULL, NULL);

        /* test spill-restore query */
        bool is_dead;
        res = drreg_is_register_dead(drcontext, reg, inst, &is_dead);
        CHECK(res == DRREG_SUCCESS, "query should work");
        instr_t *prev;
        bool found_restore = false;
        for (prev = instr_get_prev(inst); prev != NULL; prev = instr_get_prev(prev)) {
            bool spill, restore;
            reg_id_t which_reg;
            res = drreg_is_instr_spill_or_restore(drcontext, prev, &spill, &restore,
                                                  &which_reg);
            CHECK(res == DRREG_SUCCESS, "spill query should work");
            CHECK(!(spill && restore), "can't be both a spill and a restore");
            if (restore) {
                found_restore = true;
                CHECK(which_reg == reg || is_dead, "expected restore of given reg");
                break;
            }
        }
        CHECK(found_restore || is_dead, "failed to find restore");

        /* test aflags */
        res = drreg_reservation_info_ex(drcontext, DR_REG_NULL, &info);
        CHECK(res == DRREG_SUCCESS && !info.reserved &&
                  ((info.holds_app_value && !info.app_value_retained &&
                    opnd_is_null(info.opnd)) ||
                   (!info.holds_app_value && info.app_value_retained &&
                    !opnd_is_null(info.opnd))),
              "aflags un-reserve query failed");
        res = drreg_reserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
        res = drreg_reservation_info_ex(drcontext, DR_REG_NULL, &info);
        CHECK(res == DRREG_SUCCESS && info.reserved &&
                  ((info.app_value_retained && !opnd_is_null(info.opnd)) ||
                   (info.holds_app_value && opnd_is_null(info.opnd))),
              "aflags reserve query failed");
        res = drreg_restore_app_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "restore of app aflags should work");
        res = drreg_unreserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "unreserve of aflags");
#ifdef X86
        /* test aflags conflicts with xax: failing to reserve xax due to lazy
         * aflags still in xax from above; failing to reserve aflags if xax is
         * taken; failing to get app aflags if xax is taken.
         */
        drvector_t only_xax;
        drreg_init_and_fill_vector(&only_xax, false);
        drreg_set_vector_entry(&only_xax, DR_REG_XAX, true);
        res = drreg_reserve_register(drcontext, bb, inst, &only_xax, &reg);
        CHECK(res == DRREG_SUCCESS, "reserve of xax should work");
        res = drreg_reserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "reserve of aflags w/ xax taken should work");
        res = drreg_restore_app_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "restore of app aflags should work");
        res = drreg_unreserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "unreserve of aflags");
        res = drreg_unreserve_register(drcontext, bb, inst, reg);
        CHECK(res == DRREG_SUCCESS, "unreserve of xax should work");
        drvector_delete(&only_xax);
#endif
    } else if (subtest == DRREG_TEST_1_C || subtest == DRREG_TEST_2_C ||
               subtest == DRREG_TEST_3_C) {
        /* Cross-app-instr tests */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #1/2/3\n");
        if (instr_is_label(inst)) {
            res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
            CHECK(res == DRREG_SUCCESS, "reserve of test reg should work");
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_load_int(drcontext,
                                                           opnd_create_reg(reg),
                                                           OPND_CREATE_INT32(MAGIC_VAL)));
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            dr_insert_clean_call(drcontext, bb, inst, check_const_eq, false, 2,
                                 opnd_create_reg(TEST_REG), OPND_CREATE_INT32(MAGIC_VAL));
            res = drreg_unreserve_register(drcontext, bb, inst, TEST_REG);
            CHECK(res == DRREG_SUCCESS, "unreserve should work");
        }
    } else if (subtest == DRREG_TEST_4_C || subtest == DRREG_TEST_5_C) {
        /* Cross-app-instr aflags test */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #4/5\n");
        if (instr_is_label(inst)) {
            res = drreg_reserve_aflags(drcontext, bb, inst);
            CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
        } else if (instr_is_nop(inst) IF_ARM(
                       ||
                       instr_get_opcode(inst) == OP_mov &&
                           /* assembler uses "mov r0,r0" for our nop */
                           opnd_same(instr_get_dst(inst, 0), instr_get_src(inst, 0)))) {
            /* Modify aflags to test preserving for app */
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_cmp(drcontext,
                                                      opnd_create_reg(DR_REG_START_32),
                                                      OPND_CREATE_INT32(0)));
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            res = drreg_unreserve_aflags(drcontext, bb, inst);
            CHECK(res == DRREG_SUCCESS, "unreserve of aflags should work");
        }
    } else if (subtest == DRREG_TEST_6_C) {
        /* Save the 3rd DR slot at the label, restore register after
         * the xl8 point in this test.
         */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #6\n");
        if (instr_is_label(inst)) {
            dr_save_reg(drcontext, bb, inst, TEST_REG, 2);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            dr_restore_reg(drcontext, bb, inst, TEST_REG, 2);
        }
    } else if (subtest == DRREG_TEST_8_C) {
        /* Nothing to do here */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #8\n");
    } else if (subtest == DRREG_TEST_10_C) {
        /* Nothing to do here */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #10\n");
    } else if (subtest == DRREG_TEST_11_C) {
#ifdef X86
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #11\n");
        res = drreg_reserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
        res = drreg_unreserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "unreserve of aflags");
#endif
    } else if (subtest == DRREG_TEST_12_C) {
#ifdef X86
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #12\n");
        res = drreg_reserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
        if (instr_get_opcode(inst) == OP_cmp) {
            drreg_statelessly_restore_app_value(drcontext, bb, DR_REG_NULL, inst, inst,
                                                NULL, NULL);
        }
        res = drreg_unreserve_aflags(drcontext, bb, inst);
        CHECK(res == DRREG_SUCCESS, "unreserve of aflags");
#endif
    }

    drvector_delete(&allowed);

    /* XXX i#511: add more tests */

    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
event_instru2instru(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                    bool translating, void *user_data)
{
    /* Test using outside of insert event */
    uint flags;
    bool dead;
    instr_t *inst = instrlist_first(bb);
    drreg_status_t res;
    drvector_t allowed;
    reg_id_t reg0, reg1;
    ptr_int_t subtest = (ptr_int_t)user_data;

    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, TEST_REG, true);

    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg0);
    CHECK(res == DRREG_SUCCESS && reg0 == TEST_REG, "only 1 choice");
    res = drreg_unreserve_register(drcontext, bb, inst, reg0);
    CHECK(res == DRREG_SUCCESS, "default unreserve should always work");

    /* XXX: construct better tests with and without a dead reg available */
    res = drreg_reserve_dead_register(drcontext, bb, inst, &allowed, &reg0);
    if (res == DRREG_SUCCESS) {
        res = drreg_unreserve_register(drcontext, bb, inst, reg0);
        CHECK(res == DRREG_SUCCESS, "default unreserve should always work");
    }

    res = drreg_reserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
    res = drreg_restore_app_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "restore of app aflags should work");
    res = drreg_unreserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "unreserve of aflags should work");

    res = drreg_aflags_liveness(drcontext, inst, &flags);
    CHECK(res == DRREG_SUCCESS, "query of aflags should work");
    res = drreg_are_aflags_dead(drcontext, inst, &dead);
    CHECK(res == DRREG_SUCCESS, "query of aflags should work");
    CHECK((dead && !TESTANY(EFLAGS_READ_ARITH, flags)) ||
              (!dead && TESTANY(EFLAGS_READ_ARITH, flags)),
          "aflags liveness inconsistency");
    res = drreg_is_register_dead(drcontext, DR_REG_START_GPR, inst, &dead);
    CHECK(res == DRREG_SUCCESS, "query of liveness should work");

    if (subtest == DRREG_TEST_2_C) {
        /* We are running one more subtest on top of DRREG_TEST_2. Any subtest where
         * TEST_REG2 is not dead at the test's entry will do. We are reserving TEST_REG2
         * and store MAGIC_VAL to it, followed by another reservation and a restore, which
         * exposes a possible bug in register liveness forward analysis (xref i#3821).
         */
        drreg_set_vector_entry(&allowed, TEST_REG, false);
        drreg_set_vector_entry(&allowed, TEST_REG2, true);
        res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg0);
        CHECK(res == DRREG_SUCCESS && reg0 == TEST_REG2, "only 1 choice");
        res = drreg_reserve_register(drcontext, bb, inst, NULL, &reg1);
        instrlist_meta_preinsert(bb, inst,
                                 XINST_CREATE_load_int(drcontext,
                                                       opnd_create_reg(TEST_REG2),
                                                       OPND_CREATE_INT32(MAGIC_VAL)));
        res = drreg_unreserve_register(drcontext, bb, inst, reg1);
        CHECK(res == DRREG_SUCCESS, "default unreserve should always work");
        res = drreg_unreserve_register(drcontext, bb, inst, reg0);
        CHECK(res == DRREG_SUCCESS, "default unreserve should always work");
        dr_insert_clean_call(drcontext, bb, inst, check_const_ne, false, 2,
                             opnd_create_reg(TEST_REG2), OPND_CREATE_INT32(MAGIC_VAL));
    }

    drvector_delete(&allowed);

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "exit failed");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    /* We actually need 3 slots (flags + 2 scratch) but we want to test using
     * a DR slot.
     */
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "init failed");

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_ex_event(event_app2app, event_app_analysis,
                                                    event_app_instruction,
                                                    event_instru2instru, NULL))
        CHECK(false, "init failed");
    /* i#2910: test use during process init. */
    void *drcontext = dr_get_current_drcontext();
    instrlist_t *ilist = instrlist_create(drcontext);
    drreg_status_t res = drreg_reserve_aflags(drcontext, ilist, NULL);
    CHECK(res == DRREG_SUCCESS, "process init test failed");
    reg_id_t reg;
    res = drreg_reserve_register(drcontext, ilist, NULL, NULL, &reg);
    CHECK(res == DRREG_SUCCESS, "process init test failed");
    instrlist_clear_and_destroy(drcontext, ilist);
}
