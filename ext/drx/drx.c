/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.   All rights reserved.
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

#define XMM_REG_SIZE 16
#define YMM_REG_SIZE 32
#define MAX(x, y) ((x) >= (y) ? (x) : (y))

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) (x)
#else
#    define IF_WINDOWS_ELSE(x, y) (y)
#endif

#ifdef X86
/* TODO i#2985: add ARM SIMD. */
#    define PLATFORM_SUPPORTS_SCATTER_GATHER
#endif

#define MINSERT instrlist_meta_preinsert
/* For inserting an app instruction, which must have a translation ("xl8") field. */
#define PREXL8 instrlist_preinsert

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

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER

static int drx_scatter_gather_expanded;

static bool
drx_event_restore_state(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info);
#endif

/***************************************************************************
 * INIT
 */

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

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    drmgr_priority_t fault_priority = { sizeof(fault_priority),
                                        DRMGR_PRIORITY_NAME_DRX_FAULT, NULL, NULL,
                                        DRMGR_PRIORITY_FAULT_DRX };
#endif

    int count = dr_atomic_add32_return_sum(&drx_init_count, 1);
    if (count > 1)
        return true;

    drmgr_init();
    note_base = drmgr_reserve_note_range(DRX_NOTE_COUNT);
    ASSERT(note_base != DRMGR_NOTE_NONE, "failed to reserve note range");

    if (drreg_init(&ops) != DRREG_SUCCESS)
        return false;

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    if (!drmgr_register_restore_state_ex_event_ex(drx_event_restore_state,
                                                  &fault_priority))
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
                          IF_NOT_X86_(dr_spill_slot_t slot2) void *addr, int value,
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
            (slot != SPILL_SLOT_MAX + 1 IF_NOT_X86(|| slot2 != SPILL_SLOT_MAX + 1))) {
            ASSERT(false, "with drmgr, SPILL_SLOT_MAX+1 must be passed");
            return false;
        }
    } else if (!(slot >= SPILL_SLOT_1 && slot <= SPILL_SLOT_MAX)) {
        ASSERT(false, "wrong spill slot");
        return false;
    }

    /* check whether we can add lock */
    if (TEST(DRX_COUNTER_LOCK, flags)) {
#ifdef ARM
        /* FIXME i#1551: implement for ARM */
        ASSERT(false, "DRX_COUNTER_LOCK not implemented for ARM");
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
                INST_CREATE_stlr(drcontext, OPND_CREATE_MEMPTR(reg1, 0),
                                 opnd_create_reg(reg2)));
#    else /* ARM */
        /* TODO: This counter update has not been tested on a ARM_32 machine. */
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

/***************************************************************************
 * drx_expand_scatter_gather() related auxiliary functions and structures.
 */

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER

typedef struct _scatter_gather_info_t {
    bool is_evex;
    bool is_load;
    opnd_size_t scalar_index_size;
    opnd_size_t scalar_value_size;
    opnd_size_t scatter_gather_size;
    reg_id_t mask_reg;
    reg_id_t base_reg;
    reg_id_t index_reg;
    union {
        reg_id_t gather_dst_reg;
        reg_id_t scatter_src_reg;
    };
    int disp;
    int scale;
} scatter_gather_info_t;

static void
get_scatter_gather_info(instr_t *instr, scatter_gather_info_t *sg_info)
{
    /* We detect whether the instruction is EVEX by looking at its potential mask
     * operand.
     */
    opnd_t dst0 = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    sg_info->is_evex = opnd_is_reg(src0) && reg_is_opmask(opnd_get_reg(src0));
    sg_info->mask_reg = sg_info->is_evex ? opnd_get_reg(src0) : opnd_get_reg(src1);
    ASSERT(!sg_info->is_evex ||
               (opnd_get_reg(instr_get_dst(instr, 1)) == opnd_get_reg(src0)),
           "Invalid gather instruction.");
    int opc = instr_get_opcode(instr);
    opnd_t memopnd;
    switch (opc) {
    case OP_vgatherdpd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vgatherqpd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vgatherdps:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vgatherqps:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherdd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherqd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherdq:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vpgatherqq:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vscatterdpd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vscatterqpd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vscatterdps:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vscatterqps:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterdd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterqd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterdq:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vpscatterqq:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    default:
        ASSERT(false, "Incorrect opcode.");
        memopnd = opnd_create_null();
        break;
    }
    if (sg_info->is_load) {
        sg_info->scatter_gather_size = opnd_get_size(dst0);
        sg_info->gather_dst_reg = opnd_get_reg(dst0);
        memopnd = sg_info->is_evex ? src1 : src0;
    } else {
        sg_info->scatter_gather_size = opnd_get_size(src1);
        sg_info->scatter_src_reg = opnd_get_reg(src1);
        memopnd = dst0;
    }
    sg_info->index_reg = opnd_get_index(memopnd);
    sg_info->base_reg = opnd_get_base(memopnd);
    sg_info->disp = opnd_get_disp(memopnd);
    sg_info->scale = opnd_get_scale(memopnd);
}

static bool
expand_gather_insert_scalar(void *drcontext, instrlist_t *bb, instr_t *sg_instr, int el,
                            scatter_gather_info_t *sg_info, reg_id_t simd_reg,
                            reg_id_t scalar_reg, reg_id_t scratch_xmm, bool is_avx512,
                            app_pc orig_app_pc)
{
    /* Used by both AVX2 and AVX-512. */
    ASSERT(instr_is_gather(sg_instr), "Internal error: only gather instructions.");
    reg_id_t simd_reg_zmm = reg_resize_to_opsz(simd_reg, OPSZ_64);
    reg_id_t simd_reg_ymm = reg_resize_to_opsz(simd_reg, OPSZ_32);
    uint scalar_value_bytes = opnd_size_in_bytes(sg_info->scalar_value_size);
    int scalarxmmimm = el * scalar_value_bytes / XMM_REG_SIZE;
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vextracti32x4_mask(
                             drcontext, opnd_create_reg(scratch_xmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(simd_reg_zmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_vextracti128(drcontext, opnd_create_reg(scratch_xmm),
                                             opnd_create_reg(simd_reg_ymm),
                                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                   orig_app_pc));
    }
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(
                INSTR_CREATE_vpinsrd(
                    drcontext, opnd_create_reg(scratch_xmm), opnd_create_reg(scratch_xmm),
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_reg), scalar_reg)),
                    opnd_create_immed_int((el * scalar_value_bytes) % XMM_REG_SIZE /
                                              opnd_size_in_bytes(OPSZ_4),
                                          OPSZ_1)),
                orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_reg),
               "The qword index versions are unsupported in 32-bit mode.");
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(INSTR_CREATE_vpinsrq(
                          drcontext, opnd_create_reg(scratch_xmm),
                          opnd_create_reg(scratch_xmm), opnd_create_reg(scalar_reg),
                          opnd_create_immed_int((el * scalar_value_bytes) % XMM_REG_SIZE /
                                                    opnd_size_in_bytes(OPSZ_8),
                                                OPSZ_1)),
                      orig_app_pc));

    } else {
        ASSERT(false, "Unexpected index size.");
    }
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vinserti32x4_mask(
                             drcontext, opnd_create_reg(simd_reg_zmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(simd_reg_zmm), opnd_create_reg(scratch_xmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vinserti128(
                             drcontext, opnd_create_reg(simd_reg_ymm),
                             opnd_create_reg(simd_reg_ymm), opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                         orig_app_pc));
    }
    return true;
}

