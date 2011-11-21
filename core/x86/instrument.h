/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
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
 * instrument.h - interface for instrumentation
 */

#ifndef _INSTRUMENT_H_
#define _INSTRUMENT_H_ 1

#include "../globals.h"
#include "../module_shared.h"
#include "arch.h"
#include "instr.h"

/* xref _USES_DR_VERSION_ in dr_api.h (PR 250952) and compatibility
 * check in instrument.c (OLDEST_COMPATIBLE_VERSION, etc.).
 * this is defined outside of CLIENT_INTERFACE b/c it's used for
 * a general tracedump version as well.
 */
#define CURRENT_API_VERSION VERSION_NUMBER_INTEGER

#ifdef CLIENT_INTERFACE

/* DR_API EXPORT TOFILE dr_events.h */
/* DR_API EXPORT BEGIN */

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
 * result in the client library being reloaded and its dr_init()
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
 * into the code cache.  For multiple clients, the flags returned by each
 * client are or-ed together.
 */
typedef enum {
    /** Emit as normal. */
    DR_EMIT_DEFAULT              =    0,
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
    DR_EMIT_STORE_TRANSLATIONS   = 0x01,
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
 * - If there is more than one non-meta branch, only the last can be
 * conditional.
 * - A non-meta conditional branch or direct call must be the final
 * instruction in the block.
 * - There can only be one indirect branch (call, jump, or return) in
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
 * instructions: see #instr_set_ok_to_mangle()) (which should not fault) and
 * is not modifying, reordering, or removing application instructions,
 * these details can be ignored.  In that case the client should return
 * #DR_EMIT_DEFAULT and set up its basic block callback to be deterministic
 * and idempotent.  If the client is performing modifications, then in
 * order for DR to properly translate a code cache address the client must
 * use #instr_set_translation() in the basic block creation callback to set
 * the corresponding application address (the address that should be
 * presented to the application as the faulting address, or the address
 * that should be restarted after a suspend) for each modified instruction
 * and each added non-meta instruction (see #instr_set_ok_to_mangle()).
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
 *    be called for non-meta instructions even when \p translating is
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
 * A NULL value instructs DR to use the subsequent non-meta
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
 * visibility.
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
 */
void
dr_register_bb_event(dr_emit_flags_t (*func)
                     (void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating));

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
bool
dr_unregister_bb_event(dr_emit_flags_t (*func)
                       (void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating));

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
 * The user is free to inspect and modify the trace before it
 * executes, with certain restrictions on introducing control-flow
 * that include those for basic blocks (see dr_register_bb_event()).
 * Additional restrictions unique to traces also apply:
 * - Only one non-meta direct branch that targets the subsequent block
 *   in the trace can be present in each block.
 * - Each block must end with a non-meta control transfer.
 * - The parameter to a system call, normally kept in the eax register, 
 *   cannot be changed.
 * - A system call or interrupt instruction cannot be added.
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
 * additional non-meta control transfers.
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
void
dr_register_trace_event(dr_emit_flags_t (*func)
                        (void *drcontext, void *tag, instrlist_t *trace,
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
bool
dr_unregister_trace_event(dr_emit_flags_t (*func)
                          (void *drcontext, void *tag, instrlist_t *trace,
                           bool translating));

#ifdef CUSTOM_TRACES
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
void
dr_register_end_trace_event(dr_custom_trace_action_t (*func)
                            (void *drcontext, void *tag, void *next_tag));

DR_API
/**
 * Unregister a callback function for the end-trace event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_end_trace_event(dr_custom_trace_action_t (*func)
                              (void *drcontext, void *tag, void *next_tag));
#endif

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
 * #instr_ok_to_mangle()) that does not reference application memory,
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
 * The client can update \p mcontext.pc in this callback.
 *
 * \note The passed-in \p drcontext may correspond to a different thread
 * than the thread executing the callback.  Do not assume that the
 * executing thread is the target thread.
 */
void
dr_register_restore_state_event(void (*func)
                                (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                 bool restore_memory, bool app_code_consistent));

DR_API
/**
 * Unregister a callback function for the machine state restoration event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_restore_state_event(void (*func)
                                  (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                   bool restore_memory, bool app_code_consistent));

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
    /** The application machine state at the translation point. */
    dr_mcontext_t *mcontext;
    /** Whether raw_mcontext is valid. */
    bool raw_mcontext_valid;
    /** 
     * The raw pre-translated machine state at the translation
     * interruption point inside the code cache.  Clients are
     * cautioned when examining code cache instructions to not rely on
     * any details of code inserted other than their own.
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
dr_register_restore_state_ex_event(bool (*func) (void *drcontext, bool restore_memory,
                                                 dr_restore_state_info_t *info));

DR_API
/**
 * Unregister a callback function for the machine state restoration
 * event with extended ifnormation.  \return true if unregistration is
 * successful and false if it is not (e.g., \p func was not
 * registered).
 */
bool
dr_unregister_restore_state_ex_event(bool (*func) (void *drcontext, bool restore_memory,
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
    DR_EXIT_MULTI_THREAD           = 0x01,
    /**
     * Do not invoke thread exit event callbacks at process exit time.
     * Thread exit event callbacks will still be invoked at other times.
     * This is equivalent to setting the \p -skip_thread_exit_at_exit
     * runtime option.  Setting this flag can improve process exit
     * performance, but usually only when the #DR_EXIT_MULTI_THREAD flag is
     * also set, or when no process exit event is registered.
     */
    DR_EXIT_SKIP_THREAD_EXIT        = 0x02,
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

#ifdef LINUX
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
 * \p func whenever the application loads a module.  The \p loaded
 * parameter indicates whether the module is about to be loaded (the
 * normal case) or is already loaded (if the module was already there
 * at the time DR initialized). \note The client should be aware that
 * if the module is being loaded it may not be fully processed by the
 * loader (relocating, rebinding and on Linux segment remapping may
 * have not yet occurred). \note The module_data_t \p *info passed
 * to the callback routine is valid only for the duration of the
 * callback and should not be freed; a persistent copy can be made with
 * dr_copy_module_data().
 */
void
dr_register_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                           bool loaded));

DR_API
/**
 * Unregister a callback for the module load event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
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
dr_register_module_unload_event(void (*func)(void *drcontext,
                                             const module_data_t *info));

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
#ifdef WINDOWS
/* DR_API EXPORT END */

/* DR_API EXPORT BEGIN */
/**
 * Data structure passed with an exception event.  Contains the
 * machine context and the Win32 exception record.
 */
typedef struct _dr_exception_t {
    dr_mcontext_t *mcontext;   /**< Machine context at exception point. */
    EXCEPTION_RECORD *record; /**< Win32 exception record. */
    /** 
     * The raw pre-translated machine state at the exception interruption
     * point inside the code cache.  Clients are cautioned when examining
     * code cache instructions to not rely on any details of code inserted
     * other than their own.
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
#endif /* WINDOWS */
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
 * If \p func returns true, the application's system call is invoked
 * normally; if \p func returns false, the system call is skipped.  If
 * it is skipped, the return value can be set with
 * dr_syscall_set_result().  If the system call is skipped, there will
 * not be a post-syscall event.
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
 * intercept the system call.  The result of the system call can be
 * modified with dr_syscall_set_result().
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

#ifdef LINUX
/* DR_API EXPORT END */

/* FIXME: for PR 304708 I originally included siginfo_t in
 * dr_siginfo_t.  But can we really trust siginfo_t to be identical on
 * all supported platforms?  Esp. once we start supporting VMKUW,
 * MacOS, etc.  I'm removing it for now.  None of my samples need it,
 * and in my experience its fields are unreliable in any case.
 * PR 371370 covers re-adding it if users request it.
 * Xref PR 371339: we will need to not include it through signal.h but
 * instead something like this:
 *   #  define __need_siginfo_t
 *   #  include <bits/siginfo.h>
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
    /** The application machine state at the signal interruption point. */
    dr_mcontext_t *mcontext;
    /** 
     * The raw pre-translated machine state at the signal interruption
     * point inside the code cache.  NULL for delayable signals.  Clients
     * are cautioned when examining code cache instructions to not rely on
     * any details of code inserted other than their own.
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
void
dr_register_signal_event(dr_signal_action_t (*func)
                         (void *drcontext, dr_siginfo_t *siginfo));

DR_API
/**
 * Unregister a callback function for the signal event.
 * \return true if unregistration is successful and false if it is not
 * (e.g., \p func was not registered).
 */
bool
dr_unregister_signal_event(dr_signal_action_t (*func)
                           (void *drcontext, dr_siginfo_t *siginfo));
/* DR_API EXPORT BEGIN */
#endif /* LINUX */
/* DR_API EXPORT END */


/****************************************************************************
 * SECURITY SUPPORT
 */

#ifdef PROGRAM_SHEPHERDING
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
#endif /* PROGRAM_SHEPHERDING */


DR_API
/**
 * Registers a callback function for nudge events.  External entities
 * can nudge a process through the dr_nudge_process() API routine on
 * Windows or using the \p nudgeunix tool on Linux.  DR then calls \p
 * func whenever the current process receives the nudge.  On Windows,
 * the nudge event is delivered in a new non-application thread.
 * Callers must specify the target client by passing the client ID
 * that was provided in dr_init().
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
 *
 * \note Not yet supported for 32-bit processes running on 64-bit Windows (WOW64).
 */
bool
dr_nudge_client(client_id_t id, uint64 argument);


void instrument_load_client_libs(void);
void instrument_init(void);
void instrument_exit(void);
bool is_in_client_lib(app_pc addr);
bool get_client_bounds(client_id_t client_id,
                       app_pc *start/*OUT*/, app_pc *end/*OUT*/);
const char *get_client_path_from_addr(app_pc addr);
bool is_valid_client_id(client_id_t id);
void instrument_thread_init(dcontext_t *dcontext, bool client_thread, bool valid_mc);
void instrument_thread_exit_event(dcontext_t *dcontext);
void instrument_thread_exit(dcontext_t *dcontext);
#ifdef LINUX
void instrument_fork_init(dcontext_t *dcontext);
#endif
bool instrument_basic_block(dcontext_t *dcontext, app_pc tag, instrlist_t *bb,
                            bool for_trace, bool translating,
                            dr_emit_flags_t *emitflags);
dr_emit_flags_t instrument_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace,
                                 bool translating);
#ifdef CUSTOM_TRACES
dr_custom_trace_action_t instrument_end_trace(dcontext_t *dcontext, app_pc trace_tag,
                                           app_pc next_tag);
#endif
void instrument_fragment_deleted(dcontext_t *dcontext, app_pc tag, uint flags);
bool instrument_restore_state(dcontext_t *dcontext, bool restore_memory,
                              dr_restore_state_info_t *info);

module_data_t * copy_module_area_to_module_data(const module_area_t *area);
void instrument_module_load_trigger(app_pc modbase);
void instrument_module_load(module_data_t *data, bool previously_loaded);
void instrument_module_unload(module_data_t *data);

/* returns whether this sysnum should be intercepted */
bool instrument_filter_syscall(dcontext_t *dcontext, int sysnum);
/* returns whether this syscall should execute */
bool instrument_pre_syscall(dcontext_t *dcontext, int sysnum);
void instrument_post_syscall(dcontext_t *dcontext, int sysnum);
bool instrument_invoke_another_syscall(dcontext_t *dcontext);

void instrument_nudge(dcontext_t *dcontext, client_id_t id, uint64 arg);
#ifdef WINDOWS
bool instrument_exception(dcontext_t *dcontext, dr_exception_t *exception);
void wait_for_outstanding_nudges(void);
#else
dr_signal_action_t instrument_signal(dcontext_t *dcontext, dr_siginfo_t *siginfo);
bool dr_signal_hook_exists(void);
#endif
int get_num_client_threads(void);
#ifdef PROGRAM_SHEPHERDING
void instrument_security_violation(dcontext_t *dcontext, app_pc target_pc,
                                   security_violation_t violation, action_type_t *action);
#endif

#endif /* CLIENT_INTERFACE */
bool dr_get_mcontext_priv(dcontext_t *dcontext, dr_mcontext_t *dmc, priv_mcontext_t *mc);
#ifdef CLIENT_INTERFACE

bool dr_bb_hook_exists(void);
bool dr_trace_hook_exists(void);
bool dr_fragment_deleted_hook_exists(void);
bool dr_end_trace_hook_exists(void);
bool dr_thread_exit_hook_exists(void);
bool dr_exit_hook_exists(void);
bool dr_xl8_hook_exists(void);
bool hide_tag_from_client(app_pc tag);

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
 */
void *
dr_standalone_init(void);

/* DR_API EXPORT BEGIN */

#ifdef API_EXPORT_ONLY
/**
 * Use this dcontext for use with the standalone static decoder library.
 * Pass it whenever a decoding-related API routine asks for a context.
 */
#define GLOBAL_DCONTEXT  ((void *)-1)
#endif

/**************************************************
 * UTILITY ROUTINES
 */

#ifdef WINDOWS
/**
 * If \p x is false, displays a message about an assertion failure
 * (appending \p msg to the message) and then calls dr_abort()
 */ 
# define DR_ASSERT_MSG(x, msg) \
    ((void)((!(x)) ? \
        (dr_messagebox("ASSERT FAILURE: %s:%d: %s (%s)", __FILE__,  __LINE__, #x, msg),\
         dr_abort(), 0) : 0))
#else
# define DR_ASSERT_MSG(x, msg) \
    ((void)((!(x)) ? \
        (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", \
                     __FILE__,  __LINE__, #x, msg), \
         dr_abort(), 0) : 0))
#endif

/**
 * If \p x is false, displays a message about an assertion failure and
 * then calls dr_abort()
 */ 
#define DR_ASSERT(x) DR_ASSERT_MSG(x, "")

/* DR_API EXPORT END */

DR_API
/** Returns true if all \DynamoRIO caches are thread private. */
bool
dr_using_all_private_caches(void);

DR_API
/** \deprecated Replaced by dr_set_process_exit_behavior() */
void
dr_request_synchronized_exit(void);

DR_API
/** 
 * Returns the client-specific option string specified at client
 * registration.  \p client_id is the client ID passed to dr_init().
 */
const char *
dr_get_options(client_id_t client_id);

DR_API 
/**
 * Returns the client library name and path that were originally specified
 * to load the library.  If the resulting string is longer than #MAXIMUM_PATH
 * it will be truncated.  \p client_id is the client ID passed to a client's
 * dr_init() function.
 */
const char *
dr_get_client_path(client_id_t client_id);

DR_API 
/** Returns the image name (without path) of the current application. */
const char *
dr_get_application_name(void);

DR_API 
/** Returns the process id of the current process. */
process_id_t
dr_get_process_id(void);

#ifdef LINUX
DR_API 
/**
 * Returns the process id of the parent of the current process. 
 * \note Linux only.
 */
process_id_t
dr_get_parent_id(void);
#endif

#ifdef WINDOWS
/* DR_API EXPORT BEGIN */
/** Windows versions */
typedef enum {
    DR_WINDOWS_VERSION_7     = 61,
    DR_WINDOWS_VERSION_VISTA = 60,
    DR_WINDOWS_VERSION_2003  = 52,
    DR_WINDOWS_VERSION_XP    = 51,
    DR_WINDOWS_VERSION_2000  = 50,
    DR_WINDOWS_VERSION_NT    = 40,
} dr_os_version_t;

/** Data structure used with dr_get_os_version() */
typedef struct _dr_os_version_info_t {
    /** The size of this structure.  Set this to sizeof(dr_os_version_info_t). */
    size_t size;
    /** The operating system version */
    dr_os_version_t version;
} dr_os_version_info_t;
/* DR_API EXPORT END */

DR_API
/** 
 * Returns information about the version of the operating system.
 * Returns whether successful.
 */
bool
dr_get_os_version(dr_os_version_info_t *info);

DR_API
/** 
 * Returns true if this process is a 32-bit process operating on a
 * 64-bit Windows kernel, known as Windows-On-Windows-64, or WOW64.
 * Returns false otherwise.
 */
bool
dr_is_wow64(void);

DR_API
/** 
 * Returns a pointer to the application's Process Environment Block
 * (PEB).  DR swaps to a private PEB when running client code, in
 * order to isolate the client and its dependent libraries from the
 * application, so conventional methods of reading the PEB will obtain
 * the private PEB instead of the application PEB.
 */
void *
dr_get_app_PEB(void);
#endif

DR_API
/** Retrieves the current time. */
void
dr_get_time(dr_time_t *time);

DR_API
/**
 * On Linux, returns the number of milliseconds since the Epoch (Jan 1, 1970).
 * On Windows, returns the number of milliseconds since Jan 1, 1600 (this is
 * the current UTC time).
 */
uint64
dr_get_milliseconds(void);

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
/** Aborts the process immediately. */
void
dr_abort(void);

/* DR_API EXPORT BEGIN */

/**************************************************
 * APPLICATION-INDEPENDENT MEMORY ALLOCATION
 */
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

#ifdef LINUX
DR_API
/** 
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of malloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option to
 * replace a client's use of malloc, realloc, and free with internal
 * versions that allocate memory from DR's private pool.  With -wrap,
 * clients can link to libraries that allocate heap memory without
 * interfering with application allocations.
 * \note Currently Linux only.
 */
void *
__wrap_malloc(size_t size);

DR_API
/** 
 * Reallocates memory from DR's global memory pool, but mimics the
 * behavior of realloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 * \note Currently Linux only.
 */
void *
__wrap_realloc(void *mem, size_t size);

DR_API
/**
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of calloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 * \note Currently Linux only.
 */
void *
__wrap_calloc(size_t nmemb, size_t size);

DR_API
/**
 * Frees memory from DR's global memory pool.  Memory must have been
 * allocated with __wrap_malloc(). The __wrap routines are intended to
 * be used with ld's -wrap option; see __wrap_malloc() for more
 * information.
 * \note Currently Linux only.
 */
void
__wrap_free(void *mem);
#endif

/* DR_API EXPORT BEGIN */

/**************************************************
 * MEMORY QUERY/ACCESS ROUTINES
 */
/* DR_API EXPORT END */

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
 */
bool
dr_query_memory(const byte *pc, byte **base_pc, size_t *size, uint *prot);

DR_API
/**
 * Provides additional information beyond dr_query_memory().
 * Returns true if it was able to obtain information (including about
 * free regions) and sets the fields of \p info.  This routine can be
 * used to iterate over the entire address space.
 * Returns false on failure.
 *
 * \note To examine only application memory, skip memory for which
 * dr_memory_is_dr_internal() returns true.
 */
bool
dr_query_memory_ex(const byte *pc, OUT dr_mem_info_t *info);

/* DR_API EXPORT BEGIN */
#ifdef WINDOWS 
/* DR_API EXPORT END */
/* NOTE - see fixme for dr_query_memory - PR 198873. */
DR_API
/**
 * Equivalent to the win32 API function VirtualQuery().
 * See that routine for a description of
 * arguments and return values.  \note Windows-only.
 */
size_t
dr_virtual_query(const byte *pc, MEMORY_BASIC_INFORMATION *mbi, size_t mbi_size);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API
/**
 * Safely reads \p size bytes from address \p base into buffer \p
 * out_buf.  Reading is done without the possibility of an exception
 * occurring.  Optionally returns the actual number of bytes copied
 * into \p bytes_read.  Returns true if successful.  
 * \note See also DR_TRY_EXCEPT().
 */
bool
dr_safe_read(const void *base, size_t size, void *out_buf, size_t *bytes_read);

DR_API
/**
 * Safely writes \p size bytes from buffer \p in_buf to address \p
 * base.  Writing is done without the possibility of an exception
 * occurring.  Optionally returns the actual number of bytes copied
 * into \p bytes_written.  Returns true if successful.
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
 * For fault-free reads or writes in isolation, use dr_safe_read() or
 * dr_safe_write() instead, although on Windows those operations
 * invoke a system call and this construct can be more performant.
 */
#define DR_TRY_EXCEPT(drcontext, try_statement, except_statement) do {\
    void *try_cxt;                                                    \
    dr_try_setup(drcontext, &try_cxt);                                \
    if (dr_try_start(try_cxt) == 0) {                                 \
        try_statement                                                 \
        dr_try_stop(drcontext, try_cxt);                              \
    } else {                                                          \
        /* roll back first in case except faults or returns */        \
        dr_try_stop(drcontext, try_cxt);                              \
        except_statement                                              \
    }                                                                 \
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
 * Returns true iff pc is located inside a client library.
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
 */
dr_auxlib_handle_t
dr_load_aux_library(const char *name,
                    byte **lib_start /*OPTIONAL OUT*/,
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

/**************************************************
 * SIMPLE MUTEX SUPPORT
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
 * cache.  Failing to follow these restrictions can lead to deadlocks.
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
/** Unlocks \p mutex.  Asserts that mutex is currently locked. */
void
dr_mutex_unlock(void *mutex);

DR_API 
/** Tries once to lock \p mutex, returns whether or not successful. */
bool
dr_mutex_trylock(void *mutex);

#ifdef DEBUG
DR_API
/**
 * Returns true iff \p mutex is owned by the calling thread.
 * This routine is only available in debug builds.
 */
bool
dr_mutex_self_owns(void *mutex);
#endif

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

/* DR_API EXPORT BEGIN */
/**************************************************
 * MODULE INFORMATION ROUTINES
 */

/** For dr_module_iterator_* interface */
typedef void * dr_module_iterator_t;

#ifdef AVOID_API_EXPORT
/* We always give copies of the module_area_t information to clients (in the form
 * of a module_data_t defined below) to avoid locking issues (see PR 225020). */
/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
#endif
#ifdef LINUX
/** Holds information on a segment of a loaded module. */
typedef struct _module_segment_data_t {
    app_pc start; /**< Start address of the segment, page-aligned backward. */
    app_pc end;   /**< End address of the segment, page-aligned forward. */
    uint prot;    /**< Protection attributes of the segment */
} module_segment_data_t;
#endif

/**
 * Holds information about a loaded module. \note On Linux the start address can be
 * cast to an Elf32_Ehdr or Elf64_Ehdr. \note On Windows the start address can be cast to
 * an IMAGE_DOS_HEADER for use in finding the IMAGE_NT_HEADER and its OptionalHeader.
 * The OptionalHeader can be used to walk the module sections (among other things).
 * See WINNT.H. \note When accessing any memory inside the module (including header fields)
 * user is responsible for guarding against corruption and the possibility of the module
 * being unmapped.
 */
struct _module_data_t {
    union {
        app_pc start; /**< starting address of this module */
        module_handle_t handle; /**< module_handle for use with dr_get_proc_address() */
    } ; /* anonymous union of start address and module handle */
    /**
     * Ending address of this module.  Note that on Linux the module may not
     * be contiguous: there may be gaps containing other objects between start
     * and end.  Use the segments array to examine each mapped region on Linux.
     */
    app_pc end;

    app_pc entry_point; /**< entry point for this module as specified in the headers */

    uint flags; /**< Reserved, set to 0 */

    module_names_t names; /**< struct containing name(s) for this module; use
                           * dr_module_preferred_name() to get the preferred name for
                           * this module */

    char *full_path; /**< full path to the file backing this module */

#ifdef WINDOWS
    version_number_t file_version; /**< file version number from .rsrc section */
    version_number_t product_version; /**< product version number from .rsrc section */
    uint checksum; /**< module checksum from the PE headers */
    uint timestamp; /**< module timestamp from the PE headers */
    size_t module_internal_size; /**< module internal size (from PE headers SizeOfImage) */
#else
    bool contiguous;   /**< whether there are no gaps between segments */
    uint num_segments; /**< number of segments */
    /**
     * Array of num_segments entries, one per segment.  The array is sorted
     * by the start address of each segment.
     */
    module_segment_data_t *segments;
#endif
#ifdef AVOID_API_EXPORT
    /* FIXME: PR 215890: ELF64 size? Anything else? */
    /* We can add additional fields to the end without breaking compatibility */
#endif
};

/* DR_API EXPORT END */

DR_API
/**
 * Looks up the module containing \p pc.  If a module containing \p pc is found returns
 * a module_data_t describing that module else returns NULL.  Can be used to
 * obtain a module_handle_t for dr_lookup_module_section().
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

/* DR_API EXPORT BEGIN */
#ifdef WINDOWS
/* DR_API EXPORT END */
DR_API
/**
 * Returns whether \p pc is within a section within the module in \p section_found and
 * information about that section in \p section_out. \note Not yet available on Linux.
 */
bool
dr_lookup_module_section(module_handle_t lib,
                         byte *pc, IMAGE_SECTION_HEADER *section_out);

/* DR_API EXPORT BEGIN */
#endif /* WINDOWS */
/* DR_API EXPORT END */

DR_API
/**
 * Returns the entry point of the exported function with the given
 * name in the module with the given base.  Returns NULL on failure.
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
 * \note On Linux this ignores symbol preemption by other modules and only
 * examines the specified module.
 */
bool
dr_get_proc_address_ex(module_handle_t lib, const char *name,
                       dr_export_info_t *info OUT, size_t info_len);


/* DR_API EXPORT BEGIN */
/**************************************************
 * SYSTEM CALL PROCESSING ROUTINES
 */
/* DR_API EXPORT END */

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) 
 * event.  Returns the value of system call parameter number \p param_num.
 */
reg_t
dr_syscall_get_param(void *drcontext, int param_num);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event, or from a post-syscall (dr_register_post_syscall_event())
 * event when also using dr_syscall_invoke_another().  Sets the value
 * of system call parameter number \p param_num to \p new_value.
 */
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  Returns the return value of the system call that will be
 * presented to the application.
 */
reg_t
dr_syscall_get_result(void *drcontext);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) or
 * post-syscall (dr_register_post_syscall_event()) event.
 * For pre-syscall, should only be used when skipping the system call.
 * This sets the return value of the system call that the application sees
 * to \p value.
 */
void
dr_syscall_set_result(void *drcontext, reg_t value);

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
 */
bool
dr_create_dir(const char *fname);

DR_API
/** Checks for the existence of a directory. */
bool
dr_directory_exists(const char *fname);

DR_API
/** Checks the existence of a file. */
bool
dr_file_exists(const char *fname);

/* The extra BEGIN END is to get spacing nice. */
/* DR_API EXPORT BEGIN */
/* DR_API EXPORT END */
/* DR_API EXPORT BEGIN */
/* flags for use with dr_open_file() */
/** Open with read access. */
#define DR_FILE_READ               0x1
/** Open with write access, but do not open if the file already exists. */
#define DR_FILE_WRITE_REQUIRE_NEW  0x2
/**
 * Open with write access.  If the file already exists, set the file position to the
 * end of the file.
 */
#define DR_FILE_WRITE_APPEND       0x4
/**
 * Open with write access.  If the file already exists, truncate the
 * file to zero length.
 */
#define DR_FILE_WRITE_OVERWRITE    0x8
/**
 * Open with large (>2GB) file support.  Only applicable on 32-bit Linux.
 * \note DR's log files and tracedump files are all created with this flag.
 */
#define DR_FILE_ALLOW_LARGE       0x10
/** Linux-only.  This file will be closed in the child of a fork. */
#define DR_FILE_CLOSE_ON_FORK     0x20
/* DR_API EXPORT END */

DR_API 
/**
 * Opens the file \p fname. If no such file exists then one is created.
 * The file access mode is set by the \p mode_flags argument which is drawn from
 * the DR_FILE_* defines ORed together.  Returns INVALID_FILE if unsuccessful.
 *
 * On Windows, \p fname must be an absolute path (when using Windows
 * system calls directly there is no such thing as a relative path.
 * On Windows the notions of current directory and relative paths are
 * limited to user space via the Win32 API.  We may add limited
 * support for using the same current directory via Issue 298.)
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
 * The extra BEGIN END is to get spacing nice. Once we have more control over the layout
 * of the API header files share with os_shared.h. */
/* DR_API EXPORT BEGIN */
/* DR_API EXPORT END */
/* DR_API EXPORT BEGIN */
/* For use with dr_file_seek(), specifies the origin at which to apply the offset. */
#define DR_SEEK_SET 0  /**< start of file */
#define DR_SEEK_CUR 1  /**< current file position */
#define DR_SEEK_END 2  /**< end of file */
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
/**
 * If set, changes to mapped memory are private to the mapping process and
 * are not reflected in the underlying file.  If not set, changes are visible
 * to other processes that map the same file, and will be propagated
 * to the file itself.
 */
#define DR_MAP_PRIVATE               0x1
#ifdef LINUX
/**
 * If set, indicates that the passed-in start address is required rather than a
 * hint.  On Linux, this has the same semantics as mmap with MAP_FIXED: i.e.,
 * any existing mapping in [addr,addr+size) will be unmapped.  This flags is not
 * supported on Windows.
 */
#define DR_MAP_FIXED                 0x2
#endif
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
 */
bool
dr_unmap_file(void *map, size_t size);

/* TODO add delete_file, rename/move_file, copy_file, get_file_size, truncate_file etc.
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
 * The mask constants are below.
 * Logging is disabled for the release build.
 * If \p drcontext is NULL, writes to the main log file.
 */
void
dr_log(void *drcontext, uint mask, uint level, const char *fmt, ...);

/* hack to get these constants in the right place in dr_ir_api.h */
#if 0 /* now skip a line to prevent gen_api.pl from removing this */

/* DR_API EXPORT BEGIN */

/* The log mask constants */
#define LOG_NONE           0x00000000  /**< Log no data. */                              
#define LOG_STATS          0x00000001  /**< Log per-thread and global statistics. */     
#define LOG_TOP            0x00000002  /**< Log top-level information. */                
#define LOG_THREADS        0x00000004  /**< Log data related to threads. */              
#define LOG_SYSCALLS       0x00000008  /**< Log data related to system calls. */         
#define LOG_ASYNCH         0x00000010  /**< Log data related to signals/callbacks/etc. */
#define LOG_INTERP         0x00000020  /**< Log data related to app interpretation. */   
#define LOG_EMIT           0x00000040  /**< Log data related to emitting code. */        
#define LOG_LINKS          0x00000080  /**< Log data related to linking code. */         
#define LOG_CACHE          0x00000100  /**< Log data related to code cache management. */
#define LOG_FRAGMENT       0x00000200  /**< Log data related to app code fragments. */   
#define LOG_DISPATCH       0x00000400  /**< Log data on every context switch dispatch. */
#define LOG_MONITOR        0x00000800  /**< Log data related to trace building. */       
#define LOG_HEAP           0x00001000  /**< Log data related to memory management. */     
#define LOG_VMAREAS        0x00002000  /**< Log data related to address space regions. */
#define LOG_SYNCH          0x00004000  /**< Log data related to synchronization. */      
#define LOG_MEMSTATS       0x00008000  /**< Log data related to memory statistics. */    
#define LOG_OPTS           0x00010000  /**< Log data related to optimizations. */        
#define LOG_SIDELINE       0x00020000  /**< Log data related to sideline threads. */ 
#define LOG_SYMBOLS        0x00040000  /**< Log data related to app symbols. */ 
#define LOG_RCT            0x00080000  /**< Log data related to indirect transfers. */ 
#define LOG_NT             0x00100000  /**< Log data related to Windows Native API. */ 
#define LOG_HOT_PATCHING   0x00200000  /**< Log data related to hot patching. */ 
#define LOG_HTABLE         0x00400000  /**< Log data related to hash tables. */ 
#define LOG_MODULEDB       0x00800000  /**< Log data related to the module database. */ 
#define LOG_ALL            0x00ffffff  /**< Log all data. */
/* DR_API EXPORT END */
#endif


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

#ifdef PROGRAM_SHEPHERDING
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
#endif /* PROGRAM_SHEPHERDING */

/* DR_API EXPORT BEGIN */
#ifdef WINDOWS 
/* DR_API EXPORT END */
DR_API
/** 
 * Displays a message in a pop-up window. \note Windows only. \note On Windows Vista
 * most Windows services are unable to display message boxes.
 */
void
dr_messagebox(const char *fmt, ...);
/* DR_API EXPORT BEGIN */
#endif
/* DR_API EXPORT END */

DR_API 
/**
 * Stdout printing that won't interfere with the
 * application's own printing.  Currently non-buffered.
 * \note On Windows, this routine is not able to print to the cmd window
 * (issue 261).  The drsym_write_to_console() routine in the \p drsyms
 * Extension can be used to accomplish that.
 * \note On Windows, this routine does not support printing floating
 * point values.  Use dr_snprintf() instead.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.
 */
void 
dr_printf(const char *fmt, ...);

DR_API
/**
 * Printing to a file that won't interfere with the
 * application's own printing.  Currently non-buffered.
 * \note On Windows, this routine is not able to print to STDOUT
 * or STDERR in the cmd window (issue 261).  The
 * drsym_write_to_console() routine in the \p drsyms Extension can be
 * used to accomplish that.
 * \note On Windows, this routine does not support printing floating
 * point values.  Use dr_snprintf() instead.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.  Use dr_write_file() to print large buffers.
 * \note On Linux this routine does not check for errors like EINTR.  Use
 * dr_write_file() if that is a concern.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state.
 */
void
dr_fprintf(file_t f, const char *fmt, ...);

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
 * \note This routine does not support printing wide characters.  On
 * Windows you can use _snprintf() instead (though _snprintf() does
 * not support printing floating point values).
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state.
 */
int
dr_snprintf(char *buf, size_t max, const char *fmt, ...);

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
 * Get DR's segment base pointed at \p segment_register.
 * It can be used to get the base of thread-local storage segment
 * used by #dr_raw_tls_calloc.
 *
 * \note It should not be called on thread exit event, 
 * as the thread exit event may be invoked from other threads.
 * See #dr_register_thread_exit_event for details.
 */
void *
dr_get_dr_segment_base(IN reg_id_t segment_register);

DR_API
/**
 * Allocates \p num_slots contiguous thread-local storage slots that
 * can be directly accessed via an offset from \p segment_register.
 * These slots will be initialized to 0 for each new thread.
 * The slot offsets are [\p offset .. \p offset + (num_slots - 1)].
 * These slots are disjoint from the #dr_spill_slot_t register spill slots
 * and the client tls field (dr_get_tls_field()).
 * Returns whether or not the slots were successfully obtained.
 * The segment base pointed at \p segment_register can be obtained
 * using #dr_get_dr_segment_base. 
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
 */
bool
dr_raw_tls_calloc(OUT reg_id_t *segment_register,
                  OUT uint *offset,
                  IN  uint num_slots,
                  IN  uint alignment);

DR_API
/**
 * Frees \p num_slots raw thread-local storage slots starting at
 * offset \p offset that were allocated with dr_raw_tls_calloc().
 * Returns whether or not the slots were successfully freed.
 */
bool
dr_raw_tls_cfree(uint offset, uint num_slots);


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
#ifdef CLIENT_SIDELINE
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
#endif /* CLIENT_SIDELINE */

DR_API
/** Current thread sleeps for \p time_ms milliseconds. */
void
dr_sleep(int time_ms);

DR_API
/** Current thread gives up its time quantum. */
void
dr_thread_yield(void);

/* FIXME - xref PR 227619 - some other event handler are safe (image_load/unload for*
 * example) which we could note here. */
DR_API
/**
 * Suspends all other threads in the process and returns an array of
 * contexts in \p drcontexts with one context per successfully suspended
 * threads.  The contexts can be passed to routines like dr_get_thread_id()
 * or dr_get_mcontext().  However, the contexts may not be modified:
 * dr_set_mcontext() is not supported.  dr_get_mcontext() can be called on
 * the caller of this routine, unless in a Windows nudge callback.
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
dr_suspend_all_other_threads(OUT void ***drcontexts,
                             OUT uint *num_suspended,
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
dr_resume_all_other_threads(IN void **drcontexts,
                            IN uint num_suspended);

/* We do not translate the context to avoid lock issues (PR 205795).
 * We do not delay until a safe point (via regular delayable signal path)
 * since some clients may want the interrupted context: for a general
 * timer clients should create a separate thread.
 */
#ifdef LINUX
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
 *   operations: keep it as simple as possible, and avoid lock usage or
 *   I/O operations. If a general timer that does not interrupt client code
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
#endif /* LINUX */

/* DR_API EXPORT TOFILE dr_ir_utils.h */
/* DR_API EXPORT BEGIN */

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
    SPILL_SLOT_1  =  0,  /** spill slot for register save/restore routines */
    SPILL_SLOT_2  =  1,  /** spill slot for register save/restore routines */
    SPILL_SLOT_3  =  2,  /** spill slot for register save/restore routines */
    SPILL_SLOT_4  =  3,  /** spill slot for register save/restore routines */
    SPILL_SLOT_5  =  4,  /** spill slot for register save/restore routines */
    SPILL_SLOT_6  =  5,  /** spill slot for register save/restore routines */
    SPILL_SLOT_7  =  6,  /** spill slot for register save/restore routines */
    SPILL_SLOT_8  =  7,  /** spill slot for register save/restore routines */
    SPILL_SLOT_9  =  8,  /** spill slot for register save/restore routines */
#ifdef X64
    SPILL_SLOT_10 =  9,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_11 = 10,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_12 = 11,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_13 = 12,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_14 = 13,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_15 = 14,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_16 = 15,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_17 = 16,  /** spill slot for register save/restore routines
                          * \note x64 only */
    SPILL_SLOT_MAX = SPILL_SLOT_17 /** Enum value of the last register save/restore
                                    *  spill slot. */
#else
    SPILL_SLOT_MAX = SPILL_SLOT_9 /** Enum value of the last register save/restore
                                   *  spill slot. */
#endif
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
 * cache.  Use at any other time could corrupt application or \DynamoRIO
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
 * \note At completion of the inserted instructions the saved flags are in the
 * xax register.  The xax register should not be modified after using this
 * routine unless it is first saved (and later restored prior to
 * using dr_restore_arith_flags()).
 */
void 
dr_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                    dr_spill_slot_t slot);

DR_API
/** 
 * Inserts into \p ilist prior to \p where meta-instruction(s) to restore the 6
 * arithmetic flags, assuming they were saved using dr_save_arith_flags() with
 * slot \p slot and that xax holds the same value it did after the save.
 */
void 
dr_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                       dr_spill_slot_t slot);

/* FIXME PR 315333: add routine that scans ahead to see if need to save eflags.  See 
 * forward_eflags_analysis(). */

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

/* to make our own code shorter */
#define MINSERT instrlist_meta_preinsert

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
 * Stores the application state information on the DR stack, where it can
 * be accessed from \c callee using dr_get_mcontext() and modified using
 * dr_set_mcontext().
 *
 * If \p save_fpstate is true, preserves the fp/mmx/sse state on the DR stack.
 * Note that it is relatively expensive to save this state (on the
 * order of 200 cycles) and that it typically takes 512 bytes to store
 * it (see proc_fpstate_save_size()).
 *
 * DR does support translating a fault in an argument (e.g., an
 * argument that references application memory); such a fault will be
 * treated as an application exception.
 *
 * The clean call sequence will be optimized based on the runtime option
 * \ref op_cleancall "-opt_cleancall".
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
dr_insert_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                     void *callee, bool save_fpstate, uint num_args, ...);

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
 * It is up to the caller of this routine to preserve caller-saved registers.
 *
 * DR does not support translating a fault in an argument.  For fault
 * transparency, the client must perform the translation (see
 * #dr_register_restore_state_event()), or use
 * #dr_insert_clean_call().
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
dr_insert_call(void *drcontext, instrlist_t *ilist, instr_t *where,
               void *callee, uint num_args, ...);

DR_API
/**
 * Inserts into \p ilist prior to \p where meta-instruction(s) to save state for a call.
 * Stores the application state information on the DR stack.
 * Returns the size of the data stored on the DR stack (in case the caller
 * needs to align the stack pointer).
 *
 * \warning This routine does NOT save the fp/mmx/sse state: to do that the
 * instrumentation routine should call proc_save_fpstate() to save and
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
 * esp the value saved by dr_swap_to_dr_stack().
 */
void
dr_restore_app_stack(void *drcontext, instrlist_t *ilist, instr_t *where);

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
dr_insert_call_instrumentation(void *drcontext, instrlist_t *ilist,
                               instr_t *instr, void *callee);

DR_API
/**
 * Assumes that \p instr is an indirect branch.
 * Inserts into \p ilist prior to \p instr instruction(s) to call callee passing
 * two arguments:
 * -# address of branch instruction
 * -# target address of branch
 * \note Only the address portion of a far indirect branch is considered.
 * \note \p scratch_slot must be <= dr_max_opnd_accessible_spill_slot(). \p scratch_slot
 * is used internally to this routine and will be clobbered.
 */
#ifdef AVOID_API_EXPORT
/* If we re-enable -opt_speed (or -indcall2direct directly) we should add back:
 * \note This routine is not supported when the -opt_speed option is specified.
 */
#endif
void 
dr_insert_mbr_instrumentation(void *drcontext, instrlist_t *ilist,
                              instr_t *instr, void *callee, dr_spill_slot_t scratch_slot);

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
dr_insert_cbr_instrumentation(void *drcontext, instrlist_t *ilist,
                              instr_t *instr, void *callee);

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
dr_insert_ubr_instrumentation(void *drcontext, instrlist_t *ilist,
                              instr_t *instr, void *callee);

DR_API
/**
 * Returns true if the xmm0 through xmm5 for Windows, or xmm0 through
 * xmm15 for 64-bit Linux, or xmm0 through xmm7 for 32-bit Linux, 
 * fields in dr_mcontext_t are valid for this process
 * (i.e., whether this process is 64-bit or WOW64, and the processor
 * supports SSE).
 */
bool
dr_mcontext_xmm_fields_valid(void);

#endif /* CLIENT_INTERFACE */
/* dr_get_mcontext() needed for translating clean call arg errors */

DR_API
/**
 * Copies the current application machine context to \p context.
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
 *
 * Does NOT copy the pc field, except for system call events, when it
 * will point at the post-syscall address.
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
 * \note NUM_XMM_SLOTS in the dr_mcontext_t.xmm array are filled in, but
 * only if dr_mcontext_fields_valid() returns true.
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
 * Sets the application machine context to \p context.
 * This routine may only be called from:
 * - A clean call invoked by dr_insert_clean_call() or dr_prepare_for_call() 
 * - A pre- or post-syscall event (dr_register_pre_syscall_event(), 
 *   dr_register_post_syscall_event()) 
 *   dr_register_thread_exit_event())
 * - Basic block or trace creation events (dr_register_bb_event(),
 *   dr_register_trace_event()), but for basic block creation only when the
 *   basic block callback parameters \p for_trace and \p translating are
 *   false, and for trace creation only when \p translating is false.
 *
 * Ignores the pc field.
 *
 * If the size field of \p context is invalid, this routine will
 * return false.  A dr_mcontext_t obtained from DR will have the size field set.
 *
 * \return whether successful.
 *
 * \note The xmm fields are only set for processes where the underlying
 * processor supports them.  For dr_insert_clean_call() that requested \p
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
 *
 * \note dr_get_mcontext() can be used to get the register state (except pc)
 * saved in dr_insert_clean_call() or dr_prepare_for_call()
 *
 * \note If floating point state was saved by dr_prepare_for_call() or
 * dr_insert_clean_call() it is not restored (other than the valid xmm fields
 * according to dr_mcontext_xmm_fields_valid()).  The caller should instead
 * manually save and restore the floating point state with proc_save_fpstate()
 * and proc_restore_fpstate() if necessary.
 *
 * \note If the caller wishes to set any other state (such as xmm
 * registers that are not part of the mcontext) they may do so by just
 * setting that state in the current thread before making this call.
 * To set system data structures, use dr_switch_to_app_state(), make
 * the changes, and then switch back with dr_switch_to_dr_state()
 * before calling this routine.
 *
 * \note This routine may only be called from a clean call from the cache. It can not be
 * called from any registered event callback.
 * \return false if unsuccessful; if successful, does not return.
 */
bool
dr_redirect_execution(dr_mcontext_t *mcontext);

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
 * code.  Subsequent executions will cause \DynamoRIO to reconstruct
 * the fragment, and therefore call the appropriate fragment-creation
 * event hook, if registered.
 *
 * \note This routine supports deletion for the current thread
 * only.  \p drcontext must be from the current thread and must
 * be the drcontext used to create the instruction list.
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
 * doesn't apparently overlap the flushed region). Instead the caller must call
 * dr_redirect_execution() after this routine to continue execution.  Returns true if
 * successful.
 *
 * \note This routine may not be called from any registered event callback other than
 * the nudge event or the pre- or post-system call event; clean calls
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
 * the nudge event or the pre- or post-system call event; clean calls
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
                      void (*flush_completion_callback) (int flush_id));

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
 */
app_pc
dr_app_pc_from_cache_pc(byte *cache_pc);

DR_API
/**
 * Returns whether the given thread indicated by \p drcontext
 * is currently using the application version of its system state.
 * \sa dr_switch_to_dr_state(), dr_switch_to_app_state()
 */
bool
dr_using_app_state(void *drcontext);

DR_API
/**
 * Swaps to the application version of any system state for the given
 * thread.  This is meant to be used prior to examining application
 * memory, when private libraries are in use and there are two
 * versions of system state.  Invoking non-DR library routines while
 * the application state is in place can lead to unpredictable
 * results: call dr_switch_to_dr_state() before doing so.
 */
void
dr_switch_to_app_state(void *drcontext);

DR_API
/**
 * Should only be called after calling dr_switch_to_app_state().
 * Swaps from the application version of system state for the given
 * thread back to the DR and client version.
 */
void
dr_switch_to_dr_state(void *drcontext);

#ifdef CUSTOM_TRACES
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
#endif /* CUSTOM_TRACES */

#ifdef UNSUPPORTED_API
DR_API 
/**
 * All basic blocks created after this routine is called will have a prefix
 * that restores the ecx register.  Exit ctis can be made to target this prefix
 * instead of the normal entry point by using the
 * instr_branch_set_prefix_target() routine.
 * \warning This routine should almost always be called during client
 * initialization, since having a mixture of prefixed and non-prefixed basic
 * blocks can lead to trouble.
 */
void
dr_add_prefixes_to_basic_blocks(void);
#endif /* UNSUPPORTED_API */

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
 */
void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                       opnd_t buf);

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
dr_insert_get_seg_base(void *drcontext, instrlist_t *ilist, instr_t *instr,
                       reg_id_t seg, reg_id_t reg);
#endif /* _INSTRUMENT_H_ */
