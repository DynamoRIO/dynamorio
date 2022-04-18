/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
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

/* Tests the drwrap and drreg extensions used in concert. */

#include "dr_api.h"
#include "client_tools.h"
#include "drwrap.h"
#include "drreg.h"
#include "drmgr.h"
#include <string.h> /* memset */

#define SENTINEL 0xbeef

static int load_count;
static app_pc addr_two_args;

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    bool ok;
    CHECK(wrapcxt != NULL && user_data != NULL, "invalid arg");
    CHECK(drwrap_get_arg(wrapcxt, 0) == (void *)1, "get_arg wrong");
    CHECK(drwrap_get_arg(wrapcxt, 1) == (void *)2, "get_arg wrong");
    /* Test writing app registers (on non-x86-32bit anyway). */
    ok = drwrap_set_arg(wrapcxt, 0, (void *)42);
    CHECK(ok, "set_arg error");
    ok = drwrap_set_arg(wrapcxt, 1, (void *)43);
    CHECK(ok, "set_arg error");
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
    bool ok;
    CHECK(wrapcxt != NULL, "invalid arg");
    ok = drwrap_set_retval(wrapcxt, (void *)-4);
    CHECK(ok, "set_retval error");
}

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    if (strstr(dr_module_preferred_name(mod), "client.drwrap-drreg-test.appdll.") !=
        NULL) {
        load_count++;
        if (load_count == 2) {
            /* test no-frills */
            drwrap_set_global_flags(DRWRAP_NO_FRILLS);
        }

        addr_two_args = (app_pc)dr_get_proc_address(mod->handle, "two_args");
        CHECK(addr_two_args != NULL, "cannot find lib export");
        bool ok = drwrap_wrap(addr_two_args, wrap_pre, wrap_post);
        CHECK(ok, "wrap failed");
    }
}

static void
module_unload_event(void *drcontext, const module_data_t *mod)
{
    if (strstr(dr_module_preferred_name(mod), "client.drwrap-drreg-test.appdll.") !=
        NULL) {
        bool ok = drwrap_unwrap(addr_two_args, wrap_pre, wrap_post);
        CHECK(ok, "unwrap failed");
    }
}

static void
clean_call_rw(void)
{
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
    bool ok = dr_get_mcontext(drcontext, &mc);
    CHECK(ok, "dr_get_mcontext failed");
    CHECK(IF_X86_ELSE(mc.xdx, mc.r1) == 4, "app reg val not restored for clean call");
    IF_X86_ELSE(mc.xcx, mc.r2) = 3;
    ok = dr_set_mcontext(drcontext, &mc);
    CHECK(ok, "dr_set_mcontext failed");
}

static void
clean_call_check_rw(reg_t reg1, reg_t reg2)
{
    CHECK(reg1 == SENTINEL, "tool val in arg1 not restored after call");
    CHECK(reg2 == SENTINEL, "tool val in arg2 not restored after call");
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
    bool ok = dr_get_mcontext(drcontext, &mc);
    CHECK(ok, "dr_get_mcontext failed");
    CHECK(IF_X86_ELSE(mc.xdx, mc.r1) == SENTINEL,
          "tool val1 in mc not restored after call");
    CHECK(IF_X86_ELSE(mc.xdi, mc.r4) == SENTINEL,
          "tool val2 in mc not restored after call");
}

static void
clean_call_multipath(void)
{
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
    bool ok = dr_get_mcontext(drcontext, &mc);
    CHECK(ok, "dr_get_mcontext failed");
    CHECK(IF_X86_ELSE(mc.xdx, mc.r1) == 4, "app reg val not restored for clean call");
#ifdef X86
    /* This tests the drreg_statelessly_restore_app_value() respill which only
     * happens with aflags in xax.
     */
    CHECK(mc.xax == 0x42, "app xax not restored for clean call");
    /* The app did SAHF with AH=0xff => 0xd7. */
    CHECK((mc.xflags & 0xff) == 0xd7, "app aflags not restored for clean call");
#endif
}

static dr_emit_flags_t
event_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating, OUT void **user_data)
{
    int *nop_count = (int *)dr_thread_alloc(drcontext, sizeof(*nop_count));
    *user_data = nop_count;
    *nop_count = 0;
    return DR_EMIT_DEFAULT;
}

static void
clobber_key_regs(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    reg_id_t reg1, reg2, reg3;
    drreg_status_t res;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, false);
    /* Clobber param and return value registers. */
#ifdef X86
    drreg_set_vector_entry(&allowed, DR_REG_XAX, true);
    drreg_set_vector_entry(&allowed, DR_REG_XDI, true);
    drreg_set_vector_entry(&allowed, DR_REG_XSI, true);
#else
    drreg_set_vector_entry(&allowed, DR_REG_R0, true);
    drreg_set_vector_entry(&allowed, DR_REG_R1, true);
    drreg_set_vector_entry(&allowed, DR_REG_R2, true);
#endif
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg1);
    CHECK(res == DRREG_SUCCESS, "failed to reserve");
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg2);
    CHECK(res == DRREG_SUCCESS, "failed to reserve");
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg3);
    CHECK(res == DRREG_SUCCESS, "failed to reserve");

    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg1), reg1)),
                              OPND_CREATE_INT32(SENTINEL)));
    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg2), reg2)),
                              OPND_CREATE_INT32(SENTINEL)));
    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg3), reg3)),
                              OPND_CREATE_INT32(SENTINEL)));

    res = drreg_unreserve_register(drcontext, bb, inst, reg1);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve");
    res = drreg_unreserve_register(drcontext, bb, inst, reg2);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve");
    res = drreg_unreserve_register(drcontext, bb, inst, reg3);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve");

    drvector_delete(&allowed);
}

