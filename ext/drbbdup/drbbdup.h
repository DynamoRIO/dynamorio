/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.   All rights reserved.
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

#ifndef _DRBBDUP_H_
#define _DRBBDUP_H_ 1

#include "drmgr.h"
#include <stdint.h>

/**
 * @file drbbdup.h
 * @brief Header for DynamoRIO Basic Block Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drbbdup Basic Block Duplicator
 */
/*@{*/ /* begin doxygen group */

/** Success code for each drbbdup operation */
typedef enum {
    DRBBDUP_SUCCESS,                       /**< Operation succeeded. */
    DRBBDUP_ERROR_INVALID_PARAMETER,       /**< Operation failed: invalid parameter. */
    DRBBDUP_ERROR_CASE_ALREADY_REGISTERED, /**< Operation failed: already registered. */
    DRBBDUP_ERROR_CASE_LIMIT_REACHED,      /**< Operation failed: case limit reached. */
    DRBBDUP_ERROR_ALREADY_INITIALISED,     /**< DRBBDUP can only be initialised once. */
    DRBBDUP_ERROR,                         /**< Operation failed. */
} drbbdup_status_t;

/***************************************************************************
 * User call-back functions
 */

/**
 * Set up initial information related to a managing copies of a new basic block.
 * A pointer-sized value indicating the default case encoding is returned.
 * The boolean value written to \p enable_dups specifies whether code duplication should
 * be done for this basic block. If false, the basic block is always executed under the
 * default case and no duplications are made. \p enable_dynamic_handling specifies whether
 * additional copies should be dynamically generated to handle new case encodings
 * identified during runtime. This option entails flushing but can lead to more efficient
 * instrumentation depending on the user's application of drbbdup.
 *
 * Use drbbdup_register_case_value() to register other case encodings.
 *
 * @param[in] enable_dups indicates whether basic block code is duplicated to support
 * multiple case instrumentation.
 * @param[in] enable_dynamic_handling indicates whether dynamic handling is enabled.
 * @return the default case encoding.
 */
typedef uintptr_t (*drbbdup_set_up_bb_dups_t)(void *drbbdup_ctx, void *drcontext,
                                              void *tag, instrlist_t *bb,
                                              bool *enable_dups,
                                              bool *enable_dynamic_handling,
                                              void *user_data);

/**
 * When a unregistered case is identified as a candidate for dynamic handling,
 * such a call-back function is invoked to give the user the opportunity
 * to stop its generation.
 *
 * @param new_case The encoding of the unhandled case.
 * @param[out] enable_dynamic_handling sets whether dynamic generation for unhandled
 * cases should overall left on or turned off for this particular fragment.
 * @return whether a basic block copy should be dynamically generated to handle
 * the unhanlded case \p new_case.
 */
typedef bool (*drbbdup_allow_gen_t)(void *drcontext, void *tag, instrlist_t *ilist,
                                    uintptr_t new_case, bool *enable_dynamic_handling,
                                    void *user_data);

/**
 * Inserts code responsible for encoding the current runtime
 * case. The function should store the resulting pointer-sized encoding to the memory
 * destination operand obtained via drbbdup_get_encoding_opnd(). If the user has
 * implemented the encoder via a clean call, drbbdup_set_encoding() should be
 * used.
 *
 * @param pre_analysis_data the preliminary analysis data.
 */
typedef void (*drbbdup_insert_encode_t)(void *drcontext, instrlist_t *bb, instr_t *where,
                                        void *user_data, void *pre_analysis_data);

/**
 * Conducts a preliminary analysis on the basic block. The call-back is not called for
 * each case, but once for the overall fragment. Therefore, computationally expensive
 * analysis can be done by this call-back and made available to all cases of the basic
 * block.
 *
 * The user can use thread allocation for storing the pre analysis result.
 *
 * @param[in] pre_analysis_data the pointer to store the result of the preliminary
 * analysis.
 * @return whether successful or an error code on failure.
 */
typedef void (*drbbdup_pre_analyse_bb_t)(void *drcontext, instrlist_t *bb,
                                         void *user_data, void **pre_analysis_data);

/**
 * Destroys preliminary analysis data.
 *
 * @param pre_analysis_data the preliminary analysis data to destroy.
 */
typedef void (*drbbdup_destroy_pre_analysis_t)(void *drcontext, void *user_data,
                                               void *pre_analysis_data);

/**
 * Conducts an analysis on a basic block with respect to a case with encoding \p encoding.
 * The result of the analysis needs to be stored to \p analysis_data.
 *
 * @param pre_analysis_data the preliminary analysis data to destroy.
 */
typedef void (*drbbdup_analyse_bb_t)(void *drcontext, instrlist_t *bb, uintptr_t encoding,
                                     void *user_data, void *pre_analysis_data,
                                     void **analysis_data);

/**
 * Destroys analysis data \p analysis_data for the case with encoding \p encoding.
 *
 * @param pre_analysis_data preliminary analysis data.
 * @param encoding the case encoding that is associated with the analysis data.
 * @param analysis_data the analysis data to destroy.
 *
 * \note The user should not destroy pre_analysis_data.
 */
typedef void (*drbbdup_destroy_analysis_t)(void *drcontext, void *user_data,
                                           uintptr_t encoding, void *pre_analysis_data,
                                           void *analysis_data);

