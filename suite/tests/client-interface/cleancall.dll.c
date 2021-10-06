/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include "string.h"

#define MINSERT instrlist_meta_preinsert

#ifdef X64
/* we use this for clean call base-disp refs */
static reg_t buf[] = { 0xcafebabe, 0xfeedadad, 0xeeeeeeee, 0xbadcabee };
#endif

#ifdef X86
/* buffers for testing reg_set_value_ex */
byte orig_reg_val_buf[64];
byte new_reg_val_buf[64];

static void
print_error_on_fail(bool check)
{
    if (!check)
        dr_fprintf(STDERR, "error\n");
}

static void
set_gpr()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_XAX, &mcontext, orig_reg_val_buf);
    reg_get_value_ex(DR_REG_XAX, &mcontext, new_reg_val_buf);
    new_reg_val_buf[0] = 0x75;
    new_reg_val_buf[2] = 0x83;
    new_reg_val_buf[3] = 0x23;
    bool succ = reg_set_value_ex(DR_REG_XAX, &mcontext, new_reg_val_buf);
    print_error_on_fail(succ);
    memset(new_reg_val_buf, 0, 64);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
check_gpr()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_XAX, &mcontext, new_reg_val_buf);
    print_error_on_fail(new_reg_val_buf[0] == 0x75);
    print_error_on_fail(new_reg_val_buf[2] == 0x83);
    print_error_on_fail(new_reg_val_buf[3] == 0x23);
    bool succ = reg_set_value_ex(DR_REG_XAX, &mcontext, orig_reg_val_buf);
    print_error_on_fail(succ);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
set_xmm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_XMM0, &mcontext, orig_reg_val_buf);
    reg_get_value_ex(DR_REG_XMM0, &mcontext, new_reg_val_buf);
    new_reg_val_buf[0] = 0x77;
    new_reg_val_buf[2] = 0x89;
    new_reg_val_buf[14] = 0x21;
    bool succ = reg_set_value_ex(DR_REG_XMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(succ);
    memset(new_reg_val_buf, 0, 64);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
check_xmm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_XMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(new_reg_val_buf[0] == 0x77);
    print_error_on_fail(new_reg_val_buf[2] == 0x89);
    print_error_on_fail(new_reg_val_buf[14] == 0x21);
    bool succ = reg_set_value_ex(DR_REG_XMM0, &mcontext, orig_reg_val_buf);
    print_error_on_fail(succ);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
set_ymm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_YMM0, &mcontext, orig_reg_val_buf);
    reg_get_value_ex(DR_REG_YMM0, &mcontext, new_reg_val_buf);
    new_reg_val_buf[0] = 0x77;
    new_reg_val_buf[2] = 0x80;
    new_reg_val_buf[14] = 0x25;
    new_reg_val_buf[20] = 0x09;
    new_reg_val_buf[25] = 0x06;
    bool succ = reg_set_value_ex(DR_REG_YMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(succ);
    memset(new_reg_val_buf, 0, 64);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
check_ymm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_YMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(new_reg_val_buf[0] == 0x77);
    print_error_on_fail(new_reg_val_buf[2] == 0x80);
    print_error_on_fail(new_reg_val_buf[14] == 0x25);
    print_error_on_fail(new_reg_val_buf[20] == 0x09);
    print_error_on_fail(new_reg_val_buf[25] == 0x06);
    bool succ = reg_set_value_ex(DR_REG_YMM0, &mcontext, orig_reg_val_buf);
    print_error_on_fail(succ);
    dr_set_mcontext(drcontext, &mcontext);
}

#    ifdef __AVX512F__
static void
set_zmm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_ZMM0, &mcontext, orig_reg_val_buf);
    reg_get_value_ex(DR_REG_ZMM0, &mcontext, new_reg_val_buf);
    new_reg_val_buf[0] = 0x77;
    new_reg_val_buf[2] = 0x80;
    new_reg_val_buf[14] = 0x25;
    new_reg_val_buf[20] = 0x09;
    new_reg_val_buf[25] = 0x02;
    new_reg_val_buf[32] = 0x16;
    new_reg_val_buf[55] = 0x18;
    new_reg_val_buf[60] = 0x22;
    bool succ = reg_set_value_ex(DR_REG_ZMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(succ);
    memset(new_reg_val_buf, 0, 64);
    dr_set_mcontext(drcontext, &mcontext);
}

