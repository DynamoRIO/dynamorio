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

/* DynamoRIO Register Management Extension: a mediator for
 * selecting, preserving, and using registers among multiple
 * instrumentation components.
 */

#ifndef _DRREG_H_
#define _DRREG_H_ 1

#include "drmgr.h"
#include "drvector.h"

/**
 * @file drreg.h
 * @brief Header for DynamoRIO Register Management Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drreg Register Usage Coordinator
 */
/**@{*/ /* begin doxygen group */

/** Success code for each drreg operation */
typedef enum {
    DRREG_SUCCESS,                     /**< Operation succeeded. */
    DRREG_ERROR,                       /**< Operation failed. */
    DRREG_ERROR_INVALID_PARAMETER,     /**< Operation failed: invalid parameter */
    DRREG_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRREG_ERROR_REG_CONFLICT,          /**< Operation failed: register conflict */
    DRREG_ERROR_IN_USE,                /**< Operation failed: resource already in use */
    DRREG_ERROR_OUT_OF_SLOTS,          /**< Operation failed: no more TLS slots */
    /**
     * Operation failed: app value not available.
     * Set \p conservative in \p drreg_options_t to avoid this error.
     */
    DRREG_ERROR_NO_APP_VALUE,
} drreg_status_t;

/***************************************************************************
 * INIT
 */

/**
 * Priorities of drmgr instrumentation passes used by drreg.  Users
 * of drreg can use the name DRMGR_PRIORITY_NAME_DRREG in the
 * drmgr_priority_t.before field or can use these numeric priorities
 * in the drmgr_priority_t.priority field to ensure proper
 * instrumentation pass ordering.
 */
enum {
    /** Priority of drreg analysis and pre-insert */
    DRMGR_PRIORITY_INSERT_DRREG_HIGH = -7500,
    /** Priority of drreg post-insert */
    DRMGR_PRIORITY_INSERT_DRREG_LOW = 7500,
    /** Priority of drreg fault handling event */
    DRMGR_PRIORITY_FAULT_DRREG = -7500,
};

/**
 * Name of drreg instrumentation pass priorities for analysis and insert
 * steps that are meant to take place before any tool actions.
 */
#define DRMGR_PRIORITY_NAME_DRREG_HIGH "drreg_high"

/**
 * Name of drreg instrumentation pass priorities for analysis and insert
 * steps that are meant to take place after any tool actions.
 */
#define DRMGR_PRIORITY_NAME_DRREG_LOW "drreg_low"

/**
 * Name of drreg fault handling event.
 */
#define DRMGR_PRIORITY_NAME_DRREG_FAULT "drreg_fault"

/** Specifies the options when initializing drreg. */
typedef struct _drreg_options_t {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /**
     * The number of TLS spill slots to use.  This many slots will be
     * requested from DR via dr_raw_tls_calloc().  Any slots needed beyond
     * this number will use DR's base slots, which are not allowed to be
     * used across application instructions.  DR's slots are also more
     * expensive to access (beyond the first few).  DR's base slots may
     * also be used by APIs like dr_save_reg()/dr_restore_reg() (and the
     * corresponding dr_read_saved_reg()/dr_write_saved_reg()), which may
     * cause correctness issues if there's some slot usage conflict within
     * the same client or with other clients/libraries.  Therefore, all
     * cooperating client components should use drreg.  Also, clients
     * should make sure to request sufficient dedicated slots from drreg.
     * This number should be computed as one plus the number of
     * simultaneously used general-purpose register spill slots, as
     * drreg reserves one of the requested slots for arithmetic flag
     * preservation.
     *
     * For each simultaneous value that will be held in a register
     * across application instructions, an additional slot must be
     * requested for adjusting the saved application value with
     * respect to application reads and writes.
     *
     * When drreg_init() is called multiple times, the number of slots is
     * summed from each call, unless \p do_not_sum_slots is specified for
     * that call, in which case a maximum is used rather than a sum.
     */
    uint num_spill_slots;
    /**
     * By default, drreg assumes that the application will not rely
     * on the particular value of a dead register when a fault happens.
     * This allows drreg to reduce overhead.  This flag can be set to
     * request that drreg not make this assumption.
     *
     * If multiple drreg_init() calls are made, this field is combined by
     * logical OR.
     *
     * XXX i#3801: If a fault occurs between two spills of a GPR or aflags,
     * where the second spill is after an app write, the current drreg
     * application state restoration logic does not restore the dead GPR/aflags
     * even with this field set to true. It is difficult to do so without
     * additional metadata passed to the state restoration callback.
     *
     */
    bool conservative;
    /**
     * Some drreg operations are performed during drmgr events where there is
     * no direct return value to the user of drreg.  When an error is encountered
     * at these times, drreg will call this callback and pass the error value.
     * If this callback is NULL, or if it returns false, drreg will call dr_abort().
     *
     * If multiple drreg_init() calls are made, only the first callback is
     * honored (thus, libraries generally should not set this).
     */
    bool (*error_callback)(drreg_status_t status);
    /**
     * Generally, library routines should take in scratch registers directly,
     * keeping drreg reservations in the end client.  However, sometimes this
     * model is not sufficient, and a library wants to directly reserve drreg
     * registers or aflags.  The library can ensure that drreg is initialized,
     * without forcing the client to do so when the client is not directly using
     * drreg already, by calling drreg_init() and setting \p do_not_sum_slots to
     * true.  This ensures that the total requested slots is at least \p
     * num_spill_slots, but if the total is already higher than that, the total
     * is left alone..  Thus, if clients invoke drreg_init() on their own, the
     * library will not needlessly add to the number of simultaneous slots
     * needed.
     */
    bool do_not_sum_slots;
} drreg_options_t;

