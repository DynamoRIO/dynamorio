/* *******************************************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"

#include <stddef.h> /* offsetof */
#include <string.h> /* memset */

#define CALLEE_ALIGNMENT 64

#define PRE instrlist_meta_preinsert
#define APP instrlist_meta_append

/* Table of function names. */
#define FUNCTION(fn_name) #fn_name,
#define LAST_FUNCTION() NULL
static const char *func_names[] = { FUNCTIONS() };
#undef FUNCTION
#undef LAST_FUNCTION

/* Codegen function declarations. */
#define FUNCTION(fn_name) static instrlist_t *codegen_##fn_name(void *dc);
#define LAST_FUNCTION()
FUNCTIONS()
#undef FUNCTION
#undef LAST_FUNCTION

/* Table of codegen functions. */
typedef instrlist_t *(*codegen_func_t)(void *dc);
#define FUNCTION(fn_name) codegen_##fn_name,
#define LAST_FUNCTION() NULL
static codegen_func_t codegen_funcs[] = { FUNCTIONS() };
#undef FUNCTION
#undef LAST_FUNCTION

/* Create an enum for each function. */
#define FUNCTION(fn_name) FN_##fn_name,
#define LAST_FUNCTION() LAST_FUNC_ENUM
enum { FUNCTIONS() };
#undef FUNCTION
#undef LAST_FUNCTION

/* A separate define so ctags can find it. */
#define N_FUNCS LAST_FUNC_ENUM

static app_pc func_app_pcs[N_FUNCS];
static void *func_ptrs[N_FUNCS];
static bool func_called[N_FUNCS];

/* Instrumentation machine code memory. */
static void *rwx_mem;
static size_t rwx_size;

static void
codegen_instrumentation_funcs(void);
static void
free_instrumentation_funcs(void);
static void
lookup_pcs(void);

/* Generate the instrumentation. */
static void
codegen_instrumentation_funcs(void)
{
    void *dc = dr_get_current_drcontext();
    instrlist_t *ilists[N_FUNCS];
    int i;
    size_t offset = 0;
    uint rwx_prot;
    app_pc pc;

    /* Generate all of the ilists. */
    for (i = 0; i < N_FUNCS; i++) {
        ilists[i] = codegen_funcs[i](dc);
    }

    /* Compute size of each instr and the total offset. */
    for (i = 0; i < N_FUNCS; i++) {
        instr_t *inst;
        offset = ALIGN_FORWARD(offset, CALLEE_ALIGNMENT);
        for (inst = instrlist_first(ilists[i]); inst; inst = instr_get_next(inst))
            offset += instr_length(dc, inst);
    }

    /* Allocate RWX memory for the code and fill it with nops.  nops make
     * reading the disassembly in gdb easier.  */
    rwx_prot = DR_MEMPROT_EXEC | DR_MEMPROT_READ | DR_MEMPROT_WRITE;
    rwx_size = ALIGN_FORWARD(offset, PAGE_SIZE);
    rwx_mem = dr_nonheap_alloc(rwx_size, rwx_prot);
    memset(rwx_mem, 0x90, rwx_size);

    /* encode instructions, telling instrlist_encode to care about the labels */
    pc = (byte *)rwx_mem;
    for (i = 0; i < N_FUNCS; i++) {
        pc = (byte *)ALIGN_FORWARD(pc, CALLEE_ALIGNMENT);
        func_ptrs[i] = pc;
        dr_log(dc, DR_LOG_EMIT, 3, "Generated instrumentation function %s at " PFX ":",
               func_names[i], pc);
        instrlist_disassemble(dc, pc, ilists[i], dr_get_logfile(dc));
        pc = instrlist_encode(dc, ilists[i], pc, true);
        instrlist_clear_and_destroy(dc, ilists[i]);
    }
}

/* Free the instrumentation machine code. */
static void
free_instrumentation_funcs(void)
{
    dr_nonheap_free(rwx_mem, rwx_size);
}

