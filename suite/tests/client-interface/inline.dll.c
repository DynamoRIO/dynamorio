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
#include "client_tools.h"

/* List of instrumentation functions. */
#ifdef X86
#    define FUNCTIONS()             \
        FUNCTION(empty)             \
        FUNCTION(empty_1arg)        \
        FUNCTION(inscount)          \
        FUNCTION(compiler_inscount) \
        FUNCTION(gcc47_inscount)    \
        FUNCTION(callpic_pop)       \
        FUNCTION(callpic_mov)       \
        FUNCTION(nonleaf)           \
        FUNCTION(cond_br)           \
        FUNCTION(tls_clobber)       \
        FUNCTION(aflags_clobber)    \
        FUNCTION(bbcount)           \
        LAST_FUNCTION()
#elif defined(AARCH64)
#    define FUNCTIONS()             \
        FUNCTION(empty)             \
        FUNCTION(empty_1arg)        \
        FUNCTION(inscount)          \
        FUNCTION(compiler_inscount) \
        FUNCTION(aflags_clobber)    \
        FUNCTION(bbcount)           \
        LAST_FUNCTION()
#endif

#define TEST_INLINE 1
static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating);
static void
compiler_inscount(ptr_uint_t count);
#include "cleancall-opt-shared.h"

static void
test_inlined_call_args(void *dc, instrlist_t *bb, instr_t *where, int fn_idx);

static void
fill_scratch(void)
{
    void *dc = dr_get_current_drcontext();
    int slot;
    /* Set slots to 0x000... 0x111... 0x222... etc. */
    for (slot = SPILL_SLOT_1; slot <= SPILL_SLOT_MAX; slot++) {
        reg_t value = (reg_t)slot * 0x11111111;
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
        reg_t expected = (reg_t)slot * 0x11111111;
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

#ifdef X86
static instr_t *
test_aflags(void *dc, instrlist_t *bb, instr_t *where, int aflags, instr_t *before_label,
            instr_t *after_label)
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
    PRE(bb, where,
        INSTR_CREATE_mov_st(dc, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_1), xax));
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
    PRE(bb, where,
        INSTR_CREATE_mov_st(dc, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_2), xax));

    /* Assert that they match the original flags. */
    dr_insert_clean_call(dc, bb, where, (void *)check_aflags, false, 2,
                         dr_reg_spill_slot_opnd(dc, SPILL_SLOT_2),
                         OPND_CREATE_INT32(aflags));

    /* Restore and flags. */
    PRE(bb, where,
        INSTR_CREATE_mov_ld(dc, xax, dr_reg_spill_slot_opnd(dc, SPILL_SLOT_1)));
    PRE(bb, where, INSTR_CREATE_popf(dc));
    return where;
}
#endif

static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
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
    dr_insert_clean_call(dc, bb, entry, (void *)before_callee, false, 2,
                         OPND_CREATE_INTPTR(func_ptrs[i]),
                         OPND_CREATE_INTPTR(func_names[i]));

    before_label = INSTR_CREATE_label(dc);
    after_label = INSTR_CREATE_label(dc);

    switch (i) {
    default:
        /* Default behavior is to call instrumentation with no-args and
         * assert it gets inlined.
         */
        /* FIXME i#1569: passing instruction operands is NYI on AArch64.
         * We use a workaround involving ADR. */
        IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_start_pc, before_label));
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_end_pc, after_label));
        break;
    case FN_empty_1arg:
    case FN_inscount:
#ifdef X86
    case FN_compiler_inscount:
    case FN_gcc47_inscount:
#endif
        /* FIXME i#1569: passing instruction operands is NYI on AArch64.
         * We use a workaround involving ADR. */
        IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_start_pc, before_label));
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 1,
                             OPND_CREATE_INT32(0xDEAD));
        PRE(bb, entry, after_label);
        IF_AARCH64(save_current_pc(dc, bb, entry, &cleancall_end_pc, after_label));
        break;
#ifdef X86
    case FN_nonleaf:
    case FN_cond_br:
        /* These functions cannot be inlined (yet). */
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        inline_expected = false;
        break;
    case FN_tls_clobber:
        dr_insert_clean_call(dc, bb, entry, (void *)fill_scratch, false, 0);
        PRE(bb, entry, before_label);
        dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
        PRE(bb, entry, after_label);
        dr_insert_clean_call(dc, bb, entry, (void *)check_scratch, false, 0);
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
#endif
    }

    dr_insert_clean_call_ex(dc, bb, entry, (void *)after_callee,
                            DR_CLEANCALL_READS_APP_CONTEXT, IF_X86_ELSE(6, 4),
#ifdef X86
                            opnd_create_instr(before_label),
                            opnd_create_instr(after_label),
#endif
                            OPND_CREATE_INT32(inline_expected), OPND_CREATE_INT32(false),
                            OPND_CREATE_INT32(i), OPND_CREATE_INTPTR(func_names[i]));

#ifdef X86
    if (i == FN_inscount || i == FN_empty_1arg) {
        test_inlined_call_args(dc, bb, entry, i);
    }
#endif

    return DR_EMIT_DEFAULT;
}

