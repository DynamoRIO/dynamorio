/* *******************************************************************************
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
#include "client_tools.h"

#include <stddef.h> /* offsetof */
#include <string.h> /* memset */

#define CALLEE_ALIGNMENT 64

#define PRE  instrlist_meta_preinsert
#define APP  instrlist_meta_append

/* List of instrumentation functions. */
#define FUNCTIONS() \
        FUNCTION(empty) \
        FUNCTION(empty_1arg) \
        FUNCTION(inscount) \
        FUNCTION(gcc47_inscount) \
        FUNCTION(callpic_pop) \
        FUNCTION(callpic_mov) \
        FUNCTION(nonleaf) \
        FUNCTION(cond_br) \
        FUNCTION(tls_clobber) \
        FUNCTION(aflags_clobber) \
        FUNCTION(compiler_inscount) \
        FUNCTION(bbcount) \
        LAST_FUNCTION()

/* Table of function names. */
#define FUNCTION(fn_name) #fn_name,
#define LAST_FUNCTION() NULL
static const char *func_names[] = {
    FUNCTIONS()
};
#undef FUNCTION
#undef LAST_FUNCTION

/* Codegen function declarations. */
#define FUNCTION(fn_name) \
    static instrlist_t *codegen_##fn_name(void *dc);
#define LAST_FUNCTION()
FUNCTIONS()
#undef FUNCTION
#undef LAST_FUNCTION

/* Table of codegen functions. */
typedef instrlist_t *(*codegen_func_t)(void *dc);
#define FUNCTION(fn_name) codegen_##fn_name,
#define LAST_FUNCTION() NULL
static codegen_func_t codegen_funcs[] = {
    FUNCTIONS()
};
#undef FUNCTION
#undef LAST_FUNCTION

/* Create an enum for each function. */
#define FUNCTION(fn_name) FN_##fn_name,
#define LAST_FUNCTION() LAST_FUNC_ENUM
enum {
    FUNCTIONS()
};
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

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *dc, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);
static void lookup_pcs(void);
static void codegen_instrumentation_funcs(void);
static void free_instrumentation_funcs(void);
static void compiler_inscount(ptr_uint_t count);
static void test_inlined_call_args(void *dc, instrlist_t *bb, instr_t *where,
                                   int fn_idx);

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    dr_fprintf(STDERR, "INIT\n");

    /* Lookup pcs. */
    lookup_pcs();
    codegen_instrumentation_funcs();
}

static void
event_exit(void)
{
    int i;
    free_instrumentation_funcs();

    for (i = 0; i < N_FUNCS; i++) {
        DR_ASSERT_MSG(func_called[i],
                      "Instrumentation function was not called!");
    }
    dr_fprintf(STDERR, "PASSED\n");
}

static void
lookup_pcs(void)
{
    module_data_t *exe;
    int i;

    exe = dr_lookup_module_by_name(
#ifdef WINDOWS
            "client.inline.exe"
#else
            "client.inline"
#endif
            );
    for (i = 0; i < N_FUNCS; i++) {
        app_pc func_pc = (app_pc)dr_get_proc_address(
                exe->handle, func_names[i]);
        DR_ASSERT_MSG(func_pc != NULL,
                      "Unable to find a function we wanted to instrument!");
        func_app_pcs[i] = func_pc;
    }
    dr_free_module_data(exe);
}

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

    /* Compute size of each instr and set each note with the offset. */
    for (i = 0; i < N_FUNCS; i++) {
        instr_t *inst;
        offset = ALIGN_FORWARD(offset, CALLEE_ALIGNMENT);
        for (inst = instrlist_first(ilists[i]); inst;
             inst = instr_get_next(inst)) {
            instr_set_note(inst, (void *)(ptr_int_t)offset);
            offset += instr_length(dc, inst);
        }
    }

    /* Allocate RWX memory for the code and fill it with nops.  nops make
     * reading the disassembly in gdb easier.  */
    rwx_prot = DR_MEMPROT_EXEC|DR_MEMPROT_READ|DR_MEMPROT_WRITE;
    rwx_size = ALIGN_FORWARD(offset, PAGE_SIZE);
    rwx_mem = dr_nonheap_alloc(rwx_size, rwx_prot);
    memset(rwx_mem, 0x90, rwx_size);

    /* Encode instructions.  We don't worry about labels, since the notes are
     * already set. */
    pc = (byte*)rwx_mem;
    for (i = 0; i < N_FUNCS; i++) {
        pc = (byte*)ALIGN_FORWARD(pc, CALLEE_ALIGNMENT);
        func_ptrs[i] = pc;
        dr_log(dc, LOG_EMIT, 3, "Generated instrumentation function %s at "PFX
               ":", func_names[i], pc);
        instrlist_disassemble(dc, pc, ilists[i], dr_get_logfile(dc));
        pc = instrlist_encode(dc, ilists[i], pc, false);
        instrlist_clear_and_destroy(dc, ilists[i]);
    }

    /* For compiler_inscount, we don't use generated code, we just point
     * straight at the compiled code.
     */
    func_ptrs[FN_compiler_inscount] = (void*)&compiler_inscount;
}

