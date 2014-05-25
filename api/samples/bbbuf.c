/* ******************************************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

typedef struct _per_thread_t {
    void *seg_base;
    void *buf_base;
} per_thread_t;

/* iterate basic block to find a dead register */
static reg_id_t
bb_find_dead_reg(instrlist_t *ilist)
{
    instr_t *instr;
    int i;
    bool reg_is_read[DR_NUM_GPR_REGS] = { false,};

    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        if (instr_is_syscall(instr) || instr_is_interrupt(instr))
            return DR_REG_NULL;
        for (i = 0; i < DR_NUM_GPR_REGS; i++) {
            if (!reg_is_read[i] &&
                instr_reads_from_reg(instr, (reg_id_t)(DR_REG_START_GPR + i))) {
                reg_is_read[i] = true;
            }
            if (!reg_is_read[i] &&
                instr_writes_to_exact_reg(instr,
                                          (reg_id_t)(DR_REG_START_GPR + i))) {
                return (reg_id_t)(DR_REG_START_GPR + i);
            }
#ifdef X64
            /* in x64, update on 32-bit register kills the whole register */
            if (!reg_is_read[i] &&
                instr_writes_to_exact_reg(instr,
                                          reg_64_to_32
                                          ((reg_id_t)(DR_REG_START_GPR + i)))) {
                return (reg_id_t)(DR_REG_START_GPR + i);
            }
#endif
        }
    }
    return DR_REG_NULL;
}

/* iterate basic block to check if aflags are dead after (including) where */
static bool
bb_aflags_are_dead(instrlist_t *ilist, instr_t *where)
{
    instr_t *instr;
    uint flags;

    for (instr = where; instr != NULL; instr = instr_get_next(instr)) {
        flags = instr_get_arith_flags(instr);
        if (TESTANY(EFLAGS_READ_6, flags))
            return false;
        if (TESTALL(EFLAGS_WRITE_6, flags))
            return true;
    }
    return false;
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *first = instrlist_first(bb);
    app_pc   pc    = dr_fragment_app_pc(tag);
    instr_t *mov1, *mov2;
    /* We try to avoid register stealing by using "dead" register if possible.
     * However, technically, a fault could come in and want the original value
     * of the "dead" register, but that's too corner-case for us.
     */
    reg_id_t reg   = bb_find_dead_reg(bb);
    bool     steal = (reg == DR_REG_NULL);

    if (reg == DR_REG_NULL)
        reg = DR_REG_XCX; /* randomly use one if no dead reg found */

    /* save register if necessary */
    if (steal)
        dr_save_reg(drcontext, bb, first, reg, SPILL_SLOT_1);

    /* load buffer pointer from TLS field */
    MINSERT(bb, first, INSTR_CREATE_mov_ld
            (drcontext,
             opnd_create_reg(reg),
             opnd_create_far_base_disp(tls_seg, DR_REG_NULL, DR_REG_NULL,
                                       0, tls_offs, OPSZ_PTR)));

    /* store bb's start pc into the buffer */
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc,
                                     OPND_CREATE_MEMPTR(reg, 0),
                                     bb, first, &mov1, &mov2);
    DR_ASSERT(mov1 != NULL);
    instr_set_ok_to_mangle(mov1, false);
    if (mov2 != NULL)
        instr_set_ok_to_mangle(mov2, false);

    /* update the TLS buffer pointer by incrementing just the bottom 16 bits of
     * the pointer
     */
    if (bb_aflags_are_dead(bb, first)) {
        /* if aflags are dead, we use add directly */
        MINSERT(bb, first, INSTR_CREATE_add
                (drcontext,
                 opnd_create_far_base_disp(tls_seg, DR_REG_NULL, DR_REG_NULL,
                                           0, tls_offs, OPSZ_2),
                 OPND_CREATE_INT8(sizeof(app_pc))));
    } else {
        reg_id_t reg_16;
#ifdef X64
        reg_16 = reg_32_to_16(reg_64_to_32(reg));
#else
        reg_16 = reg_32_to_16(reg);
#endif
        /* we use lea to avoid aflags save/restore */
        MINSERT(bb, first, INSTR_CREATE_lea
                (drcontext,
                 opnd_create_reg(reg_16),
                 opnd_create_base_disp(reg, DR_REG_NULL, 0,
                                       sizeof(app_pc), OPSZ_lea)));
        MINSERT(bb, first, INSTR_CREATE_mov_st
                (drcontext,
                 opnd_create_far_base_disp(tls_seg, DR_REG_NULL, DR_REG_NULL,
                                           0, tls_offs, OPSZ_PTR),
                 opnd_create_reg(reg)));
    }

    /* restore register if necessary */
    if (steal)
        dr_restore_reg(drcontext, bb, first, reg, SPILL_SLOT_1);

    return DR_EMIT_DEFAULT;
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = dr_thread_alloc(drcontext, sizeof(*data));

    DR_ASSERT(data != NULL);
    dr_set_tls_field(drcontext, data);
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
    per_thread_t *data = dr_get_tls_field(drcontext);
    dr_raw_mem_free(data->buf_base, TLS_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(*data));
}

static void
event_exit(void)
{
    if (!dr_raw_tls_cfree(tls_offs, 1))
        DR_ASSERT(false);
}

DR_EXPORT void
dr_init(client_id_t id)
{
    /* register events */
    dr_register_thread_init_event(event_thread_init);
    dr_register_thread_exit_event(event_thread_exit);
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    /* The TLS field provided by DR cannot be directly accessed from code cache.
     * For better performance, we allocate raw TLS so that we can directly
     * access and update it with a single instruction.
     */
    if(!dr_raw_tls_calloc(&tls_seg, &tls_offs, 1, 0))
        DR_ASSERT(false);
}

