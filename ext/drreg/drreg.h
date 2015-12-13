/* **********************************************************
 * Copyright (c) 2013-2015 Google, Inc.   All rights reserved.
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
/*@{*/ /* begin doxygen group */

/** Success code for each drreg operation */
typedef enum {
    DRREG_SUCCESS,                  /**< Operation succeeded. */
    DRREG_ERROR,                    /**< Operation failed. */
    DRREG_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter */
    DRREG_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRREG_ERROR_REG_CONFLICT,       /**< Operation failed: register conflict */
    DRREG_ERROR_IN_USE,             /**< Operation failed: resource already in use */
    DRREG_ERROR_OUT_OF_SLOTS,       /**< Operation failed: no more TLS slots */
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
    DRMGR_PRIORITY_INSERT_DRREG_HIGH =  -7500,
    /** Priority of drreg post-insert */
    DRMGR_PRIORITY_INSERT_DRREG_LOW  =   7500,
    /** Priority of drreg fault handling event */
    DRMGR_PRIORITY_FAULT_DRREG       =  -7500,
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
     * expensive to access (beyond the first few).
     *
     * For each simultaneous value that will be held in a register
     * across application instructions, an additional slot must be
     * requested for adjusting the saved application value with
     * respect to application reads and writes.
     */
    uint num_spill_slots;
    /**
     * By default, drreg assumes that the application will not rely
     * on the particular value of a dead register when a fault happens.
     * This allows drreg to reduce overhead.  This flag can be set to
     * request that drreg not make this assumption.
     */
    bool conservative;
    /**
     * Some drreg operations are performed during drmgr events where there is
     * no direct return value to the user of drreg.  When an error is encountered
     * at these times, drreg will call this callback and pass the error value.
     * If this callback is NULL, or if it returns false, drreg will call dr_abort().
     */
    bool (*error_callback)(drreg_status_t status);
} drreg_options_t;

DR_EXPORT
/**
 * Initializes the drreg extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drreg_exit().
 *
 * @param[in] ops  Specifies the optional parameters that control how
 *   drreg operates.  Only the first caller's options are honored.
 *   Typically this is the end-user tool itself, with most other
 *   library components not directly interacting with drreg (libraries
 *   often take in scratch registers from the caller for most of their
 *   operations).
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

/***************************************************************************
 * SCRATCH REGISTERS
 */

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
 * DRREG_ERROR_NO_APP_VALUE may be returned. Set \p conservative in \p
 * drreg_options_t to avoid this error.
 *
 * If called during drmgr's insertion phase, \p where must be the
 * current application instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where,
                    reg_id_t app_reg, reg_id_t dst_reg);

DR_EXPORT
/**
 * Returns information about the TLS slot assigned to \p reg, which
 * must be a currently-reserved register.
 *
 * If \p opnd is non-NULL, returns an opnd_t in \p opnd that references
 * the TLS slot assigned to \p reg.
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


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRREG_H_ */