/* Free the instrumentation machine code. */
static void
free_instrumentation_funcs(void)
{
    dr_nonheap_free(rwx_mem, rwx_size);
}

/* Globals used by instrumentation functions. */
ptr_uint_t global_count;
static uint callee_inlined;

static dr_mcontext_t before_mcontext = {sizeof(before_mcontext),DR_MC_ALL,};
static int before_errno;
static dr_mcontext_t after_mcontext = {sizeof(after_mcontext),DR_MC_ALL,};
static int after_errno;

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
    instrlist_t *ilist;
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    byte *end_pc;

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
                          DR_MEMPROT_EXEC|DR_MEMPROT_READ|DR_MEMPROT_WRITE);
    }

    /* Patch the callee to be:
     * push xax
     * mov xax, &callee_inlined
     * mov dword [xax], 0
     * pop xax
     * ret
     */
    ilist = instrlist_create(dc);
    APP(ilist, INSTR_CREATE_push(dc, xax));
    APP(ilist, INSTR_CREATE_mov_imm
        (dc, xax, OPND_CREATE_INTPTR(&callee_inlined)));
    APP(ilist, INSTR_CREATE_mov_st
        (dc, OPND_CREATE_MEM32(DR_REG_XAX, 0), OPND_CREATE_INT32(0)));
    APP(ilist, INSTR_CREATE_pop(dc, xax));
    APP(ilist, INSTR_CREATE_ret(dc));

    end_pc = instrlist_encode(dc, ilist, func, false /* no jump targets */);
    instrlist_clear_and_destroy(dc, ilist);
    dr_log(dc, LOG_EMIT, 3, "Patched instrumentation function %s at "PFX":\n",
           (func_name ? func_name : "(null)"), func);

    /* Check there was enough room in the function.  We align every callee
     * entry point to CALLEE_ALIGNMENT, so each function has at least
     * CALLEE_ALIGNMENT bytes long.
     */
    DR_ASSERT_MSG(end_pc < func + CALLEE_ALIGNMENT,
                  "Patched code too big for smallest function!");

    /* Reset instrumentation globals. */
    global_count = 0;
    callee_inlined = 1;
}

#define NUM_GP_REGS   (1 + (IF_X64_ELSE(DR_REG_R15, DR_REG_XDI) - DR_REG_XAX))
static int reg_offsets[NUM_GP_REGS + 1] = {
    offsetof(dr_mcontext_t, xax),
    offsetof(dr_mcontext_t, xbx),
    offsetof(dr_mcontext_t, xcx),
    offsetof(dr_mcontext_t, xdx),
    offsetof(dr_mcontext_t, xdi),
    offsetof(dr_mcontext_t, xsi),
    offsetof(dr_mcontext_t, xbp),
    offsetof(dr_mcontext_t, xsp),
#ifdef X64
    offsetof(dr_mcontext_t, r8),
    offsetof(dr_mcontext_t, r9),
    offsetof(dr_mcontext_t, r10),
    offsetof(dr_mcontext_t, r11),
    offsetof(dr_mcontext_t, r12),
    offsetof(dr_mcontext_t, r13),
    offsetof(dr_mcontext_t, r14),
    offsetof(dr_mcontext_t, r15),
#endif
    offsetof(dr_mcontext_t, xflags)
};