static void
check_zmm()
{
    check_stack_alignment();
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);
    reg_get_value_ex(DR_REG_ZMM0, &mcontext, new_reg_val_buf);
    print_error_on_fail(new_reg_val_buf[0] == 0x77);
    print_error_on_fail(new_reg_val_buf[2] == 0x80);
    print_error_on_fail(new_reg_val_buf[14] == 0x25);
    print_error_on_fail(new_reg_val_buf[20] == 0x09);
    print_error_on_fail(new_reg_val_buf[25] == 0x02);
    print_error_on_fail(new_reg_val_buf[32] == 0x16);
    print_error_on_fail(new_reg_val_buf[55] == 0x18);
    print_error_on_fail(new_reg_val_buf[60] == 0x22);
    bool succ = reg_set_value_ex(DR_REG_ZMM0, &mcontext, orig_reg_val_buf);
    print_error_on_fail(succ);
    dr_set_mcontext(drcontext, &mcontext);
}
#    endif

#endif

static void
ind_call(reg_t a1, reg_t a2)
{
    dr_fprintf(STDERR, "bar " PFX " " PFX "\n", a1, a2);
}

static void (*ind_call_ptr)(reg_t a1, reg_t a2) = ind_call;

static void
foo(reg_t a1, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6, reg_t a7, reg_t a8)
{
    check_stack_alignment();
    dr_fprintf(
        STDERR,
        "foo " PFX " " PFX " " PFX " " PFX "\n    " PFX " " PFX " " PFX " " PFX "\n", a1,
        /* printing addr of buf would be non-deterministic */
        IF_X64_ELSE((a2 == (reg_t)buf) ? 1 : -1, a2), a3, a4, a5, a6, a7, a8);
}

static void
bar(reg_t a1, reg_t a2)
{
    check_stack_alignment();
    /* test indirect call handling in clean call analysis */
    ind_call_ptr(a1, a2);
}

static void
save_test()
{
    check_stack_alignment();
    int i;
    void *drcontext = dr_get_current_drcontext();
    dr_fprintf(STDERR, "verifying values\n");
    if (*(((reg_t *)dr_get_tls_field(drcontext)) + 2) != 1) {
        dr_fprintf(STDERR, "Write to client tls from cache failed, got %d, expected %d\n",
                   *(((reg_t *)dr_get_tls_field(drcontext)) + 2), 1);
    }
    for (i = SPILL_SLOT_1; i <= SPILL_SLOT_MAX; i++) {
        reg_t value = dr_read_saved_reg(drcontext, i);
        if (value != i + 1 - SPILL_SLOT_1) {
            dr_fprintf(STDERR, "slot %d value %d doesn't match expected value %d\n", i,
                       value, i + 1 - SPILL_SLOT_1);
        }
        if (i % 2 == 0) {
            /* set every other reg */
            value = 100 - i;
            dr_write_saved_reg(drcontext, i, value);
        }
    }
}

static int post_crash = 0;
static void *tag_of_interest;

static void
restore_state_event(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                    bool restore_memory, bool app_code_consistent)
{
    if (tag == tag_of_interest) {
        dr_fprintf(STDERR, "in restore_state for our clean call crash %d\n", post_crash);
        /* flush, so we can use different instrumentation next time */
        dr_delay_flush_region(dr_fragment_app_pc(tag), 1, 0, NULL);
    }
}

static void
cleancall_aflags_save(void)
{
    dr_fprintf(STDERR, "cleancall_aflags_save\n");
}

static void
cleancall_no_aflags_save(void)
{
    dr_fprintf(STDERR, "cleancall_no_aflags_save\n");
}

