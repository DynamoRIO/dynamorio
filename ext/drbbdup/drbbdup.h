/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc.   All rights reserved.
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
 * @brief Header for DynamoRIO Basic Block Duplicator Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

#define drmgr_is_first_instr \
    DO_NOT_USE_drmgr_is_first_instr_USE_drbbdup_is_first_instr_instead
#define drmgr_is_first_nonlabel_instr \
    DO_NOT_USE_drmgr_is_first_nonlabel_instr_USE_drbbdup_is_first_nonlabel_instr_instead
#define drmgr_is_last_instr \
    DO_NOT_USE_drmgr_is_last_instr_USE_drbbdup_is_last_instr_instead

/**
 * \addtogroup drbbdup Basic Block Duplicator
 */
/**@{*/ /* begin doxygen group */

/** Success code for each drbbdup operation. */
typedef enum {
    DRBBDUP_SUCCESS,                       /**< Operation succeeded. */
    DRBBDUP_ERROR_INVALID_PARAMETER,       /**< Operation failed: invalid parameter. */
    DRBBDUP_ERROR_INVALID_OPND,            /**< Operation failed: invalid case opnd. */
    DRBBDUP_ERROR_CASE_ALREADY_REGISTERED, /**< Operation failed: already registered. */
    DRBBDUP_ERROR_CASE_LIMIT_REACHED,      /**< Operation failed: case limit reached. */
    DRBBDUP_ERROR_ALREADY_INITIALISED, /**< DRBBDUP can only be initialised once. This is
                                          a fatal error. */
    DRBBDUP_ERROR,                     /**< Operation failed. */
    DRBBDUP_ERROR_UNSET_FEATURE,       /**< Operation failed: feature not set. */
    DRBBDUP_ERROR_NOT_INITIALIZED,     /**< Operation failed: not initialized. */
} drbbdup_status_t;

/***************************************************************************
 * User call-back functions
 */

/**
 * Set up initial information related to managing copies of a new basic block \p bb.
 * A pointer-sized value indicating the default case encoding is returned.
 * The boolean value written to \p enable_dups specifies whether code duplication should
 * be done for this particular basic block. If false, the basic block is always executed
 * under the default case and no duplications are made. The flag
 * \p enable_dynamic_handling specifies whether additional copies should be
 * dynamically generated to handle new case encodings identified during runtime. This
 * option entails flushing but can lead to more efficient instrumentation depending
 * on the user's application of drbbdup. The user data \p user_data is that supplied
 * to drbbdup_init().
 *
 * Use drbbdup_register_case_value(), passing \p drbbdup_ctx, to register other case
 * encodings.
 *
 * @return the default case encoding.
 */
typedef uintptr_t (*drbbdup_set_up_bb_dups_t)(void *drbbdup_ctx, void *drcontext,
                                              void *tag, instrlist_t *bb,
                                              IN bool *enable_dups,
                                              IN bool *enable_dynamic_handling,
                                              void *user_data);

/**
 * When a unregistered case \p new_case is identified as a candidate for dynamic handling,
 * such a call-back function is invoked to give the user the opportunity
 * to go ahead or stop the generation of an additional basic block copy.
 * The call-back should return true if generation should be done, and false otherwise.
 * In addition, the call-back can also turn off dynamic handling for the considered basic
 * block by setting \p enable_dynamic_handling to false.
 */
typedef bool (*drbbdup_allow_gen_t)(void *drcontext, void *tag, instrlist_t *ilist,
                                    uintptr_t new_case, bool *enable_dynamic_handling,
                                    void *user_data);

/**
 * Conducts an analysis of the original basic block. The call-back is not called for
 * each case, but once for the overall fragment. Therefore, computationally expensive
 * analysis that only needs to be done once per fragment can be implemented by this
 * call-back and made available to all cases of the basic block. The function should
 * store the analysis result in \p orig_analysis_data. The  user data \p user_data is
 * that supplied to drbbdup_init().
 *
 * It is not possible to insert note labels via this analysis call-back function.
 * Any labels inserted will not persist. Such functionality is only possible via a
 * #drbbdup_analyze_case_t call-back.
 *
 * The user can use thread allocation for storing the analysis result.
 *
 * The analysis data is destroyed via a #drbbdup_destroy_orig_analysis_t function.
 *
 * @return whether successful or an error code on failure.
 */
