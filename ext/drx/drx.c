/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
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
#include "hashtable.h"
#include "../ext_utils.h"
#include <stddef.h> /* for offsetof */
#include <string.h>

/* We use drmgr but only internally.  A user of drx will end up loading in
 * the drmgr library, but it won't affect the user's code.
 */
#include "drmgr.h"

#include "drreg.h"

#ifdef UNIX
#    ifdef LINUX
#        include "../../core/unix/include/syscall.h"
#    else
#        include <sys/syscall.h>
#    endif
#    include <signal.h> /* SIGKILL */
#endif

#include <limits.h>

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* nothing */
#    define IF_DEBUG(x)    /* nothing */
#endif                     /* DEBUG */

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) (x)
#else
#    define IF_WINDOWS_ELSE(x, y) (y)
#endif

#ifdef X86
/* TODO i#3837: Add AArch64 support. */
#    define PLATFORM_SUPPORTS_SCATTER_GATHER
#endif

#define MINSERT instrlist_meta_preinsert

#define VERBOSE 0

/* Reserved note range values */
enum {
    DRX_NOTE_AFLAGS_RESTORE_BEGIN,
    DRX_NOTE_AFLAGS_RESTORE_SAHF,
    DRX_NOTE_AFLAGS_RESTORE_END,
    DRX_NOTE_COUNT,
};

static ptr_uint_t note_base;
#define NOTE_VAL(enum_val) ((void *)(ptr_int_t)(note_base + (enum_val)))

static bool soft_kills_enabled;

static void
soft_kills_exit(void);

/* For debugging */
static uint verbose = 0;

#undef NOTIFY
#define NOTIFY(n, ...)                       \
    do {                                     \
        if (verbose >= (n)) {                \
            dr_fprintf(STDERR, __VA_ARGS__); \
        }                                    \
    } while (0)

/* defined in drx_buf.c */
bool
drx_buf_init_library(void);
void
drx_buf_exit_library(void);

/***************************************************************************
 * INIT
 */

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
bool
drx_scatter_gather_init();
void
drx_scatter_gather_exit();
#endif

static int drx_init_count;

DR_EXPORT
bool
drx_init(void)
{
    /* drx_insert_counter_update() needs 1 slot on x86 plus the 1 slot
     * drreg uses for aflags, and 2 reg slots on aarch, so 2 on both.
     * drx_expand_scatter_gather() needs 4 slots in app2app phase, which
     * cannot be reused by other phases. So, ideally we should reserve
     * 6 slots for drx. But we settle with 4 to avoid stealing too many
     * slots from other clients/libs. When drx needs more for instrumenting
     * scatter/gather instrs, we fall back on DR slots. As scatter/gather
     * instrs are spilt into their own bbs, this effect will be limited.
     * On Windows however we reserve even fewer slots, as they are
     * shared with the application and reserving even one slot can result
     * in failure to initialize for certain applications (e.g. i#1163).
     * On Linux, we set do_not_sum_slots to false so that we get at least
     * as many slots for drx use.
     */
    drreg_options_t ops = { sizeof(ops), IF_WINDOWS_ELSE(2, 4), false, NULL,
                            IF_WINDOWS_ELSE(true, false) };

    int count = dr_atomic_add32_return_sum(&drx_init_count, 1);
    if (count > 1)
        return true;

    drmgr_init();
    note_base = drmgr_reserve_note_range(DRX_NOTE_COUNT);
    ASSERT(note_base != DRMGR_NOTE_NONE, "failed to reserve note range");

    if (drreg_init(&ops) != DRREG_SUCCESS)
        return false;

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    if (!drx_scatter_gather_init())
        return false;
#endif

    return drx_buf_init_library();
}

DR_EXPORT
void
drx_exit()
{
    int count = dr_atomic_add32_return_sum(&drx_init_count, -1);
    if (count != 0)
        return;

    if (soft_kills_enabled) {
        soft_kills_exit();
        soft_kills_enabled = false;
    }
#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    drx_scatter_gather_exit();
#endif
    drx_buf_exit_library();
    drreg_exit();
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
    bool dead = false;
    IF_DEBUG(drreg_status_t res =)
    drreg_are_aflags_dead(dr_get_current_drcontext(), where, &dead);
    ASSERT(res == DRREG_SUCCESS, "drreg_are_aflags_dead failed!");
    return dead;
}

/***************************************************************************
 * INSTRUMENTATION
 */

#ifdef AARCHXX
/* XXX i#1603: add liveness analysis and pick dead regs */
#    define SCRATCH_REG0 DR_REG_R0
#    define SCRATCH_REG1 DR_REG_R1
#endif

/* insert a label instruction with note */
static void
ilist_insert_note_label(void *drcontext, instrlist_t *ilist, instr_t *where, void *note)
{
    instr_t *instr = INSTR_CREATE_label(drcontext);
    instr_set_note(instr, note);
    MINSERT(ilist, where, instr);
}

#ifdef X86 /* not yet used on ARM but we may export */

/* Insert arithmetic flags saving code with more control.
 * For x86:
 * - skip %eax save if !save_reg
 * - save %eax to reg if reg is not DR_REG_NULL,
 * - save %eax to slot otherwise
 * For ARM:
 * - saves flags to reg
 * - saves reg first to slot, unless !save_reg.
 */
static void
drx_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where, bool save_reg,
                     bool save_oflag, dr_spill_slot_t slot, reg_id_t reg)
{
#    ifdef X86
    instr_t *instr;
    /* save %eax if necessary */
    if (save_reg) {
        if (reg != DR_REG_NULL) {
            ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR && reg != DR_REG_XAX,
                   "wrong dead reg");
            MINSERT(ilist, where,
                    INSTR_CREATE_mov_st(drcontext, opnd_create_reg(reg),
                                        opnd_create_reg(DR_REG_XAX)));
        } else {
            ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX, "wrong spill slot");
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
#    elif defined(AARCHXX)
    ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR, "reg must be a GPR");
    if (save_reg) {
        ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX, "wrong spill slot");
        dr_save_reg(drcontext, ilist, where, reg, slot);
    }
    MINSERT(ilist, where,
            INSTR_CREATE_msr(drcontext, opnd_create_reg(DR_REG_CPSR),
                             OPND_CREATE_INT_MSR_NZCVQG(), opnd_create_reg(reg)));
#    endif
}