#ifdef X86
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

    for (i = 0; i < DR_NUM_GPR_REGS; i++) {
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
        dr_insert_clean_call(dc, bb, where, (void *)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, INSTR_CREATE_mov_imm(dc, arg, OPND_CREATE_INTPTR(0xDEAD)));
        dr_insert_clean_call(dc, bb, where, (void *)func_ptrs[fn_idx], false, 1, arg);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call_ex(
            dc, bb, where, (void *)after_callee, DR_CLEANCALL_READS_APP_CONTEXT, 6,
            opnd_create_instr(before_label), opnd_create_instr(after_label),
            OPND_CREATE_INT32(true), OPND_CREATE_INT32(false), OPND_CREATE_INT32(fn_idx),
            OPND_CREATE_INTPTR(0));

        /* (%reg, %other_reg, 1)-0xDEAD */
        before_label = INSTR_CREATE_label(dc);
        after_label = INSTR_CREATE_label(dc);
        arg = opnd_create_base_disp(reg, other_reg, 1, -0xDEAD, OPSZ_PTR);
        dr_insert_clean_call(dc, bb, where, (void *)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        dr_save_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        PRE(bb, where,
            INSTR_CREATE_mov_imm(dc, opnd_create_reg(reg), OPND_CREATE_INTPTR(0xDEAD)));
        PRE(bb, where,
            INSTR_CREATE_mov_imm(dc, opnd_create_reg(other_reg),
                                 OPND_CREATE_INTPTR(&hex_dead_global)));
        dr_insert_clean_call(dc, bb, where, (void *)func_ptrs[fn_idx], false, 1, arg);
        dr_restore_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call_ex(
            dc, bb, where, (void *)after_callee, DR_CLEANCALL_READS_APP_CONTEXT, 6,
            opnd_create_instr(before_label), opnd_create_instr(after_label),
            OPND_CREATE_INT32(true), OPND_CREATE_INT32(false), OPND_CREATE_INT32(fn_idx),
            OPND_CREATE_INTPTR(0));

        /* (%other_reg, %reg, 1)-0xDEAD */
        before_label = INSTR_CREATE_label(dc);
        after_label = INSTR_CREATE_label(dc);
        arg = opnd_create_base_disp(other_reg, reg, 1, -0xDEAD, OPSZ_PTR);
        dr_insert_clean_call(dc, bb, where, (void *)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[fn_idx]),
                             OPND_CREATE_INTPTR(0));
        PRE(bb, where, before_label);
        dr_save_reg(dc, bb, where, reg, SPILL_SLOT_1);
        dr_save_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        PRE(bb, where,
            INSTR_CREATE_mov_imm(dc, opnd_create_reg(other_reg),
                                 OPND_CREATE_INTPTR(0xDEAD)));
        PRE(bb, where,
            INSTR_CREATE_mov_imm(dc, opnd_create_reg(reg),
                                 OPND_CREATE_INTPTR(&hex_dead_global)));
        dr_insert_clean_call(dc, bb, where, (void *)func_ptrs[fn_idx], false, 1, arg);
        dr_restore_reg(dc, bb, where, other_reg, SPILL_SLOT_2);
        dr_restore_reg(dc, bb, where, reg, SPILL_SLOT_1);
        PRE(bb, where, after_label);
        dr_insert_clean_call_ex(
            dc, bb, where, (void *)after_callee, DR_CLEANCALL_READS_APP_CONTEXT, 6,
            opnd_create_instr(before_label), opnd_create_instr(after_label),
            OPND_CREATE_INT32(true), OPND_CREATE_INT32(false), OPND_CREATE_INT32(fn_idx),
            OPND_CREATE_INTPTR(0));
    }
}
#endif
/*****************************************************************************/
/* Instrumentation function code generation. */

/*****************************************************************************/
/* Instrumentation function code generation. */

#ifdef X86
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
    APP(ilist,
        INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_XBX),
                            OPND_CREATE_MEMPTR(DR_REG_XSP, 0)));
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
    APP(ilist,
        INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XCX, 0),
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
    APP(ilist,
        INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_XSP),
                         OPND_CREATE_INT8(sizeof(reg_t))));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xax, OPND_CREATE_INT32(0xDEAD)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xdx, OPND_CREATE_INT32(0xBEEF)));
    APP(ilist, INSTR_CREATE_mov_st(dc, OPND_CREATE_MEMPTR(DR_REG_XSP, 0), xax));
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
#    ifdef X64
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
#    else
    instr_t *pic_thunk = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_XCX),
                                             OPND_CREATE_MEMPTR(DR_REG_XSP, 0));
    codegen_prologue(dc, ilist);
    /* XXX: Do a real 32-bit PIC-style access.  For now we just use an absolute
     * reference since we're 32-bit and everything is reachable.
     */
    global = opnd_create_abs_addr(&global_count, OPSZ_PTR);
    APP(ilist, INSTR_CREATE_call(dc, opnd_create_instr(pic_thunk)));
    APP(ilist, INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_XCX), OPND_CREATE_INT32(0x0)));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xdx, global));
    APP(ilist, INSTR_CREATE_mov_ld(dc, xax, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_add(dc, xax, xdx));
    APP(ilist, INSTR_CREATE_mov_st(dc, global, xax));
    codegen_epilogue(dc, ilist);

    APP(ilist, pic_thunk);
    APP(ilist, INSTR_CREATE_ret(dc));
#    endif
    return ilist;
}
#endif
