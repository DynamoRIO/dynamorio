/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
        dr_abort();                      \
    }                                    \
} while (0);

#define MAGIC_VAL 0xabcd

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb,
                   bool for_trace, bool translating, OUT void **user_data)
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
                *user_data = (void *) val1;
                instrlist_meta_postinsert(bb, inst, INSTR_CREATE_label(drcontext));
            } else
                prev_was_mov_const = true;
        } else
            prev_was_mov_const = false;
    }
    return DR_EMIT_DEFAULT;
}

static void
check_const(ptr_int_t reg, ptr_int_t val)
{
    CHECK(reg == val, "register value not preserved");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t reg, random = IF_X86_ELSE(DR_REG_XDI, DR_REG_R5);
    drreg_status_t res;
    drvector_t allowed;
    ptr_int_t subtest = (ptr_int_t) user_data;

    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, TEST_REG, true);

    if (subtest == 0) {
        uint flags;
        bool dead;
        /* Local tests */
        res = drreg_reserve_register(drcontext, bb, inst, NULL, &reg);
        CHECK(res == DRREG_SUCCESS, "default reserve should always work");
        dr_log(drcontext, LOG_ALL, 3, "drreg at "PFX" scratch=%s\n",
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
        CHECK(res == DRREG_SUCCESS ||
              (res == DRREG_ERROR_NO_APP_VALUE && reg == random),
              "get random reg app value should always work");
        /* query tests */
        res = drreg_aflags_liveness(drcontext, inst, &flags);
        CHECK(res == DRREG_SUCCESS, "query of aflags should work");
        res = drreg_are_aflags_dead(drcontext, inst, &dead);
        CHECK(res == DRREG_SUCCESS, "query of aflags should work");
        CHECK((dead && !TESTANY(EFLAGS_READ_ARITH, flags)) ||
              (!dead && TESTANY(EFLAGS_READ_ARITH, flags)), "liveness inconsistency");
        res = drreg_unreserve_register(drcontext, bb, inst, reg);
        CHECK(res == DRREG_SUCCESS, "default unreserve should always work");

        res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
        CHECK(res == DRREG_SUCCESS && reg == TEST_REG, "only 1 choice");
        res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
        CHECK(res == DRREG_ERROR_REG_CONFLICT, "still reserved");
        {
            opnd_t opnd = opnd_create_null();
            res = drreg_reservation_info(drcontext, reg, &opnd, NULL, NULL);
            CHECK(res == DRREG_SUCCESS && opnd_is_memory_reference(opnd),
                  "slot info should succeed");
        }
        res = drreg_unreserve_register(drcontext, bb, inst, reg);
        CHECK(res == DRREG_SUCCESS, "unreserve should work");
    } else if (subtest == DRREG_TEST_1_C ||
               subtest == DRREG_TEST_2_C ||
               subtest == DRREG_TEST_3_C) {
        /* Cross-app-instr tests */
        dr_log(drcontext, LOG_ALL, 1, "drreg test #1/2/3\n");
        if (instr_is_label(inst)) {
            res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
            CHECK(res == DRREG_SUCCESS, "reserve of test reg should work");
            instrlist_meta_preinsert(bb, inst, XINST_CREATE_load_int
                                     (drcontext, opnd_create_reg(reg),
                                      OPND_CREATE_INT32(MAGIC_VAL)));
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            dr_insert_clean_call(drcontext, bb, inst, check_const, false, 2,
                                 opnd_create_reg(TEST_REG),
                                 OPND_CREATE_INT32(MAGIC_VAL));
            res = drreg_unreserve_register(drcontext, bb, inst, TEST_REG);
            CHECK(res == DRREG_SUCCESS, "unreserve should work");
        }
    } else if (subtest == DRREG_TEST_4_C ||
               subtest == DRREG_TEST_5_C) {
        /* Cross-app-instr aflags test */
        dr_log(drcontext, LOG_ALL, 1, "drreg test #4\n");
        if (instr_is_label(inst)) {
            res = drreg_reserve_aflags(drcontext, bb, inst);
            CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
        } else if (instr_is_nop(inst)
                   IF_ARM(|| instr_get_opcode(inst) == OP_mov &&
                          /* assembler uses "mov r0,r0" for our nop */
                          opnd_same(instr_get_dst(inst, 0), instr_get_src(inst, 0)))) {
            /* Modify aflags to test preserving for app */
            instrlist_meta_preinsert(bb, inst, XINST_CREATE_cmp
                                     (drcontext, opnd_create_reg(DR_REG_START_32),
                                      OPND_CREATE_INT32(0)));
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            res = drreg_unreserve_aflags(drcontext, bb, inst);
            CHECK(res == DRREG_SUCCESS, "unreserve of aflags should work");
        }
    }

    drvector_delete(&allowed);

    /* XXX i#511: add more tests */

    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
event_instru2instru(void *drcontext, void *tag, instrlist_t *bb,
                    bool for_trace, bool translating)
{
    /* Test using outside of insert event */
    uint flags;
    bool dead;
    instr_t *inst = instrlist_first(bb);
    drreg_status_t res;
    drvector_t allowed;
    reg_id_t reg;

    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, TEST_REG, true);

    res = drreg_reserve_register(drcontext, bb, inst, NULL, &reg);
    CHECK(res == DRREG_SUCCESS, "default reserve should always work");
    res = drreg_unreserve_register(drcontext, bb, inst, reg);
    CHECK(res == DRREG_SUCCESS, "default unreserve should always work");

    res = drreg_reserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "reserve of aflags should work");
    res = drreg_unreserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "unreserve of aflags should work");
    res = drreg_aflags_liveness(drcontext, inst, &flags);
    CHECK(res == DRREG_SUCCESS, "query of aflags should work");
    res = drreg_are_aflags_dead(drcontext, inst, &dead);
    CHECK(res == DRREG_SUCCESS, "query of aflags should work");
    CHECK((dead && !TESTANY(EFLAGS_READ_ARITH, flags)) ||
          (!dead && TESTANY(EFLAGS_READ_ARITH, flags)), "aflags liveness inconsistency");

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
    drreg_options_t ops = {sizeof(ops), 2 /*max slots needed*/, false};
    if (!drmgr_init() ||
        drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "init failed");

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_app_analysis,
                                                 event_app_instruction, NULL) ||
        !drmgr_register_bb_instru2instru_event(event_instru2instru, NULL))
        CHECK(false, "init failed");
}