static bool
expand_avx512_gather_insert_scalar_value(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr, int el,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scalar_value_reg, reg_id_t scratch_xmm,
                                         app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->gather_dst_reg, scalar_value_reg,
                                       scratch_xmm, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx2_gather_insert_scalar_value(void *drcontext, instrlist_t *bb,
                                       instr_t *sg_instr, int el,
                                       scatter_gather_info_t *sg_info,
                                       reg_id_t scalar_value_reg, reg_id_t scratch_xmm,
                                       app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->gather_dst_reg, scalar_value_reg,
                                       scratch_xmm, false /* AVX2 */, orig_app_pc);
}

static bool
expand_avx2_gather_insert_scalar_mask(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                      int el, scatter_gather_info_t *sg_info,
                                      reg_id_t scalar_index_reg, reg_id_t scratch_xmm,
                                      app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->mask_reg, scalar_index_reg, scratch_xmm,
                                       false /* AVX2 */, orig_app_pc);
}

static bool
expand_scatter_gather_extract_scalar(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                     int el, scatter_gather_info_t *sg_info,
                                     opnd_size_t scalar_size, uint scalar_bytes,
                                     reg_id_t from_simd_reg, reg_id_t scratch_xmm,
                                     reg_id_t scratch_reg, bool is_avx512,
                                     app_pc orig_app_pc)
{
    reg_id_t from_simd_reg_zmm = reg_resize_to_opsz(from_simd_reg, OPSZ_64);
    reg_id_t from_simd_reg_ymm = reg_resize_to_opsz(from_simd_reg, OPSZ_32);
    int scalarxmmimm = el * scalar_bytes / XMM_REG_SIZE;
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vextracti32x4_mask(
                             drcontext, opnd_create_reg(scratch_xmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(from_simd_reg_zmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_vextracti128(drcontext, opnd_create_reg(scratch_xmm),
                                             opnd_create_reg(from_simd_reg_ymm),
                                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                   orig_app_pc));
    }
    if (scalar_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpextrd(
                             drcontext,
                             opnd_create_reg(
                                 IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg)),
                             opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int((el * scalar_bytes) % XMM_REG_SIZE /
                                                       opnd_size_in_bytes(OPSZ_4),
                                                   OPSZ_1)),
                         orig_app_pc));
    } else if (scalar_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scratch_reg),
               "The qword index versions are unsupported in 32-bit mode.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpextrq(
                             drcontext, opnd_create_reg(scratch_reg),
                             opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int((el * scalar_bytes) % XMM_REG_SIZE /
                                                       opnd_size_in_bytes(OPSZ_8),
                                                   OPSZ_1)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected scalar size.");
        return false;
    }
    return true;
}

static bool
expand_avx512_scatter_extract_scalar_value(void *drcontext, instrlist_t *bb,
                                           instr_t *sg_instr, int el,
                                           scatter_gather_info_t *sg_info,
                                           reg_id_t scratch_xmm, reg_id_t scratch_reg,
                                           app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_value_size,
        opnd_size_in_bytes(sg_info->scalar_value_size), sg_info->scatter_src_reg,
        scratch_xmm, scratch_reg, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx512_scatter_gather_extract_scalar_index(void *drcontext, instrlist_t *bb,
                                                  instr_t *sg_instr, int el,
                                                  scatter_gather_info_t *sg_info,
                                                  reg_id_t scratch_xmm,
                                                  reg_id_t scratch_reg,
                                                  app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_index_size,
        opnd_size_in_bytes(sg_info->scalar_index_size), sg_info->index_reg, scratch_xmm,
        scratch_reg, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx2_gather_extract_scalar_index(void *drcontext, instrlist_t *bb,
                                        instr_t *sg_instr, int el,
                                        scatter_gather_info_t *sg_info,
                                        reg_id_t scratch_xmm, reg_id_t scratch_reg,
                                        app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_index_size,
        opnd_size_in_bytes(sg_info->scalar_index_size), sg_info->index_reg, scratch_xmm,
        scratch_reg, false /* AVX2 */, orig_app_pc);
}

static bool
expand_avx512_scatter_gather_update_mask(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr, int el,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scratch_reg, app_pc orig_app_pc,
                                         drvector_t *allowed)
{
    reg_id_t save_mask_reg;
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_mov_imm(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT32(1 << el)),
                     orig_app_pc));
    if (drreg_reserve_register(drcontext, bb, sg_instr, allowed, &save_mask_reg) !=
        DRREG_SUCCESS)
        return false;
    /* The scratch k register we're using here is always k0, because it is never
     * used for scatter/gather.
     */
    MINSERT(bb, sg_instr,
            INSTR_CREATE_kmovw(
                drcontext,
                opnd_create_reg(IF_X64_ELSE(reg_64_to_32(save_mask_reg), save_mask_reg)),
                opnd_create_reg(DR_REG_K0)));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kmovw(drcontext, opnd_create_reg(DR_REG_K0),
                                        opnd_create_reg(IF_X64_ELSE(
                                            reg_64_to_32(scratch_reg), scratch_reg))),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kandnw(drcontext, opnd_create_reg(sg_info->mask_reg),
                                         opnd_create_reg(DR_REG_K0),
                                         opnd_create_reg(sg_info->mask_reg)),
                     orig_app_pc));
    MINSERT(bb, sg_instr,
            INSTR_CREATE_kmovw(drcontext, opnd_create_reg(DR_REG_K0),
                               opnd_create_reg(IF_X64_ELSE(reg_64_to_32(save_mask_reg),
                                                           save_mask_reg))));
    if (drreg_unreserve_register(drcontext, bb, sg_instr, save_mask_reg) !=
        DRREG_SUCCESS) {
        ASSERT(false, "drreg_unreserve_register should not fail");
        return false;
    }
    return true;
}

