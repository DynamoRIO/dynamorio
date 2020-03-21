/* **********************************************************
 * Copyright (c) 2010-2020 Google, Inc.  All rights reserved.
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

/*
 * instrument_api.h - exposed API for instrumentation
 */

#ifndef _INSTRUMENT_API_H_
#define _INSTRUMENT_API_H_ 1

#include "../globals.h"
#include "../module_shared.h"
#include "arch.h"
#include "instr.h"
#include "dr_config.h"

#ifdef CLIENT_INTERFACE

/* DR_API EXPORT TOFILE dr_events.h */
/* DR_API EXPORT BEGIN */
#    ifdef API_EXPORT_ONLY
#        include "dr_config.h"
#    endif

/**************************************************
 * ROUTINES TO REGISTER EVENT CALLBACKS
 */
/**
 * @file dr_events.h
 * @brief Event callback registration routines.
 */
/* DR_API EXPORT END */

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

/* DR_API EXPORT TOFILE dr_events.h */
/* DR_API EXPORT BEGIN */
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
/* DR_API EXPORT END */

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
 * \return a #dr_emit_flags_t flag.
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
 * \return a #dr_emit_flags_t flag.
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

#    ifdef CUSTOM_TRACES
/* DR_API EXPORT BEGIN */

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

/* DR_API EXPORT END */

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
#    endif

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

/* DR_API EXPORT BEGIN */
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
     * not in the code cache.  Clients are cautioned when examining
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
/* DR_API EXPORT END */

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
 * returning a different value than it returned during the corresponding
 * thread init event.
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

/* DR_API EXPORT TOFILE dr_events.h */
/* DR_API EXPORT BEGIN */
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
/* DR_API EXPORT END */

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

#    ifdef UNIX
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
#    endif

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

/* DR_API EXPORT BEGIN */
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
    DR_XFER_RSEQ_ABORT,         /**< A Linux restartable sequence was aborted. */
} dr_kernel_xfer_type_t;

/** Data structure passed for dr_register_kernel_xfer_event(). */
typedef struct _dr_kernel_xfer_info_t {
    /** The type of event. */
    dr_kernel_xfer_type_t type;
    /**
     * The source machine context which is about to be changed.  This may be NULL
     * if it is unknown, which is the case for #DR_XFER_CALLBACK_DISPATCHER and
     * #DR_XFER_RSEQ_ABORT (where the PC is not known but the rest of the state
     * matches the current state).
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
/* DR_API EXPORT END */

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

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */

/* DR_API EXPORT BEGIN */
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
/* DR_API EXPORT END */

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
/* DR_API EXPORT BEGIN */
#    endif /* WINDOWS */
/* DR_API EXPORT END */

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

/* DR_API EXPORT BEGIN */

#    ifdef UNIX
/* DR_API EXPORT END */

/* XXX: for PR 304708 I originally included siginfo_t in
 * dr_siginfo_t.  But can we really trust siginfo_t to be identical on
 * all supported platforms?  Esp. once we start supporting VMKUW,
 * MacOS, etc.  I'm removing it for now.  None of my samples need it,
 * and in my experience its fields are unreliable in any case.
 * PR 371370 covers re-adding it if users request it.
 * If we re-add it, we should use our kernel_siginfo_t version.
 */
/* DR_API EXPORT BEGIN */
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
     */
    DR_SIGNAL_REDIRECT,
} dr_signal_action_t;
/* DR_API EXPORT END */

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
/* DR_API EXPORT BEGIN */
#    endif /* UNIX */
/* DR_API EXPORT END */

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

/****************************************************************************
 * SECURITY SUPPORT
 */

#    ifdef PROGRAM_SHEPHERDING
/* DR_API EXPORT BEGIN */

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

/* DR_API EXPORT END */

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
#    endif /* PROGRAM_SHEPHERDING */

DR_API
/**
 * Registers a callback function for nudge events.  External entities
 * can nudge a process through the dr_nudge_process() or
 * dr_nudge_pid() drconfig API routines on Windows or using the \p
 * nudgeunix tool on Linux.  A client in this process can use
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

#    ifdef WINDOWS
DR_API
/**
 * On Windows, nudges are implemented via remotely injected threads.
 * This routine returns whether or not the thread indicated by
 * \p drcontext is such a nudge thread.
 * \note Windows only.
 */
bool
dr_is_nudge_thread(void *drcontext);
#    endif

/* DR_API EXPORT TOFILE dr_tools.h */
/* DR_API EXPORT BEGIN */

/**************************************************
 * TOP-LEVEL ROUTINES
 */
/**
 * @file dr_tools.h
 * @brief Main API routines, including transparency support.
 */
/* DR_API EXPORT END */

DR_API
/**
 * Creates a DR context that can be used in a standalone program.
 * \warning This context cannot be used as the drcontext for a thread
 * running under DR control!  It is only for standalone programs that
 * wish to use DR as a library of disassembly, etc. routines.
 * \return NULL on failure, such as running on an unsupported operating
 * system version.
 */
void *
dr_standalone_init(void);

DR_API
/**
 * Restores application state modified by dr_standalone_init(), which can
 * include some signal handlers.
 */
void
dr_standalone_exit(void);

/* DR_API EXPORT BEGIN */

#    ifdef API_EXPORT_ONLY
/**
 * Use this dcontext for use with the standalone static decoder library.
 * Pass it whenever a decoding-related API routine asks for a context.
 */
#        define GLOBAL_DCONTEXT ((void *)-1)
#    endif

/**************************************************
 * UTILITY ROUTINES
 */

#    ifdef WINDOWS
/**
 * If \p x is false, displays a message about an assertion failure
 * (appending \p msg to the message) and then calls dr_abort()
 */
#        define DR_ASSERT_MSG(x, msg)                                                   \
            ((void)((!(x)) ? (dr_messagebox("ASSERT FAILURE: %s:%d: %s (%s)", __FILE__, \
                                            __LINE__, #x, msg),                         \
                              dr_abort(), 0)                                            \
                           : 0))
#    else
#        define DR_ASSERT_MSG(x, msg)                                                \
            ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)\n", \
                                         __FILE__, __LINE__, #x, msg),               \
                              dr_abort(), 0)                                         \
                           : 0))
#    endif

/**
 * If \p x is false, displays a message about an assertion failure and
 * then calls dr_abort()
 */
#    define DR_ASSERT(x) DR_ASSERT_MSG(x, "")

/* DR_API EXPORT END */

DR_API
/** Returns true if all DynamoRIO caches are thread private. */
bool
dr_using_all_private_caches(void);

DR_API
/** \deprecated Replaced by dr_set_process_exit_behavior() */
void
dr_request_synchronized_exit(void);

DR_API
/**
 * Returns the client-specific option string specified at client
 * registration.  \p client_id is the client ID passed to dr_client_main().
 *
 * \deprecated This routine is replaced by dr_client_main()'s arguments and
 * by dr_get_option_array().
 * The front-end \p drrun and other utilities now re-quote all tokens,
 * providing simpler option passing without escaping or extra quote layers.
 * This routine, for compatibility, strips those quotes off and returns
 * a flat string without any token-delimiting quotes.
 */
const char *
dr_get_options(client_id_t client_id);

DR_API
/**
 * Returns the client-specific option string specified at client
 * registration, parsed into an array of \p argc separate option tokens
 * stored in \p argv.  This is the same array of arguments passed
 * to the dr_client_main() routine.
 */
bool
dr_get_option_array(client_id_t client_id, int *argc OUT, const char ***argv OUT);

DR_API
/**
 * Read the value of a string DynamoRIO runtime option named \p option_name into
 * \p buf.  Options are listed in \ref sec_options.  DynamoRIO has many other
 * undocumented options which may be queried through this API, but they are not
 * officially supported.  The option value is truncated to \p len bytes and
 * null-terminated.
 * \return false if no option named \p option_name exists, and true otherwise.
 */
bool
dr_get_string_option(const char *option_name, char *buf OUT, size_t len);

DR_API
/**
 * Read the value of an integer DynamoRIO runtime option named \p option_name
 * into \p val.  This includes boolean options.  Options are listed in \ref
 * sec_options.  DynamoRIO has many other undocumented options which may be
 * queried through this API, but they are not officially supported.
 * \warning Always pass a full uint64 for \p val even if the option is a smaller
 * integer to avoid overwriting nearby data.
 * \return false if no option named \p option_name exists, and true otherwise.
 */
bool
dr_get_integer_option(const char *option_name, uint64 *val OUT);

DR_API
/**
 * Returns the client library name and path that were originally specified
 * to load the library.  If the resulting string is longer than #MAXIMUM_PATH
 * it will be truncated.  \p client_id is the client ID passed to a client's
 * dr_client_main() function.
 */
const char *
dr_get_client_path(client_id_t client_id);

DR_API
/**
 * Returns the base address of the client library.  \p client_id is
 * the client ID passed to a client's dr_client_main() function.
 */
byte *
dr_get_client_base(client_id_t client_id);

DR_API
/**
 * Sets information presented to users in diagnostic messages.
 * Only one name is supported, regardless of how many clients are in use.
 * If this routine is called a second time, the new values supersede
 * the original.
 * The \p report_URL is meant to be a bug tracker location where users
 * should go to report errors in the client end-user tool.
 */
bool
dr_set_client_name(const char *name, const char *report_URL);

DR_API
/**
 * Sets the version string presented to users in diagnostic messages.
 * This has a maximum length of 96 characters; anything beyond that is
 * silently truncated.
 */
bool
dr_set_client_version_string(const char *version);

DR_API
/** Returns the image name (without path) of the current application. */
const char *
dr_get_application_name(void);

DR_API
/** Returns the process id of the current process. */
process_id_t
dr_get_process_id(void);

#    ifdef UNIX
DR_API
/**
 * Returns the process id of the parent of the current process.
 * \note Linux only.
 */
process_id_t
dr_get_parent_id(void);
#    endif

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS

/** Windows versions */
/* http://msdn.microsoft.com/en-us/library/windows/desktop/ms724832(v=vs.85).aspx */
typedef enum {
    /** Windows 10 1803 major update. */
    DR_WINDOWS_VERSION_10_1803 = 105,
    /** Windows 10 1709 major update. */
    DR_WINDOWS_VERSION_10_1709 = 104,
    /** Windows 10 1703 major update. */
    DR_WINDOWS_VERSION_10_1703 = 103,
    /** Windows 10 1607 major update. */
    DR_WINDOWS_VERSION_10_1607 = 102,
    /**
     * Windows 10 TH2 1511.  For future Windows updates that change system call
     * numbers, we'll perform our own artificial minor version number update as
     * done here, and use the YYMM version as the sub-name, as officially the OS
     * version will supposedly remain 10.0 forever.
     */
    DR_WINDOWS_VERSION_10_1511 = 101,
    /** Windows 10 pre-TH2 */
    DR_WINDOWS_VERSION_10 = 100,
    /** Windows 8.1 */
    DR_WINDOWS_VERSION_8_1 = 63,
    /** Windows Server 2012 R2 */
    DR_WINDOWS_VERSION_2012_R2 = DR_WINDOWS_VERSION_8_1,
    /** Windows 8 */
    DR_WINDOWS_VERSION_8 = 62,
    /** Windows Server 2012 */
    DR_WINDOWS_VERSION_2012 = DR_WINDOWS_VERSION_8,
    /** Windows 7 */
    DR_WINDOWS_VERSION_7 = 61,
    /** Windows Server 2008 R2 */
    DR_WINDOWS_VERSION_2008_R2 = DR_WINDOWS_VERSION_7,
    /** Windows Vista */
    DR_WINDOWS_VERSION_VISTA = 60,
    /** Windows Server 2008 */
    DR_WINDOWS_VERSION_2008 = DR_WINDOWS_VERSION_VISTA,
    /** Windows Server 2003 */
    DR_WINDOWS_VERSION_2003 = 52,
    /** Windows XP 64-bit */
    DR_WINDOWS_VERSION_XP_X64 = DR_WINDOWS_VERSION_2003,
    /** Windows XP */
    DR_WINDOWS_VERSION_XP = 51,
    /** Windows 2000 */
    DR_WINDOWS_VERSION_2000 = 50,
    /** Windows NT */
    DR_WINDOWS_VERSION_NT = 40,
} dr_os_version_t;

/** Data structure used with dr_get_os_version() */
typedef struct _dr_os_version_info_t {
    /** The size of this structure.  Set this to sizeof(dr_os_version_info_t). */
    size_t size;
    /** The operating system version */
    dr_os_version_t version;
    /** The service pack major number */
    uint service_pack_major;
    /** The service pack minor number */
    uint service_pack_minor;
    /** The build number. */
    uint build_number;
    /** The release identifier (such as "1803" for a Windows 10 release). */
    char release_id[64];
    /** The edition (such as "Education" or "Professional"). */
    char edition[64];
} dr_os_version_info_t;
/* DR_API EXPORT END */

DR_API
/**
 * Returns information about the version of the operating system.
 * Returns whether successful.  \note Windows only.  \note The Windows
 * API routine GetVersionEx may hide distinctions between versions,
 * such as between Windows 8 and Windows 8.1.  DR reports the true
 * low-level version.
 *
 */
bool
dr_get_os_version(dr_os_version_info_t *info);

DR_API
/**
 * Returns true if this process is a 32-bit process operating on a
 * 64-bit Windows kernel, known as Windows-On-Windows-64, or WOW64.
 * Returns false otherwise.  \note Windows only.
 */
bool
dr_is_wow64(void);

DR_API
/**
 * Returns a pointer to the application's Process Environment Block
 * (PEB).  DR swaps to a private PEB when running client code, in
 * order to isolate the client and its dependent libraries from the
 * application, so conventional methods of reading the PEB will obtain
 * the private PEB instead of the application PEB.  \note Windows only.
 */
void *
dr_get_app_PEB(void);

DR_API
/**
 * Converts a process handle to a process id.
 * \return Process id if successful; INVALID_PROCESS_ID on failure.
 * \note Windows only.
 */
process_id_t
dr_convert_handle_to_pid(HANDLE process_handle);

DR_API
/**
 * Converts a process id to a process handle.
 * \return Process handle if successful; INVALID_HANDLE_VALUE on failure.
 * \note Windows only.
 */
HANDLE
dr_convert_pid_to_handle(process_id_t pid);

/* DR_API EXPORT BEGIN */
#    endif /* WINDOWS */
/* DR_API EXPORT END */

DR_API
/** Retrieves the current time. */
void
dr_get_time(dr_time_t *time);

DR_API
/**
 * Returns the number of milliseconds since Jan 1, 1601 (this is
 * the current UTC time).
 *
 * \note This is the Windows standard.  UNIX time functions typically
 * count from the Epoch (Jan 1, 1970).  The Epoch is 11644473600*1000
 * milliseconds after Jan 1, 1601.
 */
uint64
dr_get_milliseconds(void);

DR_API
/**
 * Returns the number of microseconds since Jan 1, 1601 (this is
 * the current UTC time).
 *
 * \note This is the Windows standard.  UNIX time functions typically
 * count from the Epoch (Jan 1, 1970).  The Epoch is 11644473600*1000*1000
 * microseconds after Jan 1, 1601.
 */
uint64
dr_get_microseconds(void);

DR_API
/**
 * Returns a pseudo-random number in the range [0..max).
 * The pseudo-random sequence can be repeated by passing the seed
 * used during a run to the next run via the -prng_seed runtime option.
 */
uint
dr_get_random_value(uint max);

DR_API
/**
 * Sets the seed used for dr_get_random_value().  Generally this would
 * only be called during client initialization.
 */
void
dr_set_random_seed(uint seed);

DR_API
/** Returns the seed used for dr_get_random_value(). */
uint
dr_get_random_seed(void);

DR_API
/**
 * Aborts the process immediately without any cleanup (i.e., the exit event
 * will not be called).
 */
void
dr_abort(void);

DR_API
/**
 * Aborts the process immediately without any cleanup (i.e., the exit event
 * will not be called) with the exit code \p exit_code.
 *
 * On Linux, only the bottom 8 bits of \p exit_code will be honored
 * for a normal exit.  If bits 9..16 are not all zero, DR will send an
 * unhandled signal of that signal number instead of performing a normal
 * exit.
 */
void
dr_abort_with_code(int exit_code);

DR_API
/**
 * Exits the process, first performing a full cleanup that will
 * trigger the exit event (dr_register_exit_event()).  The process
 * exit code is set to \p exit_code.
 *
 * On Linux, only the bottom 8 bits of \p exit_code will be honored
 * for a normal exit.  If bits 9..16 are not all zero, DR will send an
 * unhandled signal of that signal number instead of performing a normal
 * exit.
 *
 * \note Calling this from \p dr_client_main or from the primary thread's
 * initialization event is not guaranteed to always work, as DR may
 * invoke a thread exit event where a thread init event was never
 * called.  We recommend using dr_abort_ex() or waiting for full
 * initialization prior to use of this routine.
 */
void
dr_exit_process(int exit_code);

/* DR_API EXPORT BEGIN */
/** Indicates the type of memory dump for dr_create_memory_dump(). */
typedef enum {
    /**
     * A "livedump", or "ldmp", DynamoRIO's own custom memory dump format.
     * The ldmp format does not currently support specifying a context
     * for the calling thread, so it will always include the call frames
     * to dr_create_memory_dump().  The \p ldmp.exe tool can be used to
     * create a dummy process (using the \p dummy.exe executable) which
     * can then be attached to by the debugger (use a non-invasive attach)
     * in order to view the memory dump contents.
     *
     * \note Windows only.
     */
    DR_MEMORY_DUMP_LDMP = 0x0001,
} dr_memory_dump_flags_t;

/** Indicates the type of memory dump for dr_create_memory_dump(). */
typedef struct _dr_memory_dump_spec_t {
    /** The size of this structure.  Set this to sizeof(dr_memory_dump_spec_t). */
    size_t size;
    /** The type of memory dump requested. */
    dr_memory_dump_flags_t flags;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This string is
     * stored inside the ldmp as the reason for the dump.
     */
    const char *label;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This is an optional output
     * field that, if non-NULL, will be written with the path to the created file.
     */
    char *ldmp_path;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This is the maximum size,
     * in bytes, of ldmp_path.
     */
    size_t ldmp_path_size;
} dr_memory_dump_spec_t;
/* DR_API EXPORT END */

DR_API
/**
 * Requests that DR create a memory dump file of the current process.
 * The type of dump is specified by \p spec.
 *
 * \return whether successful.
 *
 * \note this function is only supported on Windows for now.
 */
bool
dr_create_memory_dump(dr_memory_dump_spec_t *spec);

/* DR_API EXPORT BEGIN */

/**************************************************
 * APPLICATION-INDEPENDENT MEMORY ALLOCATION
 */

/** Flags used with dr_custom_alloc() */
typedef enum {
    /**
     * If this flag is not specified, dr_custom_alloc() uses a managed
     * heap to allocate the memory, just like dr_thread_alloc() or
     * dr_global_alloc().  In that case, it ignores any requested
     * protection bits (\p prot parameter), and the location (\p addr
     * parameter) must be NULL.  If this flag is specified, a
     * page-aligned, separate block of memory is allocated, in a
     * similar fashion to dr_nonheap_alloc().
     */
    DR_ALLOC_NON_HEAP = 0x0001,
    /**
     * This flag only applies to heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is not specified).  If this flag is not
     * specified, global heap is used (just like dr_global_alloc())
     * and the \p drcontext parameter is ignored.  If it is specified,
     * thread-private heap specific to \p drcontext is used, just like
     * dr_thread_alloc().
     */
    DR_ALLOC_THREAD_PRIVATE = 0x0002,
    /**
     * Allocate memory that is 32-bit-displacement reachable from the
     * code caches and from the client library.  Memory allocated
     * through dr_thread_alloc(), dr_global_alloc(), and
     * dr_nonheap_alloc() is also reachable, but for
     * dr_custom_alloc(), the resulting memory is not reachable unless
     * this flag is specified.  If this flag is passed, the requested
     * location (\p addr parameter) must be NULL.  This flag is not
     * compatible with #DR_ALLOC_LOW_2GB, #DR_ALLOC_FIXED_LOCATION, or
     * #DR_ALLOC_NON_DR.
     */
    DR_ALLOC_CACHE_REACHABLE = 0x0004,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified).  The flag requests that
     * memory be allocated at a specific address, given in the \p addr
     * parameter.  Without this flag, the \p addr parameter is not
     * honored.  This flag is not compatible with #DR_ALLOC_LOW_2GB or
     * #DR_ALLOC_CACHE_REACHABLE.
     */
    DR_ALLOC_FIXED_LOCATION = 0x0008,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified) in 64-bit mode.  The flag
     * requests that memory be allocated in the low 2GB of the address
     * space.  If this flag is passed, the requested location (\p addr
     * parameter) must be NULL.  This flag is not compatible with
     * #DR_ALLOC_FIXED_LOCATION.
     */
    DR_ALLOC_LOW_2GB = 0x0010,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified).  When this flag is specified,
     * the allocated memory is not considered to be DynamoRIO or tool
     * memory and thus is not kept separate from the application.
     * This is similar to dr_raw_mem_alloc().  Use of this memory is
     * at the client's own risk.  This flag is not compatible with
     * #DR_ALLOC_CACHE_REACHABLE.
     */
    DR_ALLOC_NON_DR = 0x0020,