static void
lookup_pcs(void)
{
    module_data_t *exe;
    int i;

    exe = dr_lookup_module_by_name(dr_get_application_name());
    DR_ASSERT_MSG(exe != NULL, "Could not find application binary name in modules!");
    for (i = 0; i < N_FUNCS; i++) {
        app_pc func_pc = (app_pc)dr_get_proc_address(exe->handle, func_names[i]);
        DR_ASSERT_MSG(func_pc != NULL,
                      "Unable to find a function we wanted to instrument!");
        func_app_pcs[i] = func_pc;
    }
    dr_free_module_data(exe);
}

static void
event_exit(void)
{
    int i;
    free_instrumentation_funcs();

    for (i = 0; i < N_FUNCS; i++) {
        DR_ASSERT_MSG(func_called[i], "Instrumentation function was not called!");
    }
    dr_fprintf(STDERR, "PASSED\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    dr_fprintf(STDERR, "INIT\n");

    /* Lookup pcs. */
    lookup_pcs();
    codegen_instrumentation_funcs();
    /* For compiler_inscount, we don't use generated code, we just point
     * straight at the compiled code.
     */
#if !defined(TEST_INLINE) || defined(X86)
    func_ptrs[FN_compiler_inscount] = (void *)&compiler_inscount;
#endif
}

#ifdef X86
static int reg_offsets[DR_NUM_GPR_REGS + 1] = {
    offsetof(dr_mcontext_t, xax),   offsetof(dr_mcontext_t, xbx),
    offsetof(dr_mcontext_t, xcx),   offsetof(dr_mcontext_t, xdx),
    offsetof(dr_mcontext_t, xdi),   offsetof(dr_mcontext_t, xsi),
    offsetof(dr_mcontext_t, xbp),   offsetof(dr_mcontext_t, xsp),
#    ifdef X64
    offsetof(dr_mcontext_t, r8),    offsetof(dr_mcontext_t, r9),
    offsetof(dr_mcontext_t, r10),   offsetof(dr_mcontext_t, r11),
    offsetof(dr_mcontext_t, r12),   offsetof(dr_mcontext_t, r13),
    offsetof(dr_mcontext_t, r14),   offsetof(dr_mcontext_t, r15),
#    endif
    offsetof(dr_mcontext_t, xflags)
};
#elif defined(AARCH64)
static int reg_offsets[DR_NUM_GPR_REGS + 1] = {
    offsetof(dr_mcontext_t, r0),    offsetof(dr_mcontext_t, r1),
    offsetof(dr_mcontext_t, r2),    offsetof(dr_mcontext_t, r3),
    offsetof(dr_mcontext_t, r4),    offsetof(dr_mcontext_t, r5),
    offsetof(dr_mcontext_t, r6),    offsetof(dr_mcontext_t, r7),
    offsetof(dr_mcontext_t, r8),    offsetof(dr_mcontext_t, r9),
    offsetof(dr_mcontext_t, r10),   offsetof(dr_mcontext_t, r11),
    offsetof(dr_mcontext_t, r12),   offsetof(dr_mcontext_t, r13),
    offsetof(dr_mcontext_t, r14),   offsetof(dr_mcontext_t, r15),
    offsetof(dr_mcontext_t, r16),   offsetof(dr_mcontext_t, r17),
    offsetof(dr_mcontext_t, r18),   offsetof(dr_mcontext_t, r19),
    offsetof(dr_mcontext_t, r20),   offsetof(dr_mcontext_t, r21),
    offsetof(dr_mcontext_t, r22),   offsetof(dr_mcontext_t, r23),
    offsetof(dr_mcontext_t, r24),   offsetof(dr_mcontext_t, r25),
    offsetof(dr_mcontext_t, r26),   offsetof(dr_mcontext_t, r27),
    offsetof(dr_mcontext_t, r28),   offsetof(dr_mcontext_t, r29),
    offsetof(dr_mcontext_t, r30),   offsetof(dr_mcontext_t, r31),
    offsetof(dr_mcontext_t, xflags)
};
#endif

/* Globals used by instrumentation functions. */
ptr_uint_t global_count;
static uint callee_inlined;

static dr_mcontext_t before_mcontext = {
    sizeof(before_mcontext),
    DR_MC_ALL,
};
static int before_errno;
static dr_mcontext_t after_mcontext = {
    sizeof(after_mcontext),
    DR_MC_ALL,
};
static int after_errno;

#ifdef AARCH64
static app_pc cleancall_start_pc;
static app_pc cleancall_end_pc;
#endif

static bool
mcontexts_equal(dr_mcontext_t *mc_a, dr_mcontext_t *mc_b, int func_index)
{
    int i;
#ifdef X86
    int simd_bytes_used;
#endif
    /* Check GPRs. */
    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
        reg_t a = *(reg_t *)((byte *)mc_a + reg_offsets[i]);
        reg_t b = *(reg_t *)((byte *)mc_b + reg_offsets[i]);
        if (a != b)
            return false;
    }

#ifdef TEST_INLINE
    /* Check xflags for all funcs except bbcount, which has dead flags. */
    if (mc_a->xflags != mc_b->xflags IF_X86(&&func_index != FN_bbcount))
        return false;
#else
    if (mc_a->xflags != mc_b->xflags)
        return false;
#endif

#ifdef X86
    /* Only look at the initialized bits of the SSE regs. */
    /* XXX i#1312: fix and extend test for AVX-512. */
    simd_bytes_used = (proc_has_feature(FEATURE_AVX) ? 32 : 16);
#    ifdef __AVX512F__
    /* If the test was compiled with AVX-512, it implies that the machine supported it. */
    simd_bytes_used = 64;
#    endif
    /* FIXME i#1312: this needs to be proc_num_simd_registers() once we fully support
     * saving AVX-512 state for clean calls. The clean call test is already clobbering
     * AVX-512 extended registers, but we can't compare and test them here until we
     * support saving and restoring them.
     */
    for (i = 0; i < proc_num_simd_saved(); i++) {
        if (memcmp(&mc_a->simd[i], &mc_b->simd[i], simd_bytes_used) != 0)
            return false;
    }
#elif defined(AARCH64)
    size_t vl = proc_get_vector_length_bytes();
    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        if (memcmp(&mc_a->simd[i], &mc_b->simd[i], vl) != 0)
            return false;
    }
    if (proc_has_feature(FEATURE_SVE)) {
        for (i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
            if (memcmp(&mc_a->svep[i], &mc_b->svep[i], vl / 8) != 0)
                return false;
        }
        if (memcmp(&mc_a->ffr, &mc_b->ffr, vl / 8) != 0)
            return false;
    }
