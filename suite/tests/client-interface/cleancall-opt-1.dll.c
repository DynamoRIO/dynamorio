/* *******************************************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of MIT nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL MIT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test the clean call inliner. */

#include "dr_api.h"

/* List of instrumentation functions. */
#define FUNCTIONS()             \
    FUNCTION(empty)             \
    FUNCTION(out_of_line)       \
    FUNCTION(inscount)          \
    FUNCTION(compiler_inscount) \
    FUNCTION(bbcount)           \
    FUNCTION(aflags_clobber)    \
    LAST_FUNCTION()

static void
compiler_inscount(ptr_uint_t count);

static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating);
#include "cleancall-opt-shared.h"

static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *entry = instrlist_first(bb);
    app_pc entry_pc = instr_get_app_pc(entry);
    int i;
    bool inline_expected = false;
    bool out_of_line_expected = true;
    instr_t *before_label;
    instr_t *after_label;

    for (i = 0; i < N_FUNCS; i++) {
        if (entry_pc == func_app_pcs[i])
            break;
    }
    if (i == N_FUNCS)
        return DR_EMIT_DEFAULT;

    /* We're inserting a call to a function in this bb. */
    func_called[i] = 1;
    dr_insert_clean_call(dc, bb, entry, (void *)before_callee, false, 2,
                         OPND_CREATE_INTPTR(func_ptrs[i]),
                         OPND_CREATE_INTPTR(func_names[i]));

    before_label = INSTR_CREATE_label(dc);
    after_label = INSTR_CREATE_label(dc);

    /* FIXME i#1569: passing instruction operands is NYI on AArch64.
     * We use a workaround involving ADR. */
    IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_start_pc, before_label));
    PRE(bb, entry, before_label);

    switch (i) {
    default: dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0); break;
    case FN_inscount:
    case FN_compiler_inscount:
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 1,
                             OPND_CREATE_INT32(0xDEAD));
        break;
    }

    PRE(bb, entry, after_label);
    IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_end_pc, after_label));

    switch (i) {
    default: out_of_line_expected = false; break;
    case FN_out_of_line: out_of_line_expected = true; break;
    }

    dr_insert_clean_call_ex(
        dc, bb, entry, (void *)after_callee, DR_CLEANCALL_READS_APP_CONTEXT,
        IF_X86_ELSE(6, 4),
#ifdef X86
        opnd_create_instr(before_label), opnd_create_instr(after_label),
#endif
        OPND_CREATE_INT32(inline_expected), OPND_CREATE_INT32(out_of_line_expected),
        OPND_CREATE_INT32(i), OPND_CREATE_INTPTR(func_names[i]));

    return DR_EMIT_DEFAULT;
}

/*****************************************************************************/
/* Instrumentation function code generation. */

/* Modifies all GPRS and SIMD registers on X86 only. */
static instrlist_t *
codegen_out_of_line(void *dc)
{
    int i;
    instrlist_t *ilist = instrlist_create(dc);

    codegen_prologue(dc, ilist);
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        reg_id_t reg = DR_REG_START_GPR + (reg_id_t)i;
        if (reg == DR_REG_XSP || reg == IF_X86_ELSE(DR_REG_XBP, DR_REG_LR))
            continue;
        APP(ilist,
            XINST_CREATE_load_int(dc, opnd_create_reg(reg), OPND_CREATE_INTPTR(0xf1f1)));
    }
    /* FIXME i#1569: FMOV support is NYI on AArch64 */
#ifdef X86
    for (i = 0; i < proc_num_simd_registers(); i++) {
        reg_id_t reg = DR_REG_XMM0 + (reg_id_t)i;
#    ifdef __AVX512F__
        /* For the AVX-512 version, we're still using vmovq to a XMM register, but it
         * zeroes out [MAX_VL:64] (32 for 32-bit), so the test is still working as
         * we want.
         */
        APP(ilist,
            INSTR_ENCODING_HINT(
                IF_X64_ELSE(INSTR_CREATE_vmovq, INSTR_CREATE_vmovd)(
                    dc, opnd_create_reg(reg), opnd_create_reg(DR_REG_START_GPR)),
                DR_ENCODING_HINT_X86_EVEX));
#    else
        APP(ilist,
            IF_X64_ELSE(INSTR_CREATE_vmovq, INSTR_CREATE_vmovd)(
                dc, opnd_create_reg(reg), opnd_create_reg(DR_REG_START_GPR)));
#    endif
    }

    /* Modify flags */
    APP(ilist,
        INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_INT32(0xffff)));
#endif
    codegen_epilogue(dc, ilist);
    return ilist;
}
