/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

#ifndef _DR_EVENTS_H_
#define _DR_EVENTS_H_ 1

#include "dr_config.h"

/**
 * @file dr_events.h
 * @brief Event callback registration routines.
 */

/**************************************************
 * ROUTINES TO REGISTER EVENT CALLBACKS
 */

DR_API
/**
 * Registers a callback function for the process exit event.  DR calls \p
 * func when the process exits.  By default, the process exit event will be
 * executed with only a single live thread.  dr_set_process_exit_behavior()
 * can provide superior exit performance for clients that have flexible
 * exit event requirements.
 *
 * On Linux, SYS_execve does NOT result in an exit event, but it WILL
 * result in the client library being reloaded and its dr_client_main()
 * routine being called.
 */
void
dr_register_exit_event(void (*func)(void));

DR_API
/**
 * Unregister a callback function for the process exit event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_exit_event(void (*func)(void));

DR_API
/**
 * Registers a function which is called after all other threads have been taken over
 * during a process attach event, whether externally triggered or internally triggered
 * (via dr_app_start() or related functions).  If this process instance was not
 * initiated by an attach or takeover methodology where multiple application threads may
 * exist at the time of takeover (such as a process newly created on Linux), this
 * registration function returns false.
 *
 * The attach methodology operates in a staggered fashion, with each thread being taken
 * over and executed under DR control in turn.  If the application has many threads,
 * threads taken over early in this process may execute substantial amounts of
 * instrumented code before the threads taken over last start executing instrumented
 * code.  The purpose of this event is to provide a single control point where all
 * threads are known to be under DR control and running instrumented code.
 */
bool
dr_register_post_attach_event(void (*func)(void));

DR_API
/**
 * Unregister a callback function for the post-attach event (see
 * dr_register_post_attach_event()).  \return true if unregistration is successful and
 * false if it is not (e.g., \p func was not registered).
 */
bool
dr_unregister_post_attach_event(void (*func)(void));

DR_API
/**
 * Registers a function which is called at the start of a full detach of DR from the
 * current process, whether externally triggered or internally triggered (via
 * dr_app_stop_and_cleanup() or related functions), as well as at the start of DR
 * sending all threads native but not cleaning up its own state (through dr_app_stop()).
 *
 * The detach methodology operates in a staggered fashion, with each thread being
 * returned to native control in turn.  If the application has many threads, threads
 * detached late in this process may execute substantial amounts of instrumented code
 * before the full detach process is complete, while threads detached early are running
 * natively.  The purpose of this event is to provide a single final control point where
 * all threads are known to be under DR control and running instrumented code.  The exit
 * event (see dr_register_exit_event()) is not called until after all other threads have
 * been detached.
 */
void
dr_register_pre_detach_event(void (*func)(void));

DR_API
/**
 * Unregister a callback function for the post-attach event (see
 * dr_register_pre_detach_event()).  \return true if unregistration is successful and
 * false if it is not (e.g., \p func was not registered).
 */
bool
dr_unregister_pre_detach_event(void (*func)(void));

/**
 * Flags controlling the behavior of basic blocks and traces when emitted
 * into the code cache.  These flags are bitmasks that can be combined by
 * or-ing together.  For multiple clients, the flags returned by each
 * client are or-ed together.
 */
typedef enum {
    /** Emit as normal. */
    DR_EMIT_DEFAULT = 0,
    /**
     * Store translation information at emit time rather than calling
     * the basic block or trace event later to recreate the
     * information.  Note that even if a standalone basic block has
     * stored translations, if when it is added to a trace it does not
     * request storage (and the trace callback also does not request
     * storage) then the basic block callback may still be called to
     * translate for the trace.
     *
     * \sa #dr_register_bb_event()
     */
    DR_EMIT_STORE_TRANSLATIONS = 0x01,
    /**
     * Only valid when applied to a basic block.  Indicates that the
     * block is eligible for persisting to a persistent code cache
     * file on disk.  By default, no blocks are eligible, as tools
     * must take care in order to properly support persistence.
     * Note that the block is not guaranteed to be persisted if
     * it contains complex features that prevent DR from
     * easily persisting it.
     */
    DR_EMIT_PERSISTABLE = 0x02,
    /**
     * Only valid when applied to a basic block.  Indicates that the
     * block must terminate a trace.  Normally this should be set when
     * an abnormal exit is used from the block that is incompatible with
     * trace building's attempt to inline the continuation from the block
     * to its successor.  Note that invoking dr_redirect_execution() from a
     * clean call called from a block aborts trace building and thus this
     * flag need not be set for that scenario.
     */
    DR_EMIT_MUST_END_TRACE = 0x04,
    /**
     * Requests that DR relinquish control of the current thread and
     * let it run natively until the client indicates that DR should
     * take over again.  While native, on Windows, currently only the
     * thread init event (dr_register_thread_init_event()) will be
     * raised, and nothing on Linux: no events will occur in the
     * native thread.  On Windows, DR tries to monitor any actions a
     * native thread might take that affect correct execution from the
     * code cache, but running natively carries risks.  Consider this
     * feature experimental, particularly on Linux.
     */
    DR_EMIT_GO_NATIVE = 0x08,
} dr_emit_flags_t;

