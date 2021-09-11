/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.   All rights reserved.
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

/* DynamoRIO Multi-Instrumentation Manager Extension: a mediator for
 * combining and coordinating multiple instrumentation passes
 */

#ifndef _DRMGR_H_
#define _DRMGR_H_ 1

/**
 * @file drmgr.h
 * @brief Header for DynamoRIO Multi-Instrumentation Manager Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drmgr Multi-Instrumentation Manager
 */
/**@{*/ /* begin doxygen group */

#ifndef STATIC_DRMGR_ONLY
/* A client can use a static function like drmgr_decode_sysnum_from_wrapper
 * directly, in which case, we allow the client to use following functions.
 */

/* drmgr replaces the bb event */
#    define dr_register_bb_event DO_NOT_USE_bb_event_USE_drmgr_bb_events_instead
#    define dr_unregister_bb_event DO_NOT_USE_bb_event_USE_drmgr_bb_events_instead

/* drmgr replaces the tls field routines */
#    define dr_get_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_set_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_insert_read_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
#    define dr_insert_write_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead

/* drmgr replaces these events in order to provide ordering control */
#    define dr_register_thread_init_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
#    define dr_unregister_thread_init_event \
        DO_NOT_USE_thread_event_USE_drmgr_events_instead
#    define dr_register_thread_exit_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
#    define dr_unregister_thread_exit_event \
        DO_NOT_USE_thread_event_USE_drmgr_events_instead
#    define dr_register_pre_syscall_event DO_NOT_USE_pre_syscall_USE_drmgr_events_instead
#    define dr_unregister_pre_syscall_event \
        DO_NOT_USE_pre_syscall_USE_drmgr_events_instead
#    define dr_register_post_syscall_event \
        DO_NOT_USE_post_syscall_USE_drmgr_events_instead
#    define dr_unregister_post_syscall_event \
        DO_NOT_USE_post_syscall_USE_drmgr_events_instead
#    define dr_register_module_load_event DO_NOT_USE_module_load_USE_drmgr_events_instead
#    define dr_unregister_module_load_event \
        DO_NOT_USE_module_load_USE_drmgr_events_instead
#    define dr_register_module_unload_event DO_NOT_USE_module_unload_USE_drmgr_instead
#    define dr_unregister_module_unload_event DO_NOT_USE_module_unload_USE_drmgr_instead
#    define dr_register_low_on_memory_event DO_NOT_USE_low_on_memory_USE_drmgr_instead
#    define dr_unregister_low_on_memory_event DO_NOT_USE_low_on_memory_USE_drmgr_instead
#    define dr_register_kernel_xfer_event DO_NOT_USE_kernel_xfer_event_USE_drmgr_instead
#    define dr_unregister_kernel_xfer_event DO_NOT_USE_kernel_xfer_event_USE_drmgr_instead
#    define dr_register_signal_event DO_NOT_USE_signal_event_USE_drmgr_instead
#    define dr_unregister_signal_event DO_NOT_USE_signal_event_USE_drmgr_instead
#    define dr_register_exception_event DO_NOT_USE_exception_event_USE_drmgr_instead
#    define dr_unregister_exception_event DO_NOT_USE_exception_event_USE_drmgr_instead
#    define dr_register_restore_state_event DO_NOT_USE_restore_state_USE_drmgr_instead
#    define dr_unregister_restore_state_event DO_NOT_USE_restore_state_USE_drmgr_instead
#    define dr_register_restore_state_ex_event \
        DO_NOT_USE_restore_state_ex_USE_drmgr_instead
#    define dr_unregister_restore_state_ex_event \
        DO_NOT_USE_restore_state_ex_USE_drmgr_instead

#endif /* !STATIC_DRMGR_ONLY */

/***************************************************************************
 * TYPES
 */

/**
 * Callback function for the first, fourth, and fifth stages: app2app, instru2instru, and
 * meta_instru transformations on the whole instruction list.
 *
 * See #dr_emit_flags_t for an explanation of the return value.  If
 * any instrumentation pass requests #DR_EMIT_STORE_TRANSLATIONS, they
 * will be stored.
 */
typedef dr_emit_flags_t (*drmgr_xform_cb_t)(void *drcontext, void *tag, instrlist_t *bb,
                                            bool for_trace, bool translating);

/**
 * Callback function for the second stage: application analysis.
 *
 * The \p user_data parameter can be used to pass data from this stage
 * to the third stage.
 *
 * See #dr_emit_flags_t for an explanation of the return value.  If
 * any instrumentation pass requests #DR_EMIT_STORE_TRANSLATIONS, they
 * will be stored.
 */
typedef dr_emit_flags_t (*drmgr_analysis_cb_t)(void *drcontext, void *tag,
                                               instrlist_t *bb, bool for_trace,
                                               bool translating, OUT void **user_data);

/**
 * Callback function for the first stage when using a user data parameter:
 * app2app transformations on instruction list.
 */
typedef drmgr_analysis_cb_t drmgr_app2app_ex_cb_t;

/**
 * Callback function for the second, fourth, and fifth stages when using a user
 * data parameter for all five: analysis, instru2instru, and meta_instru
 * transformations on the whole instruction list.
 *
 * See #dr_emit_flags_t for an explanation of the return value.  If
 * any instrumentation pass requests #DR_EMIT_STORE_TRANSLATIONS, they
 * will be stored.
 */
typedef dr_emit_flags_t (*drmgr_ilist_ex_cb_t)(void *drcontext, void *tag,
                                               instrlist_t *bb, bool for_trace,
                                               bool translating, void *user_data);

/**
 * Callback function for the third stage: instrumentation insertion.
 *
 * The \p user_data parameter contains data passed from the second
 * stage to this stage.
 *
 * See #dr_emit_flags_t for an explanation of the return value.  If
 * any instrumentation pass requests #DR_EMIT_STORE_TRANSLATIONS, they
 * will be stored.
 */
typedef dr_emit_flags_t (*drmgr_insertion_cb_t)(void *drcontext, void *tag,
                                                instrlist_t *bb, instr_t *inst,
                                                bool for_trace, bool translating,
                                                void *user_data);

/**
 * Callback function for opcode based instrumentation. In particular, this callback
 * is triggered only for specific instruction opcodes. This is done during the
 * third stage, i.e., instrumentation insertion.
 *
 * See #dr_emit_flags_t for an explanation of the return value.  If
 * any instrumentation pass requests #DR_EMIT_STORE_TRANSLATIONS, they
 * will be stored.
 */
typedef dr_emit_flags_t (*drmgr_opcode_insertion_cb_t)(void *drcontext, void *tag,
                                                       instrlist_t *bb, instr_t *inst,
                                                       bool for_trace, bool translating,
                                                       void *user_data);

