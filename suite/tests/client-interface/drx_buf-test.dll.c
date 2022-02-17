/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
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

/* Tests the drx_buf extension */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drx.h"
#include "drx_buf-test-shared.h"

#define CIRCULAR_FAST_SZ DRX_BUF_FAST_CIRCULAR_BUFSZ
#define CIRCULAR_SLOW_SZ 256
#define TRACE_SZ 256

#define MINSERT instrlist_meta_preinsert

#ifdef X64
static char cmp[] = "ABCDEFGHABCDEFGH";
#else
static char cmp[] = "ABCDEFGH";
#endif

static const char test_copy[TRACE_SZ] =
    "12345678911234567892123456789312345678941234567895123456789612"
    "12345678911234567892123456789312345678941234567895123456789612"
    "12345678911234567892123456789312345678941234567895123456789612"
    "12345678911234567892123456789312345678941234567895123456789612";
static const char test_null[TRACE_SZ];

static drx_buf_t *circular_fast;
static drx_buf_t *circular_slow;
static drx_buf_t *trace;
static volatile int num_faults;

static void
event_thread_init(void *drcontext)
{
    /* memset all of the buffers, to verify
     * that they've already been initialized
     */
    byte *buf_base;

    buf_base = drx_buf_get_buffer_base(drcontext, circular_fast);
    memset(buf_base, 0, CIRCULAR_FAST_SZ);

    buf_base = drx_buf_get_buffer_base(drcontext, circular_slow);
    memset(buf_base, 0, CIRCULAR_SLOW_SZ);

    buf_base = drx_buf_get_buffer_base(drcontext, trace);
    memset(buf_base, 0, TRACE_SZ);
}

static void
verify_buffers_empty(drx_buf_t *client)
{
    void *drcontext = dr_get_current_drcontext();
    byte *buf_base, *buf_ceil;

    buf_base = drx_buf_get_buffer_base(drcontext, client);
    buf_ceil = drx_buf_get_buffer_ptr(drcontext, client);
    CHECK(buf_base == buf_ceil, "buffer not empty");
}

static void
verify_buffers_dirty(drx_buf_t *client, int32_t test)
{
    void *drcontext = dr_get_current_drcontext();
    byte *buf_base, *buf_ceil;

    buf_base = drx_buf_get_buffer_base(drcontext, client);
    buf_ceil = drx_buf_get_buffer_ptr(drcontext, client);
    CHECK(buf_base + sizeof(int32_t) == buf_ceil, "buffer not dirty");
    CHECK(*(int32_t *)buf_base == test, "buffer has wrong value");
}

static void
verify_trace_buffer(void *drcontext, void *buf_base, size_t size)
{
    dr_atomic_add32_return_sum(&num_faults, 1);
}

static void
verify_store(drx_buf_t *client)
{
    void *drcontext = dr_get_current_drcontext();
    char *buf_base = drx_buf_get_buffer_base(drcontext, client);
    CHECK(strcmp(buf_base, cmp) == 0,
          "Store immediate or Store register failed to copy right value");
    memset(buf_base, 0, drx_buf_get_buffer_size(drcontext, client));
}

static void
verify_memcpy(drx_buf_t *client)
{
    void *drcontext = dr_get_current_drcontext();
    char *buf_base = drx_buf_get_buffer_base(drcontext, client);
    CHECK(memcmp(test_copy, buf_base, sizeof(test_copy)) == 0,
          "drx_buf_insert_buf_memcpy() did not correctly copy the bytes over");
    memset(buf_base, 0, drx_buf_get_buffer_size(drcontext, client));
}

static void
verify_buffers_nulled(drx_buf_t *client)
{
    void *drcontext = dr_get_current_drcontext();
    byte *buf_base = drx_buf_get_buffer_base(drcontext, client);
    CHECK(memcmp(buf_base, test_null, sizeof(test_null)) == 0, "buffer not nulled");
}