DR_API
/**
 * Registers a callback function for the basic block event.  DR calls
 * \p func before inserting a new basic block into the code cache.
 * When adding a basic block to a new trace, DR calls \p func again
 * with \p for_trace set to true, giving the client the opportunity to
 * keep its same instrumentation in the trace, or to change it.  The
 * original basic block's instrumentation is unchanged by whatever
 * action is taken in the \p for_trace call.
 *
 * DR constructs <em>dynamic basic blocks</em>, which are distinct
 * from a compiler's classic basic blocks.  DR does not know all entry
 * points ahead of time, and will end up duplicating the tail of a
 * basic block if a later entry point is discovered that targets the
 * middle of a block created earlier, or if a later entry point
 * targets straight-line code that falls through into code already
 * present in a block.
 *
 * DR may call \p func again if it needs to translate from code cache
 * addresses back to application addresses, which happens on faulting
 * instructions as well as in certain situations involving suspended
 * threads or forcibly relocated threads.  The \p translating
 * parameter distinguishes the two types of calls and is further
 * explained below.
 *
 * - \p drcontext is a pointer to the input program's machine context.
 * Clients should not inspect or modify the context; it is provided as
 * an opaque pointer (i.e., <tt>void *</tt>) to be passed to API
 * routines that require access to this internal data.
 * drcontext is specific to the current thread, but in normal
 * configurations the basic block being created is thread-shared: thus,
 * when allocating data structures with the same lifetime as the
 * basic block, usually global heap (#dr_global_alloc()) is a better
 * choice than heap tied to the thread that happened to first create
 * the basic block (#dr_thread_alloc()).  Thread-private heap is fine
 * for temporary structures such as instr_t and instrlist_t.
 *
 * - \p tag is a unique identifier for the basic block fragment.
 * Use dr_fragment_app_pc() to translate it to an application address.
 * - \p bb is a pointer to the list of instructions that comprise the
 * basic block.  Clients can examine, manipulate, or completely
 * replace the instructions in the list.
 *
 * - \p translating indicates whether this callback is for basic block
 * creation (false) or is for address translation (true).  This is
 * further explained below.
 *
 * The callback function should return a #dr_emit_flags_t flag.
 *
 * The user is free to inspect and modify the block before it
 * executes, but must adhere to the following restrictions:
 * - If there is more than one application branch, only the last can be
 * conditional.
 * - An application conditional branch must be the final
 * instruction in the block.
 * - An application direct call must be the final
 * instruction in the block unless it is inserted by DR for elision and the
 * subsequent instructions are the callee.
 * - There can only be one indirect branch (call, jump, or return) in
 * a basic block, and it must be the final instruction in the
 * block.
 * - There can only be one far branch (call, jump, or return) in
 * a basic block, and it must be the final instruction in the
 * block.
 * - The AArch64 instruction ISB must be the final instruction in the
 * block.
 * - The exit control-flow of a block ending in a system call or
 * int instruction cannot be changed, nor can instructions be inserted
 * after the system call or int instruction itself, unless
 * the system call or int instruction is removed entirely.
 * - The number of an interrupt cannot be changed.  (Note that the
 * parameter to a system call, normally kept in the eax register, can
 * be freely changed in a basic block: but not in a trace.)
 * - A system call or interrupt instruction can only be added
 * if it satisfies the above constraints: i.e., if it is the final
 * instruction in the block and the only system call or interrupt.
 * - Any AArch64 #OP_isb instruction must be the last instruction
 * in its block.
 * - All IT blocks must be legal.  For example, application instructions
 * inside an IT block cannot be removed or added to without also
 * updating the OP_it instruction itself.  Clients can use
 * the combination of dr_remove_it_instrs() and dr_insert_it_instrs()
 * to more easily manage IT blocks while maintaining the simplicity
 * of examining individual instructions in isolation.
 * - The block's application source code (as indicated by the
 * translation targets, set by #instr_set_translation()) must remain
 * within the original bounds of the block (the one exception to this
 * is that a jump can translate to its target).  Otherwise, DR's cache
 * consistency algorithms cannot guarantee to properly invalidate the
 * block if the source application code is modified.  To send control
 * to other application code regions, truncate the block and use a
 * direct jump to target the desired address, which will then
 * materialize in the subsequent block, rather than embedding the
 * desired instructions in this block.
 * - There is a limit on the size of a basic block in the code cache.
 * DR performs its own modifications, especially on memory writes for
 * cache consistency of self-modifying (or false sharing) code
 * regions.  If an assert fires in debug build indicating a limit was
 * reached, either truncate blocks or use the -max_bb_instrs runtime
 * option to ask DR to make them smaller.
 *
 * To support transparent fault handling, DR must translate a fault in the
 * code cache into a fault at the corresponding application address.  DR
 * must also be able to translate when a suspended thread is examined by
 * the application or by DR itself for internal synchronization purposes.
 * If the client is only adding observational instrumentation (i.e., meta
 * instructions: see #instr_set_meta()) (which should not fault) and
 * is not modifying, reordering, or removing application instructions,
 * these details can be ignored.  In that case the client should return
 * #DR_EMIT_DEFAULT and set up its basic block callback to be deterministic
 * and idempotent.  If the client is performing modifications, then in
 * order for DR to properly translate a code cache address the client must
 * use #instr_set_translation() in the basic block creation callback to set
 * the corresponding application address (the address that should be
 * presented to the application as the faulting address, or the address
 * that should be restarted after a suspend) for each modified instruction
 * and each added application instruction (see #instr_set_app()).
 *
 * There are two methods for using the translated addresses:
 *
 * -# Return #DR_EMIT_STORE_TRANSLATIONS from the basic block creation
 *    callback.  DR will then store the translation addresses and use
 *    the stored information on a fault.  The basic block callback for
 *    \p tag will not be called with \p translating set to true.  Note
 *    that unless #DR_EMIT_STORE_TRANSLATIONS is also returned for \p
 *    for_trace calls (or #DR_EMIT_STORE_TRANSLATIONS is returned in
 *    the trace callback), each constituent block comprising the trace
 *    will need to be re-created with both \p for_trace and \p
 *    translating set to true.  Storing translations uses additional
 *    memory that can be significant: up to 20% in some cases, as it
 *    prevents DR from using its simple data structures and forces it
 *    to fall back to its complex, corner-case design.  This is why DR
 *    does not store all translations by default.
 * -# Return #DR_EMIT_DEFAULT from the basic block creation callback.
 *    DR will then call the callback again during fault translation
 *    with \p translating set to true.  All modifications to \p bb
 *    that were performed on the creation callback must be repeated on
 *    the translating callback.  This option is only possible when
 *    basic block modifications are deterministic and idempotent, but
 *    it saves memory.  Naturally, global state changes triggered by
 *    block creation should be wrapped in checks for \p translating
 *    being false.  Even in this case, #instr_set_translation() should
 *    be called for application instructions even when \p translating is
 *    false, as DR may decide to store the translations at creation
 *    time for reasons of its own.
 *
 * Furthermore, if the client's modifications change any part of the
 * machine state besides the program counter, the client should use
 * #dr_register_restore_state_event() or
 * #dr_register_restore_state_ex_event() to restore the registers and
 * application memory to their original application values.
 *
 * For meta instructions that do not reference application memory
 * (i.e., they should not fault), leave the translation field as NULL.
 * A NULL value instructs DR to use the subsequent application
 * instruction's translation as the application address, and to fail
 * when translating the full state.  Since the full state will only be
 * needed when relocating a thread (as stated, there will not be a
 * fault here), failure indicates that this is not a valid relocation
 * point, and DR's thread synchronization scheme will use another
 * spot.  If the translation field is set to a non-NULL value, the
 * client should be willing to also restore the rest of the machine
 * state at that point (restore spilled registers, etc.) via
 * #dr_register_restore_state_event() or
 * #dr_register_restore_state_ex_event().  This is necessary for meta
 * instructions that reference application memory.  DR takes care of
 * such potentially-faulting instructions added by its own API
 * routines (#dr_insert_clean_call() arguments that reference
 * application data, #dr_insert_mbr_instrumentation()'s read of
 * application indirect branch data, etc.)
 *
 * \note In order to present a more straightforward code stream to clients,
 * this release of DR disables several internal optimizations.  As a result,
 * some applications may see a performance degradation.  Applications making
 * heavy use of system calls are the most likely to be affected.
 * Future releases may allow clients some control over performance versus
 * visibility.  The \ref op_speed "-opt_speed" option can regain some
 * of this performance at the cost of more complex basic blocks that
 * cross control transfers.
 *
 * \note If multiple clients are present, the instruction list for a
 * basic block passed to earlier-registered clients will contain the
 * instrumentation and modifications put in place by later-registered
 * clients.
 *
 * \note Basic blocks can be deleted due to hitting capacity limits or
 * cache consistency events (when the source application code of a
 * basic block is modified).  In that case, the client will see a new
 * basic block callback if the block is then executed again after
 * deletion.  The deletion event (#dr_register_delete_event()) will be
 * raised at deletion time.
 *
 * \note If the -thread_private runtime option is specified, clients
 * should expect to see duplicate tags for separate threads, albeit
 * with different dcrcontext values.  Additionally, DR employs a
 * cache-sizing algorithm for thread private operation that
 * proactively deletes fragments.  Even with thread-shared caches
 * enabled, however, certain situations cause DR to emit
 * thread-private basic blocks (e.g., self-modifying code).  In this
 * case, clients should be prepared to see duplicate tags without an
 * intermediate deletion.
 *
 * \note A client can change the control flow of the application by
 * changing the control transfer instruction at end of the basic block.
 * If a basic block is ended with a non-control transfer instruction,
 * an application jump instruction can be inserted.
 * If a basic block is ended with a conditional branch,
 * \p instrlist_set_fall_through_target can be used to change the
 * fall-through target.
 * If a basic block is ended with a call instruction,
 * \p instrlist_set_return_target can be used to change the return
 * target of the call.
 */
void dr_register_bb_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                  instrlist_t *bb, bool for_trace,
                                                  bool translating));

DR_API
/**
 * Unregister a callback function for the basic block event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * \note We do not recommend unregistering for the basic block event
 * unless it aways returned #DR_EMIT_STORE_TRANSLATIONS (including
 * when \p for_trace is true, or if the client has a trace creation
 * callback that returns #DR_EMIT_STORE_TRANSLATIONS).  Unregistering
 * can prevent proper state translation on a later fault or other
 * translation event for this basic block or for a trace that includes
 * this basic block.  Instead of unregistering, turn the event
 * callback into a nop.
 */
bool dr_unregister_bb_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                    instrlist_t *bb, bool for_trace,
                                                    bool translating));

DR_API
/**
 * Registers a callback function for the trace event.  DR calls \p func
 * before inserting a new trace into the code cache.  DR may call \p func
 * again if it needs to translate from code cache addresses back to
 * application addresses, which happens on faulting instructions as well as
 * in certain situations involving suspended threads or forcibly relocated
 * threads.  The \p translating parameter distinguishes the two types of
 * calls and behaves identically to the same parameter in the basic
 * block callback: see #dr_register_bb_event() for further details.
 *
 * Traces are not built if the -disable_traces runtime option
 * is specified.
 *
 * - \p drcontext is a pointer to the input program's machine context.
 * Clients should not inspect or modify the context; it is provided as
 * an opaque pointer (i.e., <tt>void *</tt>) to be passed to API
 * routines that require access to this internal data.
 * - \p tag is a unique identifier for the trace fragment.
 * - \p trace is a pointer to the list of instructions that comprise the
 * trace.
 * - \p translating indicates whether this callback is for trace creation
 * (false) or is for fault address recreation (true).  This is further
 * explained below.
 *
 * The callback function should return a #dr_emit_flags_t flag.
 *
 * The user is free to inspect and modify the non-control-flow
 * instructions in the trace before it
 * executes, with certain restrictions
 * that include those for basic blocks (see dr_register_bb_event()).
 * Additional restrictions unique to traces also apply:
 * - The sequence of blocks composing the trace cannot be changed once
 *   the trace is created.  Instead, modify the component blocks by
 *   changing the block continuation addresses in the basic block callbacks
 *   (called with \p for_trace set to true) as the trace is being built.
 * - The (application) control flow instruction (if any) terminating each
 *   component block cannot be changed.
 * - Application control flow instructions cannot be added.
 * - The parameter to a system call, normally kept in the eax register,
 *   cannot be changed.
 * - A system call or interrupt instruction cannot be added.
 * - If both a floating-point state save instruction (fnstenv, fnsave,
 *   fxsave, xsave, or xsaveopt) and a prior regular floating-point
 *   instruction are present, the regular instruction cannot be
 *   removed.
 *
 * If hitting a size limit due to extensive instrumentation, reduce
 * the -max_trace_bbs option to start with a smaller trace.
 *
 * The basic block restrictions on modifying application source code
 * apply to traces as well.  If the user wishes to change which basic
 * blocks comprise the trace, either the
 * #dr_register_end_trace_event() should be used or the \p for_trace
 * basic block callbacks should modify their continuation addresses
 * via direct jumps.
 *
 * All of the comments for #dr_register_bb_event() regarding
 * transparent fault handling and state translation apply to the trace
 * callback as well.  Please read those comments carefully.
 *
 * \note As each basic block is added to a new trace, the basic block
 * callback (see #dr_register_bb_event()) is called with its \p
 * for_trace parameter set to true.  In order to preserve basic block
 * instrumentation inside of traces, a client need only act
 * identically with respect to the \p for_trace parameter; it can
 * ignore the trace event if its goal is to place instrumentation
 * on all code.
 *
 * \note Certain control flow modifications applied to a basic block
 * can prevent it from becoming part of a trace: e.g., adding
 * additional application control transfers.
 *
 * \note If multiple clients are present, the instruction list for a
 * trace passed to earlier-registered clients will contain the
 * instrumentation and modifications put in place by later-registered
 * clients; similarly for each constituent basic block.
 *
 * \note Traces can be deleted due to hitting capacity limits or cache
 * consistency events (when the source application code of a trace is
 * modified).  In that case, the client will see a new trace callback
 * if a new trace containing that code is created again after
 * deletion.  The deletion event (#dr_register_delete_event()) will be
 * raised at deletion time.
 */
