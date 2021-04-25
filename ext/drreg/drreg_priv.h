/* **********************************************************
 * Copyright (c) 2021 Google, Inc.   All rights reserved.
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

#ifndef _DRREG_PRIV_H_
#define _DRREG_PRIV_H_ 1

/* Don't use doxygen since this is an internal interface. */

#include "dr_api.h"
#include "drreg.h"

/***************************************************************************
 * DEFINITIONS AND DATA STRUCTURES
 */

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define LOG(dc, mask, level, ...) dr_log(dc, mask, level, __VA_ARGS__)
#else
#    define ASSERT(x, msg)            /* nothing */
#    define LOG(dc, mask, level, ...) /* nothing */
#endif

#ifdef WINDOWS
#    define DISPLAY_ERROR(msg) dr_messagebox(msg)
#else
#    define DISPLAY_ERROR(msg) dr_fprintf(STDERR, "%s\n", msg);
#endif

#define PRE instrlist_meta_preinsert

/* This is an arbitrary hard-coded upper limit of how many slots drreg is able to track.
 * This should accommodate all use-cases. Note, the client is responsible for reserving
 * enough slots for its use.
 */
#define ARBITRARY_UPPER_LIMIT (SPILL_SLOT_MAX + DR_NUM_GPR_REGS + 1)
#define MAX_SPILLS ARBITRARY_UPPER_LIMIT

/* We choose the number of available slots for spilling SIMDs to arbitrarily match
 * double their theoretical max number for a given build.
 *
 * Indirect spill area for SIMD is always allocated in TLS and therefore suitable for
 * cross-app. Note that this is in contrast to GPRs, which requires allocated raw thread
 * storage for cross-app spilling as DR slots are not guaranteed to preserve stored data
 * in such cases.
 */
#define MAX_SIMD_SPILLS (DR_NUM_SIMD_VECTOR_REGS * 2)

/* The Oth slot is always reserved for AFLAGS. */
#define AFLAGS_SLOT 0

/* Liveness states for gprs. */
#define REG_DEAD ((void *)(ptr_uint_t)0)
#define REG_LIVE ((void *)(ptr_uint_t)1)
#define REG_UNKNOWN ((void *)(ptr_uint_t)2) /* only used outside drmgr insert phase */

#ifdef X86
/* Defines whether SIMD and indirect spilling is supported by drreg. */
#    define SIMD_SUPPORTED
#endif

#ifdef SIMD_SUPPORTED
#    define XMM_REG_SIZE 16
#    define YMM_REG_SIZE 32
#    define ZMM_REG_SIZE 64
#    define SIMD_REG_SIZE ZMM_REG_SIZE
#    define _IF_SIMD_SUPPORTED(x) , x
#else
#    define _IF_SIMD_SUPPORTED(x)
/* FIXME i#3844: NYI on ARM */
#endif

typedef struct _drreg_internal_reg_info_t {
    /* XXX: better to flip around and store bitvector of registers per instr
     * in a single drvector_t?
     */
    /* The live vector holds one entry per app instr in the bb.
     * For registers, each vector entry holds REG_{LIVE,DEAD}.
     * For aflags, each vector entry holds a ptr_uint_t with the EFLAGS_READ_ARITH bits
     * telling which arithmetic flags are live at that point.
     */
    drvector_t live;
    bool in_use;
    uint app_uses; /* # of uses in this bb by app */
    /* With lazy restore, and b/c we must set native to false, we need to record
     * whether we spilled or not (we could instead record live_idx at time of
     * reservation).
     */
    bool ever_spilled;

    /* Where is the app value for this reg? */
    bool native;   /* app value is in original app reg */
    reg_id_t xchg; /* if !native && != REG_NULL, value was exchanged w/ this dead reg */
    int slot;      /* if !native && xchg==REG_NULL, value is in this TLS slot # */
} drreg_internal_reg_info_t;

typedef struct _drreg_internal_per_thread_t {
    instr_t *cur_instr;
    int live_idx;
    drreg_internal_reg_info_t reg[DR_NUM_GPR_REGS];
    drreg_internal_reg_info_t simd_reg[DR_NUM_SIMD_VECTOR_REGS];
    byte *simd_spill_start; /* storage returned by allocator (may not be aligned) */
    byte *simd_spills;      /* aligned storage for SIMD data */
    drreg_internal_reg_info_t aflags;
    reg_id_t slot_use[MAX_SPILLS]; /* holds the reg_id_t of which reg is inside */
    reg_id_t simd_slot_use[MAX_SIMD_SPILLS]; /* importantly, this can store partial SIMD
                                                registers  */
    int pending_unreserved;      /* count of to-be-lazily-restored unreserved gpr regs */
    int simd_pending_unreserved; /* count of to-be-lazily-restored unreserved SIMD regs */
    /* We store the linear address of our TLS for access from another thread: */
    byte *tls_seg_base;
    /* bb-local values */
    drreg_bb_properties_t bb_props;
    bool bb_has_internal_flow;
} drreg_internal_per_thread_t;

extern drreg_internal_per_thread_t drreg_internal_init_pt;

extern drreg_options_t drreg_internal_ops;

extern int tls_idx;

/* The raw tls segment offset of tls slots for gpr registers. */
extern uint drreg_internal_tls_slot_offs;
extern reg_id_t drreg_internal_tls_seg;
/* The raw tls segment offset of the pointer to the SIMD block indirect spill area. */
extern uint drreg_internal_tls_simd_offs;

typedef struct _drreg_internal_driver_t {

} drreg_internal_driver_t;

/***************************************************************************
 * SPILLING AND RESTORING
 */

drreg_status_t
drreg_internal_bb_insert_restore_all(
    void *drcontext, instrlist_t *bb, instr_t *inst, bool force_restore,
    OUT bool *regs_restored _IF_SIMD_SUPPORTED(OUT bool *simd_regs_restored));

/***************************************************************************
 * ANALYSIS AND CROSS-APP-INSTR
 */

void
count_app_uses(drreg_internal_per_thread_t *pt, opnd_t opnd);

/***************************************************************************
 * USE OUTSIDE INSERT PHASE
 */

/* For use outside drmgr's insert phase where we don't know the bounds of the
 * app instrs, we fall back to a more expensive liveness analysis on each
 * insertion.
 *
 * XXX: we'd want to add a new API for instru2instru that takes in
 * both the save and restore points at once to allow keeping aflags in
 * eax and other optimizations.
 */
drreg_status_t
drreg_forward_analysis(void *drcontext, instr_t *start);

/***************************************************************************
 * REGISTER RESERVATION
 */

/* Assumes liveness info is already set up in drreg_internal_per_thread_t. */
drreg_status_t
drreg_internal_reserve(void *drcontext, drreg_spill_class_t spill_class,
                       instrlist_t *ilist, instr_t *where, drvector_t *reg_allowed,
                       bool only_if_no_spill, OUT reg_id_t *reg_out);

/***************************************************************************
 * HELPER FUNCTIONS
 */

/* Returns whether an instruction is control transfer instruction. */
bool
drreg_internal_is_xfer(instr_t *inst);

drreg_internal_per_thread_t *
drreg_internal_get_tls_data(void *drcontext);

void
drreg_internal_report_error(drreg_status_t res, const char *msg);

#ifdef DEBUG
app_pc
get_where_app_pc(instr_t *where);
#endif

#endif /* _DRREG_PRIV_ */