static dr_emit_flags_t
event_app_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                   bool translating, OUT void **user_data)
{
    instr_t *inst, *label;
    bool prev_was_mov_const = false;
    ptr_int_t val1, val2;
    *user_data = NULL;
    /* Look for duplicate mov immediates telling us which subtest we're in */
    for (inst = instrlist_first_app(bb); inst != NULL; inst = instr_get_next_app(inst)) {
        if (instr_is_mov_constant(inst, prev_was_mov_const ? &val2 : &val1)) {
            if (prev_was_mov_const && val1 == val2 &&
                val1 != 0 && /* rule out xor w/ self */
                opnd_is_reg(instr_get_dst(inst, 0)) &&
                opnd_get_reg(instr_get_dst(inst, 0)) == TEST_REG) {
                *user_data = (void *)val1;
                label = INSTR_CREATE_label(drcontext);
                instr_set_translation(label, instr_get_app_pc(inst));
                instrlist_meta_postinsert(bb, inst, label);
            } else
                prev_was_mov_const = true;
        } else
            prev_was_mov_const = false;
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t reg_ptr = IF_X86_ELSE(DR_REG_XDX, TEST_REG);
    reg_id_t reg_tmp = IF_X86_ELSE(DR_REG_XCX, DR_REG_R3);
    /* We need a third register on ARM, because updating the buf pointer
     * requires a second scratch reg.
     */
    reg_id_t scratch = IF_X86_ELSE(reg_tmp, DR_REG_R5);
    ptr_int_t subtest = (ptr_int_t)user_data;

    if (!instr_is_label(inst))
        return DR_EMIT_DEFAULT;

#ifdef X86
    scratch = reg_resize_to_opsz(scratch, OPSZ_4);
#endif
    if (subtest == DRX_BUF_TEST_1_C) {
        /* testing fast circular buffer */
        /* test to make sure that on first invocation, the buffer is empty */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(circular_fast));

        /* load the buf pointer, and then write a garbage element to the buffer */
        drx_buf_insert_load_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 0);
        drx_buf_insert_update_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr,
                                      reg_tmp, sizeof(int));

        /* verify the buffer was written to */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_dirty, false, 2,
                             OPND_CREATE_INTPTR(circular_fast), opnd_create_reg(scratch));

        /* fast circular buffer: trigger an overflow */
        drx_buf_insert_load_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr);
        drx_buf_insert_update_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr,
                                      reg_tmp, CIRCULAR_FAST_SZ - sizeof(int));

        /* the buffer is now clean */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(circular_fast));
    } else if (subtest == DRX_BUF_TEST_2_C) {
        /* testing slow circular buffer */
        /* test to make sure that on first invocation, the buffer is empty */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(circular_slow));

        /* load the buf pointer, and then write an element to the buffer */
        drx_buf_insert_load_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 0);
        drx_buf_insert_update_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr,
                                      DR_REG_NULL, sizeof(int));

        /* verify the buffer was written to */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_dirty, false, 2,
                             OPND_CREATE_INTPTR(circular_slow), opnd_create_reg(scratch));

        /* slow circular buffer: trigger a fault */
        drx_buf_insert_load_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr);
        drx_buf_insert_update_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr,
                                      DR_REG_NULL, CIRCULAR_SLOW_SZ - sizeof(int));
        /* the "trigger" is a write, so we write whatever garbage is in reg_tmp */
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 0);

        /* the buffer is now clean */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(circular_slow));
    } else if (subtest == DRX_BUF_TEST_3_C) {
        /* testing trace buffer */
        /* test to make sure that on first invocation, the buffer is empty */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(trace));

        /* load the buf pointer, and then write an element to the buffer */
        drx_buf_insert_load_buf_ptr(drcontext, trace, bb, inst, reg_ptr);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 0);
        drx_buf_insert_update_buf_ptr(drcontext, trace, bb, inst, reg_ptr, DR_REG_NULL,
                                      sizeof(int));

        /* verify the buffer was written to */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_dirty, false, 2,
                             OPND_CREATE_INTPTR(trace), opnd_create_reg(scratch));

        /* trace buffer: trigger a fault and verify */
        drx_buf_insert_load_buf_ptr(drcontext, trace, bb, inst, reg_ptr);
        drx_buf_insert_update_buf_ptr(drcontext, trace, bb, inst, reg_ptr, DR_REG_NULL,
                                      TRACE_SZ - sizeof(int));
        /* the "trigger" is a write, so we write whatever garbage is in reg_tmp */
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 0);

        /* the buffer is now clean */
        dr_insert_clean_call(drcontext, bb, inst, verify_buffers_empty, false, 1,
                             OPND_CREATE_INTPTR(trace));
    } else if (subtest == DRX_BUF_TEST_4_C) {
        /* test immediate store: 8 bytes (if possible), 4 bytes, 2 bytes and 1 byte */
        /* "ABCDEFGH\x00" (x2 for x64) */
        drx_buf_insert_load_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x41, OPSZ_1), OPSZ_1, 0);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x42, OPSZ_1), OPSZ_1, 1);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x4443, OPSZ_2), OPSZ_2, 2);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x48474645, OPSZ_4), OPSZ_4, 4);