#    ifdef WINDOWS
    /**
     * This flag only applies to non-heap, non-DR memory (i.e., when
     * both #DR_ALLOC_NON_HEAP and #DR_ALLOC_NON_DR are specified) on
     * Windows.  When this flag is specified, the allocated memory is
     * reserved but not committed, just like the MEM_RESERVE Windows API
     * flag (the default is MEM_RESERVE|MEM_COMMIT).
     */
    DR_ALLOC_RESERVE_ONLY = 0x0040,
    /**
     * This flag only applies to non-heap, non-DR memory (i.e., when both
     * #DR_ALLOC_NON_HEAP and #DR_ALLOC_NON_DR are specified) on Windows.
     * This flag must be combined with DR_ALLOC_FIXED_LOCATION.  When this
     * flag is specified, previously allocated memory is committed, just
     * like the MEM_COMMIT Windows API flag (when this flag is not passed,
     * the effect is MEM_RESERVE|MEM_COMMIT).  When passed to
     * dr_custom_free(), this flag causes a de-commit, just like the
     * MEM_DECOMMIT Windows API flag.  This flag cannot be combined with
     * #DR_ALLOC_LOW_2GB and must include a non-NULL requested location (\p
     * addr parameter).
     */
    DR_ALLOC_COMMIT_ONLY = 0x0080,
#    endif
} dr_alloc_flags_t;
/* DR_API EXPORT END */

DR_API
/**
 * Allocates \p size bytes of memory from DR's memory pool specific to the
 * thread associated with \p drcontext.
 */
void *
dr_thread_alloc(void *drcontext, size_t size);

DR_API
/**
 * Frees thread-specific memory allocated by dr_thread_alloc().
 * \p size must be the same as that passed to dr_thread_alloc().
 */
void
dr_thread_free(void *drcontext, void *mem, size_t size);

DR_API
/** Allocates \p size bytes of memory from DR's global memory pool. */
void *
dr_global_alloc(size_t size);

DR_API
/**
 * Frees memory allocated by dr_global_alloc().
 * \p size must be the same as that passed to dr_global_alloc().
 */
void
dr_global_free(void *mem, size_t size);

DR_API
/**
 * Allocates memory with the properties requested by \p flags.
 *
 * If \p addr is non-NULL (only allowed with certain flags), it must
 * be page-aligned.
 *
 * To make more space available for the code caches when running
 * larger applications, or for clients that use a lot of heap memory
 * that is not directly referenced from the cache, we recommend that
 * dr_custom_alloc() be called to obtain memory that is not guaranteed
 * to be reachable from the code cache (by not passing
 * #DR_ALLOC_CACHE_REACHABLE).  This frees up space in the reachable
 * region.
 *
 * Returns NULL on failure.
 */
void *
dr_custom_alloc(void *drcontext, dr_alloc_flags_t flags, size_t size, uint prot,
                void *addr);

DR_API
/**
 * Frees memory allocated by dr_custom_alloc().  The same \p flags
 * and \p size must be passed here as were passed to dr_custom_alloc().
 */
bool
dr_custom_free(void *drcontext, dr_alloc_flags_t flags, void *addr, size_t size);

DR_API
/**
 * Allocates \p size bytes of memory as a separate allocation from DR's
 * heap, allowing for separate protection.
 * The \p prot protection should use the DR_MEMPROT_READ,
 * DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * When creating a region to hold dynamically generated code, use
 * this routine in order to create executable memory.
 */
void *
dr_nonheap_alloc(size_t size, uint prot);

DR_API
/**
 * Frees memory allocated by dr_nonheap_alloc().
 * \p size must be the same as that passed to dr_nonheap_alloc().
 */
void
dr_nonheap_free(void *mem, size_t size);

DR_API
/**
 * \warning This raw memory allocation interface is in flux and is subject to
 * change in the next release.  Consider it experimental in this release.
 *
 * Allocates \p size bytes (page size aligned) of memory as a separate
 * allocation at preferred base \p addr that must be page size aligned,
 * allowing for separate protection.
 * If \p addr is NULL, an arbitrary address is picked.
 *
 * The \p prot protection should use the DR_MEMPROT_READ,
 * DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * The allocated memory is not considered to be DynamoRIO or tool memory and
 * thus is not kept separate from the application. Use of this memory is at the
 * client's own risk.
 *
 * The resulting memory is guaranteed to be initialized to all zeroes.
 *
 * Returns the actual address allocated or NULL if memory allocation at
 * preferred base fails.
 */
void *
dr_raw_mem_alloc(size_t size, uint prot, void *addr);

DR_API
/**
 * Frees memory allocated by dr_raw_mem_alloc().
 * \p addr and \p size must be the same as that passed to dr_raw_mem_alloc()
 * on Windows.
 */
bool
dr_raw_mem_free(void *addr, size_t size);

#    ifdef LINUX
DR_API
/**
 * Calls mremap with the specified parameters and returns the result.
 * The old memory must be non-DR memory, and the new memory is also
 * considered to be non-DR memory (see #DR_ALLOC_NON_DR).
 * \note Linux-only.
 */
void *
dr_raw_mremap(void *old_address, size_t old_size, size_t new_size, int flags,
              void *new_address);

DR_API
/**
 * Sets the program break to the specified value.  Invokes the SYS_brk
 * system call and returns the result.  This is the application's
 * program break, so use this system call only when deliberately
 * changing the application's behavior.
 * \note Linux-only.
 */
void *
dr_raw_brk(void *new_address);
#    endif

DR_API
/**
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of malloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option to
 * replace a client's use of malloc, realloc, and free with internal
 * versions that allocate memory from DR's private pool.  With -wrap,
 * clients can link to libraries that allocate heap memory without
 * interfering with application allocations.
 */
void *
__wrap_malloc(size_t size);

DR_API
/**
 * Reallocates memory from DR's global memory pool, but mimics the
 * behavior of realloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 */
void *
__wrap_realloc(void *mem, size_t size);

DR_API
/**
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of calloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 */
void *
__wrap_calloc(size_t nmemb, size_t size);

DR_API
/**
 * Frees memory from DR's global memory pool.  Memory must have been
 * allocated with __wrap_malloc(). The __wrap routines are intended to
 * be used with ld's -wrap option; see __wrap_malloc() for more
 * information.
 */
void
__wrap_free(void *mem);

DR_API
/**
 * Allocates memory for a new string identical to 'str' and copies the
 * contents of 'str' into the new string, including a terminating
 * null.  Memory must be freed with __wrap_free().  The __wrap
 * routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 */
char *
__wrap_strdup(const char *str);

/* DR_API EXPORT BEGIN */

/**************************************************
 * MEMORY QUERY/ACCESS ROUTINES
 */

#    ifdef DR_PAGE_SIZE_COMPATIBILITY

#        undef PAGE_SIZE
/**
 * Size of a page of memory. This uses a function call so be careful
 * where performance is critical.
 */
#        define PAGE_SIZE dr_page_size()

/**
 * Convenience macro to align to the start of a page of memory.
 * It uses a function call so be careful where performance is critical.
 */
#        define PAGE_START(x) (((ptr_uint_t)(x)) & ~(dr_page_size() - 1))

#    endif /* DR_PAGE_SIZE_COMPATIBILITY */

/* DR_API EXPORT END */

DR_API
/** Returns the size of a page of memory. */
size_t
dr_page_size(void);

DR_API
/**
 * Checks to see that all bytes with addresses in the range [\p pc, \p pc + \p size - 1]
 * are readable and that reading from that range won't generate an exception (see also
 * dr_safe_read() and DR_TRY_EXCEPT()).
 * \note Nothing guarantees that the memory will stay readable for any length of time.
 * \note On Linux, especially if the app is in the middle of loading a library
 * and has not properly set up the .bss yet, a page that seems readable can still
 * generate SIGBUS if beyond the end of an mmapped file.  Use dr_safe_read() or
 * DR_TRY_EXCEPT() to avoid such problems.
 */
bool
dr_memory_is_readable(const byte *pc, size_t size);

/* FIXME - this is a real view of memory including changes made for dr cache consistency,
 * but what we really want to show the client is the apps view of memory (which would
 * requires fixing correcting the view and fixing up exceptions for areas we made read
 * only) - see PR 198873 */
DR_API
/**
 * An os neutral method for querying a memory address. Returns true
 * iff a memory region containing \p pc is found.  If found additional
 * information about the memory region is returned in the optional out
 * arguments \p base_pc, \p size, and \p prot where \p base_pc is the
 * start address of the memory region continaing \p pc, \p size is the
 * size of said memory region and \p prot is an ORed combination of
 * DR_MEMPROT_* flags describing its current protection.
 *
 * \note To examine only application memory, skip memory for which
 * dr_memory_is_dr_internal() or dr_memory_is_in_client() returns true.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, it will include both #DR_MEMPROT_WRITE
 * and #DR_MEMPROT_PRETEND_WRITE in \p prot.
 */
bool
dr_query_memory(const byte *pc, byte **base_pc, size_t *size, uint *prot);

DR_API
/**
 * Provides additional information beyond dr_query_memory().
 * Returns true if it was able to obtain information (including about
 * free regions) and sets the fields of \p info.  This routine can be
 * used to iterate over the entire address space.  Such an iteration
 * should stop on reaching the top of the address space, or on
 * reaching kernel memory (look for #DR_MEMTYPE_ERROR_WINKERNEL) on
 * Windows.
 *
 * Returns false on failure and sets info->type to a DR_MEMTYPE_ERROR*
 * code indicating the reason for failure.
 *
 * \note To examine only application memory, skip memory for which
 * dr_memory_is_dr_internal() returns true.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, it will include both #DR_MEMPROT_WRITE
 * and #DR_MEMPROT_PRETEND_WRITE in \p info->prot.
 */
bool
dr_query_memory_ex(const byte *pc, OUT dr_mem_info_t *info);

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Equivalent to the win32 API function VirtualQuery().
 * See that routine for a description of
 * arguments and return values.  \note Windows only.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, this routine will indicate that the
 * memory is writable.  Call dr_query_memory() or dr_query_memory_ex()
 * before attempting to write to application memory to ensure it's
 * not read-only underneath.
 */
size_t
dr_virtual_query(const byte *pc, MEMORY_BASIC_INFORMATION *mbi, size_t mbi_size);
/* DR_API EXPORT BEGIN */
#    endif
/* DR_API EXPORT END */

DR_API
/**
 * Safely reads \p size bytes from address \p base into buffer \p
 * out_buf.  Reading is done without the possibility of an exception
 * occurring.  Returns true if the entire \p size bytes were read;
 * otherwise returns false and if \p bytes_read is non-NULL returns the
 * partial number of bytes read in \p bytes_read.
 * \note See also DR_TRY_EXCEPT().
 */
bool
dr_safe_read(const void *base, size_t size, void *out_buf, size_t *bytes_read);

DR_API
/**
 * Safely writes \p size bytes from buffer \p in_buf to address \p
 * base.  Writing is done without the possibility of an exception
 * occurring.    Returns true if the entire \p size bytes were written;
 * otherwise returns false and if \p bytes_written is non-NULL returns the
 * partial number of bytes written in \p bytes_written.
 * \note See also DR_TRY_EXCEPT().
 */
bool
dr_safe_write(void *base, size_t size, const void *in_buf, size_t *bytes_written);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
void
dr_try_setup(void *drcontext, void **try_cxt);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
int
dr_try_start(void *buf);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
void
dr_try_stop(void *drcontext, void *try_cxt);

/* DR_API EXPORT BEGIN */
/**
 * Simple try..except support for executing operations that might
 * fault and recovering if they do.  Be careful with this feature
 * as it has some limitations:
 * - do not use a return within a try statement (we do not have
 *   language support)
 * - any automatic variables that you want to use in the except
 *   block should be declared volatile
 * - no locks should be grabbed in a try statement (because
 *   there is no finally support to release them)
 * - nesting is supported, but finally statements are not
 *   supported
 *
 * For fault-free reads in isolation, use dr_safe_read() instead.
 * dr_safe_read() out-performs DR_TRY_EXCEPT.
 *
 * For fault-free writes in isolation, dr_safe_write() can be used,
 * although on Windows it invokes a system call and can be less
 * performant than DR_TRY_EXCEPT.
 */
#    define DR_TRY_EXCEPT(drcontext, try_statement, except_statement)  \
        do {                                                           \
            void *try_cxt;                                             \
            dr_try_setup(drcontext, &try_cxt);                         \
            if (dr_try_start(try_cxt) == 0) {                          \
                try_statement dr_try_stop(drcontext, try_cxt);         \
            } else {                                                   \
                /* roll back first in case except faults or returns */ \
                dr_try_stop(drcontext, try_cxt);                       \
                except_statement                                       \
            }                                                          \
        } while (0)
/* DR_API EXPORT END */

DR_API
/**
 * Modifies the memory protections of the region from \p start through
 * \p start + \p size.  Modification of memory allocated by DR or of
 * the DR or client libraries themselves is allowed under the
 * assumption that the client knows what it is doing.  Modification of
 * the ntdll.dll library on Windows is not allowed.  Returns true if
 * successful.
 */
bool
dr_memory_protect(void *base, size_t size, uint new_prot);

DR_API
/**
 * Returns true iff pc is memory allocated by DR for its own
 * purposes, and would not exist if the application were run
 * natively.
 */
bool
dr_memory_is_dr_internal(const byte *pc);

DR_API
/**
 * Returns true iff pc is located inside a client library, an Extension
 * library used by a client, or an auxiliary client library (see
 * dr_load_aux_library()).
 */
bool
dr_memory_is_in_client(const byte *pc);

/* DR_API EXPORT BEGIN */

/**************************************************
 * CLIENT AUXILIARY LIBRARIES
 */
/* DR_API EXPORT END */

DR_API
/**
 * Loads the library with the given path as an auxiliary client
 * library.  The library is not treated as an application module but
 * as an extension of DR.  The library will be included in
 * dr_memory_is_in_client() and any faults in the library will be
 * considered client faults.  The bounds of the loaded library are
 * returned in the optional out variables.  On failure, returns NULL.
 *
 * If only a filename and not a full path is given, this routine will
 * search for the library in the standard search locations for DR's
 * private loader.
 */
dr_auxlib_handle_t
dr_load_aux_library(const char *name, byte **lib_start /*OPTIONAL OUT*/,
                    byte **lib_end /*OPTIONAL OUT*/);

DR_API
/**
 * Looks up the exported routine with the given name in the given
 * client auxiliary library loaded by dr_load_aux_library().  Returns
 * NULL on failure.
 */
dr_auxlib_routine_ptr_t
dr_lookup_aux_library_routine(dr_auxlib_handle_t lib, const char *name);

DR_API
/**
 * Unloads the given library, which must have been loaded by
 * dr_load_aux_library().  Returns whether successful.
 */
bool
dr_unload_aux_library(dr_auxlib_handle_t lib);

/* DR_API EXPORT BEGIN */

#    if defined(WINDOWS) && !defined(X64)
/* DR_API EXPORT END */
DR_API
/**
 * Similar to dr_load_aux_library(), but loads a 64-bit library for
 * access from a 32-bit process running on a 64-bit Windows kernel.
 * Fails if called from a 32-bit kernel or from a 64-bit process.
 * The library will be located in the low part of the address space
 * with 32-bit addresses.
 * Functions in the library can be called with dr_invoke_x64_routine().
 *
 * \warning Invoking 64-bit code is fragile.  Currently, this routine
 * uses the system loader, under the assumption that little isolation
 * is needed versus application 64-bit state.  Consider use of this routine
 * experimental: use at your own risk!
 *
 * \note Windows only.
 *
 * \note Currently this routine does not support loading kernel32.dll
 * or any library that depends on it.
 * It also does not invoke the entry point for any dependent libraries
 * loaded as part of loading \p name.
 *
 * \note Currently this routine does not support Windows 8 or higher.
 */
dr_auxlib64_handle_t
dr_load_aux_x64_library(const char *name);

DR_API
/**
 * Looks up the exported routine with the given name in the given
 * 64-bit client auxiliary library loaded by dr_load_aux_x64_library().  Returns
 * NULL on failure.
 * The returned function can be called with dr_invoke_x64_routine().
 *
 * \note Windows only.
 *
 * \note Currently this routine does not support Windows 8.
 */
dr_auxlib64_routine_ptr_t
dr_lookup_aux_x64_library_routine(dr_auxlib64_handle_t lib, const char *name);

DR_API
/**
 * Unloads the given library, which must have been loaded by
 * dr_load_aux_x64_library().  Returns whether successful.
 *
 * \note Windows only.
 */
bool
dr_unload_aux_x64_library(dr_auxlib64_handle_t lib);

DR_API
/**
 * Must be called from 32-bit mode.  Switches to 64-bit mode, calls \p
 * func64 with the given parameters, switches back to 32-bit mode, and
 * then returns to the caller.  Requires that \p func64 be located in
 * the low 4GB of the address space.  All parameters must be 32-bit
 * sized, and all are widened via sign-extension when passed to \p
 * func64.
 *
 * \return -1 on failure; else returns the return value of \p func64.
 *
 * \warning Invoking 64-bit code is fragile.  The WOW64 layer assumes
 * there is no other 64-bit code that will be executed.
 * dr_invoke_x64_routine() attempts to save the WOW64 state, but it
 * has not been tested in all versions of WOW64.  Also, invoking
 * 64-bit code that makes callbacks is not supported, as not only a
 * custom wrapper to call the 32-bit code in the right mode would be
 * needed, but also a way to restore the WOW64 state in case the
 * 32-bit callback makes a system call.  Consider use of this routine
 * experimental: use at your own risk!
 *
 * \note Windows only.
 */
int64
dr_invoke_x64_routine(dr_auxlib64_routine_ptr_t func64, uint num_params, ...);

/* DR_API EXPORT BEGIN */

#    endif /* WINDOWS && !X64 */
/* DR_API EXPORT END */

/* DR_API EXPORT BEGIN */

/**************************************************
 * LOCK SUPPORT
 */
/* DR_API EXPORT END */

DR_API
/**
 * Initializes a mutex.
 *
 * Warning: there are restrictions on when DR-provided mutexes, and
 * locks in general, can be held by a client: no lock should be held
 * while application code is executing in the code cache.  Locks can
 * be used while inside client code reached from clean calls out of
 * the code cache, but they must be released before returning to the
 * cache.  A lock must also be released by the same thread that acquired
 * it.  Failing to follow these restrictions can lead to deadlocks.
 */
void *
dr_mutex_create(void);

DR_API
/** Deletes \p mutex. */
void
dr_mutex_destroy(void *mutex);

DR_API
/** Locks \p mutex.  Waits until the mutex is successfully held. */
void
dr_mutex_lock(void *mutex);

DR_API
/**
 * Unlocks \p mutex.  Asserts that mutex is currently locked by the
 * current thread.
 */
void
dr_mutex_unlock(void *mutex);

DR_API
/** Tries once to lock \p mutex and returns whether or not successful. */
bool
dr_mutex_trylock(void *mutex);

DR_API
/**
 * Returns true iff \p mutex is owned by the calling thread.
 * This routine is only available in debug builds.
 * In release builds it always returns true.
 */
bool
dr_mutex_self_owns(void *mutex);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \warning This routine is not sufficient on its own to prevent deadlocks
 * during scenarios where DR wants to suspend all threads such as detach or
 * relocation. See dr_app_recurlock_lock() and dr_mark_safe_to_suspend().
 *
 * \return whether successful.
 */
bool
dr_mutex_mark_as_app(void *mutex);

DR_API
/**
 * Creates and initializes a read-write lock.  A read-write lock allows
 * multiple readers or alternatively a single writer.  The lock
 * restrictions for mutexes apply (see dr_mutex_create()).
 */
void *
dr_rwlock_create(void);

DR_API
/** Deletes \p rwlock. */
void
dr_rwlock_destroy(void *rwlock);

DR_API
/** Acquires a read lock on \p rwlock. */
void
dr_rwlock_read_lock(void *rwlock);

DR_API
/** Releases a read lock on \p rwlock. */
void
dr_rwlock_read_unlock(void *rwlock);

DR_API
/** Acquires a write lock on \p rwlock. */
void
dr_rwlock_write_lock(void *rwlock);

DR_API
/** Releases a write lock on \p rwlock. */
void
dr_rwlock_write_unlock(void *rwlock);

DR_API
/** Tries once to acquire a write lock on \p rwlock and returns whether successful. */
bool
dr_rwlock_write_trylock(void *rwlock);

DR_API
/** Returns whether the calling thread owns the write lock on \p rwlock. */
bool
dr_rwlock_self_owns_write_lock(void *rwlock);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \return whether successful.
 */