void dr_register_trace_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                     instrlist_t *trace,
                                                     bool translating));

DR_API
/**
 * Unregister a callback function for the trace event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * \note We do not recommend unregistering for the trace event unless it
 * always returned #DR_EMIT_STORE_TRANSLATIONS, as doing so can prevent
 * proper state translation on a later fault or other translation event.
 * Instead of unregistering, turn the event callback into a nop.
 */
bool dr_unregister_trace_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                       instrlist_t *trace,
                                                       bool translating));

/**
 * DR will call the end trace event if it is registered prior to
 * adding each basic block to a trace being generated.  The return
 * value of the event callback should be from the
 * dr_custom_trace_action_t enum.
 *
 * \note DR treats CUSTOM_TRACE_CONTINUE as an advisement only.  Certain
 * fragments are not suitable to be included in a trace and if DR runs
 * into one it will end the trace regardless of what the client returns
 * through the event callback.
 */
typedef enum {
    CUSTOM_TRACE_DR_DECIDES,
    CUSTOM_TRACE_END_NOW,
    CUSTOM_TRACE_CONTINUE
} dr_custom_trace_action_t;

DR_API
/**
 * Registers a callback function for the end-trace event.  DR calls \p
 * func before extending a trace with a new basic block.  The \p func
 * should return one of the #dr_custom_trace_action_t enum values.
 */
void dr_register_end_trace_event(dr_custom_trace_action_t (*func)(void *drcontext,
                                                                  void *tag,
                                                                  void *next_tag));

DR_API
/**
 * Unregister a callback function for the end-trace event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool dr_unregister_end_trace_event(dr_custom_trace_action_t (*func)(void *drcontext,
                                                                    void *tag,
                                                                    void *next_tag));

/* For the new-bb-before-deletion-event problem (PR 495787, and
 * described in the comment below):
 * Note that we do not want a new "unreachable event" b/c clients need
 * to keep bb info around in case the semi-flushed bb hits a fault.
 * The main worry w/ the counter approach, in addition to ensuring
 * it handles duplicates due to thread-private, is that can we
 * guarantee that deletion events will be in order, or can a new
 * fragment be deleted prior to older fragments?  For most clients
 * it won't matter I suppose.
 */
DR_API
/**
 * Registers a callback function for the fragment deletion event.  DR
 * calls \p func whenever it removes a fragment from the code cache.
 * Due to DR's high-performance non-precise flushing, a fragment
 * can be made inaccessible but not actually freed for some time.
 * A new fragment can thus be created before the deletion event
 * for the old fragment is raised.  We recommended using a counter
 * to ignore subsequent deletion events when using per-fragment
 * data structures and duplicate fragments are seen.
 *
 * \note drcontext may be NULL when thread-shared fragments are being
 * deleted during process exit.  For this reason, thread-private
 * heap should not be used for data structures intended to be freed
 * at thread-shared fragment deletion.
 */
void
dr_register_delete_event(void (*func)(void *drcontext, void *tag));

DR_API
/**
 * Unregister a callback function for the fragment deletion event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_delete_event(void (*func)(void *drcontext, void *tag));

DR_API
/**
 * Registers a callback function for the machine state restoration event.
 * DR calls \p func whenever it needs to translate a code cache machine
 * context from the code cache to its corresponding original application
 * context.  DR needs to translate when instructions fault in the cache as
 * well as when a suspended thread is examined or relocated for internal
 * purposes.
 *
 * If a client is only adding instrumentation (meta-code: see
 * #instr_is_meta()) that does not reference application memory,
 * and is not reordering or removing application instructions, then it
 * need not register for this event.  If, however, a client is
 * modifying application code or is adding code that can fault, the
 * client must be capable of restoring the original context.
 *
 * When DR needs to translate a code cache context, DR recreates the
 * faulting instruction's containing fragment, storing translation
 * information along the way, by calling the basic block and/or trace event
 * callbacks with the \p translating parameter set to true.  DR uses the
 * recreated code to identify the application instruction (\p mcontext.pc)
 * corresponding to the faulting code cache instruction.  If the client
 * asked to store translation information by returning
 * #DR_EMIT_STORE_TRANSLATIONS from the basic block or trace event
 * callback, then this step of re-calling the event callback is skipped and
 * the stored value is used as the application address (\p mcontext.pc).
 *
 * DR then calls the fault state restoration event to allow the client
 * to restore the registers and application memory to their proper
 * values as they would have appeared if the original application code
 * had been executed up to the \p mcontext.pc instruction.  Memory
 * should only be restored if the \p restore_memory parameter is true;
 * if it is false, DR may only be querying for the address (\p
 * mcontext.pc) or register state and may not relocate this thread.
 *
 * DR will call this event for all translation attempts, even when the
 * register state already contains application values, to allow
 * clients to restore memory.
 *
 * The \p app_code_consistent parameter indicates whether the original
 * application code containing the instruction being translated is
 * guaranteed to still be in the same state it was when the code was
 * placed in the code cache.  This guarantee varies depending on the
 * type of cache consistency being used by DR.
 *
 * The client can update \p mcontext.pc in this callback.  The client
 * should not change \p mcontext.flags: it should remain DR_MC_ALL.
 *
 * \note The passed-in \p drcontext may correspond to a different thread
 * than the thread executing the callback.  Do not assume that the
 * executing thread is the target thread.
 */
void
dr_register_restore_state_event(void (*func)(void *drcontext, void *tag,
                                             dr_mcontext_t *mcontext, bool restore_memory,
                                             bool app_code_consistent));

DR_API
/**
 * Unregister a callback function for the machine state restoration event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_restore_state_event(void (*func)(void *drcontext, void *tag,
                                               dr_mcontext_t *mcontext,
                                               bool restore_memory,
                                               bool app_code_consistent));

/**
 * Data structure passed within dr_exception_t, dr_siginfo_t, and
 * dr_restore_state_info_t.
 * Contains information about the code fragment inside the code cache
 * at the exception/signal/translation interruption point.
 */
typedef struct _dr_fault_fragment_info_t {
    /**
     * The tag of the code fragment inside the code cache at the
     * exception/signal/translation interruption point. NULL for
     * interruption not in the code cache.
     */
    void *tag;
    /**
     * The start address of the code fragment inside the code cache at
     * the exception/signal/translation interruption point. NULL for interruption
     * not in the code cache (in which case generally only unusual cases of clients
     * changing memory require restoration).  Clients are cautioned when examining
     * code cache instructions to not rely on any details of code
     * inserted other than their own.
     */
    byte *cache_start_pc;
    /** Indicates whether the interrupted code fragment is a trace */
    bool is_trace;
    /**
     * Indicates whether the original application code containing the
     * code corresponding to the exception/signal/translation interruption point
     * is guaranteed to still be in the same state it was when the
     * code was placed in the code cache. This guarantee varies
     * depending on the type of cache consistency being used by DR.
     */
    bool app_code_consistent;
    /**
     * The recreated ilist for this fragment, which contains instrs added
     * by the basic block event(s) with \p translating set to true and also
     * DR's own mangling of some instrs. This includes client-added metadata
     * in the form of notes and label instrs too. This may be helpful in
     * restoring app state on a fault.
     * When the recreated ilist is not available, this is set to NULL. This
     * may happen when a client returns #DR_EMIT_STORE_TRANSLATIONS, or for
     * DR internal reasons when the app code may not be consistent: for pending
     * deletion or self-modifying fragments.  It will also be NULL for non-code-cache
     * cases where \p cache_start_pc is also NULL.
     */
    instrlist_t *ilist;
} dr_fault_fragment_info_t;

/**
 * Data structure passed to a restore_state_ex event handler (see
 * dr_register_restore_state_ex_event()).  Contains the machine
 * context at the translation point and other translation
 * information.
 */
typedef struct _dr_restore_state_info_t {
    /**
     * The application machine state at the translation point.
     * The client can update register values and the program counter
     * by changing this context.  The client should not change \p
     * mcontext.flags: it should remain DR_MC_ALL.
     */
    dr_mcontext_t *mcontext;
    /** Whether raw_mcontext is valid. */
    bool raw_mcontext_valid;
    /**
     * The raw pre-translated machine state at the translation
     * interruption point inside the code cache.  Clients are
     * cautioned when examining code cache instructions to not rely on
     * any details of code inserted other than their own.
     * Modifying this context will not affect the translation.
     */
    dr_mcontext_t *raw_mcontext;
    /**
     * Information about the code fragment inside the code cache
     * at the translation interruption point.
     */
    dr_fault_fragment_info_t fragment_info;
} dr_restore_state_info_t;

