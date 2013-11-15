/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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

/* DynamoRio eXtension utilities */

#include "dr_api.h"
#include "drx.h"

/* We use drmgr but only internally.  A user of drx will end up loading in
 * the drmgr library, but it won't affect the user's code.
 */
#include "drmgr.h"

#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
# define ASSERT(x, msg) /* nothing */
#endif /* DEBUG */

#define MINSERT instrlist_meta_preinsert
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)
#define TEST TESTANY
#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)
#define ALIGN_FORWARD(x, alignment)  \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_BACKWARD(x, alignment) \
    (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))

/* Reserved note range values */
enum {
    DRX_NOTE_AFLAGS_RESTORE_BEGIN,
    DRX_NOTE_AFLAGS_RESTORE_SAHF,
    DRX_NOTE_AFLAGS_RESTORE_END,
    DRX_NOTE_COUNT,
};
static ptr_uint_t note_base;
#define NOTE_VAL(enum_val) ((void *)(ptr_int_t)(note_base + (enum_val)))

/***************************************************************************
 * INIT
 */

static int drx_init_count;

DR_EXPORT
bool
drx_init(void)
{
    int count = dr_atomic_add32_return_sum(&drx_init_count, 1);
    if (count > 1)
        return true;

    drmgr_init();
    note_base = drmgr_reserve_note_range(DRX_NOTE_COUNT);
    ASSERT(note_base != DRMGR_NOTE_NONE, "failed to reserve note range");

    return true;
}

DR_EXPORT
void
drx_exit()
{
    int count = dr_atomic_add32_return_sum(&drx_init_count, -1);
    if (count != 0)
        return;

    drmgr_exit();
}


/***************************************************************************
 * INSTRUCTION NOTE FIELD
 */

/* For historical reasons we have this routine exported by drx.
 * We just forward to drmgr.
 */
DR_EXPORT
ptr_uint_t
drx_reserve_note_range(size_t size)
{
    return drmgr_reserve_note_range(size);
}

/***************************************************************************
 * ANALYSIS
 */

DR_EXPORT
bool
drx_aflags_are_dead(instr_t *where)
{
    instr_t *instr;
    uint flags;
    for (instr = where; instr != NULL; instr = instr_get_next(instr)) {
        /* we treat syscall/interrupt as aflags read */
        if (instr_is_syscall(instr) || instr_is_interrupt(instr))
            return false;
        flags = instr_get_arith_flags(instr);
        if (TESTANY(EFLAGS_READ_6, flags))
            return false;
        if (TESTALL(EFLAGS_WRITE_6, flags))
            return true;
        if (instr_is_cti(instr)) {
            if (instr_ok_to_mangle(instr) &&
                (instr_is_ubr(instr) || instr_is_call_direct(instr))) {
                instr_t *next = instr_get_next(instr);
                opnd_t   tgt  = instr_get_target(instr);
                /* continue on elision */
                if (next != NULL && instr_ok_to_mangle(next) &&
                    opnd_is_pc(tgt) &&
                    opnd_get_pc(tgt) == instr_get_app_pc(next))
                    continue;
            }
            /* unknown target, assume aflags is live */
            return false;
        }
    }
    return false;
}


/***************************************************************************
 * INSTRUMENTATION
 */

/* insert a label instruction with note */
static void
ilist_insert_note_label(void *drcontext, instrlist_t *ilist, instr_t *where,
                        void *note)
{
    instr_t *instr = INSTR_CREATE_label(drcontext);
    instr_set_note(instr, note);
    MINSERT(ilist, where, instr);
}

/* insert arithmetic flags saving code with more control
 * - skip %eax save if !save_eax
 * - save %eax to reg if reg is not DR_REG_NULL,
 * - save %eax to slot otherwise
 */
static void
drx_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                     bool save_eax, bool save_oflag,
                     dr_spill_slot_t slot, reg_id_t reg)
{
    instr_t *instr;

    /* save %eax if necessary */
    if (save_eax) {
        if (reg != DR_REG_NULL) {
            ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR &&
                   reg != DR_REG_XAX, "wrong dead reg");
            MINSERT(ilist, where,
                    INSTR_CREATE_mov_st(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_reg(DR_REG_XAX)));
        } else {
            ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX,
                   "wrong spill slot");
            dr_save_reg(drcontext, ilist, where, DR_REG_XAX, slot);
        }
    }
    /* lahf */
    instr = INSTR_CREATE_lahf(drcontext);
    MINSERT(ilist, where, instr);
    if (save_oflag) {
        /* seto %al */
        instr = INSTR_CREATE_setcc(drcontext, OP_seto, opnd_create_reg(DR_REG_AL));
        MINSERT(ilist, where, instr);
    }
}

/* insert arithmetic flags restore code with more control
 * - skip %eax restore if !restore_eax
 * - restore %eax from reg if reg is not DR_REG_NULL
 * - restore %eax from slot otherwise
 *
 * Routine merge_prev_drx_aflags_switch looks for labels inserted by
 * drx_restore_arith_flags, so changes to this routine may affect
 * merge_prev_drx_aflags_switch.
 */
