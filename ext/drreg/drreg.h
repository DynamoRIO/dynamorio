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
    DRMGR_PRIORITY_INSERT_DRREG_HIGH =  -750,
    /** Priority of drreg post-insert */
    DRMGR_PRIORITY_INSERT_DRREG_LOW  =   750,
    /** Priority of drreg fault handling event */
    DRMGR_PRIORITY_FAULT_DRREG       =  -750,
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
     * this number will use DR's base slots (which beyond the first few
     * are more expensive to access).
     */
    uint num_spill_slots;
    /**
     * By default, drreg assumes that the application will not rely
     * on the particular value of a dead register when a fault happens.
     * This allows drreg to reduce overhead.  This flag can be set to
     * request that drreg not make this assumption.
     */
    bool conservative;
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
 * In debug build, drreg tracks the maximum simultanous number of spill
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
 * Must be called during drmgr's insertion phase.  Requests exclusive
 * use of the arithmetic flags register.  Spills the application value
 * at \p where in \p ilist, if necessary.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Terminates
 * exclusive use of the arithmetic flags register.  Restores the
 * application value at \p where in \p ilist, if necessary.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_unreserve_aflags(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Returns in \p value
 * EFLAGS_READ_6 bits telling which arithmetic flags are live at the current
 * drmgr instruction.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_aflags_liveness(void *drcontext, OUT uint *value);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Returns in \p dead
 * whether the arithmetic flags are all dead at the point of \p inst.
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
 * Must be called during drmgr's insertion phase.  Requests exclusive
 * use of an application register, spilling the application value at
 * \p where in \p ilist if necessary.  The register chosen is returned
 * in \p reg.  If \p reg_allowed is non-NULL, only registers from the
 * specified set will be considered, where \p reg_allowed must be a
 * vector with one boolean entry for each general-purpose register in
 * [DR_REG_START_GPR..DR_REG_STOP_GPR].
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_reserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                       drvector_t *reg_allowed, OUT reg_id_t *reg);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Inserts
 * instructions at \p where in \p ilist to retrieve the application
 * value for \p app_reg into \p dst_reg.  This will automatically be
 * done for reserved registers prior to an application instruction
 * that reads \p app_reg, but sometimes instrumentation needs to read
 * the application value of a register that has been reserved.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_get_app_value(void *drcontext, instrlist_t *ilist, instr_t *where,
                    reg_id_t app_reg, OUT reg_id_t *dst_reg);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Terminates
 * exclusive use of the register \p reg.  Restores the application
 * value at \p where in \p ilist, if necessary.
 *
 * @return whether successful or an error code on failure.
 */
drreg_status_t
drreg_unreserve_register(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg);


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRREG_H_ */