DR_API
/**
 * Registers a callback function for the machine state restoration
 * event with extended information.
 *
 * This event is identical to that for dr_register_restore_state_event()
 * with the following exceptions:
 *
 * - Additional information is provided in the
 *   dr_restore_state_info_t structure, including the pre-translation
 *   context (containing the address inside the code cache of the
 *   translation point) and the starting address of the containing
 *   fragment in the code cache.  Certain registers may not contain
 *   proper application values in \p info->raw_mcontext.  Clients are
 *   cautioned against relying on any details of code cache layout or
 *   register usage beyond instrumentation inserted by the client
 *   itself when examining \p info->raw_mcontext.
 *
 * - The callback function returns a boolean indicating the success of
 *   the translation.  When DR is translating not for a fault but for
 *   thread relocation, the \p restore_memory parameter will be false.
 *   Such translation can target a meta-instruction that can fault
 *   (i.e., it has a non-NULL translation field).  For that scenario, a client
 *   can choose not to translate.  Such instructions do not always
 *   require full translation for faults, and allowing translation
 *   failure removes the requirement that a client must translate at
 *   all such instructions.  Note, however, that returning false can
 *   cause performance degradation as DR must then resume the thread
 *   and attempt to re-suspend it at a safer spot.  Clients must
 *   return true for translation points in application code in order
 *   to avoid catastropic failure to suspend, and should thus identify
 *   whether translation points are inside their own instrumentation
 *   before returning false.  Translation for relocation will never
 *   occur in meta instructions, so clients only need to look for
 *   meta-may-fault instructions.  Clients should never return false
 *   when \p restore_memory is true.
 *
 * - If multiple callbacks are registered, the first one that returns
 *   false will short-circuit event delivery to later callbacks.
 */
void
dr_register_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                dr_restore_state_info_t *info));

DR_API
/**
 * Unregister a callback function for the machine state restoration
 * event with extended ifnormation.  \return true if unregistration is
 * successful and false if it is not (e.g., \p func was not
 * registered).
 */
bool
dr_unregister_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                  dr_restore_state_info_t *info));

DR_API
/**
 * Registers a callback function for the thread initialization event.
 * DR calls \p func whenever the application creates a new thread.
 */
void
dr_register_thread_init_event(void (*func)(void *drcontext));

DR_API
/**
 * Unregister a callback function for the thread initialization event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_thread_init_event(void (*func)(void *drcontext));

DR_API
/**
 * Registers a callback function for the thread exit event.  DR calls
 * \p func whenever an application thread exits.  The passed-in
 * drcontext should be used instead of calling
 * dr_get_current_drcontext(), as the thread exit event may be invoked
 * from other threads, and using dr_get_current_drcontext() can result
 * in failure to clean up the right resources, and at process exit
 * time it may return NULL.
 *
 * On Linux, SYS_execve may or may not result in a thread exit event.
 * If the client registers its thread exit callback as a pre-SYS_execve
 * callback as well, it must ensure that the callback acts as noop
 * if called for the second time.
 *
 * On Linux, the thread exit event may be invoked twice for the same thread
 * if that thread is alive during a process fork, but doesn't call the fork
 * itself.  The first time the event callback is executed from the fork child
 * immediately after the fork, the second time it is executed during the
 * regular thread exit.
 * Clients may want to avoid touching resources shared between processes,
 * like files, from the post-fork execution of the callback. The post-fork
 * version of the callback can be recognized by dr_get_process_id()
 * returning a different value than dr_get_process_id_from_drcontext().
 *
 * See dr_set_process_exit_behavior() for options controlling performance
 * and whether thread exit events are invoked at process exit time in
 * release build.
 */
void
dr_register_thread_exit_event(void (*func)(void *drcontext));

DR_API
/**
 * Unregister a callback function for the thread exit event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_thread_exit_event(void (*func)(void *drcontext));

/**
 * Flags controlling thread behavior at process exit time in release build.
 * See dr_set_process_exit_behavior() for further details.
 */
typedef enum {
    /**
     * Do not guarantee that the process exit event is executed
     * single-threaded.  This is equivalent to specifying the \p
     * -multi_thread_exit runtime option.  Setting this flag can improve
     * process exit performance, but usually only when the
     * #DR_EXIT_SKIP_THREAD_EXIT flag is also set, or when no thread exit
     * events are registered.
     */
    DR_EXIT_MULTI_THREAD = 0x01,
    /**
     * Do not invoke thread exit event callbacks at process exit time.
     * Thread exit event callbacks will still be invoked at other times.
     * This is equivalent to setting the \p -skip_thread_exit_at_exit
     * runtime option.  Setting this flag can improve process exit
     * performance, but usually only when the #DR_EXIT_MULTI_THREAD flag is
     * also set, or when no process exit event is registered.
     */
    DR_EXIT_SKIP_THREAD_EXIT = 0x02,
} dr_exit_flags_t;

DR_API
/**
 * Specifies how process exit should be handled with respect to thread exit
 * events and thread synchronization in release build.  In debug build, and
 * in release build by default, all threads are always synchronized at exit
 * time, resulting in a single-threaded process exit event, and all thread
 * exit event callbacks are always called.  This routine can provide more
 * performant exits in release build by avoiding the synchronization if the
 * client is willing to skip thread exit events at process exit and is
 * willing to execute its process exit event with multiple live threads.
 */
void
dr_set_process_exit_behavior(dr_exit_flags_t flags);

DR_API
/**
 * The #DR_DISALLOW_UNSAFE_STATIC declaration requests that DR perform sanity
 * checks to ensure that client libraries will also operate safely when linked
 * statically into an application.  This overrides that request, facilitating
 * having runtime options that are not supported in a static context.
 */
void
dr_allow_unsafe_static_behavior(void);

#ifdef UNIX
DR_API
/**
 * Registers a callback function for the fork event.  DR calls \p func
 * whenever the application forks a new process.
 * \note Valid on Linux only.
 */
void
dr_register_fork_init_event(void (*func)(void *drcontext));

DR_API
/**
 * Unregister a callback function for the fork event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_fork_init_event(void (*func)(void *drcontext));
#endif

DR_API
/**
 * Registers a callback function for the module load event.  DR calls
 * \p func whenever the application loads a module (typically a
 * library but this term includes the executable).  The \p loaded
 * parameter indicates whether the module is fully initialized by the
 * loader or in the process of being loaded.  This parameter is present
 * only for backward compatibility: current versions of DR always pass true,
 * and the client can assume that relocating, rebinding, and (on Linux) segment
 * remapping have already occurred.
 *
 * \note The module_data_t \p info passed to the callback routine is
 * valid only for the duration of the callback and should not be
 * freed; a persistent copy can be made with dr_copy_module_data().
 *
 * \note Registration cannot be done during the basic block event: it should be
 * done at initialization time.
 */
void
dr_register_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                           bool loaded));

DR_API
/**
 * Unregister a callback for the module load event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 *
 * \note Unregistering for this event is not supported during the
 * basic block event.
 */
bool
dr_unregister_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                             bool loaded));

DR_API
/**
 * Registers a callback function for the module unload event.  DR
 * calls \p func whenever the application unloads a module.
 * \note The module_data_t \p *info passed to
 * the callback routine is valid only for the duration of the callback
 * and should not be freed; a persistent copy can be made with
 * dr_copy_module_data().
 */
void
dr_register_module_unload_event(void (*func)(void *drcontext, const module_data_t *info));

DR_API
/**
 * Unregister a callback function for the module unload event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_module_unload_event(void (*func)(void *drcontext,
                                               const module_data_t *info));

/** Identifies the type of kernel transfer for dr_register_kernel_xfer_event(). */
typedef enum {
    DR_XFER_SIGNAL_DELIVERY,      /**< Signal delivery to application handler. */
    DR_XFER_SIGNAL_RETURN,        /**< Signal return system call. */
    DR_XFER_APC_DISPATCHER,       /**< Asynchronous procedure call dispatcher. */
    DR_XFER_EXCEPTION_DISPATCHER, /**< Exception dispatcher. */
    DR_XFER_RAISE_DISPATCHER,     /**< Raised exception dispatcher. */
    DR_XFER_CALLBACK_DISPATCHER,  /**< Callback dispatcher. */
    DR_XFER_CALLBACK_RETURN,    /**< A return from a callback by syscall or interrupt. */
    DR_XFER_CONTINUE,           /**< NtContinue system call. */
    DR_XFER_SET_CONTEXT_THREAD, /**< NtSetContextThread system call. */
    DR_XFER_CLIENT_REDIRECT,    /**< dr_redirect_execution() or #DR_SIGNAL_REDIRECT. */
    /**
     * A Linux restartable sequence was aborted.  The interrupted PC for a signal in
     * the execution instrumentation points to the precise interrupted
     * instruction; but for an abort in the native exeuction, the PC always points
     * to the abort handler, rather than the precise instruction that was aborted.
     * This aligns with kernel behavior: the interrupted PC is not saved anywhere.
     */
    DR_XFER_RSEQ_ABORT,
} dr_kernel_xfer_type_t;