#endif

    return true;
}

static void
dump_diff_mcontexts(void)
{
    int i;
    dr_fprintf(STDERR,
               "Registers clobbered by supposedly clean call!\n"
               "Printing GPRs + flags:\n");
    for (i = 0; i < DR_NUM_GPR_REGS + 1; i++) {
        reg_t before_reg = *(reg_t *)((byte *)&before_mcontext + reg_offsets[i]);
        reg_t after_reg = *(reg_t *)((byte *)&after_mcontext + reg_offsets[i]);
        const char *reg_name =
            (i < DR_NUM_GPR_REGS ? get_register_name(DR_REG_START_GPR + i) : "xflags");
        const char *diff_str = (before_reg == after_reg ? "" : " <- DIFFERS");
        dr_fprintf(STDERR, "%s before: " PFX " after: " PFX "%s\n", reg_name, before_reg,
                   after_reg, diff_str);
    }

#ifdef X86
    dr_fprintf(STDERR, "Printing XMM regs:\n");
#elif defined(AARCH64)
    dr_fprintf(STDERR, "Printing SIMD/SVE regs:\n");
#endif
    /* XXX i#1312: check if test can get extended to AVX-512. */
    for (i = 0; i < proc_num_simd_registers(); i++) {
#ifdef X86
        dr_zmm_t before_reg = before_mcontext.simd[i];
        dr_zmm_t after_reg = after_mcontext.simd[i];
        size_t mmsz = proc_has_feature(FEATURE_AVX) ? sizeof(dr_ymm_t) : sizeof(dr_xmm_t);
#    ifdef __AVX512F__
        /* If the test was compiled with AVX-512, it implies that the machine supported
         * it.
         */
        mmsz = sizeof(dr_zmm_t);
#    endif
        const char *diff_str =
            (memcmp(&before_reg, &after_reg, mmsz) == 0 ? "" : " <- DIFFERS");
        dr_fprintf(STDERR, "xmm%2d before: %08x%08x%08x%08x", i, before_reg.u32[0],
                   before_reg.u32[1], before_reg.u32[2], before_reg.u32[3]);
        if (proc_has_feature(FEATURE_AVX)) {
            dr_fprintf(STDERR, "%08x%08x%08x%08x", before_reg.u32[4], before_reg.u32[5],
                       before_reg.u32[6], before_reg.u32[7]);
        }
        dr_fprintf(STDERR, " after: %08x%08x%08x%08x", after_reg.u32[0], after_reg.u32[1],
                   after_reg.u32[2], after_reg.u32[3]);
        if (proc_has_feature(FEATURE_AVX)) {
            dr_fprintf(STDERR, "%08x%08x%08x%08x", after_reg.u32[4], after_reg.u32[5],
                       after_reg.u32[6], after_reg.u32[7]);
        }
#elif defined(AARCH64)
        const size_t mmsz = proc_get_vector_length_bytes();
        dr_simd_t before_reg, after_reg;
        char reg_name[4];
        if (i >= (MCXT_NUM_SIMD_SVE_SLOTS + MCXT_NUM_SVEP_SLOTS)) {
            strcpy(reg_name, "FFR");
            before_reg = before_mcontext.ffr;
            after_reg = after_mcontext.ffr;
        } else if (i >= MCXT_NUM_SIMD_SVE_SLOTS) {
            dr_snprintf(reg_name, 4, "P%2d", i - MCXT_NUM_SIMD_SVE_SLOTS);
            before_reg = before_mcontext.svep[i - MCXT_NUM_SIMD_SVE_SLOTS];
            after_reg = after_mcontext.svep[i - MCXT_NUM_SIMD_SVE_SLOTS];
        } else {
            dr_snprintf(reg_name, 4, "Z%2d", i);
            before_reg = before_mcontext.simd[i];
            after_reg = after_mcontext.simd[i];
        }

        const char *diff_str =
            (memcmp(&before_reg, &after_reg, mmsz) == 0 ? "" : " <- DIFFERS");

        dr_fprintf(STDERR, "%s before: %08x%08x%08x%08x", reg_name, before_reg.u32[0],
                   before_reg.u32[1], before_reg.u32[2], before_reg.u32[3]);
        dr_fprintf(STDERR, " after: %08x%08x%08x%08x", after_reg.u32[0], after_reg.u32[1],
                   after_reg.u32[2], after_reg.u32[3]);
#endif
        dr_fprintf(STDERR, "%s\n", diff_str);
    }
}

