/* ******************************************************************************
 * Copyright (c) 2013-2015 Google, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* Code Manipulation API Sample:
 * bbbuf.c
 *
 * This sample demonstrates how to use a TLS field for per-thread profiling.
 * For each thread, we create a 64KB buffer with 64KB-aligned start address,
 * and store that into a TLS slot.
 * At the beginning of each basic block, we insert code to
 * - load the pointer from the TLS slot,
 * - store the starting pc of the basic block into the buffer,
 * - update the pointer by incrementing just the low 16 bits of the pointer
 *   so we will fill the buffer in a cyclical way.
 * This sample can be used for hot path profiling or debugging with execution
 * history.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include <string.h>

#define MINSERT instrlist_meta_preinsert

#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))

#define BUF_64K_BYTE (1 << 16)
/* We make TLS_BUF_SIZE to be 128KB so we can have a 64KB buffer
 * with 64KB aligned starting address.
 */
#define TLS_BUF_SIZE (BUF_64K_BYTE * 2)
static reg_id_t tls_seg;
static uint     tls_offs;
static int      tls_idx;

typedef struct _per_thread_t {
    void *seg_base;
    void *buf_base;
} per_thread_t;

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    app_pc pc = dr_fragment_app_pc(tag);
    reg_id_t reg;
#ifdef ARM
    /* We need a 2nd scratch reg for several operations */
    reg_id_t reg2;
#else
    bool dead;
#endif

    /* We do all our work at the start of the block prior to the first instr */
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    /* We need a scratch register */
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg) != DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return DR_EMIT_DEFAULT;
    }

#ifdef ARM
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &reg2) != DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return DR_EMIT_DEFAULT;
    }
#endif

    /* load buffer pointer from TLS field */
    dr_insert_read_raw_tls(drcontext, bb, inst, tls_seg, tls_offs, reg);

    /* store bb's start pc into the buffer */
#ifdef X86
    /* XXX i#1694: split this sample into separate simple and optimized versions,
     * with the simple using cross-platform instru and the optimized split into
     * arm vs x86 versions.
     */
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc,
                                     OPND_CREATE_MEMPTR(reg, 0),
                                     bb, inst, NULL, NULL);
#elif defined(ARM)
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc,
                                     opnd_create_reg(reg2),
                                     bb, inst, NULL, NULL);
    MINSERT(bb, inst, XINST_CREATE_store
            (drcontext, OPND_CREATE_MEMPTR(reg, 0), opnd_create_reg(reg2)));
#endif

    /* update the TLS buffer pointer by incrementing just the bottom 16 bits of
     * the pointer
     */
#ifdef X86
    if (drreg_are_aflags_dead(drcontext, inst, &dead) == DRREG_SUCCESS && dead) {
        /* if aflags are dead, we use add directly */
        MINSERT(bb, inst, INSTR_CREATE_add
                (drcontext,
                 opnd_create_far_base_disp(tls_seg, DR_REG_NULL, DR_REG_NULL,
                                           0, tls_offs, OPSZ_2),
                 OPND_CREATE_INT8(sizeof(app_pc))));
    } else {
        reg_id_t reg_16;
# ifdef X64
        reg_16 = reg_32_to_16(reg_64_to_32(reg));
# else
        reg_16 = reg_32_to_16(reg);
# endif
        /* we use lea to avoid aflags save/restore */
        MINSERT(bb, inst, INSTR_CREATE_lea
                (drcontext,
                 opnd_create_reg(reg_16),
                 opnd_create_base_disp(reg, DR_REG_NULL, 0,
                                       sizeof(app_pc), OPSZ_lea)));
        dr_insert_write_raw_tls(drcontext, bb, inst, tls_seg, tls_offs, reg);
    }
#elif defined(ARM)
    /* We use this sequence:
     *   mov r1, #4
     *   uqadd16 r0, r0, r1
     */
    MINSERT(bb, inst, INSTR_CREATE_mov
            (drcontext, opnd_create_reg(reg2), OPND_CREATE_INT8(sizeof(app_pc))));
    MINSERT(bb, inst, INSTR_CREATE_uqadd16
            (drcontext, opnd_create_reg(reg), opnd_create_reg(reg),
             opnd_create_reg(reg2)));
    dr_insert_write_raw_tls(drcontext, bb, inst, tls_seg, tls_offs, reg);
#endif

    if (drreg_unreserve_register(drcontext, bb, inst, reg) != DRREG_SUCCESS)
        DR_ASSERT(false);
#ifdef ARM
    if (drreg_unreserve_register(drcontext, bb, inst, reg2) != DRREG_SUCCESS)
        DR_ASSERT(false);
#endif

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = dr_thread_alloc(drcontext, sizeof(*data));

    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);
    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     * It is mainly for users using a debugger to get the execution history.
     */
    data->seg_base = dr_get_dr_segment_base(tls_seg);
    /* We allocate a 128KB buffer to make sure we have a 64KB buffer with
     * 64KB-aligned starting address, so that we can fill the buffer
     * cyclically by incrementing the bottom 16 bits of the pointer.
     */
    data->buf_base = dr_raw_mem_alloc(TLS_BUF_SIZE,
                                      DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                                      NULL);
    DR_ASSERT(data->seg_base != NULL && data->buf_base != NULL);
    memset(data->buf_base, 0, TLS_BUF_SIZE);
    /* put the 64KB-aligned address into TLS slot as the pointer pointing
     * to the 64KB cyclic buffer
     */
    *(void **)((byte *)(data->seg_base) + tls_offs) = (void *)
        ALIGN_FORWARD(data->buf_base, BUF_64K_BYTE);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = drmgr_get_tls_field(drcontext, tls_idx);
    dr_raw_mem_free(data->buf_base, TLS_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(*data));
}

static void
event_exit(void)
{
    if (!dr_raw_tls_cfree(tls_offs, 1))
        DR_ASSERT(false);

    if (!drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);

    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = {sizeof(ops), 2 /*max slots needed*/, false};
    dr_set_client_name("DynamoRIO Sample Client 'bbbuf'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() ||
        drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx > -1);

    /* The TLS field provided by DR cannot be directly accessed from the code cache.
     * For better performance, we allocate raw TLS so that we can directly
     * access and update it with a single instruction.
     */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, 1, 0))
        DR_ASSERT(false);
}