/** Data structure passed for dr_register_kernel_xfer_event(). */
typedef struct _dr_kernel_xfer_info_t {
    /** The type of event. */
    dr_kernel_xfer_type_t type;
    /**
     * The source machine context which is about to be changed.  This may be NULL
     * if it is unknown, which is the case for #DR_XFER_CALLBACK_DISPATCHER.
     * For #DR_XFER_RSEQ_ABORT, due to the constraints of handling restartable
     * sequences, the abort PC will point prior to the committing store, while
     * that store already executed during instrumentation.  We recommend that
     * clients treat the store as never-executed in that situation, if possible,
     * to produce a more-representative sequence.
     */
    const dr_mcontext_t *source_mcontext;
    /**
     * The target program counter of the transfer.  To obtain the full target state,
     * call dr_get_mcontext().  (For efficiency purposes, only frequently needed
     * state is included by default.)
     */
    app_pc target_pc;
    /**
     * The target stack pointer of the transfer.  To obtain the full target state,
     * call dr_get_mcontext().  (For efficiency purposes, only frequently needed
     * state is included by default.)
     */
    reg_t target_xsp;
    /** For #DR_XFER_SIGNAL_DELIVERY and #DR_XFER_SIGNAL_RETURN, the signal number. */
    int sig;
} dr_kernel_xfer_info_t;

DR_API
/**
 * Registers a callback function for the kernel transfer event.  DR
 * calls \p func whenever the kernel is about to directly transfer control
 * without an explicit user-mode control transfer instruction.
 * This includes the following scenarios, which are distinguished by \p type:
 * - On UNIX, a signal is about to be delivered to an application handler.
 *   This event differs from a dr_register_signal_event() callback in that the
 *   latter is called regardless of whether the application has a handler,
 *   and it does not provide the target context of any handler.
 * - On UNIX, a signal return system call is about to be invoked.
 * - On Windows, the asynchronous procedure call dispatcher is about to be invoked.
 * - On Windows, the callback dispatcher is about to be invoked.
 * - On Windows, the exception dispatcher is about to be invoked.
 * - On Windows, the NtContinue system call is about to be invoked.
 * - On Windows, the NtSetContextThread system call is about to be invoked.
 * - On Windows, the NtCallbackReturn system call is about to be invoked.
 * - On Windows, interrupt 0x2b is about to be invoked.
 * - The client requests redirection using dr_redirect_execution() or
 *   #DR_SIGNAL_REDIRECT.
 *
 * The prior context, if known, is provided in \p info->source_mcontext; if
 * unknown, \p info->source_mcontext is NULL.  Multimedia state is typically
 * not provided in \p info->source_mcontext, which is reflected in its \p flags.
 *
 * The target program counter and stack are provided in \p info->target_pc and \p
 * info->target_xsp.  Further target state can be examined by calling
 * dr_get_mcontext() and modified by calling dr_set_mcontext().  Changes to the
 * target state, including the pc, are supported for all cases except
 * NtCallbackReturn and interrupt 0x2b.  However, dr_get_mcontext() and
 * dr_set_mcontext() are limited for the Windows system calls NtContinue and
 * NtSetContextThread to the ContextFlags set by the application: dr_get_mcontext()
 * will adjust the dr_mcontext_t.flags to reflect what's available, and
 * dr_set_mcontext() will only set what's also set in ContextFlags.  Given the
 * disparity in how Ebp/Rbp is handled (in #DR_MC_INTEGER but in CONTEXT_CONTROL),
 * clients that care about that register are better off using system call events
 * instead of kernel transfer events to take actions on these two system calls.
 *
 * This is a convenience event: all of the above events can be detected using
 * combinations of other events.  This event is meant to be used to identify all
 * changes in the program counter that do not arise from explicit control flow
 * instructions.
 */
void
dr_register_kernel_xfer_event(void (*func)(void *drcontext,
                                           const dr_kernel_xfer_info_t *info));

DR_API
/**
 * Unregister a callback function for the kernel transfer event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_kernel_xfer_event(void (*func)(void *drcontext,
                                             const dr_kernel_xfer_info_t *info));

#ifdef WINDOWS

/**
 * Data structure passed with an exception event.  Contains the
 * machine context and the Win32 exception record.
 */
typedef struct _dr_exception_t {
    /**
     * Machine context at exception point.  The client should not
     * change \p mcontext->flags: it should remain DR_MC_ALL.
     */
    dr_mcontext_t *mcontext;
    EXCEPTION_RECORD *record; /**< Win32 exception record. */
    /**
     * The raw pre-translated machine state at the exception interruption
     * point inside the code cache.  Clients are cautioned when examining
     * code cache instructions to not rely on any details of code inserted
     * other than their own.
     * The client should not change \p raw_mcontext.flags: it should
     * remain DR_MC_ALL.
     */
    dr_mcontext_t *raw_mcontext;
    /**
     * Information about the code fragment inside the code cache at
     * the exception interruption point.
     */
    dr_fault_fragment_info_t fault_fragment_info;
} dr_exception_t;

DR_API
/**
 * Registers a callback function for the exception event.  DR calls \p func
 * whenever the application throws an exception.  If \p func returns true,
 * the exception is delivered to the application's handler along with any
 * changes made to \p excpt->mcontext.  If \p func returns false, the
 * faulting instruction in the code cache is re-executed using \p
 * excpt->raw_mcontext, including any changes made to that structure.
 * Clients are expected to use \p excpt->raw_mcontext when using faults as
 * a mechanism to push rare cases out of an instrumentation fastpath that
 * need to examine instrumentation instructions rather than the translated
 * application state and should normally not examine it for application
 * instruction faults.  Certain registers may not contain proper
 * application values in \p excpt->raw_mcontext for exceptions in
 * application instructions.  Clients are cautioned against relying on any
 * details of code cache layout or register usage beyond instrumentation
 * inserted by the client itself when examining \p excpt->raw_mcontext.
 *
 * If multiple callbacks are registered, the first one that returns
 * false will short-circuit event delivery to later callbacks.
 *
 * DR raises this event for exceptions outside the code cache that
 * could come from code generated by a client.  For such exceptions,
 * mcontext is not translated and is identical to raw_mcontext.
 *
 * To skip the passing of the exception to the application's exception
 * handlers and to send control elsewhere instead, a client can call
 * dr_redirect_execution() from \p func.
 *
 * \note \p excpt->fault_fragment_info data is provided with
 * \p excpt->raw_mcontext. It is valid only if
 * \p excpt->fault_fragment_info.cache_start_pc is not \p NULL.
 * It provides clients information about the code fragment being
 * executed at the exception interruption point. Clients are cautioned
 * against relying on any details of code cache layout or register
 * usage beyond  instrumentation inserted by the client itself.
 * \note Only valid on Windows.
 * \note The function is not called for RaiseException.
 */
void
dr_register_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt));

DR_API
/**
 * Unregister a callback function for the exception event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt));
#endif /* WINDOWS */

DR_API
/**
 * Registers a callback function for the syscall filter event.  DR
 * calls \p func to decide whether to invoke the syscall events for
 * each system call site encountered with a statically-determinable
 * system call number.  If \p func returns true, the pre-syscall
 * (dr_register_pre_syscall_event()) and post-syscall
 * (dr_register_post_syscall_event()) events will be invoked.
 * Otherwise, the events may or may not occur, depending on whether DR
 * itself needs to intercept them and whether the system call number
 * is statically determinable.  System call number determination can
 * depend on whether the -opt_speed option is enabled.  If a system
 * call number is not determinable, the filter event will not be
 * called, but the pre and post events will be called.
 *
 * Intercepting every system call can be detrimental to performance
 * for certain types of applications.  Filtering provides for greater
 * performance by letting uninteresting system calls execute without
 * interception overhead.
 */
void
dr_register_filter_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_API
/**
 * Unregister a callback function for the syscall filter event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_filter_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_API
/**
 * Registers a callback function for the pre-syscall event.  DR calls
 * \p func whenever the application is about to invoke a system call,
 * if any client asked for that system call number to be intercepted
 * via the filter event (dr_register_filter_syscall_event()).
 * Any client registering a pre- or post-syscall event should also
 * register a filter event.
 *
 * The application parameters to the system call can be viewed with
 * dr_syscall_get_param() and set with dr_syscall_set_param().  The
 * system call number can also be changed with
 * dr_syscall_set_sysnum().
 *
 * The application's machine state can be accessed and set with
 * dr_get_mcontext() and dr_set_mcontext().  Changing registers in
 * this way overlaps with system call parameter changes on some
 * platforms.  On Linux, for SYS_clone, client changes to the ebp/rbp
 * register will be ignored by the clone child.
 *
 * On MacOS, whether 32-bit or 64-bit, the system call number passed
 * (\p sysnum) has been normalized to a positive number with the top 8
 * bits set to 0x1 for a Mach system call, 0x3 for Machdep, and 0x0
 * for BSD (allowing the direct use of SYS_ constants).  Access the
 * raw eax register to view the unmodified number.
 *
 * If \p func returns true, the application's system call is invoked
 * normally; if \p func returns false, the system call is skipped.  If
 * it is skipped, the return value can be set with
 * dr_syscall_set_result() or dr_syscall_set_result_ex().  If the
 * system call is skipped, there will not be a post-syscall event.
 * If multiple callbacks are registered, the first one that returns
 * false will short-circuit event delivery to later callbacks.
 */