static void
dump_cc_code(void *dc, app_pc start_inline, app_pc end_inline, int func_index)
{
    app_pc pc, next_pc;
    dr_fprintf(STDERR, "Clean call code for %s:\n", func_names[func_index]);
    for (pc = start_inline; pc != end_inline; pc = next_pc) {
        next_pc = disassemble(dc, pc, STDERR);
    }
}

static void
#ifdef AARCH64
after_callee(bool inline_expected, bool out_of_line_expected, int func_index,
             const char *func_name)
#else
after_callee(app_pc start_inline, app_pc end_inline, bool inline_expected,
             bool out_of_line_expected, int func_index, const char *func_name)
#endif
{

    void *dc;
#ifdef AARCH64
    app_pc start_inline = cleancall_start_pc;
    app_pc end_inline = cleancall_end_pc;
#endif

    /* Save mcontext after call. */
    dc = dr_get_current_drcontext();
    dr_get_mcontext(dc, &after_mcontext);

    /* Compare mcontexts. */
    if (before_errno != after_errno) {
        dr_fprintf(STDERR, "errnos differ!\nbefore: %d, after: %d\n", before_errno,
                   after_errno);
    }
    if (!mcontexts_equal(&before_mcontext, &after_mcontext, func_index)) {
        dump_diff_mcontexts();
        dump_cc_code(dc, start_inline, end_inline, func_index);
    }

    /* Now that we use the mcontext in dcontext, we expect no stack usage. */
    if (inline_expected) {
        app_pc pc, next_pc;
        instr_t instr;
        bool found_xsp = false;
        instr_init(dc, &instr);
        for (pc = start_inline; pc != end_inline; pc = next_pc) {
            next_pc = decode(dc, pc, &instr);
            if (instr_uses_reg(&instr, DR_REG_XSP)) {
                found_xsp = true;
            }
            instr_reset(dc, &instr);
        }
        if (found_xsp) {
            dr_fprintf(STDERR, "Found stack usage in inlined code for %s\n",
                       func_names[func_index]);
            dump_cc_code(dc, start_inline, end_inline, func_index);
        }
    }

    if (inline_expected && !callee_inlined) {
        dr_fprintf(STDERR, "Function %s was not inlined!\n", func_names[func_index]);
        dump_cc_code(dc, start_inline, end_inline, func_index);
    } else if (!inline_expected && callee_inlined) {
        dr_fprintf(STDERR, "Function %s was inlined unexpectedly!\n",
                   func_names[func_index]);
        dump_cc_code(dc, start_inline, end_inline, func_index);
    }

    /* Now that we use the mcontext in dcontext, we expect no stack usage. */
    if (out_of_line_expected) {
        app_pc pc, next_pc;
        instr_t instr;
        uint blr_count = 0;
        instr_init(dc, &instr);
        for (pc = start_inline; pc != end_inline; pc = next_pc) {
            next_pc = decode(dc, pc, &instr);
            if (instr_get_opcode(&instr) == IF_X86_ELSE(OP_call, OP_blr)) {
                blr_count += 1;
            }
            instr_reset(dc, &instr);
        }
        if (blr_count != 3) {
            dr_fprintf(
                STDERR,
                "Expected out-of-line call but did not find exactly 3 " IF_X86_ELSE(
                    "CALL", "BLR") " instructions.\n");
            dump_cc_code(dc, start_inline, end_inline, func_index);
        }
    }

    /* Function-specific checks. */
    switch (func_index) {
    case FN_bbcount:
        if (global_count != 1) {
            dr_fprintf(STDERR, "global_count not updated properly after bbcount!\n");
            dump_cc_code(dc, start_inline, end_inline, func_index);
        }
        break;
    case FN_inscount:
#if !defined(TEST_INLINE) || defined(X86)
    case FN_compiler_inscount:
#endif
        if (global_count != 0xDEAD) {
            dr_fprintf(STDERR, "global_count not updated properly after inscount!\n");
            dump_cc_code(dc, start_inline, end_inline, func_index);
        }
        break;
    default: break;
    }

    if (func_name != NULL)
        dr_fprintf(STDERR, "Called func %s.\n", func_name);
}