static bool
expand_avx2_gather_update_mask(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                               int el, scatter_gather_info_t *sg_info,
                               reg_id_t scratch_xmm, reg_id_t scratch_reg,
                               app_pc orig_app_pc)
{
    /* The width of the mask element and data element is identical per definition of the
     * instruction.
     */
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(
                INSTR_CREATE_xor(
                    drcontext,
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg)),
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg))),
                orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_xor(drcontext, opnd_create_reg(scratch_reg),
                                          opnd_create_reg(scratch_reg)),
                         orig_app_pc));
    }
    reg_id_t null_index_reg = scratch_reg;
    if (!expand_avx2_gather_insert_scalar_mask(drcontext, bb, sg_instr, el, sg_info,
                                               null_index_reg, scratch_xmm, orig_app_pc))
        return false;
    return true;
}

static bool
expand_avx2_gather_make_test(void *drcontext, instrlist_t *bb, instr_t *sg_instr, int el,
                             scatter_gather_info_t *sg_info, reg_id_t scratch_xmm,
                             reg_id_t scratch_reg, instr_t *skip_label,
                             app_pc orig_app_pc)
{
    /* The width of the mask element and data element is identical per definition of the
     * instruction.
     */
    expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_value_size,
        opnd_size_in_bytes(sg_info->scalar_value_size), sg_info->mask_reg, scratch_xmm,
        scratch_reg, false /* AVX2 */, orig_app_pc);
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_shr(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT8(31)),
                         orig_app_pc));
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_and(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT32(1)),
                         orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_shr(drcontext, opnd_create_reg(scratch_reg),
                                          OPND_CREATE_INT8(63)),
                         orig_app_pc));
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_and(drcontext, opnd_create_reg(scratch_reg),
                                          OPND_CREATE_INT32(1)),
                         orig_app_pc));
    }
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip_label)),
                     orig_app_pc));
    return true;
}

static bool
expand_avx512_scatter_gather_make_test(void *drcontext, instrlist_t *bb,
                                       instr_t *sg_instr, int el,
                                       scatter_gather_info_t *sg_info,
                                       reg_id_t scratch_reg, instr_t *skip_label,
                                       app_pc orig_app_pc)
{
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kmovw(drcontext,
                                        opnd_create_reg(IF_X64_ELSE(
                                            reg_64_to_32(scratch_reg), scratch_reg)),
                                        opnd_create_reg(sg_info->mask_reg)),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_test(drcontext,
                                       opnd_create_reg(IF_X64_ELSE(
                                           reg_64_to_32(scratch_reg), scratch_reg)),
                                       OPND_CREATE_INT32(1 << el)),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip_label)),
                     orig_app_pc));
    return true;
}

static bool
expand_avx512_scatter_store_scalar_value(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scalar_index_reg,
                                         reg_id_t scalar_value_reg, app_pc orig_app_pc)
{
    if (sg_info->base_reg == IF_X64_ELSE(DR_REG_RAX, DR_REG_EAX)) {
        /* We need the app's base register value. If it's xax, then it may be used to
         * store flags by drreg.
         */
        drreg_get_app_value(drcontext, bb, sg_instr, sg_info->base_reg,
                            sg_info->base_reg);
    }
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_st(
                             drcontext,
                             opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                   sg_info->scale, sg_info->disp, OPSZ_4),
                             opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_value_reg),
                                                         scalar_value_reg))),
                         orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_index_reg),
               "Internal error: scratch index register not 64-bit.");
        ASSERT(reg_is_64bit(scalar_value_reg),
               "Internal error: scratch value register not 64-bit.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_st(
                             drcontext,
                             opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                   sg_info->scale, sg_info->disp, OPSZ_8),
                             opnd_create_reg(scalar_value_reg)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected index size.");
        return false;
    }
    return true;
}

static bool
expand_gather_load_scalar_value(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                scatter_gather_info_t *sg_info, reg_id_t scalar_index_reg,
                                app_pc orig_app_pc)
{
    if (sg_info->base_reg == IF_X64_ELSE(DR_REG_RAX, DR_REG_EAX)) {
        /* We need the app's base register value. If it's xax, then it may be used to
         * store flags by drreg.
         */
        drreg_get_app_value(drcontext, bb, sg_instr, sg_info->base_reg,
                            sg_info->base_reg);
    }
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(INSTR_CREATE_mov_ld(
                          drcontext,
                          opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_index_reg),
                                                      scalar_index_reg)),
                          opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                sg_info->scale, sg_info->disp, OPSZ_4)),
                      orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_index_reg),
               "Internal error: scratch register not 64-bit.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(scalar_index_reg),
                                             opnd_create_base_disp(
                                                 sg_info->base_reg, scalar_index_reg,
                                                 sg_info->scale, sg_info->disp, OPSZ_8)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected index size.");
        return false;
    }
    return true;
}

#endif