bool
dr_rwlock_mark_as_app(void *rwlock);

DR_API
/**
 * Creates and initializes a recursive lock.  A recursive lock allows
 * the same thread to acquire it multiple times.  The lock
 * restrictions for mutexes apply (see dr_mutex_create()).
 */
void *
dr_recurlock_create(void);

DR_API
/** Deletes \p reclock. */
void
dr_recurlock_destroy(void *reclock);

DR_API
/** Acquires \p reclock, or increments the ownership count if already owned. */
void
dr_recurlock_lock(void *reclock);

DR_API
/**
 * Acquires \p reclock, or increments the ownership count if already owned.
 * Calls to this method which block (i.e. when the lock is already held) are
 * marked safe to suspend AND transfer; in that case the provided mcontext \p mc
 * will overwrite the current thread's mcontext. \p mc must have a valid PC
 * and its flags must be DR_MC_ALL.
 *
 * This routine must be used in clients holding application locks to prevent
 * deadlocks in a way similar to dr_mark_safe_to_suspend(), but this routine
 * is intended to be called by a clean call and may return execution to the
 * provided mcontext rather than returning normally.
 *
 * If this routine is called from a clean call, callers should not return
 * normally. Instead, dr_redirect_execution() or dr_redirect_native_target()
 * should be called to to prevent a return into a flushed code page.
 */
void
dr_app_recurlock_lock(void *reclock, dr_mcontext_t *mc);

DR_API
/** Decrements the ownership count of \p reclock and releases if zero. */
void
dr_recurlock_unlock(void *reclock);

DR_API
/** Tries once to acquire \p reclock and returns whether successful. */
bool
dr_recurlock_trylock(void *reclock);

DR_API
/** Returns whether the calling thread owns \p reclock. */
bool
dr_recurlock_self_owns(void *reclock);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \return whether successful.
 */
bool
dr_recurlock_mark_as_app(void *reclock);

DR_API
/** Creates an event object on which threads can wait and be signaled. */
void *
dr_event_create(void);

DR_API
/** Destroys an event object. */
bool
dr_event_destroy(void *event);

DR_API
/** Suspends the current thread until \p event is signaled. */
bool
dr_event_wait(void *event);

DR_API
/** Wakes up at most one thread waiting on \p event. */
bool
dr_event_signal(void *event);

DR_API
/** Resets \p event to no longer be in a signaled state. */
bool
dr_event_reset(void *event);

DR_API
/**
 * Use this function to mark a region of code as safe for DR to suspend
 * the client while inside the region.  DR will not relocate the client
 * from the region and will resume it at precisely the suspend point.
 *
 * This function must be used in client code that acquires application locks.
 * Use this feature with care!  Do not mark code as safe to suspend that has
 * a code cache return point.  I.e., do not call this routine from a clean
 * call. For acquiring application locks from a clean call, see
 * dr_app_recurlock_lock().
 *
 * No DR locks can be held while in a safe region.  Consequently, do
 * not call this routine from any DR event callback.  It may only be used
 * from natively executing code.
 *
 * Always invoke this routine in pairs, with the first passing true
 * for \p enter and the second passing false, thus delimiting the
 * region.
 */
bool
dr_mark_safe_to_suspend(void *drcontext, bool enter);

DR_API
/**
 * Atomically adds \p val to \p *dest and returns the sum.
 * \p dest must not straddle two cache lines.
 */
int
dr_atomic_add32_return_sum(volatile int *dest, int val);

#    ifdef X64
DR_API
/**
 * Atomically adds \p val to \p *dest and returns the sum.
 * \p dest must not straddle two cache lines.
 */
int64
dr_atomic_add64_return_sum(volatile int64 *dest, int64 val);
#    endif

/* DR_API EXPORT BEGIN */
/**************************************************
 * MODULE INFORMATION ROUTINES
 */

/** For dr_module_iterator_* interface */
typedef void *dr_module_iterator_t;

#    ifdef AVOID_API_EXPORT
/* We always give copies of the module_area_t information to clients (in the form
 * of a module_data_t defined below) to avoid locking issues (see PR 225020). */
/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
#    endif
#    ifdef UNIX
/** Holds information on a segment of a loaded module. */
typedef struct _module_segment_data_t {
    app_pc start;  /**< Start address of the segment, page-aligned backward. */
    app_pc end;    /**< End address of the segment, page-aligned forward. */
    uint prot;     /**< Protection attributes of the segment */
    uint64 offset; /**< Offset of the segment from the beginning of the backing file */
} module_segment_data_t;
#    endif

/**
 * Holds information about a loaded module. \note On Linux the start address can be
 * cast to an Elf32_Ehdr or Elf64_Ehdr. \note On Windows the start address can be cast to
 * an IMAGE_DOS_HEADER for use in finding the IMAGE_NT_HEADER and its OptionalHeader.
 * The OptionalHeader can be used to walk the module sections (among other things).
 * See WINNT.H. \note On MacOS the start address can be cast to mach_header or
 * mach_header_64.
 * \note When accessing any memory inside the module (including header fields)
 * user is responsible for guarding against corruption and the possibility of the module
 * being unmapped.
 */
struct _module_data_t {
    union {
        app_pc start;           /**< starting address of this module */
        module_handle_t handle; /**< module_handle for use with dr_get_proc_address() */
    };                          /* anonymous union of start address and module handle */
    /**
     * Ending address of this module.  If the module is not contiguous
     * (which is common on MacOS, and can happen on Linux), this is the
     * highest address of the module, but there can be gaps in between start
     * and end that are either unmapped or that contain other mappings or
     * libraries.   Use the segments array to examine each mapped region,
     * and use dr_module_contains_addr() as a convenience routine, rather than
     * checking against [start..end).
     */
    app_pc end;

    app_pc entry_point; /**< entry point for this module as specified in the headers */

    uint flags; /**< Reserved, set to 0 */

    module_names_t names; /**< struct containing name(s) for this module; use
                           * dr_module_preferred_name() to get the preferred name for
                           * this module */

    char *full_path; /**< full path to the file backing this module */

#    ifdef WINDOWS
    version_number_t file_version;    /**< file version number from .rsrc section */
    version_number_t product_version; /**< product version number from .rsrc section */
    uint checksum;                    /**< module checksum from the PE headers */
    uint timestamp;                   /**< module timestamp from the PE headers */
    /** Module internal size (from PE headers SizeOfImage). */
    size_t module_internal_size;
#    else
    bool contiguous;   /**< whether there are no gaps between segments */
    uint num_segments; /**< number of segments */
    /**
     * Array of num_segments entries, one per segment.  The array is sorted
     * by the start address of each segment.
     */
    module_segment_data_t *segments;
    uint timestamp;               /**< Timestamp from ELF Mach-O headers. */
#        ifdef MACOS
    uint current_version;         /**< Current version from Mach-O headers. */
    uint compatibility_version;   /**< Compatibility version from Mach-O headers. */
    byte uuid[16];                /**< UUID from Mach-O headers. */
#        endif
#    endif
#    ifdef AVOID_API_EXPORT
    /* FIXME: PR 215890: ELF64 size? Anything else? */
    /* We can add additional fields to the end without breaking compatibility */
#    endif
};

/* DR_API EXPORT END */

DR_API
/**
 * Looks up the module containing \p pc.  If a module containing \p pc is found
 * returns a module_data_t describing that module.  Returns NULL if \p pc is
 * outside all known modules, which is the case for most dynamically generated
 * code.  Can be used to obtain a module_handle_t for dr_lookup_module_section()
 * or dr_get_proc_address() via the \p handle field inside module_data_t.
 *
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_lookup_module(byte *pc);

DR_API
/**
 * Looks up the module with name \p name ignoring case.  If an exact name match is found
 * returns a module_data_t describing that module else returns NULL.  User must call
 * dr_free_module_data() on the returned module_data_t once finished. Can be used to
 * obtain a module_handle_t for dr_get_proc_address().
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_lookup_module_by_name(const char *name);

DR_API
/**
 * Looks up module data for the main executable.
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_get_main_module(void);

DR_API
/**
 * Initialize a new module iterator.  The returned module iterator contains a snapshot
 * of the modules loaded at the time it was created.  Use dr_module_iterator_hasnext()
 * and dr_module_iterator_next() to walk the loaded modules.  Call
 * dr_module_iterator_stop() when finished to release the iterator. \note The iterator
 * does not prevent modules from being loaded or unloaded while the iterator is being
 * walked.
 */
dr_module_iterator_t *
dr_module_iterator_start(void);

DR_API
/**
 * Returns true if there is another loaded module in the iterator.
 */
bool
dr_module_iterator_hasnext(dr_module_iterator_t *mi);

DR_API
/**
 * Retrieves the module_data_t for the next loaded module in the iterator. User must call
 * dr_free_module_data() on the returned module_data_t once finished.
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_module_iterator_next(dr_module_iterator_t *mi);

DR_API
/**
 * User should call this routine to free the module iterator.
 */
void
dr_module_iterator_stop(dr_module_iterator_t *mi);

DR_API
/**
 * Makes a copy of \p data.  Copy must be freed with dr_free_module_data().
 * Useful for making persistent copies of module_data_t's received as part of
 * image load and unload event callbacks.
 */
module_data_t *
dr_copy_module_data(const module_data_t *data);

DR_API
/**
 * Frees a module_data_t returned by dr_module_iterator_next(), dr_lookup_module(),
 * dr_lookup_module_by_name(), or dr_copy_module_data(). \note Should NOT be used with
 * a module_data_t obtained as part of a module load or unload event.
 */
void
dr_free_module_data(module_data_t *data);

DR_API
/**
 * Returns the preferred name for the module described by \p data from
 * \p data->module_names.
 */
const char *
dr_module_preferred_name(const module_data_t *data);

DR_API
/**
 * Returns whether \p addr is contained inside any segment of the module \p data.
 * We recommend using this routine rather than checking against the \p start
 * and \p end fields of \p data, as modules are not always contiguous.
 */
bool
dr_module_contains_addr(const module_data_t *data, app_pc addr);

/* DR_API EXPORT BEGIN */
/**
 * Iterator over the list of modules that a given module imports from.  Created
 * by calling dr_module_import_iterator_start() and must be freed by calling
 * dr_module_import_iterator_stop().
 *
 * \note On Windows, delay-loaded DLLs are not included yet.
 *
 * \note ELF does not import directly from other modules.
 */
struct _dr_module_import_iterator_t;
typedef struct _dr_module_import_iterator_t dr_module_import_iterator_t;

/**
 * Descriptor used to iterate the symbols imported from a specific module.
 */
struct _dr_module_import_desc_t;
typedef struct _dr_module_import_desc_t dr_module_import_desc_t;

/**
 * Module import data returned from dr_module_import_iterator_next().
 *
 * String fields point into the importing module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.  The iterator routines
 * themselves handle faults by stopping the iteration.
 *
 * \note ELF does not import directly from other modules.
 */
typedef struct _dr_module_import_t {
    /**
     * Specified name of the imported module or API set.
     */
    const char *modname;

    /**
     * Opaque handle that can be passed to dr_symbol_import_iterator_start().
     * Valid until the original module is unmapped.
     */
    dr_module_import_desc_t *module_import_desc;
} dr_module_import_t;
/* DR_API EXPORT END */

DR_API
/**
 * Creates a module import iterator.  Iterates over the list of modules that a
 * given module imports from.
 *
 * \note ELF does not import directly from other modules.
 */
dr_module_import_iterator_t *
dr_module_import_iterator_start(module_handle_t handle);

DR_API
/**
 * Returns true if there is another module import in the iterator.
 *
 * \note ELF does not import directly from other modules.
 */
bool
dr_module_import_iterator_hasnext(dr_module_import_iterator_t *iter);

DR_API
/**
 * Advances the passed-in iterator and returns the current module import in the
 * iterator.  The pointer returned is only valid until the next call to
 * dr_module_import_iterator_next() or dr_module_import_iterator_stop().
 *
 * \note ELF does not import directly from other modules.
 */
dr_module_import_t *
dr_module_import_iterator_next(dr_module_import_iterator_t *iter);

DR_API
/**
 * Stops import iteration and frees a module import iterator.
 *
 * \note ELF does not import directly from other modules.
 */
void
dr_module_import_iterator_stop(dr_module_import_iterator_t *iter);

/* DR_API EXPORT BEGIN */
/**
 * Symbol import iterator data type.  Can be created by calling
 * dr_symbol_import_iterator_start() and must be freed by calling
 * dr_symbol_import_iterator_stop().
 */
struct _dr_symbol_import_iterator_t;
typedef struct _dr_symbol_import_iterator_t dr_symbol_import_iterator_t;

/**
 * Symbol import data returned from dr_symbol_import_iterator_next().
 *
 * String fields point into the importing module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.
 */
typedef struct _dr_symbol_import_t {
    const char *name;    /**< Name of imported symbol, if available. */
    const char *modname; /**< Preferred name of module (Windows only). */
    bool delay_load;     /**< This import is delay-loaded (Windows only). */
    bool by_ordinal;     /**< Import is by ordinal, not name (Windows only). */
    ptr_uint_t ordinal;  /**< Ordinal value (Windows only). */
    /* DR_API EXPORT END */
    /* We never ask the client to allocate this struct, so we can go ahead and
     * add fields here without breaking ABI compat.
     */
    /* DR_API EXPORT BEGIN */
} dr_symbol_import_t;
/* DR_API EXPORT END */

DR_API
/**
 * Creates an iterator over symbols imported by a module.  If \p from_module is
 * NULL, all imported symbols are yielded, regardless of which module they were
 * imported from.
 *
 * On Windows, from_module is obtained from a \p dr_module_import_t and used to
 * iterate over all of the imports from a specific module.
 *
 * The iterator returned is invalid until after the first call to
 * dr_symbol_import_iterator_next().
 *
 * \note On Windows, symbols imported from delay-loaded DLLs are not included
 * yet.
 */
dr_symbol_import_iterator_t *
dr_symbol_import_iterator_start(module_handle_t handle,
                                dr_module_import_desc_t *from_module);

DR_API
/**
 * Returns true if there is another imported symbol in the iterator.
 */
bool
dr_symbol_import_iterator_hasnext(dr_symbol_import_iterator_t *iter);

DR_API
/**
 * Returns the next imported symbol.  The returned pointer is valid until the
 * next call to dr_symbol_import_iterator_next() or
 * dr_symbol_import_iterator_stop().
 */
dr_symbol_import_t *
dr_symbol_import_iterator_next(dr_symbol_import_iterator_t *iter);

DR_API
/**
 * Stops symbol import iteration and frees the iterator.
 */
void
dr_symbol_import_iterator_stop(dr_symbol_import_iterator_t *iter);

/* DR_API EXPORT BEGIN */
/**
 * Symbol export iterator data type.  Can be created by calling
 * dr_symbol_export_iterator_start() and must be freed by calling
 * dr_symbol_export_iterator_stop().
 */
struct _dr_symbol_export_iterator_t;
typedef struct _dr_symbol_export_iterator_t dr_symbol_export_iterator_t;

/**
 * Symbol export data returned from dr_symbol_export_iterator_next().
 *
 * String fields point into the exporting module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.
 *
 * On Windows, the address in \p addr may not be inside the exporting module if
 * it is a forward and has been patched by the loader.  In that case, \p forward
 * will be NULL.
 */
typedef struct _dr_symbol_export_t {
    const char *name;    /**< Name of exported symbol, if available. */
    app_pc addr;         /**< Address of the exported symbol. */
    const char *forward; /**< Forward name, or NULL if not forwarded (Windows only). */
    ptr_uint_t ordinal;  /**< Ordinal value (Windows only). */
    /**
     * Whether an indirect code object (see dr_export_info_t).  (Linux only).
     */
    bool is_indirect_code;
    bool is_code; /**< Whether code as opposed to exported data (Linux only). */
    /* DR_API EXPORT END */
    /* We never ask the client to allocate this struct, so we can go ahead and
     * add fields here without breaking ABI compat.
     */
    /* DR_API EXPORT BEGIN */
} dr_symbol_export_t;
/* DR_API EXPORT END */

DR_API
/**
 * Creates an iterator over symbols exported by a module.
 * The iterator returned is invalid until after the first call to
 * dr_symbol_export_iterator_next().
 *
 * \note To iterate over all symbols in a module and not just those exported,
 * use the \ref page_drsyms.
 */
dr_symbol_export_iterator_t *
dr_symbol_export_iterator_start(module_handle_t handle);

DR_API
/**
 * Returns true if there is another exported symbol in the iterator.
 */
bool
dr_symbol_export_iterator_hasnext(dr_symbol_export_iterator_t *iter);

DR_API
/**
 * Returns the next exported symbol.  The returned pointer is valid until the
 * next call to dr_symbol_export_iterator_next() or
 * dr_symbol_export_iterator_stop().
 */
dr_symbol_export_t *
dr_symbol_export_iterator_next(dr_symbol_export_iterator_t *iter);

DR_API
/**
 * Stops symbol export iteration and frees the iterator.
 */
void
dr_symbol_export_iterator_stop(dr_symbol_export_iterator_t *iter);

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Returns whether \p pc is within a section within the module in \p section_found and
 * information about that section in \p section_out. \note Not yet available on Linux.
 */
bool
dr_lookup_module_section(module_handle_t lib, byte *pc,
                         IMAGE_SECTION_HEADER *section_out);

/* DR_API EXPORT BEGIN */
#    endif /* WINDOWS */
/* DR_API EXPORT END */

DR_API
/**
 * Set whether or not the module referred to by \p handle should be
 * instrumented.  If \p should_instrument is false, code from the module will
 * not be passed to the basic block event.  If traces are enabled, code from the
 * module will still reach the trace event.  Must be called from the module load
 * event for the module referred to by \p handle.
 * \return whether successful.
 *
 * \warning Turning off instrumentation for modules breaks clients and
 * extensions, such as drwrap, that expect to see every instruction.
 */
bool
dr_module_set_should_instrument(module_handle_t handle, bool should_instrument);

DR_API
/**
 * Return whether code from the module should be instrumented, meaning passed
 * to the basic block event.
 */
bool
dr_module_should_instrument(module_handle_t handle);

DR_API
/**
 * Returns the entry point of the exported function with the given
 * name in the module with the given base.  Returns NULL on failure.
 *
 * On Linux, when we say "exported" we mean present in the dynamic
 * symbol table (.dynsym).  Global functions and variables in an
 * executable (as opposed to a library) are not exported by default.
 * If an executable is built with the \p -rdynamic flag to \p gcc, its
 * global symbols will be present in .dynsym and dr_get_proc_address()
 * will locate them.  Otherwise, the drsyms Extension (see \ref
 * page_drsyms) must be used to locate the symbols.  drsyms searches
 * the debug symbol table (.symtab) in addition to .dynsym.
 *
 * \note On Linux this ignores symbol preemption by other modules and only
 * examines the specified module.
 * \note On Linux, in order to handle indirect code objects, use
 * dr_get_proc_address_ex().
 */
generic_func_t
dr_get_proc_address(module_handle_t lib, const char *name);

/* DR_API EXPORT BEGIN */
/**
 * Data structure used by dr_get_proc_address_ex() to retrieve information
 * about an exported symbol.
 */
typedef struct _dr_export_info_t {
    /**
     * The entry point of the export as an absolute address located
     * within the queried module.  This address is identical to what
     * dr_get_proc_address_ex() returns.
     */
    generic_func_t address;
    /**
     * Relevant for Linux only.  Set to true iff this export is an
     * indirect code object, which is a new ELF extension allowing
     * runtime selection of which implementation to use for an
     * exported symbol.  The address of such an export is a function
     * that takes no arguments and returns the address of the selected
     * implementation.
     */
    bool is_indirect_code;
} dr_export_info_t;
/* DR_API EXPORT END */

DR_API
/**
 * Returns information in \p info about the symbol \p name exported
 * by the module \p lib.  Returns false if the symbol is not found.
 * See the information in dr_get_proc_address() about what an
 * "exported" function is on Linux.
 *
 * \note On Linux this ignores symbol preemption by other modules and only
 * examines the specified module.
 */
bool
dr_get_proc_address_ex(module_handle_t lib, const char *name, dr_export_info_t *info OUT,
                       size_t info_len);

/* DR_API EXPORT BEGIN */
/** Flags for use with dr_map_executable_file(). */
typedef enum {
    /**
     * Requests that writable segments are not mapped, to save address space.
     * This may be ignored on some platforms and may only be honored for
     * a writable segment that is at the very end of the loaded module.
     */
    DR_MAPEXE_SKIP_WRITABLE = 0x0002,
} dr_map_executable_flags_t;
/* DR_API EXPORT END */

DR_API
/**
 * Loads \p filename as an executable file for examination, rather
 * than for execution.  No entry point, initialization, or constructor
 * code is executed, nor is any thread-local storage or other
 * resources set up.  Returns the size (which may include unmappped
 * gaps) in \p size.  The return value of the function is the base
 * address at which the file is mapped.
 *
 * \note Not currently supported on Mac OSX.
 */
