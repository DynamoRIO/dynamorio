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

/* Test xl8 pc of rip-rel instruction (xref #3307) caused by
 * asynch interrupt.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include "mangle_suspend-shared.h"
#include <string.h> /* memset */

#define MAX_RESUME_COUNT 10

static byte *add_instr_pc = NULL;

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, OUT void **user_data)
{
    instr_t *inst;
    bool prev_was_mov_const = false;
    ptr_int_t val1, val2;
    *user_data = NULL;

    if (translating)
        return DR_EMIT_DEFAULT;

#ifdef X86_64
    /* Look for duplicate mov immediates telling us which subaction we're in */
    for (inst = instrlist_first_app(bb); inst != NULL; inst = instr_get_next_app(inst)) {
        if (instr_is_mov_constant(inst, prev_was_mov_const ? &val2 : &val1)) {
            if (prev_was_mov_const && val1 == val2 &&
                val1 != 0 && /* rule out xor w/ self */
                opnd_is_reg(instr_get_dst(inst, 0)) &&
                opnd_get_reg(instr_get_dst(inst, 0)) == SUSPEND_TEST_REG) {
                *user_data = (void *)val1;
                instrlist_meta_postinsert(bb, inst, INSTR_CREATE_label(drcontext));
            } else
                prev_was_mov_const = true;
        } else
            prev_was_mov_const = false;
    }
#else
        /* XXX i#3329: port to ARM if possible. */
#endif
    return DR_EMIT_DEFAULT;
}

static void
suspend_test_1_func()
{
#ifdef X86_64
    void **drcontexts = NULL;
    uint num_suspended;
    if (dr_suspend_all_other_threads(&drcontexts, &num_suspended, NULL)) {
        if (num_suspended != 1)
            CHECK(false, "num_suspended unexpected!");
        dr_mcontext_t mc = { sizeof(mc), DR_MC_INTEGER | DR_MC_CONTROL };
        if (!dr_get_mcontext(drcontexts[0], &mc))
            CHECK(false, "dr_get_mcontext failed!");
        /* Check xip such that we only perform the check if we are in the loop
         * body of the test, in order to avoid races. The magic values 16 and 42
         * are byte counts to compute the bounds of the asm loop in the test.
         * The values need to stay in synch with the test.
         */
        if (mc.xip >= add_instr_pc - 16 && mc.xip <= add_instr_pc + 42) {
            if (add_instr_pc == mc.xip) {
                if (TEST_1_LOOP_COUNT_REG_MC != 1) {
                    CHECK(false, "loop count reg expected to be 1");
                    exit(1);
                }
            } else if (TEST_1_LOOP_COUNT_REG_MC != 2) {
                CHECK(false, "loop count reg expected to be 2");
                exit(1);
            }
        }
        int resume_count = 0;
        while (!dr_resume_all_other_threads(drcontexts, num_suspended)) {
            if (resume_count++ == MAX_RESUME_COUNT) {
                CHECK(false, "resume failed!");
                exit(1);
            }
        }
    }
#else
    /* XXX i#3329: port to ARM if possible. */
#endif
}

static void
suspend_test_2_func()
{
#ifdef X86_64
    void **drcontexts = NULL;
    uint num_suspended;
    if (dr_suspend_all_other_threads(&drcontexts, &num_suspended, NULL)) {
        if (num_suspended != 1)
            CHECK(false, "num_suspended unexpected!");
        dr_mcontext_t mc = { sizeof(mc), DR_MC_INTEGER | DR_MC_CONTROL };
        if (!dr_get_mcontext(drcontexts[0], &mc))
            CHECK(false, "dr_get_mcontext failed!");
        /* Check xip such that we only perform the check if we are in the loop
         * body of the test, in order to avoid races. The magic values 16 and 42
         * are byte counts to compute the bounds of the asm loop in the test.
         * The values need to stay in synch with the test.
         */
        if (mc.xip >= add_instr_pc - 16 && mc.xip <= add_instr_pc + 42) {
            if (add_instr_pc == mc.xip) {
                if (TEST_2_LOOP_COUNT_REG_MC != 1) {
                    CHECK(false, "loop count reg expected to be 1");
                    exit(1);
                }
            } else if (TEST_2_LOOP_COUNT_REG_MC != 2) {
                CHECK(false, "loop count reg expected to be 2");
                exit(1);
            }
            if (TEST_2_CHECK_REG_MC != 0) {
                CHECK(false, "check reg expected to be 0");
                exit(1);
            }
        }
        int resume_count = 0;
        while (!dr_resume_all_other_threads(drcontexts, num_suspended)) {
            if (resume_count++ == MAX_RESUME_COUNT) {
                CHECK(false, "resume failed!");
                exit(1);
            }
        }
    }
#else
    /* XXX i#3329: port to ARM if possible. */
#endif
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    ptr_int_t subaction = (ptr_int_t)user_data;

    if (translating)
        return DR_EMIT_DEFAULT;

    if (subaction == TEST_VAL_C) {
        if (instr_is_label(inst)) {
#ifdef X86_64
            for (instr_t *inst = instrlist_first(bb); inst; inst = instr_get_next(inst)) {
                if (instr_get_opcode(inst) == OP_add &&
                    instr_has_rel_addr_reference(inst)) {
                    add_instr_pc = instr_get_app_pc(inst);
                    return DR_EMIT_DEFAULT;
                }
            }
            CHECK(false, "add instruction not found");
#endif
        }
    } else if (subaction == SUSPEND_VAL_TEST_1_C) {
        if (instr_is_label(inst)) {
            /* This is expected to be in a separate thread that the test creates. The
             * test is just executing the label instructions in a loop.
             */
            dr_insert_clean_call_ex(drcontext, bb, inst, suspend_test_1_func,
                                    DR_CLEANCALL_READS_APP_CONTEXT, 0);
        }
    } else if (subaction == SUSPEND_VAL_TEST_2_C) {
        if (instr_is_label(inst)) {
            dr_insert_clean_call_ex(drcontext, bb, inst, suspend_test_2_func,
                                    DR_CLEANCALL_READS_APP_CONTEXT, 0);
        }
    }

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction))
        CHECK(false, "exit failed");

    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    if (!drmgr_init())
        CHECK(false, "init failed");
    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_app_analysis,
                                                 event_app_instruction, NULL))
        CHECK(false, "init failed");
}