#ifdef AARCH64
static void
save_current_pc(void *dc, instrlist_t *ilist, instr_t *where, app_pc *ptr, instr_t *label)
{
    opnd_t scratch_reg1 = opnd_create_reg(DR_REG_X0);
    opnd_t scratch_reg2 = opnd_create_reg(DR_REG_X1);
    PRE(ilist, where,
        INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_SP), opnd_create_reg(DR_REG_SP),
                         OPND_CREATE_INT16(16)));

    PRE(ilist, where,
        INSTR_CREATE_stp(dc, opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_16),
                         scratch_reg1, scratch_reg2));

    instrlist_insert_mov_immed_ptrsz(dc, (long)ptr, scratch_reg1, ilist, where, NULL,
                                     NULL);
    PRE(ilist, where, INSTR_CREATE_adr(dc, scratch_reg2, opnd_create_instr(label)));
    PRE(ilist, where,
        INSTR_CREATE_str(dc, opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_8),
                         scratch_reg2));
    PRE(ilist, where,
        INSTR_CREATE_ldp(dc, scratch_reg1, scratch_reg2,
                         opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_16)));
    PRE(ilist, where,
        INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_SP), opnd_create_reg(DR_REG_SP),
                         OPND_CREATE_INT16(16)));
}
#endif

