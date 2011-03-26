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

#ifndef ASM_CODE_ONLY

#include "dr_api.h"

#ifdef LINUX
#include "signal.h"  /* SIGTRAP */
#endif

#define PRE  instrlist_meta_preinsert

/* Instrumentation functions that work for 32-bit x86. */
#define FUNCTIONS_32() \
        FUNCTION(empty) \
        FUNCTION(inscount) \
        FUNCTION(callpic_pop) \
        FUNCTION(callpic_mov) \
        FUNCTION(cond_br) \
        FUNCTION(nonleaf) \
        FUNCTION(tls_clobber)

/* Instrumentation functions that use 64-bit extensions. */
#define FUNCTIONS_64()

#ifdef X64
#define FUNCTIONS() \
        FUNCTIONS_32() \
        FUNCTIONS_64() \
        LAST_FUNCTION()
#else
#define FUNCTIONS() \
        FUNCTIONS_32() \
        LAST_FUNCTION()
#endif

/* Table of function names. */
#define FUNCTION(fn_name) #fn_name,
#define LAST_FUNCTION() NULL
static const char *func_names[] = {
    FUNCTIONS()
};
#undef FUNCTION
#undef LAST_FUNCTION

/* Function declarations. */
#define FUNCTION(fn_name) void fn_name();
#define LAST_FUNCTION()
FUNCTIONS()
#undef FUNCTION
#undef LAST_FUNCTION

/* Table of instrumentation function pointers. */
#define FUNCTION(fn_name) (void*)&fn_name,
#define LAST_FUNCTION() NULL
static void *func_ptrs[] = {
    FUNCTIONS()
};
#undef FUNCTION
#undef LAST_FUNCTION

/* Count the number of functions. */
#define FUNCTION(fn_name) FN_##fn_name,
#define LAST_FUNCTION() LAST_FUNC_ENUM
enum {
    FUNCTIONS()
};
#undef FUNCTION
#undef LAST_FUNCTION

/* A separate define so ctags can find it. */
#define N_FUNCS LAST_FUNC_ENUM

static app_pc func_pcs[N_FUNCS];
static bool func_called[N_FUNCS];

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *dc, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);
static void lookup_pcs(void);
static void mark_instrument_code_writable(void);

/* These aren't really void * globals, they're just placeholders whose address
 * we take to get the range of the text section so we can mark it writable.
 * This relies on the assembler not reordering the labels and functions, which
 * seems reasonable.
 */
extern void *instrument_code_start;
extern void *instrument_code_end;

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    dr_fprintf(STDERR, "INIT\n");

    /* Lookup pcs. */
    lookup_pcs();
    mark_instrument_code_writable();
}

static void
lookup_pcs(void)
{
    dr_module_iterator_t *iter;
    int i;

    iter = dr_module_iterator_start();
    while (dr_module_iterator_hasnext(iter)) {
        module_data_t *data = dr_module_iterator_next(iter);
        module_handle_t handle = data->handle;
        for (i = 0; i < N_FUNCS; i++) {
            app_pc func_pc = (app_pc)dr_get_proc_address(handle, func_names[i]);
            if (func_pc != NULL) {
                func_pcs[i] = func_pc;
            }
        }
        dr_free_module_data(data);
    }
    dr_module_iterator_stop(iter);

    for (i = 0; i < N_FUNCS; i++) {
        DR_ASSERT_MSG(func_pcs[i] != NULL,
                      "Unable to find a function we wanted to instrument!");
    }
}

#define ALIGN_BACKWARD(x, alignment) \
        (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((ptr_uint_t)(alignment)-1)))

static void
mark_instrument_code_writable(void)
{
    void *base = (void*)ALIGN_BACKWARD(&instrument_code_start, 4096);
    void *end = (void*)ALIGN_FORWARD(&instrument_code_end, 4096);
    size_t size = (size_t)end - (size_t)base;
    dr_memory_protect(base, size,
                      DR_MEMPROT_EXEC|DR_MEMPROT_READ|DR_MEMPROT_WRITE);
}

static void
event_exit(void)
{
    int i;
    for (i = 0; i < N_FUNCS; i++) {
        DR_ASSERT_MSG(func_called[i],
                      "Instrumentation function was not called!");
    }
    dr_fprintf(STDERR, "PASSED\n");
}

/* Globals used by instrumentation functions. */
ptr_uint_t count;
static bool patched_func_called;

static void
before_inlined_call(byte *func_ptr)
{
    count = 0;
    /* Assert that all calls are inlined by placing a breakpoint instruction at
     * the beginning of it. */
    func_ptr[0] = 0xCC;  /* int3 */
}

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

static void
patched_func(void)
{
    patched_func_called = true;
}

static void
after_patched(void)
{
    DR_ASSERT(patched_func_called);
}

/* Patch a function to tail call to patched_func. */
static void
patch_func(app_pc func)
{
    void *dc;
    instrlist_t *ilist;
    instr_t *jmp;

    /* These functions are both in the client shlib, so there should be no
     * reachability issues. */
    dc = dr_get_current_drcontext();
    ilist = instrlist_create(dc);
    jmp = INSTR_CREATE_jmp(dc, opnd_create_pc((app_pc)&patched_func));
    instrlist_append(ilist, jmp);
    instrlist_encode(dc, ilist, func, false /* no jump targets */);
    instrlist_clear_and_destroy(dc, ilist);

    patched_func_called = false;
}