byte *
dr_map_executable_file(const char *filename, dr_map_executable_flags_t flags,
                       size_t *size OUT);

DR_API
/**
 * Unmaps a file loaded by dr_map_executable_file().
 */
bool
dr_unmap_executable_file(byte *base, size_t size);

/* DR_API EXPORT BEGIN */
/**************************************************
 * SYSTEM CALL PROCESSING ROUTINES
 */

/**
 * Data structure used to obtain or modify the result of an application
 * system call by dr_syscall_get_result_ex() and dr_syscall_set_result_ex().
 */
typedef struct _dr_syscall_result_info_t {
    /** The caller should set this to the size of the structure. */
    size_t size;
    /**
     * Indicates whether the system call succeeded or failed.  For
     * dr_syscall_set_result_ex(), this requests that DR set any
     * additional machine state, if any, used by the particular
     * plaform that is not part of \p value to indicate success or
     * failure (e.g., on MacOS the carry flag is used to indicate
     * success).
     *
     * For Windows, the success result from dr_syscall_get_result_ex()
     * should only be relied upon for ntoskrnl system calls.  For
     * other Windows system calls (such as win32k.sys graphical
     * (NtGdi) or user (NtUser) system calls), computing success
     * depends on each particular call semantics and is beyond the
     * scope of this routine (consider using the "drsyscall" Extension
     * instead).
     *
     * For Mach syscalls on MacOS, the success result from
     * dr_syscall_get_result_ex() should not be relied upon.
     * Computing success depends on each particular call semantics and
     * is beyond the scope of this routine (consider using the
     * "drsyscall" Extension instead).
     */
    bool succeeded;
    /**
     * The raw main value returned by the system call.
     * See also the \p high field.
     */
    reg_t value;
    /**
     * On some platforms (such as MacOS), a 32-bit application's
     * system call can return a 64-bit value.  For such calls,
     * this field will hold the top 32 bit bits, if requested
     * by \p use_high.  It is up to the caller to know which
     * system calls have 64-bit return values.  System calls that
     * return only 32-bit values do not clear the upper bits.
     * Consider using the "drsyscall" Extension in order to obtain
     * per-system-call semantic information, including return type.
     */
    reg_t high;
    /**
     * This should be set by the caller, and only applies to 32-bit
     * system calls.  For dr_syscall_get_result_ex(), this requests
     * that DR fill in the \p high field.  For
     * dr_syscall_set_result_ex(), this requests that DR set the high
     * 32 bits of the application-facing result to the value in the \p
     * high field.
     */
    bool use_high;
    /**
     * This should be set by the caller.  For dr_syscall_get_result_ex(),
     * this requests that DR fill in the \p errno_value field.
     * For dr_syscall_set_result_ex(), this requests that DR set the
     * \p value to indicate the particular error code in \p errno_value.
     */
    bool use_errno;
    /**
     * If requested by \p use_errno, if a system call fails (i.e., \p
     * succeeded is false) dr_syscall_get_result_ex() will set this
     * field to the absolute value of the error code returned (i.e.,
     * on Linux, it will be inverted from what the kernel directly
     * returns, in order to facilitate cross-platform clients that
     * operate on both Linux and MacOS).  For Linux and Macos, when
     * \p succeeded is true, \p errno_value is set to 0.
     *
     * If \p use_errno is set for dr_syscall_set_result_ex(), then
     * this value will be stored as the system call's return value,
     * negated if necessary for the underlying platform.  In that
     * case, \p value will be ignored.
     */
    uint errno_value;
} dr_syscall_result_info_t;
/* DR_API EXPORT END */

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event.  Returns the value of system call parameter number \p param_num.
 *
 * It is up to the caller to ensure that reading this parameter is
 * safe: this routine does not know the number of parameters for each
 * system call, nor does it check whether this might read off the base
 * of the stack.
 *
 * \note On some platforms, notably MacOS, a 32-bit application's
 * system call can still take a 64-bit parameter (typically on the
 * stack).  In that situation, this routine will consider the 64-bit
 * parameter to be split into high and low parts, each with its own
 * parameter number.
 */
reg_t
dr_syscall_get_param(void *drcontext, int param_num);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event, or from a post-syscall (dr_register_post_syscall_event())
 * event when also using dr_syscall_invoke_another().  Sets the value
 * of system call parameter number \p param_num to \p new_value.
 *
 * It is up to the caller to ensure that writing this parameter is
 * safe: this routine does not know the number of parameters for each
 * system call, nor does it check whether this might write beyond the
 * base of the stack.
 *
 * \note On some platforms, notably MacOS, a 32-bit application's
 * system call can still take a 64-bit parameter (typically on the
 * stack).  In that situation, this routine will consider the 64-bit
 * parameter to be split into high and low parts, each with its own
 * parameter number.
 */
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  Returns the return value of the system call that will be
 * presented to the application.
 *
 * \note On some platforms (such as MacOS), a 32-bit application's
 * system call can return a 64-bit value.  Use dr_syscall_get_result_ex()
 * to obtain the upper bits in that case.
 *
 * \note On some platforms (such as MacOS), whether a system call
 * succeeded or failed cannot be determined from the main result
 * value.  Use dr_syscall_get_result_ex() to obtain the success result
 * in such cases.
 */
reg_t
dr_syscall_get_result(void *drcontext);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  Returns whether it successfully retrieved the results
 * of the system call into \p info.
 *
 * The caller should set the \p size, \p use_high, and \p use_errno fields
 * of \p info prior to calling this routine.
 * See the fields of #dr_syscall_result_info_t for details.
 */
bool
dr_syscall_get_result_ex(void *drcontext, dr_syscall_result_info_t *info INOUT);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) or
 * post-syscall (dr_register_post_syscall_event()) event.
 * For pre-syscall, should only be used when skipping the system call.
 * This sets the return value of the system call that the application sees
 * to \p value.
 *
 * \note On MacOS, do not use this function as it fails to set the
 * carry flag and thus fails to properly indicate whether the system
 * call succeeded or failed: use dr_syscall_set_result_ex() instead.
 */
void
dr_syscall_set_result(void *drcontext, reg_t value);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) or
 * post-syscall (dr_register_post_syscall_event()) event.
 * For pre-syscall, should only be used when skipping the system call.
 *
 * This sets the returned results of the system call as specified in
 * \p info.  Returns whether it successfully did so.
 * See the fields of #dr_syscall_result_info_t for details.
 */
bool
dr_syscall_set_result_ex(void *drcontext, dr_syscall_result_info_t *info);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event, or from a post-syscall (dr_register_post_syscall_event())
 * event when also using dr_syscall_invoke_another().  Sets the system
 * call number of the system call about to be invoked to \p new_num.
 */
void
dr_syscall_set_sysnum(void *drcontext, int new_num);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  An additional system call will be invoked immediately,
 * using the current values of the parameters, which can be set with
 * dr_syscall_set_param().  The system call to be invoked should be
 * specified with dr_syscall_set_sysnum().
 *
 * Use this routine with caution.  Especially on Windows, care must be
 * taken if the application is expected to continue afterward.  When
 * system call parameters are stored on the stack, modifying them can
 * result in incorrect application behavior, particularly when setting
 * more parameters than were present in the original system call,
 * which will result in corruption of the application stack.
 *
 * On Windows, when the first system call is interruptible
 * (alertable), the additional system call may be delayed.
 *
 * DR will set key registers such as r10 for 64-bit or xdx for
 * sysenter or WOW64 system calls.  However, DR will not set ecx for
 * WOW64; that is up to the client.
 */
void
dr_syscall_invoke_another(void *drcontext);

#    ifdef WINDOWS
DR_API
/**
 * Must be invoked from dr_client_main().  Requests that the named ntoskrnl
 * system call be intercepted even when threads are native (e.g., due
 * to #DR_EMIT_GO_NATIVE).  Only a limited number of system calls
 * being intercepted while native are supported.  This routine will
 * fail once that limit is reached.
 *
 * @param[in] name      The system call name.  The name must match an exported
 *   system call wrapper in \p ntdll.dll.
 * @param[in] sysnum    The system call number (the value placed in the eax register).
 * @param[in] num_args  The number of arguments to the system call.
 * @param[in] wow64_index  The value placed in the ecx register when this system
 *   call is executed in a WOW64 process.  This value should be obtainable
 *   by examining the system call wrapper.
 *
 * \note Windows only.
 */
bool
dr_syscall_intercept_natively(const char *name, int sysnum, int num_args,
                              int wow64_index);
#    endif

/* DR_API EXPORT BEGIN */
/**************************************************
 * PLATFORM-INDEPENDENT FILE SUPPORT
 *
 * Since a FILE cannot be used outside of the DLL it was created in,
 * we have to use HANDLE on Windows.
 * We hide the distinction behind the file_t type
 */
/* DR_API EXPORT END */

DR_API
/**
 * Creates a new directory.  Fails if the directory already exists
 * or if it can't be created.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_create_dir(const char *fname);

DR_API
/**
 * Deletes the given directory.  Fails if the directory is not empty.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_delete_dir(const char *fname);

DR_API
/**
 * Returns the current directory for this process in \p buf.
 * On Windows, reading the current directory is considered unsafe
 * except during initialization, as it is stored in user memory and
 * access is not controlled via any standard synchronization.
 */
bool
dr_get_current_directory(char *buf, size_t bufsz);

DR_API
/**
 * Checks for the existence of a directory.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_directory_exists(const char *fname);

DR_API
/**
 * Checks the existence of a file.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_file_exists(const char *fname);

/* The extra BEGIN END is to get spacing nice. */
/* DR_API EXPORT BEGIN */
/* DR_API EXPORT END */
/* DR_API EXPORT BEGIN */
/* flags for use with dr_open_file() */
/** Open with read access. */
#    define DR_FILE_READ 0x1
/** Open with write access, but do not open if the file already exists. */
#    define DR_FILE_WRITE_REQUIRE_NEW 0x2
/**
 * Open with write access.  If the file already exists, set the file position to the
 * end of the file.
 */
#    define DR_FILE_WRITE_APPEND 0x4
/**
 * Open with write access.  If the file already exists, truncate the
 * file to zero length.
 */
#    define DR_FILE_WRITE_OVERWRITE 0x8
/**
 * Open with large (>2GB) file support.  Only applicable on 32-bit Linux.
 * \note DR's log files and tracedump files are all created with this flag.
 */
#    define DR_FILE_ALLOW_LARGE 0x10
/** Linux-only.  This file will be closed in the child of a fork. */
#    define DR_FILE_CLOSE_ON_FORK 0x20
/**
 * Open with write-only access.  Meant for use with pipes.  Linux-only.
 * Mutually exclusive with DR_FILE_WRITE_REQUIRE_NEW, DR_FILE_WRITE_APPEND, and
 * DR_FILE_WRITE_OVERWRITE.
 */
#    define DR_FILE_WRITE_ONLY 0x40
/* DR_API EXPORT END */

DR_API
/**
 * Opens the file \p fname. If no such file exists then one is created.
 * The file access mode is set by the \p mode_flags argument which is drawn from
 * the DR_FILE_* defines ORed together.  Returns INVALID_FILE if unsuccessful.
 *
 * On Windows, \p fname is safest as an absolute path (when using Windows system
 * calls directly there is no such thing as a relative path).  A relative path
 * passed to this routine will be converted to absolute on a best-effort basis
 * using the current directory that was set at process initialization time.
 * (The most recently set current directory can be retrieved (albeit with no
 * safety guarantees) with dr_get_current_directory().)  Drive-implied-absolute
 * paths ("\foo.txt") and other-drive-relative paths ("c:foo.txt") are not
 * supported.
 *
 * On Linux, the file descriptor will be marked as close-on-exec.  The
 * DR_FILE_CLOSE_ON_FORK flag can be used to automatically close a
 * file on a fork.
 *
 * \note No more then one write mode flag can be specified.
 *
 * \note On Linux, DR hides files opened by clients from the
 * application by using file descriptors that are separate from the
 * application's and preventing the application from closing
 * client-opened files.
 */
file_t
dr_open_file(const char *fname, uint mode_flags);

DR_API
/** Closes file \p f. */
void
dr_close_file(file_t f);

DR_API
/**
 * Renames the file \p src to \p dst, replacing an existing file named \p dst if
 * \p replace is true.
 * Atomic if \p src and \p dst are on the same filesystem.
 * Returns true if successful.
 */
bool
dr_rename_file(const char *src, const char *dst, bool replace);

DR_API
/**
 * Deletes the file referred to by \p filename.
 * Returns true if successful.
 * On both Linux and Windows, if filename refers to a symlink, the symlink will
 * be deleted and not the target of the symlink.
 * On Windows, this will fail to delete any file that was not opened with
 * FILE_SHARE_DELETE and is still open.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_delete_file(const char *filename);

DR_API
/** Flushes any buffers for file \p f. */
void
dr_flush_file(file_t f);

DR_API
/**
 * Writes \p count bytes from \p buf to file \p f.
 * Returns the actual number written.
 */
ssize_t
dr_write_file(file_t f, const void *buf, size_t count);

DR_API
/**
 * Reads up to \p count bytes from file \p f into \p buf.
 * Returns the actual number read.
 */
ssize_t
dr_read_file(file_t f, void *buf, size_t count);

/* NOTE - keep in synch with OS_SEEK_* in os_shared.h and SEEK_* from Linux headers.
 * The extra BEGIN END is to get spacing nice. Once we have more control over the
 * layout of the API header files share with os_shared.h. */
/* DR_API EXPORT BEGIN */
/* DR_API EXPORT END */
/* DR_API EXPORT BEGIN */
/* For use with dr_file_seek(), specifies the origin at which to apply the offset. */
#    define DR_SEEK_SET 0 /**< start of file */
#    define DR_SEEK_CUR 1 /**< current file position */
#    define DR_SEEK_END 2 /**< end of file */
/* DR_API EXPORT END */

DR_API
/**
 * Sets the current file position for file \p f to \p offset bytes
 * from the specified origin, where \p origin is one of the DR_SEEK_*
 * values.  Returns true if successful.
 */
bool
dr_file_seek(file_t f, int64 offset, int origin);

DR_API
/**
 * Returns the current position for the file \p f in bytes from the start of the file.
 * Returns -1 on an error.
 */
int64
dr_file_tell(file_t f);

DR_API
/**
 * Returns a new copy of the file handle \p f.
 * Returns INVALID_FILE on error.
 */
file_t
dr_dup_file_handle(file_t f);

DR_API
/**
 * Determines the size of the file \p fd.
 * On success, returns the size in \p size.
 * \return whether successful.
 */
bool
dr_file_size(file_t fd, OUT uint64 *size);

/* The extra BEGIN END is to get spacing nice. */
/* DR_API EXPORT BEGIN */
/* DR_API EXPORT END */
/* DR_API EXPORT BEGIN */
/* flags for use with dr_map_file() */
enum {
    /**
     * If set, changes to mapped memory are private to the mapping process and
     * are not reflected in the underlying file.  If not set, changes are visible
     * to other processes that map the same file, and will be propagated
     * to the file itself.
     */
    DR_MAP_PRIVATE = 0x0001,
#    ifdef UNIX
    /**
     * If set, indicates that the passed-in start address is required rather than a
     * hint.  On Linux, this has the same semantics as mmap with MAP_FIXED: i.e.,
     * any existing mapping in [addr,addr+size) will be unmapped.  This flag is not
     * supported on Windows.
     */
    DR_MAP_FIXED = 0x0002,
#    endif
#    ifdef WINDOWS
    /**
     * If set, loads the specified file as an executable image, rather than a data
     * file.  This flag is not supported on Linux.
     */
    DR_MAP_IMAGE = 0x0004,
#    endif
    /**
     * If set, loads the specified file at a location that is reachable from
     * the code cache and client libraries by a 32-bit displacement.  If not
     * set, the mapped file is not guaranteed to be reachable from the cache.
     */
    DR_MAP_CACHE_REACHABLE = 0x0008,
};
/* DR_API EXPORT END */

DR_API
/**
 * Memory-maps \p size bytes starting at offset \p offs from the file \p f
 * at address \p addr with privileges \p prot.
 *
 * @param[in] f The file to map.
 * @param[in,out] size The requested size to map.  Upon successful return,
 *   contains the actual mapped size.
 * @param[in] offs The offset within the file at which to start the map.
 * @param[in] addr The requested start address of the map.  Unless \p fixed
 *   is true, this is just a hint and may not be honored.
 * @param[in] prot The access privileges of the mapping, composed of
 *   the DR_MEMPROT_READ, DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * @param[in] flags Optional DR_MAP_* flags.
 *
 * \note Mapping image files for execution is not supported.
 *
 * \return the start address of the mapping, or NULL if unsuccessful.
 */
void *
dr_map_file(file_t f, INOUT size_t *size, uint64 offs, app_pc addr, uint prot,
            uint flags);

DR_API
/**
 * Unmaps a portion of a file mapping previously created by dr_map_file().
 * \return whether successful.
 *
 * @param[in]  map   The base address to be unmapped. Must be page size aligned.
 * @param[in]  size  The size to be unmapped.
 *                   All pages overlapping with the range are unmapped.
 *
 * \note On Windows, the whole file will be unmapped instead.
 *
 */
bool
dr_unmap_file(void *map, size_t size);

/* TODO add copy_file, truncate_file etc.
 * All should be easy though at some point should perhaps tell people to just use the raw
 * systemcalls, esp for linux where they're documented and let them provide their own
 * wrappers. */

/* DR_API EXPORT BEGIN */

/**************************************************
 * PRINTING
 */
/* DR_API EXPORT END */

DR_API
/**
 * Writes to DR's log file for the thread with drcontext \p drcontext if the current
 * loglevel is >= \p level and the current \p logmask & \p mask != 0.
 * The mask constants are the DR_LOG_* defines below.
 * Logging is disabled for the release build.
 * If \p drcontext is NULL, writes to the main log file.
 */
void
dr_log(void *drcontext, uint mask, uint level, const char *fmt, ...);

/* hack to get these constants in the right place in dr_ir_api.h */

/* DR_API EXPORT BEGIN */

/* The log mask constants */
#    define DR_LOG_NONE 0x00000000     /**< Log no data. */
#    define DR_LOG_STATS 0x00000001    /**< Log per-thread and global statistics. */
#    define DR_LOG_TOP 0x00000002      /**< Log top-level information. */
#    define DR_LOG_THREADS 0x00000004  /**< Log data related to threads. */
#    define DR_LOG_SYSCALLS 0x00000008 /**< Log data related to system calls. */
#    define DR_LOG_ASYNCH 0x00000010   /**< Log data related to signals/callbacks/etc. */
#    define DR_LOG_INTERP 0x00000020   /**< Log data related to app interpretation. */
#    define DR_LOG_EMIT 0x00000040     /**< Log data related to emitting code. */
#    define DR_LOG_LINKS 0x00000080    /**< Log data related to linking code. */
#    define DR_LOG_CACHE                                           \
        0x00000100 /**< Log data related to code cache management. \
                    */
#    define DR_LOG_FRAGMENT                                     \
        0x00000200 /**< Log data related to app code fragments. \
                    */
#    define DR_LOG_DISPATCH                                                           \
        0x00000400                    /**< Log data on every context switch dispatch. \
                                       */
#    define DR_LOG_MONITOR 0x00000800 /**< Log data related to trace building. */
#    define DR_LOG_HEAP 0x00001000    /**< Log data related to memory management. */
#    define DR_LOG_VMAREAS 0x00002000 /**< Log data related to address space regions. */
#    define DR_LOG_SYNCH 0x00004000   /**< Log data related to synchronization. */
#    define DR_LOG_MEMSTATS                                                            \
        0x00008000                         /**< Log data related to memory statistics. \
                                            */