/* Insert arithmetic flags restore code with more control.
 * For x86:
 * - skip %eax restore if !restore_reg
 * - restore %eax from reg if reg is not DR_REG_NULL
 * - restore %eax from slot otherwise
 * For ARM:
 * - restores flags from reg
 * - restores reg to slot, unless !restore_reg.
 * Routine merge_prev_drx_aflags_switch looks for labels inserted by
 * drx_restore_arith_flags, so changes to this routine may affect
 * merge_prev_drx_aflags_switch.
 */
static void
drx_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                        bool restore_reg, bool restore_oflag, dr_spill_slot_t slot,
                        reg_id_t reg)
{
    instr_t *instr;
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN));
#    ifdef X86
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
    if (restore_reg) {
        if (reg != DR_REG_NULL) {
            ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR && reg != DR_REG_XAX,
                   "wrong dead reg");
            MINSERT(ilist, where,
                    INSTR_CREATE_mov_st(drcontext, opnd_create_reg(DR_REG_XAX),
                                        opnd_create_reg(reg)));
        } else {
            ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX, "wrong spill slot");
            dr_restore_reg(drcontext, ilist, where, DR_REG_XAX, slot);
        }
    }
#    elif defined(AARCHXX)
    ASSERT(reg >= DR_REG_START_GPR && reg <= DR_REG_STOP_GPR, "reg must be a GPR");
    instr =
        INSTR_CREATE_mrs(drcontext, opnd_create_reg(reg), opnd_create_reg(DR_REG_CPSR));
    instr_set_note(instr, NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_SAHF));
    MINSERT(ilist, where, instr);
    if (restore_reg) {
        ASSERT(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX, "wrong spill slot");
        dr_restore_reg(drcontext, ilist, where, reg, slot);
    }
#    endif
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_END));
}
#endif /* X86 */

/* Check if current instrumentation can be merged into previous aflags
 * (or on ARM, GPR) save/restore inserted by drx_restore_arith_flags.
 * Returns NULL if cannot merge. Otherwise, returns the right insertion point,
 * i.e., DRX_NOTE_AFLAGS_RESTORE_BEGIN label instr.
 *
 * This routine looks for labels inserted by drx_restore_arith_flags,
 * so changes to drx_restore_arith_flags may affect this routine.
 * On ARM the labels are from drx_insert_counter_update.
 */
static instr_t *
merge_prev_drx_spill(instrlist_t *ilist, instr_t *where, bool aflags)
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
    /* On ARM we do not want to merge two drx spills if they are
     * predicated differently.
     */
    if (instr_get_predicate(instr) != instrlist_get_auto_predicate(ilist))
        return NULL;

    /* find DRX_NOTE_AFLAGS_RESTORE_BEGIN */
    for (instr = instr_get_prev(instr); instr != NULL; instr = instr_get_prev(instr)) {
        if (instr_is_app(instr)) {
            /* we do not expect any app instr */
            ASSERT(false, "drx aflags restore is corrupted");
            return NULL;
        }
        if (instr_is_label(instr)) {
            if (instr_get_note(instr) == NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN)) {
                ASSERT(!aflags || has_sahf, "missing sahf");
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
        ALIGN_BACKWARD(addr + size - 1, cache_line_size))
        return false;
    return true;
}

DR_EXPORT
bool
drx_insert_counter_update(void *drcontext, instrlist_t *ilist, instr_t *where,
                          dr_spill_slot_t slot,
                          IF_AARCHXX_(dr_spill_slot_t slot2) void *addr, int value,
                          uint flags)
{
    instr_t *instr;
    bool use_drreg = false;
#ifdef X86
    bool save_aflags = true;
#elif defined(AARCHXX)
    bool save_regs = true;
    reg_id_t reg1, reg2;
#endif
    bool is_64 = TEST(DRX_COUNTER_64BIT, flags);
    /* Requires drx_init(), where it didn't when first added. */
    if (drx_init_count == 0) {
        ASSERT(false, "drx_insert_counter_update requires drx_init");
        return false;
    }
    if (drcontext == NULL) {
        ASSERT(false, "drcontext cannot be NULL");
        return false;
    }
    if (drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION) {
        use_drreg = true;
        if (drmgr_current_bb_phase(drcontext) == DRMGR_PHASE_INSERTION &&
            (slot != SPILL_SLOT_MAX + 1 IF_AARCHXX(|| slot2 != SPILL_SLOT_MAX + 1))) {
            ASSERT(false, "with drmgr, SPILL_SLOT_MAX+1 must be passed");
            return false;
        }
    } else if (!(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX)) {
        ASSERT(false, "wrong spill slot");
        return false;
    }

    /* check whether we can add lock */
    if (TEST(DRX_COUNTER_LOCK, flags)) {
#ifdef AARCHXX
        /* TODO i#1551,i#1569: implement for AArchXX. */
        ASSERT(false, "DRX_COUNTER_LOCK not implemented for AArchXX");
        return false;
#endif
        if (IF_NOT_X64(is_64 ||) /* 64-bit counter in 32-bit mode */
            counter_crosses_cache_line((byte *)addr, is_64 ? 8 : 4))
            return false;
    }

#ifdef X86
    if (use_drreg) {
        if (drreg_reserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            return false;
    } else {
        /* if save_aflags, check if we can merge with the prev aflags save */
        save_aflags = !drx_aflags_are_dead(where);
        if (save_aflags) {
            instr = merge_prev_drx_spill(ilist, where, true /*aflags*/);
            if (instr != NULL) {
                save_aflags = false;
                where = instr;
            }
        }
        /* save aflags if necessary */
        if (save_aflags) {
            drx_save_arith_flags(drcontext, ilist, where, true /* save eax */,
                                 true /* save oflag */, slot, DR_REG_NULL);
        }
    }
    /* update counter */
    instr = INSTR_CREATE_add(
        drcontext,
        OPND_CREATE_ABSMEM(addr, IF_X64_ELSE((is_64 ? OPSZ_8 : OPSZ_4), OPSZ_4)),
        OPND_CREATE_INT_32OR8(value));
    if (TEST(DRX_COUNTER_LOCK, flags))
        instr = LOCK(instr);
    /* On x86, for DRX_COUNTER_REL_ACQ, we don't need to do anything as the
     * ISA provides release-acquire semantics for regular stores and loads.
     */
    MINSERT(ilist, where, instr);

#    ifndef X64
    if (is_64) {
        MINSERT(ilist, where,
                INSTR_CREATE_adc(
                    drcontext, OPND_CREATE_ABSMEM((void *)((ptr_int_t)addr + 4), OPSZ_4),
                    OPND_CREATE_INT32(0)));
    }
#    endif /* !X64 */
    if (use_drreg) {
        if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            return false;
    } else {
        /* restore aflags if necessary */
        if (save_aflags) {
            drx_restore_arith_flags(drcontext, ilist, where, true /* restore eax */,
                                    true /* restore oflag */, slot, DR_REG_NULL);
        }
    }
#elif defined(AARCHXX)
#    ifdef ARM
    /* FIXME i#1551: implement 64-bit counter support */
    ASSERT(!is_64, "DRX_COUNTER_64BIT is not implemented for ARM_32");
#    endif /* ARM */

    if (use_drreg) {
        if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg1) !=
                DRREG_SUCCESS ||
            drreg_reserve_register(drcontext, ilist, where, NULL, &reg2) != DRREG_SUCCESS)
            return false;
    } else {
        reg1 = SCRATCH_REG0;
        reg2 = SCRATCH_REG1;
        /* merge w/ prior restore */
        if (save_regs) {
            instr = merge_prev_drx_spill(ilist, where, false /*!aflags*/);
            if (instr != NULL) {
                save_regs = false;
                where = instr;
            }
        }
        if (save_regs) {
            dr_save_reg(drcontext, ilist, where, reg1, slot);
            dr_save_reg(drcontext, ilist, where, reg2, slot2);
        }
    }
    /* XXX: another optimization is to look for the prior increment's
     * address being near this one, and add to reg1 instead of
     * taking 2 instrs to load it fresh.
     */
    /* Update the counter either with release-acquire semantics (when the
     * DRX_COUNTER_REL_ACQ flag is on) or without any barriers.
     */
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)addr, opnd_create_reg(reg1),
                                     ilist, where, NULL, NULL);
    if (TEST(DRX_COUNTER_REL_ACQ, flags)) {
#    ifdef AARCH64
        MINSERT(ilist, where,
                INSTR_CREATE_ldar(drcontext, opnd_create_reg(reg2),
                                  OPND_CREATE_MEMPTR(reg1, 0)));
        if (value >= 0) {
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(value)));
        } else {
            MINSERT(ilist, where,
                    XINST_CREATE_sub(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(-value)));
        }
        MINSERT(ilist, where,
                INSTR_CREATE_stlr(drcontext, OPND_CREATE_MEMPTR(reg1, 0),
                                  opnd_create_reg(reg2)));
