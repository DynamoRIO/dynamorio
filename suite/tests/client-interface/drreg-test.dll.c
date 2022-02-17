/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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
#include "drx.h"
#include "client_tools.h"
#include "drreg-test-shared.h"
#include <string.h> /* memset */

#define MAGIC_VAL 0xabcd

static ptr_uint_t note_base;
#define NOTE_VAL(enum_val) ((void *)(ptr_int_t)(note_base + (enum_val)))

/* Enum describing the different types of notes in drreg-test. */
enum { DRREG_TEST_LABEL_MARKER, DRREG_TEST_NOTE_COUNT };

uint tls_offs_app2app_spilled_aflags;
uint tls_offs_app2app_spilled_reg;
uint tls_offs_test_reg_1;
uint tls_offs_aflags;

static bool
is_drreg_test_label_marker(instr_t *inst)
{
    if (instr_is_label(inst) && instr_get_note(inst) == NOTE_VAL(DRREG_TEST_LABEL_MARKER))
        return true;
    return false;
}

static uint
spill_test_reg_to_slot(void *drcontext, instrlist_t *bb, instr_t *inst,
                       reg_id_t reg_to_reserve, drvector_t *allowed, bool overwrite)
{
    uint tls_offs, offs;
    reg_id_t reg;
    opnd_t opnd;
    bool is_dr_slot;
    CHECK(drreg_reserve_register(drcontext, bb, inst, allowed, &reg) == DRREG_SUCCESS,
          "unable to reserve register");
    CHECK(reg == reg_to_reserve, "only 1 option");
    CHECK(drreg_reservation_info(drcontext, reg, &opnd, &is_dr_slot, &offs) ==
              DRREG_SUCCESS,
          "unable to get reservation info");
    CHECK(offs != -1, "gpr should be spilled to some slot.");
    if (is_dr_slot) {
        tls_offs = opnd_get_disp(opnd);
    } else {
        tls_offs = offs;
    }
    if (overwrite) {
        /* Load with some value so that we need to restore it later. */
        instrlist_meta_preinsert(bb, inst,
                                 XINST_CREATE_load_int(drcontext, opnd_create_reg(reg),
                                                       OPND_CREATE_INT32(MAGIC_VAL)));
    }
    return tls_offs;
}

static void
ensure_aflags_in_slot(void *drcontext, instrlist_t *bb, instr_t *inst)
{
#ifdef X86
    /* Make sure that aflags are spilled to some slot, instead of being stored in xax.
     */
    CHECK(drreg_get_app_value(drcontext, bb, inst, DR_REG_XAX, DR_REG_XCX) ==
              DRREG_SUCCESS,
          "cannot get app value");
#endif
}

static void
write_aflags(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    instrlist_meta_preinsert(bb, inst,
                             XINST_CREATE_cmp(drcontext, opnd_create_reg(DR_REG_START_32),
                                              OPND_CREATE_INT32(0)));
}
static uint
spill_aflags_to_slot(void *drcontext, instrlist_t *bb, instr_t *inst, bool overwrite)
{
    uint tls_offs, offs;
    opnd_t opnd;
    bool is_dr_slot;
    CHECK(drreg_reserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
          "cannot reserve aflags");
    ensure_aflags_in_slot(drcontext, bb, inst);
    if (overwrite) {
        /* Load aflags with some value so that we need to restore it later. */
        write_aflags(drcontext, bb, inst);
    }
    CHECK(drreg_reservation_info(drcontext, DR_REG_NULL, &opnd, &is_dr_slot, &offs) ==
              DRREG_SUCCESS,
          "unable to get reservation info");
    CHECK(offs != -1, "aflags should be spilled to some slot.");
    if (is_dr_slot) {
        tls_offs = opnd_get_disp(opnd);
    } else {
        tls_offs = offs;
    }
    return tls_offs;
}