void
dr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_API
/**
 * Unregister a callback function for the pre-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_pre_syscall_event(bool (*func)(void *drcontext, int sysnum));

DR_API
/**
 * Registers a callback function for the post-syscall event.  DR calls
 * \p func whenever the application just finished invoking a system
 * call, if any client asked for that system call number to be
 * intercepted via the filter event
 * (dr_register_filter_syscall_event()) or if DR itself needs to
 * intercept the system call.
 * Any client registering a pre- or post-syscall event should also
 * register a filter event.
 *
 * The result of the system call can be modified with
 * dr_syscall_set_result() or dr_syscall_set_result_ex().
 *
 * System calls that change control flow or terminate the current
 * thread or process typically do not have a post-syscall event.
 * These include SYS_exit, SYS_exit_group, SYS_execve, SYS_sigreturn,
 * and SYS_rt_sigreturn on Linux, and NtTerminateThread,
 * NtTerminateProcess (depending on the parameters), NtCallbackReturn,
 * and NtContinue on Windows.
 *
 * The application's machine state can be accessed and set with
 * dr_get_mcontext() and dr_set_mcontext().
 *
 * On MacOS, whether 32-bit or 64-bit, the system call number passed
 * (\p sysnum) has been normalized to a positive number with the top 8
 * bits set to 0x1 for a Mach system call, 0x3 for Machdep, and 0x0
 * for BSD (allowing the direct use of SYS_ constants).  Access the
 * raw eax register to view the unmodified number.
 *
 * Additional system calls may be invoked by calling
 * dr_syscall_invoke_another() prior to returning from the
 * post-syscall event callback.  The system call to be invoked should
 * be specified with dr_syscall_set_sysnum(), and its parameters can
 * be set with dr_syscall_set_param().
 */
void
dr_register_post_syscall_event(void (*func)(void *drcontext, int sysnum));

DR_API
/**
 * Unregister a callback function for the post-syscall event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_post_syscall_event(void (*func)(void *drcontext, int sysnum));

#ifdef UNIX

/* XXX: for PR 304708 I originally included siginfo_t in
 * dr_siginfo_t.  But can we really trust siginfo_t to be identical on
 * all supported platforms?  Esp. once we start supporting VMKUW,
 * MacOS, etc.  I'm removing it for now.  None of my samples need it,
 * and in my experience its fields are unreliable in any case.
 * PR 371370 covers re-adding it if users request it.
 * If we re-add it, we should use our kernel_siginfo_t version.
 */
/**
 * Data structure passed with a signal event.  Contains the machine
 * context at the signal interruption point and other signal
 * information.
 */
typedef struct _dr_siginfo_t {
    /** The signal number. */
    int sig;
    /** The context of the thread receiving the signal. */
    void *drcontext;
    /**
     * The application machine state at the signal interruption point.
     * The client should not change \p mcontext.flags: it should
     * remain DR_MC_ALL.
     */
    dr_mcontext_t *mcontext;
    /**
     * The raw pre-translated machine state at the signal interruption
     * point inside the code cache.  NULL for delayable signals.  Clients
     * are cautioned when examining code cache instructions to not rely on
     * any details of code inserted other than their own.
     * The client should not change \p mcontext.flags: it should
     * remain DR_MC_ALL.
     */
    dr_mcontext_t *raw_mcontext;
    /** Whether raw_mcontext is valid. */
    bool raw_mcontext_valid;
    /**
     * For SIGBUS and SIGSEGV, the address whose access caused the signal
     * to be raised (as calculated by DR).
     */
    byte *access_address;
    /**
     * Indicates this signal is blocked.  DR_SIGNAL_BYPASS is not allowed,
     * and a second event will be sent if the signal is later delivered to
     * the application.  Events are only sent for blocked non-delayable signals,
     * not for delayable signals.
     */
    bool blocked;
    /**
     * Information about the code fragment inside the code cache
     * at the signal interruption point.
     */
    dr_fault_fragment_info_t fault_fragment_info;
} dr_siginfo_t;

/**
 * Return value of client signal event callback, determining how DR will
 * proceed with the signal.
 */
typedef enum {
    /** Deliver signal to the application as normal. */
    DR_SIGNAL_DELIVER,
    /** Suppress signal as though it never happened. */
    DR_SIGNAL_SUPPRESS,
    /**
     * Deliver signal according to the default SIG_DFL action, as would
     * happen if the application had no handler.
     */
    DR_SIGNAL_BYPASS,
    /**
     * Do not deliver the signal.  Instead, redirect control to the
     * application state specified in dr_siginfo_t.mcontext.
     * Clients may modify dr_siginfo_t.mcontext.pc to redirect control to
     * a desired pc, but beware that it is important to set the app state
     * (like the register values) as expected by the transfer point.
     */
    DR_SIGNAL_REDIRECT,
} dr_signal_action_t;

DR_API
/**
 * Requests that DR call the provided callback function \p func whenever a
 * signal is received by any application thread.  The return value of \p
 * func determines whether DR delivers the signal to the application.
 * To redirect execution return DR_SIGNAL_REDIRECT (do not call
 * dr_redirect_execution() from a signal callback).  The callback function
 * will be called even if the application has no handler or has registered
 * a SIG_IGN or SIG_DFL handler.  If multiple callbacks are registered,
 * the first one that returns other than DR_SIGNAL_DELIVER will short-circuit
 * event delivery to later callbacks.
 *
 * Modifications to the fields of \p siginfo->mcontext will be propagated
 * to the application if it has a handler for the signal, if
 * DR_SIGNAL_DELIVER is returned.
 *
 * The \p siginfo->raw_mcontext data is only provided for non-delayable
 * signals (e.g., SIGSEGV) that must be delivered immediately.  Whether it
 * is supplied is specified in \p siginfo->raw_mcontext_valid.  It is
 * intended for clients using faults as a mechanism to push rare cases out
 * of an instrumentation fastpath that need to examine instrumentation
 * instructions rather than the translated application state.  Certain
 * registers may not contain proper application values in \p
 * excpt->raw_mcontext for exceptions in application instructions.  Clients
 * are cautioned against relying on any details of code cache layout or
 * register usage beyond instrumentation inserted by the client itself.  If
 * DR_SIGNAL_SUPPRESS is returned, \p siginfo->mcontext is ignored and \p
 * siginfo->raw_mcontext is used as the resumption context.  The client's
 * changes to \p siginfo->raw_mcontext will take effect.
 *
 * For a delayable signal, DR raises a signal event only when about to
 * deliver the signal to the application.  Thus, if the application has
 * blocked a delayable signal, the corresponding signal event will not
 * occur until the application unblocks the signal, even if such a signal
 * is delivered by the kernel.  For non-delayable signals, DR will raise a
 * signal event on initial receipt of the signal, with the \p
 * siginfo->blocked field set.  Such a blocked signal will have a second
 * event raised when it is delivered to the application (if it is not
 * suppressed by the client, and if there is not already a pending blocked
 * signal, for non-real-time signals).
 *
 * DR raises this event for faults outside the code cache that
 * could come from code generated by a client.  For such cases,
 * mcontext is not translated and is identical to raw_mcontext.
 *
 * DR will not raise a signal event for a SIGSEGV or SIGBUS
 * raised by a client code fault rather than the application.  Use
 * dr_safe_read(), dr_safe_write(), or DR_TRY_EXCEPT() to prevent such
 * faults.
 *
 * \note \p siginfo->fault_fragment_info data is provided
 * with \p siginfo->raw_mcontext. It is valid only if
 * \p siginfo->fault_fragment_info.cache_start_pc is not
 * \p NULL. It provides clients information about the code fragment
 * being executed at the signal interruption point. Clients are
 * cautioned against relying on any details of code cache layout or
 * register usage beyond instrumentation inserted by the client
 * itself.
 *
 * \note Only valid on Linux.
 *
 * \note DR always requests SA_SIGINFO for all signals.
 *
 * \note This version of DR does not intercept the signals SIGCONT,
 * SIGSTOP, SIGTSTP, SIGTTIN, or SIGTTOU.  Future versions should add
 * support for these signals.
 *
 * \note If the client uses signals for its own communication it should set
 * a flag to distinguish its own uses of signals from the application's
 * use.  Races where the two are re-ordered should not be problematic.
 */
void dr_register_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                         dr_siginfo_t *siginfo));

DR_API
/**
 * Unregister a callback function for the signal event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool dr_unregister_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                           dr_siginfo_t *siginfo));
#endif /* UNIX */

DR_API
/**
 * Registers a callback function for the low on memory event.  DR calls \p func
 * whenever virtual memory is tight and enables the client to help free space.
 */
void
dr_register_low_on_memory_event(void (*func)());

DR_API
/**
 * Unregister a callback function for low on memory events.
 * \return true if unregistration is successful and false if it is not
 * (e.g., the function was not registered).
 */
bool
dr_unregister_low_on_memory_event(void (*func)());

DR_API
/**
 * Registers a callback function that is invoked whenever a clean call is inserted
 * in instrumentation, such as by dr_insert_clean_call(), dr_insert_clean_call_ex(),
 * or dr_insert_clean_call_ex_varg().
 * 'where' is a label with note value #DR_NOTE_CLEAN_CALL_END; the clean call
 * sequence will be inserted prior to the label after all clean call callbacks
 * are finished (so the clean call sequence is not yet visible at the time this
 * callback is invoked).
 */
void
dr_register_clean_call_insertion_event(void (*func)(void *drcontext, instrlist_t *ilist,
                                                    instr_t *where,
                                                    dr_cleancall_save_t call_flags));

DR_API
/**
 * Unregisters a callback function that was registered with
 * dr_register_call_insertion_event().
 */