static bool
mcontexts_equal(dr_mcontext_t *mc_a, dr_mcontext_t *mc_b, int func_index)
{
    int i;
    int ymm_bytes_used;
    /* Check GPRs. */
    for (i = 0; i < NUM_GP_REGS; i++) {
        reg_t a = *(reg_t*)((byte*)mc_a + reg_offsets[i]);
        reg_t b = *(reg_t*)((byte*)mc_b + reg_offsets[i]);
        if (a != b)
            return false;
    }

    /* Check xflags for all funcs except bbcount, which has dead flags. */
    if (mc_a->xflags != mc_b->xflags && func_index != FN_bbcount)
        return false;

    /* Only look at the initialized bits of the SSE regs. */
    ymm_bytes_used = (proc_has_feature(FEATURE_AVX) ? 32 : 16);
    for (i = 0; i < NUM_SIMD_SLOTS; i++) {
        if (memcmp(&mc_a->ymm[i], &mc_b->ymm[i], ymm_bytes_used) != 0)
            return false;
    }
    return true;
}

static void
dump_diff_mcontexts(void)
{
    uint i;
    dr_fprintf(STDERR, "Registers clobbered by supposedly clean call!\n"
               "Printing GPRs + flags:\n");
    for (i = 0; i < NUM_GP_REGS + 1; i++) {
        reg_t before_reg = *(reg_t*)((byte*)&before_mcontext + reg_offsets[i]);
        reg_t after_reg  = *(reg_t*)((byte*)&after_mcontext  + reg_offsets[i]);
        const char *reg_name = (i < NUM_GP_REGS ?
                                get_register_name(DR_REG_XAX + i) :
                                "xflags");
        const char *diff_str = (before_reg == after_reg ?
                                "" : " <- DIFFERS");
        dr_fprintf(STDERR, "%s before: "PFX" after: "PFX"%s\n",
                   reg_name, before_reg, after_reg, diff_str);
    }

    dr_fprintf(STDERR, "Printing XMM regs:\n");
    for (i = 0; i < NUM_SIMD_SLOTS; i++) {
        dr_ymm_t before_reg = before_mcontext.ymm[i];
        dr_ymm_t  after_reg =  after_mcontext.ymm[i];
        size_t mmsz = proc_has_feature(FEATURE_AVX) ? sizeof(dr_xmm_t) :
                sizeof(dr_ymm_t);
        const char *diff_str =
                (memcmp(&before_reg, &after_reg, mmsz) == 0 ? "" : " <- DIFFERS");
        dr_fprintf(STDERR, "xmm%2d before: %08x%08x%08x%08x",
                   i,
                   before_reg.u32[0], before_reg.u32[1],
                   before_reg.u32[2], before_reg.u32[3]);
        if (proc_has_feature(FEATURE_AVX)) {
            dr_fprintf(STDERR, "%08x%08x%08x%08x",
                       before_reg.u32[4], before_reg.u32[5],
                       before_reg.u32[6], before_reg.u32[7]);
        }
        dr_fprintf(STDERR, " after: %08x%08x%08x%08x",
                   after_reg.u32[0], after_reg.u32[1],
                   after_reg.u32[2], after_reg.u32[3]);
        if (proc_has_feature(FEATURE_AVX)) {
            dr_fprintf(STDERR, "%08x%08x%08x%08x",
                       after_reg.u32[4], after_reg.u32[5],
                       after_reg.u32[6], after_reg.u32[7]);
        }
        dr_fprintf(STDERR, "%s\n", diff_str);
    }
}

static void
dump_inlined_code(void *dc, app_pc start_inline, app_pc end_inline,
                  int func_index)
{
    app_pc pc, next_pc;
    dr_fprintf(STDERR, "Inlined code for %s:\n", func_names[func_index]);
    for (pc = start_inline; pc != end_inline; pc = next_pc) {
        next_pc = disassemble(dc, pc, STDERR);
    }
}