static bool first_bb = true;
static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr, *next_next_instr;
    bool modified = false;

#define PRE(bb, i) instrlist_preinsert(bb, instr, INSTR_XL8(i, dr_fragment_app_pc(tag)))
#define PREM(bb, i) \
    instrlist_meta_preinsert(bb, instr, INSTR_XL8(i, dr_fragment_app_pc(tag)))

    if (first_bb) {
        instr_t *add, *cmp;
        /* test cleancall with/without aflags save
         *   cleancall_aflags_save
         *   cmp # fake cmp app instr
         *   cleancall_no_aflags_save
         *   add # fake add app instr
         */
        first_bb = false;
        instr = instrlist_first(bb);
        cmp = INSTR_CREATE_cmp(drcontext, opnd_create_reg(DR_REG_XAX),
                               opnd_create_reg(DR_REG_XAX));
        PRE(bb, cmp);
        add = INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_XAX),
                               OPND_CREATE_INT32(0));
        PRE(bb, add);
        dr_insert_clean_call(drcontext, bb, add, (void *)cleancall_no_aflags_save, false,
                             0);
        dr_insert_clean_call(drcontext, bb, cmp, (void *)cleancall_aflags_save, false, 0);

#ifdef X86
        /* Other unrelated tests for setting register values. */

        dr_insert_clean_call_ex(drcontext, bb, instr, set_gpr,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);
        dr_insert_clean_call_ex(drcontext, bb, instr, check_gpr,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);

        dr_insert_clean_call_ex(drcontext, bb, instr, set_xmm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);
        dr_insert_clean_call_ex(drcontext, bb, instr, check_xmm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);

        dr_insert_clean_call_ex(drcontext, bb, instr, set_ymm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);
        dr_insert_clean_call_ex(drcontext, bb, instr, check_ymm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);

#    ifdef __AVX512F__
        dr_insert_clean_call_ex(drcontext, bb, instr, set_zmm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);
        dr_insert_clean_call_ex(drcontext, bb, instr, check_zmm,
                                DR_CLEANCALL_READS_APP_CONTEXT, 0);