typedef void (*drbbdup_analyze_orig_t)(void *drcontext, void *tag, instrlist_t *bb,
                                       void *user_data, IN void **orig_analysis_data);

/**
 * Destroys analysis data \p orig_analysis_data.
 *
 * The function is not invoked by drbbdup if \p orig_analysis_data was set
 * to NULL by the the #drbbdup_analyze_orig_t function.
 *
 */
typedef void (*drbbdup_destroy_orig_analysis_t)(void *drcontext, void *user_data,
                                                void *orig_analysis_data);

/**
 * Conducts an analysis on a basic block with respect to a case with encoding \p encoding.
 * The result of the analysis needs to be stored in \p case_analysis_data.
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data that was conducted on the original bb is also provided.
 *
 * The user can use thread allocation for storing the analysis result.
 *
 * The analysis data is destroyed via a #drbbdup_analyze_case_t function.
 */
typedef void (*drbbdup_analyze_case_t)(void *drcontext, void *tag, instrlist_t *bb,
                                       uintptr_t encoding, void *user_data,
                                       void *orig_analysis_data,
                                       IN void **case_analysis_data);

/**
 * Identical to #drbbdup_analyze_case_t except for two extra parameters, "for_trace"
 * and "translating", and the return value.  These all match the same parameters and
 * return values used with #drmgr_analysis_cb_t and dr_register_bb_event().
 * The returned flags will be merged in the same manner as for #drmgr_analysis_cb_t.
 *
 */
typedef dr_emit_flags_t (*drbbdup_analyze_case_ex_t)(void *drcontext, void *tag,
                                                     instrlist_t *bb, bool for_trace,
                                                     bool translating, uintptr_t encoding,
                                                     void *user_data,
                                                     void *orig_analysis_data,
                                                     IN void **case_analysis_data);

/**
 * Destroys analysis data \p case_analysis_data for the case with encoding \p encoding.
 *
 * The function is not invoked by drbbdup if \p case_analysis_data was set to NULL by the
 * #drbbdup_analyze_case_t function.
 *
 * The  user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data that was conducted on the original bb is also provided.
 *
 * \note The user should not destroy orig_analysis_data.
 */
typedef void (*drbbdup_destroy_case_analysis_t)(void *drcontext, uintptr_t encoding,
                                                void *user_data, void *orig_analysis_data,
                                                void *case_analysis_data);

/**
 * Inserts code responsible for encoding the current runtime
 * case at point of entry to the dispatcher. The function should
 * store the resulting pointer-sized encoding to memory that is
 * directly accessible via the reference operand passed to drbbdup_init().
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data, which was conducted on the original bb, is also provided.
 *
 * \note This call-back is optional and if set to NULL when initializing drbbdup,
 * the runtime case encoding is just loaded. The memory storing the runtime case
 * encoding is not modified by drbbdup.
 */
typedef void (*drbbdup_insert_encode_t)(void *drcontext, void *tag, instrlist_t *bb,
                                        instr_t *where, void *user_data,
                                        void *orig_analysis_data);

/**
 * A user-defined call-back function that is invoked to instrument an instruction \p
 * instr. The inserted code must be placed at \p where.
 *
 * Instrumentation must be driven according to the passed case encoding \p encoding.
 *
 * The user data \p user_data is that supplied to drbbdup_init(). Analysis data
 * \p orig_analysis_data and \p case_analysis_data are also provided.
 *
 */
typedef void (*drbbdup_instrument_instr_t)(void *drcontext, void *tag, instrlist_t *bb,
                                           instr_t *instr, instr_t *where,
                                           uintptr_t encoding, void *user_data,
                                           void *orig_analysis_data,
                                           void *case_analysis_data);

/**
 * Identical to #drbbdup_instrument_instr_t except for two extra parameters, "for_trace"
 * and "translating", and the return value.  These all match the same parameters and
 * return values used with #drmgr_insertion_cb_t and dr_register_bb_event().
 * The returned flags will be merged in the same manner as for #drmgr_insertion_cb_t.
 *
 */
typedef dr_emit_flags_t (*drbbdup_instrument_instr_ex_t)(
    void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, instr_t *where,
    bool for_trace, bool translating, uintptr_t encoding, void *user_data,
    void *orig_analysis_data, void *case_analysis_data);

/***************************************************************************
 * INIT
 */

/**
 * Specifies the options when initialising drbbdup. \p set_up_bb_dups
 * and \p instrument_instr cannot be NULL.  \p runtime_case_opnd must be
 * a pointer-sized memory reference, unless \p non_default_case_limit is 0.
 */