/** Specifies the ordering of callbacks for \p drmgr's events */
typedef struct _drmgr_priority_t {
    /** The size of the drmgr_priority_t struct */
    size_t struct_size;
    /**
     * A name for the callback being registered, to be used by other
     * components when specifying their relative order.
     * This field is mandatory.
     */
    const char *name;
    /**
     * The name of another callback that the callback being registered
     * should precede.  This field is optional and can be NULL.
     */
    const char *before;
    /**
     * The name of another callback that the callback being registered
     * should follow.  This field is optional and can be NULL.
     */
    const char *after;
    /**
     * The numeric priority of the callback.  This is the primary field for
     * determining callback order, with lower numbers placed earlier in the
     * callback invocation order.  The \p before and \p after requests must have
     * numeric priorities that are smaller than and greater than, respectively,
     * the requesting callback.  Numeric ties are invoked in unspecified order.
     */
    int priority;
} drmgr_priority_t;

/** Specifies the callbacks when registering all \p drmgr's bb instrumentation events */
typedef struct _drmgr_instru_events_t {
    /** The size of the drmgr_instru_events_t struct. */
    size_t struct_size;
    /** Callback for the app2app event. */
    drmgr_app2app_ex_cb_t app2app_func;
    /** Callback for the analysis event. */
    drmgr_ilist_ex_cb_t analysis_func;
    /** Callback for the insertion event. */
    drmgr_insertion_cb_t insertion_func;
    /** Callback for the instru2instru event. */
    drmgr_ilist_ex_cb_t instru2instru_func;
    /** Callback for the meta_instru event. */
    drmgr_ilist_ex_cb_t meta_instru_func;
} drmgr_instru_events_t;

/** Labels the current bb building phase */
typedef enum {
    DRMGR_PHASE_NONE,          /**< Not currently in a bb building event. */
    DRMGR_PHASE_APP2APP,       /**< Currently in the app2app phase. */
    DRMGR_PHASE_ANALYSIS,      /**< Currently in the analysis phase. */
    DRMGR_PHASE_INSERTION,     /**< Currently in the instrumentation insertion phase. */
    DRMGR_PHASE_INSTRU2INSTRU, /**< Currently in the instru2instru phase. */
    DRMGR_PHASE_META_INSTRU,   /**< Currently in the meta-instrumentation phase. */
} drmgr_bb_phase_t;

/***************************************************************************
 * INIT
 */

DR_EXPORT
/**
 * Initializes the drmgr extension.  Must be called prior to any of the
 * other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drmgr_exit().
 *
 * \return whether successful.
 */
bool
drmgr_init(void);

DR_EXPORT
/**
 * Cleans up the drmgr extension.
 */
void
drmgr_exit(void);

/***************************************************************************
 * BB EVENTS
 */

DR_EXPORT
/**
 * Registers a callback function for the first instrumentation stage:
 * application-to-application ("app2app") transformations on each
 * basic block.  drmgr will call \p func as the first of five
 * instrumentation stages for each dynamic application basic block.
 * Examples of app2app transformations include replacing one function
 * with another or replacing one instruction with another throughout
 * an application.
 *
 * The app2app passes are allowed to modify and insert non-meta (i.e.,
 * application) instructions and are intended for application code
 * transformations.  These passes should avoid adding meta
 * instructions other than label instructions.
 *
 * All instrumentation must follow the guidelines for
 * #dr_register_bb_event() with the exception that multiple
 * application control transfer instructions are supported so long as
 * all but one have intra-block \p instr_t targets.  This is to
 * support internal control flow that may be necessary for some
 * application-to-application transformations.  These control transfer
 * instructions should have a translation set so that later passes
 * know which application address they correspond to.  \p drmgr will
 * mark all of the extra non-meta control transfers as meta, and clear
 * their translation fields, right before passing to DynamoRIO, in
 * order to satisfy DynamoRIO's constraints.  This allows all of the
 * instrumentation passes to see these instructions as application
 * instructions, which is how they should be treated.
 *
 * \return false if the given priority request cannot be satisfied
 * (e.g., \p priority->before is already ordered after \p
 * priority->after) or the given name is already taken.
 *
 * @param[in]  func        The callback to be called.
 * @param[in]  priority    Specifies the relative ordering of the callback.
 *                         Can be NULL, in which case a default priority is used.
 */