static void
after_callee(app_pc start_inline, app_pc end_inline, bool inline_expected,
             int func_index, const char *func_name)
{
    void *dc;

    /* Save mcontext after call. */
    dc = dr_get_current_drcontext();
    dr_get_mcontext(dc, &after_mcontext);

    /* Compare mcontexts. */
    if (before_errno != after_errno) {
        dr_fprintf(STDERR, "errnos differ!\nbefore: %d, after: %d\n",
                   before_errno, after_errno);
    }
    if (!mcontexts_equal(&before_mcontext, &after_mcontext, func_index)) {
        dump_diff_mcontexts();
        dump_inlined_code(dc, start_inline, end_inline, func_index);
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
            dump_inlined_code(dc, start_inline, end_inline, func_index);
        }
    }

    if (inline_expected && !callee_inlined) {
        dr_fprintf(STDERR, "Function %s was not inlined!\n",
                   func_names[func_index]);
        dump_inlined_code(dc, start_inline, end_inline, func_index);
    } else if (!inline_expected && callee_inlined) {
        dr_fprintf(STDERR, "Function %s was inlined unexpectedly!\n",
                   func_names[func_index]);
        dump_inlined_code(dc, start_inline, end_inline, func_index);
    }

    /* Function-specific checks. */
    switch (func_index) {
    case FN_inscount:
    case FN_compiler_inscount:
        if (global_count != 0xDEAD) {
            dr_fprintf(STDERR, "global_count not updated properly after inscount!\n");
            dump_inlined_code(dc, start_inline, end_inline, func_index);
        }
        break;
    default:
        break;
    }

    if (func_name != NULL)
        dr_fprintf(STDERR, "Called func %s.\n", func_name);
}

static void
fill_scratch(void)
{
    void *dc = dr_get_current_drcontext();
    int slot;
    /* Set slots to 0x000... 0x111... 0x222... etc. */
    for (slot = SPILL_SLOT_1; slot <= SPILL_SLOT_MAX; slot++) {
        reg_t value = slot * 0x11111111;
        dr_write_saved_reg(dc, slot, value);
    }
}

static void
check_scratch(void)
{
    void *dc = dr_get_current_drcontext();
    int slot;
    /* Check that slots are 0x000... 0x111... 0x222... etc. */
    for (slot = SPILL_SLOT_1; slot <= SPILL_SLOT_MAX; slot++) {
        reg_t value = dr_read_saved_reg(dc, slot);
        reg_t expected = slot * 0x11111111;
        if (value != expected)
            dr_fprintf(STDERR, "Client scratch slot clobbered by clean call!\n");
    }
}

static void
check_aflags(int actual, int expected)
{
    byte ah = (actual >> 8) & 0xFF;
    byte al = actual & 0xFF;
    byte eh = (expected >> 8) & 0xFF;
    byte el = expected & 0xFF;
    dr_fprintf(STDERR, "actual: %04x, expected: %04x\n", actual, expected);
    DR_ASSERT_MSG(ah == eh, "Aflags clobbered!");
    DR_ASSERT_MSG(al == el, "Overflow clobbered!");
    dr_fprintf(STDERR, "passed for %04x\n", expected);
}

