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
        FUNCTION(inscount) \
        FUNCTION(callpic_pop) \
        FUNCTION(callpic_mov) \
        FUNCTION(nonleaf) \
        FUNCTION(cond_br) \
        FUNCTION(tls_clobber) \
        FUNCTION(aflags_clobber) \
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
}

/* Free the instrumentation machine code. */
static void
free_instrumentation_funcs(void)
{
    dr_nonheap_free(rwx_mem, rwx_size);
}

/* Globals used by instrumentation functions. */
ptr_uint_t count;
static uint callee_inlined;

static dr_mcontext_t before_mcontext = {sizeof(before_mcontext),DR_MC_ALL,};
static int before_errno;
static dr_mcontext_t after_mcontext = {sizeof(after_mcontext),DR_MC_ALL,};
static int after_errno;

static void
after_inscount(void)
{
    DR_ASSERT(count == 0xDEAD);
}

static void
after_callpic(void)
{
    DR_ASSERT(count == 1);
}

/* Reset count and patch the out-of-line version of the instrumentation function
 * so we can find out if it got called, which would mean it wasn't inlined.
 *
 * XXX: We are using self-modifying code on the callee!  If DR tries to
 * disassemble the callee's ilist after the modification, it will trigger
 * assertion failures in the disassembler.
 */
static void
before_callee(app_pc func, const char *func_name)
{
    void *dc;
    instrlist_t *ilist;
    opnd_t xax = opnd_create_reg(DR_REG_XAX);
    byte *end_pc;

    dr_fprintf(STDERR, "Calling func %s...\n", func_name);

    /* Save mcontext before call. */
    dc = dr_get_current_drcontext();
    dr_get_mcontext(dc, &before_mcontext);

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
           func_name, func);

    /* Check there was enough room in the function.  We align every callee
     * entry point to CALLEE_ALIGNMENT, so each function has at least
     * CALLEE_ALIGNMENT bytes long.
     */
    DR_ASSERT_MSG(end_pc < func + CALLEE_ALIGNMENT,
                  "Patched code too big for smallest function!");

    /* Reset instrumentation globals. */
    count = 0;
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