#    define DR_LOG_OPTS 0x00010000         /**< Log data related to optimizations. */
#    define DR_LOG_SIDELINE 0x00020000     /**< Log data related to sideline threads. */
#    define DR_LOG_SYMBOLS 0x00040000      /**< Log data related to app symbols. */
#    define DR_LOG_RCT 0x00080000          /**< Log data related to indirect transfers. */
#    define DR_LOG_NT 0x00100000           /**< Log data related to Windows Native API. */
#    define DR_LOG_HOT_PATCHING 0x00200000 /**< Log data related to hot patching. */
#    define DR_LOG_HTABLE 0x00400000       /**< Log data related to hash tables. */
#    define DR_LOG_MODULEDB 0x00800000 /**< Log data related to the module database. */
#    define DR_LOG_ALL 0x00ffffff      /**< Log all data. */
#    ifdef DR_LOG_DEFINE_COMPATIBILITY
#        define LOG_NONE DR_LOG_NONE         /**< Identical to #DR_LOG_NONE. */
#        define LOG_STATS DR_LOG_STATS       /**< Identical to #DR_LOG_STATS. */
#        define LOG_TOP DR_LOG_TOP           /**< Identical to #DR_LOG_TOP. */
#        define LOG_THREADS DR_LOG_THREADS   /**< Identical to #DR_LOG_THREADS. */
#        define LOG_SYSCALLS DR_LOG_SYSCALLS /**< Identical to #DR_LOG_SYSCALLS. */
#        define LOG_ASYNCH DR_LOG_ASYNCH     /**< Identical to #DR_LOG_ASYNCH. */
#        define LOG_INTERP DR_LOG_INTERP     /**< Identical to #DR_LOG_INTERP. */
#        define LOG_EMIT DR_LOG_EMIT         /**< Identical to #DR_LOG_EMIT. */
#        define LOG_LINKS DR_LOG_LINKS       /**< Identical to #DR_LOG_LINKS. */
#        define LOG_CACHE DR_LOG_CACHE       /**< Identical to #DR_LOG_CACHE. */
#        define LOG_FRAGMENT DR_LOG_FRAGMENT /**< Identical to #DR_LOG_FRAGMENT. */
#        define LOG_DISPATCH DR_LOG_DISPATCH /**< Identical to #DR_LOG_DISPATCH. */
#        define LOG_MONITOR DR_LOG_MONITOR   /**< Identical to #DR_LOG_MONITOR. */
#        define LOG_HEAP DR_LOG_HEAP         /**< Identical to #DR_LOG_HEAP. */
#        define LOG_VMAREAS DR_LOG_VMAREAS   /**< Identical to #DR_LOG_VMAREAS. */
#        define LOG_SYNCH DR_LOG_SYNCH       /**< Identical to #DR_LOG_SYNCH. */
#        define LOG_MEMSTATS DR_LOG_MEMSTATS /**< Identical to #DR_LOG_MEMSTATS. */
#        define LOG_OPTS DR_LOG_OPTS         /**< Identical to #DR_LOG_OPTS. */
#        define LOG_SIDELINE DR_LOG_SIDELINE /**< Identical to #DR_LOG_SIDELINE. */
#        define LOG_SYMBOLS DR_LOG_SYMBOLS   /**< Identical to #DR_LOG_SYMBOLS. */
#        define LOG_RCT DR_LOG_RCT           /**< Identical to #DR_LOG_RCT. */
#        define LOG_NT DR_LOG_NT             /**< Identical to #DR_LOG_NT. */
#        define LOG_HOT_PATCHING \
            DR_LOG_HOT_PATCHING              /**< Identical to #DR_LOG_HOT_PATCHING. */
#        define LOG_HTABLE DR_LOG_HTABLE     /**< Identical to #DR_LOG_HTABLE. */
#        define LOG_MODULEDB DR_LOG_MODULEDB /**< Identical to #DR_LOG_MODULEDB. */
#        define LOG_ALL DR_LOG_ALL           /**< Identical to #DR_LOG_ALL. */
#    endif

/* DR_API EXPORT END */

DR_API
/**
 * Returns the log file for the thread with drcontext \p drcontext.
 * If \p drcontext is NULL, returns the main log file.
 */
file_t
dr_get_logfile(void *drcontext);

DR_API
/**
 * Returns true iff the -stderr_mask runtime option is non-zero, indicating
 * that the user wants notification messages printed to stderr.
 */
bool
dr_is_notify_on(void);

DR_API
/** Returns a handle to stdout. */
file_t
dr_get_stdout_file(void);

DR_API
/** Returns a handle to stderr. */
file_t
dr_get_stderr_file(void);

DR_API
/** Returns a handle to stdin. */
file_t
dr_get_stdin_file(void);

#    ifdef PROGRAM_SHEPHERDING
DR_API
/** Writes a security violation forensics report to the supplied file. The forensics
 *  report will include detailed information about the source and target addresses of the
 *  violation as well as information on the current thread, process, and machine.  The
 *  forensics report is generated in an xml block described by dr_forensics-1.0.dtd.
 *  The encoding used is iso-8859-1.
 *
 *  The dcontext, violation, and action arguments are supplied by the security violation
 *  event callback.  The file argument is the file to write the forensics report to and
 *  the violation_name argument is a supplied name for the violation.
 */
void
dr_write_forensics_report(void *dcontext, file_t file,
                          dr_security_violation_type_t violation,
                          dr_security_violation_action_t action,
                          const char *violation_name);
#    endif /* PROGRAM_SHEPHERDING */

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Displays a message in a pop-up window. \note Windows only. \note On Windows Vista
 * most Windows services are unable to display message boxes.
 */
void
dr_messagebox(const char *fmt, ...);
/* DR_API EXPORT BEGIN */
#    endif
/* DR_API EXPORT END */

DR_API
/**
 * Stdout printing that won't interfere with the
 * application's own printing.
 * It is not buffered, which means that it should not be used for
 * very frequent, small print amounts: for that the client should either
 * do its own buffering or it should use printf from the C library
 * via DR's private loader.
 * \note On Windows 7 and earlier, this routine is not able to print
 * to the \p cmd window
 * unless dr_enable_console_printing() is called ahead of time, and
 * even then there are limitations: see dr_enable_console_printing().
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.  Use dr_snprintf() and dr_write_file() for
 * large output.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state.
 */
void
dr_printf(const char *fmt, ...);

DR_API
/**
 * Printing to a file that won't interfere with the
 * application's own printing.
 * It is not buffered, which means that it should not be used for
 * very frequent, small print amounts: for that the client should either
 * do its own buffering or it should use printf from the C library
 * via DR's private loader.
 * \note On Windows 7 and earlier, this routine is not able to print to STDOUT or
 * STDERR in the \p cmd window unless dr_enable_console_printing() is
 * called ahead of time, and even then there are limitations: see
 * dr_enable_console_printing().
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.  Use dr_snprintf() and dr_write_file() for
 * large output.
 * \note On Linux this routine does not check for errors like EINTR.  Use
 * dr_write_file() if that is a concern.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state.
 * On success, the number of bytes written is returned.
 * On error, -1 is returned.
 */
ssize_t
dr_fprintf(file_t f, const char *fmt, ...);

DR_API
/**
 * Identical to dr_fprintf() but exposes va_list.
 */
ssize_t
dr_vfprintf(file_t f, const char *fmt, va_list ap);

#    ifdef WINDOWS
DR_API
/**
 * Enables dr_printf() and dr_fprintf() to work with a legacy console
 * window (viz., \p cmd on Windows 7 or earlier).  Loads a private
 * copy of kernel32.dll (if not
 * already loaded) in order to accomplish this.  To keep the default
 * DR lean and mean, loading kernel32.dll is not performed by default.
 *
 * This routine must be called during client initialization (\p dr_client_main()).
 * If called later, it will fail.
 *
 * Without calling this routine, dr_printf() and dr_fprintf() will not
 * print anything in a console window on Windows 7 or earlier, nor will they
 * print anything when running a graphical application.
 *
 * Even after calling this routine, there are significant limitations
 * to console printing support in DR:
 *
 *  - On Windows versions prior to Vista, and for WOW64 applications
 *    on Vista, it does not work from the exit event.  Once the
 *    application terminates its state with csrss (toward the very end
 *    of ExitProcess), no output will show up on the console.  We have
 *    no good solution here yet as exiting early is not ideal.
 *  - In the future, with earliest injection (Issue 234), writing to the
 *    console may not work from the client init event on Windows 7 and
 *    earlier (it will work on Windows 8).
 *
 * These limitations stem from the complex arrangement of the console
 * window in Windows (prior to Windows 8), where printing to it
 * involves sending a message
 * in an undocumented format to the \p csrss process, rather than a
 * simple write to a file handle.  We recommend using a terminal
 * window such as cygwin's \p rxvt rather than the \p cmd window, or
 * alternatively redirecting all output to a file, which will solve
 * all of the above limitations.
 *
 * Returns whether successful.
 * \note Windows only.
 */
bool
dr_enable_console_printing(void);

DR_API
/**
 * Returns true if the current standard error handle belongs to a
 * legacy console window (viz., \p cmd on Windows 7 or earlier).  DR's
 * dr_printf() and dr_fprintf()
 * do not work with such console windows unless
 * dr_enable_console_printing() is called ahead of time, and even then
 * there are limitations detailed in dr_enable_console_printing().
 * This routine may result in loading a private copy of kernel32.dll.
 * \note Windows only.
 */
bool
dr_using_console(void);
#    endif

DR_API
/**
 * Utility routine to print a formatted message to a string.  Will not
 * print more than max characters.  If successful, returns the number
 * of characters printed, not including the terminating null
 * character.  If the number of characters to write equals max, then
 * the caller is responsible for supplying a terminating null
 * character.  If the number of characters to write exceeds max, then
 * max characters are written and -1 is returned.  If an error
 * occurs, a negative value is returned.
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state.
 */
int
dr_snprintf(char *buf, size_t max, const char *fmt, ...);

DR_API
/**
 * Wide character version of dr_snprintf().  All of the comments for
 * dr_snprintf() apply, except for the hs or S format specifiers.
 * On Windows, these will assume that the input is UTF-8, and will
 * convert to UTF-16.  On Linux, they will widen a single-byte
 * character string into a wchar_t character string with zero as the
 * high-order bytes.
 */
int
dr_snwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, ...);

DR_API
/**
 * Identical to dr_snprintf() but exposes va_list.
 */
int
dr_vsnprintf(char *buf, size_t max, const char *fmt, va_list ap);

DR_API
/**
 * Identical to dr_snwprintf() but exposes va_list.
 */
int
dr_vsnwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, va_list ap);

DR_API
/**
 * Utility routine to parse strings that match a pre-defined format string,
 * similar to the sscanf() C routine.
 *
 * @param[in] str   String to parse.
 * @param[in] fmt   Format string controlling parsing.
 * @param[out] ...  All remaining parameters interpreted as output parameter
 *                  pointers.  The type of each parameter must match the type
 *                  implied by the corresponding format specifier in \p fmt.
 * \return The number of specifiers matched.
 *
 * The benefit of using dr_sscanf() over native sscanf() is that DR's
 * implementation is standalone, signal-safe, and cross-platform.  On Linux,
 * sscanf() has been observed to call malloc().  On Windows, sscanf() will call
 * strlen(), which can break when using mapped files.
 *
 * The behavior of dr_sscanf() is mostly identical to that of the sscanf() C
 * routine.
 *
 * Supported format specifiers:
 * - \%s: Matches a sequence of non-whitespace characters.  The string is copied
 *   into the provided output buffer.  To avoid buffer overflow, the caller
 *   should use a width specifier.
 * - \%c: Matches any single character.
 * - \%d: Matches a signed decimal integer.
 * - \%u: Matches an unsigned decimal integer.
 * - \%x: Matches an unsigned hexadecimal integer, with or without a leading 0x.
 * - \%p: Matches a pointer-sized hexadecimal integer as %x does.
 * - \%%: Matches a literal % character.  Does not store output.
 *
 * Supported format modifiers:
 * - *: The * modifier causes the scan to match the specifier, but not store any
 *   output.  No output parameter is consumed for this specifier, and one should
 *   not be passed.
 * - 0-9: A decimal integer preceding the specifier gives the width to match.
 *   For strings, this indicates the maximum number of characters to copy.  For
 *   integers, this indicates the maximum number of digits to parse.
 * - h: Marks an integer specifier as short.
 * - l: Marks an integer specifier as long.
 * - ll: Marks an integer specifier as long long.  Use this for 64-bit integers.
 *
 * \warning dr_sscanf() does \em not support parsing floating point numbers yet.
 */
int
dr_sscanf(const char *str, const char *fmt, ...);

DR_API
/**
 * Utility function that aids in tokenizing a string, such as a client
 * options string from dr_get_options().  The function scans \p str
 * until a non-whitespace character is found.  It then starts copying
 * into \p buf until a whitespace character is found denoting the end
 * of the token.  If the token begins with a quote, the token
 * continues (including across whitespace) until the matching end
 * quote is found.  Characters considered whitespace are ' ', '\\t',
 * '\\r', and '\\n'; characters considered quotes are '\\'', '\\"', and
 * '`'.
 *
 * @param[in]  str     The start of the string containing the next token.
 * @param[out] buf     A buffer to store a null-terminated copy of the next token.
 * @param[in]  buflen  The capacity of the buffer, in characters.  If the token
 *                     is too large to fit, it will be truncated and null-terminated.
 *
 * \return a pointer to the end of the token in \p str.  Thus, to retrieve
 * the subsequent token, call this routine again with the prior return value
 * as the new value of \p str.  Returns NULL when the end of \p str is reached.
 */
const char *
dr_get_token(const char *str, char *buf, size_t buflen);

DR_API
/** Prints \p msg followed by the instruction \p instr to file \p f. */
void
dr_print_instr(void *drcontext, file_t f, instr_t *instr, const char *msg);

DR_API
/** Prints \p msg followed by the operand \p opnd to file \p f. */
void
dr_print_opnd(void *drcontext, file_t f, opnd_t opnd, const char *msg);

/* DR_API EXPORT BEGIN */

/**************************************************
 * THREAD SUPPORT
 */
/* DR_API EXPORT END */

DR_API
/**
 * Returns the DR context of the current thread.
 */
void *
dr_get_current_drcontext(void);

DR_API
/** Returns the thread id of the thread with drcontext \p drcontext. */
thread_id_t
dr_get_thread_id(void *drcontext);

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Returns a Windows handle to the thread with drcontext \p drcontext.
 * This handle is DR's handle to this thread (it is not a separate
 * copy) and as such it should not be closed by the caller; nor should
 * it be used beyond the thread's exit, as DR's handle will be closed
 * at that point.
 *
 * The handle should have THREAD_ALL_ACCESS privileges.
 * \note Windows only.
 */
HANDLE
dr_get_dr_thread_handle(void *drcontext);
/* DR_API EXPORT BEGIN */
#    endif
/* DR_API EXPORT END */

DR_API
/**
 * Returns the user-controlled thread-local-storage field.  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use dr_insert_read_tls_field().
 */
void *
dr_get_tls_field(void *drcontext);

DR_API
/**
 * Sets the user-controlled thread-local-storage field.  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use dr_insert_write_tls_field().
 */
void
dr_set_tls_field(void *drcontext, void *value);

DR_API
/**
 * Get DR's thread local storage segment base pointed at by \p tls_register.
 * It can be used to get the base of the thread-local storage segment
 * used by #dr_raw_tls_calloc.
 *
 * \note It should not be called on thread exit event,
 * as the thread exit event may be invoked from other threads.
 * See #dr_register_thread_exit_event for details.
 */
void *
dr_get_dr_segment_base(IN reg_id_t tls_register);

DR_API
/**
 * Allocates \p num_slots contiguous thread-local storage (TLS) slots that
 * can be directly accessed via an offset from \p tls_register.
 * If \p alignment is non-zero, the slots will be aligned to \p alignment.
 * These slots will be initialized to 0 for each new thread.
 * The slot offsets are [\p offset .. \p offset + (num_slots - 1)].
 * These slots are disjoint from the #dr_spill_slot_t register spill slots
 * and the client tls field (dr_get_tls_field()).
 * Returns whether or not the slots were successfully obtained.
 * The linear address of the TLS base pointed at by \p tls_register can be obtained
 * using #dr_get_dr_segment_base.
 * Raw TLs slots can be read directly using dr_insert_read_raw_tls() and written
 * using dr_insert_write_raw_tls().
 *
 * Supports passing 0 for \p num_slots, in which case \p tls_register will be
 * written but no other action taken.
 *
 * \note These slots are useful for thread-shared code caches.  With
 * thread-private caches, DR's memory pools are guaranteed to be
 * reachable via absolute or rip-relative accesses from the code cache
 * and client libraries.
 *
 * \note These slots are a limited resource.  On Windows the slots are
 * shared with the application and reserving even one slot can result
 * in failure to initialize for certain applications.  On Linux they
 * are more plentiful and transparent but currently DR limits clients
 * to no more than 64 slots.
 *
 * \note On Mac OS, TLS slots may not be initialized to zero.
 */
bool
dr_raw_tls_calloc(OUT reg_id_t *tls_register, OUT uint *offset, IN uint num_slots,
                  IN uint alignment);

DR_API
/**
 * Frees \p num_slots raw thread-local storage slots starting at
 * offset \p offset that were allocated with dr_raw_tls_calloc().
 * Returns whether or not the slots were successfully freed.
 */
bool
dr_raw_tls_cfree(uint offset, uint num_slots);

DR_API
/**
 * Returns an operand that refers to the raw TLS slot with offset \p
 * tls_offs from the TLS base \p tls_register.
 */
opnd_t
dr_raw_tls_opnd(void *drcontext, reg_id_t tls_register, uint tls_offs);

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to read into the
 * general-purpose full-size register \p reg from the raw TLS slot with offset
 * \p tls_offs from the TLS base \p tls_register.
 */
void
dr_insert_read_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t tls_register, uint tls_offs, reg_id_t reg);

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to store the value in the
 * general-purpose full-size register \p reg into the raw TLS slot with offset
 * \p tls_offs from the TLS base \p tls_register.
 */
void
dr_insert_write_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t tls_register, uint tls_offs, reg_id_t reg);

/* PR 222812: due to issues in supporting client thread synchronization
 * and other complexities we are using nudges for simple push-i/o and
 * saving thread creation for sideline usage scenarios.
 * These are implemented in <os>/os.c.
 */
/* PR 231301: for synch with client threads we can't distinguish between
 * client_lib->ntdll/gencode/other_lib (which is safe) from
 * client_lib->dr->ntdll/gencode/other_lib (which isn't) so we consider both
 * unsafe.  If the client thread spends a lot of time in ntdll or worse directly
 * makes blocking/long running system calls (note dr_thread_yield, dr_sleep,
 * dr_mutex_lock, and dr_messagebox are ok) then it may have performance or
 * correctness (if the synch times out) impacts.
 */
#    ifdef CLIENT_SIDELINE
DR_API
/**
 * Creates a new thread that is marked as a non-application thread (i.e., DR
 * will let it run natively and not execute its code from the code cache).  The
 * thread will terminate automatically simply by returning from \p func; if
 * running when the application terminates its last thread, the client thread
 * will also terminate when DR shuts the process down.
 *
 * Init and exit events will not be raised for this thread (instead simply place
 * init and exit code in \p func).
 *
 * The new client thread has a drcontext that can be used for thread-private
 * heap allocations.  It has a stack of the same size as the DR stack used by
 * application threads.
 *
 * On Linux, this thread is guaranteed to have its own private itimer
 * if dr_set_itimer() is called from it.  However this does mean it
 * will have its own process id.
 *
 * A client thread should refrain from spending most of its time in
 * calls to other libraries or making blocking or long-running system
 * calls as such actions may incur performance or correctness problems
 * with DR's synchronization engine, which needs to be able to suspend
 * client threads at safe points and cannot determine whether the
 * aforementioned actions are safe for suspension.  Calling
 * dr_sleep(), dr_thread_yield(), dr_messagebox(), or using DR's locks
 * are safe.  If a client thread spends a lot of time holding locks,
 * consider marking it as un-suspendable by calling
 * dr_client_thread_set_suspendable() for better performance.
 *
 * Client threads, whether suspendable or not, must never execute from
 * the code cache as the underlying fragments might be removed by another
 * thread.
 *
 * Client threads are suspended while DR is not executing the application.
 * This includes initialization time: the client thread's \p func code will not
 * execute until DR starts executing the application.
 *
 * \note Thread creation via this routine is not yet fully
 * transparent: on Windows, the thread will show up in the list of
 * application threads if the operating system is queried about
 * threads.  The thread will not trigger a DLL_THREAD_ATTACH message.
 * On Linux, the thread will not receive signals meant for the application,
 * and is guaranteed to have a private itimer.
 */
bool
dr_create_client_thread(void (*func)(void *param), void *arg);

DR_API
/**
 * Can only be called from a client thread: returns false if called
 * from a non-client thread.
 *
 * Controls whether a client thread created with dr_create_client_thread()
 * will be suspended by DR for synchronization operations such as
 * flushing or client requests like dr_suspend_all_other_threads().
 * A client thread that spends a lot of time holding locks can gain
 * greater performance by not being suspended.
 *
 * A client thread \b will be suspended for a thread termination
 * operation, including at process exit, regardless of its suspendable
 * requests.
 */
bool
dr_client_thread_set_suspendable(bool suspendable);
#    endif /* CLIENT_SIDELINE */

DR_API
/** Current thread sleeps for \p time_ms milliseconds. */
void
dr_sleep(int time_ms);

DR_API
/** Current thread gives up its time quantum. */
void
dr_thread_yield(void);

/* DR_API EXPORT BEGIN */
/** Flags controlling the behavior of dr_suspend_all_other_threads_ex(). */
typedef enum {
    /**
     * By default, native threads are not suspended by
     * dr_suspend_all_other_threads_ex().  This flag requests that native
     * threads (including those temporarily-native due to actions such as
     * #DR_EMIT_GO_NATIVE) be suspended as well.
     */
    DR_SUSPEND_NATIVE = 0x0001,
} dr_suspend_flags_t;
/* DR_API EXPORT END */