void
aflags_ensure_live(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    /* Make sure that aflags are not dead, otherwise drreg may not even
     * reserve a spill slot.
     */
#ifdef X86
    instrlist_meta_preinsert(bb, inst, INSTR_CREATE_lahf(drcontext));
#elif defined(ARM)
    instrlist_meta_preinsert(bb, inst,
                             INSTR_CREATE_mrs(drcontext, opnd_create_reg(TEST_REG2),
                                              opnd_create_reg(DR_REG_CPSR)));
#elif defined(AARCH64)
    /* XXX i#4930: This is not yet needed on AArch64 because aflags are always
     * live. If i#4930 is fixed, we'd need to read NZCV.
     */
#endif
}
static dr_emit_flags_t
event_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating, OUT void **user_data)
{
    instr_t *inst;
    bool prev_was_mov_const = false;
    ptr_int_t val, val1, val2;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, TEST_REG, true);

    *user_data = NULL;
    /* Look for duplicate mov immediates telling us which subtest we're in */
    for (inst = instrlist_first_app(bb); inst != NULL; inst = instr_get_next_app(inst)) {
        if (instr_is_mov_constant(inst, prev_was_mov_const ? &val2 : &val1)) {
            if (prev_was_mov_const && val1 == val2 &&
                val1 != 0 && /* rule out xor w/ self */
                opnd_is_reg(instr_get_dst(inst, 0)) &&
                opnd_get_reg(instr_get_dst(inst, 0)) == TEST_REG) {
                *user_data = (void *)val1;
                instr_t *label = INSTR_CREATE_label(drcontext);
                instr_set_note(label, NOTE_VAL(DRREG_TEST_LABEL_MARKER));
                instrlist_meta_postinsert(bb, inst, label);
            } else {
                /* This is needed in order to catch the pair of duplicate mov immediates
                 * if they occur later in a sequence of mov instrs.
                 */
                if (prev_was_mov_const)
                    val1 = val2;
                else
                    prev_was_mov_const = true;
            }
        } else
            prev_was_mov_const = false;
    }
    ptr_int_t subtest = *((ptr_int_t *)user_data);
    if (subtest == DRREG_TEST_13_C || subtest == DRREG_TEST_14_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_reg = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #13/#14: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_reg =
                    spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed, true);
            } else if (inst == instrlist_last(bb)) {
                /* Make sure that TEST_REG isn't dead after its app2app spill.
                 * If it is dead, its next spill will only reserve a slot, but not
                 * actually write to it. To test restore in the multi-phase nested
                 * spill case (test #13, #14), we need it to actually write.
                 */
                instrlist_meta_preinsert(bb, inst,
                                         XINST_CREATE_add(drcontext,
                                                          opnd_create_reg(TEST_REG),
                                                          OPND_CREATE_INT32(1)));
                CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                          DRREG_SUCCESS,
                      "cannot unreserve register");
            }
        }
    } else if (subtest == DRREG_TEST_17_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_reg = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #17: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_reg =
                    spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed, true);
            } else if (instr_is_mov_constant(inst, &val) &&
                       val == TEST_INSTRUMENTATION_MARKER_3) {
                /* Make sure that TEST_REG isn't dead after its app2app spill.
                 * If it is dead, its next spill will only reserve a slot, but not
                 * actually write to it. To test restore in the multi-phase
                 * overlapping spill case (test #17), we need it to actually write.
                 */
                instrlist_meta_preinsert(bb, inst,
                                         XINST_CREATE_add(drcontext,
                                                          opnd_create_reg(TEST_REG),
                                                          OPND_CREATE_INT32(1)));
                CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                          DRREG_SUCCESS,
                      "cannot unreserve register");
            }
        }
    } else if (subtest == DRREG_TEST_20_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #20: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_reg = spill_test_reg_to_slot(
                    drcontext, bb, inst, TEST_REG, &allowed, false);
            } else if (inst == instrlist_last(bb)) {
                /* Make sure that TEST_REG isn't dead after its app2app spill.
                 * If it is dead, its next spill will only reserve a slot, but not
                 * actually write to it.
                 */
                instrlist_meta_preinsert(bb, inst,
                                         XINST_CREATE_add(drcontext,
                                                          opnd_create_reg(TEST_REG),
                                                          OPND_CREATE_INT32(1)));
                CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                          DRREG_SUCCESS,
                      "cannot unreserve register");
            }
        }
    } else if (subtest == DRREG_TEST_21_C || subtest == DRREG_TEST_22_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_aflags = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #21/22: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_aflags =
                    spill_aflags_to_slot(drcontext, bb, inst, true);
            } else if (inst == instrlist_last(bb)) {
                aflags_ensure_live(drcontext, bb, inst);
                CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                      "cannot unreserve aflags");
            }
        }
    } else if (subtest == DRREG_TEST_23_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #23: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_aflags =
                    spill_aflags_to_slot(drcontext, bb, inst, true);
            } else if (instr_is_mov_constant(inst, &val) &&
                       val == TEST_INSTRUMENTATION_MARKER_3) {
                aflags_ensure_live(drcontext, bb, inst);
                CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                      "cannot unreserve aflags");
            }
        }
    } else if (subtest == DRREG_TEST_25_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #25: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_aflags =
                    spill_aflags_to_slot(drcontext, bb, inst, false);
            } else if (inst == instrlist_last(bb)) {
                aflags_ensure_live(drcontext, bb, inst);
                CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                      "cannot unreserve aflags");
            }
        }
    } else if (subtest == DRREG_TEST_36_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_reg = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #36: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_2) {
                tls_offs_app2app_spilled_reg =
                    spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed, true);
            } else if (instr_is_mov_constant(inst, &val) &&
                       val == TEST_INSTRUMENTATION_MARKER_3) {
                /* Make sure that TEST_REG isn't dead after its app2app spill.
                 * If it is dead, its next spill will only reserve a slot, but not
                 * actually write to it.
                 */
                instrlist_meta_preinsert(bb, inst,
                                         XINST_CREATE_add(drcontext,
                                                          opnd_create_reg(TEST_REG),
                                                          OPND_CREATE_INT32(1)));
                CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                          DRREG_SUCCESS,
                      "cannot unreserve register");
            }
        }
    } else if (subtest == DRREG_TEST_37_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_aflags = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #21/22: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_2) {
                tls_offs_app2app_spilled_aflags =
                    spill_aflags_to_slot(drcontext, bb, inst, true);
            } else if (instr_is_mov_constant(inst, &val) &&
                       val == TEST_INSTRUMENTATION_MARKER_3) {
                aflags_ensure_live(drcontext, bb, inst);
                CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                      "cannot unreserve aflags");
            }
        }
    } else if (subtest == DRREG_TEST_38_C) {
        CHECK(drreg_set_bb_properties(
                  drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS) == DRREG_SUCCESS,
              "unable to set bb properties");
        /* Reset for this bb. */
        tls_offs_app2app_spilled_reg = -1;
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #38: app2app phase\n");
        for (inst = instrlist_first_app(bb); inst != NULL;
             inst = instr_get_next_app(inst)) {
            if (instr_is_mov_constant(inst, &val) &&
                val == TEST_INSTRUMENTATION_MARKER_1) {
                tls_offs_app2app_spilled_reg =
                    spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed, true);
            } else if (instr_is_mov(inst) && opnd_is_reg(instr_get_src(inst, 0)) &&
                       opnd_get_reg(instr_get_src(inst, 0)) == TEST_REG) {
                /* Make sure that the app2app value of TEST_REG isn't dead. If it is
                 * dead, the insertion phase spill will only reserve a slot, but not
                 * actually write to it.
                 */
                instrlist_meta_preinsert(bb, inst,
                                         XINST_CREATE_add(drcontext,
                                                          opnd_create_reg(TEST_REG),
                                                          OPND_CREATE_INT32(1)));
                CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                          DRREG_SUCCESS,
                      "cannot unreserve register");
            }
        }
    }
    drvector_delete(&allowed);
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, OUT void *user_data)
{
    for (instr_t *instr = instrlist_first(bb); instr != NULL;
         instr = instr_get_next(instr)) {
        bool dead;
        CHECK(drreg_are_aflags_dead(drcontext, instr, &dead) == DRREG_SUCCESS,
              "drreg_are_aflags_dead should always work");
        CHECK(drx_aflags_are_dead(instr) == dead,
              "aflags liveness estimation of drx and drreg should always be consistent");
    }
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

static void
empty_clean_call()
{
    /* TODO i#4999: fix memory leak when the following statement is removed and
     * this routine is truly empty.
     */
    dr_printf("in clean call\n");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t reg, random = IF_X86_ELSE(DR_REG_XDI, DR_REG_R5);
    drreg_status_t res;
    drvector_t allowed_test_reg_1, allowed_test_reg_2;
    ptr_int_t subtest = (ptr_int_t)user_data;
    drreg_reserve_info_t info = {
        sizeof(info),
    };
    ptr_int_t val;
#ifdef X86
    drvector_t allowed_test_reg_xax;
    drreg_init_and_fill_vector(&allowed_test_reg_xax, false);
    drreg_set_vector_entry(&allowed_test_reg_xax, DR_REG_XAX, true);
#endif
    drreg_init_and_fill_vector(&allowed_test_reg_1, false);
    drreg_set_vector_entry(&allowed_test_reg_1, TEST_REG, true);

    drreg_init_and_fill_vector(&allowed_test_reg_2, false);
    drreg_set_vector_entry(&allowed_test_reg_2, TEST_REG2, true);

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

        res = drreg_reserve_register(drcontext, bb, inst, &allowed_test_reg_1, &reg);
        CHECK(res == DRREG_SUCCESS && reg == TEST_REG, "only 1 choice");
        res = drreg_reserve_register(drcontext, bb, inst, &allowed_test_reg_1, &reg);
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
               subtest == DRREG_TEST_3_C || subtest == DRREG_TEST_18_C) {
        /* Cross-app-instr tests */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #1/2/3\n");
        if (is_drreg_test_label_marker(inst)) {
            res = drreg_reserve_register(drcontext, bb, inst, &allowed_test_reg_1, &reg);
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
    } else if (subtest == DRREG_TEST_4_C || subtest == DRREG_TEST_5_C ||
               subtest == DRREG_TEST_26_C) {
        /* Cross-app-instr aflags test */
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #4/5/26\n");
        if (is_drreg_test_label_marker(inst)) {
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
        if (is_drreg_test_label_marker(inst)) {
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
    } else if (subtest == DRREG_TEST_13_C || subtest == DRREG_TEST_14_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #13/14: insertion phase\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(tls_offs_app2app_spilled_reg != -1,
                  "unable to use any spill slot in app2app phase.");
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                   &allowed_test_reg_1, true);
            CHECK(tls_offs_app2app_spilled_reg != tls_offs,
                  "found conflict in use of spill slots across multiple phases");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_3) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
#ifdef X86
    } else if (subtest == DRREG_TEST_15_C || subtest == DRREG_TEST_27_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #15/#27\n");
        if (instr_is_nop(inst)) {
            CHECK(drreg_reserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot reserve aflags");
            /* Load with some value so that we need to restore it later. */
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
#endif
    } else if (subtest == DRREG_TEST_16_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #16\n");
        if (instr_is_nop(inst)) {
            CHECK(drreg_reserve_register(drcontext, bb, inst, &allowed_test_reg_1,
                                         &reg) == DRREG_SUCCESS,
                  "cannot reserve aflags");
            /* Load with some value so that we need to restore it later. */
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_load_int(drcontext,
                                                           opnd_create_reg(TEST_REG),
                                                           OPND_CREATE_INT32(MAGIC_VAL)));
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            res = drreg_unreserve_register(drcontext, bb, inst, TEST_REG);
            CHECK(res == DRREG_SUCCESS, "default unreserve should always work");
        }
    } else if (subtest == DRREG_TEST_17_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #17\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(tls_offs_app2app_spilled_reg != -1,
                  "unable to use any spill slot in app2app phase.");
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                   &allowed_test_reg_1, true);
            CHECK(tls_offs_app2app_spilled_reg != tls_offs,
                  "found conflict in use of spill slots across multiple phases");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
#ifdef AARCH64
    } else if (subtest == DRREG_TEST_19_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #19\n");
        CHECK(dr_get_stolen_reg() == TEST_REG_STOLEN, "stolen reg doesn't match");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            tls_offs_test_reg_1 = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                         &allowed_test_reg_1, true);
            CHECK(tls_offs_test_reg_1 != TEST_FAUX_SPILL_TLS_OFFS, "unexpected tls offs");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "unreserve should work");
        }
