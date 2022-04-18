/* **********************************************************
 * Copyright (c) 2020-2021 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * API regression test for stolen register translation.
 */

#include "dr_api.h"
#include "client_tools.h"

#define BAD_VALUE 0xdeadbeef

/* We assume the app is single-threaded and don't worry about races. */
static ptr_int_t app_stolen_reg_val;

static void
restore_event(void *drcontext, void *tag, dr_mcontext_t *mcontext, bool restore_memory,
              bool app_code_consistent)
{
    /* A real client would need to check that this restore is at a point where we
     * have actually changed the value, but for this test we blindly restore on every
     * restore event for simplicity.
     */
    dr_log(drcontext, DR_LOG_ALL, 2, "Changing the stolen reg value from %ld to %ld\n",
           mcontext->IF_ARM_ELSE(r10, r28), app_stolen_reg_val);
    mcontext->IF_ARM_ELSE(r10, r28) = app_stolen_reg_val;
}

static void
do_flush(app_pc next_pc)
{
    dr_fprintf(STDERR, "Performing synchall flush\n");
    if (!dr_flush_region(NULL, ~0UL))
        DR_ASSERT(false);
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    mcontext.pc = dr_app_pc_as_jump_target(dr_get_isa_mode(drcontext), next_pc);
    dr_redirect_execution(&mcontext);
    DR_ASSERT(false);
}

// Storing the original stolen reg before mconext set.
static ptr_int_t orig_value = 0;

// Random value to detect after mcontext set.
static ptr_int_t test_value = 7;

static void
read_and_restore_stolen_reg_value()
{
    dr_fprintf(STDERR, "%s entered\n", __FUNCTION__);
    void *drcontext = dr_get_current_drcontext();

    dr_fprintf(STDERR, "test value = " IF_ARM_ELSE("%d", "%ld") "\n", test_value);

    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;

    dr_fprintf(STDERR, "fetching TLS\n");

    dr_get_mcontext(drcontext, &mc);

    /* The key part of the test: that the modified value shows up here. */
    DR_ASSERT(mc.IF_ARM_ELSE(r10, r28) == test_value);
    dr_fprintf(STDERR, "mc->stolen_reg after = " IF_ARM_ELSE("%d", "%ld") "\n",
               mc.IF_ARM_ELSE(r10, r28));

    mc.IF_ARM_ELSE(r10, r28) = orig_value;

    dr_set_mcontext(drcontext, &mc);
}

static void
change_stolen_reg_value()
{
    dr_fprintf(STDERR, "%s entered\n", __FUNCTION__);

    void *drcontext = dr_get_current_drcontext();

    dr_fprintf(STDERR, "test value = " IF_ARM_ELSE("%d", "%ld") "\n", test_value);

    dr_fprintf(STDERR, "setting TLS\n");

    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mc);

    orig_value = mc.IF_ARM_ELSE(r10, r28);

    mc.IF_ARM_ELSE(r10, r28) = test_value;

    dr_set_mcontext(drcontext, &mc);
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr, *next_next_instr;
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;
        /* Look for the sentinel-SIGSEGV sequence from the app.
         * Look for "mov <stolen-reg>, <const>; mov r0, <const>; ldr rx, [r0].
         */
        ptr_int_t stolen_val, substitute_val;
        if (instr_is_mov_constant(instr, &stolen_val) &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == dr_get_stolen_reg() &&
            next_instr != NULL && instr_is_mov_constant(next_instr, &substitute_val) &&
            substitute_val != stolen_val && opnd_is_reg(instr_get_dst(next_instr, 0)) &&
            opnd_get_reg(instr_get_dst(next_instr, 0)) == DR_REG_R0 &&
            next_next_instr != NULL && instr_reads_memory(next_next_instr) &&
            opnd_is_base_disp(instr_get_src(next_next_instr, 0)) &&
            opnd_get_base(instr_get_src(next_next_instr, 0)) == DR_REG_R0) {
            /* Now change the stolen reg value to be r0's value, before the crash. */
            dr_log(drcontext, DR_LOG_ALL, 2, "Setting stolen reg val in block %p\n", tag);
            app_stolen_reg_val = stolen_val;
            dr_insert_set_stolen_reg_value(drcontext, bb, next_next_instr, DR_REG_R0);
            break;
        }
        /* Look for the sentinel-nop sequence prior to 2nd thread creation.
         * Look for "mov <stolen-reg>, <const>; mov r0, <const>; nop.
         */
        if (instr_is_mov_constant(instr, &stolen_val) &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == dr_get_stolen_reg() &&
            next_instr != NULL && instr_is_mov_constant(next_instr, &substitute_val) &&
            substitute_val != stolen_val && opnd_is_reg(instr_get_dst(next_instr, 0)) &&
            opnd_get_reg(instr_get_dst(next_instr, 0)) == DR_REG_R0 &&
            next_next_instr != NULL && instr_is_nop(next_next_instr)) {
            /* Change the stolen reg value. */
            dr_log(drcontext, DR_LOG_ALL, 2, "Setting stolen reg val in block %p\n", tag);
            app_stolen_reg_val = stolen_val;
            dr_insert_set_stolen_reg_value(drcontext, bb, next_next_instr, DR_REG_R0);
            break;
        }
        /* Look for the sentinel-nop sequence from the app's 2nd thread.
         * Look for "mov <stolen-reg>, <const>; nop; nop.
         */
        if (instr_is_mov_constant(instr, &stolen_val) &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == dr_get_stolen_reg() &&
            next_instr != NULL && instr_is_nop(next_instr) && next_next_instr != NULL &&
            instr_is_nop(next_next_instr)) {
            dr_insert_clean_call_ex(
                drcontext, bb, next_next_instr, (void *)do_flush,
                DR_CLEANCALL_READS_APP_CONTEXT, 1,
                OPND_CREATE_INTPTR((ptr_uint_t)instr_get_app_pc(next_next_instr)));
            break;
        }

        // Look for mov <stolen-reg>, #0xdead.
        ptr_int_t imm1 = 0;
        if (instr_is_mov_constant(instr, &imm1) && opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == dr_get_stolen_reg() &&
            imm1 == 0xdead) {
            dr_insert_clean_call_ex(
                drcontext, bb, instr, (void *)change_stolen_reg_value,
                DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 0);

            dr_insert_clean_call_ex(
                drcontext, bb, instr, (void *)read_and_restore_stolen_reg_value,
                DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 0);
        }
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    // Stop test failing silently if we ever change the stolen reg value.
    if (dr_get_stolen_reg() != IF_ARM_ELSE(DR_REG_R10, DR_REG_R28)) {
        dr_fprintf(STDERR,
                   "ERROR: stolen reg value has changed, this test needs to be updated");
        DR_ASSERT(false);
    }

    dr_register_bb_event(bb_event);
    dr_register_restore_state_event(restore_event);
}