/* FIXME - xref PR 227619 - some other event handler are safe (image_load/unload for*
 * example) which we could note here. */
DR_API
/**
 * Suspends all other threads in the process and returns an array of
 * contexts in \p drcontexts with one context per successfully suspended
 * thread.  The contexts can be passed to routines like dr_get_thread_id()
 * or dr_get_mcontext().  However, the contexts may not be modified:
 * dr_set_mcontext() is not supported.  dr_get_mcontext() can be called on
 * the caller of this routine, unless in a Windows nudge callback.
 *
 * The \p flags argument controls which threads are suspended and may
 * add further options in the future.
 *
 * The number of successfully suspended threads, which is also the length
 * of the \p drcontexts array, is returned in \p num_suspended, which is a
 * required parameter.  The number of un-successfully suspended threads, if
 * any, is returned in the optional parameter \p num_unsuspended.  The
 * calling thread is not considered in either count.  DR can fail to
 * suspend a thread for privilege reasons (e.g., on Windows in a
 * low-privilege process where another process injected a thread).  This
 * function returns true iff all threads were suspended, in which case \p
 * num_unsuspended will be 0.
 *
 * The caller must invoke dr_resume_all_other_threads() in order to resume
 * the suspended threads, free the \p drcontexts array, and release
 * coarse-grain locks that prevent new threads from being created.
 *
 * This routine may not be called from any registered event callback
 * other than the nudge event or the pre- or post-system call event.
 * It may be called from clean calls out of the cache.
 * This routine may not be called while any locks are held that
 * could block a thread processing a registered event callback or cache
 * callout.
 *
 * \note A client wishing to invoke this routine from an event callback can
 * queue up a nudge via dr_nudge_client() and invoke this routine from the
 * nudge callback.
 */
bool
dr_suspend_all_other_threads_ex(OUT void ***drcontexts, OUT uint *num_suspended,
                                OUT uint *num_unsuspended, dr_suspend_flags_t flags);

DR_API
/** Identical to dr_suspend_all_other_threads_ex() with \p flags set to 0. */
bool
dr_suspend_all_other_threads(OUT void ***drcontexts, OUT uint *num_suspended,
                             OUT uint *num_unsuspended);

DR_API
/**
 * May only be used after invoking dr_suspend_all_other_threads().  This
 * routine resumes the threads that were suspended by
 * dr_suspend_all_other_threads() and must be passed the same array and
 * count of suspended threads that were returned by
 * dr_suspend_all_other_threads().  It also frees the \p drcontexts array
 * and releases the locks acquired by dr_suspend_all_other_threads().  The
 * return value indicates whether all resumption attempts were successful.
 */
bool
dr_resume_all_other_threads(IN void **drcontexts, IN uint num_suspended);

DR_API
/**
 * Returns whether the thread represented by \p drcontext is currently
 * executing natively (typically due to an earlier #DR_EMIT_GO_NATIVE
 * return value).
 */
bool
dr_is_thread_native(void *drcontext);

DR_API
/**
 * Causes the thread owning \p drcontext to begin executing in the
 * code cache again once it is resumed.  The thread must currently be
 * suspended (typically by dr_suspend_all_other_threads_ex() with
 * #DR_SUSPEND_NATIVE) and must be currently native (typically from
 * #DR_EMIT_GO_NATIVE).  \return whether successful.
 */
bool
dr_retakeover_suspended_native_thread(void *drcontext);

/* We do not translate the context to avoid lock issues (PR 205795).
 * We do not delay until a safe point (via regular delayable signal path)
 * since some clients may want the interrupted context: for a general
 * timer clients should create a separate thread.
 */
#    ifdef UNIX
DR_API
/**
 * Installs an interval timer in the itimer sharing group that
 * contains the calling thread.
 *
 * @param[in] which Must be one of ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 * @param[in] millisec The frequency of the timer, in milliseconds.  Passing
 *   0 disables the timer.
 * @param[in] func The function that will be called each time the
 *   timer fires.  It will be passed the context of the thread that
 *   received the itimer signal and its machine context, which has not
 *   been translated and so may contain raw code cache values.  The function
 *   will be called from a signal handler that may have interrupted a
 *   lock holder or other critical code, so it must be careful in its
 *   operations: keep it as simple as possible, and avoid any non-reentrant actions
 *   such as lock usage. If a general timer that does not interrupt client code
 *   is required, the client should create a separate thread via
 *   dr_create_client_thread() (which is guaranteed to have a private
 *   itimer) and set the itimer there, where the callback function can
 *   perform more operations safely if that new thread never acquires locks
 *   in its normal operation.
 *
 * Itimer sharing varies by kernel.  Prior to 2.6.12 itimers were
 * thread-private; after 2.6.12 they are shared across a thread group,
 * though there could be multiple thread groups in one address space.
 * The dr_get_itimer() function can be used to see whether a thread
 * already has an itimer in its group to avoid re-setting an itimer
 * set by an earlier thread.  A client thread created by
 * dr_create_client_thread() is guaranteed to not share its itimers
 * with application threads.
 *
 * The itimer will operate successfully in the presence of an
 * application itimer of the same type.
 *
 * Additional itimer signals are blocked while in our signal handler.
 *
 * The return value indicates whether the timer was successfully
 * installed (or uninstalled if 0 was passed for \p millisec).
 *
 * \note Linux-only.
 */
bool
dr_set_itimer(int which, uint millisec,
              void (*func)(void *drcontext, dr_mcontext_t *mcontext));

DR_API
/**
 * If an interval timer is already installed in the itimer sharing group that
 * contains the calling thread, returns its frequency.  Else returns 0.
 *
 * @param[in] which Must be one of ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 *
 * \note Linux-only.
 */
uint
dr_get_itimer(int which);
#    endif /* UNIX */

DR_API
/**
 * Should be called during process initialization.  Requests more accurate
 * tracking of the #dr_where_am_i_t value for use with dr_where_am_i().  By
 * default, if this routine is not called, DR avoids some updates to the value
 * that incur extra overhead, such as identifying clean callees.
 */
void
dr_track_where_am_i(void);

DR_API
/**
 * Returns whether DR is using accurate tracking of the #dr_where_am_i value.
 * Typically this is enabled by calling dr_track_where_am_i().
 */
bool
dr_is_tracking_where_am_i(void);

DR_API
/**
 * Returns the #dr_where_am_i_t value indicating in which area of code \p pc
 * resides.  This is meant for use with dr_set_itimer() for PC sampling for
 * profiling purposes.  If the optional \p tag is non-NULL and \p pc is inside a
 * fragment in the code cache, the fragment's tag is returned in \p tag.  It is
 * recommended that the user of this routine also call dr_track_where_am_i()
 * during process initialization for more accurate results.
 */
dr_where_am_i_t
dr_where_am_i(void *drcontext, app_pc pc, OUT void **tag);

/* DR_API EXPORT TOFILE dr_ir_utils.h */
/* DR_API EXPORT BEGIN */
#    ifdef API_EXPORT_ONLY
#        include "dr_ir_instr.h"
#    endif

/**************************************************
 * CODE TRANSFORMATION UTILITIES
 */
/**
 * @file dr_ir_utils.h
 * @brief Code transformation utilities.
 */

/**
 * An enum of spill slots to use with dr_save_reg(), dr_restore_reg(),
 * dr_save_arith_flags(), dr_restore_arith_flags() and
 * dr_insert_mbr_instrumentation().  Values stored in spill slots remain
 * valid only until the next non-meta (i.e. application) instruction.  Spill slots
 * can be accessed/modifed during clean calls and restore_state_events (see
 * dr_register_restore_state_event()) with dr_read_saved_reg() and
 * dr_write_saved_reg().
 *
 * Spill slots <= dr_max_opnd_accessible_spill_slot() can be directly accessed
 * from client inserted instructions with dr_reg_spill_slot_opnd().
 *
 * \note Some spill slots may be faster to access than others.  Currently spill
 * slots 1-3 are significantly faster to access than the others when running
 * without -thread_private.  When running with -thread_private all spill slots
 * are expected to have similar performance.  This is subject to change in future
 * releases, but clients may assume that smaller numbered spill slots are faster
 * or the same cost to access as larger numbered spill slots.
 *
 * \note The number of spill slots may change in future releases.
 */
typedef enum {
    SPILL_SLOT_1 = 0, /** spill slot for register save/restore routines */
    SPILL_SLOT_2 = 1, /** spill slot for register save/restore routines */
    SPILL_SLOT_3 = 2, /** spill slot for register save/restore routines */
    SPILL_SLOT_4 = 3, /** spill slot for register save/restore routines */
    SPILL_SLOT_5 = 4, /** spill slot for register save/restore routines */
    SPILL_SLOT_6 = 5, /** spill slot for register save/restore routines */
    SPILL_SLOT_7 = 6, /** spill slot for register save/restore routines */
    SPILL_SLOT_8 = 7, /** spill slot for register save/restore routines */
    SPILL_SLOT_9 = 8, /** spill slot for register save/restore routines */
#    ifdef X64
    SPILL_SLOT_10 = 9,             /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_11 = 10,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_12 = 11,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_13 = 12,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_14 = 13,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_15 = 14,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_16 = 15,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_17 = 16,            /** spill slot for register save/restore routines
                                    * \note x64 only */
    SPILL_SLOT_MAX = SPILL_SLOT_17 /** Enum value of the last register save/restore
                                    *  spill slot. */
#    else
    SPILL_SLOT_MAX = SPILL_SLOT_9 /** Enum value of the last register save/restore
                                   *  spill slot. */
#    endif
} dr_spill_slot_t;
/* DR_API EXPORT END */

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * register \p reg in the spill slot \p slot.  See dr_restore_reg(). Use
 * dr_read_saved_reg() and dr_write_saved_reg() to access spill slots from clean
 * calls and restore_state_events (see dr_register_restore_state_event()).
 * \note The stored value remains available only until the next non-meta (i.e.
 * application) instruction. Use dr_insert_write_tls_field() and
 * dr_insert_read_tls_field() for a persistent (but more costly to access)
 * thread-local-storage location.  See also dr_raw_tls_calloc().
 */
void
dr_save_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
            dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the
 * register \p reg from the spill slot \p slot.  See dr_save_reg() for notes on
 * lifetime and alternative access to spill slots.
 */
void
dr_restore_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
               dr_spill_slot_t slot);

DR_API
/**
 * Returns the largest dr_spill_slot_t that can be accessed with an opnd_t from
 * dr_reg_spill_slot_opnd().
 */
dr_spill_slot_t
dr_max_opnd_accessible_spill_slot(void);

DR_API
/**
 * Returns an opnd_t that directly accesses the spill slot \p slot. Only slots
 * <= dr_max_opnd_accessible_spill_slot() can be used with this routine.
 * \note \p slot must be <= dr_max_opnd_accessible_spill_slot()
 */
opnd_t
dr_reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot);

/* internal version */
opnd_t
reg_spill_slot_opnd(dcontext_t *dcontext, dr_spill_slot_t slot);

DR_API
/**
 * Can be used from a clean call or a restore_state_event (see
 * dr_register_restore_state_event()) to see the value saved in spill slot
 * \p slot by dr_save_reg().
 */
reg_t
dr_read_saved_reg(void *drcontext, dr_spill_slot_t slot);

DR_API
/**
 * Can be used from a clean call to modify the value saved in the spill slot
 * \p slot by dr_save_reg() such that a later dr_restore_reg() will see the
 * new value.
 *
 * \note This routine should only be used during a clean call out of the
 * cache.  Use at any other time could corrupt application or DynamoRIO
 * state.
 */
void
dr_write_saved_reg(void *drcontext, dr_spill_slot_t slot, reg_t value);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the 6
 * arithmetic flags into xax after first saving xax to the spill slot \p slot.
 * This is equivalent to dr_save_reg() of xax to \p slot followed by lahf and
 * seto al instructions.  See dr_restore_arith_flags().
 *
 * \warning At completion of the inserted instructions the saved flags are in the
 * xax register.  The xax register should not be modified after using this
 * routine unless it is first saved (and later restored prior to
 * using dr_restore_arith_flags()).
 *
 * \note X86-only
 *
 * \deprecated This routine is equivalent to dr_save_reg() followed by
 * dr_save_arith_flags_to_xax().
 */
void
dr_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                    dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the 6
 * arithmetic flags, assuming they were saved using dr_save_arith_flags() with
 * slot \p slot and that xax holds the same value it did after the save.
 *
 * \note X86-only
 *
 * \deprecated This routine is equivalent to dr_restore_arith_flags_from_xax()
 * followed by dr_restore_reg().
 */
void
dr_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                       dr_spill_slot_t slot);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the 6
 * arithmetic flags into xax.  This currently uses DynamoRIO's "lahf ; seto al"
 * code sequence, which is faster and easier than pushf.  If the caller wishes
 * to use xax between saving and restoring these flags, they must save and
 * restore xax, potentially using dr_save_reg()/dr_restore_reg().  If the caller
 * needs to save both the current value of xax and the flags stored to xax by
 * this routine, they must use separate spill slots, or they will overwrite the
 * original xax value in memory.
 * \note X86-only
 *
 * \warning Clobbers xax; the caller must ensure xax is dead or saved at
 * \p where.
 */
void
dr_save_arith_flags_to_xax(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the 6
 * arithmetic flags from xax.  This currently uses DynamoRIO's "add $0x7f %al ;
 * sahf" code sequence, which is faster and easier than popf.  The caller must
 * ensure that xax contains the arithmetic flags, most likely from
 * dr_save_arith_flags_to_xax().
 * \note X86-only
 */
void
dr_restore_arith_flags_from_xax(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * arithmetic flags (6 arithmetic flags on X86 or APSR on ARM) into \p reg.
 * If the caller wishes to use \p reg between saving and restoring these flags,
 * they must save and restore \p reg, potentially using dr_save_reg()/dr_restore_reg().
 * If the caller needs to save both the current value of \p reg and the flags stored
 * to \p reg by this routine, they must use separate spill slots, or they will
 * overwrite the original \p reg value in memory.
 *
 * \note On X86, only DR_REG_XAX should be passed in.
 *
 * \warning Clobbers \p reg; the caller must ensure \p reg is dead or saved at
 * \p where.
 */
void
dr_save_arith_flags_to_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                           reg_id_t reg);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore
 * the arithmetic flags (6 arithmetic flags on X86 or APSR on ARM) from \p reg.
 * The caller must ensure that \p reg contains the program status flags,
 * most likely from dr_save_arith_flags_to_reg().
 *
 * \note On X86, only DR_REG_XAX should be passed in.
 */
void
dr_restore_arith_flags_from_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                                reg_id_t reg);

DR_API
/**
 * A convenience routine to aid restoring the arith flags done by outlined code,
 * such as when handling restore state events. The routine takes the current value of
 * the flags register \p cur_xflags, as well as the saved value \p saved_xflag, in order
 * to return the original app value.
 */
reg_t
dr_merge_arith_flags(reg_t cur_xflags, reg_t saved_xflag);

/* FIXME PR 315327: add routines to save, restore and access from C code xmm registers
 * from our dcontext slots.  Not clear we really need to since we can't do it all
 * that much faster than the client can already with read/write tls field (only one
 * extra load) or (if -thread_private) absolute addresses.
 */

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to read into the
 * general-purpose full-size register \p reg from the user-controlled drcontext
 * field for this thread.  Reads from the same field as dr_get_tls_field().
 */
void
dr_insert_read_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to write the
 * general-purpose full-size register \p reg to the user-controlled drcontext field
 * for this thread.  Writes to the same field as dr_set_tls_field().
 */
void
dr_insert_write_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg);

#endif /* CLIENT_INTERFACE (need the next few functions for hot patching) */

DR_API
/** Inserts \p instr as a non-application instruction into \p ilist prior to \p where. */
void
instrlist_meta_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Inserts \p instr as a non-application instruction into \p ilist after \p where. */
void
instrlist_meta_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Inserts \p instr as a non-application instruction onto the end of \p ilist */
void
instrlist_meta_append(instrlist_t *ilist, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) into \p ilist prior to \p where.
 *
 * \deprecated Essentially equivalent to instrlist_meta_preinsert()
 */
void
instrlist_meta_fault_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) into \p ilist after \p where.
 *
 * \deprecated Essentially equivalent to instrlist_meta_postinsert()
 */
void
instrlist_meta_fault_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/**
 * Inserts \p instr as a non-application instruction that can fault (see
 * instr_set_meta_may_fault()) onto the end of \p ilist.
 *
 * \deprecated Essentially equivalent to instrlist_meta_append()
 */
void
instrlist_meta_fault_append(instrlist_t *ilist, instr_t *instr);

/* dr_insert_* are used by general DR */

/* FIXME PR 213600: for clean call args that reference memory the
 * client may prefer to receive the fault itself rather than it being treated
 * as an app exception (xref PR 302951).
 */
DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save state
 * for a call, switch to this thread's DR stack, set up the passed-in
 * parameters, make a call to \p callee, clean up the parameters, and
 * then restore the saved state.
 *
 * The callee must use the standard C calling convention that matches the
 * underlying 32-bit or 64-bit binary interface convention ("cdecl"). Other
 * calling conventions, such as "fastcall" and "stdcall", are not supported.
 *
 * This routine expects to be passed a number of arguments beyond \p
 * num_args equal to the value of \p num_args.  Each of those
 * arguments is a parameter to pass to the clean call, in the order
 * passed to this routine.  Each argument should be of type #opnd_t
 * and will be copied into the proper location for that argument
 * slot as specified by the calling convention.
 *
 * Stores the application state information on the DR stack, where it can
 * be accessed from \c callee using dr_get_mcontext() and modified using
 * dr_set_mcontext().
 *
 * On x86, if \p save_fpstate is true, preserves the fp/mmx state on the
 * DR stack. Note that it is relatively expensive to save this state (on the
 * order of 200 cycles) and that it typically takes 512 bytes to store
 * it (see proc_fpstate_save_size()).
 * The last floating-point instruction address in the saved state is left in
 * an untranslated state (i.e., it may point into the code cache).
 *
 * On ARM/AArch64, \p save_fpstate is ignored.
 *
 * DR does support translating a fault in an argument (e.g., an
 * argument that references application memory); such a fault will be
 * treated as an application exception.
 *
 * The clean call sequence will be optimized based on the runtime option
 * \ref op_cleancall "-opt_cleancall".
 *
 * For 64-bit, for purposes of reachability, this call is assumed to
 * be destined for encoding into DR's code cache-reachable memory region.
 * This includes the code cache as well as memory allocated with
 * dr_thread_alloc(), dr_global_alloc(), dr_nonheap_alloc(), or
 * dr_custom_alloc() with #DR_ALLOC_CACHE_REACHABLE.  The call used
 * here will be direct if it is reachable from those locations; if it
 * is not reachable, an indirect call through r11 will be used (with
 * r11's contents being clobbered).  Use dr_insert_clean_call_ex()
 * with #DR_CLEANCALL_INDIRECT to ensure reachability when encoding to
 * a location other than DR's regular code region.  See also
 * dr_insert_call_ex().
 *
 * \note The stack used to save state and call \p callee is limited to
 * 20KB by default; this can be changed with the -stack_size DR runtime
 * parameter.  This stack cannot be used to store state that persists
 * beyond \c callee's return point.
 *
 * \note This routine only supports passing arguments that are
 * integers or pointers of a size equal to the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 * The routine also supports immediate integers that are smaller than
 * the register size, and for 64-bit mode registers or memory references that
 * are OPSZ_4.
 *
 * \note For 64-bit mode, passing arguments that use calling
 * convention registers (for Windows, RCX, RDX, R8, R9; for Linux,
 * RDI, RSI, RDX, RCX, R8 and R9) are supported but may incur
 * additional stack usage.
 *
 * \note For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * \note For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * \note Arguments that reference sub-register portions of DR_REG_XSP are
 * not supported (full DR_REG_XSP is supported).
 */
void
dr_insert_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                     bool save_fpstate, uint num_args, ...);

/* DR_API EXPORT BEGIN */
/**
 * Flags to request non-default preservation of state in a clean call
 * as well as other call options.
 */
typedef enum {
    /**
     * Save floating-point state (x86-specific).
     * The last floating-point instruction address in the saved state is left in
     * an untranslated state (i.e., it may point into the code cache).
     */
    DR_CLEANCALL_SAVE_FLOAT = 0x0001,
    /**
     * Skip saving the flags and skip clearing the flags (including
     * DF) for client execution.  Note that this can cause problems
     * if dr_redirect_execution() is called from a clean call,
     * as an uninitialized flags value can cause subtle errors.
     */
    DR_CLEANCALL_NOSAVE_FLAGS = 0x0002,
    /** Skip saving any XMM or YMM registers. */
    DR_CLEANCALL_NOSAVE_XMM = 0x0004,
    /** Skip saving any XMM or YMM registers that are never used as parameters. */
    DR_CLEANCALL_NOSAVE_XMM_NONPARAM = 0x0008,
    /** Skip saving any XMM or YMM registers that are never used as return values. */
    DR_CLEANCALL_NOSAVE_XMM_NONRET = 0x0010,
    /**
     * Requests that an indirect call be used to ensure reachability, both for
     * reaching the callee and for any out-of-line helper routine calls.
     * Only honored for 64-bit mode, where r11 will be used for the indirection.
     */
    DR_CLEANCALL_INDIRECT = 0x0020,
    /* internal use only: maps to META_CALL_RETURNS_TO_NATIVE in insert_meta_call_vargs */
    DR_CLEANCALL_RETURNS_TO_NATIVE = 0x0040,
    /**
     * Requests that out-of-line state save and restore routines be used even
     * when a subset of the state does not need to be preserved for this callee.
     * Also disables inlining.
     * This helps guarantee that the inserted code remains small.
     */
    DR_CLEANCALL_ALWAYS_OUT_OF_LINE = 0x0080,
} dr_cleancall_save_t;
/* DR_API EXPORT END */