/* Reset global_count and patch the out-of-line version of the instrumentation function
 * so we can find out if it got called, which would mean it wasn't inlined.
 *
 * XXX: We modify the callee code!  If DR tries to disassemble the callee's
 * ilist after the modification, it will trigger assertion failures in the
 * disassembler.
 */
static void
before_callee(app_pc func, const char *func_name)
{
    void *dc;
#ifdef TEST_INLINE
    instrlist_t *ilist;
    byte *end_pc;
    opnd_t scratch_reg = opnd_create_reg(IF_X86_ELSE(DR_REG_XAX, DR_REG_X0));
#endif

    if (func_name != NULL)
        dr_fprintf(STDERR, "Calling func %s...\n", func_name);

    /* Save mcontext before call. */
    dc = dr_get_current_drcontext();
    dr_get_mcontext(dc, &before_mcontext);

    /* If this is compiler_inscount, we need to unprotect our own text section
     * so we can make this code modification.
     */
    if (func == (app_pc)compiler_inscount) {
        app_pc start_pc = (app_pc)ALIGN_BACKWARD(func, PAGE_SIZE);
        app_pc end_pc = func;
        instr_t instr;
        instr_init(dc, &instr);
        do {
            instr_reset(dc, &instr);
            end_pc = decode(dc, end_pc, &instr);
        } while (!instr_is_return(&instr));
        end_pc += instr_length(dc, &instr);
        instr_reset(dc, &instr);
        end_pc = (app_pc)ALIGN_FORWARD(end_pc, PAGE_SIZE);
        dr_memory_protect(start_pc, (size_t)(end_pc - start_pc),
                          DR_MEMPROT_EXEC | DR_MEMPROT_READ | DR_MEMPROT_WRITE);
    }

#ifdef TEST_INLINE
    ilist = instrlist_create(dc);
#    if defined(X86)
    /* Patch the callee to be:
     * push xax
     * mov xax, &callee_inlined
     * mov dword [xax], 0
     * pop xax
     * ret
     */
    APP(ilist, INSTR_CREATE_push(dc, scratch_reg));
    APP(ilist,
        INSTR_CREATE_mov_imm(dc, scratch_reg, OPND_CREATE_INTPTR(&callee_inlined)));
    APP(ilist,
        INSTR_CREATE_mov_st(dc, OPND_CREATE_MEM32(DR_REG_XAX, 0), OPND_CREATE_INT32(0)));
    APP(ilist, INSTR_CREATE_pop(dc, scratch_reg));
    APP(ilist, INSTR_CREATE_ret(dc));
#    elif defined(AARCH64)
    APP(ilist,
        INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_SP), opnd_create_reg(DR_REG_SP),
                         OPND_CREATE_INT16(16)));

    APP(ilist,
        INSTR_CREATE_str(dc, opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8),
                         scratch_reg));

    instrlist_insert_mov_immed_ptrsz(dc, (long)&callee_inlined, scratch_reg, ilist, NULL,
                                     NULL, NULL);
    APP(ilist,
        INSTR_CREATE_str(dc, opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_8),
                         opnd_create_reg(DR_REG_XZR)));
    APP(ilist,
        INSTR_CREATE_ldr(dc, scratch_reg,
                         opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_8)));
    APP(ilist,
        INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_SP), opnd_create_reg(DR_REG_SP),
                         OPND_CREATE_INT16(16)));
    APP(ilist, INSTR_CREATE_br(dc, opnd_create_reg(DR_REG_X30)));