/*****************************************************************************************
 * drx_expand_scatter_gather()
 *
 * The function expands scatter and gather instructions to a sequence of equivalent
 * scalar operations. Gather instructions are expanded into a sequence of mask register
 * bit tests, extracting the index value, a scalar load, inserting the scalar value into
 * the destination simd register, and mask register bit updates. Scatter instructions
 * are similarly expanded into a sequence, but deploy a scalar store. Registers spilled
 * and restored by drreg are not illustrated in the sequence below.
 *
 * ------------------------------------------------------------------------------
 * AVX2 vpgatherdd, vgatherdps, vpgatherdq, vgatherdpd, vpgatherqd, vgatherqps, |
 * vpgatherqq, vgatherqpd:                                                      |
 * ------------------------------------------------------------------------------
 *
 * vpgatherdd (%rax,%ymm1,4)[4byte] %ymm2 -> %ymm0 %ymm2 sequence laid out here,
 * others are similar:
 *
 * Extract mask dword. qword versions use vpextrq:
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x00 -> %ecx
 * Test mask bit:
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 * Skip element if mask not set:
 *   jz             <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti128   %ymm1 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x00 -> %ecx
 * Restore app's base register value (may not be present):
 *   mov            %rax -> %gs:0x00000090[8byte]
 *   mov            %gs:0x00000098[8byte] -> %rax
 * Load scalar value:
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 * Insert scalar value in destination register:
 *   vextracti128   %ymm0 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x00 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x00 -> %ymm0
 * Set mask dword to zero:
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x00 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x00 -> %ymm2
 *   skip0:
 * Do the same as above for the next element:
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x01 -> %ecx
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 *   jz             <skip1>
 *   vextracti128   %ymm1 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x01 -> %ecx
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti128   %ymm0 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x01 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x00 -> %ymm0
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x01 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x00 -> %ymm2
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   vextracti128   %ymm2 $0x01 -> %xmm3
 *   vpextrd        %xmm3 $0x03 -> %ecx
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 *   jz             <skip7>
 *   vextracti128   %ymm1 $0x01 -> %xmm3
 *   vpextrd        %xmm3 $0x03 -> %ecx
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti128   %ymm0 $0x01 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x03 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x01 -> %ymm0
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x01 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x03 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x01 -> %ymm2
 *   skip7:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   vpxor          %ymm2 %ymm2 -> %ymm2
 *
 * ---------------------------------------------------------------------------------
 * AVX-512 vpgatherdd, vgatherdps, vpgatherdq, vgatherdpd, vpgatherqd, vgatherqps, |
 * vpgatherqq, vgatherqpd:                                                         |
 * ---------------------------------------------------------------------------------
 *
 * vpgatherdd {%k1} (%rax,%zmm1,4)[4byte] -> %zmm0 %k1 sequence laid out here,
 * others are similar:
 *
 * Extract mask bit:
 *   kmovw           %k1 -> %ecx
 * Test mask bit:
 *   test            %ecx $0x00000001
 * Skip element if mask not set:
 *   jz              <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %ecx
 * Restore app's base register value (may not be present):
 *   mov             %rax -> %gs:0x00000090[8byte]
 *   mov             %gs:0x00000098[8byte] -> %rax
 * Load scalar value:
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 * Insert scalar value in destination register:
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x00 -> %xmm2
 *   vinserti32x4    {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 * Set mask bit to zero:
 *   mov             $0x00000001 -> %ecx
 * %k0 is saved to a gpr here, while the gpr
 * is managed by drreg. This is not further
 * layed out in this example.
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 * It is not illustrated that %k0 is restored here.
 *   skip0:
 * Do the same as above for the next element:
 *   kmovw           %k1 -> %ecx
 *   test            %ecx $0x00000002
 *   jz              <skip1>
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %ecx
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x01 -> %xmm2
 *   vinserti32x4    {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 *   mov             $0x00000002 -> %ecx
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   kmovw           %k1 -> %ecx
 *   test            %ecx $0x00008000
 *   jz              <skip15>
 *   vextracti32x4   {%k0} $0x03 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %ecx
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti32x4   {%k0} $0x03 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x03 -> %xmm2
 *   vinserti32x4    {%k0} $0x03 %zmm0 %xmm2 -> %zmm0
 *   mov             $0x00008000 -> %ecx
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip15:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   kxorq           %k1 %k1 -> %k1
 *
 * --------------------------------------------------------------------------
 * AVX-512 vpscatterdd, vscatterdps, vpscatterdq, vscatterdpd, vpscatterqd, |
 * vscatterqps, vpscatterqq, vscatterqpd:                                   |
 * --------------------------------------------------------------------------
 *
 * vpscatterdd {%k1} %zmm0 -> (%rcx,%zmm1,4)[4byte] %k1 sequence laid out here,
 * others are similar:
 *
 * Extract mask bit:
 *   kmovw           %k1 -> %edx
 * Test mask bit:
 *   test            %edx $0x00000001
 * Skip element if mask not set:
 *   jz              <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %edx
 * Extract scalar value dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %ebx
 * Store scalar value:
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 * Set mask bit to zero:
 *   mov             $0x00000001 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip0:
 * Do the same as above for the next element:
 *   kmovw           %k1 -> %edx
 *   test            %edx $0x00000002
 *   jz              <skip1>
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %edx
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %ebx
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 *   mov             $0x00000002 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   kmovw           %k1 -> %edx
 *   test            %edx $0x00008000
 *   jz              <skip15>
 *   vextracti32x4   {%k0} $0x03 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %edx
 *   vextracti32x4   {%k0} $0x03 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %ebx
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 *   mov             $0x00008000 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip15:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   kxorq           %k1 %k1 -> %k1
 */
bool
drx_expand_scatter_gather(void *drcontext, instrlist_t *bb, OUT bool *expanded)
{
#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    instr_t *instr, *next_instr, *first_app = NULL;
    bool delete_rest = false;
#endif
    if (expanded != NULL)
        *expanded = false;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP) {
        return false;
    }
#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER
    /* Make each scatter or gather instruction be in their own basic block.
     * TODO i#3837: cross-platform code like the following bb splitting can be shared
     * with other architectures in the future.
     */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (delete_rest) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        } else if (instr_is_app(instr)) {
            if (first_app == NULL)
                first_app = instr;
            if (instr_is_gather(instr) || instr_is_scatter(instr)) {
                delete_rest = true;
                if (instr != first_app) {
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                }
            }
        }
    }
    if (first_app == NULL)
        return true;
    if (!instr_is_gather(first_app) && !instr_is_scatter(first_app))
        return true;

    /* We want to avoid spill slot conflicts with later instrumentation passes. */
    drreg_status_t res_bb_props =
        drreg_set_bb_properties(drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS);
    DR_ASSERT(res_bb_props == DRREG_SUCCESS);

    dr_atomic_store32(&drx_scatter_gather_expanded, 1);

    instr_t *sg_instr = first_app;
    scatter_gather_info_t sg_info;
    bool res = false;
    /* XXX: we may want to make this function public, as it may be useful to clients. */
    get_scatter_gather_info(sg_instr, &sg_info);
#    ifndef X64
    if (sg_info.scalar_index_size == OPSZ_8 || sg_info.scalar_value_size == OPSZ_8) {
        /* FIXME i#2985: we do not yet support expansion of the qword index and value
         * scatter/gather versions in 32-bit mode.
         */
        return false;
    }