DR_API
/**
 * Identical to dr_insert_clean_call() except it takes in \p
 * save_flags which allows requests to not save certain state.  This
 * is intended for use at application call entry points or other
 * contexts where a client is comfortable making assumptions.  Keep in
 * mind that any register that is not saved will not be present in a
 * context obtained from dr_get_mcontext().
 */
void
dr_insert_clean_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        dr_cleancall_save_t save_flags, uint num_args, ...);

/* Inserts a complete call to callee with the passed-in arguments, wrapped
 * by an app save and restore.
 * On x86, if \p save_fpstate is true, saves the fp/mmx state.
 * On ARM/AArch64, \p save_fpstate is ignored.
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_prepare_for_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 *
 * NOTE : dr_insert_cbr_instrumentation has assumption about the clean call
 * instrumentation layout, changes to the clean call instrumentation may break
 * dr_insert_cbr_instrumentation.
 */
void
dr_insert_clean_call_ex_varg(void *drcontext, instrlist_t *ilist, instr_t *where,
                             void *callee, dr_cleancall_save_t save_flags, uint num_args,
                             opnd_t *args);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to set
 * up the passed-in parameters, make a call to \p callee, and clean up
 * the parameters.
 *
 * The callee must use the standard C calling convention that matches the
 * underlying 32-bit or 64-bit binary interface convention ("cdecl"). Other
 * calling conventions, such as "fastcall" and "stdcall", are not supported.
 *
 * This routine uses the existing stack.  In 64-bit mode, this routine assumes
 * that the stack pointer is currently 16-byte aligned.
 *
 * The application state is NOT saved or restored (use dr_prepare_for_call()
 * and dr_cleanup_after_call(), or replace this routine with dr_insert_clean_call()).
 * The parameter set-up may write to registers if the calling convention so
 * dictates.  The registers are NOT saved beforehand (to do so, use
 * dr_insert_clean_call()).
 *
 * It is up to the caller of this routine to preserve any caller-saved registers
 * that the callee might modify.
 *
 * DR does not support translating a fault in an argument.  For fault
 * transparency, the client must perform the translation (see
 * #dr_register_restore_state_event()), or use
 * #dr_insert_clean_call().
 *
 * For 64-bit, for purposes of reachability, this call is assumed to
 * be destined for encoding into DR's code cache-reachable memory region.
 * This includes the code cache as well as memory allocated with
 * dr_thread_alloc(), dr_global_alloc(), dr_nonheap_alloc(), or
 * dr_custom_alloc() with #DR_ALLOC_CACHE_REACHABLE.  The call used
 * here will be direct if it is reachable from those locations; if it
 * is not reachable, an indirect call through r11 will be used (with
 * r11's contents being clobbered).  Use dr_insert_call_ex() when
 * encoding to a location other than DR's regular code region.
 *
 * \note This routine only supports passing arguments that are
 * integers or pointers of a size equal to the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 * The routine also supports immediate integers that are smaller than
 * the register size, and for 64-bit mode registers or memory references that
 * are OPSZ_4.
 *
 * \note For 64-bit mode, passing arguments that use calling
 * convention registers (for Windows, RCX, RDX, R8, R9; for Linux,
 * RDI, RSI, RDX, RCX, R8 and R9) are supported but may incur
 * additional stack usage.
 *
 * \note For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * \note For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * \note Arguments that reference DR_REG_XSP are not supported in 64-bit mode.
 */
void
dr_insert_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
               uint num_args, ...);

DR_API
/**
 * Identical to dr_insert_call() except it takes in \p encode_pc
 * indicating roughly where the call sequence will be encoded.  If \p
 * callee is not reachable from \p encode_pc plus or minus one page,
 * an indirect call will be used instead of the direct call used by
 * dr_insert_call().  The indirect call overwrites the r11 register.
 *
 * \return true if the inserted call is direct and false if indirect.
 */
bool
dr_insert_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, byte *encode_pc,
                  void *callee, uint num_args, ...);

/* Not exported.  Currently used for ARM to avoid storing to %lr. */
void
dr_insert_call_noreturn(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        uint num_args, ...);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save state for a call.
 * Stores the application state information on the DR stack.
 * Returns the size of the data stored on the DR stack (in case the caller
 * needs to align the stack pointer).
 *
 * \warning On x86, this routine does NOT save the fp/mmx state: to do that
 * the instrumentation routine should call proc_save_fpstate() to save and
 * then proc_restore_fpstate() to restore (or use dr_insert_clean_call()).
 *
 * \note The preparation modifies the DR_REG_XSP and DR_REG_XAX registers
 * (after saving them).  Use dr_insert_clean_call() instead if an
 * argument to the subsequent call that references DR_REG_XAX is
 * desired.
 *
 * \note The stack used to save the state is limited to
 * 20KB by default; this can be changed with the -stack_size DR runtime
 * parameter.  This stack cannot be used to store state that persists
 * beyond a single clean call, code cache execution, or probe callback
 * function execution.
 */
uint
dr_prepare_for_call(void *drcontext, instrlist_t *ilist, instr_t *instr);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore state
 * after a call.
 */
void
dr_cleanup_after_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      uint sizeof_param_area);

#ifdef CLIENT_INTERFACE
DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the current
 * esp and switch to this thread's DR stack.
 * \note The DR stack is limited to 20KB by default; this can be changed with
 * the -stack_size DR runtime parameter.  This stack cannot be used to store
 * state that persists beyond a single clean call, code cache execution,
 * or probe callback function execution.
 */
void
dr_swap_to_clean_stack(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore into
 * esp the value saved by dr_swap_to_clean_stack().
 */
void
dr_restore_app_stack(void *drcontext, instrlist_t *ilist, instr_t *where);

DR_API
/**
 * Calls the specified function \p func after switching to the DR stack
 * for the thread corresponding to \p drcontext.
 * Passes in 8 arguments.  Uses the C calling convention, so \p func will work
 * just fine even if if takes fewer than 8 args.
 * Swaps the stack back upon return and returns the value returned by \p func.
 *
 * On Windows, this routine does swap the TEB stack fields, avoiding
 * issues with fault handling on Windows 8.1.  This means there is no need
 * for the callee to use dr_switch_to_dr_state_ex() with DR_STATE_STACK_BOUNDS.
 */
void *
dr_call_on_clean_stack(void *drcontext, void *(*func)(void), void *arg1, void *arg2,
                       void *arg3, void *arg4, void *arg5, void *arg6, void *arg7,
                       void *arg8);

/* providing functionality of old -instr_calls and -instr_branches flags */
DR_API
/**
 * Assumes that \p instr is a near call.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of call instruction (caller)
 * -# target address of call (callee)
 */
void
dr_insert_call_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               void *callee);

DR_API
/**
 * Assumes that \p instr is an indirect branch.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of branch instruction
 * -# target address of branch
 * \note Only the address portion of a far indirect branch is considered.
 * \note \p scratch_slot must be <= dr_max_opnd_accessible_spill_slot(). \p
 * scratch_slot is used internally to this routine and will be clobbered.
 */
#    ifdef AVOID_API_EXPORT
/* If we re-enable -opt_speed (or -indcall2direct directly) we should add back:
 * \note This routine is not supported when the -opt_speed option is specified.
 */
#    endif
void
dr_insert_mbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee, dr_spill_slot_t scratch_slot);

DR_API
/**
 * Assumes that \p instr is a conditional branch
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * three arguments:
 * -# address of branch instruction
 * -# target address of branch
 * -# 0 if the branch is not taken, 1 if it is taken
 */
void
dr_insert_cbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee);

DR_API
/**
 * Assumes that \p instr is a conditional branch
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * four arguments:
 * -# address of branch instruction
 * -# target address of branch
 * -# fall-through address of branch
 * -# 0 if the branch is not taken, 1 if it is taken
 * -# user defined operand (e.g., TLS slot, immed value, register, etc.)
 * \note The user defined operand cannot use register ebx!
 */
void
dr_insert_cbr_instrumentation_ex(void *drcontext, instrlist_t *ilist, instr_t *instr,
                                 void *callee, opnd_t user_data);

/* FIXME: will never see any ubrs! */
DR_API
/**
 * Assumes that \p instr is a direct, near, unconditional branch.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of branch instruction
 * -# target address of branch
 *
 * \warning Basic block eliding is controlled by -max_elide_jmp.  If that
 * option is set to non-zero, ubrs may never be seen.
 */
void
dr_insert_ubr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee);

DR_API
/**
 * Causes DynamoRIO to insert code that stores \p value into the
 * return address slot on the stack immediately after the original
 * value is read by the return instruction \p instr.
 * \p instr must be a return instruction or this routine will fail.
 *
 * On ARM, \p value is ignored and instead a value that is guaranteed
 * to not look like a return address is used.  This is for efficiency
 * reasons, as on ARM it would require an extra register spill in
 * order to write an arbitrary value.
 *
 * \note This is meant to make it easier to obtain efficient
 * callstacks by eliminating stale return addresses from prior stack
 * frames.  However, it is possible that writing to the application
 * stack could result in incorrect application behavior, so use this
 * at your own risk.
 *
 * \return whether successful.
 */
bool
dr_clobber_retaddr_after_read(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              ptr_uint_t value);

DR_API
/**
 * Returns true if the simd fields in dr_mcontext_t are valid xmm, ymm, or zmm values
 * (i.e., whether the underlying processor supports SSE).
 * \note If #DR_MC_MULTIMEDIA is not specified when calling dr_get_mcontext(),
 * the simd fields will not be filled in regardless of the return value
 * of this routine.
 */
bool
dr_mcontext_xmm_fields_valid(void);

DR_API
/**
 * Returns true if the simd fields in dr_mcontext_t are valid zmm values
 * (i.e., whether the underlying processor and OS support AVX-512 and AVX-512 code
 * is present).
 * \note If #DR_MC_MULTIMEDIA is not specified when calling dr_get_mcontext(),
 * the simd fields will not be filled in regardless of the return value
 * of this routine.
 */
bool
dr_mcontext_zmm_fields_valid(void);

#endif /* CLIENT_INTERFACE */
/* dr_get_mcontext() needed for translating clean call arg errors */

DR_API
/**
 * Copies the fields of the current application machine context selected
 * by the \p flags field of \p context into \p context.
 *
 * This routine may only be called from:
 * - A clean call invoked by dr_insert_clean_call() or dr_prepare_for_call()
 * - A pre- or post-syscall event (dr_register_pre_syscall_event(),
 *   dr_register_post_syscall_event())
 * - Basic block or trace creation events (dr_register_bb_event(),
 *   dr_register_trace_event()), but for basic block creation only when the
 *   basic block callback parameters \p for_trace and \p translating are
 *   false, and for trace creation only when \p translating is false.
 * - A nudge callback (dr_register_nudge_event()) on Linux.
 *   (On Windows nudges happen in separate dedicated threads.)
 * - A thread or process exit event (dr_register_thread_exit_event(),
 *   dr_register_exit_event())
 * - A thread init event (dr_register_thread_init_event()) for all but
 *   the initial thread.
 * - A kernel transfer event (dr_register_kernel_xfer_event()).  Here the obtained
 *   context is the target context of the transfer, not the source (about to be
 *   changed) context.  For Windows system call event types #DR_XFER_CONTINUE and
 *   #DR_XFER_SET_CONTEXT_THREAD, only the portions of the context selected by the
 *   application are available.  The \p flags field of \p context is adjusted to
 *   reflect which fields were returned.  Given the disparity in how Ebp/Rbp is
 *   handled (in #DR_MC_INTEGER but in CONTEXT_CONTROL), clients that care about that
 *   register are better off using system call events instead of kernel transfer events
 *   to take actions on these two system calls.
 *
 * Even when #DR_MC_CONTROL is specified, does NOT copy the pc field,
 * except for system call events, when it will point at the
 * post-syscall address, and kernel transfer events, when it will point to the
 * target pc.
 *
 * Returns false if called from the init event or the initial thread's
 * init event; returns true otherwise (cannot distinguish whether the
 * caller is in a clean call so it is up to the caller to ensure it is
 * used properly).
 *
 * The size field of \p context must be set to the size of the
 * structure as known at compile time.  If the size field is invalid,
 * this routine will return false.
 *
 * The flags field of \p context must be set to the desired amount of
 * information using the dr_mcontext_flags_t values.  Asking for
 * multimedia registers incurs a higher performance cost.  An invalid
 * flags value will return false.
 *
 * \note NUM_SIMD_SLOTS in the dr_mcontext_t.xmm array are filled in,
 * but only if dr_mcontext_xmm_fields_valid() returns true and
 * #DR_MC_MULTIMEDIA is set in the flags field.
 *
 * \note The context is the context saved at the dr_insert_clean_call() or
 * dr_prepare_for_call() points.  It does not correct for any registers saved
 * with dr_save_reg().  To access registers saved with dr_save_reg() from a
 * clean call use dr_read_saved_reg().
 *
 * \note System data structures are swapped to private versions prior to
 * invoking clean calls or client events.  Use dr_switch_to_app_state()
 * to examine the application version of system state.
 */
bool
dr_get_mcontext(void *drcontext, dr_mcontext_t *context);

#ifdef CLIENT_INTERFACE
DR_API
/**
 * Sets the fields of the application machine context selected by the
 * flags field of \p context to the values in \p context.
 *
 * This routine may only be called from:
 * - A clean call invoked by dr_insert_clean_call() or dr_prepare_for_call()
 * - A pre- or post-syscall event (dr_register_pre_syscall_event(),
 *   dr_register_post_syscall_event())
 *   dr_register_thread_exit_event())
 * - A kernel transfer event (dr_register_kernel_xfer_event()) other than
 *   #DR_XFER_CALLBACK_RETURN.  Here the modified context is the target context of
 *   the transfer, not the source (about to be changed) context.  For Windows system
 *   call event types #DR_XFER_CONTINUE and #DR_XFER_SET_CONTEXT_THREAD, only the
 *   portions of the context selected by the application can be changed.  The \p
 *   flags field of \p context is adjusted to reflect which fields these are.  Given
 *   the disparity in how Ebp/Rbp is handled (in #DR_MC_INTEGER but in
 *   CONTEXT_CONTROL), clients that care about that register are better off using
 *   system call events instead of kernel transfer events to take actions on these
 *   two system calls.  - Basic block or trace creation events
 *   (dr_register_bb_event(), dr_register_trace_event()), but for basic block
 *   creation only when the basic block callback parameters \p for_trace and \p
 *   translating are false, and for trace creation only when \p translating is false.
 *
 * Ignores the pc field, except for kernel transfer events.
 *
 * If the size field of \p context is invalid, this routine will
 * return false.  A dr_mcontext_t obtained from DR will have the size field set.
 *
 * The flags field of \p context must be set to select the desired
 * fields for copying, using the dr_mcontext_flags_t values.  Asking
 * to copy multimedia registers incurs a higher performance cost.  An
 * invalid flags value will return false.
 *
 * \return whether successful.
 *
 * \note The xmm fields are only set for processes where the underlying
 * processor supports them (and when DR_MC_MULTIMEDIA is set in the flags field).
 * For dr_insert_clean_call() that requested \p
 * save_fpstate, the xmm values set here override that saved state.  Use
 * dr_mcontext_xmm_fields_valid() to determine whether the xmm fields are valid.
 */
bool
dr_set_mcontext(void *drcontext, dr_mcontext_t *context);

/* FIXME - combine with dr_set_mcontext()?  Implementation wise it's nice to split the
 * two since handling the pc with dr_set_mcontext() would complicate the clean call
 * handling. But perhaps would be nicer from an interface perspective to combine them. */
DR_API
/**
 * Immediately resumes application execution from a clean call out of the cache (see
 * dr_insert_clean_call() or dr_prepare_for_call()) or an exception event with the
 * state specified in \p mcontext (including pc, and including the xmm fields
 * that are valid according to dr_mcontext_xmm_fields_valid()).
 * The flags field of \p context must contain DR_MC_ALL; using a partial set
 * of fields is not suported.
 *
 * \note dr_get_mcontext() can be used to get the register state (except pc)
 * saved in dr_insert_clean_call() or dr_prepare_for_call().
 *
 * \note If floating point state was saved by dr_prepare_for_call() or
 * dr_insert_clean_call() it is not restored (other than the valid xmm
 * fields according to dr_mcontext_xmm_fields_valid(), if
 * DR_MC_MULTIMEDIA is specified in the flags field).  The caller
 * should instead manually save and restore the floating point state
 * with proc_save_fpstate() and proc_restore_fpstate() if necessary.
 *
 * \note If the caller wishes to set any other state (such as xmm
 * registers that are not part of the mcontext) they may do so by just
 * setting that state in the current thread before making this call.
 * To set system data structures, use dr_switch_to_app_state(), make
 * the changes, and then switch back with dr_switch_to_dr_state()
 * before calling this routine.
 *
 * \note This routine may only be called from a clean call from the cache. It can not be
 * called from any registered event callback except the exception event
 * (dr_register_exception_event()).  From a signal event callback, use the
 * #DR_SIGNAL_REDIRECT return value rather than calling this routine.
 *
 * \note For ARM, to redirect execution to a Thumb target (#DR_ISA_ARM_THUMB),
 * set the least significant bit of the mcontext pc to 1. Reference
 * \ref sec_thumb for more information.
 *
 * \return false if unsuccessful; if successful, does not return.
 */
bool
dr_redirect_execution(dr_mcontext_t *context);

/* DR_API EXPORT BEGIN */
/** Flags to request non-default preservation of state in a clean call */
#    define SPILL_SLOT_REDIRECT_NATIVE_TGT SPILL_SLOT_1
/* DR_API EXPORT END */

DR_API
/**
 * Returns the target to use for a native context transfer to a target
 * application address.
 *
 * Normally, redirection is performed from a client context in a clean
 * call or event callback by invoking dr_redirect_execution().  In
 * some circumstances, redirection from an application (or "native")
 * context is desirable without creating an application control
 * transfer in a basic block.
 *
 * To accomplish such a redirection, store the target application
 * address in #SPILL_SLOT_REDIRECT_NATIVE_TGT by calling
 * dr_write_saved_reg().  Set up any other application state as
 * desired directly in the current machine context.  Then jump to the
 * target returned by this routine.  By default, the target is global
 * and can be cached globally.  However, if traces are thread-private,
 * or if traces are disabled and basic blocks are thread-private,
 * there will be a separate target per \p drcontext.
 *
 * If a basic block is exited via such a redirection, the block should
 * be emitted with the flag DR_EMIT_MUST_END_TRACE in order to avoid
 * trace building errors.
 *
 * For ARM, the address returned by this routine has its least significant
 * bit set to 1 if the target is Thumb.
 *
 * Returns null on error.
 */
byte *
dr_redirect_native_target(void *drcontext);

/* DR_API EXPORT BEGIN */
#    ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Copies the machine state in \p src into \p dst.  Sets the \p
 * ContextFlags field of \p dst to reflect the \p flags field of \p
 * src.  However, CONTEXT_CONTROL includes Ebp/Rbp, while that's under
 * #DR_MC_INTEGER, so we recommend always setting both #DR_MC_INTEGER
 * and #DR_MC_CONTROL when calling this routine.
 *
 * It is up to the caller to ensure that \p dst is allocated and
 * initialized properly in order to contain multimedia processor
 * state, if #DR_MC_MULTIMEDIA is set in the \p flags field of \p src.
 *
 * The current segment register values are filled in under the assumption
 * that this context is for the calling thread.
 *
 * \note floating-point values are not filled in for \p dst.
 * \note Windows only.
 *
 * \return false if unsuccessful; if successful, does not return.
 */
bool
dr_mcontext_to_context(CONTEXT *dst, dr_mcontext_t *src);
/* DR_API EXPORT BEGIN */
#    endif
/* DR_API EXPORT END */

DR_API
/**
 * Create meta instructions for storing pointer-size integer \p val to \p dst,
 * and then insert them into \p ilist prior to \p where.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 * If the instruction is a no-op (when dst is the zero register on AArch64)
 * then no instructions are created and NULL is returned in first and last.
 */