DR_EXPORT
/**
 * Initializes the drreg extension.  Must be called prior to any of the other
 * routines.  Can be called multiple times (by separate components, normally)
 * but each call must be paired with a corresponding call to drreg_exit().  The
 * fields of \p ops are combined from multiple calls as described in the
 * documentation for each field.  Typically the end-user tool itself specifies
 * these options, with most other library components not directly interacting
 * with drreg (libraries often take in scratch registers from the caller for
 * most of their operations).
 *
 * @param[in] ops  Specifies the optional parameters that control how
 *   drreg operates.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_init(drreg_options_t *ops);

DR_EXPORT
/**
 * Cleans up the drreg extension.
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_exit(void);

DR_EXPORT
/**
 * In debug build, drreg tracks the maximum simultaneous number of spill
 * slots in use.  This can help a user to tune drreg_options_t.num_spill_slots.
 *
 * @param[out] max  The maximum number of spill slots used is written here.
 * @return whether successful or an error code on failure.  In release
 * build, this routine always fails.
 */
drreg_status_t
drreg_max_slots_used(OUT uint *max);

/***************************************************************************
 * ARITHMETIC FLAGS
 */

DR_EXPORT
/**
 * Requests exclusive use of the arithmetic flags register.  Spills
 * the application value at \p where in \p ilist, if necessary.
 * When used during drmgr's insertion phase, optimizations such as
 * keeping the application flags value in a register and eliding
 * duplicate spills and restores will be automatically applied.
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * TODO i#3823: Support multi-phase use. This will require adding support
 * to spill aflags to slots other than AFLAGS_SLOT.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_EXPORT
/**
 * Terminates exclusive use of the arithmetic flags register.
 * Restores the application value at \p where in \p ilist, if
 * necessary.
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_EXPORT
/**
 * Returns in \p value EFLAGS_READ_6 bits telling which arithmetic
 * flags are live at the point of \p inst.  If called during drmgr's
 * insertion phase, \p inst must be the current application
 * instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_aflags_liveness(void *drcontext, instr_t *inst, OUT uint *value);

DR_EXPORT
/**
 * Returns in \p dead whether the arithmetic flags are all dead at the
 * point of \p inst.  If called during drmgr's insertion phase, \p
 * inst must be the current application instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_are_aflags_dead(void *drcontext, instr_t *inst, bool *dead);

DR_EXPORT
/**
 * This routine ensures that the application's value for the arithmetic flags is
 * in place prior to \p where.  This is automatically done when the flags are
 * reserved prior to an application instruction, but sometimes instrumentation
 * needs to read the value of the flags.  This is intended as a convenience
 * barrier for lazy restores performed by drreg.
 *
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * TODO i#3823: Support multi-phase use. This will require adding support
 * to spill aflags to slots other than AFLAGS_SLOT.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_restore_app_aflags(void *drcontext, instrlist_t *ilist, instr_t *where);

/***************************************************************************
 * SCRATCH REGISTERS
 */