#    else /* ARM */
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(reg2),
                                  OPND_CREATE_MEMPTR(reg1, 0)));
        MINSERT(ilist, where, INSTR_CREATE_dmb(drcontext, OPND_CREATE_INT(DR_DMB_ISH)));
        if (value >= 0) {
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(value)));
        } else {
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(-value)));
        }
        MINSERT(ilist, where, INSTR_CREATE_dmb(drcontext, OPND_CREATE_INT(DR_DMB_ISH)));
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg1, 0),
                                   opnd_create_reg(reg2)));
#    endif
    } else {
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(reg2),
                                  OPND_CREATE_MEMPTR(reg1, 0)));
        if (value >= 0) {
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(value)));
        } else {
            MINSERT(ilist, where,
                    XINST_CREATE_sub(drcontext, opnd_create_reg(reg2),
                                     OPND_CREATE_INT(-value)));
        }
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg1, 0),
                                   opnd_create_reg(reg2)));
    }
    if (use_drreg) {
        if (drreg_unreserve_register(drcontext, ilist, where, reg1) != DRREG_SUCCESS ||
            drreg_unreserve_register(drcontext, ilist, where, reg2) != DRREG_SUCCESS)
            return false;
    } else if (save_regs) {
        ilist_insert_note_label(drcontext, ilist, where,
                                NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN));
        dr_restore_reg(drcontext, ilist, where, reg2, slot2);
        dr_restore_reg(drcontext, ilist, where, reg1, slot);
        ilist_insert_note_label(drcontext, ilist, where,
                                NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_END));
    }
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented - below to make compiler happy */
    ASSERT(false, "Not implemented");
    instr = merge_prev_drx_spill(ilist, where, false);
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_BEGIN));
    ilist_insert_note_label(drcontext, ilist, where,
                            NOTE_VAL(DRX_NOTE_AFLAGS_RESTORE_END));
    return false;
#endif
    return true;
}

/***************************************************************************
 * SOFT KILLS
 */

/* Track callbacks in a simple list protected by a lock */
typedef struct _cb_entry_t {
    /* XXX: the bool return value is complex to support in some situations.  We
     * ignore the return value and always skip the app's termination of the
     * child process for jobs containing multiple pids and for
     * JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.  If we wanted to not skip those we'd
     * have to emulate the kill via NtTerminateProcess, which doesn't seem worth
     * it when our two use cases (DrMem and drcov) don't need that kind of
     * control.
     */
    bool (*cb)(process_id_t, int);
    struct _cb_entry_t *next;
} cb_entry_t;

static cb_entry_t *cb_list;
static void *cb_lock;

static bool
soft_kills_invoke_cbs(process_id_t pid, int exit_code)
{
    cb_entry_t *e;
    bool skip = false;
    NOTIFY(1, "--drx-- parent %d soft killing pid %d code %d\n", dr_get_process_id(), pid,
           exit_code);
    dr_mutex_lock(cb_lock);
    for (e = cb_list; e != NULL; e = e->next) {
        /* If anyone wants to skip, we skip */
        skip = e->cb(pid, exit_code) || skip;
    }
    dr_mutex_unlock(cb_lock);
    return skip;
}

#ifdef WINDOWS

/* The system calls we need to watch for soft kills.
 * These are are in ntoskrnl so we get away without drsyscall.
 */
enum {
    SYS_NUM_PARAMS_TerminateProcess = 2,
    SYS_NUM_PARAMS_TerminateJobObject = 2,
    SYS_NUM_PARAMS_SetInformationJobObject = 4,
    SYS_NUM_PARAMS_Close = 1,
    SYS_NUM_PARAMS_DuplicateObject = 7,
};

enum {
    SYS_WOW64_IDX_TerminateProcess = 0,
    SYS_WOW64_IDX_TerminateJobObject = 0,
    SYS_WOW64_IDX_SetInformationJobObject = 7,
    SYS_WOW64_IDX_Close = 0,
    SYS_WOW64_IDX_DuplicateObject = 0,
};