/**
 * A user-defined call-back function that is invoked to instrument an instruction \p
 * instr. The inserted code must be placed at \p where.
 *
 * Instrumentation must be driven according to the passed case encoding \p encoding.
 *
 * @param instr the instr to be instrumented.
 * @param where the location where to insert instrumentation.
 * @param encoding the encoding of the current case considered.
 * @param pre_analysis_data preliminary analysis data.
 * @param analysis_data analysis data for the particular case.
 */
typedef void (*drbbdup_instrument_instr_t)(void *drcontext, instrlist_t *bb,
                                           instr_t *instr, instr_t *where,
                                           uintptr_t encoding, void *user_data,
                                           void *pre_analysis_data, void *analysis_data);

/***************************************************************************
 * INIT
 */

/**
 * Specifies the options when initialising drbbdup. \p init_bb_dups and \p insert_encode
 * cannot be NULL, while \p dup_limit must be greater than zero.
 */
typedef struct {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /** A user-defined call-back function that sets up how to duplicate a basic block.
     * Cannot be NULL.
     */
    drbbdup_set_up_bb_dups_t set_up_bb_dups;
    /**
     * A user-defined call-back function that inserts code to encode the runtime case.
     * The resulting encoding is used by the dispatcher to direct control to the
     * appropriate basic block. Cannot be NULL.
     */
    drbbdup_insert_encode_t insert_encode;
    /**
     * A user-defined call-back function that conducts a preliminary analysis on a basic
     * block.
     */
    drbbdup_pre_analyse_bb_t pre_analyse_bb;
    /**
     * A user-defined call-back function that destroys preliminary analysis data.
     */
    drbbdup_destroy_pre_analysis_t destroy_pre_analysis;
    /**
     * A user-defined call-back function that analyses a basic block for a particular
     * case.
     */
    drbbdup_analyse_bb_t analyse_bb;
    /**
     * A user-defined call-back function that destroys analysis data for a particular
     * case.
     */
    drbbdup_destroy_analysis_t destroy_analysis;
    /**
     * A user-defined call-back function that instruments an instruction with respect to a
     * particular case.
     */
    drbbdup_instrument_instr_t instrument_instr;
    /**
     * A user-defined call-back function that determines whether to dynamically generate
     * a basic block to handle a new case.
     */
    drbbdup_allow_gen_t allow_gen;
    /* User-data made available to user-defined call-back functions that drbbdup invokes
     * to manage basic block duplication.
     */
    void *user_data;
    /* The maximum number of cases, excluding the default case, that can be associated
     * with a basic block. Once the limit is reached and an unhandled case is encountered,
     * control is directed to the default case.
     */
    ushort dup_limit;
    /**
     * The number of times an unhandled case is encountered by a single thread before it
     * becomes a candidate for dynamic generation.
     */
    ushort hit_threshold;
} drbbdup_options_t;

/**
 * Priorities of drmgr instrumentation passes used by drbbdup. Users
 * can perform app2app manipulations by ordering such changes before
 * DRMGR_PRIORITY_DUPLICATE_DRBBDUP.
 */
enum {
    /** Priority of drbbdup. */
    DRMGR_PRIORITY_DRBBDUP = -1500
};

/**
 * Name of drbbdup priorities for analysis and insert
 * steps that are meant to take place before any tool actions.
 */
#define DRMGR_PRIORITY_NAME_DRBBDUP "drbbdup"

DR_EXPORT
/**
 * Initialises the drbbdup extension. Must be called before use of any other routines.
 *
 * It cannot be called multiple times as duplication management is specific to a single
 * use-case where only a one default case encoding is associated with each basic block.
 *
 * The \p ops_in parameter are options which dictate how drbbdup manages
 * bb copies. The following user-defined call-backs cannot be NULL: \p init_bb_dups
 *
 * @param ops_in options which dictate how drbbdup operates.
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_init(drbbdup_options_t *ops_in);

DR_EXPORT
/**
 * Cleans up the drbbdup extension.
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_exit(void);

/***************************************************************************
 * ENCODING
 */

DR_EXPORT
/**
 * Sets the runtime case encoding.
 *
 * Must be called from a clean call inserted via a drbbdup_insert_encode_t call-back
 * function.
 *
 * @param encoding the encoding to be set.
 */
drbbdup_status_t
drbbdup_set_encoding(uintptr_t encoding);

DR_EXPORT
/**
 * Retrieves a memory destination operand which should be used to set the runtime case
 * encoding.
 *
 * Must be called from code stemming from a drbbdup_insert_encode_t call-back function.
 *
 * @return a destination operand that refers to a memory location where the encoding
 * should be stored.
 */
opnd_t
drbbdup_get_encoding_opnd();

DR_EXPORT
/**
 * Registers a non-default case encoding. The function should only be called by a
 * drbbdup_set_up_bb_dups_t call-back function which provides \p drbbdup_ctx.
 *
 * @param drbbdup_ctx the drbbdup context of a basic block fragment.
 * @param encoding the case encoding to be registered.
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_register_case_encoding(void *drbbdup_ctx, uintptr_t encoding);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the first instruction of a
 * basic block copy. The result is returned in \p is_start.
 *
 * @param instr the instruction to check.
 * @param[out] is_start indicates whether instr is first.
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_is_first_instr(void *drcontext, instr_t *instr, bool *is_start);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the last instruction of basic
 * block copy. The result is returned in \p is_last.
 *
 * @param instr the instruction to check.
 * @param[out] is_last indicates whether instr is last.
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_is_last_instr(void *drcontext, instr_t *instr, bool *is_last);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRBBDUP_H_ */