static instr_t *
test_aflags(void *dc, instrlist_t *bb, instr_t *where, int aflags,
            instr_t *before_label, instr_t *after_label)
{
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    opnd_t al = opnd_create_reg(DR_REG_AL);

    /* Save flags and REG_XAX
     * XXX: Assumes we can push to application stack, which happens to be valid
     * for this test application.
     *
     * pushf
     * mov [SPILL_SLOT_1], REG_XAX
     */
    PRE(bb, where, INSTR_CREATE_pushf(dc));
    PRE(bb, where, INSTR_CREATE_mov_st
        (dc, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_1), xax));
    /* Then populate aflags from XAX:
     * mov REG_XAX, aflags
     * add al, HEX(7F)
     * sahf ah
     */
    PRE(bb, where, INSTR_CREATE_mov_imm(dc, xax, OPND_CREATE_INTPTR(aflags)));
    PRE(bb, where, INSTR_CREATE_add(dc, al, OPND_CREATE_INT8(0x7F)));
    PRE(bb, where, INSTR_CREATE_sahf(dc));

    if (before_label != NULL)
        PRE(bb, where, before_label);
    dr_insert_clean_call(dc, bb, where, func_ptrs[FN_aflags_clobber], false, 0);
    if (after_label != NULL)
        PRE(bb, where, after_label);

    /* Get the flags back into XAX, and then to SPILL_SLOT_2:
     * mov REG_XAX, 0
     * lahf
     * seto al
     * mov [SPILL_SLOT_2], REG_XAX
     */
    PRE(bb, where, INSTR_CREATE_mov_imm(dc, xax, OPND_CREATE_INTPTR(0)));
    PRE(bb, where, INSTR_CREATE_lahf(dc));
    PRE(bb, where, INSTR_CREATE_setcc(dc, OP_seto, al));
    PRE(bb, where, INSTR_CREATE_mov_st
        (dc, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_2), xax));

    /* Assert that they match the original flags. */
    dr_insert_clean_call(dc, bb, where, (void*)check_aflags, false, 2,
                         dr_reg_spill_slot_opnd(dc, SPILL_SLOT_2),
                         OPND_CREATE_INT32(aflags));

    /* Restore and flags. */
    PRE(bb, where, INSTR_CREATE_mov_ld
        (dc, xax, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_1)));
    PRE(bb, where, INSTR_CREATE_popf(dc));
    return where;
}

static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *entry = instrlist_first(bb);
    app_pc entry_pc = instr_get_app_pc(entry);
    int i;
    bool inline_expected = true;
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
    dr_insert_clean_call(dc, bb, entry, (void*)before_callee, false, 2,
                         OPND_CREATE_INTPTR(func_ptrs[i]),
                         OPND_CREATE_INTPTR(func_names[i]));

    before_label = INSTR_CREATE_label(dc);
    after_label = INSTR_CREATE_label(dc);

    switch (i) {
    default:
        /* Default behavior is to call instrumentation with no-args and
         * assert it gets inlined.
         */
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        break;
    case FN_empty_1arg:
    case FN_inscount:
    case FN_gcc47_inscount:
    case FN_compiler_inscount:
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 1,
                             OPND_CREATE_INT32(0xDEAD));
        PRE(bb, entry, after_label);
        break;
    case FN_nonleaf:
    case FN_cond_br:
        /* These functions cannot be inlined (yet). */
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        inline_expected = false;
        break;
    case FN_tls_clobber:
        dr_insert_clean_call(dc, bb, entry, (void*)fill_scratch, false, 0);
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        dr_insert_clean_call(dc, bb, entry, (void*)check_scratch, false, 0);
        break;
    case FN_aflags_clobber:
        /* ah is: SF:ZF:0:AF:0:PF:1:CF.  If we turn everything on we will
         * get all 1's except bits 3 and 5, giving a hex mask of 0xD7.
         * Overflow is in the low byte (al usually) so use use a mask of
         * 0xD701 first.  If we turn everything off we get 0x0200.
         */
        entry = test_aflags(dc, bb, entry, 0xD701, before_label, after_label);
        (void)test_aflags(dc, bb, entry, 0x00200, NULL, NULL);
        break;
    }
    dr_insert_clean_call(dc, bb, entry, (void*)after_callee, false, 5,
                         opnd_create_instr(before_label),
                         opnd_create_instr(after_label),
                         OPND_CREATE_INT32(inline_expected),
                         OPND_CREATE_INT32(i),
                         OPND_CREATE_INTPTR(func_names[i]));

    if (i == FN_inscount || i == FN_empty_1arg) {
        test_inlined_call_args(dc, bb, entry, i);
    }

    return DR_EMIT_DEFAULT;
}