static int sysnum_TerminateProcess;
static int sysnum_TerminateJobObject;
static int sysnum_SetInformationJobObject;
static int sysnum_Close;
static int sysnum_DuplicateObject;

/* Table of job handles for which the app set JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE */
#    define JOB_TABLE_HASH_BITS 6
static hashtable_t job_table;

/* Entry in job_table.  If it is present in the table, it should only be
 * accessed while holding the table lock.
 */
typedef struct _job_info_t {
    /* So far just a reference count.  We don't need store a duplicated handle
     * b/c we always have a valid app handle for this job.
     */
    uint ref_count;
} job_info_t;

/* We need CLS as we track data across syscalls, where TLS is not sufficient */
static int cls_idx_soft;

typedef struct _cls_soft_t {
    /* For NtSetInformationJobObject */
    DWORD job_limit_flags_orig;
    DWORD *job_limit_flags_loc;
    /* For NtDuplicateObject */
    bool dup_proc_src_us;
    bool dup_proc_dst_us;
    ULONG dup_options;
    HANDLE dup_src;
    HANDLE *dup_dst;
    job_info_t *dup_jinfo;
    /* If we add data for more syscalls, we could use a union to save space */
} cls_soft_t;

/* XXX: should we have some kind of shared wininc/ dir for these common defines?
 * I don't really want to include core/win32/ntdll.h here.
 */

typedef LONG NTSTATUS;
#    define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)

/* Since we invoke only in a client/privlib context, we can statically link
 * with ntdll to call these syscall wrappers:
 */
#    define GET_NTDLL(NtFunction, signature) NTSYSAPI NTSTATUS NTAPI NtFunction signature

GET_NTDLL(NtQueryInformationJobObject,
          (IN HANDLE JobHandle, IN JOBOBJECTINFOCLASS JobInformationClass,
           OUT PVOID JobInformation, IN ULONG JobInformationLength,
           OUT PULONG ReturnLength OPTIONAL));

#    define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005L)

#    define NT_CURRENT_PROCESS ((HANDLE)(ptr_int_t)-1)

typedef LONG KPRIORITY;

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation,
} PROCESSINFOCLASS;

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    void *PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;
typedef PROCESS_BASIC_INFORMATION *PPROCESS_BASIC_INFORMATION;

GET_NTDLL(NtQueryInformationProcess,
          (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
           OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
           OUT PULONG ReturnLength OPTIONAL));
GET_NTDLL(NtTerminateProcess, (IN HANDLE ProcessHandle, IN NTSTATUS ExitStatus));

static ssize_t
num_job_object_pids(HANDLE job)
{
    /* i#1401: despite what Nebbett says and MSDN hints at, on Win7 at least
     * JobObjectBasicProcessIdList returning STATUS_BUFFER_OVERFLOW does NOT
     * fill in any data at all.  We thus have to query through a different
     * mechanism.
     */
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
    NTSTATUS res;
    DWORD len;
    res = NtQueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info,
                                      sizeof(info), &len);
    NOTIFY(1, "--drx-- job 0x%x => %d pids len=%d res=0x%08x\n", job,
           info.ActiveProcesses, len, res);
    if (NT_SUCCESS(res))
        return info.ActiveProcesses;
    else
        return -1;
}

static bool
get_job_object_pids(HANDLE job, JOBOBJECT_BASIC_PROCESS_ID_LIST *list, size_t list_sz)
{
    NTSTATUS res;
    res = NtQueryInformationJobObject(job, JobObjectBasicProcessIdList, list,
                                      (ULONG)list_sz, NULL);
    return NT_SUCCESS(res);
}

/* XXX: should DR provide a routine to query this? */
static bool
get_app_exit_code(int *exit_code)
{
    ULONG got;
    PROCESS_BASIC_INFORMATION info;
    NTSTATUS res;
    memset(&info, 0, sizeof(PROCESS_BASIC_INFORMATION));
    res = NtQueryInformationProcess(NT_CURRENT_PROCESS, ProcessBasicInformation, &info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);
    if (!NT_SUCCESS(res) || got != sizeof(PROCESS_BASIC_INFORMATION))
        return false;
    *exit_code = (int)info.ExitStatus;
    return true;
}

static void
soft_kills_context_init(void *drcontext, bool new_depth)
{
    cls_soft_t *cls;
    if (new_depth) {
        cls = (cls_soft_t *)dr_thread_alloc(drcontext, sizeof(*cls));
        drmgr_set_cls_field(drcontext, cls_idx_soft, cls);
    } else {
        cls = (cls_soft_t *)drmgr_get_cls_field(drcontext, cls_idx_soft);
    }
    memset(cls, 0, sizeof(*cls));
}

static void
soft_kills_context_exit(void *drcontext, bool thread_exit)
{
    if (thread_exit) {
        cls_soft_t *cls = (cls_soft_t *)drmgr_get_cls_field(drcontext, cls_idx_soft);
        dr_thread_free(drcontext, cls, sizeof(*cls));
    }
    /* else, nothing to do: we leave the struct for re-use on next callback */
}

static int
soft_kills_get_sysnum(const char *name, int num_params, int wow64_idx)
{
    static module_handle_t ntdll;
    app_pc wrapper;
    int sysnum;

    if (ntdll == NULL) {
        module_data_t *data = dr_lookup_module_by_name("ntdll.dll");
        if (data == NULL)
            return -1;
        ntdll = data->handle;
        dr_free_module_data(data);
    }
    wrapper = (app_pc)dr_get_proc_address(ntdll, name);
    if (wrapper == NULL)
        return -1;
    sysnum = drmgr_decode_sysnum_from_wrapper(wrapper);
    if (sysnum == -1)
        return -1;
    /* Ensure that DR intercepts these if we go native.
     * XXX: better to only do this if client plans to use native execution
     * to reduce the hook count and shrink chance of hook conflicts?
     */
    if (!dr_syscall_intercept_natively(name, sysnum, num_params, wow64_idx))
        return -1;
    return sysnum;
}