#    endif
    uint no_of_elements = opnd_size_in_bytes(sg_info.scatter_gather_size) /
        MAX(opnd_size_in_bytes(sg_info.scalar_index_size),
            opnd_size_in_bytes(sg_info.scalar_value_size));
    reg_id_t scratch_reg0 = DR_REG_INVALID, scratch_reg1 = DR_REG_INVALID;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, true);
    /* We need the scratch registers and base register app's value to be available at the
     * same time. Do not use.
     */
    drreg_set_vector_entry(&allowed, sg_info.base_reg, false);
    if (drreg_reserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
    if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_reg0) !=
        DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
    if (instr_is_scatter(sg_instr)) {
        if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_reg1) !=
            DRREG_SUCCESS)
            goto drx_expand_scatter_gather_exit;
    }
    app_pc orig_app_pc = instr_get_app_pc(sg_instr);
    reg_id_t scratch_xmm;
    /* Search the instruction for an unused xmm register we will use as a temp. */
    for (scratch_xmm = DR_REG_START_XMM; scratch_xmm <= DR_REG_STOP_XMM; ++scratch_xmm) {
        if ((sg_info.is_evex ||
             scratch_xmm != reg_resize_to_opsz(sg_info.mask_reg, OPSZ_16)) &&
            scratch_xmm != reg_resize_to_opsz(sg_info.index_reg, OPSZ_16) &&
            /* redundant with scatter_src_reg */
            scratch_xmm != reg_resize_to_opsz(sg_info.gather_dst_reg, OPSZ_16))
            break;
    }
    /* FIXME i#2985: spill scratch_xmm using a future drreg extension for simd. */
    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(sg_instr);
    emulated_instr.instr = sg_instr;
    /* Tools should instrument the data operations in the sequence. */
    emulated_instr.flags = DR_EMULATE_INSTR_ONLY;
    drmgr_insert_emulation_start(drcontext, bb, sg_instr, &emulated_instr);

    if (sg_info.is_evex) {
        if (/* AVX-512 */ instr_is_gather(sg_instr)) {
            for (uint el = 0; el < no_of_elements; ++el) {
                instr_t *skip_label = INSTR_CREATE_label(drcontext);
                if (!expand_avx512_scatter_gather_make_test(drcontext, bb, sg_instr, el,
                                                            &sg_info, scratch_reg0,
                                                            skip_label, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_extract_scalar_index(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm, scratch_reg0,
                        orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_index_reg = scratch_reg0;
                if (!expand_gather_load_scalar_value(drcontext, bb, sg_instr, &sg_info,
                                                     scalar_index_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_value_reg = scratch_reg0;
                if (!expand_avx512_gather_insert_scalar_value(drcontext, bb, sg_instr, el,
                                                              &sg_info, scalar_value_reg,
                                                              scratch_xmm, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_update_mask(drcontext, bb, sg_instr, el,
                                                              &sg_info, scratch_reg0,
                                                              orig_app_pc, &allowed))
                    goto drx_expand_scatter_gather_exit;
                MINSERT(bb, sg_instr, skip_label);
            }
        } else /* AVX-512 instr_is_scatter(sg_instr) */ {
            for (uint el = 0; el < no_of_elements; ++el) {
                instr_t *skip_label = INSTR_CREATE_label(drcontext);
                expand_avx512_scatter_gather_make_test(drcontext, bb, sg_instr, el,
                                                       &sg_info, scratch_reg0, skip_label,
                                                       orig_app_pc);
                if (!expand_avx512_scatter_gather_extract_scalar_index(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm, scratch_reg0,
                        orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_index_reg = scratch_reg0;
                reg_id_t scalar_value_reg = scratch_reg1;
                if (!expand_avx512_scatter_extract_scalar_value(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm,
                        scalar_value_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_store_scalar_value(
                        drcontext, bb, sg_instr, &sg_info, scalar_index_reg,
                        scalar_value_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_update_mask(drcontext, bb, sg_instr, el,
                                                              &sg_info, scratch_reg0,
                                                              orig_app_pc, &allowed))
                    goto drx_expand_scatter_gather_exit;
                MINSERT(bb, sg_instr, skip_label);
            }
        }
        /* The mask register is zeroed completely when instruction finishes. */
        if (proc_has_feature(FEATURE_AVX512BW)) {
            PREXL8(
                bb, sg_instr,
                INSTR_XL8(INSTR_CREATE_kxorq(drcontext, opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg)),
                          orig_app_pc));
        } else {
            PREXL8(
                bb, sg_instr,
                INSTR_XL8(INSTR_CREATE_kxorw(drcontext, opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg)),
                          orig_app_pc));
        }
    } else {
        /* AVX2 instr_is_gather(sg_instr) */
        for (uint el = 0; el < no_of_elements; ++el) {
            instr_t *skip_label = INSTR_CREATE_label(drcontext);
            if (!expand_avx2_gather_make_test(drcontext, bb, sg_instr, el, &sg_info,
                                              scratch_xmm, scratch_reg0, skip_label,
                                              orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            if (!expand_avx2_gather_extract_scalar_index(drcontext, bb, sg_instr, el,
                                                         &sg_info, scratch_xmm,
                                                         scratch_reg0, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            reg_id_t scalar_index_reg = scratch_reg0;
            if (!expand_gather_load_scalar_value(drcontext, bb, sg_instr, &sg_info,
                                                 scalar_index_reg, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            reg_id_t scalar_value_reg = scratch_reg0;
            if (!expand_avx2_gather_insert_scalar_value(drcontext, bb, sg_instr, el,
                                                        &sg_info, scalar_value_reg,
                                                        scratch_xmm, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            if (!expand_avx2_gather_update_mask(drcontext, bb, sg_instr, el, &sg_info,
                                                scratch_xmm, scratch_reg0, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            MINSERT(bb, sg_instr, skip_label);
        }
        /* The mask register is zeroed completely when instruction finishes. */
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpxor(drcontext, opnd_create_reg(sg_info.mask_reg),
                                            opnd_create_reg(sg_info.mask_reg),
                                            opnd_create_reg(sg_info.mask_reg)),
                         orig_app_pc));
    }
    ASSERT(scratch_reg0 != scratch_reg1,
           "Internal error: scratch registers must be different");
    if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_reg0) !=
        DRREG_SUCCESS) {
        ASSERT(false, "drreg_unreserve_register should not fail");
        goto drx_expand_scatter_gather_exit;
    }
    if (instr_is_scatter(sg_instr)) {
        if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_reg1) !=
            DRREG_SUCCESS) {
            ASSERT(false, "drreg_unreserve_register should not fail");
            goto drx_expand_scatter_gather_exit;
        }
    }
    if (drreg_unreserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
#    if VERBOSE
    dr_print_instr(drcontext, STDERR, sg_instr, "\tThe instruction\n");
#    endif

    drmgr_insert_emulation_end(drcontext, bb, sg_instr);
    /* Remove and destroy the original scatter/gather. */
    instrlist_remove(bb, sg_instr);
#    if VERBOSE
    dr_fprintf(STDERR, "\twas expanded to the following sequence:\n");
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        dr_print_instr(drcontext, STDERR, instr, "");
    }
#    endif

    if (expanded != NULL)
        *expanded = true;
    res = true;

drx_expand_scatter_gather_exit:
    drvector_delete(&allowed);
    return res;

#else /* !PLATFORM_SUPPORTS_SCATTER_GATHER */
    /* TODO i#3837: add support for AArch64. */
    if (expanded != NULL)
        *expanded = false;
    return true;
#endif
}

/***************************************************************************
 * RESTORE STATE
 */

#ifdef PLATFORM_SUPPORTS_SCATTER_GATHER

/*
 * x86 scatter/gather emulation sequence support
 *
 * The following state machines exist in order to detect restore events that need
 * additional attention by drx in order to fix the application state on top of the
 * fixes that drreg already makes. For the AVX-512 scatter/gather sequences these are
 * instruction windows where a scratch mask is being used, and the windows after
 * each scalar load/store but before the destination mask register update. For AVX2,
 * the scratch mask is an xmm register and will be handled by drreg directly (future
 * update, xref #3844).
 *
 * The state machines allow for instructions like drreg spill/restore and instrumentation
 * in between recognized states. This is an approximation and could be broken in many
 * ways, e.g. by a client adding more than DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX
 * number of instructions as instrumentation, or by altering the emulation sequence's
 * code.
 * TODO i#5005: A more safe way to do this would be along the lines of xref i#3801: if
 * we had instruction lists available, we could see and pass down emulation labels
 * instead of guessing the sequence based on decoding the code cache.
 *
 * AVX-512 gather sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0
 *         vextracti32x4 {%k0} $0x00 %zmm1 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1
 *         vpextrd       %xmm2 $0x00 -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2
 *         mov           (%rax,%rcx,4)[4byte] -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3
 * (a)     vextracti32x4 {%k0} $0x00 %zmm0 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4
 * (a)     vpinsrd       %xmm2 %ecx $0x00 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5
 * (a)     vinserti32x4  {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6
 * (a)     mov           $0x00000001 -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7
 * (a)     kmovw         %k0 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8
 * (a)     kmovw         %ecx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9
 * (a) (b) kandnw        %k0 %k1 -> %k1
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10
 *     (b) kmovw         %edx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 * (b): The instruction window where the scratch mask is clobbered w/o support by drreg.
 *
 * AVX-512 scatter sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0
 *         vextracti32x4 {%k0} $0x00 %zmm1 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1
 *         vpextrd       %xmm2 $0x00 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2
 *         vextracti32x4 {%k0} $0x00 %zmm0 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3
 *         vpextrd       %xmm2 $0x00 -> %ebx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4
 *         mov           %ebx -> (%rcx,%rdx,4)[4byte]
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5
 * (a)     mov           $0x00000001 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6
 * (a)     kmovw         %k0 -> %ebp
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7
 * (a)     kmovw         %edx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8
 * (a) (b) kandnw        %k0 %k1 -> %k1
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9
 *     (b) kmovw         %ebp -> %k0
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 * (b): The instruction window where the scratch mask is clobbered w/o support by drreg.
 *
 * AVX2 gather sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0
 *         vextracti128  %ymm2 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1
 *         vpextrd       %xmm3 $0x00 -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2
 *         mov           (%rax,%rcx,4)[4byte] -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3
 * (a)     vextracti128  %ymm0 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4
 * (a)     vpinsrd       %xmm3 %ecx $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5
 * (a)     vinserti128   %ymm0 %xmm3 $0x00 -> %ymm0
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6
 * (a)     xor           %ecx %ecx -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7
 * (a)     vextracti128  %ymm2 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8
 * (a)     vpinsrd       %xmm3 %ecx $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9
 * (a)     vinserti128   %ymm2 %xmm3 $0x00 -> %ymm2
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 *
 */

#    define DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX 32

/* States of the AVX-512 gather detection state machine. */
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0 0
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1 1
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2 2
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3 3
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4 4
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5 5
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6 6
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7 7
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8 8
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9 9
#    define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10 10

/* States of the AVX-512 scatter detection state machine. */
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0 0
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1 1
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2 2
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3 3
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4 4
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5 5
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6 6
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7 7
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8 8
#    define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9 9

/* States of the AVX2 gather detection state machine. */
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0 0
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1 1
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2 2
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3 3
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4 4
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5 5
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6 6
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7 7
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8 8
#    define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9 9

typedef struct _drx_state_machine_params_t {
    byte *pc;
    byte *prev_pc;
    /* state machine's state */
    int detect_state;
    /* detected start pc of destination mask update */
    byte *restore_dest_mask_start_pc;
    /* detected start pc of scratch mask usage */
    byte *restore_scratch_mask_start_pc;
    /* counter to allow for skipping unknown instructions */
    int skip_unknown_instr_count;
    /* detected scratch xmm register for mask update */
    reg_id_t the_scratch_xmm;
    /* detected gpr register that holds the mask update immediate */
    reg_id_t gpr_bit_mask;
    /* detected gpr register that holds the app's mask state */
    reg_id_t gpr_save_scratch_mask;
    /* counter of scalar element in the scatter/gather sequence */
    uint scalar_mask_update_no;
    /* temporary scratch gpr for the AVX-512 scatter value */
    reg_id_t gpr_scratch_index;
    /* temporary scratch gpr for the AVX-512 scatter index */
    reg_id_t gpr_scratch_value;
    instr_t inst;
    dr_restore_state_info_t *info;
    scatter_gather_info_t *sg_info;
} drx_state_machine_params_t;

static void
advance_state(int new_detect_state, drx_state_machine_params_t *params)
{
    params->detect_state = new_detect_state;
    params->skip_unknown_instr_count = 0;
}

/* Advances to state 0 if counter has exceeded threshold, returns otherwise. */
static inline void
skip_unknown_instr_inc(int reset_state, drx_state_machine_params_t *params)
{
    if (params->skip_unknown_instr_count++ >= DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX) {
        advance_state(reset_state, params);
    }
}

/* Run the state machines and decode the code cache. The state machines will search the
 * code for whether the translation pc is in one of the instruction windows that need
 * additional handling by drx in order to restore specific state of the application's mask
 * registers. We consider this sufficiently accurate, but this is still an approximation.
 */
static bool
drx_restore_state_scatter_gather(
    void *drcontext, dr_restore_state_info_t *info, scatter_gather_info_t *sg_info,
    bool (*state_machine_func)(void *drcontext, drx_state_machine_params_t *params))
{
    drx_state_machine_params_t params;
    params.restore_dest_mask_start_pc = NULL;
    params.restore_scratch_mask_start_pc = NULL;
    params.detect_state = 0;
    params.skip_unknown_instr_count = 0;
    params.the_scratch_xmm = DR_REG_NULL;
    params.gpr_bit_mask = DR_REG_NULL;
    params.gpr_save_scratch_mask = DR_REG_NULL;
    params.scalar_mask_update_no = 0;
    params.info = info;
    params.sg_info = sg_info;
    params.pc = params.info->fragment_info.cache_start_pc;
    instr_init(drcontext, &params.inst);
    /* As the state machine is looking for blocks of code that the fault may hit, the 128
     * bytes is a conservative approximation of the block's size, see (a) and (b) above.
     */
    while (params.pc <= params.info->raw_mcontext->pc + 128) {
        instr_reset(drcontext, &params.inst);
        params.prev_pc = params.pc;
        params.pc = decode(drcontext, params.pc, &params.inst);
        if (params.pc == NULL) {
            /* Upon a decoding error we simply give up. */
            break;
        }
        /* If there is a gather or scatter instruction in the code cache, then it is wise
         * to assume that this is not an emulated sequence that we need to examine
         * further.
         */
        if (instr_is_gather(&params.inst))
            break;
        if (instr_is_scatter(&params.inst))
            break;
        if ((*state_machine_func)(drcontext, &params))
            break;
    }
    instr_free(drcontext, &params.inst);
    return true;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx2_gather_sequence_state_machine(void *drcontext,
                                       drx_state_machine_params_t *params)
{
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
                break;
            }
        }
        /* We don't need to ignore any instructions here, because we are already in
         * DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0.
         */
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2, params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2:
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_reads_memory(&params->inst)) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_memory_reference(src0)) {
                    if (opnd_uses_reg(src0, params->gpr_scratch_index)) {
                        opnd_t dst0 = instr_get_dst(&params->inst, 0);
                        if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                            params->restore_dest_mask_start_pc = params->pc;
                            advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3,
                                          params);
                            break;
                        }
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4, params);
                break;
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                params->the_scratch_xmm = DR_REG_NULL;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5:
        if (instr_get_opcode(&params->inst) == OP_vinserti128) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->sg_info->gather_dst_reg) {
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6:
        if (instr_get_opcode(&params->inst) == OP_xor) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            if (opnd_is_reg(dst0) && opnd_is_reg(src0) && opnd_is_reg(src1)) {
                reg_id_t reg_dst0 = opnd_get_reg(dst0);
                reg_id_t reg_src0 = opnd_get_reg(src0);
                reg_id_t reg_src1 = opnd_get_reg(src1);
                ASSERT(reg_is_gpr(reg_dst0) && reg_is_gpr(reg_src0) &&
                           reg_is_gpr(reg_src1),
                       "internal error: unexpected instruction format");
                if (reg_dst0 == reg_src0 && reg_src0 == reg_src1) {
                    params->gpr_bit_mask = reg_dst0;
                    advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7, params);
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0)) {
                if (opnd_get_reg(src0) == params->sg_info->mask_reg) {
                    opnd_t dst0 = instr_get_dst(&params->inst, 0);
                    if (opnd_is_reg(dst0)) {
                        reg_id_t tmp_reg = opnd_get_reg(dst0);
                        if (!reg_is_strictly_xmm(tmp_reg))
                            break;
                        params->the_scratch_xmm = tmp_reg;
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            opnd_t src1 = instr_get_src(&params->inst, 1);
            if (opnd_is_reg(src1)) {
                if (opnd_get_reg(src1) == params->gpr_bit_mask) {
                    ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                           "internal error: unexpected instruction format");
                    reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
                    if (tmp_reg == params->the_scratch_xmm) {
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9:
        if (instr_get_opcode(&params->inst) == OP_vinserti128) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)) &&
                       opnd_is_reg(instr_get_src(&params->inst, 0)) &&
                       opnd_is_reg(instr_get_src(&params->inst, 1)),
                   "internal error: unexpected instruction format");
            reg_id_t dst0 = opnd_get_reg(instr_get_dst(&params->inst, 0));
            reg_id_t src0 = opnd_get_reg(instr_get_src(&params->inst, 0));
            reg_id_t src1 = opnd_get_reg(instr_get_src(&params->inst, 1));
            if (src1 == params->the_scratch_xmm) {
                if (src0 == params->sg_info->mask_reg) {
                    if (dst0 == params->sg_info->mask_reg) {
                        if (params->restore_dest_mask_start_pc <=
                                params->info->raw_mcontext->pc &&
                            params->info->raw_mcontext->pc <= params->prev_pc) {
                            /* Fix the gather's destination mask here and zero out
                             * the bit that the emulation sequence hadn't done
                             * before the fault hit.
                             */
                            ASSERT(reg_is_strictly_xmm(params->sg_info->mask_reg) ||
                                       reg_is_strictly_ymm(params->sg_info->mask_reg),
                                   "internal error: unexpected instruction format");
                            byte val[YMM_REG_SIZE];
                            if (!reg_get_value_ex(params->sg_info->mask_reg,
                                                  params->info->raw_mcontext, val)) {
                                ASSERT(
                                    false,
                                    "internal error: can't read mcontext's mask value");
                            }
                            uint mask_byte =
                                opnd_size_in_bytes(params->sg_info->scalar_index_size) *
                                    (params->scalar_mask_update_no + 1) -
                                1;
                            val[mask_byte] &= ~(byte)128;
                            reg_set_value_ex(params->sg_info->mask_reg,
                                             params->info->mcontext, val);
                            /* We are done. */
                            return true;
                        }
                        params->scalar_mask_update_no++;
                        uint no_of_elements =
                            opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                            MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                                opnd_size_in_bytes(params->sg_info->scalar_value_size));
                        if (params->scalar_mask_update_no > no_of_elements) {
                            /* Unlikely that something looks identical to an emulation
                             * sequence for this long, but we safely can return here.
                             */
                            return true;
                        }
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx512_scatter_sequence_state_machine(void *drcontext,
                                          drx_state_machine_params_t *params)
{
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
                break;
            }
        }
        /* We don't need to ignore any instructions here, because we are already in
         * DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0.
         */
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2,
                                  params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3, params);
                break;
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_value = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4: {
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_writes_memory(&params->inst)) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_memory_reference(dst0)) {
                    opnd_t src0 = instr_get_src(&params->inst, 0);
                    if (opnd_is_reg(src0) &&
                        opnd_uses_reg(src0, params->gpr_scratch_value) &&
                        opnd_uses_reg(dst0, params->gpr_scratch_index)) {
                        params->restore_dest_mask_start_pc = params->pc;
                        advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5: {
        ptr_int_t val;
        if (instr_is_mov_constant(&params->inst, &val)) {
            /* If more than one bit is set, this is not what we're looking for. */
            if (val == 0 || (val & (val - 1)) != 0)
                break;
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_gpr = opnd_get_reg(dst0);
                if (reg_is_gpr(tmp_gpr)) {
                    params->gpr_bit_mask = tmp_gpr;
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(dst0);
                    if (reg_is_gpr(tmp_gpr)) {
                        params->gpr_save_scratch_mask = tmp_gpr;
                        advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7:
        ASSERT(params->gpr_bit_mask != DR_REG_NULL,
               "internal error: expected gpr register to be recorded in state "
               "machine.");
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == params->gpr_bit_mask) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                    params->restore_scratch_mask_start_pc = params->pc;
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8:
        if (instr_get_opcode(&params->inst) == OP_kandnw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                if (opnd_is_reg(src1) &&
                    opnd_get_reg(src1) == params->sg_info->mask_reg &&
                    opnd_is_reg(dst0) &&
                    opnd_get_reg(dst0) == params->sg_info->mask_reg) {
                    if (params->restore_dest_mask_start_pc <=
                            params->info->raw_mcontext->pc &&
                        params->info->raw_mcontext->pc <= params->prev_pc) {
                        /* Fix the scatter's destination mask here and zero out
                         * the bit that the emulation sequence hadn't done
                         * before the fault hit.
                         */
                        params->info->mcontext
                            ->opmask[params->sg_info->mask_reg - DR_REG_K0] &=
                            ~(1 << params->scalar_mask_update_no);
                        /* We are not done yet, we have to fix up the scratch
                         * mask as well.
                         */
                    }
                    /* We are counting the scalar load number in the sequence
                     * here.
                     */
                    params->scalar_mask_update_no++;
                    uint no_of_elements =
                        opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                        MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                            opnd_size_in_bytes(params->sg_info->scalar_value_size));
                    if (params->scalar_mask_update_no > no_of_elements) {
                        /* Unlikely that something looks identical to an emulation
                         * sequence for this long, but we safely can return here.
                         */
                        return true;
                    }
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_reg(src0)) {
                    if (reg_is_gpr(opnd_get_reg(src0)) &&
                        params->restore_scratch_mask_start_pc <=
                            params->info->raw_mcontext->pc &&
                        params->info->raw_mcontext->pc <= params->prev_pc) {
                        /* The scratch mask is always k0. This is hard-coded
                         * in drx. We carefully only update the lowest 16 bits
                         * because the mask was saved with kmovw.
                         */
                        ASSERT(sizeof(params->info->mcontext->opmask[0]) ==
                                   sizeof(long long),
                               "internal error: unexpected opmask slot size");
                        params->info->mcontext->opmask[0] &= ~0xffffLL;
                        params->info->mcontext->opmask[0] |=
                            reg_get_value(params->gpr_save_scratch_mask,
                                          params->info->raw_mcontext) &
                            0xffff;
                        /* We are done. If we did fix up the scatter's destination
                         * mask, this already has happened.
                         */
                        return true;
                    }
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0,
                                  params);
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx512_gather_sequence_state_machine(void *drcontext,
                                         drx_state_machine_params_t *params)
{
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
                break;
            }
        }
        /* We don't need to ignore any instructions here, because we are already in
         * DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0.
         */
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2, params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2:
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_reads_memory(&params->inst)) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_memory_reference(src0) &&
                    opnd_uses_reg(src0, params->gpr_scratch_index)) {
                    opnd_t dst0 = instr_get_dst(&params->inst, 0);
                    if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                        params->restore_dest_mask_start_pc = params->pc;
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5:
        if (instr_get_opcode(&params->inst) == OP_vinserti32x4) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->sg_info->gather_dst_reg) {
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6: {
        ptr_int_t val;
        if (instr_is_mov_constant(&params->inst, &val)) {
            /* If more than one bit is set, this is not what we're looking for. */
            if (val == 0 || (val & (val - 1)) != 0)
                break;
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_gpr = opnd_get_reg(dst0);
                if (reg_is_gpr(tmp_gpr)) {
                    params->gpr_bit_mask = tmp_gpr;
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7, params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(dst0);
                    if (reg_is_gpr(tmp_gpr)) {
                        params->gpr_save_scratch_mask = tmp_gpr;
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8:
        ASSERT(params->gpr_bit_mask != DR_REG_NULL,
               "internal error: expected gpr register to be recorded in state "
               "machine.");
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == params->gpr_bit_mask) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                    params->restore_scratch_mask_start_pc = params->pc;
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9, params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9:
        if (instr_get_opcode(&params->inst) == OP_kandnw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                if (opnd_is_reg(src1) &&
                    opnd_get_reg(src1) == params->sg_info->mask_reg) {
                    if (opnd_is_reg(dst0) &&
                        opnd_get_reg(dst0) == params->sg_info->mask_reg) {
                        if (params->restore_dest_mask_start_pc <=
                                params->info->raw_mcontext->pc &&
                            params->info->raw_mcontext->pc <= params->prev_pc) {
                            /* Fix the gather's destination mask here and zero out
                             * the bit that the emulation sequence hadn't done
                             * before the fault hit.
                             */
                            params->info->mcontext
                                ->opmask[params->sg_info->mask_reg - DR_REG_K0] &=
                                ~(1 << params->scalar_mask_update_no);
                            /* We are not done yet, we have to fix up the scratch
                             * mask as well.
                             */
                        }
                        /* We are counting the scalar load number in the sequence
                         * here.
                         */
                        params->scalar_mask_update_no++;
                        uint no_of_elements =
                            opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                            MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                                opnd_size_in_bytes(params->sg_info->scalar_value_size));
                        if (params->scalar_mask_update_no > no_of_elements) {
                            /* Unlikely that something looks identical to an emulation
                             * sequence for this long, but we safely can return here.
                             */
                            return true;
                        }
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_reg(src0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(src0);
                    if (reg_is_gpr(tmp_gpr)) {
                        if (params->restore_scratch_mask_start_pc <=
                                params->info->raw_mcontext->pc &&
                            params->info->raw_mcontext->pc <= params->prev_pc) {
                            /* The scratch mask is always k0. This is hard-coded
                             * in drx. We carefully only update the lowest 16 bits
                             * because the mask was saved with kmovw.
                             */
                            ASSERT(sizeof(params->info->mcontext->opmask[0]) ==
                                       sizeof(long long),
                                   "internal error: unexpected opmask slot size");
                            params->info->mcontext->opmask[0] &= ~0xffffLL;
                            params->info->mcontext->opmask[0] |=
                                reg_get_value(params->gpr_save_scratch_mask,
                                              params->info->raw_mcontext) &
                                0xffff;
                            /* We are done. If we did fix up the gather's destination
                             * mask, this already has happened.
                             */
                            return true;
                        }
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

static bool
drx_restore_state_for_avx512_gather(void *drcontext, dr_restore_state_info_t *info,
                                    scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx512_gather_sequence_state_machine);
}

static bool
drx_restore_state_for_avx512_scatter(void *drcontext, dr_restore_state_info_t *info,
                                     scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx512_scatter_sequence_state_machine);
}

static bool
drx_restore_state_for_avx2_gather(void *drcontext, dr_restore_state_info_t *info,
                                  scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx2_gather_sequence_state_machine);
}

static bool
drx_event_restore_state(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info)
{
    instr_t inst;
    bool success = true;
    if (info->fragment_info.cache_start_pc == NULL)
        return true; /* fault not in cache */
    if (dr_atomic_load32(&drx_scatter_gather_expanded) == 0) {
        /* Nothing to do if nobody had never called expand_scatter_gather() before. */
        return true;
    }
    if (!info->fragment_info.app_code_consistent) {
        /* Can't verify application code.
         * XXX i#2985: is it better to keep searching?
         */
        return true;
    }
    instr_init(drcontext, &inst);
    byte *pc = decode(drcontext, dr_fragment_app_pc(info->fragment_info.tag), &inst);
    if (pc != NULL) {
        scatter_gather_info_t sg_info;
        if (instr_is_gather(&inst)) {
            get_scatter_gather_info(&inst, &sg_info);
            if (sg_info.is_evex) {
                success = success &&
                    drx_restore_state_for_avx512_gather(drcontext, info, &sg_info);
            } else {
                success = success &&
                    drx_restore_state_for_avx2_gather(drcontext, info, &sg_info);
            }
        } else if (instr_is_scatter(&inst)) {
            get_scatter_gather_info(&inst, &sg_info);
            success = success &&
                drx_restore_state_for_avx512_scatter(drcontext, info, &sg_info);
        }
    }
    instr_free(drcontext, &inst);
    return success;
}

#endif