/* For all regs, pass arguments of the form:
 * %reg
 * (%reg, %xax, 1)-0xDEAD
 * (%xax, %reg, 1)-0xDEAD
 */
static void
test_inlined_call_args(void *dc, instrlist_t *bb, instr_t *where, int fn_idx)
{
    uint i;
    static const ptr_uint_t hex_dead_global = 0xDEAD;

    for (i = 0; i < NUM_GP_REGS; i++) {
        reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
        reg_id_t other_reg = (reg == DR_REG_XAX ? DR_REG_XBX : DR_REG_XAX);
        opnd_t arg;
        instr_t *before_label;
        instr_t *after_label;

        /* FIXME: We should test passing the app %xsp to an inlined function,
         * but I hesitate to store a non-stack location in XSP.
         */
        if (reg == DR_REG_XSP)
            continue;

        /* %reg */
        before_label = INSTR_CREATE_label(dc);
        after_label = INSTR_CREATE_label(dc);
        arg = opnd_create_reg(reg);
        dr_insert_clean_call(dc, bb, where, (void*)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, INSTR_CREATE_mov_imm
            (dc, arg, OPND_CREATE_INTPTR(0xDEAD)));
        dr_insert_clean_call(dc, bb, where, (void*)func_ptrs[fn_idx], false, 1,
                             arg);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call(dc, bb, where, (void*)after_callee, false, 5,
                             opnd_create_instr(before_label),
                             opnd_create_instr(after_label),
                             OPND_CREATE_INT32(true),
                             OPND_CREATE_INT32(fn_idx),
                             OPND_CREATE_INTPTR(0));

        /* (%reg, %other_reg, 1)-0xDEAD */
        before_label = INSTR_CREATE_label(dc);
        after_label = INSTR_CREATE_label(dc);
        arg = opnd_create_base_disp(reg, other_reg, 1, -0xDEAD, OPSZ_PTR);
        dr_insert_clean_call(dc, bb, where, (void*)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        dr_save_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        PRE(bb, where, INSTR_CREATE_mov_imm
            (dc, opnd_create_reg(reg), OPND_CREATE_INTPTR(0xDEAD)));
        PRE(bb, where, INSTR_CREATE_mov_imm
            (dc, opnd_create_reg(other_reg),
             OPND_CREATE_INTPTR(&hex_dead_global)));
        dr_insert_clean_call(dc, bb, where, (void*)func_ptrs[fn_idx], false, 1,
                             arg);
        dr_restore_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call(dc, bb, where, (void*)after_callee, false, 5,
                             opnd_create_instr(before_label),
                             opnd_create_instr(after_label),
                             OPND_CREATE_INT32(true),
                             OPND_CREATE_INT32(fn_idx),
                             OPND_CREATE_INTPTR(0));

        /* (%other_reg, %reg, 1)-0xDEAD */
        before_label = INSTR_CREATE_label(dc);
        after_label = INSTR_CREATE_label(dc);
        arg = opnd_create_base_disp(other_reg, reg, 1, -0xDEAD, OPSZ_PTR);
        dr_insert_clean_call(dc, bb, where, (void*)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        dr_save_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        PRE(bb, where, INSTR_CREATE_mov_imm
            (dc, opnd_create_reg(other_reg), OPND_CREATE_INTPTR(0xDEAD)));
        PRE(bb, where, INSTR_CREATE_mov_imm
            (dc, opnd_create_reg(reg),
             OPND_CREATE_INTPTR(&hex_dead_global)));
        dr_insert_clean_call(dc, bb, where, (void*)func_ptrs[fn_idx], false, 1,
                             arg);
        dr_restore_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call(dc, bb, where, (void*)after_callee, false, 5,
                             opnd_create_instr(before_label),
                             opnd_create_instr(after_label),
                             OPND_CREATE_INT32(true),
                             OPND_CREATE_INT32(fn_idx),
                             OPND_CREATE_INTPTR(0));
    }
}

/*****************************************************************************/
/* Instrumentation function code generation. */

/*
prologue:
    push REG_XBP
    mov REG_XBP, REG_XSP
*/
static void
codegen_prologue(void *dc, instrlist_t *ilist)
{
    APP(ilist, INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_XBP)));
    APP(ilist, INSTR_CREATE_mov_ld
        (dc, opnd_create_reg(DR_REG_XBP), opnd_create_reg(DR_REG_XSP)));
}