static void
soft_kills_handle_job_termination(void *drcontext, HANDLE job, int exit_code)
{
    ssize_t num_jobs = num_job_object_pids(job);
    NOTIFY(1, "--drx-- for job 0x%x got %d jobs\n", job, num_jobs);
    if (num_jobs > 0) {
        JOBOBJECT_BASIC_PROCESS_ID_LIST *list;
        size_t sz = sizeof(*list) + (num_jobs - 1) * sizeof(list->ProcessIdList[0]);
        byte *buf = dr_thread_alloc(drcontext, sz);
        list = (JOBOBJECT_BASIC_PROCESS_ID_LIST *)buf;
        if (get_job_object_pids(job, list, sz)) {
            uint i;
            NOTIFY(1, "--drx-- for job 0x%x got %d jobs in list\n", job,
                   list->NumberOfProcessIdsInList);
            for (i = 0; i < list->NumberOfProcessIdsInList; i++) {
                process_id_t pid = list->ProcessIdList[i];
                if (!soft_kills_invoke_cbs(pid, exit_code)) {
                    /* Client is not terminating and requests not to skip the action.
                     * But since we have multiple pids, we go with a local decision
                     * here and emulate the kill.
                     */
                    HANDLE phandle = dr_convert_pid_to_handle(pid);
                    if (phandle != INVALID_HANDLE_VALUE)
                        NtTerminateProcess(phandle, exit_code);
                    /* else, child stays alive: not much we can do */
                }
            }
        }
        dr_thread_free(drcontext, buf, sz);
    } /* else query failed: I'd issue a warning log msg if not inside drx library */
}

static void
soft_kills_free_job_info(void *ptr)
{
    job_info_t *jinfo = (job_info_t *)ptr;
    if (jinfo->ref_count == 0)
        dr_global_free(jinfo, sizeof(*jinfo));
}

/* Called when the app closes a job handle "job".
 * Caller must hold job_table lock.
 * If "remove" is true, removes from the hashtable and de-allocates "jinfo",
 * if refcount is 0.
 */
static void
soft_kills_handle_close(void *drcontext, job_info_t *jinfo, HANDLE job, int exit_code,
                        bool remove)
{
    ASSERT(jinfo->ref_count > 0, "invalid ref count");
    jinfo->ref_count--;
    if (jinfo->ref_count == 0) {
        NOTIFY(1, "--drx-- closing kill-on-close handle 0x%x in pid %d\n", job,
               dr_get_process_id());
        /* XXX: It's possible for us to miss a handle being closed from another
         * process.  In such a case, our ref count won't reach 0 and we'll
         * fail to kill the child at all.
         * If that handle value is re-used as a job object (else our job queryies
         * will get STATUS_OBJECT_TYPE_MISMATCH) with no kill-on-close, we could
         * incorrectly kill a job when the app is just closing its handle, but
         * this would only happen when a job is being controlled from multiple
         * processes.  We'll have to live with the risk.  We could watch
         * NtCreateJobObject but it doesn't seem worth it.
         */
        soft_kills_handle_job_termination(drcontext, job, exit_code);
    }
    if (remove)
        hashtable_remove(&job_table, (void *)job);
}

static bool
soft_kills_filter_syscall(void *drcontext, int sysnum)
{
    return (sysnum == sysnum_TerminateProcess || sysnum == sysnum_TerminateJobObject ||
            sysnum == sysnum_SetInformationJobObject || sysnum == sysnum_Close ||
            sysnum == sysnum_DuplicateObject);
}

static bool
soft_kills_pre_SetInformationJobObject(void *drcontext, cls_soft_t *cls)
{
    HANDLE job = (HANDLE)dr_syscall_get_param(drcontext, 0);
    JOBOBJECTINFOCLASS class = (JOBOBJECTINFOCLASS)dr_syscall_get_param(drcontext, 1);
    ULONG sz = (ULONG)dr_syscall_get_param(drcontext, 3);
    /* MSDN claims that JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE requires an
     * extended info struct, which we trust, though it seems odd as it's
     * a flag in the basic struct.
     */
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    if (class == JobObjectExtendedLimitInformation && sz >= sizeof(info) &&
        dr_safe_read((byte *)dr_syscall_get_param(drcontext, 2), sizeof(info), &info,
                     NULL)) {
        if (TEST(JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
                 info.BasicLimitInformation.LimitFlags)) {
            /* Remove the kill-on-close flag from the syscall arg.
             * We restore in post-syscall in case app uses the memory
             * for something else.  There is of course a race where another
             * thread could use it and get the wrong value: -soft_kills isn't
             * perfect.
             */
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION *ptr =
                (JOBOBJECT_EXTENDED_LIMIT_INFORMATION *)dr_syscall_get_param(drcontext,
                                                                             2);
            ULONG new_flags = info.BasicLimitInformation.LimitFlags &
                (~JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE);
            bool isnew;
            job_info_t *jinfo;
            cls->job_limit_flags_orig = info.BasicLimitInformation.LimitFlags;
            cls->job_limit_flags_loc = &ptr->BasicLimitInformation.LimitFlags;
            ASSERT(sizeof(cls->job_limit_flags_orig) ==
                       sizeof(ptr->BasicLimitInformation.LimitFlags),
                   "size mismatch");
            if (!dr_safe_write(cls->job_limit_flags_loc,
                               sizeof(ptr->BasicLimitInformation.LimitFlags), &new_flags,
                               NULL)) {
                /* XXX: Any way we can send a WARNING on our failure to write? */
                NOTIFY(1,
                       "--drx-- FAILED to remove kill-on-close from job 0x%x "
                       "in pid %d\n",
                       job, dr_get_process_id());
            } else {
                NOTIFY(1, "--drx-- removed kill-on-close from job 0x%x in pid %d\n", job,
                       dr_get_process_id());
            }
            /* Track the handle so we can notify the client on close or exit */
            hashtable_lock(&job_table);
            /* See if already there (in case app called Set 2x) */
            if (hashtable_lookup(&job_table, (void *)job) == NULL) {
                jinfo = (job_info_t *)dr_global_alloc(sizeof(*jinfo));
                jinfo->ref_count = 1;
                isnew = hashtable_add(&job_table, (void *)job, (void *)jinfo);
                ASSERT(isnew, "missed an NtClose");
            }
            hashtable_unlock(&job_table);
        }
    }
    return true;
}

/* We must do two things on NtDuplicateObject:
 * 1) Update our job table: adding a new entry for the duplicate,
 *    and removing the source handle if it is closed.
 * 2) Process a handle being closed but a new one not being
 *    created (in this process): corner case that triggers a kill.
 */