#    endif /* X86 */
    end_pc = instrlist_encode(dc, ilist, func, false /* no jump targets */);
    instrlist_clear_and_destroy(dc, ilist);
    dr_log(dc, DR_LOG_EMIT, 3, "Patched instrumentation function %s at " PFX ":\n",
           (func_name ? func_name : "(null)"), func);

    /* Check there was enough room in the function.  We align every callee
     * entry point to CALLEE_ALIGNMENT, so each function has at least
     * CALLEE_ALIGNMENT bytes long.
     */
    DR_ASSERT_MSG(end_pc < func + CALLEE_ALIGNMENT,
                  "Patched code too big for smallest function!");
    callee_inlined = 1;
#endif /* ifdef TEST_INLINE */

    /* Reset instrumentation globals. */
    global_count = 0;
}

/*
prologue:
    push REG_XBP
    mov REG_XBP, REG_XSP
*/
static void
codegen_prologue(void *dc, instrlist_t *ilist)
{
#ifdef X86
    APP(ilist, INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_XBP)));
    APP(ilist,
        INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_XBP),
                            opnd_create_reg(DR_REG_XSP)));
#endif
}

/*
epilogue:
    leave
    ret
*/
static void
codegen_epilogue(void *dc, instrlist_t *ilist)
{
#ifdef X86
    APP(ilist, INSTR_CREATE_leave(dc));
#endif
    APP(ilist, XINST_CREATE_return(dc));
}

/*
empty:
    ret
*/
static instrlist_t *
codegen_empty(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    APP(ilist, XINST_CREATE_return(dc));
    return ilist;
}

/* i#988: We fail to inline if the number of arguments to the same clean call
 * routine increases. empty is used for a 0 arg clean call, so we add empty_1arg
 * for test_inlined_call_args(), which passes 1 arg.
 */
static instrlist_t *
codegen_empty_1arg(void *dc)
{
    return codegen_empty(dc);
}

/* Return either a stack access opnd_t or the first regparm.  Assumes frame
 * pointer is not omitted. */
static opnd_t
codegen_opnd_arg1(void)
{
    /* FIXME: Perhaps DR should expose this.  It currently tracks this in
     * core/instr.h. */
#if defined(AARCH64)
    return opnd_create_reg(DR_REG_X0);
#elif defined(X64)
    return opnd_create_reg(IF_WINDOWS_ELSE(DR_REG_RCX, DR_REG_RDI));
#else  /* X86 */
    /* Stack offset accounts for an additional push in prologue. */
    return OPND_CREATE_MEMPTR(DR_REG_XBP, 2 * sizeof(reg_t));
#endif /* AARCH64 */
}

/* We want to test that we can auto-inline whatever the compiler generates for
 * inscount.
 */
static void
compiler_inscount(ptr_uint_t count)
{
    global_count += count;
}

/* We generate an empty ilist for compiler_inscount and don't use it.
 * Originally I tried to decode compiler_inscount and re-encode it in the RWX
 * memory along with our other callees, but that breaks 32-bit PIC code.  Even
 * if we set the translation for each instruction in this ilist, that will be
 * lost when we encode and decode in the inliner.
 */
static instrlist_t *
codegen_compiler_inscount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    APP(ilist, XINST_CREATE_return(dc));
    return ilist;
}

/*
X86:
  inscount:
      push REG_XBP
      mov REG_XBP, REG_XSP
      mov REG_XAX, ARG1
      add [global_count], REG_XAX
      leave
      ret

AArch64:
   inscount:
     x4 and x5 are used as scratch registers
     mov &global_count to x4 using a series of movz and movk instructions
     ldr x5, [x4]
     add x5, x5, x0
     str [x4], x5
*/
static instrlist_t *
codegen_inscount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t scratch1 = opnd_create_reg(IF_X86_ELSE(DR_REG_XAX, DR_REG_X4));
    IF_AARCH64(opnd_t scratch2);
    codegen_prologue(dc, ilist);
#ifdef X86
    APP(ilist, INSTR_CREATE_mov_ld(dc, scratch1, codegen_opnd_arg1()));
    APP(ilist,
        INSTR_CREATE_add(dc, OPND_CREATE_ABSMEM(&global_count, OPSZ_PTR), scratch1));
