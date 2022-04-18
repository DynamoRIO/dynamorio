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

#ifndef _DRSTATECMP_H_
#define _DRSTATECMP_H_

/**
 * @file drstatecmp.h
 * @brief Header for DynamoRIO Machine State Comparison Library
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drstatecmp Machine State Comparison Library
 */
/**@{*/ /* begin doxygen group */

/** Success code for each drstatecmp operation. */
typedef enum {
    DRSTATECMP_SUCCESS,                   /**< Operation succeeded. */
    DRSTATECMP_ERROR,                     /**< Operation failed. */
    DRSTATECMP_ERROR_ALREADY_INITIALIZED, /**< drstatecmp can only be initialized once. */
    DRSTATECMP_ERROR_NOT_INITIALIZED,     /**< Operation failed: not initialized. */
} drstatecmp_status_t;

/***************************************************************************
 * INIT
 */

/** Specifies the options when initializing drstatecmp. */
typedef struct {
    /**
     * When a state comparison fails, drstatecmp will call this callback and pass the
     * error message. If this callback is NULL, drstatecmp will call #DR_ASSERT_MSG.
     */
    void (*error_callback)(const char *msg, void *tag);

} drstatecmp_options_t;

/**
 * Priorities of drmgr instrumentation passes used by drstatecmp.  Users
 * of drstatecmp can use the name #DRMGR_PRIORITY_NAME_DRSTATECMP in the
 * #drmgr_priority_t.before field or can use these numeric priorities
 * in the #drmgr_priority_t.priority field to ensure proper pass ordering.
 */
/**
 * Priority of drstatecmp passes. Requires the highest priority among all app2app passes.
 */
#define DRMGR_PRIORITY_DRSTATECMP -8000
/** Name of drstatecmp pass priorities. */
#define DRMGR_PRIORITY_NAME_DRSTATECMP "drstatecmp_prio"

DR_EXPORT
/**
 * Determines whether a given basic block can be checked by the drstatecmp extension.
 */
bool
drstatecmp_bb_checks_enabled(instrlist_t *bb);

DR_EXPORT
/**
 * Initializes the drstatecmp extension.  Must be called prior to any of the other
 * routines.  Can be called only once and must be paired with a corresponding call to
 * drstatecmp_exit().
 *
 * @return whether successful or an error code on failure.
 */
drstatecmp_status_t
drstatecmp_init(drstatecmp_options_t *ops_in);

DR_EXPORT
/**
 * Cleans up the drstatecmp extension.
 *
 * @return whether successful or an error code on failure.
 */
drstatecmp_status_t
drstatecmp_exit(void);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRSTATECMP_H_ */