bool
drmgr_register_bb_app2app_event(drmgr_xform_cb_t func, drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters a callback function for the first instrumentation stage.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_app2app_event(drmgr_xform_cb_t func);

DR_EXPORT
/**
 * Registers callback functions for the second and third
 * instrumentation stages: application analysis and instrumentation
 * insertion.  drmgr will call \p func as the second of five
 * instrumentation stages for each dynamic application basic block.
 *
 * The first stage performed any changes to the original application
 * code, and later stages are not allowed to change application code.
 * Application analysis passes in the second stage are not allowed to
 * add to or change the instruction list other than adding label
 * instructions, and are intended for analysis of application code
 * either for immediate use or for use by the third stage.  Label
 * instructions can be used to store data for use in subsequent stages
 * with custom tags inserted as notes via drmgr_reserve_note_range()
 * and custom data stored via instr_get_label_data_area().
 *
 * The third instrumentation stage is instrumentation insertion.
 * Unlike the other stages, this one passes only one instruction to
 * the callback, allowing each registered component to act on one
 * instruction before moving to the next instruction.  Instrumentation
 * insertion passes are allowed to insert meta instructions only
 * immediately prior to the passed-in instruction: not before any
 * prior non-meta instruction nor after any subsequent non-meta
 * instruction.  They are not allowed to insert new non-meta
 * instructions or change existing non-meta instructions.  Because
 * other components may have already acted on the instruction list, be
 * sure to ignore already existing meta instructions.
 *
 * The \p analysis_func and \p insertion_func share the same priority.
 * Their user_data parameter can be used to pass data from the
 * analysis stage to the insertion stage.
 *
 * All instrumentation must follow the guidelines for
 * #dr_register_bb_event().
 *
 * On ARM, for the instrumentation insertion event, drmgr does not set
 * the predicate for all meta instructions inserted by each callback
 * to match the predicate of the corresponding application instruction.
 * It is the client's responsibility to set proper predicates for
 * all meta instructions.  At the end of all instrumentation stages,
 * drmgr automatically adds enough IT instructions to create legal IT
 * blocks in Thumb mode.
 *
 * \return false if the given priority request cannot be satisfied
 * (e.g., \p priority->before is already ordered after \p
 * priority->after) or the given name is already taken.
 *
 * @param[in]  analysis_func   The analysis callback to be called for the second stage.
 *                             Can be NULL if insertion_func is non-NULL, in which
 *                             case the user_data passed to insertion_func is NULL
 *                             and drmgr_unregister_bb_insertion_event() must be
 *                             used to unregister.
 * @param[in]  insertion_func  The insertion callback to be called for the third stage.
 *                             Can be NULL if analysis_func is non-NULL.
 * @param[in]  priority        Specifies the relative ordering of both callbacks.
 *                             Can be NULL, in which case a default priority is used.
 *
 * \note It is possible for meta instructions to be present and passed
 * to the analysis and/or insertion stages, if they were added in the
 * app2app or analysis stages.  While this is discouraged, it is
 * sometimes unavoidable, such as for drwrap_replace_native().  We
 * recommend that all instrumentation stages check for meta
 * instructions (and ignore them, typically).
 */
bool
drmgr_register_bb_instrumentation_event(drmgr_analysis_cb_t analysis_func,
                                        drmgr_insertion_cb_t insertion_func,
                                        drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters \p func and its corresponding insertion
 * callback from the second and third instrumentation stages.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_instrumentation_event(drmgr_analysis_cb_t func);

DR_EXPORT
/**
 * Unregisters \p func from the second and third instrumentation stages.
 * If an analysis callback was passed to drmgr_register_bb_instrumentation_event(),
 * use drmgr_unregister_bb_instrumentation_event() instead.
 *
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_insertion_event(drmgr_insertion_cb_t func);

DR_EXPORT
/**
 * Registers a callback function for the fourth instrumentation stage:
 * instrumentation-to-instrumentation transformations on each
 * basic block.  drmgr will call \p func as the fourth of five
 * instrumentation stages for each dynamic application basic block.
 * Instrumentation-to-instrumentation passes are allowed to insert meta
 * instructions but not non-meta instructions, and are intended for
 * optimization of prior instrumentation passes.
 *
 * All instrumentation must follow the guidelines for
 * #dr_register_bb_event().
 *
 * \return false if the given priority request cannot be satisfied
 * (e.g., \p priority->before is already ordered after \p
 * priority->after) or the given name is already taken.
 *
 * @param[in]  func        The callback to be called.
 * @param[in]  priority    Specifies the relative ordering of the callback.
 *                         Can be NULL, in which case a default priority is used.
 */
bool
drmgr_register_bb_instru2instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters a callback function for the fourth instrumentation stage.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_instru2instru_event(drmgr_xform_cb_t func);

DR_EXPORT
/**
 * Registers callbacks for the first four instrumentation passes at once, with a \p
 * user_data parameter passed among them all, enabling data sharing for all
 * four of them.  See the documentation for drmgr_register_bb_app2app_event(),
 * drmgr_register_bb_instrumentation_event(), and drmgr_register_bb_instru2instru_event()
 * for further details of each pass. The aforementioned routines are identical to this
 * with the exception of the extra \p user_data parameter, which is an OUT parameter to
 * the \p app2app_func and passed in to the three subsequent callbacks. The \p priority
 * param can be NULL, in which case a default priority is used.
 */
bool
drmgr_register_bb_instrumentation_ex_event(drmgr_app2app_ex_cb_t app2app_func,
                                           drmgr_ilist_ex_cb_t analysis_func,
                                           drmgr_insertion_cb_t insertion_func,
                                           drmgr_ilist_ex_cb_t instru2instru_func,
                                           drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters the four given callbacks that
 * were registered via drmgr_register_bb_instrumentation_ex_event().
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_instrumentation_ex_event(drmgr_app2app_ex_cb_t app2app_func,
                                             drmgr_ilist_ex_cb_t analysis_func,
                                             drmgr_insertion_cb_t insertion_func,
                                             drmgr_ilist_ex_cb_t instru2instru_func);

DR_EXPORT
/**
 * Registers callback functions for the third
 * instrumentation stage: instrumentation
 * insertion.  drmgr will call \p func for each instruction with the
 * specific opcode \p opcode.
 *
 * More than one callback function can be mapped to the same opcode. Their
 * execution sequence is determined by their priority \p priority (if set). Ordering
 * based on priority is also taken into account with respect to insert per instr events.
 *
 * Since this callback is triggered during instrumentation insertion,
 * same usage rules apply. The callback is allowed to insert meta
 * instructions only immediately prior to the passed-in instruction.
 * New non-meta instructions cannot be inserted.
 *
 * \return false upon failure.
 *
 * @param[in]  func  The opcode insertion callback to be called for the third
 * stage for a specific opcode instruction. Cannot be NULL.
 * @param[in]  opcode          The opcode to associate with the insertion callback.
 * @param[in]  priority        Specifies the relative ordering of both callbacks.
 *                             Can be NULL, in which case a default priority is used.
 * @param[in]  user_data       User data made available when triggering the callback
 * \p func. Can be NULL.
 *
 * \note It is possible that this callback will be triggered for meta instructions.
 * Therefore, we recommend that the callback check for meta instructions
 * (and ignore them, typically).
 */
bool
drmgr_register_opcode_instrumentation_event(drmgr_opcode_insertion_cb_t func, int opcode,
                                            drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregisters the opcode-specific callback that
 * was registered via drmgr_register_opcode_instrumentation_event().
 *
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered for the passed opcode \p opcode).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_opcode_instrumentation_event(drmgr_opcode_insertion_cb_t func,
                                              int opcode);

DR_EXPORT
/**
 * Registers a callback function for the fifth instrumentation stage:
 * meta-instrumentation analysis and transformations on each
 * basic block.  drmgr will call \p func as the fifth of five
 * instrumentation stages for each dynamic application basic block.
 * Meta-instrumentation passes are allowed to insert both meta and
 * non-meta instructions, and are primarily intended for debugging prior
 * instrumentation passes.
 *
 * All instrumentation must follow the guidelines for
 * #dr_register_bb_event().
 *
 * \return false if the given priority request cannot be satisfied
 * (e.g., \p priority->before is already ordered after \p
 * priority->after) or the given name is already taken.
 *
 * @param[in]  func        The callback to be called.
 * @param[in]  priority    Specifies the relative ordering of the callback.
 *                         Can be NULL, in which case a default priority is used.
 */
bool
drmgr_register_bb_meta_instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters a callback function for the fifth instrumentation stage.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_meta_instru_event(drmgr_xform_cb_t func);

DR_EXPORT
/**
 * Registers callbacks for all five instrumentation passes at once, with a \p
 * user_data parameter passed among them all, enabling data sharing for all
 * five of them.  See the documentation for drmgr_register_bb_app2app_event(),
 * drmgr_register_bb_instrumentation_event(), drmgr_register_bb_instru2instru_event(), and
 * drmgr_register_bb_meta_instru_event() for further details of each pass. The
 * aforementioned routines are identical to this with the exception of the extra \p
 * user_data parameter, which is an OUT parameter to the \p app2app_func and passed in to
 * the four subsequent callbacks. The \p priority param can be NULL, in which case a
 * default priority is used.
 */
bool
drmgr_register_bb_instrumentation_all_events(drmgr_instru_events_t *events,
                                             drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregisters the callbacks that were registered via
 * drmgr_register_bb_instrumentation_all_events().
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * The recommendations for dr_unregister_bb_event() about when it
 * is safe to unregister apply here as well.
 */
bool
drmgr_unregister_bb_instrumentation_all_events(drmgr_instru_events_t *events);

DR_EXPORT
/**
 * Returns which bb phase is the current one, if any.
 *
 * \note May be called without calling drmgr_init() to test whether drmgr
 * is in use.
 */
drmgr_bb_phase_t
drmgr_current_bb_phase(void *drcontext);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Returns whether \p instr is the
 * first instruction (of any type) in the instruction list (as of immediately after
 * the analysis phase).
 */
bool
drmgr_is_first_instr(void *drcontext, instr_t *instr);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Returns whether \p instr is the
 * first non-label instruction in the instruction list (as of immediately after
 * the analysis phase).
 */
bool
drmgr_is_first_nonlabel_instr(void *drcontext, instr_t *instr);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.  Returns whether \p instr is the
 * last instruction (of any type) in the instruction list (as of immediately after
 * the analysis phase).
 */
bool
drmgr_is_last_instr(void *drcontext, instr_t *instr);

/***************************************************************************
 * TLS
 */

DR_EXPORT
/**
 * Reserves a thread-local storage (tls) slot for every thread.
 * Returns the index of the slot, which should be passed to
 * drmgr_get_tls_field() and drmgr_set_tls_field().  Returns -1 if
 * there are no more slots available.  Each slot is initialized to
 * NULL for each thread and should be properly initialized with
 * drmgr_set_tls_field() in the thread initialization event (see
 * dr_register_thread_init_event()).
 */
int
drmgr_register_tls_field(void);

DR_EXPORT
/**
 * Frees a previously reserved thread-local storage (tls) slot index.
 * Returns false if the slot was not actually reserved.
 */
bool
drmgr_unregister_tls_field(int idx);

DR_EXPORT
/**
 * Returns the user-controlled thread-local-storage field for the
 * given index, which was returned by drmgr_register_tls_field().  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use drmgr_insert_read_tls_field().
 */
void *
drmgr_get_tls_field(void *drcontext, int idx);

DR_EXPORT
/**
 * Sets the user-controlled thread-local-storage field for the
 * given index, which was returned by drmgr_register_tls_field().  To
 * generate an instruction sequence that writes the drcontext field
 * inline in the code cache, use drmgr_insert_write_tls_field().
 * \return whether successful.
 */
bool
drmgr_set_tls_field(void *drcontext, int idx, void *value);

DR_EXPORT
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to read
 * into the general-purpose full-size register \p reg from the
 * user-controlled drcontext field for this thread and index.  Reads
 * from the same field as drmgr_get_tls_field().
 * \return whether successful.
 */
bool
drmgr_insert_read_tls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                            reg_id_t reg);

DR_EXPORT
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to
 * write the general-purpose full-size register \p reg to the
 * user-controlled drcontext field for this thread and index.  Writes
 * to the same field as drmgr_set_tls_field().  The register \p scratch
 * will be overwritten.
 * \return whether successful.
 */
bool
drmgr_insert_write_tls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                             reg_id_t reg, reg_id_t scratch);

/***************************************************************************
 * CLS
 */

DR_EXPORT
/**
 * Reserves a callback-local storage (cls) slot.  Thread-local storage
 * (tls) is callback-shared.  Callbacks interrupt thread execution to
 * execute arbitrary amounts of code in a new context before returning
 * to the interrupted context.  Thread-local storage fields that
 * persist across application execution can be overwritten during
 * callback execution, resulting in incorrect values when returning to
 * the original context.  Callback-local storage, rather than
 * thread-local storage, should be used for any fields that store
 * information specific to the application's execution.
 *
 * Returns the index of the slot, which should be passed to
 * drmgr_get_cls_field() and drmgr_set_cls_field().  Returns -1 if
 * there are no more slots available.
 *
 * Callbacks are frequent, but normally the stack of callback contexts
 * is only a few entries deep.  It is most efficient to re-use cls
 * data from prior callbacks, only allocating new memory when entering
 * a new context stack depth.  The \p cb_init_func parameter is
 * invoked on each new callback context, with \p new_depth set to true
 * only when entering a new callback context stack depth.  When \p
 * new_depth is false, drmgr_get_cls_field() will return the value set
 * at that depth the last time it was reached, and the client would
 * normally not need to allocate memory but would only need to
 * initialize it.  When \p new_depth is true, drmgr_get_cls_field()
 * will return NULL, and the user should use drmgr_set_cls_field() to
 * initialize the slot itself as well as whatever it points to.
 *
 * Similarly, normal usage should ignore \p cb_exit_func unless it is
 * called with \p thread_exit set to true, in which case any memory
 * in the cls slot should be de-allocated.
 *
 * Callbacks are Windows-specific.  The cls interfaces are not marked
 * for Windows-only, however, to facilitate cross-platform code.  We
 * recommend that cross-plaform code be written using cls fields on
 * both platforms; the fields on Linux will never be stacked and will
 * function as tls fields.  Technically the same context interruption
 * can occur with a Linux signal, but Linux signals typically execute
 * small amounts of code and avoid making stateful changes;
 * furthermore, there is no guaranteed end point to a signal.  The
 * drmgr_push_cls() and drmgr_pop_cls() interface can be used to
 * provide a stack of contexts on Linux, or to provide a stack of
 * contexts for any other purpose such as layered wrapped functions.
 * These push and pop functions are automatically called on Windows
 * callback entry and exit, with the push called in DR's kernel xfer
 * event prior to any client callback for that event, and pop called in
 * the same event but after any client callback.
 */
int
drmgr_register_cls_field(void (*cb_init_func)(void *drcontext, bool new_depth),
                         void (*cb_exit_func)(void *drcontext, bool thread_exit));

DR_EXPORT
/**
 * Frees a previously reserved callback-local storage (cls) slot index and
 * unregisters its event callbacks.
 * Returns false if the slot was not actually reserved.
 */
bool
drmgr_unregister_cls_field(void (*cb_init_func)(void *drcontext, bool new_depth),
                           void (*cb_exit_func)(void *drcontext, bool thread_exit),
                           int idx);

DR_EXPORT
/**
 * Returns the user-controlled callback-local-storage field for the
 * given index, which was returned by drmgr_register_cls_field().  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use drmgr_insert_read_cls_field().
 */
void *
drmgr_get_cls_field(void *drcontext, int idx);

DR_EXPORT
/**
 * Sets the user-controlled callback-local-storage field for the
 * given index, which was returned by drmgr_register_cls_field().  To
 * generate an instruction sequence that writes the drcontext field
 * inline in the code cache, use drmgr_insert_write_cls_field().
 * \return whether successful.
 */
bool
drmgr_set_cls_field(void *drcontext, int idx, void *value);

DR_EXPORT
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to read
 * into the general-purpose full-size register \p reg from the
 * user-controlled drcontext field for the current (at
 * execution time) callback and index.  Reads from the same field as
 * drmgr_get_cls_field().
 * \return whether successful.
 */
bool
drmgr_insert_read_cls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                            reg_id_t reg);

DR_EXPORT
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to
 * write the general-purpose full-size register \p reg to the
 * user-controlled drcontext field for the current (at execution time)
 * callback and index.  Writes to the same field as
 * drmgr_set_cls_field().  The register \p scratch will be
 * overwritten.
 * \return whether successful.
 */
bool
drmgr_insert_write_cls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                             reg_id_t reg, reg_id_t scratch);

