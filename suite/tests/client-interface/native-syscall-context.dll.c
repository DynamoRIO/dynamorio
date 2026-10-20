/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include <string.h>

static app_pc native_syscalls, return_pc;
static int yield_number = -1, before, after;
static char syscall_name[128] = "NtYieldExecution";

static dr_emit_flags_t
event_bb(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (dr_fragment_app_pc(tag) == native_syscalls)
        return DR_EMIT_GO_NATIVE;
    return DR_EMIT_DEFAULT;
}

static bool
filter(void *dc, int number)
{
    return number == yield_number;
}

static void
check_context(void *dc)
{
    dr_mcontext_t context = { sizeof(context), DR_MC_CONTROL };
    ASSERT(dr_get_mcontext(dc, &context));
    ASSERT(context.pc == return_pc);
}

static bool
pre_syscall(void *dc, int number)
{
    if (number == yield_number) {
        check_context(dc);
        ++before;
    }
    return true;
}

static void
post_syscall(void *dc, int number)
{
    if (number == yield_number) {
        check_context(dc);
        ++after;
    }
}

static void
event_exit(void)
{
    ASSERT(before == 3 && after == 3);
    dr_fprintf(STDERR, "native callbacks verified\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *app = dr_get_main_module();
    module_data_t *ntdll = dr_lookup_module_by_name("ntdll.dll");
    void *dc = dr_get_current_drcontext();
    instr_t *inst = instr_create(dc);
    app_pc pc = (app_pc)dr_get_proc_address(ntdll->handle, syscall_name);
    native_syscalls = (app_pc)dr_get_proc_address(app->handle, "native_syscalls");
    ASSERT(native_syscalls != NULL && pc != NULL);
    for (int i = 0; i < 16; ++i) {
        app_pc next = decode(dc, pc, inst);
        ASSERT(next != NULL);
        instr_set_translation(inst, pc);
        if (instr_get_opcode(inst) == OP_mov_imm && opnd_is_reg(instr_get_dst(inst, 0)) &&
            opnd_get_reg(instr_get_dst(inst, 0)) == DR_REG_EAX)
            yield_number = (int)opnd_get_immed_int(instr_get_src(inst, 0));
        if (instr_is_syscall(inst)) {
            return_pc = next;
            break;
        }
        instr_reset(dc, inst);
        pc = next;
    }
    instr_destroy(dc, inst);
    dr_free_module_data(ntdll);
    dr_free_module_data(app);
    ASSERT(yield_number >= 0 && return_pc != NULL);
    ASSERT(dr_syscall_intercept_natively(syscall_name, yield_number, 0, 0));

    dr_register_bb_event(event_bb);
    dr_register_filter_syscall_event(filter);
    dr_register_pre_syscall_event(pre_syscall);
    dr_register_post_syscall_event(post_syscall);
    dr_register_exit_event(event_exit);
}