/** Flags passed to drreg_set_bb_properties(). */
typedef enum {
    /**
     * drreg was designed for linear control flow and assumes that it can safely
     * wait to restore an unreserved scratch register across application
     * instructions.  If a client inserts internal control flow that crosses
     * application instructions (hence "spanning"), and the client is not
     * explicitly ensuring that each forward jump contains the same set of saved
     * scratch registers at its source and target (typically done by saving all
     * scratch registers needed inside control flow prior to any forward
     * branches), the client should set this property either prior to the drmgr
     * insertion phase or as early as possible in the insertion phase.  Setting
     * this property causes application instructions to become barriers to spilled
     * scratch registers that have been unreserved but have not yet been lazily
     * restored.  drreg will still collapse adjacent spill+restore pairs for the
     * same app instr.
     */
    DRREG_CONTAINS_SPANNING_CONTROL_FLOW = 0x001,
    /**
     * drreg was designed for linear control flow.  Normally, drreg disables
     * optimizations if it sees any kind of internal control flow (viz., a branch
     * with an instr_t target) that was added during drmgr's app2app phase, which
     * includes flow added by drutil_expand_rep_string().  The primary consequence
     * of disabling optimizations means that application instructions become
     * barriers to spilled scratch registers that have been unreserved but have
     * not yet been lazily restored, which are restored prior to each application
     * instruction.  If this flag is set, drreg assumes that internal control flow
     * either does not cross application instructions or that the client is
     * ensuring that each forward jump contains the same set of saved scratch
     * registers at its source and target (typically done by saving all scratch
     * registers needed inside control flow prior to any forward branches).  Such
     * scratch registers are then restored prior to each application instruction.
     */
    DRREG_IGNORE_CONTROL_FLOW = 0x002,
    /**
     * Turns off register restoration at the end of the block.
     * Note that it is still required that registers have their original
     * values at the end of a basic block. Therefore, restoration needs
     * to be handled by the user manually, usually via drreg_restore_all().
     */
    DRREG_USER_RESTORES_AT_BB_END = 0x004,

    /**
     * Turns on stricter logic to find free register spill slots. This avoids
     * conflicts with slots used to spill some register value in prior instrumentation
     * passes. An example usage is in drx_expand_scatter_gather() which is used in
     * the app2app pass and requires spilling of registers to slots that may
     * conflict with slots used during later instrumentation passes. Using this
     * option also makes spill slots used in prior phases less available in
     * future phases; the current logic skips over a slot if there's a usage
     * found anywhere later in the bb added by any previous phase. So it requires
     * additional spill slots as well.
     */
    DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS = 0x008,
} drreg_bb_properties_t;

DR_EXPORT
/**
 * Requests exclusive use of an application register, spilling the
 * application value at \p where in \p ilist if necessary.  The
 * register chosen is returned in \p reg.
 *
 * When used during drmgr's insertion phase, optimizations such as
 * keeping the application flags value in a register and eliding
 * duplicate spills and restores will be automatically applied.
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * If \p reg_allowed is non-NULL, only registers from the specified
 * set will be considered, where \p reg_allowed must be a vector with
 * one entry for each general-purpose register in
 * [#DR_REG_START_GPR..#DR_REG_STOP_GPR] where a NULL entry indicates
 * not allowed and any non-NULL entry indicates allowed.  The
 * drreg_init_and_fill_vector() routine can be used to set up \p
 * reg_allowed.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                       drvector_t *reg_allowed, OUT reg_id_t *reg);

DR_EXPORT
/**
 * Identical to drreg_reserve_register() except returns failure if no
 * register is available that does not require a spill.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reserve_dead_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                            drvector_t *reg_allowed, OUT reg_id_t *reg);

DR_EXPORT
/**
 * Initializes \p vec to hold #DR_NUM_GPR_REGS entries, each either
 * set to NULL if \p allowed is false or a non-NULL value if \p
 * allowed is true.  This is intendend as a convenience routine for
 * setting up the \p reg_allowed parameter to
 * drreg_reserve_register().
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_init_and_fill_vector(drvector_t *vec, bool allowed);

DR_EXPORT
/**
 * Sets the entry in \p vec at index \p reg minus #DR_REG_START_GPR to
 * NULL if \p allowed is false or a non-NULL value if \p allowed is
 * true.  This is intendend as a convenience routine for setting up
 * the \p reg_allowed parameter to drreg_reserve_register().
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_set_vector_entry(drvector_t *vec, reg_id_t reg, bool allowed);

DR_EXPORT
/**
 * Inserts instructions at \p where in \p ilist to retrieve the
 * application value for \p app_reg into \p dst_reg.  This will
 * automatically be done for reserved registers prior to an
 * application instruction that reads \p app_reg, but sometimes
 * instrumentation needs to read the application value of a register
 * that has been reserved.  If \p app_reg is a dead register,
 * #DRREG_ERROR_NO_APP_VALUE may be returned. Set \p conservative in \p
 * drreg_options_t to avoid this error.
 *
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * On ARM, asking to place the application value of the register
 * returned by dr_get_stolen_reg() into itself is not supported.
 * Instead the caller should use dr_insert_get_stolen_reg_value() and
 * opnd_replace_reg() to swap the use of the stolen register within a
 * tool instruction with a scratch register.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t app_reg,
                    reg_id_t dst_reg);

DR_EXPORT
/**
 * This is a convenience routine that calls drreg_get_app_value() for
 * every register used by \p opnd, with that register passed as the
 * application and destination registers.  This routine will write to
 * reserved as well as unreserved registers.  This is intended as a
 * convenience barrier for lazy restores performed by drreg.
 *
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * On ARM, asking to place the application value of the register
 * returned by dr_get_stolen_reg() into itself is not supported.  If
 * \p opnd uses the stolen register, this routine will swap it for a
 * scratch register.  This scratch register will be \p *swap if \p
 * *swap is not #DR_REG_NULL; otherwise, drreg_reserve_register() with
 * NULL for \p reg_allowed will be called, and the result returned in
 * \p *swap.  It is up to the caller to un-reserve the register in
 * that case.
 *
 * @return whether successful or an error code on failure.  On failure,
 * any values that were already restored are not undone.
 */