/*
epilogue:
    leave
    ret
*/
static void
codegen_epilogue(void *dc, instrlist_t *ilist)
{
    APP(ilist, INSTR_CREATE_leave(dc));
    APP(ilist, INSTR_CREATE_ret(dc));
}

/*
empty:
    ret
*/
static instrlist_t *
codegen_empty(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    APP(ilist, INSTR_CREATE_ret(dc));
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
#ifdef X64
# ifdef UNIX
    int reg = DR_REG_RDI;
# else /* WINDOWS */
    int reg = DR_REG_RCX;
# endif
    return opnd_create_reg(reg);
#else /* X86 */
# ifdef UNIX
    int arg_offset = 1;
# else /* WINDOWS */
    int arg_offset = 5;
# endif
    return OPND_CREATE_MEMPTR(DR_REG_XBP, arg_offset * sizeof(reg_t));
#endif
}

/*
inscount:
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XAX, ARG1
    add [global_count], REG_XAX
    leave
    ret
*/
static instrlist_t *
codegen_inscount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_add(dc, OPND_CREATE_ABSMEM(&global_count, OPSZ_PTR), xax));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
callpic_pop:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_label
    Lnext_label:
    pop REG_XBX
    leave
    ret
*/
static instrlist_t *
codegen_callpic_pop(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *next_label = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(next_label)));
    APP(ilist, next_label);
    APP(ilist, INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_XBX)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
callpic_mov:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_instr_mov
    Lnext_instr_mov:
    mov REG_XBX, [REG_XSP]
    leave
    ret
*/
static instrlist_t *
codegen_callpic_mov(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *next_label = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(next_label)));
    APP(ilist, next_label);
    APP(ilist, INSTR_CREATE_mov_ld
        (dc, opnd_create_reg(DR_REG_XBX), OPND_CREATE_MEMPTR(DR_REG_XSP, 0)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Non-leaf functions cannot be inlined.
nonleaf:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call other_func
    leave
    ret
other_func:
    ret
*/
static instrlist_t *
codegen_nonleaf(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *other_func = INSTR_CREATE_label(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(other_func)));
    codegen_epilogue(dc, ilist);
    APP(ilist, other_func);
    APP(ilist, INSTR_CREATE_ret(dc));
    return ilist;
}

/* Conditional branches cannot be inlined.  Avoid flags usage to make test case
 * more specific.
cond_br:
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XCX, ARG1
    jecxz Larg_zero
        mov REG_XAX, HEX(DEADBEEF)
        mov SYMREF(global_count), REG_XAX
    Larg_zero:
    leave
    ret
*/
static instrlist_t *
codegen_cond_br(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    instr_t *arg_zero = INSTR_CREATE_label(dc);
    opnd_t xcx = opnd_create_reg(DR_REG_XCX);
    codegen_prologue(dc, ilist);
    /* If arg1 is non-zero, write 0xDEADBEEF to global_count. */
    APP(ilist, INSTR_CREATE_mov_ld(dc, xcx, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_jecxz(dc, opnd_create_instr(arg_zero)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xcx, OPND_CREATE_INTPTR(&global_count)));
    APP(ilist, INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XCX, 0),
                                   OPND_CREATE_INT32((int)0xDEADBEEF)));
    APP(ilist, arg_zero);
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* A function that uses 2 registers and 1 local variable, which should fill all
 * of the scratch slots that the inliner uses.  This used to clobber the scratch
 * slots exposed to the client.
tls_clobber:
    push REG_XBP
    mov REG_XBP, REG_XSP
    sub REG_XSP, ARG_SZ
    mov REG_XAX, HEX(DEAD)
    mov REG_XDX, HEX(BEEF)
    mov [REG_XSP], REG_XAX
    leave
    ret
*/
static instrlist_t *
codegen_tls_clobber(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    opnd_t xdx = opnd_create_reg(DR_REG_XDX);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_sub
        (dc, opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT8(sizeof(reg_t))));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xax, OPND_CREATE_INT32(0xDEAD)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xdx, OPND_CREATE_INT32(0xBEEF)));
    APP(ilist, INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XSP, 0), xax));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Zero the aflags.  Inliner must ensure they are restored.