#ifdef X64
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x4847464544434241, OPSZ_8),
                                 OPSZ_8, 8);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x00, OPSZ_1), OPSZ_1, 17);
#else
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x00, OPSZ_1), OPSZ_1, 9);
#endif
        dr_insert_clean_call(drcontext, bb, inst, verify_store, false, 1,
                             OPND_CREATE_INTPTR(circular_fast));
    } else if (subtest == DRX_BUF_TEST_5_C) {
        /* test register store: 8 bytes (if possible), 4 bytes, 2 bytes and 1 byte */
        /* "ABCDEFGH\x00" (x2 for x64) */
        drx_buf_insert_load_buf_ptr(drcontext, circular_fast, bb, inst, reg_ptr);
        scratch = reg_resize_to_opsz(scratch, OPSZ_1);
        MINSERT(bb, inst,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      opnd_create_immed_int(0x41, OPSZ_1)));
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_1, 0);
        MINSERT(bb, inst,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      opnd_create_immed_int(0x42, OPSZ_1)));
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_1, 1);
        scratch = reg_resize_to_opsz(scratch, OPSZ_2);
        MINSERT(bb, inst,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      opnd_create_immed_int(0x4443, OPSZ_2)));
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_2, 2);
        scratch = reg_resize_to_opsz(scratch, OPSZ_4);
#ifdef X86
        MINSERT(bb, inst,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch),
                                      opnd_create_immed_int(0x48474645, OPSZ_4)));
#else
        instrlist_insert_mov_immed_ptrsz(
            drcontext, 0x48474645, opnd_create_reg(reg_resize_to_opsz(scratch, OPSZ_PTR)),
            bb, inst, NULL, NULL);
#endif
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_4, 4);
#ifdef X64
        scratch = reg_resize_to_opsz(scratch, OPSZ_8);
        /* only way to reliably move a 64 bit int into a register */
        instrlist_insert_mov_immed_ptrsz(drcontext, 0x4847464544434241,
                                         opnd_create_reg(scratch), bb, inst, NULL, NULL);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, DR_REG_NULL,
                                 opnd_create_reg(scratch), OPSZ_8, 8);
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x00, OPSZ_1), OPSZ_1, 17);
#else
        drx_buf_insert_buf_store(drcontext, circular_fast, bb, inst, reg_ptr, scratch,
                                 opnd_create_immed_int(0x00, OPSZ_1), OPSZ_1, 9);