drreg_status_t
drreg_restore_app_values(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t opnd,
                         INOUT reg_id_t *swap);

DR_EXPORT
/**
 * Restores the spilled value (typically the application value) for
 * all registers and flags at \p where.
 */
drreg_status_t
drreg_restore_all(void *drcontext, instrlist_t *bb, instr_t *where);

DR_EXPORT
/**
 * This routine is meant for use with instrumentation that uses separate control flow
 * paths, such as a fastpath and a slowpath, where the slowpath needs access to the
 * full application state yet must retain scratch register parity with the fastpath.
 * The application value for \p reg is restored into \p reg at \p where_restore, but
 * internal drreg state is not updated to reflect this.  Furthermore, if doing so
 * affects subsequent behavior, such as when \p reg is being used to hold the
 * preserved application value for another register or flags, instructions are
 * inserted at \p where_respill to restore the state, such that \p where_respill will
 * operate correctly whether \p where_restore was executed or not.  The optional
 * output parameters \p restore_needed and \p respill_needed are set to indicate
 * whether instructions were inserted at \p where_restore and \p where_respill,
 * respectively.
 *
 * For correct operation on x86 in the case when aflags are in xax and this routine
 * is invoked to get app value of xax, there shouldn't be any new reservation between
 * \p where_restore and \p where_respill that may write to a spill slot and clobber
 * the temporary slot used in this routine.
 *
 * The results from drreg_reservation_info_ex() can be used to predict the behavior
 * of this routine.  A restore is needed if !drreg_reserve_info_t.holds_app_value.
 * and drreg_reserve_info_t.app_value_retained.  A respill is needed if a restore is
 * needed and drreg_reserve_info_t.opnd is a register.
 *
 * If \p app_reg is a dead register, #DRREG_ERROR_NO_APP_VALUE may be returned. Set
 * \p conservative in \p drreg_options_t to avoid this error.
 *
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * To restore the arithmetic flags, pass #DR_REG_NULL for \p reg.
 *
 * On ARM, passing \p reg equal to dr_get_stolen_reg() is not supported.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_statelessly_restore_app_value(void *drcontext, instrlist_t *ilist, reg_id_t reg,
                                    instr_t *where_restore, instr_t *where_respill,
                                    bool *restore_needed OUT, bool *respill_needed OUT);

DR_EXPORT
/**
 * Invokes drreg_statelessly_restore_app_value() for the arithmetic flags and every
 * general-purpose register.  Returns the logical OR of the 'restore_needed' and
 * 'respill_needed' results from all of the drreg_statelessly_restore_app_value() calls.
 * If any step results in an error, that error is returned and the output parameters
 * are not filled in (despite partial restores potentially remaining in place).
 */