#    endif
#endif
    }

    /* Look for 3 nops to indicate handler is set up */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;

        if (instr_is_nop(instr) && next_instr != NULL && instr_is_nop(next_instr) &&
            next_next_instr != NULL && instr_is_call_direct(next_next_instr)) {

            ASSERT(tag_of_interest == NULL || tag_of_interest == tag);
            tag_of_interest = tag;
            modified = true;

            /* # of crashes is tied to # of setjmps in cleancall.c */

            if (post_crash == 0) {
                /* Test crash in 1st clean call arg */
                dr_fprintf(STDERR, "inserting clean call crash code 1\n");
                dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void *)foo,
                                     false /* don't save fp state */, 1,
                                     OPND_CREATE_ABSMEM(NULL, OPSZ_4));
                post_crash++;
            } else if (post_crash == 1) {
                /* Test crash in 2nd clean call arg */
                dr_fprintf(STDERR, "inserting clean call crash code 2\n");
                dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void *)foo,
                                     false /* don't save fp state */, 2,
                                     OPND_CREATE_INT32(0),
                                     OPND_CREATE_ABSMEM(NULL, OPSZ_4));
                post_crash++;
            } else if (post_crash == 2) {
                /* PR 307242: test xsp args */
                reg_id_t scratch;
#ifdef X64
#    ifdef WINDOWS
                scratch = REG_XCX;
#    else
                scratch = REG_XDI;
#    endif
#else
                scratch = REG_XAX;
#endif

                dr_fprintf(STDERR, "inserting xsp arg testing\n");
                /* See notes below: we crash after, so can clobber regs */
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(scratch),
                                         OPND_CREATE_INT32(sizeof(reg_t))));
                PRE(bb,
                    INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((int)0xbcbcaba0)));
                PRE(bb,
                    INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((int)0xbcbcaba1)));
                dr_insert_clean_call(
                    drcontext, bb, instr, (void *)bar, false /* don't save fp state */, 2,
                    OPND_CREATE_MEM32(REG_XSP, 0),
                    /* test conflicting w/ scratch reg */
                    opnd_create_base_disp(REG_XSP, scratch, 1, 0, OPSZ_PTR));
                /* Even though we'll be doing a longjmp, building w/ VS2010 results
                 * in silent failure on handling the exception so we restore xsp.
                 */
                PRE(bb,
                    INSTR_CREATE_lea(
                        drcontext, opnd_create_reg(REG_XSP),
                        OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, 2 * sizeof(reg_t))));
                PRE(bb,
                    INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(REG_XAX),
                                        OPND_CREATE_ABSMEM(NULL, OPSZ_PTR)));
                post_crash++;
            } else if (post_crash == 3) {
#ifdef X86_64
                /* For x64, test using calling convention regs as params.
                 * We do different things depending on order, whether a
                 * memory reference, etc.
                 * To test our values, we clobber app registers.  The app
                 * has a setjmp set up, so we crash after for a deterministic
                 * result.
                 */
                dr_fprintf(STDERR, "inserting clean call arg testing\n");
                /* We do not translate the regs back */

                /* We arrange to have our base-disps all be small offsets off buf */
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_RDX),
                                         OPND_CREATE_INT32(sizeof(reg_t))));
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_RCX),
                                         OPND_CREATE_INTPTR(buf)));
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_R8),
                                         OPND_CREATE_INT32(-42)));
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_R9),
                                         OPND_CREATE_INT32((int)0xdeadbeef)));
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_RAX),
                                         OPND_CREATE_INT32(2 * sizeof(reg_t))));
                PRE(bb,
                    INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_RBP),
                                         OPND_CREATE_INT32(3 * sizeof(reg_t))));
                dr_insert_clean_call(
                    drcontext, bb, instr, (void *)foo, false /* don't save fp state */, 8,
                    /* Pick registers used by both Windows and Linux */
                    opnd_create_reg(REG_RDX), opnd_create_reg(REG_RCX),
                    opnd_create_reg(REG_R9), opnd_create_reg(REG_R8),
                    OPND_CREATE_MEM32(REG_RCX, 0),
                    /* test having only index register conflict */
                    opnd_create_base_disp(REG_RBP, REG_RCX, 1, 0, OPSZ_PTR),
                    /* test OPSZ_4, and using register modified
                     * by clean call setup (rax) */
                    opnd_create_base_disp(REG_RAX, REG_RCX, 1, 0, OPSZ_4),
                    /* test having both base and index conflict */
                    opnd_create_base_disp(REG_RDX, REG_RCX, 1, 0, OPSZ_PTR));