void
instrlist_insert_mov_immed_ptrsz(void *drcontext, ptr_int_t val, opnd_t dst,
                                 instrlist_t *ilist, instr_t *where, instr_t **first OUT,
                                 instr_t **last OUT);

DR_API
/**
 * Create meta instructions for pushing pointer-size integer \p val on the stack,
 * and then insert them into \p ilist prior to \p where.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 */
void
instrlist_insert_push_immed_ptrsz(void *drcontext, ptr_int_t val, instrlist_t *ilist,
                                  instr_t *where, instr_t **first OUT,
                                  instr_t **last OUT);

DR_API
/**
 * Create meta instructions for storing the address of \p src_inst to \p dst,
 * and then insert them into \p ilist prior to \p where.
 * The \p encode_estimate parameter, used only for 64-bit mode,
 * indicates whether the final address of \p src_inst, when it is
 * encoded later, will fit in 32 bits or needs 64 bits.
 * If the encoding will be in DynamoRIO's code cache, pass NULL.
 * If the final encoding location is unknown, pass a high address to be on
 * the safe side.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 * If the instruction is a no-op (when dst is the zero register on AArch64)
 * then no instructions are created and NULL is returned in first and last.
 */
void
instrlist_insert_mov_instr_addr(void *drcontext, instr_t *src_inst, byte *encode_estimate,
                                opnd_t dst, instrlist_t *ilist, instr_t *where,
                                instr_t **first OUT, instr_t **last OUT);

DR_API
/**
 * Create meta instructions for pushing the address of \p src_inst on the stack,
 * and then insert them into \p ilist prior to \p where.
 * The \p encode_estimate parameter, used only for 64-bit mode,
 * indicates whether the final address of \p src_inst, when it is
 * encoded later, will fit in 32 bits or needs 64 bits.
 * If the encoding will be in DynamoRIO's code cache, pass NULL.
 * If the final encoding location is unknown, pass a high address to be on
 * the safe side.
 * Pointers to the first and last created meta instructions are returned
 * in \p first and \p last, unless only one meta instruction is created,
 * in which case NULL is returned in last.
 */
void
instrlist_insert_push_instr_addr(void *drcontext, instr_t *src_inst,
                                 byte *encode_estimate, instrlist_t *ilist,
                                 instr_t *where, instr_t **first OUT, instr_t **last OUT);

DR_API
/**
 * Returns the register that is stolen and used by DynamoRIO.
 * Reference \ref sec_reg_stolen for more information.
 */
reg_id_t
dr_get_stolen_reg(void);

DR_API
/**
 * Insert code to get the application value of the register stolen by DynamoRIO
 * into register \p reg.
 * Reference \ref sec_reg_stolen for more information.
 *
 * \return whether successful.
 *
 * \note ARM-only
 */
bool
dr_insert_get_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg);

DR_API
/**
 * Insert code to set the value of register \p reg as the application value of
 * the register stolen by DynamoRIO
 * Reference \ref sec_reg_stolen for more information.
 *
 * \return whether successful.
 *
 * \note ARM-only
 */
bool
dr_insert_set_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg);

DR_API
/**
 * Removes all OP_it instructions from \p ilist without changing the
 * instructions that were inside each IT block.  This is intended to
 * be paired with dr_insert_it_instrs(), where a client's examination
 * of the application instruction list and insertion of
 * instrumentation occurs in between the two calls and thus does not
 * have to worry about groups of instructions that cannot be separated
 * or changed.  The resulting predicated instructions are not
 * encodable in Thumb mode (#DR_ISA_ARM_THUMB): dr_insert_it_instrs()
 * must be called before encoding.
 *
 * \return the number of OP_it instructions removed; -1 on error.
 *
 * \note ARM-only
 */
int
dr_remove_it_instrs(void *drcontext, instrlist_t *ilist);

DR_API
/**
 * Inserts enough OP_it instructions with proper parameters into \p
 * ilist to make all predicated instructions in \p ilist legal in
 * Thumb mode (#DR_ISA_ARM_THUMB).  Treats predicated app and tool
 * instructions identically, but marks inserted OP_it instructions as
 * app instructions (see instr_set_app()).
 *
 * \return the number of OP_it instructions inserted; -1 on error.
 *
 * \note ARM-only
 */
int
dr_insert_it_instrs(void *drcontext, instrlist_t *ilist);

/* DR_API EXPORT TOFILE dr_tools.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * ADAPTIVE OPTIMIZATION SUPPORT
 */
/* DR_API EXPORT END */

/* xref PR 199115 and PR 237461: We decided to make the replace and
 * delete routines valid for -thread_private only.  Both routines
 * replace for the current thread and leave the other threads
 * unmodified.  The rationale is that we expect these routines will be
 * primarily useful for optimization, where a client wants to modify a
 * fragment specific to one thread.
 */
DR_API
/**
 * Replaces the fragment with tag \p tag with the instructions in \p
 * ilist.  This routine is only valid with the -thread_private option;
 * it replaces the fragment for the current thread only.  After
 * replacement, the existing fragment is allowed to complete if
 * currently executing.  For example, a clean call replacing the
 * currently executing fragment will safely return to the existing
 * code.  Subsequent executions will use the new instructions.
 *
 * \note The routine takes control of \p ilist and all responsibility
 * for deleting it.  The client should not keep, use, or reference,
 * the instrlist or any of the instrs it contains after passing.
 *
 * \note This routine supports replacement for the current thread
 * only.  \p drcontext must be from the current thread and must
 * be the drcontext used to create the instruction list.
 * This routine may not be called from the thread exit event.
 *
 * \return false if the fragment does not exist and true otherwise.
 */
bool
dr_replace_fragment(void *drcontext, void *tag, instrlist_t *ilist);

DR_API
/**
 * Deletes the fragment with tag \p tag.  This routine is only valid
 * with the -thread_private option; it deletes the fragment in the
 * current thread only.  After deletion, the existing fragment is
 * allowed to complete execution.  For example, a clean call deleting
 * the currently executing fragment will safely return to the existing
 * code.  Subsequent executions will cause DynamoRIO to reconstruct
 * the fragment, and therefore call the appropriate fragment-creation
 * event hook, if registered.
 *
 * \note This routine supports deletion for the current thread
 * only.  \p drcontext must be from the current thread and must
 * be the drcontext used to create the instruction list.
 * This routine may not be called from the thread exit event.
 *
 * \note Other options of removing the code fragments from code cache include
 * dr_flush_region(), dr_unlink_flush_region(), and dr_delay_flush_region().
 *
 * \return false if the fragment does not exist and true otherwise.
 */
bool
dr_delete_fragment(void *drcontext, void *tag);

/* FIXME - xref PR 227619 - some other event handler are safe (image_load/unload for*
 * example) which we could note here. */
DR_API
/**
 * Flush all fragments containing any code from the region [\p start, \p start + \p size).
 * Once this routine returns no execution will occur out of the fragments flushed.
 * This routine may only be called during a clean call from the cache, from a nudge
 * event handler, or from a pre- or post-system call event handler.
 * It may not be called from any other event callback.  No locks can
 * held when calling this routine.  If called from a clean call, caller can NOT return
 * to the cache (the fragment that was called out of may have been flushed even if it
 * doesn't apparently overlap the flushed region). Instead the caller must redirect
 * execution via dr_redirect_execution() (or DR_SIGNAL_REDIRECT from a signal callback)
 * after this routine to continue execution.  Returns true if successful.
 *
 * \note This routine may not be called from any registered event callback other than
 * the nudge event, the pre- or post-system call events, the exception event, or
 * the signal event; clean calls
 * out of the cache may call this routine.
 *
 * \note If called from a clean call, caller must continue execution by calling
 * dr_redirect_execution() after this routine, as the fragment containing the callout may
 * have been flushed. The context to use can be obtained via
 * dr_get_mcontext() with the exception of the pc to continue at which must be passed as
 * an argument to the callout (see instr_get_app_pc()) or otherwise determined.
 *
 * \note This routine may not be called while any locks are held that could block a thread
 * processing a registered event callback or cache callout.
 *
 * \note dr_delay_flush_region() has fewer restrictions on use, but is less synchronous.
 *
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start. A flush of \p size == 0 is not allowed.
 *
 * \note As currently implemented, dr_delay_flush_region() with no completion callback
 * routine specified can be substantially more performant.
 */
bool
dr_flush_region(app_pc start, size_t size);

/* FIXME - get rid of the no locks requirement by making event callbacks !couldbelinking
 * and no dr locks (see PR 227619) so that client locks owned by this thread can't block
 * any couldbelinking thread.  FIXME - would be nice to make this available for
 * windows since there's less of a performance hit than using synch_all flushing, but
 * with coarse_units can't tell if we need a synch all flush or not and that confuses
 * the interface a lot. FIXME - xref PR 227619 - some other event handler are safe
 * (image_load/unload for example) which we could note here. */
/* FIXME - add a completion callback (see vm_area_check_shared_pending()). */
/* FIXME - could enable on windows when -thread_private since no coarse then. */
DR_API
/**
 * Flush all fragments containing any code from the region [\p start, \p start + \p size).
 * Control will not enter a fragment containing code from the region after this returns,
 * but a thread already in such a fragment will finish out the fragment.  This includes
 * the current thread if this is called from a clean call that returns to the cache.
 * This routine may only be called during a clean call from the cache, from a nudge
 * event handler, or from a pre- or post-system call event handler.
 * It may not be called from any other event callback.  No locks can be
 * held when calling this routine.  Returns true if successful.
 *
 * \note This routine may not be called from any registered event callback other than
 * the nudge event, the pre- or post-system call events, the exception event, or
 * the signal event; clean calls
 * out of the cache may call this routine.
 * \note This routine may not be called while any locks are held that could block a thread
 * processing a registered event callback or cache callout.
 * \note dr_delay_flush_region() has fewer restrictions on use, but is less synchronous.
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start. A flush of \p size == 0 is not allowed.
 * \note This routine is only available with either the -thread_private
 * or -enable_full_api options.  It is not available when -opt_memory is specified.
 */
bool
dr_unlink_flush_region(app_pc start, size_t size);

/* FIXME - can we better bound when the flush will happen?  Maybe unlink shared syscalls
 * or similar or check the queue in more locations?  Should always hit the flush before
 * executing new code in the cache, and I think we'll always hit it before a nudge is
 * processed too.  Could trigger a nudge, or do this in a nudge, but that's rather
 * expensive. */
DR_API
/**
 * Request a flush of all fragments containing code from the region
 * [\p start, \p start + \p size).  The flush will be performed at the next safe
 * point in time (usually before any new code is added to the cache after this
 * routine is called). If \p flush_completion_callback is non-NULL, it will be
 * called with the \p flush_id provided to this routine when the flush completes,
 * after which no execution will occur out of the fragments flushed. Returns true
 * if the flush was successfully queued.
 *
 * \note dr_flush_region() and dr_unlink_flush_region() can give stronger guarantees on
 * when the flush will occur, but have more restrictions on use.
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start.  A flush of \p size == 0 is not allowed.
 * \note As currently implemented there may be a performance penalty for requesting a
 * \p flush_completion_callback; for most performant usage set
 * \p flush_completion_callback to NULL.
 */
bool
dr_delay_flush_region(app_pc start, size_t size, uint flush_id,
                      void (*flush_completion_callback)(int flush_id));

DR_API
/** Returns whether or not there is a fragment in code cache with tag \p tag. */
bool
dr_fragment_exists_at(void *drcontext, void *tag);

DR_API
/**
 * Returns true if a basic block with tag \p tag exists in the code cache.
 */
bool
dr_bb_exists_at(void *drcontext, void *tag);

DR_API
/**
 * Looks up the fragment with tag \p tag.
 * If not found, returns 0.
 * If found, returns the total size occupied in the cache by the fragment.
 */
uint
dr_fragment_size(void *drcontext, void *tag);

DR_API
/** Retrieves the application PC of a fragment with tag \p tag. */
app_pc
dr_fragment_app_pc(void *tag);

DR_API
/**
 * Given an application PC, returns a PC that contains the application code
 * corresponding to the original PC.  In some circumstances on Windows DR
 * inserts a jump on top of the original code, which the client will not
 * see in the bb and trace hooks due to DR replacing it there with the
 * displaced original application code in order to present the client with
 * an unmodified view of the application code.  A client should use this
 * routine when attempting to decode the original application instruction
 * that caused a fault from the translated fault address, as the translated
 * address may actually point in the middle of DR's jump.
 *
 * \note Other applications on the system sometimes insert their own hooks,
 * which will not be hidden by DR and will appear to the client as jumps
 * and subsequent displaced code.
 */
app_pc
dr_app_pc_for_decoding(app_pc pc);

DR_API
/**
 * Given a code cache pc, returns the corresponding application pc.
 * This involves translating the state and thus may incur calls to
 * the basic block and trace events (see dr_register_bb_event()).
 * If translation fails, returns NULL.
 * This routine may not be called from a thread exit event.
 */
app_pc
dr_app_pc_from_cache_pc(byte *cache_pc);

DR_API
/**
 * Returns whether the given thread indicated by \p drcontext
 * is currently using the application version of its system state.
 * \sa dr_switch_to_dr_state(), dr_switch_to_app_state().
 *
 * This function does not indicate whether the machine context
 * (registers) contains application state or not.
 *
 * On Linux, DR very rarely switches the system state, while on
 * Windows DR switches the system state to the DR and client version
 * on every event callback or clean call.
 */
bool
dr_using_app_state(void *drcontext);

DR_API
/** Equivalent to dr_switch_to_app_state_ex(drcontext, DR_STATE_ALL). */
void
dr_switch_to_app_state(void *drcontext);

DR_API
/**
 * Swaps to the application version of any system state for the given
 * thread.  This is meant to be used prior to examining application
 * memory, when private libraries are in use and there are two
 * versions of system state.  Invoking non-DR library routines while
 * the application state is in place can lead to unpredictable
 * results: call dr_switch_to_dr_state() (or the _ex version) before
 * doing so.
 *
 * This function does not affect whether the current machine context
 * (registers) contains application state or not.
 *
 * The \p flags argument allows selecting a subset of the state to swap.
 */
void
dr_switch_to_app_state_ex(void *drcontext, dr_state_flags_t flags);

DR_API
/** Equivalent to dr_switch_to_dr_state_ex(drcontext, DR_STATE_ALL). */
void
dr_switch_to_dr_state(void *drcontext);

DR_API
/**
 * Should only be called after calling dr_switch_to_app_state() (or
 * the _ex version), or in certain cases where a client is running its
 * own code in an application state.  Swaps from the application
 * version of system state for the given thread back to the DR and
 * client version.
 *
 * This function does not affect whether the current machine context
 * (registers) contains application state or not.
 *
 * A client must call dr_switch_to_dr_state() in order to safely call
 * private library routines if it is running in an application context
 * where dr_using_app_state() returns true.  On Windows, this is the
 * case for any application context, as the system state is always
 * swapped.  On Linux, however, execution of application code in the
 * code cache only swaps the machine context and not the system state.
 * Thus, on Linux, while in the code cache, dr_using_app_state() will
 * return false, and it is safe to invoke private library routines
 * without calling dr_switch_to_dr_state().  Only if client or
 * client-invoked code will examine a segment selector or descriptor
 * does the state need to be swapped.  A state swap is much more
 * expensive on Linux (it requires a system call) than on Windows.
 *
 * The same flags that were passed to dr_switch_to_app_state_ex() should
 * be passed here.
 */
void
dr_switch_to_dr_state_ex(void *drcontext, dr_state_flags_t flags);

DR_API
/**
 * Intended to be called between dr_app_setup() and dr_app_start() to
 * pre-create code cache fragments for each basic block address in the
 * \p tags array.  This speeds up the subsequent attach when
 * dr_app_start() is called.
 * If any code in the passed-in tags array is not readable, it is up to the
 * caller to handle any fault, as DR's own signal handlers are not enabled
 * at this point.
 * Returns whether successful.
 */
bool
dr_prepopulate_cache(app_pc *tags, size_t tags_count);

/* DR_API EXPORT BEGIN */
/**
 * Specifies the type of indirect branch for use with dr_prepopulate_indirect_targets().
 */
typedef enum {
    DR_INDIRECT_RETURN, /**< Return instruction type. */
    DR_INDIRECT_CALL,   /**< Indirect call instruction type. */
    DR_INDIRECT_JUMP,   /**< Indirect jump instruction type. */
} dr_indirect_branch_type_t;
/* DR_API EXPORT END */

DR_API
/**
 * Intended to augment dr_prepopulate_cache() by populating DR's indirect branch
 * tables, avoiding trips back to the dispatcher during initial execution.  This is only
 * effective when one of the the runtime options -shared_trace_ibt_tables and
 * -shared_bb_ibt_tables (depending on whether traces are enabled) is turned on, as
 * this routine does not try to populate tables belonging to threads other than the
 * calling thread.
 *
 * This is meant to be called between dr_app_setup() and dr_app_start(), immediately
 * after calling dr_prepopulate_cache().  It adds entries for each target address in
 * the \p tags array to the indirect branch table for the branch type \p branch_type.
 *
 * Returns whether successful.
 */
bool
dr_prepopulate_indirect_targets(dr_indirect_branch_type_t branch_type, app_pc *tags,
                                size_t tags_count);

DR_API
/**
 * Retrieves various statistics exported by DR as global, process-wide values.
 * The API is not thread-safe.
 * The caller is expected to pass a pointer to a valid, initialized dr_stats_t
 * value, with the size field set (see dr_stats_t).
 * Returns false if stats are not enabled.
 */
bool
dr_get_stats(dr_stats_t *drstats);

#    ifdef CUSTOM_TRACES
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * CUSTOM TRACE SUPPORT
 */

/* DR_API EXPORT END */

DR_API
/**
 * Marks the fragment associated with tag \p tag as
 * a trace head.  The fragment need not exist yet -- once it is
 * created it will be marked as a trace head.
 *
 * DR associates a counter with a trace head and once it
 * passes the -hot_threshold parameter, DR begins building
 * a trace.  Before each fragment is added to the trace, DR
 * calls the client's end_trace callback to determine whether
 * to end the trace.  (The callback will be called both for
 * standard DR traces and for client-defined traces.)
 *
 * \note Some fragments are unsuitable for trace heads. DR will
 * ignore attempts to mark such fragments as trace heads and will return
 * false. If the client marks a fragment that doesn't exist yet as a trace
 * head and DR later determines that the fragment is unsuitable for
 * a trace head it will unmark the fragment as a trace head without
 * notifying the client.
 *
 * \note Some fragments' notion of trace heads is dependent on
 * which previous block targets them.  For these fragments, calling
 * this routine will only mark as a trace head for targets from
 * the same memory region.
 *
 * Returns true if the target fragment is marked as a trace head.
 */
bool
dr_mark_trace_head(void *drcontext, void *tag);

DR_API
/**
 * Checks to see if the fragment (or future fragment) with tag \p tag
 * is marked as a trace head.
 */
bool
dr_trace_head_at(void *drcontext, void *tag);

DR_API
/** Checks to see that if there is a trace in the code cache at tag \p tag. */
bool
dr_trace_exists_at(void *drcontext, void *tag);
#    endif /* CUSTOM_TRACES */

#endif /* CLIENT_INTERFACE */

/****************************************************************************
 * proc.c routines exported here due to proc.h being in arch_exports.h
 * which is included in places where opnd_t isn't a complete type.
 * These are used for dr_insert_clean_call() and thus are not just CLIENT_INTERFACE.
 */
/* DR_API EXPORT TOFILE dr_proc.h */
DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save the
 * floating point state into the 16-byte-aligned buffer referred to by
 * \p buf, which must be 512 bytes for processors with the FXSR
 * feature, and 108 bytes for those without (where this routine does
 * not support 16-bit operand sizing).  \p buf should have size of
 * OPSZ_512; this routine will automatically adjust it to OPSZ_108 if
 * necessary.  \note proc_fpstate_save_size() can be used to determine
 * the particular size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 *
 * The last floating-point instruction address is left in an
 * untranslated state (i.e., it may point into the code cache).
 */
void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the
 * floating point state from the 16-byte-aligned buffer referred to by
 * buf, which must be 512 bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not
 * support 16-bit operand sizing).  \p buf should have size of
 * OPSZ_512; this routine will automatically adjust it to OPSZ_108 if
 * necessary.  \note proc_fpstate_save_size() can be used to determine
 * the particular size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 */
void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t buf);

DR_API
/**
 * Insert code to get the segment base address pointed to by seg into
 * register reg. In Linux, it is only supported with -mangle_app_seg option.
 * In Windows, it only supports getting base address of the TLS segment.
 *
 * \return whether successful.
 */
bool
dr_insert_get_seg_base(void *drcontext, instrlist_t *ilist, instr_t *instr, reg_id_t seg,
                       reg_id_t reg);

/* DR_API EXPORT TOFILE dr_events.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * PERSISTENT CACHE SUPPORT
 */
/* DR_API EXPORT END */

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

#endif /* _INSTRUMENT_API_H_ */
