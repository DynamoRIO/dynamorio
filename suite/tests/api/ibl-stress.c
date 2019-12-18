/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

#include "configure.h"
#include "dr_api.h"
#include "drvector.h"
#include "tools.h"
#include "thread.h"
#include "condvar.h"

#ifdef LARGER_TEST
/* 20K sequences gives us ~150K bbs. */
#    define NUM_SEQUENCES 20000
#    define NUM_THREADS 16
#else
/* We scale down from the larger size which more readily exposes races
 * to a size suitable for a regression test on a small-sized VM.
 */
#    define NUM_SEQUENCES 1000
#    define NUM_THREADS 8
#endif

#define VERBOSE 0

#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

/***************************************************************************
 * Synthetic code generation
 */

#ifndef X86
#    error Non-x86 is not supported.
#endif

static byte *generated_code;
static size_t code_size;

static void
print_instr_pc(instr_t *instr, byte *encode_pc)
{
#if VERBOSE > 0
    dr_fprintf(STDERR, "%p: ", encode_pc);
    instr_disassemble(GLOBAL_DCONTEXT, instr, STDERR);
    dr_fprintf(STDERR, "\n");
#endif
}

static byte *
append_ilist(instrlist_t *ilist, byte *encode_pc, instr_t *instr)
{
    instrlist_append(ilist, instr);
    print_instr_pc(instr, encode_pc);
    return encode_pc + instr_length(GLOBAL_DCONTEXT, instr);
}

static byte *
generate_stack_accesses(instrlist_t *ilist, drvector_t *tags, byte *encode_pc)
{
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_push(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XBP)));
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_push(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XBX)));
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_push(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XDI)));
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_push(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XSI)));
    encode_pc = append_ilist(
        ilist, encode_pc, INSTR_CREATE_pop(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XSI)));
    encode_pc = append_ilist(
        ilist, encode_pc, INSTR_CREATE_pop(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XDI)));
    encode_pc = append_ilist(
        ilist, encode_pc, INSTR_CREATE_pop(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XBX)));
    encode_pc = append_ilist(
        ilist, encode_pc, INSTR_CREATE_pop(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XBP)));
    return encode_pc;
}

static byte *
generate_direct_call(instrlist_t *ilist, drvector_t *tags, byte *encode_pc)
{
    instr_t *callee = INSTR_CREATE_label(GLOBAL_DCONTEXT);
    instr_t *after_callee = INSTR_CREATE_label(GLOBAL_DCONTEXT);
    encode_pc = append_ilist(
        ilist, encode_pc, INSTR_CREATE_call(GLOBAL_DCONTEXT, opnd_create_instr(callee)));
    drvector_append(tags, encode_pc);
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_jmp(GLOBAL_DCONTEXT, opnd_create_instr(after_callee)));
    drvector_append(tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, callee);
    encode_pc = generate_stack_accesses(ilist, tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, INSTR_CREATE_ret(GLOBAL_DCONTEXT));
    drvector_append(tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, after_callee);
    return encode_pc;
}

static byte *
generate_indirect_call(instrlist_t *ilist, drvector_t *tags, byte *encode_pc)
{
    instr_t *callee = INSTR_CREATE_label(GLOBAL_DCONTEXT);
    instr_t *after_callee = INSTR_CREATE_label(GLOBAL_DCONTEXT);
    instr_t *first, *last;
    instrlist_insert_mov_instr_addr(GLOBAL_DCONTEXT, callee, generated_code,
                                    opnd_create_reg(DR_REG_XAX), ilist, NULL, &first,
                                    &last);
    while (first != NULL && first != last) {
        print_instr_pc(first, encode_pc);
        encode_pc += instr_length(GLOBAL_DCONTEXT, first);
        first = instr_get_next(first);
    }
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_call_ind(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XAX)));
    drvector_append(tags, encode_pc);
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_jmp(GLOBAL_DCONTEXT, opnd_create_instr(after_callee)));
    drvector_append(tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, callee);
    encode_pc = generate_stack_accesses(ilist, tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, INSTR_CREATE_ret(GLOBAL_DCONTEXT));
    drvector_append(tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, after_callee);
    return encode_pc;
}