#endif
    } else if (subtest == DRREG_TEST_20_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #20\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            tls_offs_test_reg_1 = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                         &allowed_test_reg_1, true);
            CHECK(tls_offs_app2app_spilled_reg != tls_offs_test_reg_1,
                  "spill slot conflict across phases");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
            /* So that the slot is released and can be reused below and overwritten. */
            CHECK(drreg_get_app_value(drcontext, bb, inst, TEST_REG, TEST_REG) ==
                      DRREG_SUCCESS,
                  "should get app value");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_3) {
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG2,
                                                   &allowed_test_reg_2, false);
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_load_int(drcontext,
                                                           opnd_create_reg(TEST_REG),
                                                           OPND_CREATE_INT32(MAGIC_VAL)));
            CHECK(tls_offs == tls_offs_test_reg_1, "must use the freed up slot");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG2) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
    } else if (subtest == DRREG_TEST_21_C || subtest == DRREG_TEST_22_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #21/22: insertion phase\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(tls_offs_app2app_spilled_aflags != -1,
                  "unable to use any spill slot for aflags in app2app phase.");
            uint tls_offs = spill_aflags_to_slot(drcontext, bb, inst, true);
            CHECK(tls_offs_app2app_spilled_aflags != tls_offs,
                  "found conflict in use of aflags spill slot across different phases");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_3) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
    } else if (subtest == DRREG_TEST_23_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #23\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_2) {
            tls_offs_aflags = spill_aflags_to_slot(drcontext, bb, inst, true);
            CHECK(tls_offs_app2app_spilled_aflags != tls_offs_aflags,
                  "aflags spill slot conflict across phases");
        }
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_3) {
            /* After app2app releases aflags, it'll be stored in rax as it is dead.
             * But we want it to be in a slot for this test.
             */
            ensure_aflags_in_slot(drcontext, bb, inst);
            /* Write aflags so that we need to restore later. */
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
    } else if (subtest == DRREG_TEST_24_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #24\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_aflags_to_slot(drcontext, bb, inst, true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            /* App aflags read before this app instr restored aflags. Overwrite
             * so that we need to restore them.
             */
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            res = drreg_unreserve_aflags(drcontext, bb, inst);
            CHECK(res == DRREG_SUCCESS, "unreserve should always work");
        }
    } else if (subtest == DRREG_TEST_25_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #25\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            tls_offs_aflags = spill_aflags_to_slot(drcontext, bb, inst, false);
            CHECK(tls_offs_app2app_spilled_aflags != tls_offs_aflags,
                  "aflags spill slot conflict across phases");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
            /* So that the slot is released and can be reused below and overwritten. */
            CHECK(drreg_restore_app_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "unable to restore app aflags");
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_3) {
            /* We need to reserve an extra gpr to use the aflags slot freed above
             * because the first reservation uses the slot used by the temp reg during
             * the aflags spill.
             */
            spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG2, &allowed_test_reg_2,
                                   false);
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                   &allowed_test_reg_1, false);
            CHECK(tls_offs == tls_offs_aflags, "must use the freed up slot");
            /* Overwrite aflags so that we need to restore it later. */
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG2) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
#ifdef X86
    } else if (subtest == DRREG_TEST_28_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #28\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            CHECK(drreg_reserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot reserve aflags");
            /* Load with some value so that we need to restore it later. */
            write_aflags(drcontext, bb, inst);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_statelessly_restore_app_value(drcontext, bb, DR_REG_XAX, inst,
                                                      inst, NULL, NULL) == DRREG_SUCCESS,
                  "cannot statelessly restore xax value");
            CHECK(drreg_statelessly_restore_app_value(drcontext, bb, DR_REG_XAX, inst,
                                                      inst, NULL, NULL) == DRREG_SUCCESS,
                  "cannot statelessly restore xax value");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
    } else if (subtest == DRREG_TEST_29_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #29\n");

        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            /* Spill xax so that its tool value needs to be saved before reading aflags.
             * drreg accomplishes this by reserving another reg and xchg'ing xax with it.
             */
            spill_test_reg_to_slot(drcontext, bb, inst, DR_REG_XAX, &allowed_test_reg_xax,
                                   false);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_reserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot reserve aflags");
            /* Load with some value so that we need to restore it later. */
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, DR_REG_XAX) ==
                      DRREG_SUCCESS,
                  "cannot unreserve xax");
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
#endif
    } else if (subtest == DRREG_TEST_30_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #30\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed_test_reg_1,
                                   true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
            /* Force a restore. */
            CHECK(drreg_get_app_value(drcontext, bb, inst, TEST_REG, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot get app value");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            /* We use SPILL_SLOT_9 as that is an mcontext slot. */
            dr_save_reg(drcontext, bb, inst, TEST_REG, SPILL_SLOT_9);
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_load_int(drcontext,
                                                           opnd_create_reg(TEST_REG),
                                                           OPND_CREATE_INT32(MAGIC_VAL)));
            dr_restore_reg(drcontext, bb, inst, TEST_REG, SPILL_SLOT_9);
        }
    } else if (subtest == DRREG_TEST_31_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #31\n");
        reg_id_t aflags_reg = IF_X86_ELSE(DR_REG_XAX, TEST_REG);
        /* We use a different reg for aflags restoration on non-X86. */
        reg_id_t aflags_reg2 = IF_X86_ELSE(DR_REG_XAX, TEST_REG3);
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_aflags_to_slot(drcontext, bb, inst, true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
            /* Force a restore. */
            CHECK(drreg_restore_app_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot restore aflags");
            write_aflags(drcontext, bb, inst);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            /* We use SPILL_SLOT_9 as that is an mcontext slot. */
            dr_save_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_9);
            dr_save_arith_flags_to_reg(drcontext, bb, inst, aflags_reg);
            dr_save_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_8);
            dr_restore_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_9);
            write_aflags(drcontext, bb, inst);
            dr_save_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_9);
            dr_restore_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_8);
            dr_restore_arith_flags_from_reg(drcontext, bb, inst, aflags_reg2);
            dr_restore_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_9);
        }
    } else if (subtest == DRREG_TEST_32_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #32\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            drvector_t allowed_test_reg_mcontext_reg;
            drreg_init_and_fill_vector(&allowed_test_reg_mcontext_reg, false);
            drreg_set_vector_entry(&allowed_test_reg_mcontext_reg,
                                   TEST_REG_CLEAN_CALL_MCONTEXT, true);
            spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG_CLEAN_CALL_MCONTEXT,
                                   &allowed_test_reg_mcontext_reg, true);
            drvector_delete(&allowed_test_reg_mcontext_reg);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst,
                                           TEST_REG_CLEAN_CALL_MCONTEXT) == DRREG_SUCCESS,
                  "cannot unreserve register");
            /* Force a restore. */
            CHECK(drreg_get_app_value(drcontext, bb, inst, TEST_REG_CLEAN_CALL_MCONTEXT,
                                      TEST_REG_CLEAN_CALL_MCONTEXT) == DRREG_SUCCESS,
                  "cannot get app value");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            /* This internally spills/restores regs using mcontext base.
             * This is similar to the clean call used by drcachesim.
             */
            dr_insert_clean_call_ex(drcontext, bb, inst, (void *)empty_clean_call,
                                    DR_CLEANCALL_ALWAYS_OUT_OF_LINE, 0);
            dr_insert_clean_call(drcontext, bb, inst, (void *)empty_clean_call, false, 0);
            dr_insert_clean_call(drcontext, bb, inst, (void *)check_const_eq, false, 2,
                                 opnd_create_reg(TEST_REG),
                                 OPND_CREATE_INT32(DRREG_TEST_32_C));
        }
    } else if (subtest == DRREG_TEST_33_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #33\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_aflags_to_slot(drcontext, bb, inst, true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
            /* Force a restore. */
            CHECK(drreg_restore_app_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot restore aflags");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            /* This internally spills/restores regs using mcontext base. */
            dr_insert_clean_call_ex(drcontext, bb, inst, (void *)empty_clean_call,
                                    DR_CLEANCALL_ALWAYS_OUT_OF_LINE, 0);
            dr_insert_clean_call(drcontext, bb, inst, (void *)empty_clean_call, false, 0);
            dr_insert_clean_call(drcontext, bb, inst, (void *)check_const_eq, false, 2,
                                 opnd_create_reg(TEST_REG),
                                 OPND_CREATE_INT32(DRREG_TEST_33_C));
        }
    } else if (subtest == DRREG_TEST_34_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #34\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG, &allowed_test_reg_1,
                                   true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            /* We use SPILL_SLOT_9 as that is an mcontext slot. */
            dr_save_reg(drcontext, bb, inst, TEST_REG, SPILL_SLOT_9);
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_load_int(drcontext,
                                                           opnd_create_reg(TEST_REG),
                                                           OPND_CREATE_INT32(MAGIC_VAL)));
            dr_restore_reg(drcontext, bb, inst, TEST_REG, SPILL_SLOT_9);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
    } else if (subtest == DRREG_TEST_35_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #35\n");
        reg_id_t aflags_reg = IF_X86_ELSE(DR_REG_XAX, TEST_REG);
        reg_id_t aflags_reg2 = IF_X86_ELSE(DR_REG_XAX, TEST_REG3);
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            spill_aflags_to_slot(drcontext, bb, inst, true);
        } else if (instr_is_mov_constant(inst, &val) &&
                   val == TEST_INSTRUMENTATION_MARKER_2) {
            /* We use SPILL_SLOT_9 as that is an mcontext slot. */
            dr_save_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_9);
            dr_save_arith_flags_to_reg(drcontext, bb, inst, aflags_reg);
            dr_save_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_8);
            dr_restore_reg(drcontext, bb, inst, aflags_reg, SPILL_SLOT_9);
            write_aflags(drcontext, bb, inst);
            dr_save_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_9);
            dr_restore_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_8);
            dr_restore_arith_flags_from_reg(drcontext, bb, inst, aflags_reg2);
            dr_restore_reg(drcontext, bb, inst, aflags_reg2, SPILL_SLOT_9);
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
    } else if (subtest == DRREG_TEST_36_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #36: insertion phase\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            CHECK(tls_offs_app2app_spilled_reg != -1,
                  "unable to use any spill slot in app2app phase.");
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                   &allowed_test_reg_1, true);
            CHECK(tls_offs_app2app_spilled_reg != tls_offs,
                  "found conflict in use of spill slots across multiple phases");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
    } else if (subtest == DRREG_TEST_37_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #37: insertion phase\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            CHECK(tls_offs_app2app_spilled_aflags != -1,
                  "unable to use any spill slot for aflags in app2app phase.");
            uint tls_offs = spill_aflags_to_slot(drcontext, bb, inst, true);
            CHECK(tls_offs_app2app_spilled_aflags != tls_offs,
                  "found conflict in use of aflags spill slot across different phases");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_aflags(drcontext, bb, inst) == DRREG_SUCCESS,
                  "cannot unreserve aflags");
        }
    } else if (subtest == DRREG_TEST_38_C) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drreg test #38: insertion phase\n");
        if (instr_is_mov_constant(inst, &val) && val == TEST_INSTRUMENTATION_MARKER_1) {
            CHECK(tls_offs_app2app_spilled_reg != -1,
                  "unable to use any spill slot in app2app phase.");
            uint tls_offs = spill_test_reg_to_slot(drcontext, bb, inst, TEST_REG,
                                                   &allowed_test_reg_1, true);
            CHECK(tls_offs_app2app_spilled_reg != tls_offs,
                  "found conflict in use of spill slots across multiple phases");
        } else if (drmgr_is_last_instr(drcontext, inst)) {
            CHECK(drreg_unreserve_register(drcontext, bb, inst, TEST_REG) ==
                      DRREG_SUCCESS,
                  "cannot unreserve register");
        }
    }