DR_EXPORT
/**
 * Pushes a new callback context onto the callback-local storage (cls)
 * context stack for the given thread.  This function is automatically
 * called on entry to a new Windows callback.  Users can invoke it to
 * provide context stacks for their own uses, including Linux signals
 * or layered wrapped functions.  Invoking this function will trigger
 * the \p cb_init_func passed to drmgr_register_cls_field().
 * \return whether successful.
 */
bool
drmgr_push_cls(void *drcontext);

DR_EXPORT
/**
 * Pops a callback context from the callback-local storage (cls)
 * context stack for the given thread.  This function is automatically
 * called on exit from a Windows callback.  Users can invoke it to
 * provide context stacks for their own uses, including Linux signals
 * or layered wrapped functions.  Invoking this function will trigger
 * the \p cb_exit_func passed to drmgr_register_cls_field().
 *
 * Returns false if the context stack has only one entry.
 */
bool
drmgr_pop_cls(void *drcontext);

DR_EXPORT
/**
 * Returns the user-controlled callback-local-storage field for the
 * given index, which was returned by drmgr_register_cls_field(), for
 * the parent context (i.e., the context most recently pushed either
 * by a Windows callback entry or a call to drmgr_push_cls()).  If
 * there is no parent context, returns NULL.
 */
void *
drmgr_get_parent_cls_field(void *drcontext, int idx);