static void
insert_rw_call(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    reg_id_t reg1, reg2;
    drreg_status_t res;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, false);
    /* Clobber the reg we check in clean_call_rw(), and pick a 2nd for a tool value. */
#ifdef X86
    drreg_set_vector_entry(&allowed, DR_REG_XDX, true);
    drreg_set_vector_entry(&allowed, DR_REG_XDI, true);
#else
    drreg_set_vector_entry(&allowed, DR_REG_R1, true);
    drreg_set_vector_entry(&allowed, DR_REG_R4, true);
#endif
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg1);
    CHECK(res == DRREG_SUCCESS, "failed to reserve reg1");
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg2);
    CHECK(res == DRREG_SUCCESS, "failed to reserve reg2");
    res = drreg_reserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "failed to reserve aflags");

    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg1), reg1)),
                              OPND_CREATE_INT32(SENTINEL)));
    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg2), reg2)),
                              OPND_CREATE_INT32(SENTINEL)));
    dr_insert_clean_call_ex(
        drcontext, bb, inst, clean_call_rw,
        DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 0);

    /* Ensure our tool value is restored. */
    dr_insert_clean_call_ex(drcontext, bb, inst, clean_call_check_rw, 0, 2,
                            opnd_create_reg(reg1), opnd_create_reg(reg2));

    res = drreg_unreserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve aflags");
    res = drreg_unreserve_register(drcontext, bb, inst, reg2);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve reg2");
    res = drreg_unreserve_register(drcontext, bb, inst, reg1);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve reg1");

    drvector_delete(&allowed);
}

static void
insert_multipath_call(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    reg_id_t reg;
    drreg_status_t res;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, false);
    /* Clobber the reg we check in clean_call_multipath(). */
#ifdef X86
    drreg_set_vector_entry(&allowed, DR_REG_XDX, true);
#else
    drreg_set_vector_entry(&allowed, DR_REG_R1, true);
#endif
    res = drreg_reserve_register(drcontext, bb, inst, &allowed, &reg);
    CHECK(res == DRREG_SUCCESS, "failed to reserve");
    res = drreg_reserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "failed to reserve aflags");

    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_load_int(drcontext,
                              opnd_create_reg(IF_X64_ELSE(reg_64_to_32(reg), reg)),
                              OPND_CREATE_INT32(SENTINEL)));

    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    /* The app executes twice and sets rcx/r0 to 0 for one of them. */
    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_cmp(drcontext, opnd_create_reg(IF_X86_ELSE(DR_REG_XCX, DR_REG_R0)),
                         OPND_CREATE_INT32(0)));
    instrlist_meta_preinsert(
        bb, inst,
        XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(skip_call)));
    dr_insert_clean_call_ex(drcontext, bb, inst, clean_call_multipath,
                            DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_MULTIPATH, 0);
    instrlist_meta_preinsert(bb, inst, skip_call);

    res = drreg_unreserve_aflags(drcontext, bb, inst);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve aflags");
    res = drreg_unreserve_register(drcontext, bb, inst, reg);
    CHECK(res == DRREG_SUCCESS, "failed to unreserve");

    drvector_delete(&allowed);
}

#ifdef ARM
static bool
instr_is_mov_nop(instr_t *inst)
{
    /* The assembler sometimes puts in "mov r0, r0" instead of nop. */
    if (instr_get_opcode(inst) != OP_mov)
        return false;
    if (!opnd_is_reg(instr_get_src(inst, 0)) || !opnd_is_reg(instr_get_dst(inst, 0)))
        return false;
    if (opnd_get_reg(instr_get_src(inst, 0)) != opnd_get_reg(instr_get_dst(inst, 0)))
        return false;
    return true;
}
#endif

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    /* We want to have tool values in registers to test drreg restoring app values.
     * Rather than finding the location of all the drwrap clean calls, we simply
     * reserve and clobber several registers in every block.
     */
    if (drmgr_is_first_instr(drcontext, inst))
        clobber_key_regs(drcontext, bb, inst);

    /* Look for nop;nop;nop in reg_val_test() and 4 nops in multipath_test(). */
    if (instr_is_app(inst)) {
        int *nop_count = (int *)user_data;
        if (instr_get_opcode(inst) == OP_nop IF_ARM(|| instr_is_mov_nop(inst))) {
            ++(*nop_count);
        } else {
            if (*nop_count == 3) {
                insert_rw_call(drcontext, bb, inst);
            } else if (*nop_count == 4) {
                insert_multipath_call(drcontext, bb, inst);
            }
            *nop_count = 0;
        }
    }

    if (drmgr_is_last_instr(drcontext, inst))
        dr_thread_free(drcontext, user_data, sizeof(int));
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    drreg_exit();
    drwrap_exit();
    if (!drmgr_unregister_bb_instrumentation_event(event_analysis) ||
        !drmgr_unregister_module_load_event(module_load_event) ||
        !drmgr_unregister_module_unload_event(module_unload_event))
        CHECK(false, "init failed");
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 3 /*max slots needed*/, false };
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS || !drwrap_init())
        CHECK(false, "init failed");
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_analysis, event_app_instruction,
                                                 NULL) ||
        !drmgr_register_module_load_event(module_load_event) ||
        !drmgr_register_module_unload_event(module_unload_event))
        CHECK(false, "init failed");
}
