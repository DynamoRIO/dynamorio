/* **********************************************************
 * Copyright (c) 2022 Arm Limited   All rights reserved.
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

/* Tests a memory operand's index register is handled correctly if it happens
 * to be the stolen register W28, rather than X28.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "client_tools.h"
#include "drreg-test-shared.h"

static void
event_exit(void)
{
    drreg_exit();
    drmgr_exit();
    drutil_exit();
}

/* Simply calls drutil_insert_get_mem_addr(). */
static bool
insert_get_addr(void *drcontext, instrlist_t *ilist, instr_t *instr, opnd_t mref)
{
    reg_id_t reg_ptr, reg_tmp, reg_addr;

    if (drreg_reserve_register(drcontext, ilist, instr, NULL, &reg_tmp) !=
        DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    if (drreg_reserve_register(drcontext, ilist, instr, NULL, &reg_ptr) !=
        DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    /* XXX i#5498: We should check the address in reg_ptr for correctness. A
     * constant reference address in this client to check for correctness based
     * on the test binary stolen-reg-index is not a viable approach because
     * different versions of compiler, linker/loader and OS will produce
     * different addresses at runtime.
     */
    if (!drutil_insert_get_mem_addr(drcontext, ilist, instr, mref, reg_ptr, reg_tmp)) {
        dr_printf("drutil_insert_get_mem_addr() failed!\n");
        return false;
    }

    if (drreg_unreserve_register(drcontext, ilist, instr, reg_tmp) != DRREG_SUCCESS)
        DR_ASSERT(false);

    if (drreg_unreserve_register(drcontext, ilist, instr, reg_ptr) != DRREG_SUCCESS)
        DR_ASSERT(false);

    return true;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    opnd_t opnd;
    reg_id_t reg;
    reg_id_t stolen = dr_get_stolen_reg();
    DR_ASSERT(stolen == DR_REG_X28);

    /* Selects store insruction, str x1, [x0, w28, uxtw #0] from test subject,
     * stolen-reg-index.c.
     */
    if (instr_writes_memory(inst)) {
        opnd = instr_get_dst(inst, 0);
        if (opnd_is_memory_reference(opnd) && opnd_is_base_disp(opnd)) {
            if (opnd_get_index(opnd) == stolen && opnd_get_base(opnd) == DR_REG_X0)
                dr_printf("store memref with index reg X28\n");
            if (opnd_get_index(opnd) == DR_REG_W28 && opnd_get_base(opnd) == DR_REG_X0)
                dr_printf("store memref with index reg W28\n");
            if (insert_get_addr(drcontext, bb, inst, instr_get_dst(inst, 0)))
                return DR_EMIT_DEFAULT;
            else
                DR_ASSERT(false);
        }
    }

    /* Selects load instruction, ldr x1, [x0, x28, lsl #0] from test subject,
     * stolen-reg-index.c.
     */
    if (instr_reads_memory(inst)) {
        opnd = instr_get_src(inst, 0);
        if (opnd_is_memory_reference(opnd) && opnd_is_base_disp(opnd)) {
            if (opnd_get_index(opnd) == stolen && opnd_get_base(opnd) == DR_REG_X0)
                dr_printf("load memref with index reg X28\n");
            if (opnd_get_index(opnd) == DR_REG_W28 && opnd_get_base(opnd) == DR_REG_X0)
                dr_printf("load memref with index reg W28\n");
            if (insert_get_addr(drcontext, bb, inst, instr_get_src(inst, 0)))
                return DR_EMIT_DEFAULT;
            else
                DR_ASSERT(false);
        }
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);
    CHECK(dr_get_stolen_reg() == TEST_REG_STOLEN, "stolen reg doesn't match");

    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
}