#endif
                PRE(bb,
                    INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(REG_XAX),
                                        OPND_CREATE_ABSMEM(NULL, OPSZ_PTR)));
                post_crash++;
            } else {
                /* Test register saving and restoring and access to saved registers
                 * from outside the cache. */
                int i;
                instr_t *fault = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(REG_XAX),
                                                     OPND_CREATE_ABSMEM(NULL, OPSZ_PTR));
                instr_t *post_fault = INSTR_CREATE_label(drcontext);
                dr_fprintf(STDERR, "inserting saved reg access testing\n");

                /* we want to test all the slots so juggle around to save xax and flags
                 * to client's tls. */
                dr_save_reg(drcontext, bb, instr, REG_XBX, SPILL_SLOT_1);
                dr_insert_read_tls_field(drcontext, bb, instr, REG_XBX);
                PREM(bb,
                     INSTR_CREATE_mov_st(drcontext,
                                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                                               0 * sizeof(reg_t),
                                                               OPSZ_PTR),
                                         opnd_create_reg(REG_XAX)));
                dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_2);
                PREM(bb,
                     INSTR_CREATE_mov_st(drcontext,
                                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                                               1 * sizeof(reg_t),
                                                               OPSZ_PTR),
                                         opnd_create_reg(REG_XAX)));
                PREM(bb,
                     INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(REG_XAX),
                                          OPND_CREATE_INT32(1)));
                /* test tls writing */
                PREM(bb,
                     INSTR_CREATE_mov_st(drcontext,
                                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                                               2 * sizeof(reg_t),
                                                               OPSZ_PTR),
                                         opnd_create_reg(REG_XAX)));
                dr_restore_reg(drcontext, bb, instr, REG_XBX, SPILL_SLOT_1);

                /* now test the slots */
                /* xax is our tls + 0, flags is our tls + sizeof(reg_t) */
                for (i = SPILL_SLOT_1; i <= SPILL_SLOT_MAX; i++) {
                    dr_save_reg(drcontext, bb, instr, REG_XAX, i);
                    PREM(bb, INSTR_CREATE_inc(drcontext, opnd_create_reg(REG_XAX)));
                }
                dr_insert_clean_call(drcontext, bb, instr, (void *)save_test,
                                     true /* try saving the fp state */, 0);
                for (i = SPILL_SLOT_1; i <= SPILL_SLOT_MAX; i++) {
                    /* test using opnd */
                    if (i < dr_max_opnd_accessible_spill_slot()) {
                        PREM(bb,
                             INSTR_CREATE_cmp(
                                 drcontext, dr_reg_spill_slot_opnd(drcontext, i),
                                 (i % 2 == 0) ? OPND_CREATE_INT8(100 - i)
                                              : OPND_CREATE_INT8(i + 1 - SPILL_SLOT_1)));
                        PREM(bb,
                             INSTR_CREATE_jcc(drcontext, OP_jne,
                                              opnd_create_instr(fault)));
                    }
                    /* test using restore routine */
                    dr_restore_reg(drcontext, bb, instr, REG_XAX, i);
                    PREM(bb,
                         INSTR_CREATE_cmp(drcontext, opnd_create_reg(REG_XAX),
                                          (i % 2 == 0)
                                              ? OPND_CREATE_INT8(100 - i)
                                              : OPND_CREATE_INT8(i + 1 - SPILL_SLOT_1)));
                    PREM(bb,
                         INSTR_CREATE_jcc(drcontext, OP_jne, opnd_create_instr(fault)));
                }
                PREM(bb,
                     INSTR_CREATE_jmp_short(drcontext, opnd_create_instr(post_fault)));
                PRE(bb, fault); /* pre not prem since we want this to be an app fault */
                PREM(bb, post_fault);

                /* Now juggle xax and flags back from client tls */
                dr_save_reg(drcontext, bb, instr, REG_XBX, SPILL_SLOT_1);
                dr_insert_read_tls_field(drcontext, bb, instr, REG_XBX);
                PREM(bb,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(REG_XAX),
                                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                                               1 * sizeof(reg_t),
                                                               OPSZ_PTR)));
                dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_MAX);
                PREM(bb,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(REG_XAX),
                                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                                               0 * sizeof(reg_t),
                                                               OPSZ_PTR)));
                dr_restore_reg(drcontext, bb, instr, REG_XBX, SPILL_SLOT_1);

#if VERBOSE /* for debugging */
                instrlist_disassemble(drcontext, tag, bb, dr_get_stdout_file());
#endif
                post_crash++; /* note we don't actually crash so this must be the
                               * last test */
            }
        }
    }

    if (modified) /* store since not constant instrumentation */
        return DR_EMIT_STORE_TRANSLATIONS;
    else
        return DR_EMIT_DEFAULT;
}

static void
thread_exit(void *drcontext)
{
    dr_thread_free(drcontext, dr_get_tls_field(drcontext), 3 * sizeof(reg_t));
}

static void
thread_start(void *drcontext)
{
    dr_set_tls_field(drcontext, dr_thread_alloc(drcontext, 3 * sizeof(reg_t)));
}

static void
app_exit_event(void)
{
    check_stack_alignment();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
    dr_register_thread_init_event(thread_start);
    dr_register_thread_exit_event(thread_exit);
    dr_register_restore_state_event(restore_state_event);
    dr_register_exit_event(app_exit_event);
}