drreg_status_t
drreg_statelessly_restore_all(void *drcontext, instrlist_t *ilist, instr_t *where_restore,
                              instr_t *where_respill, bool *restore_needed OUT,
                              bool *respill_needed OUT);

DR_EXPORT
/**
 * Returns information about the TLS slot assigned to \p reg, which
 * must be a currently-reserved register. To query information about
 * the arithmetic flags, pass #DR_REG_NULL for \p reg.
 *
 * If \p opnd is non-NULL, returns an opnd_t in \p opnd that references
 * the TLS slot assigned to \p reg.  If too many slots are in use and
 * \p reg is stored in a non-directly-addressable slot, returns a null
 * #opnd_t in \p opnd.
 *
 * If \p is_dr_slot is non-NULL, returns true if the slot is a DR slot
 * (and can thus be accessed by dr_read_saved_reg()) and false if the
 * slot is inside the dr_raw_tls_calloc() allocation used by drreg.
 *
 * If \p tls_offs is non-NULL, if the slot is a DR slot, returns the
 * DR slot index; otherwise, returns the offset from the TLS base of
 * the slot assigned to \p reg.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reservation_info(void *drcontext, reg_id_t reg, opnd_t *opnd OUT,
                       bool *is_dr_slot OUT, uint *tls_offs OUT);

/**
 * Contains information about a register's reservation and restoration status.
 */
typedef struct _drreg_reserve_info_t {
    /** The size of this structure.  It must be set by the caller before querying. */
    size_t size;
    /** Whether the register is currently reserved. */
    bool reserved;
    /** Whether the register currently holds the application value. */
    bool holds_app_value;
    /**
     * Whether the application's value is stored somewhere (it may not be if the
     * register is dead and \p conservative is not set in \p drreg_options_t).
     */
    bool app_value_retained;
    /**
     * True if the register's TLS slot is a DR slot (and can thus be accessed by
     * dr_read_saved_reg()) and false if either the slot is inside the
     * dr_raw_tls_calloc() allocation used by drreg or the "slot" is another
     * register.
     */
    bool is_dr_slot;
    /**
     * An operand that references the location storing the application value.  If
     * too many slots are in use and the register is stored in a
     * non-directly-addressable slot, this is set to a null #opnd_t.
     * If the register is being kept in a different register, this will be a
     * register operand holding that location.
     * If \p app_value_retained is false, this is set to a null #opnd_t.
     */
    opnd_t opnd;
    /**
     * If the register's application value is not stored in a TLS slot, this is -1.
     * If so, and the slot is a DR slot, this holds the DR slot index; if it is not a
     * DR slot, this holds the offset from the TLS base of the slot assigned to the
     * register.
     */
    int tls_offs;
} drreg_reserve_info_t;

DR_EXPORT
/**
 * Returns information about the reservation and restoration status of
 * \p reg.  The \p size field of \p info must be set before calling.
 * To query information about the arithmetic flags, pass #DR_REG_NULL for \p reg.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reservation_info_ex(void *drcontext, reg_id_t reg, drreg_reserve_info_t *info OUT);

DR_EXPORT
/**
 * Terminates exclusive use of the register \p reg.  Restores the
 * application value at \p where in \p ilist, if necessary.  If called
 * during drmgr's insertion phase, \p where must be the current
 * application instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg);

DR_EXPORT
/**
 * Returns in \p dead whether the register \p reg is dead at the
 * point of \p inst.  If called during drmgr's insertion phase, \p
 * inst must be the current application instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_is_register_dead(void *drcontext, reg_id_t reg, instr_t *inst, bool *dead);

DR_EXPORT
/**
 * May only be called during drmgr's app2app, analysis, or insertion phase.
 * Sets the given properties for the current basic block being instrumented.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_set_bb_properties(void *drcontext, drreg_bb_properties_t flags);

DR_EXPORT
/**
 * Analyzes \p instr and returns whether it is a drreg register spill
 * in \p spill, a drreg register restore in \p restore, and which
 * register is being spilled or restored in \p reg_spilled.  Each
 * output parameter is optional and may be NULL.  If DR's spill slots
 * are being used (see drreg_options_t.num_spill_slots), this routine
 * may not be able to distinguish a drreg spill or restore from some
 * other spill or restore.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_is_instr_spill_or_restore(void *drcontext, instr_t *instr, bool *spill OUT,
                                bool *restore OUT, reg_id_t *reg_spilled OUT);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRREG_H_ */