/***************************************************************************
 * INSTRUCTION NOTE FIELD
 */

enum {
    DRMGR_NOTE_NONE = 0, /* == DRX_NOTE_NONE */
};

DR_EXPORT
/**
 * Reserves \p size values in the namespace for use in the \p note
 * field of instructions.  The reserved range starts at the return
 * value and is contiguous.  Returns DRMGR_NOTE_NONE on failure.
 * Un-reserving is not supported.
 */
ptr_uint_t
drmgr_reserve_note_range(size_t size);

/***************************************************************************
 * EMULATION
 */

/**
 * Flags describing different types of emulation markers.
 */
typedef enum {
    /**
     * Indicates that the entire rest of the basic block is one emulation sequence.
     * There is no end marker, so drmgr_is_emulation_end() will never return true.
     * No support is provided for traces: clients must examine the constituent
     * blocks instead to find emulation information.
     * This is used for emulation sequences that include a block-terminating
     * conditional branch, indirect branch, or system call or interrupt, as DR
     * does not allow a label to appear after such instructions.  These
     * sequences typically want to isolate their emulation to include the entire
     * block in any case.
     */
    DR_EMULATE_REST_OF_BLOCK = 0x0001,
    /**
     * When used with drmgr_in_emulation_region(), indicates that the current
     * instruction is the first instruction of the emulation region.  This allows
     * a client to act on the original instruction just once, despite multiple
     * emulation instructions.
     */
    DR_EMULATE_IS_FIRST_INSTR = 0x0002,
    /**
     * Indicates that only the instruction fetch is being emulated differently.
     * The operation of the instruction remains the same.
     * Observational instrumentation should examine the original instruction
     * (in #emulated_instr_t.instr) for instruction fetch purposes, but should
     * examine the emulation sequence for data accesses (e.g., loads and stores for
     * a memory address tracing tool).  If this flag is not set, instrumentation
     * should instrument the original instruction in every way and ignore the
     * emulation sequence.
     *
     * This flag is used for instruction refactorings that simplify instrumentation:
     * e.g., drutil_expand_rep_string() and drx_expand_scatter_gather().  It is not
     * used for true emulation of an instruction, for example, for replacing an
     * instruction that is not supported by the current hardware with
     * an alternative sequence of instructions.
     */
    DR_EMULATE_INSTR_ONLY = 0x0004,
} dr_emulate_options_t;

/**
 * Holds data about an emulated instruction, typically populated by an emulation
 * client and read by an observational client.
 *
 * \note The emulated \p instr is part of the label represented by
 * \p emulated_instr_t and as such it will be freed when the label created by
 * drmgr_insert_emulation_start() is freed.
 */
typedef struct _emulated_instr_t {
    size_t size;    /**< Size of this struct, used for API compatibility checks. */
    app_pc pc;      /**< The PC address of the emulated instruction. */
    instr_t *instr; /**< The emulated instruction. See __Note__ above. */
    dr_emulate_options_t flags; /**< Flags further describing the emulation. */
} emulated_instr_t;

/**
 * Inserts a label into \p ilist prior to \p where to indicate the start of a
 * sequence of instructions emulating an instruction \p instr. The label has data
 * attached which describes the instruction being emulated.
 *
 * A label will also appear at the end of the sequence, added using
 * drmgr_insert_emulation_end() (unless #DR_EMULATE_REST_OF_BLOCK is set). These start
 * and stop labels can be detected by an observational client using
 * drmgr_is_emulation_start() and
 * drmgr_is_emulation_end() allowing the client to distinguish between native
 * app instructions and instructions used for emulation.
 *
 * When calling this function, the \p size field of \p instr should be set using
 * sizeof(). This allows the API to check for compatibility.
 *
 * Information about the instruction being emulated can be read from the label using
 * drmgr_get_emulated_instr_data().
 *
 * If label callbacks are used, please note that the callback will not be cloned
 * and its use is currently not consistent (xref i#3962).
 *
 * \return false if the caller's \p emulated_instr_t is not compatible, true otherwise.
 *
 */
DR_EXPORT
bool
drmgr_insert_emulation_start(void *drcontext, instrlist_t *ilist, instr_t *where,
                             emulated_instr_t *instr);

/**
 * Inserts a label into \p ilist prior to \p where to indicate the end of a
 * sequence of instructions emulating an instruction, preceded by a label created
 * with drmgr_insert_emulation_start().  Alternatively, #DR_EMULATE_REST_OF_BLOCK
 * can be used on the start label to include the entire block, with no need for
 * an end label.
 */