bool
dr_unregister_clean_call_insertion_event(void (*func)(void *drcontext, instrlist_t *ilist,
                                                      instr_t *where,
                                                      dr_cleancall_save_t call_flags));

/****************************************************************************
 * SECURITY SUPPORT
 */

#ifdef PROGRAM_SHEPHERDING

/** Types of security violations that can be received at a security violation event
 *  callback.
 *  - DR_RCO_* :
 *      A violation of the Restricted Code Origins policies.  The target address is not
 *      in an allowed execution area.
 *      - DR_RCO_STACK_VIOLATION - The target address is on the current thread's stack.
 *      - DR_RCO_HEAP_VIOLATION - The target address is not on the current thread's stack.
 *  - DR_RCT_* :
 *      A violation of the Restricted Control Transfer policies.  The transition from the
 *      source address to the target address is not allowed.
 *      - DR_RCT_RETURN_VIOLATION - The transition from source_pc to target_pc is via a
 *        return instruction.  The target address does not follow an executed call
 *        instruction and is not exempted.
 *      - DR_RCT_INDIRECT_CALL_VIOLATION - The transition from source_pc to target_pc is
 *        via an indirect call instruction.
 *      - DR_RCT_INDIRECT_JUMP_VIOLATION - The transition from source_pc to target_pc is
 *        via an indirect jmp instruction.
 *  - DR_UNKNOWN_VIOLATION :
 *      An unknown violation type, the client shouldn't expect to see this.
 */
typedef enum {
    DR_RCO_STACK_VIOLATION,
    DR_RCO_HEAP_VIOLATION,
    DR_RCT_RETURN_VIOLATION,
    DR_RCT_INDIRECT_CALL_VIOLATION,
    DR_RCT_INDIRECT_JUMP_VIOLATION,
    DR_UNKNOWN_VIOLATION,
} dr_security_violation_type_t;

/** Types of remediations available at a security violation event callback.
 *  - DR_VIOLATION_ACTION_CONTINUE :
 *       Continue application execution as if no violation occurred. Use this if the
 *       violation is determined to be a false positive.
 *  - DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT :
 *       Continue application execution after applying any changes made to the mcontext.
 *       Use this to fix up the application's state and continue execution.
 *  - DR_VIOLATION_ACTION_KILL_PROCESS :
 *       Immediately kills the process.  This is the safest course of action to take
 *       when faced with possibly corrupt application state, but availability concerns
 *       may dictate using one of the other choices, since they can be less disruptive.
 *  - DR_VIOLATION_ACTION_KILL_THREAD :
 *       Immediately kills the thread that caused the violation (the current thread).
 *       If the current thread is part of a pool of worker threads kept by the application
 *       then it's likely the application will recover gracefully.  If the thread is
 *       responsible for a particular function within the application (such as a
 *       particular service within an svchost process) then the application may continue
 *       with only that functionality lost.  Note that no cleanup of the thread's state
 *       is preformed (application locks it owns are not released and, for Windows NT and
 *       2000 its stack is not freed).  However, the client will still receive the thread
 *       exit event for this thread.
 *  - DR_VIOLATION_ACTION_THROW_EXCEPTION :
 *       Causes the application to receive an unreadable memory execution exception in
 *       the thread that caused the violation (the current thread).  The exception will
 *       appear to originate from an application attempt to execute from the target
 *       address.  If the application has good exception handling it may recover
 *       gracefully.
 */
typedef enum {
    DR_VIOLATION_ACTION_CONTINUE,
    DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT,
    DR_VIOLATION_ACTION_KILL_PROCESS,
    DR_VIOLATION_ACTION_KILL_THREAD,
    DR_VIOLATION_ACTION_THROW_EXCEPTION,
} dr_security_violation_action_t;

DR_API
/**
 * Registers a callback function for the security violation event.  DR
 * calls \p func whenever it intercepts a security violation.  Clients
 * can override the default remediation by changing \p action.  If
 * multiple callbacks are registered, the callback registered last has
 * final control over the action.  \note source_pc can be NULL if DR
 * fails to recreate the source pc.
 */
void
dr_register_security_event(void (*func)(void *drcontext, void *source_tag,
                                        app_pc source_pc, app_pc target_pc,
                                        dr_security_violation_type_t violation,
                                        dr_mcontext_t *mcontext,
                                        dr_security_violation_action_t *action));

DR_API
/**
 * Unregister a callback function for the security violation event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_security_event(void (*func)(void *drcontext, void *source_tag,
                                          app_pc source_pc, app_pc target_pc,
                                          dr_security_violation_type_t violation,
                                          dr_mcontext_t *mcontext,
                                          dr_security_violation_action_t *action));
#endif /* PROGRAM_SHEPHERDING */

DR_API
/**
 * Registers a callback function for nudge events.  External entities
 * can nudge a process through the dr_nudge_process() or
 * dr_nudge_pid() drconfig API routines on Windows or using the \p
 * drnudgeunix tool on Linux.  A client in this process can use
 * dr_nudge_client() to raise a nudge, while a client in another
 * process can use dr_nudge_client_ex().
 *
 * DR calls \p func whenever the current process receives a nudge.
 * On Windows, the nudge event is delivered in a new non-application
 * thread.  Callers must specify the target client by passing the
 * client ID that was provided in dr_client_main().
 */
void
dr_register_nudge_event(void (*func)(void *drcontext, uint64 argument), client_id_t id);

DR_API
/**
 * Unregister a callback function for the nudge event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_nudge_event(void (*func)(void *drcontext, uint64 argument), client_id_t id);

DR_API
/**
 * Triggers an asynchronous nudge event in the current process.  The callback
 * function registered with dr_register_nudge_event() will be called with the
 * supplied \p argument (in a new non-application thread on Windows).
 *
 * \note On Linux, the nudge will not be delivered until this thread exits
 * the code cache.  Thus, if this routine is called from a clean call,
 * dr_redirect_execution() should be used to ensure cache exit.
 */
bool
dr_nudge_client(client_id_t id, uint64 argument);

DR_API
/**
 * Triggers an asynchronous nudge event in a target process.  The callback
 * function registered with dr_register_nudge_event() for the
 * specified client in the specified process will be called with the
 * supplied \p argument (in a new non-application thread on Windows).
 *
 * \note On Linux, if \p pid is the current process, the nudge will
 * not be delivered until this thread exits the code cache.  Thus, if
 * this routine is called from a clean call and \p pid is the current
 * process, dr_redirect_execution() should be used to ensure cache exit.
 *
 * \param[in]   process_id      The system id of the process to nudge
 *                              (see dr_get_process_id())
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \param[in]   argument        An argument passed to the client's nudge
 *                              handler.
 *
 * \param[in]   timeout_ms      Windows only.  The number of milliseconds to wait for
 *                              each nudge to complete before continuing. If INFINITE
 *                              is supplied then the wait is unbounded. If 0
 *                              is supplied the no wait is performed.  If a
 *                              non-0 wait times out DR_NUDGE_TIMEOUT will be returned.
 *
 * \return      A dr_config_status_t code indicating the result of the nudge.
 */
dr_config_status_t
dr_nudge_client_ex(process_id_t process_id, client_id_t client_id, uint64 argument,
                   uint timeout_ms);

#ifdef WINDOWS
DR_API
/**
 * On Windows, nudges are implemented via remotely injected threads.
 * This routine returns whether or not the thread indicated by
 * \p drcontext is such a nudge thread.
 * \note Windows only.
 */
bool
dr_is_nudge_thread(void *drcontext);
#endif

/****************************************************************************
 * PERSISTENT CACHE SUPPORT
 */

DR_API
/**
 * Takes in the \p perscxt opaque parameter passed to various persistence
 * events and returns the beginning address of the code region being
 * persisted.
 */
app_pc
dr_persist_start(void *perscxt);

DR_API
/**
 * Takes in the \p perscxt opaque parameter passed to various persistence
 * events and returns the size of the code region being persisted.
 */
size_t
dr_persist_size(void *perscxt);

DR_API
/**
 * Takes in the \p perscxt opaque parameter passed to various
 * persistence events and returns whether the fragment identified by
 * \p tag is being persisted.  This routine can be called outside of a
 * persistence event, in which case the \p perscxt parameter should be
 * NULL.
 */
bool
dr_fragment_persistable(void *drcontext, void *perscxt, void *tag);