aflags_clobber:
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XAX, 0
    add al, HEX(7F)
    sahf
    leave
    ret
*/
static instrlist_t *
codegen_aflags_clobber(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_mov_imm
        (dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_INTPTR(0)));
    APP(ilist, INSTR_CREATE_add
        (dc, opnd_create_reg(DR_REG_AL), OPND_CREATE_INT8(0x7F)));
    APP(ilist, INSTR_CREATE_sahf(dc));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
bbcount:
    push REG_XBP
    mov REG_XBP, REG_XSP
    inc [global_count]
    leave
    ret
*/
static instrlist_t *
codegen_bbcount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    codegen_prologue(dc, ilist);
    APP(ilist, INSTR_CREATE_inc(dc, OPND_CREATE_ABSMEM(&global_count, OPSZ_PTR)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/* Reduced code from inscount generated by gcc47 -O0.
gcc47_inscount:
#ifdef X64
    push   %rbp
    mov    %rsp,%rbp
    mov    %rdi,-0x8(%rbp)
    mov    global_count(%rip),%rdx
    mov    -0x8(%rbp),%rax
    add    %rdx,%rax
    mov    %rax,global_count(%rip)
    pop    %rbp
    retq
#else
    push   %ebp
    mov    %esp,%ebp
    call   pic_thunk
    add    $0x1c86,%ecx
    mov    global_count(%ecx),%edx
    mov    0x8(%ebp),%eax
    add    %edx,%eax
    mov    %eax,global_count(%ecx)
    pop    %ebp
    ret
pic_thunk:
    mov    (%esp),%ecx
    ret
#endif
*/
static instrlist_t *
codegen_gcc47_inscount(void *dc)
{
    instrlist_t *ilist = instrlist_create(dc);
    opnd_t global;
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    opnd_t xdx = opnd_create_reg(DR_REG_XDX);
#ifdef X64
    /* This local is past TOS.  That's OK by the sysv x64 ABI. */
    opnd_t local = OPND_CREATE_MEMPTR(DR_REG_XBP, -(int)sizeof(reg_t));
    codegen_prologue(dc, ilist);
    global = opnd_create_rel_addr(&global_count, OPSZ_PTR);
    APP(ilist, INSTR_CREATE_mov_st(dc, local, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xdx, global));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, local));
    APP(ilist, INSTR_CREATE_add(dc, xax, xdx));
    APP(ilist, INSTR_CREATE_mov_st(dc, global, xax));
    codegen_epilogue(dc, ilist);
#else
    instr_t *pic_thunk = INSTR_CREATE_mov_ld
        (dc, opnd_create_reg(DR_REG_XCX), OPND_CREATE_MEMPTR(DR_REG_XSP, 0));
    codegen_prologue(dc, ilist);
    /* XXX: Do a real 32-bit PIC-style access.  For now we just use an absolute
     * reference since we're 32-bit and everything is reachable.
     */
    global = opnd_create_abs_addr(&global_count, OPSZ_PTR);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(pic_thunk)));
    APP(ilist, INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_XCX),
                                OPND_CREATE_INT32(0x0)));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xdx, global));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_add(dc, xax, xdx));
    APP(ilist, INSTR_CREATE_mov_st(dc, global, xax));
    codegen_epilogue(dc, ilist);

    APP(ilist, pic_thunk);
    APP(ilist, INSTR_CREATE_ret(dc));
#endif
    return ilist;
}

/* We want to test that we can auto-inline whatever the compiler generates for
 * inscount.
 */
static void
compiler_inscount(ptr_uint_t count) {
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
    APP(ilist, INSTR_CREATE_ret(dc));
    return ilist;
}