static void
drx_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                        bool restore_eax, bool restore_oflag,
                        dr_spill_slot_t slot, reg_id_t reg)
{
    instr_t *instr;
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN));
    if (restore_oflag) {
        /* add 0x7f, %al */
        instr = INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_AL),
                                 OPND_CREATE_INT8(0x7f));
        MINSERT(ilist, where, instr);
    }
    /* sahf */
    instr = INSTR_CREATE_sahf(drcontext);
    instr_set_note(instr, NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_SAHF));
    MINSERT(ilist, where, instr);
    /* restore eax if necessary */
    if (restore_eax) {
        if (reg != DR_REG_NULL) {
            ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR &&
                   reg != DR_REG_XAX, "wrong dead reg");
            MINSERT(ilist, where,
                    INSTR_CREATE_mov_st(drcontext,
                                        opnd_create_reg(DR_REG_XAX),
                                        opnd_create_reg(reg)));
        } else {
            ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX,
                   "wrong spill slot");
            dr_restore_reg(drcontext, ilist, where, DR_REG_XAX, slot);
        }
    }
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_END));
}

/* Check if current instrumentation can be merged into previous aflags
 * save/restore inserted by drx_restore_arith_flags.
 * Returns NULL if cannot merge. Otherwise, returns the right insertion point,
 * i.e., DRX_NOTE_AFLAGS_RESTORE_BEGIN label instr.
 *
 * This routine looks for labels inserted by drx_restore_arith_flags,
 * so changes to drx_restore_arith_flags may affect this routine.
 */
static instr_t *
merge_prev_drx_aflags_switch(instr_t *where)
{
    instr_t *instr;
#ifdef DEBUG
    bool has_sahf = false;
#endif

    if (where == NULL)
        return NULL;
    instr = instr_get_prev(where);
    if (instr == NULL)
        return NULL;
    if (!instr_is_label(instr))
        return NULL;
    /* Check if prev instr is DRX_NOTE_AFLAGS_RESTORE_END.
     * We bail even there is only a label instr in between, which
     * might be a target of internal cti.
     */
    if (instr_get_note(instr) != NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_END))
        return NULL;

    /* find DRX_NOTE_AFLAGS_RESTORE_BEGIN */
    for (instr  = instr_get_prev(instr);
         instr != NULL;
         instr  = instr_get_prev(instr)) {
        if (instr_ok_to_mangle(instr)) {
            /* we do not expect any app instr */
            ASSERT(false, "drx aflags restore is corrupted");
            return NULL;
        }
        if (instr_is_label(instr)) {
            if (instr_get_note(instr) == NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN)) {
                ASSERT(has_sahf, "missing sahf");
                return instr;
            }
            /* we do not expect any other label instr */
            ASSERT(false, "drx aflags restore is corrupted");
            return NULL;
#ifdef DEBUG
        } else {
            if (instr_get_note(instr) == NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_SAHF))
                has_sahf = true;
#endif
        }
    }
    return NULL;
}

static bool
counter_crosses_cache_line(byte *addr, size_t size)
{
    size_t cache_line_size = proc_get_cache_line_size();
    if (ALIGN_BACKWARD(addr, cache_line_size) ==
        ALIGN_BACKWARD(addr+size-1, cache_line_size))
        return false;
    return true;
}

DR_EXPORT
bool
drx_insert_counter_update(void *drcontext, instrlist_t *ilist, instr_t *where,
                          dr_spill_slot_t slot, void *addr, int value,
                          uint flags)
{
    instr_t *instr;
    bool save_aflags = !drx_aflags_are_dead(where);
    bool is_64 = TEST(DRX_COUNTER_64BIT, flags);

    if (drcontext == NULL) {
        ASSERT(false, "drcontext cannot be NULL");
        return false;
    }
    if (!(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX)) {
        ASSERT(false, "wrong spill slot");
        return false;
    }

    /* check whether we can add lock */
    if (TEST(DRX_COUNTER_LOCK, flags)) {
        if (IF_NOT_X64(is_64 ||) /* 64-bit counter in 32-bit mode */
            counter_crosses_cache_line((byte *)addr, is_64 ? 8 : 4))
            return false;
    }

    /* if save_aflags, check if we can merge with the prev aflags save */
    if (save_aflags) {
        instr = merge_prev_drx_aflags_switch(where);
        if (instr != NULL) {
            save_aflags = false;
            where = instr;
        }
    }

    /* save aflags if necessary */
    if (save_aflags) {
        drx_save_arith_flags(drcontext, ilist, where,
                             true /* save eax */, true /* save oflag */,
                             slot, DR_REG_NULL);
    }
    /* update counter */
    instr = INSTR_CREATE_add(drcontext,
                             OPND_CREATE_ABSMEM
                             (addr, IF_X64_ELSE((is_64 ? OPSZ_8 : OPSZ_4), OPSZ_4)),
                             OPND_CREATE_INT32(value));
    if (TEST(DRX_COUNTER_LOCK, flags))
        instr = LOCK(instr);
    MINSERT(ilist, where, instr);
            
#ifndef X64
    if (is_64) {
        MINSERT(ilist, where,
                INSTR_CREATE_adc(drcontext,
                                 OPND_CREATE_ABSMEM
                                 ((void *)((ptr_int_t)addr + 4), OPSZ_4),
                                 OPND_CREATE_INT32(0)));
    }
#endif /* !X64 */
    /* restore aflags if necessary */
    if (save_aflags) {
        drx_restore_arith_flags(drcontext, ilist, where,
                                true /* restore eax */, true /* restore oflag */,
                                slot, DR_REG_NULL);
    }
    return true;
}