static byte *
generate_indirect_jump(instrlist_t *ilist, drvector_t *tags, byte *encode_pc)
{
    instr_t *target = INSTR_CREATE_label(GLOBAL_DCONTEXT);
    instr_t *first, *last;
    instrlist_insert_mov_instr_addr(GLOBAL_DCONTEXT, target, generated_code,
                                    opnd_create_reg(DR_REG_XAX), ilist, NULL, &first,
                                    &last);
    while (first != NULL && first != last) {
        print_instr_pc(first, encode_pc);
        encode_pc += instr_length(GLOBAL_DCONTEXT, first);
        first = instr_get_next(first);
    }
    encode_pc =
        append_ilist(ilist, encode_pc,
                     INSTR_CREATE_jmp_ind(GLOBAL_DCONTEXT, opnd_create_reg(DR_REG_XAX)));
    drvector_append(tags, encode_pc);
    encode_pc = append_ilist(ilist, encode_pc, target);
    encode_pc = generate_stack_accesses(ilist, tags, encode_pc);
    return encode_pc;
}

static void
generate_code()
{
    const size_t sequence_size = 73; /* Measured manually. */
    /* The final return takes up 1 byte. */
    code_size = NUM_SEQUENCES * sequence_size + 1;
    generated_code =
        (byte *)allocate_mem(code_size, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE);
    assert(generated_code != NULL);

    /* Synthesize code which includes a lot of indirect branches to test i#3098.
     * We pre-populate the cache to better stress the ibt tables.
     * If we instead incrementally build blocks, the ibt table additions are
     * mixed into the slow, serializing block building, and we don't see
     * many races that way.
     */
    drvector_t tags;
    /* Each sequence has 7 bb's.  We round up to 8 to cover the extra and have a
     * rounder number.
     */
    drvector_init(&tags, 8 * NUM_SEQUENCES, false, NULL);
    drvector_append(&tags, generated_code);
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    byte *encode_pc = generated_code;
    for (int i = 0; i < NUM_SEQUENCES; ++i) {
        encode_pc = generate_stack_accesses(ilist, &tags, encode_pc);
        encode_pc = generate_direct_call(ilist, &tags, encode_pc);
        encode_pc = generate_indirect_call(ilist, &tags, encode_pc);
        encode_pc = generate_indirect_jump(ilist, &tags, encode_pc);
    }
    /* The outer level is a function. */
    encode_pc = append_ilist(ilist, encode_pc, INSTR_CREATE_ret(GLOBAL_DCONTEXT));

    byte *end_pc = instrlist_encode(GLOBAL_DCONTEXT, ilist, generated_code, true);
    assert(end_pc <= generated_code + code_size);

    protect_mem(generated_code, code_size, ALLOW_EXEC | ALLOW_READ);

#if VERBOSE > 0
    for (int i = 0; i < tags.entries; i++)
        dr_fprintf(STDERR, "%d: %p\n", i, tags.array[i]);
#endif
    bool success = dr_prepopulate_cache((byte **)tags.array, tags.entries);
    assert(success);

    drvector_delete(&tags);
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
}

void
cleanup_code(void)
{
    free_mem((char *)generated_code, code_size);
}

/***************************************************************************
 * Top-level
 */

static void *thread_continue;
static void *thread_ready[NUM_THREADS];

THREAD_FUNC_RETURN_TYPE
thread_function(void *arg)
{
    const int ITERS = 5;
    int i;
    unsigned int idx = (unsigned int)(ptr_uint_t)arg;
    signal_cond_var(thread_ready[idx]);
    wait_cond_var(thread_continue);
    for (i = 0; i < ITERS; ++i) {
        ((void (*)(void))generated_code)();
    }
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(void)
{
    int i;
    thread_t thread[NUM_THREADS];
    thread_continue = create_cond_var();
    dr_app_setup();
    generate_code();
    dr_app_start();
    for (i = 0; i < NUM_THREADS; i++) {
        thread_ready[i] = create_cond_var();
        thread[i] = create_thread(thread_function, (void *)(ptr_uint_t)i);
    }
    signal_cond_var(thread_continue);
    for (i = 0; i < NUM_THREADS; i++)
        wait_cond_var(thread_ready[i]);
    for (i = 0; i < NUM_THREADS; i++) {
        join_thread(thread[i]);
        destroy_cond_var(thread_ready[i]);
    }
    dr_app_stop_and_cleanup();
    cleanup_code();
    destroy_cond_var(thread_continue);
    print("all done\n");
    return 0;
}