DR_API
/**
 * Registers callback functions for storing read-only data in each persisted
 * cache file.  When generating a new persisted cache file, DR first calls \p
 * func_size to obtain the size required for read-only data in each persisted
 * cache file.  DR subsequently calls \p func_persist to write the actual data.
 * DR ensures that no other thread will execute in between the calls
 * to \p func_size and \p func_persist.
 *
 * Upon loading a previously-written persisted cache file, DR calls \p
 * func_resurrect to validate and read back in data from the persisted file.
 *
 * For each callback, the \p perscxt parameter can be passed to the routines
 * dr_persist_start(), dr_persist_size(), and dr_fragment_persistable() to
 * identify the region of code being persisted.
 *
 * @param[in] func_size The function to call to determine the size needed for
 *   persisted data.  The \p file_offs parameter indicates the offset from the start
 *   of the persisted file where this data will reside (which is needed to
 *   calculate patch displacements).  The callback can store a void* value into
 *   the address specified by \p user_data.  This value will be passed to \p
 *   func_persist and if a patch callback is registered (see
 *   dr_register_persist_patch()) to \p func_patch.  The same value will be
 *   shared with persisted code callbacks (see dr_register_persist_rx()) and
 *   writable data callbacks (see dr_register_persist_rw()).
 * @param[in] func_persist  The function to call to write the actual data.
 *   Data to be persisted should be written to the file \p fd via
 *   dr_write_file().  The data will be read-only when the persisted file is
 *   loaded back in for use.  The return value of the function indicates success
 *   of the write.  If the function returns false, the persisted cache file
 *   being generated will be abandoned under the assumption of a non-recoverable
 *   error.
 * @param[in] func_resurrect  The function to call to validate previously written data.
 *   The \p map variable points to the mapped-in data that was written at
 *   persist time.  The return value of the function indicates success of the
 *   resurrection.  If the function returns false, the persisted cache file
 *   being loaded will be abandoned under the assumption of a non-recoverable
 *   error.  Any validation that the persisted file is suitable for use should
 *   be performed by the function prior to any restoration work needed for the
 *   data.  The \p map address should be updated to point to the end of
 *   the persisted data (i.e., on return it should equal its start value plus
 *   the size that was passed to dr_register_persist_ro_size()).
 *   DR will perform self-consistency checks, including whether the
 *   whole pcache is present and that a checksum of at least part of
 *   the file matches, prior to calling this callback.  Thus, the
 *   client can assume that it is not truncated.
 * \note \p func_resurrect may be called during persisted file generation if
 *  a persisted file already exists, in order to merge with that file.
 * \return whether successful.
 */
bool
dr_register_persist_ro(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT));

DR_API
/**
 * Unregister callback functions for storing read-only data in a persisted cache file.
 * \return true if unregistration is successful and false if it is not
 * (e.g., one of the functions was not registered).
 */
bool
dr_unregister_persist_ro(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT));

DR_API
/**
 * Registers callback functions for storing executable code (outside of normal
 * code blocks) in each persisted cache file.  When generating a new persisted
 * cache file, DR first calls \p func_size to obtain the size required for
 * executable code in each persisted cache file.  DR subsequently calls \p
 * func_persist to write the actual code.
 * DR ensures that no other thread will execute in between the calls
 * to \p func_size and \p func_persist.
 *
 * Upon loading a previously-written persisted cache file, DR calls \p
 * func_resurrect to validate and read back in code from the persisted
 * file.
 *
 * For each callback, the \p perscxt parameter can be passed to the routines
 * dr_persist_start(), dr_persist_size(), and dr_fragment_persistable() to
 * identify the region of code being persisted.
 *
 * @param[in] func_size  The function to call to determine the size needed
 *   for persisted code.  The \p file_offs parameter indicates the offset from the start
 *   of the persisted file where this code will reside (which is needed to
 *   calculate patch displacements).  The callback can store a void* value into
 *   the address specified by \p user_data.  This value will be passed to \p
 *   func_persist and if a patch callback is registered (see
 *   dr_register_persist_patch()) to \p func_patch.  The same value will be
 *   shared with read-only data callbacks (see dr_register_persist_ro()) and
 *   writable data callbacks (see dr_register_persist_rw()).
 * @param[in] func_persist  The function to call to write the actual code.
 *   Code to be persisted should be written to the file \p fd via
 *   dr_write_file().  The code will be read-only when the persisted file is
 *   loaded back in for use.  The return value of the function indicates success
 *   of the write.  If the function returns false, the persisted cache file
 *   being generated will be abandoned under the assumption of a non-recoverable
 *   error.
 * @param[in] func_resurrect  The function to call to validate previously written code.
 *   The \p map variable points to the mapped-in code that was written at
 *   persist time.  The return value of the function indicates success of the
 *   resurrection.  If the function returns false, the persisted cache file
 *   being loaded will be abandoned under the assumption of a non-recoverable
 *   error.  Any validation that the persisted file is suitable for use should
 *   be performed by the function prior to any restoration work needed for the
 *   code.  The \p map address should be updated to point to the end of
 *   the persisted data (i.e., on return it should equal its start value plus
 *   the size that was passed to dr_register_persist_rx_size()).
 *   DR will perform self-consistency checks, including whether the
 *   whole pcache is present and that a checksum of at least part of
 *   the file matches, prior to calling this callback.  Thus, the
 *   client can assume that it is not truncated.
 * \note \p func_resurrect may be called during persisted file generation if
 *  a persisted file already exists, in order to merge with that file.
 * \return whether successful.
 */
bool
dr_register_persist_rx(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT));

DR_API
/**
 * Unregister callback functions for storing executable code in a persisted cache file.
 * \return true if unregistration is successful and false if it is not
 * (e.g., one of the functions was not registered).
 */
bool
dr_unregister_persist_rx(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT));

DR_API
/**
 * Registers callback functions for storing writable data in each persisted
 * cache file.  When generating a new persisted cache file, DR first calls \p
 * func_size to obtain the size required for writable data in each persisted
 * cache file.  DR subsequently calls \p func_persist to write the actual data.
 * DR ensures that no other thread will execute in between the calls
 * to \p func_size and \p func_persist.
 *
 * Upon loading a previously-written persisted cache file, DR calls \p
 * func_resurrect to validate and read back in data from the persisted file.
 *
 * For each callback, the \p perscxt parameter can be passed to the routines
 * dr_persist_start(), dr_persist_size(), and dr_fragment_persistable() to
 * identify the region of code being persisted.
 *
 * @param[in] func_size  The function to call to determine the size needed
 *   for persisted data.  The \p file_offs parameter indicates the offset from the start
 *   of the persisted file where this data will reside (which is needed to
 *   calculate patch displacements).  The callback can store a void* value into
 *   the address specified by \p user_data.  This value will be passed to \p
 *   func_persist and if a patch callback is registered (see
 *   dr_register_persist_patch()) to \p func_patch.  The same value will be
 *   shared with persisted code callbacks (see dr_register_persist_rx()) and
 *   read-only data callbacks (see dr_register_persist_ro()).
 * @param[in] func_persist  The function to call to write the actual data.
 *   Data to be persisted should be written to the file \p fd via
 *   dr_write_file().  The data will be writable when the persisted file is
 *   loaded back in for use.  The return value of the function indicates success
 *   of the write.  If the function returns false, the persisted cache file
 *   being generated will be abandoned under the assumption of a non-recoverable
 *   error.
 * @param[in] func_resurrect  The function to call to validate previously written data.
 *   The \p map variable points to the mapped-in data that was written at
 *   persist time.  The return value of the function indicates success of the
 *   resurrection.  If the function returns false, the persisted cache file
 *   being loaded will be abandoned under the assumption of a non-recoverable
 *   error.  Any validation that the persisted file is suitable for use should
 *   be performed by the function prior to any restoration work needed for the
 *   data.  The \p map address should be updated to point to the end of
 *   the persisted data (i.e., on return it should equal its start value plus
 *   the size that was passed to dr_register_persist_rw_size()).
 *   DR will perform self-consistency checks, including whether the
 *   whole pcache is present and that a checksum of at least part of
 *   the file matches, prior to calling this callback.  Thus, the
 *   client can assume that it is not truncated.
 * \note \p func_resurrect may be called during persisted file generation if
 *  a persisted file already exists, in order to merge with that file.
 * \return whether successful.
 */
bool
dr_register_persist_rw(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT));

DR_API
/**
 * Unregister callback functions for storing writable data in a persisted cache file.
 * \return true if unregistration is successful and false if it is not
 * (e.g., one of the functions was not registered).
 */
bool
dr_unregister_persist_rw(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT));

DR_API
/**
 * \warning This patching interface is in flux and is subject to
 * change in the next release.  Consider it experimental in this
 * release.
 *
 * Registers a callback function for patching code prior to storing it in a
 * persisted cache file.  The length of each instruction cannot be changed, but
 * displacements and offsets can be adjusted to make the code
 * position-independent.  A patch callback is only called once per persisted
 * file, regardless of whether one or all of read-only, executable, or writable
 * data has been added.  Use the \p user_data parameter to pass the file offset
 * or other data from the other persistence events to this one.
 *
 * @param[in] func_patch  The function to call to perform any necessary
 *   patching of the to-be-persisted basic block code.  The function
 *   should decode up to \p bb_size bytes from \p bb_start and look for call or
 *   jump displacements or rip-relative data references that need to
 *   be updated to use data in the persisted file.  There is no padding
 *   between instructions, so a simple decode loop will find every instruction.
 *   The \p perscxt parameter can be passed to the routines
 *   dr_persist_start(), dr_persist_size(), and dr_fragment_persistable() to
 *   identify the region of code being persisted.
 * \return whether successful.
 */
bool
dr_register_persist_patch(bool (*func_patch)(void *drcontext, void *perscxt,
                                             byte *bb_start, size_t bb_size,
                                             void *user_data));

DR_API
/**
 * Unregister a callback function for patching persisted code.
 * \return true if unregistration is successful and false if it is not
 * (e.g., the function was not registered).
 */
bool
dr_unregister_persist_patch(bool (*func_patch)(void *drcontext, void *perscxt,
                                               byte *bb_start, size_t bb_size,
                                               void *user_data));

DR_API
/**
 * Query whether detach is in progress. This is useful for clients that want to
 * avoid the cost of resetting their global state on exit if there is no
 * detaching and thus no chance of re-attaching.
 */
bool
dr_is_detaching(void);

#endif /* _DR_EVENTS_H_ */