typedef struct {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /**
     * A user-defined call-back function that sets up how to duplicate a basic block.
     * Cannot be NULL.
     */
    drbbdup_set_up_bb_dups_t set_up_bb_dups;
    /**
     * A user-defined call-back function that inserts code to encode the runtime case.
     * The resulting encoding is used by the dispatcher to direct control to the
     * appropriate basic block.
     *
     * It can be left NULL. In such cases, it is expected that the runtime case encoding
     * of a thread is done by external code and updated on demand. Essentially,
     * drbbdup guarantees that it won't change the client's memory that stores the
     * encoding, thus enabling insert_encode to perform no operation and not be needed.
     */
    drbbdup_insert_encode_t insert_encode;
    /**
     * A user-defined call-back function that conducts an analysis of the original basic
     * block.
     */
    drbbdup_analyze_orig_t analyze_orig;
    /**
     * A user-defined call-back function that destroys analysis data of the original basic
     * block.
     */
    drbbdup_destroy_orig_analysis_t destroy_orig_analysis;
    /**
     * A user-defined call-back function that analyzes a basic block for a particular
     * case.
     */
    drbbdup_analyze_case_t analyze_case;
    /**
     * A user-defined call-back function that destroys analysis data for a particular
     * case.
     */
    drbbdup_destroy_case_analysis_t destroy_case_analysis;
    /**
     * A user-defined call-back function that instruments an instruction with respect to a
     * particular case.  Either this or the instrument_instr_ex field must be set.
     */
    drbbdup_instrument_instr_t instrument_instr;
    /**
     * A user-defined call-back function that determines whether to dynamically generate
     * a basic block copy to handle a new case encountered at runtime. The function may
     * be NULL, and in this case drbbdup will always consider dynamic handling for new
     * cases.
     */
    drbbdup_allow_gen_t allow_gen;
    /**
     * An operand that refers to the memory containing the current runtime case encoding.
     * During runtime, the dispatcher loads the runtime encoding via this operand
     * in order to direct control to the appropriate basic block. The opnd must be
     * pointer-sized.
     */
    opnd_t runtime_case_opnd;
    /**
     * Instructs drbbdup whether or not the loading of the runtime case should use
     * release-acquire semantics.
     */
    bool atomic_load_encoding;
    /**
     * User-data made available to user-defined call-back functions that drbbdup invokes
     * to manage basic block duplication.
     */
    void *user_data;
    /**
     * The maximum number of alternative cases, excluding the default case, that can be
     * associated with a basic block. Once the limit is reached and an unhandled case is
     * encountered, control is directed to the default case.  If this is set to 0,
     * no duplication is performed on any block, and 0 is passed as the encoding to the
     * \p analyze_case and \p instrument_instr (and their extended version) callbacks.
     */
    ushort non_default_case_limit;
    /**
     * Approximately, the number of times an unhandled case should be encountered by a
     * thread before it becomes a candidate for dynamic generation.
     */
    ushort hit_threshold;
    /**
     * Determines whether drbbdup should track a variety of statistics. Note, keeping
     * track of statistics incurs additional overhead and it is not recommended at
     * deployment.
     *
     * In order for the client to successfully call drbbdup_get_stats(), the flag must be
     * set to true.
     */
    bool is_stat_enabled;
    /**
     * Gives an upper bound on the value of all case encodings.  This is used to
     * optimize the dispatch code on AArchXX: in particular, an upper bound <256
     * avoids an extra scratch register.  Set to 0 to indicate there is no bound.
     */
    uintptr_t max_case_encoding;
    /**
     * Identical to analyze_case but taking extra parameters and with a return value.
     * Only one of this field or the analyze_case field can be set.
     */
    drbbdup_analyze_case_ex_t analyze_case_ex;
    /**
     * Identical to instrument_instr but taking extra parameters and with a return value.
     * Either this or the instrument_instr field must be set.
     */
    drbbdup_instrument_instr_ex_t instrument_instr_ex;
    /**
     * If \p enable_dynamic_handling will *never* be set by \p set_up_bb_dups for
     * *any* basic block, this field can be set to true.  This reduces memory
     * usage by not allocating bookkeeping data needed for dynamic handling.
     */
    bool never_enable_dynamic_handling;
} drbbdup_options_t;

/**
 * Various statistics related to drbbdup.
 */
