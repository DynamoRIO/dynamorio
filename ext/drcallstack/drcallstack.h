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

/* DynamoRIO Callstack Walker. */

#ifndef _DRCALLSTACK_H_
#define _DRCALLSTACK_H_ 1

/**
 * @file drcallstack.h
 * @brief Header for DynamoRIO Callstack Walker
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drcallstack Callstack Walker
 */
/**@{*/ /* begin doxygen group */

/** Success code for each drcallstack operation. */
typedef enum {
    DRCALLSTACK_SUCCESS,                     /**< Operation succeeded. */
    DRCALLSTACK_NO_MORE_FRAMES,              /**< No further frames found. */
    DRCALLSTACK_ERROR,                       /**< Operation failed. */
    DRCALLSTACK_ERROR_INVALID_PARAMETER,     /**< Operation failed: invalid parameter */
    DRCALLSTACK_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
} drcallstack_status_t;

/***************************************************************************
 * INIT
 */

/** Specifies the options when initializing drcallstack. */
typedef struct _drcallstack_options_t {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /* We expect to add more options in the future. */
} drcallstack_options_t;

/** Describes one callstack frame. */
typedef struct _drcallstack_frame_t {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /** The program counter. */
    app_pc pc;
    /** The stack address for the start of the frame. */
    reg_t sp;
} drcallstack_frame_t;

/** Opaque type. */
struct _drcallstack_walk_t;
/** Opaque type. */
typedef struct _drcallstack_walk_t drcallstack_walk_t;

DR_EXPORT
/**
 * Initializes the drcallstack extension.  Must be called prior to any of the other
 * routines.  Can be called multiple times (by separate components, normally)
 * but each call must be paired with a corresponding call to drcallstack_exit().
 *
 * @param[in] ops  Specifies the optional parameters that control how
 *   drcallstack operates.
 *
 * @return whether successful or an error code on failure.
 */
drcallstack_status_t
drcallstack_init(drcallstack_options_t *ops);

DR_EXPORT
/**
 * Cleans up the drcallstack extension.
 * @return whether successful or an error code on failure.
 */
drcallstack_status_t
drcallstack_exit(void);

DR_EXPORT
/**
 * Initializes a new callstack walk with the passed-in context 'mc', which
 * must have #DR_MC_CONTROL and #DR_MC_INTEGER filled in.
 * The 'walk' pointer should then be passed to repeated calls to
 * drcallstack_next_frame() until it returns #DRCALLSTACK_NO_MORE_FRAMES.
 * drcallstack_cleanup_walk() should then be called to free up resources.
 *
 * \note Currently callstack walking is only available for Linux.
 */
drcallstack_status_t
drcallstack_init_walk(dr_mcontext_t *mc, OUT drcallstack_walk_t **walk);

DR_EXPORT
/**
 * Called when the 'walk' pointer is no longer needed.
 */
drcallstack_status_t
drcallstack_cleanup_walk(drcallstack_walk_t *walk);

DR_EXPORT
/**
 * First, call drcallstack_init_walk() to initialize 'walk'.
 * The 'walk' pointer should then be passed to repeated calls to
 * drcallstack_next_frame() until it returns #DRCALLSTACK_NO_MORE_FRAMES
 * or an error code.
 * drcallstack_cleanup_walk() should then be called to free up resources.
 *
 * \note Currently callstack walking is only available for Linux.
 */
drcallstack_status_t
drcallstack_next_frame(drcallstack_walk_t *walk, OUT drcallstack_frame_t *frame);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRCALLSTACK_H_ */