static void
fill_scratch(void)
{
    void *dc = dr_get_current_drcontext();
    int slot;
    /* Set slots to 0x111... 0x222... etc. */
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
    /* Check that slots are 0x111... 0x222... etc. */
    for (slot = SPILL_SLOT_1; slot <= SPILL_SLOT_MAX; slot++) {
        reg_t value = dr_read_saved_reg(dc, slot);
        reg_t expected = slot * 0x11111111;
        DR_ASSERT_MSG(value == expected,
                      "Client scratch slot clobbered by clean call!");
    }
}

static dr_emit_flags_t
event_basic_block(void *dc, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *entry = instrlist_first(bb);
    app_pc entry_pc = instr_get_app_pc(entry);
    int i;
    for (i = 0; i < N_FUNCS; i++) {
        if (entry_pc != func_pcs[i])
            continue;

        func_called[i] = 1;
        dr_insert_clean_call(dc, bb, entry, (void*)before_inlined_call, false,
                             1, OPND_CREATE_INTPTR(func_ptrs[i]));

        switch (i) {
        default:
            /* Default to no-arg call. */
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            break;
        case FN_inscount:
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 1,
                                 OPND_CREATE_INT32(0xDEAD));
            dr_insert_clean_call(dc, bb, entry, (void*)after_inscount, false,
                                 0);
            break;
        case FN_callpic_pop:
        case FN_callpic_mov:
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            dr_insert_clean_call(dc, bb, entry, (void*)after_callpic, false, 0);
            break;
        case FN_nonleaf:
        case FN_cond_br:
            /* These functions cannot be inlined, so we assert that they are
             * not inlined by patching the out of line version to tail call to
             * another function.
             */
            dr_insert_clean_call(dc, bb, entry, (void*)patch_func, false, 1,
                                 OPND_CREATE_INTPTR(func_ptrs[i]));
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            dr_insert_clean_call(dc, bb, entry, (void*)after_patched, false, 0);
            break;
        case FN_tls_clobber:
            /* TODO(rnk): Fix this failing test.
            dr_insert_clean_call(dc, bb, entry, (void*)fill_scratch, false, 0);
            dr_insert_clean_call(dc, bb, entry, func_ptrs[i], false, 0);
            dr_insert_clean_call(dc, bb, entry, (void*)check_scratch, false, 0);
            */
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}

#else /* ASM_CODE_ONLY */

#include "asm_defines.asm"

START_FILE

/* Add labels that we can use to mark this space as writable. */
DECLARE_GLOBAL(instrument_code_start)
GLOBAL_LABEL(instrument_code_start:)

DECLARE_FUNC(empty)
GLOBAL_LABEL(empty:)
    ret
    END_FUNC(empty)

DECL_EXTERN(count)

DECLARE_FUNC(inscount)
GLOBAL_LABEL(inscount:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    push REG_XAX
    mov REG_XAX, ARG1
    add SYMREF(count), REG_XAX
    pop REG_XAX
    leave
    ret
    END_FUNC(inscount)

DECLARE_FUNC(callpic_pop)
GLOBAL_LABEL(callpic_pop:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    push REG_XAX
    call .Lnext_instr_pop
    .Lnext_instr_pop:
    pop REG_XAX
    add REG_XAX, count - .Lnext_instr_pop
    incq [REG_XAX]
    pop REG_XAX
    leave
    ret
    END_FUNC(callpic_pop)

DECLARE_FUNC(callpic_mov)
GLOBAL_LABEL(callpic_mov:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    push REG_XAX
    call .Lnext_instr_mov
    .Lnext_instr_mov:
    mov REG_XAX, [REG_XSP]
    add REG_XAX, count - .Lnext_instr_mov
    incq [REG_XAX]
    pop REG_XAX
    leave
    ret
    END_FUNC(callpic_mov)

/* Simple function that can't be inlined due to conditional branch.  Loads
 * 'count', compares to 0, and if equal sets 'count' to 0xDEADBEEF.  We avoid
 * touching FLAGS, since that may also affect the inliner's decisions.
 */
DECLARE_FUNC(cond_br)
GLOBAL_LABEL(cond_br:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    push REG_XCX
    mov REG_XCX, SYMREF(count)
    jecxz .Lcount_zero
    jmp .Lnon_zero
    .Lcount_zero:
        mov REG_XCX, HEX(DEADBEEF)
        mov SYMREF(count), REG_XCX
    .Lnon_zero:
    pop REG_XCX
    leave
    ret
    END_FUNC(cond_br)

/* Non-leaf functions cannot be inlined.
 */
DECLARE_FUNC(nonleaf)
GLOBAL_LABEL(nonleaf:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    call cond_br
    leave
    ret
    END_FUNC(nonleaf)

/* A simple function that uses 2 registers and 1 local variable, which should
 * fill all of the scratch slots that the inliner uses.  This used to clobber
 * the scratch slots exposed to the client. */
DECLARE_FUNC(tls_clobber)
GLOBAL_LABEL(tls_clobber:)
    push REG_XBP
    mov REG_XBP, REG_XSP
    mov REG_XAX, HEX(DEAD)
    mov REG_XDX, HEX(BEEF)
    /* Materialize 0xDEADBEEF in XAX.  Why?  Who knows. */
    shl REG_XAX, 16
    or REG_XAX, REG_XDX
    leave
    ret
    END_FUNC(tls_clobber)

DECLARE_GLOBAL(instrument_code_end)
GLOBAL_LABEL(instrument_code_end:)

END_FILE

#endif /* ASM_CODE_ONLY */