static bool
soft_kills_pre_DuplicateObject(void *drcontext, cls_soft_t *cls)
{
    HANDLE proc_src = (HANDLE)dr_syscall_get_param(drcontext, 0);
    process_id_t id_src = dr_convert_handle_to_pid(proc_src);
    cls->dup_proc_src_us = (id_src == dr_get_process_id());
    cls->dup_jinfo = NULL;
    if (cls->dup_proc_src_us) {
        /* NtDuplicateObject seems more likely than NtClose to fail, so we
         * shift as much handling as possible post-syscall.
         */
        HANDLE proc_dst = (HANDLE)dr_syscall_get_param(drcontext, 2);
        process_id_t id_dst = dr_convert_handle_to_pid(proc_dst);
        cls->dup_proc_dst_us = (id_dst == dr_get_process_id());
        cls->dup_src = (HANDLE)dr_syscall_get_param(drcontext, 1);
        cls->dup_dst = (HANDLE *)dr_syscall_get_param(drcontext, 3);
        cls->dup_options = (ULONG)dr_syscall_get_param(drcontext, 6);
        hashtable_lock(&job_table);
        /* We have to save jinfo b/c dup_src will be gone */
        cls->dup_jinfo = (job_info_t *)hashtable_lookup(&job_table, (void *)cls->dup_src);
        if (cls->dup_jinfo != NULL) {
            if (TEST(DUPLICATE_CLOSE_SOURCE, cls->dup_options)) {
                /* "This occurs regardless of any error status returned"
                 * according to MSDN DuplicateHandle, and Nebbett.
                 * Thus, we act on this here, which avoids any handle value
                 * reuse race, and we don't have to undo in post.
                 * If this weren't true, we'd have to reinstate in the table
                 * on failure, and we'd have to duplicate the handle
                 * (dr_dup_file_handle would do it -- at least w/ current impl)
                 * to call soft_kills_handle_close() in post.
                 */
                if (!cls->dup_proc_dst_us) {
                    NOTIFY(1, "--drx-- job 0x%x closed in pid %d w/ dst outside proc\n",
                           cls->dup_src, dr_get_process_id());
                    /* The exit code is set to 0 by the kernel for this case */
                    soft_kills_handle_close(drcontext, cls->dup_jinfo, cls->dup_src, 0,
                                            true /*remove*/);
                } else {
                    hashtable_remove(&job_table, (void *)cls->dup_src);
                    /* Adjust refcount after removing to avoid freeing prematurely.
                     * The refcount may be sitting at 0, but no other thread should
                     * be able to affect it as there is no hashtable entry.
                     */
                    ASSERT(cls->dup_jinfo->ref_count > 0, "invalid ref count");
                    cls->dup_jinfo->ref_count--;
                }
            }
        }
        hashtable_unlock(&job_table);
    }
    return true;
}

static void
soft_kills_post_DuplicateObject(void *drcontext)
{
    cls_soft_t *cls = (cls_soft_t *)drmgr_get_cls_field(drcontext, cls_idx_soft);
    HANDLE dup_dst;
    if (cls->dup_jinfo == NULL)
        return;
    if (!NT_SUCCESS(dr_syscall_get_result(drcontext)))
        return;
    ASSERT(cls->dup_proc_src_us, "shouldn't get here");
    if (!cls->dup_proc_dst_us)
        return; /* already handled in pre */
    /* At this point we have a successful intra-process duplication.  If
     * DUPLICATE_CLOSE_SOURCE, we already removed from the table in pre.
     */
    hashtable_lock(&job_table);
    if (cls->dup_dst != NULL &&
        dr_safe_read(cls->dup_dst, sizeof(dup_dst), &dup_dst, NULL)) {
        NOTIFY(1, "--drx-- job 0x%x duplicated as 0x%x in pid %d\n", cls->dup_src,
               dup_dst, dr_get_process_id());
        cls->dup_jinfo->ref_count++;
        hashtable_add(&job_table, (void *)dup_dst, (void *)cls->dup_jinfo);
    }
    hashtable_unlock(&job_table);
}

/* Returns whether to execute the system call */
static bool
soft_kills_pre_syscall(void *drcontext, int sysnum)
{
    cls_soft_t *cls = (cls_soft_t *)drmgr_get_cls_field(drcontext, cls_idx_soft);
    /* Xref DrMem i#544, DrMem i#1297, and DRi#1231: give child
     * processes a chance for clean exit for dumping of data or other
     * actions.
     *
     * XXX: a child under DR but not a supporting client will be left
     * alive: but that's a risk we can live with.
     */
    if (sysnum == sysnum_TerminateProcess) {
        HANDLE proc = (HANDLE)dr_syscall_get_param(drcontext, 0);
        process_id_t pid = dr_convert_handle_to_pid(proc);
        if (pid != INVALID_PROCESS_ID && pid != dr_get_process_id()) {
            int exit_code = (int)dr_syscall_get_param(drcontext, 1);
            NOTIFY(1, "--drx-- NtTerminateProcess in pid %d\n", dr_get_process_id());
            if (soft_kills_invoke_cbs(pid, exit_code)) {
                dr_syscall_set_result(drcontext, 0 /*success*/);
                return false; /* skip syscall */
            } else
                return true; /* execute syscall */
        }
    } else if (sysnum == sysnum_TerminateJobObject) {
        /* There are several ways a process in a job can be killed:
         *
         *   1) NtTerminateJobObject
         *   2) The last handle is closed + JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE is set
         *   3) JOB_OBJECT_LIMIT_ACTIVE_PROCESS is hit
         *   4) Time limit and JOB_OBJECT_TERMINATE_AT_END_OF_JOB is hit
         *
         * XXX: we only handle #1 and #2.
         */
        HANDLE job = (HANDLE)dr_syscall_get_param(drcontext, 0);
        NTSTATUS exit_code = (NTSTATUS)dr_syscall_get_param(drcontext, 1);
        NOTIFY(1, "--drx-- NtTerminateJobObject job 0x%x in pid %d\n", job,
               dr_get_process_id());
        soft_kills_handle_job_termination(drcontext, job, exit_code);
        /* We always skip this syscall.  If individual processes were requested
         * to not be skipped, we emulated via NtTerminateProcess in
         * soft_kills_handle_job_termination().
         */
        dr_syscall_set_result(drcontext, 0 /*success*/);
        return false; /* skip syscall */
    } else if (sysnum == sysnum_SetInformationJobObject) {
        return soft_kills_pre_SetInformationJobObject(drcontext, cls);
    } else if (sysnum == sysnum_Close) {
        /* If a job object, act on it, and remove from our table */
        HANDLE handle = (HANDLE)dr_syscall_get_param(drcontext, 0);
        job_info_t *jinfo;
        hashtable_lock(&job_table);
        jinfo = (job_info_t *)hashtable_lookup(&job_table, (void *)handle);
        if (jinfo != NULL) {
            NOTIFY(1, "--drx-- explicit close of job 0x%x in pid %d\n", handle,
                   dr_get_process_id());
            /* The exit code is set to 0 by the kernel for this case */
            soft_kills_handle_close(drcontext, jinfo, handle, 0, true /*remove*/);
        }
        hashtable_unlock(&job_table);
    } else if (sysnum == sysnum_DuplicateObject) {
        return soft_kills_pre_DuplicateObject(drcontext, cls);
    }
    return true;
}