DR_EXPORT
void
drmgr_insert_emulation_end(void *drcontext, instrlist_t *ilist, instr_t *where);

/**
 * Checks the instruction \p instr to see if it is an emulation start label
 * created by drmgr_insert_emulation_start(). Typically used in an
 * instrumentation client running with an emulation client.
 *
 * \return true if \p instr is an emulation start label, false otherwise.
 */
DR_EXPORT
bool
drmgr_is_emulation_start(instr_t *instr);

/**
 * Checks the instruction \p instr to see if it is an emulation end label
 * created by drmgr_insert_emulation_end(). Typically used in an
 * instrumentation client running with an emulation client.
 *
 * \return true if \p instr is an emulation end label, false otherwise.
 */
DR_EXPORT
bool
drmgr_is_emulation_end(instr_t *instr);

/**
 * Loads \p emulated with the emulated instruction data from \p instr set by
 * drmgr_insert_emulation_start().
 *
 * When calling this function, the \p size field of \p emulated should be set using
 * sizeof(). This allows the API to check for compatibility.
 *
 * @param[in]  instr       The label instruction which specifies start of emulation.
 * @param[out] emulated    The emulated instruction data.
 *
 * \return false if the caller's \p emulated_instr_t is not compatible, true otherwise.
 */
DR_EXPORT
bool
drmgr_get_emulated_instr_data(instr_t *instr, OUT emulated_instr_t *emulated);

/**
 * Must be called during drmgr's insertion phase.  Returns whether the current
 * instruction in the phase is inside an emulation region.  If it returns true,
 * \p emulation_info is written with a pointer to information about the emulation.
 * The pointed-at information's lifetime is the full range of the emulation region.
 *
 * This is a convenience routine that records the state for the
 * drmgr_is_emulation_start() and drmgr_is_emulation_end() labels and the
 * #DR_EMULATE_REST_OF_BLOCK flag to prevent the client having to store state.
 *
 * While this is exported, there is still complexity in analyzing the
 * different flags, and we recommend that clients do not use this
 * function directly but instead use the two routines
 * drmgr_orig_app_instr_for_fetch() and
 * drmgr_orig_app_instr_for_operands() (which internally use this function):
 *
 * dr_emit_flags_t
 * event_insertion(void *drcontext, void *tag, instrlist_t *bb, instr_t *insert_instr,
 *                 bool for_trace, bool translating, void *user_data) {
 *     instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
 *     if (instr_fetch != NULL)
 *         record_instr_fetch(instr_fetch);
 *     instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
 *     if (instr_operands_valid != NULL)
 *         record_data_addresses(instr_operands);
 *     return DR_EMIT_DEFAULT;
 * }
 *
 * \return false if the caller's \p emulated_instr_t is not compatible, true otherwise.
 */
DR_EXPORT
bool
drmgr_in_emulation_region(void *drcontext, OUT const emulated_instr_t **emulation_info);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.
 *
 * Returns the instruction to consider as the current application
 * instruction for observational clients with respect to which
 * instruction is executed, taking into account emulation.  This may be
 * different from the current instruction list instruction passed to
 * the insertion event.
 *
 * Returns NULL if observational clients should skip the current instruction
 * list instruction.
 */
instr_t *
drmgr_orig_app_instr_for_fetch(void *drcontext);

DR_EXPORT
/**
 * Must be called during drmgr's insertion phase.
 *
 * Returns the instruction to consider as the current application
 * instruction for observational clients with respect to the operands
 * of the instruction, taking into account emulation.  This may be
 * different from the current instruction list instruction passed to
 * the insertion event.
 *
 * Returns NULL if observational clients should skip the current instruction
 * list instruction.
 */
instr_t *
drmgr_orig_app_instr_for_operands(void *drcontext);

/***************************************************************************
 * UTILITIES
 */

#ifdef WINDOWS
DR_EXPORT
/**
 * Given a system call wrapper routine \p entry of the Native API variety,
 * decodes the routine and returns the system call number.
 * Returns -1 on error.
 */
int
drmgr_decode_sysnum_from_wrapper(app_pc entry);
#endif

/***************************************************************************
 * DR EVENT REPLACEMENTS WITH NO SEMANTIC CHANGES
 */

DR_EXPORT
/**
 * Registers a callback function for the thread initialization event.
 * drmgr calls \p func whenever the application creates a new thread.
 * \return whether successful.
 */
bool
drmgr_register_thread_init_event(void (*func)(void *drcontext));

DR_EXPORT
/**
 * Registers a callback function for the thread initialization event,
 * ordered by \p priority. drmgr calls \p func whenever the application
 * creates a new thread. \return whether successful.
 */