#ifdef X86
    drvector_delete(&allowed_test_reg_xax);
#endif
    drvector_delete(&allowed_test_reg_1);
    drvector_delete(&allowed_test_reg_2);

    /* XXX i#511: add more tests */

    /* Test state restoration without ilist of faulting fragment. */
    if (subtest == DRREG_TEST_18_C || subtest == DRREG_TEST_26_C ||
        subtest == DRREG_TEST_27_C)
        return DR_EMIT_STORE_TRANSLATIONS;
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

    /* Skip instru2instru2 aflags checks for other aflags tests, or
     * to avoid complex instrumented bbs.
     */
    if (subtest == DRREG_TEST_19_C || subtest == DRREG_TEST_20_C ||
        subtest == DRREG_TEST_21_C || subtest == DRREG_TEST_22_C ||
        subtest == DRREG_TEST_23_C || subtest == DRREG_TEST_24_C ||
        subtest == DRREG_TEST_25_C || subtest == DRREG_TEST_30_C ||
        subtest == DRREG_TEST_31_C || subtest == DRREG_TEST_32_C ||
        subtest == DRREG_TEST_33_C || subtest == DRREG_TEST_34_C ||
        subtest == DRREG_TEST_35_C || subtest == DRREG_TEST_36_C ||
        subtest == DRREG_TEST_37_C) {
        return DR_EMIT_DEFAULT;
    }

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
    if (!drmgr_unregister_bb_instrumentation_ex_event(event_app2app, event_app_analysis,
                                                      event_app_instruction,
                                                      event_instru2instru) ||
        drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "exit failed");
    /* We skip drx_init/drx_exit because that will change the number of total drreg spill
     * slots available for the drreg tests. Skipping drx_init is okay because we use only
     * drx_are_aflags_dead which doesn't need any drx state to be initialised. */
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

    note_base = drmgr_reserve_note_range(DRREG_TEST_NOTE_COUNT);
    ASSERT(note_base != DRMGR_NOTE_NONE);

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