static void
soft_kills_post_syscall(void *drcontext, int sysnum)
{
    if (sysnum == sysnum_SetInformationJobObject) {
        cls_soft_t *cls = (cls_soft_t *)drmgr_get_cls_field(drcontext, cls_idx_soft);
        if (cls->job_limit_flags_loc != NULL) {
            /* Restore the app's memory */
            if (!dr_safe_write(cls->job_limit_flags_loc,
                               sizeof(cls->job_limit_flags_orig),
                               &cls->job_limit_flags_orig, NULL)) {
                /* If we weren't in drx lib I'd log a warning */
            }
            cls->job_limit_flags_loc = NULL;
        }
    } else if (sysnum == sysnum_DuplicateObject) {
        soft_kills_post_DuplicateObject(drcontext);
    }
}
#else /* WINDOWS */

static bool
soft_kills_filter_syscall(void *drcontext, int sysnum)
{
    return (sysnum == SYS_kill);
}

/* Returns whether to execute the system call */
static bool
soft_kills_pre_syscall(void *drcontext, int sysnum)
{
    if (sysnum == SYS_kill) {
        process_id_t pid = (process_id_t)dr_syscall_get_param(drcontext, 0);
        int sig = (int)dr_syscall_get_param(drcontext, 1);
        if (sig == SIGKILL && pid != INVALID_PROCESS_ID && pid != dr_get_process_id()) {
            /* Pass exit code << 8 for use with dr_exit_process() */
            int exit_code = sig << 8;
            if (soft_kills_invoke_cbs(pid, exit_code)) {
                /* set result to 0 (success) and use_high and use_errno to false */
                dr_syscall_result_info_t info = {
                    sizeof(info),
                };
                info.succeeded = true;
                dr_syscall_set_result_ex(drcontext, &info);
                return false; /* skip syscall */
            } else
                return true; /* execute syscall */
        }
    }
    return true;
}

static void
soft_kills_post_syscall(void *drcontext, int sysnum)
{
    /* nothing yet */
}

#endif /* UNIX */

static bool
soft_kills_init(void)
{
#ifdef WINDOWS
    IF_DEBUG(bool ok;)
#endif
    /* XXX: would be nice to fail if it's not still process init,
     * but we don't have an easy way to check.
     */
    soft_kills_enabled = true;

    NOTIFY(1, "--drx-- init pid %d %s\n", dr_get_process_id(), dr_get_application_name());

    cb_lock = dr_mutex_create();

#ifdef WINDOWS
    hashtable_init_ex(&job_table, JOB_TABLE_HASH_BITS, HASH_INTPTR, false /*!strdup*/,
                      false /*!synch*/, soft_kills_free_job_info, NULL, NULL);

    sysnum_TerminateProcess =
        soft_kills_get_sysnum("NtTerminateProcess", SYS_NUM_PARAMS_TerminateProcess,
                              SYS_WOW64_IDX_TerminateProcess);
    if (sysnum_TerminateProcess == -1)
        return false;
    sysnum_TerminateJobObject =
        soft_kills_get_sysnum("NtTerminateJobObject", SYS_NUM_PARAMS_TerminateJobObject,
                              SYS_WOW64_IDX_TerminateJobObject);
    if (sysnum_TerminateJobObject == -1)
        return false;
    sysnum_SetInformationJobObject = soft_kills_get_sysnum(
        "NtSetInformationJobObject", SYS_NUM_PARAMS_SetInformationJobObject,
        SYS_WOW64_IDX_SetInformationJobObject);
    if (sysnum_SetInformationJobObject == -1)
        return false;
    sysnum_Close =
        soft_kills_get_sysnum("NtClose", SYS_NUM_PARAMS_Close, SYS_WOW64_IDX_Close);
    if (sysnum_Close == -1)
        return false;
    sysnum_DuplicateObject =
        soft_kills_get_sysnum("NtDuplicateObject", SYS_NUM_PARAMS_DuplicateObject,
                              SYS_WOW64_IDX_DuplicateObject);
    if (sysnum_DuplicateObject == -1)
        return false;

    cls_idx_soft =
        drmgr_register_cls_field(soft_kills_context_init, soft_kills_context_exit);
    if (cls_idx_soft == -1)
        return false;

    /* Ensure that DR intercepts these when we're native */
    IF_DEBUG(ok =)
    dr_syscall_intercept_natively("NtTerminateProcess", sysnum_TerminateProcess,
                                  SYS_NUM_PARAMS_TerminateProcess,
                                  SYS_WOW64_IDX_TerminateProcess);
    ASSERT(ok, "failure to watch syscall while native");
    IF_DEBUG(ok =)
    dr_syscall_intercept_natively("NtTerminateJobObject", sysnum_TerminateJobObject,
                                  SYS_NUM_PARAMS_TerminateJobObject,
                                  SYS_WOW64_IDX_TerminateJobObject);
    ASSERT(ok, "failure to watch syscall while native");
    IF_DEBUG(ok =)
    dr_syscall_intercept_natively(
        "NtSetInformationJobObject", sysnum_SetInformationJobObject,
        SYS_NUM_PARAMS_SetInformationJobObject, SYS_WOW64_IDX_SetInformationJobObject);
    ASSERT(ok, "failure to watch syscall while native");
    IF_DEBUG(ok =)
    dr_syscall_intercept_natively("NtClose", sysnum_Close, SYS_NUM_PARAMS_Close,
                                  SYS_WOW64_IDX_Close);
    ASSERT(ok, "failure to watch syscall while native");
    IF_DEBUG(ok =)
    dr_syscall_intercept_natively("NtDuplicateObject", sysnum_DuplicateObject,
                                  SYS_NUM_PARAMS_DuplicateObject,
                                  SYS_WOW64_IDX_DuplicateObject);
    ASSERT(ok, "failure to watch syscall while native");
#endif

    if (!drmgr_register_pre_syscall_event(soft_kills_pre_syscall) ||
        !drmgr_register_post_syscall_event(soft_kills_post_syscall))
        return false;
    dr_register_filter_syscall_event(soft_kills_filter_syscall);

    return true;
}