bool
drmgr_register_thread_init_event_ex(void (*func)(void *drcontext),
                                    drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the thread initialization event,
 * ordered by \p priority. Allows for the passing of user data \p user_data
 * which is available upon the execution of the callback. drmgr calls \p func
 * whenever the application creates a new thread. \return whether successful.
 *
 * See also drmgr_register_thread_init_event_ex().
 */
bool
drmgr_register_thread_init_event_user_data(void (*func)(void *drcontext, void *user_data),
                                           drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregister a callback function for the thread initialization event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_thread_init_event(void (*func)(void *drcontext));

DR_EXPORT
/**
 * Unregister a callback function for the thread initialization event which
 * also has user data passed to the callback. \return true if unregistration
 * is successful and false if it is not (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_thread_init_event().
 */
bool
drmgr_unregister_thread_init_event_user_data(void (*func)(void *drcontext,
                                                          void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the thread exit event.  drmgr calls \p func
 * whenever DR would, when an application thread exits.  All the constraints of
 * dr_register_thread_exit_event() apply.  \return whether successful.
 */
bool
drmgr_register_thread_exit_event(void (*func)(void *drcontext));

DR_EXPORT
/**
 * Registers a callback function for the thread exit event, ordered by \p priority.
 * drmgr calls \p func whenever DR would, when an application thread exits. All
 * the constraints of dr_register_thread_exit_event() apply. \return whether
 * successful.
 */
bool
drmgr_register_thread_exit_event_ex(void (*func)(void *drcontext),
                                    drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the thread exit event,
 * ordered by \p priority. Allows for the passing of user data \p user_data
 * which is available upon the execution of the callback. drmgr calls \p func
 * when an application thread exits. \return whether successful.
 *
 * See also drmgr_register_thread_exit_event_ex().
 */
bool
drmgr_register_thread_exit_event_user_data(void (*func)(void *drcontext, void *user_data),
                                           drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregister a callback function for the thread exit event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_thread_exit_event(void (*func)(void *drcontext));

DR_EXPORT
/**
 * Unregister a callback function for the thread exit event which
 * also has user data passed to the callback. \return true if unregistration
 * is successful and false if it is not (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_thread_exit_event().
 */
bool
drmgr_unregister_thread_exit_event_user_data(void (*func)(void *drcontext,
                                                          void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the pre-syscall event, which
 * behaves just like DR's pre-syscall event dr_register_pre_syscall_event().
 * In particular, a filter event is still needed to ensure that a pre- or post-syscall
 * event is actually called: use dr_register_filter_syscall_event().
 * \return whether successful.
 */
bool
drmgr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_EXPORT
/**
 * Registers a callback function for the pre-syscall event, which
 * behaves just like DR's pre-syscall event
 * dr_register_pre_syscall_event(), except that it is ordered according
 * to \p priority.  A default priority of 0 is used for events registered
 * via drmgr_register_pre_syscall_event().
 * A filter event is still needed to ensure that a pre- or post-syscall
 * event is actually called: use dr_register_filter_syscall_event().
 * \return whether successful.
 */
bool
drmgr_register_pre_syscall_event_ex(bool (*func)(void *drcontext, int sysnum),
                                    drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the pre-syscall event,
 * ordered by \p priority. Allows for the passing of user data \p user_data
 * which is available upon the execution of the callback.
 * \return whether successful.
 *
 * See also drmgr_register_pre_syscall_event_ex().
 */
bool
drmgr_register_pre_syscall_event_user_data(bool (*func)(void *drcontext, int sysnum,
                                                        void *user_data),
                                           drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregister a callback function for the pre-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_pre_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_EXPORT
/**
 * Unregister a callback function, which takes user data, for the pre-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_pre_syscall_event().
 */
bool
drmgr_unregister_pre_syscall_event_user_data(bool (*func)(void *drcontext, int sysnum,
                                                          void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the post-syscall event, which
 * behaves just like DR's post-syscall event
 * dr_register_post_syscall_event().
 * \return whether successful.
 */
bool
drmgr_register_post_syscall_event(void (*func)(void *drcontext, int sysnum));

DR_EXPORT
/**
 * Registers a callback function for the post-syscall event, which
 * behaves just like DR's post-syscall event
 * dr_register_post_syscall_event(), except that it is ordered according
 * to \p priority.  A default priority of 0 is used for events registered
 * via drmgr_register_post_syscall_event().
 * \return whether successful.
 */
bool
drmgr_register_post_syscall_event_ex(void (*func)(void *drcontext, int sysnum),
                                     drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the post-syscall event,
 * ordered by \p priority. Allows for the passing of user data \p user_data
 * which is available upon the execution of the callback.
 * \return whether successful.
 *
 * See also drmgr_register_post_syscall_event_ex().
 */
bool
drmgr_register_post_syscall_event_user_data(void (*func)(void *drcontext, int sysnum,
                                                         void *user_data),
                                            drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregister a callback function for the post-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_post_syscall_event(void (*func)(void *drcontext, int sysnum));

DR_EXPORT
/**
 * Unregister a callback function, which takes user data, for the post-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_post_syscall_event().
 */
bool
drmgr_unregister_post_syscall_event_user_data(void (*func)(void *drcontext, int sysnum,
                                                           void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the module load event, which
 * behaves just like DR's module load event
 * dr_register_module_load_event().
 * \return whether successful.
 */
bool
drmgr_register_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                              bool loaded));

DR_EXPORT
/**
 * Registers a callback function for the module load event, which
 * behaves just like DR's module load event
 * dr_register_module_load_event(), except that it is ordered according
 * to \p priority.  A default priority of 0 is used for events registered
 * via drmgr_register_module_load_event().
 * \return whether successful.
 */
bool
drmgr_register_module_load_event_ex(void (*func)(void *drcontext,
                                                 const module_data_t *info, bool loaded),
                                    drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the module load event, which
 * behaves just like DR's module load event dr_register_module_load_event().
 * Allows for the passing of user input \p user_data, which is available upon
 * the execution of the callback. \return whether successful.
 *
 * See also drmgr_register_module_load_event_ex().
 */
bool
drmgr_register_module_load_event_user_data(void (*func)(void *drcontext,
                                                        const module_data_t *info,
                                                        bool loaded, void *user_data),
                                           drmgr_priority_t *priority, void *user_data);

DR_EXPORT
/**
 * Unregister a callback function for the module load event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_module_load_event(void (*func)(void *drcontext,
                                                const module_data_t *info, bool loaded));

DR_EXPORT
/**
 * Unregister a callback function, which takes user data as a parameter
 * for the module load event.  \return true if unregistration is successful
 * and false if it is not (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_module_load_event().
 */
bool
drmgr_unregister_module_load_event_user_data(void (*func)(void *drcontext,
                                                          const module_data_t *info,
                                                          bool loaded, void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the module unload event, which
 * behaves just like DR's module unload event
 * dr_register_module_unload_event().
 * \return whether successful.
 */
bool
drmgr_register_module_unload_event(void (*func)(void *drcontext,
                                                const module_data_t *info));

DR_EXPORT
/**
 * Registers a callback function for the module unload event, which
 * behaves just like DR's module unload event
 * dr_register_module_unload_event(), except that it is ordered according
 * to \p priority.  A default priority of 0 is used for events registered
 * via drmgr_register_module_unload_event().
 * \return whether successful.
 */
bool
drmgr_register_module_unload_event_ex(void (*func)(void *drcontext,
                                                   const module_data_t *info),
                                      drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the module unload event, which
 * behaves just like DR's module unload event dr_register_module_unload_event().
 * Allows for the passing of user data, \p user_data, which is available upon the
 * execution of the callback. \return whether successful.
 *
 * See also drmgr_register_module_unload_event_ex().
 */
bool
drmgr_register_module_unload_event_user_data(void (*func)(void *drcontext,
                                                          const module_data_t *info,
                                                          void *user_data),
                                             drmgr_priority_t *priority, void *user_data);
DR_EXPORT
/**
 * Unregister a callback function for the module unload event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_module_unload_event(void (*func)(void *drcontext,
                                                  const module_data_t *info));

DR_EXPORT
/**
 * Unregister a callback function, that takes user data as a parameter,
 * for the module unload event. \return true if unregistration is
 * successful and false if it is not (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_module_unload_event().
 */
bool
drmgr_unregister_module_unload_event_user_data(void (*func)(void *drcontext,
                                                            const module_data_t *info,
                                                            void *user_data));

DR_EXPORT
/**
 * Registers a callback function for the kernel transfer event, which
 * behaves just like DR's kernel transfer event dr_register_kernel_xfer_event().
 * \return whether successful.
 */
bool
drmgr_register_kernel_xfer_event(void (*func)(void *drcontext,
                                              const dr_kernel_xfer_info_t *info));

DR_EXPORT
/**
 * Registers a callback function for the kernel transfer event, which behaves just
 * like DR's kernel transfer event dr_register_kernel_xfer_event(), except that it is
 * ordered according to \p priority.  A default priority of 0 is used for events
 * registered via drmgr_register_module_unload_event().
 * \return whether successful.
 */
bool
drmgr_register_kernel_xfer_event_ex(void (*func)(void *drcontext,
                                                 const dr_kernel_xfer_info_t *info),
                                    drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregister a callback function for the kernel transfer event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_kernel_xfer_event(void (*func)(void *drcontext,
                                                const dr_kernel_xfer_info_t *info));

#ifdef UNIX
DR_EXPORT
/**
 * Registers a callback function for the signal event, which
 * behaves just like DR's signal event dr_register_signal_event().
 * \return whether successful.
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_register_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                       dr_siginfo_t *siginfo));
/* clang-format on */

DR_EXPORT
/**
 * Registers a callback function for the signal event, which
 * behaves just like DR's signal event dr_register_signal_event(),
 * except that it is ordered according to \p priority.  A default
 * priority of 0 is used for events registered via
 * drmgr_register_signal_event().  Just like for DR, the first
 * callback to return other than DR_SIGNAL_DELIVER will
 * short-circuit event delivery to later callbacks.
 * \return whether successful.
 */
bool
drmgr_register_signal_event_ex(dr_signal_action_t (*func)(void *drcontext,
                                                          dr_siginfo_t *siginfo),
                               drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function for the signal event, which
 * behaves just like DR's module load event dr_register_signal_event().
 * Allows for the passing of user input \p user_data, which is available upon
 * the execution of the callback. \return whether successful.
 *
 * See also drmgr_register_signal_event_ex().
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_register_signal_event_user_data(dr_signal_action_t (*func)(void *drcontext,
                                                                 dr_siginfo_t *siginfo,
                                                                 void *user_data),
                                      drmgr_priority_t *priority, void *user_data);
/* clang-format on */

DR_EXPORT
/**
 * Unregister a callback function for the signal event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_unregister_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                         dr_siginfo_t *siginfo));
/* clang-format on */

DR_EXPORT
/**
 * Unregister a callback function for the signal event which
 * also has user data passed to the callback. \return true if unregistration
 * is successful and false if it is not (e.g., \p func was not registered).
 *
 * See also drmgr_unregister_signal_event().
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_unregister_signal_event_user_data(dr_signal_action_t (*func)(void *drcontext,
                                                                   dr_siginfo_t *siginfo,
                                                                   void *user_data));
/* clang-format on */
#endif /* UNIX */

#ifdef WINDOWS
DR_EXPORT
/**
 * Registers a callback function for the exception event, which
 * behaves just like DR's exception event dr_register_exception_event().
 * \return whether successful.
 */
bool
drmgr_register_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt));

DR_EXPORT
/**
 * Registers a callback function for the exception event, which
 * behaves just like DR's exception event dr_register_exception_event(),
 * except that it is ordered according to \p priority.  A default
 * priority of 0 is used for events registered via
 * drmgr_register_exception_event().  Just like for DR, the first
 * callback to return false will short-circuit event delivery to later
 * callbacks.
 * \return whether successful.
 */
bool
drmgr_register_exception_event_ex(bool (*func)(void *drcontext, dr_exception_t *excpt),
                                  drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregister a callback function for the exception event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt));
#endif /* WINDOWS */

DR_EXPORT
/**
 * Registers a callback function for the restore state event, which
 * behaves just like DR's restore state event dr_register_restore_state_event().
 * \return whether successful.
 */
bool
drmgr_register_restore_state_event(void (*func)(void *drcontext, void *tag,
                                                dr_mcontext_t *mcontext,
                                                bool restore_memory,
                                                bool app_code_consistent));

DR_EXPORT
/**
 * Registers a callback function for the restore state extended event, which
 * behaves just like DR's restore state event
 * dr_register_restore_state_ex_event().
 * \return whether successful.
 */
bool
drmgr_register_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                   dr_restore_state_info_t *info));

DR_EXPORT
/**
 * Registers a callback function for the restore state extended event,
 * which behaves just like DR's restore state event
 * dr_register_restore_state_ex_event(), except that it is ordered
 * according to \p priority among both extended and regular callbacks.
 * A default priority of 0 is used for events registered via
 * drmgr_register_restore_state_event() or
 * drmgr_register_restore_state_ex_event().  Just like for DR, the
 * first callback to return false will short-circuit event delivery to
 * later callbacks.
 * \return whether successful.
 */
bool
drmgr_register_restore_state_ex_event_ex(bool (*func)(void *drcontext,
                                                      bool restore_memory,
                                                      dr_restore_state_info_t *info),
                                         drmgr_priority_t *priority);

DR_EXPORT
/**
 * Unregister a callback function for the restore state event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_restore_state_event(void (*func)(void *drcontext, void *tag,
                                                  dr_mcontext_t *mcontext,
                                                  bool restore_memory,
                                                  bool app_code_consistent));

DR_EXPORT
/**
 * Unregister a callback function for the restore state extended event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
drmgr_unregister_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                     dr_restore_state_info_t *info));

DR_EXPORT
/**
 * Registers a callback function \p func for the low-on-memory event. The callback
 * provides a means for the client to free any non-critical data found on the heap, which
 * could avoid a potential out-of-memory crash (particularly on 32-bit). \return whether
 * successful.
 */
bool
drmgr_register_low_on_memory_event(void (*func)());

DR_EXPORT
/**
 * Registers a callback function \p func for the low-on-memory event just like
 * drmgr_register_low_on_memory_event(), but the callback is prioritised according
 * to \p priority.
 * \return whether successful.
 */
bool
drmgr_register_low_on_memory_event_ex(void (*func)(), drmgr_priority_t *priority);

DR_EXPORT
/**
 * Registers a callback function \p func for the low-on-memory event just like
 * drmgr_register_low_on_memory_event(), but allows the passing of user data
 * \p user_data. The callback is prioritised according to \p priority.
 * \return whether successful.
 */
bool
drmgr_register_low_on_memory_event_user_data(void (*func)(void *user_data),
                                             drmgr_priority_t *priority, void *user_data);

DR_EXPORT
bool
/**
 * Unregister a callback function for the low-on-memory event.
 * \return true if the unregistration of \p func is successful.
 */
drmgr_unregister_low_on_memory_event(void (*func)());

DR_EXPORT
/**
 * Unregister a callback function that accepts user-data for the low-on-memory event.
 * \return true if the unregistration of \p func is successful.
 */
bool
drmgr_unregister_low_on_memory_event_user_data(void (*func)(void *user_data));

DR_EXPORT
/**
 * Disables auto predication globally for this basic block.
 * \return whether successful.
 * \note Only to be used in the drmgr insertion event.
 */
bool
drmgr_disable_auto_predication(void *drcontext, instrlist_t *ilist);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRMGR_H_ */