static void
after_callee(bool inline_expected, const char *func_name)
{
    int i;
    void *dc;

#if defined(WINDOWS) && !defined(X64)
    int xmm_uninit;
    int num_uninit;
#endif

    /* Save mcontext after call. */
    dc = dr_get_current_drcontext();
    dr_get_mcontext(dc, &after_mcontext);

#if defined(WINDOWS) && !defined(X64)
    /* For a 32-bit build on a 32-bit Windows kernel, no xmm registers are
     * saved, and the array is left uninitialized.  On WOW64 xmm6-7 are not
     * saved.  To avoid spurious failures, we zero out the uninit slots.
     * On pure X64, there are only 6 slots, so none are uninitialized.
     */
    xmm_uninit = dr_is_wow64() ? 6 : 0;
    num_uninit = NUM_XMM_SLOTS - xmm_uninit;
    memset(&before_mcontext.ymm[xmm_uninit], 0, num_uninit * sizeof(dr_ymm_t));
    memset(&after_mcontext.ymm[xmm_uninit], 0, num_uninit * sizeof(dr_ymm_t));
#endif
    if (!proc_has_feature(FEATURE_AVX)) {
        /* top bits of ymm slots will be uninitialized */
        for (i = 0; i < NUM_XMM_SLOTS; i++) {
            memset(&before_mcontext.ymm[i].u32[4], 0, sizeof(dr_xmm_t));
            memset(&after_mcontext.ymm[i].u32[4], 0, sizeof(dr_xmm_t));
        }
    }

    /* Compare mcontexts. */
    if (before_errno != after_errno) {
        dr_fprintf(STDERR, "errnos differ!\nbefore: %d, after: %d\n",
                   before_errno, after_errno);
    }
    if (memcmp(&before_mcontext, &after_mcontext, sizeof(dr_mcontext_t))) {
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
        for (i = 0; i < NUM_XMM_SLOTS; i++) {
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

    if (inline_expected) {
        DR_ASSERT_MSG(callee_inlined, "Function was not inlined!");
    } else {
        DR_ASSERT_MSG(!callee_inlined, "Function was inlined unexpectedly!");
    }

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
        DR_ASSERT_MSG(value == expected,
                      "Client scratch slot clobbered by clean call!");
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
test_aflags(void *dc, instrlist_t *bb, instr_t *where, int aflags)
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

    dr_insert_clean_call(dc, bb, where, func_ptrs[FN_aflags_clobber], false, 0);

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

    for (i = 0; i < N_FUNCS; i++) {
        bool inline_expected = true;

        if (entry_pc != func_app_pcs[i])
            continue;

        func_called[i] = 1;
        dr_insert_clean_call(dc, bb, entry, (void*)before_callee, false, 2,
                             OPND_CREATE_INTPTR(func_ptrs[i]),
                             OPND_CREATE_INTPTR(func_names[i]));

        switch (i) {
        default:
            /* Default behavior is to call instrumentation with no-args and
             * assert it gets inlined. */
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            break;
        case FN_inscount:
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 1,
                                 OPND_CREATE_INT32(0xDEAD));
            dr_insert_clean_call(dc, bb, entry, (void*)after_inscount, false,
                                 0);
            break;
        case FN_nonleaf:
        case FN_cond_br:
            /* These functions cannot be inlined (yet). */
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            inline_expected = false;
            break;
        case FN_tls_clobber:
            dr_insert_clean_call(dc, bb, entry, (void*)fill_scratch, false, 0);
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            dr_insert_clean_call(dc, bb, entry, (void*)check_scratch, false, 0);
            break;
        case FN_aflags_clobber:
            /* ah is: SF:ZF:0:AF:0:PF:1:CF.  If we turn everything on we will
             * get all 1's except bits 3 and 5, giving a hex mask of 0xD7.
             * Overflow is in the low byte (al usually) so use use a mask of
             * 0xD701 first.  If we turn everything off we get 0x0200.
             */
            entry = test_aflags(dc, bb, entry, 0xD701);
            (void)test_aflags(dc, bb, entry, 0x00200);
            break;
        }
        dr_insert_clean_call(dc, bb, entry, (void*)after_callee, false, 2,
                             OPND_CREATE_INT32(inline_expected),
                             OPND_CREATE_INTPTR(func_names[i]));
    }
    return DR_EMIT_DEFAULT;
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

/* Return either a stack access opnd_t or the first regparm.  Assumes frame
 * pointer is not omitted. */
static opnd_t
codegen_opnd_arg1(void)
{
    /* FIXME: Perhaps DR should expose this.  It currently tracks this in
     * core/instr.h. */
#ifdef X64
# ifdef LINUX
    int reg = DR_REG_RDI;
# else /* WINDOWS */
    int reg = DR_REG_RCX;
# endif
    return opnd_create_reg(reg);
#else /* X86 */
# ifdef LINUX
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
    mov REG_XDX, &count
    add [REG_XDX], REG_XAX
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
    APP(ilist, INSTR_CREATE_mov_imm
        (dc, opnd_create_reg(DR_REG_XDX), OPND_CREATE_INTPTR(&count)));
    APP(ilist, INSTR_CREATE_add(dc, OPND_CREATE_MEMPTR(DR_REG_XDX, 0), xax));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
callpic_pop:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_label
    Lnext_label:
    pop REG_XAX
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
    APP(ilist, INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_XAX)));
    codegen_epilogue(dc, ilist);
    return ilist;
}

/*
callpic_mov:
    push REG_XBP
    mov REG_XBP, REG_XSP
    call Lnext_instr_mov
    Lnext_instr_mov:
    mov REG_XAX, [REG_XSP]
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
        (dc, opnd_create_reg(DR_REG_XAX), OPND_CREATE_MEMPTR(DR_REG_XSP, 0)));
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
        mov SYMREF(count), REG_XAX
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
    /* If arg1 is non-zero, write 0xDEADBEEF to count. */
    APP(ilist, INSTR_CREATE_mov_ld(dc, xcx, codegen_opnd_arg1()));
    APP(ilist, INSTR_CREATE_jecxz(dc, opnd_create_instr(arg_zero)));
    APP(ilist, INSTR_CREATE_mov_imm(dc, xcx, OPND_CREATE_INTPTR(&count)));
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