static void
soft_kills_exit(void)
{
    cb_entry_t *e;
#ifdef WINDOWS
    /* Any open job handles will be closed, triggering
     * JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
     */
    uint i;
    /* The exit code used is the exit code for this process */
    int exit_code;
    if (!get_app_exit_code(&exit_code))
        exit_code = 0;
    hashtable_lock(&job_table);
    for (i = 0; i < HASHTABLE_SIZE(job_table.table_bits); i++) {
        hash_entry_t *he;
        for (he = job_table.table[i]; he != NULL; he = he->next) {
            HANDLE job = (HANDLE)he->key;
            job_info_t *jinfo = (job_info_t *)he->payload;
            NOTIFY(1, "--drx-- implicit close of job 0x%x in pid %d\n", job,
                   dr_get_process_id());
            soft_kills_handle_close(dr_get_current_drcontext(), jinfo, job, exit_code,
                                    false /*do not remove*/);
        }
    }
    hashtable_unlock(&job_table);

    hashtable_delete(&job_table);

    drmgr_unregister_cls_field(soft_kills_context_init, soft_kills_context_exit,
                               cls_idx_soft);
#endif

    dr_mutex_lock(cb_lock);
    while (cb_list != NULL) {
        e = cb_list;
        cb_list = e->next;
        dr_global_free(e, sizeof(*e));
    }
    dr_mutex_unlock(cb_lock);

    dr_mutex_destroy(cb_lock);
}

bool
drx_register_soft_kills(bool (*event_cb)(process_id_t pid, int exit_code))
{
    /* We split our init from drx_init() to avoid extra work when nobody
     * requests this feature.
     */
    static int soft_kills_init_count;
    cb_entry_t *e;
    int count = dr_atomic_add32_return_sum(&soft_kills_init_count, 1);
    if (count == 1) {
        soft_kills_init();
    }

    e = dr_global_alloc(sizeof(*e));
    e->cb = event_cb;

    dr_mutex_lock(cb_lock);
    e->next = cb_list;
    cb_list = e;
    dr_mutex_unlock(cb_lock);
    return true;
}

/***************************************************************************
 * INSTRUCTION LIST
 */

DR_EXPORT
size_t
drx_instrlist_size(instrlist_t *ilist)
{
    instr_t *instr;
    size_t size = 0;

    for (instr = instrlist_first(ilist); instr != NULL; instr = instr_get_next(instr))
        size++;

    return size;
}

DR_EXPORT
size_t
drx_instrlist_app_size(instrlist_t *ilist)
{
    instr_t *instr;
    size_t size = 0;

    for (instr = instrlist_first_app(ilist); instr != NULL;
         instr = instr_get_next_app(instr))
        size++;

    return size;
}

/***************************************************************************
 * LOGGING
 */
#ifdef WINDOWS
#    define DIRSEP '\\'
#else
#    define DIRSEP '/'
#endif

file_t
drx_open_unique_file(const char *dir, const char *prefix, const char *suffix,
                     uint extra_flags, char *result OUT, size_t result_len)
{
    char buf[MAXIMUM_PATH];
    file_t f = INVALID_FILE;
    int i;
    ssize_t len;
    for (i = 0; i < 10000; i++) {
        len = dr_snprintf(
            buf, BUFFER_SIZE_ELEMENTS(buf), "%s%c%s.%04d.%s", dir, DIRSEP, prefix,
            (extra_flags == DRX_FILE_SKIP_OPEN) ? dr_get_random_value(9999) : i, suffix);
        if (len < 0)
            return INVALID_FILE;
        NULL_TERMINATE_BUFFER(buf);
        if (extra_flags != DRX_FILE_SKIP_OPEN)
            f = dr_open_file(buf, DR_FILE_WRITE_REQUIRE_NEW | extra_flags);
        if (f != INVALID_FILE || extra_flags == DRX_FILE_SKIP_OPEN) {
            if (result != NULL)
                dr_snprintf(result, result_len, "%s", buf);
            return f;
        }
    }
    return INVALID_FILE;
}

file_t
drx_open_unique_appid_file(const char *dir, ptr_int_t id, const char *prefix,
                           const char *suffix, uint extra_flags, char *result OUT,
                           size_t result_len)
{
    int len;
    char appid[MAXIMUM_PATH];
    const char *app_name = dr_get_application_name();
    if (app_name == NULL)
        app_name = "<unknown-app>";
    len = dr_snprintf(appid, BUFFER_SIZE_ELEMENTS(appid), "%s.%s.%05d", prefix, app_name,
                      id);
    if (len < 0 || (size_t)len >= BUFFER_SIZE_ELEMENTS(appid))
        return INVALID_FILE;
    NULL_TERMINATE_BUFFER(appid);

    return drx_open_unique_file(dir, appid, suffix, extra_flags, result, result_len);
}

bool
drx_open_unique_appid_dir(const char *dir, ptr_int_t id, const char *prefix,
                          const char *suffix, char *result OUT, size_t result_len)
{
    char buf[MAXIMUM_PATH];
    int i;
    ssize_t len;
    for (i = 0; i < 10000; i++) {
        const char *app_name = dr_get_application_name();
        if (app_name == NULL)
            app_name = "<unknown-app>";
        len = dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s%c%s.%s.%05d.%04d.%s", dir,
                          DIRSEP, prefix, app_name, id, i, suffix);
        if (len < 0 || (size_t)len >= BUFFER_SIZE_ELEMENTS(buf))
            return false;
        NULL_TERMINATE_BUFFER(buf);
        if (dr_create_dir(buf)) {
            if (result != NULL)
                dr_snprintf(result, result_len, "%s", buf);
            return true;
        }
    }
    return false;
}

bool
drx_tail_pad_block(void *drcontext, instrlist_t *ilist)
{
    instr_t *last = instrlist_last_app(ilist);

    if (instr_is_cti(last) || instr_is_syscall(last)) {
        /* This basic block is already branch or syscall-terminated */
        return false;
    }
    instrlist_meta_postinsert(ilist, last, INSTR_CREATE_label(drcontext));
    return true;
}