typedef struct {
    /** Set this to the size of this structure. */
    size_t struct_size;
    /** Number of fragments which have case handling turned off. */
    unsigned long no_dup_count;
    /** Number of fragments which have dynamic case handling turned off. */
    unsigned long no_dynamic_handling_count;
    /** Number of cases handled via dynamic generation. */
    unsigned long gen_count;
    /**
     * Execution count of bails to the default case due to encountered unhandled
     * cases.
     */
    unsigned long bail_count;
} drbbdup_stats_t;

/**
 * Priorities of drmgr instrumentation passes used by drbbdup. Users
 * can perform app2app manipulations prior to duplication
 * by ordering such changes before #DRMGR_PRIORITY_APP2APP_DRBBDUP.
 */
enum {
    /** Priority of drbbdup's app2app stage. */
    DRMGR_PRIORITY_APP2APP_DRBBDUP = 6500,
    /** Priority of drbbdup's insert stage. */
    DRMGR_PRIORITY_INSERT_DRBBDUP = -6500,
    /** Priority of drbbdup's restore state event. */
    DRMGR_PRIORITY_RESTORE_DRBBDUP = -99900,
};

/**
 * Name of drbbdup app2app priority.
 */
#define DRMGR_PRIORITY_APP2APP_NAME_DRBBDUP "drbbdup_app2app"

/**
 * Name of drbbdup insert priority.
 */
#define DRMGR_PRIORITY_INSERT_NAME_DRBBDUP "drbbdup_insert"

/**
 * Name of drbbdup restore state priority.
 */
#define DRMGR_PRIORITY_RESTORE_NAME_DRBBDUP "drbbdup_restore"

DR_EXPORT
/**
 * Initialises the drbbdup extension. Must be called before use of any other routines.
 *
 * It cannot be called multiple times as duplication management is specific to a single
 * use-case where only one default case encoding is associated with each basic block.
 *
 * The \p ops_in parameter is a set of options which dictate how drbbdup manages
 * basic block copies.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_init(drbbdup_options_t *ops_in);

DR_EXPORT
/**
 * Cleans up the drbbdup extension.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_exit(void);

DR_EXPORT
/**
 * Registers a non-default case encoding \p encoding. The function should only be called
 * by a #drbbdup_set_up_bb_dups_t call-back function which provides \p drbbdup_ctx.
 *
 * The same encoding cannot be registered more than once.
 *
 * @return whether successful or an error code on failure.
 */
drbbdup_status_t
drbbdup_register_case_encoding(void *drbbdup_ctx, uintptr_t encoding);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the first instruction of
 * the currently considered basic block copy. The result is returned in \p is_start.
 *
 * Must be called via a #drbbdup_instrument_instr_t call-back function.
 *
 * @return whether successful or an error code on failure.
 * @note when using drbbdup, do not rely on drmgr_is_first_instr().
 */
drbbdup_status_t
drbbdup_is_first_instr(void *drcontext, instr_t *instr, OUT bool *is_start);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the first non label instruction of
 * the currently considered basic block copy. The result is returned in \p is_nonlabel.
 *
 * Must be called via a #drbbdup_instrument_instr_t call-back function.
 *
 * @return whether successful or an error code on failure.
 * @note when using drbbdup, do not rely on drmgr_is_first_nonlabel_instr().
 */
drbbdup_status_t
drbbdup_is_first_nonlabel_instr(void *drcontext, instr_t *instr, bool *is_nonlabel);

DR_EXPORT
/**
 * Indicates whether the instruction \p instr is the last instruction of the currently
 * considered basic block copy. The result is returned in \p is_last.
 *
 * Must be called via a #drbbdup_instrument_instr_t call-back function.
 *
 * @return whether successful or an error code on failure.
 * @note when using drbbdup, do not rely on drmgr_is_last_instr().
 */
drbbdup_status_t
drbbdup_is_last_instr(void *drcontext, instr_t *instr, OUT bool *is_last);

DR_EXPORT
/**
 * Returns various statistics regarding drbbdup. In particular, the routine
 * populates \p stats with current values.
 *
 * Note that the invocation of this routine is only successful if statistics gathering
 * is set via #drbbdup_options_t when initializing drbbdup.
 *
 * Internally, a lock is used while gathering the statistics.
 */
drbbdup_status_t
drbbdup_get_stats(OUT drbbdup_stats_t *stats);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRBBDUP_H_ */