#elif defined(AARCH64)
    scratch2 = opnd_create_reg(DR_REG_X5);
    instrlist_insert_mov_immed_ptrsz(dc, (long)&global_count, scratch1, ilist, NULL, NULL,
                                     NULL);
    APP(ilist,
        INSTR_CREATE_ldr(
            dc, scratch2,
            opnd_create_base_disp(opnd_get_reg(scratch1), DR_REG_NULL, 0, 0, OPSZ_8)));
    APP(ilist, INSTR_CREATE_add(dc, scratch2, scratch2, codegen_opnd_arg1()));
    APP(ilist,
        INSTR_CREATE_str(
            dc, opnd_create_base_disp(opnd_get_reg(scratch1), DR_REG_NULL, 0, 0, OPSZ_8),
            scratch2));
#else
#    error NYI
#endif
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
X86
  bbcount:
      push REG_XBP
      mov REG_XBP, REG_XSP
      inc [global_count]
      leave
      ret

AArch64
   bbcount:
     x0 and x1 are used as scratch registers
     mov &global_count to x0 using a series of movz and movk instructions
     ldr x1, [x0]
     add x1, x1, #1
     str [x0], x1

*/
static instrlist_t *
codegen_bbcount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
#ifndef X86
    reg_t reg1 = DR_REG_X0;
    reg_t reg2 = DR_REG_X1;
#endif

    codegen_prologue(dc, ilist);
#ifdef X86
    APP(ilist, INSTR_CREATE_inc(dc, OPND_CREATE_ABSMEM(&global_count, OPSZ_PTR)));
#else
    instrlist_insert_mov_immed_ptrsz(dc, (ptr_int_t)&global_count, opnd_create_reg(reg1),
                                     ilist, NULL, NULL, NULL);
    APP(ilist, XINST_CREATE_load(dc, opnd_create_reg(reg2), OPND_CREATE_MEMPTR(reg1, 0)));
    APP(ilist, XINST_CREATE_add(dc, opnd_create_reg(reg2), OPND_CREATE_INT(1)));
    APP(ilist,
        XINST_CREATE_store(dc, OPND_CREATE_MEMPTR(reg1, 0), opnd_create_reg(reg2)));
#endif
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Clobber aflags.  Clean call optimizations must ensure they are restored.
X86 - Zero the aflags
    aflags_clobber:
      push REG_XBP
      mov REG_XBP, REG_XSP
      mov REG_XAX, 0
      add al, HEX(7F)
      sahf
      leave
      ret

AArch64 - All ones the aflags
    aflags_clobber:
      sets the NZCV flags (bits 31, 30, 29, 28)
      orr     x1, xzr, #0xF0000000
      msr     NZCV, x1

TODO: x86 does the following when testing aflags, do the same for aarch64.
 * save aflags
 * set aflags to expected value
 * clean call to test function
 * read aflags & check if they match the expected value
 * restore original aflags
*/
static instrlist_t *
codegen_aflags_clobber(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    codegen_prologue(dc, ilist);
#ifdef X86
    APP(ilist,
        INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_INTPTR(0)));
    APP(ilist, INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_AL), OPND_CREATE_INT8(0x7F)));
    APP(ilist, INSTR_CREATE_sahf(dc));
#elif defined(AARCH64)
    opnd_t reg1 = opnd_create_reg(DR_REG_X0);
    opnd_t opnd_zero_reg = opnd_create_reg(DR_REG_XZR);
    opnd_t opnd_nzcv_reg = opnd_create_reg(DR_REG_NZCV);
    APP(ilist,
        instr_create_1dst_2src(dc, OP_orr, reg1, opnd_zero_reg,
                               OPND_CREATE_INT32(0xF0000000)));
    APP(ilist, instr_create_1dst_1src(dc, OP_msr, opnd_nzcv_reg, reg1));
#endif
    codegen_epilogue(dc, ilist);
    return ilist;
}