#endif
        dr_insert_clean_call(drcontext, bb, inst, verify_store, false, 1,
                             OPND_CREATE_INTPTR(circular_fast));
    } else if (subtest == DRX_BUF_TEST_6_C) {
        /* Currently, the fast circular buffer does not recommend variable-size
         * writes, for good reason. We don't test the memcpy operation on the
         * fast circular buffer.
         */
        /* verify memcpy works on the slow clrcular buffer */
        drx_buf_insert_load_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr);
        instrlist_insert_mov_immed_ptrsz(
            drcontext, (ptr_int_t)test_copy,
            opnd_create_reg(reg_resize_to_opsz(scratch, OPSZ_PTR)), bb, inst, NULL, NULL);
        drx_buf_insert_buf_memcpy(drcontext, circular_slow, bb, inst, reg_ptr,
                                  reg_resize_to_opsz(scratch, OPSZ_PTR),
                                  sizeof(test_copy));
        /* NULL out the buffer */
        drx_buf_insert_load_buf_ptr(drcontext, circular_slow, bb, inst, reg_ptr);
        instrlist_insert_mov_immed_ptrsz(
            drcontext, (ptr_int_t)test_null,
            opnd_create_reg(reg_resize_to_opsz(scratch, OPSZ_PTR)), bb, inst, NULL, NULL);
        drx_buf_insert_buf_memcpy(drcontext, circular_slow, bb, inst, reg_ptr,
                                  reg_resize_to_opsz(scratch, OPSZ_PTR),
                                  sizeof(test_null));
        /* Unfortunately, we can't just use the check in verify_buffer_empty, because
         * drx_buf_insert_buf_memcpy() incrememnts the buffer pointer internally, unlike
         * drx_buf_insert_buf_store(). We simply check that the buffer was NULLed out.
         */
        dr_insert_clean_call(drcontext, bb, inst, (void *)verify_buffers_nulled, false, 1,
                             OPND_CREATE_INTPTR(circular_slow));
        /* verify memcpy works on the trace buffer */
        drx_buf_insert_load_buf_ptr(drcontext, trace, bb, inst, reg_ptr);
        instrlist_insert_mov_immed_ptrsz(
            drcontext, (ptr_int_t)test_copy,
            opnd_create_reg(reg_resize_to_opsz(scratch, OPSZ_PTR)), bb, inst, NULL, NULL);
        drx_buf_insert_buf_memcpy(drcontext, trace, bb, inst, reg_ptr,
                                  reg_resize_to_opsz(scratch, OPSZ_PTR),
                                  sizeof(test_copy));
        /* NULL out the buffer */
        drx_buf_insert_load_buf_ptr(drcontext, trace, bb, inst, reg_ptr);
        instrlist_insert_mov_immed_ptrsz(
            drcontext, (ptr_int_t)test_null,
            opnd_create_reg(reg_resize_to_opsz(scratch, OPSZ_PTR)), bb, inst, NULL, NULL);
        drx_buf_insert_buf_memcpy(drcontext, trace, bb, inst, reg_ptr,
                                  reg_resize_to_opsz(scratch, OPSZ_PTR),
                                  sizeof(test_null));
        /* verify buffer was NULLed */
        dr_insert_clean_call(drcontext, bb, inst, (void *)verify_buffers_nulled, false, 1,
                             OPND_CREATE_INTPTR(trace));
    }

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    /* we are supposed to have faulted NUM_ITER times per thread, plus 2 more
     * because the callback is called on thread_exit(). Finally, two more for
     * drx_buf_insert_buf_memcpy().
     */
    CHECK(num_faults == NUM_ITER * 2 + 2 + 2, "the number of faults don't match up");
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction))
        CHECK(false, "exit failed");
    drx_buf_free(circular_fast);
    drx_buf_free(circular_slow);
    drx_buf_free(trace);
    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_exit();
    drx_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    if (!drmgr_init())
        CHECK(false, "init failed");

    /* init buffer routines */
    drx_init();
    circular_fast = drx_buf_create_circular_buffer(DRX_BUF_FAST_CIRCULAR_BUFSZ);
    circular_slow = drx_buf_create_circular_buffer(CIRCULAR_SLOW_SZ);
    trace = drx_buf_create_trace_buffer(TRACE_SZ, verify_trace_buffer);
    CHECK(circular_fast != NULL, "circular fast failed");
    CHECK(circular_slow != NULL, "circular slow failed");
    CHECK(trace != NULL, "trace failed");

    CHECK(drmgr_register_thread_init_event(event_thread_init),
          "event thread init failed");

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_app_analysis,
                                                 event_app_instruction, NULL))
        CHECK(false, "init failed");
}
