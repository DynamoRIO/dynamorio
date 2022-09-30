/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "monitor.c"
 */

#include "globals.h"
#include "fragment.h"
#include "link.h"
#include "utils.h"
#include "emit.h"
#include "fcache.h"
#include "monitor.h"
#include "instrument.h"
#include "instr.h"
#include "perscache.h"
#include "disassemble.h"

/* in interp.c.  not declared in arch_exports.h to avoid having to go
 * make monitor_data_t opaque in globals.h.
 */
extern bool
mangle_trace(dcontext_t *dcontext, instrlist_t *ilist, monitor_data_t *md);

/* SPEC2000 applu has a trace head entry fragment of size 40K! */
/* streamit's fft had a 944KB bb (ridiculous unrolling) */
/* no reason to have giant traces, second half will become 2ndary trace */
/* The instrumentation easily makes trace large,
 * so we should make the buffer size bigger, if a client is used.*/
#define MAX_TRACE_BUFFER_SIZE MAX_FRAGMENT_SIZE
/* most traces are under 1024 bytes.
 * for x64, the rip-rel instrs prevent a memcpy on a resize
 */
#ifdef X64
#    define INITIAL_TRACE_BUFFER_SIZE MAX_TRACE_BUFFER_SIZE
#else
#    define INITIAL_TRACE_BUFFER_SIZE 1024 /* in bytes */
#endif

#define INITIAL_NUM_BLKS 8

#define INIT_COUNTER_TABLE_SIZE 9
#define COUNTER_TABLE_LOAD 75
/* counters must be in unprotected memory
 * we don't support local unprotected so we use global
 */
/* cannot use HEAPACCT here so we use ... */
#define COUNTER_ALLOC(dc, ...)                         \
    (TEST(SELFPROT_LOCAL, dynamo_options.protect_mask) \
         ? global_unprotected_heap_alloc(__VA_ARGS__)  \
         : heap_alloc(dc, __VA_ARGS__))
#define COUNTER_FREE(dc, p, ...)                        \
    (TEST(SELFPROT_LOCAL, dynamo_options.protect_mask)  \
         ? global_unprotected_heap_free(p, __VA_ARGS__) \
         : heap_free(dc, p, __VA_ARGS__))

static void
reset_trace_state(dcontext_t *dcontext, bool grab_link_lock);

/* synchronization of shared traces */
DECLARE_CXTSWPROT_VAR(mutex_t trace_building_lock, INIT_LOCK_FREE(trace_building_lock));

/* For clearing counters on trace deletion we follow a lazy strategy
 * using a sentinel value to determine whether we've built a trace or not
 */
#define TH_COUNTER_CREATED_TRACE_VALUE() (INTERNAL_OPTION(trace_threshold) + 1U)

static void
delete_private_copy(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    if (md->last_copy != NULL) {
        LOG(THREAD, LOG_MONITOR, 4, "Deleting previous private copy F%d (" PFX ")\n",
            md->last_copy->id, md->last_copy->tag);
        /* cannot have monitor_remove_fragment called, since that would abort trace
         * if last_copy is last_fragment
         */
        if (md->last_copy == md->last_fragment) {
            /* don't have to do internal_restore_last since deleting the thing */
            md->last_fragment = NULL;
        }
        if (md->last_copy == dcontext->last_fragment)
            last_exit_deleted(dcontext);
        if (TEST(FRAG_WAS_DELETED, md->last_copy->flags)) {
            /* case 8083: private copy can't be deleted in trace_abort() since
             * needed for pc translation (at least until -safe_translate_flushed
             * is on by default), so if we come here later we must check
             * for an intervening flush to avoid double-deletion.
             */
            LOG(THREAD, LOG_MONITOR, 4,
                "\tprivate copy was flushed so not re-deleting\n");
            STATS_INC(num_trace_private_deletions_flushed);
        } else {
            fragment_delete(dcontext, md->last_copy,
                            FRAGDEL_NO_MONITOR
                                /* private fragments are invisible */
                                | FRAGDEL_NO_HTABLE);
        }
        md->last_copy = NULL;
        STATS_INC(num_trace_private_deletions);
    }
}

static void
create_private_copy(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    /* trying to share the tail of the trace ilist is a bad idea --
     * violates instrlist_t abstraction, have to deal w/ changes for
     * bb->trace (like ibl target) and worry about encoding process
     * changing instr_t state in a way that will affect the trace...
     *
     * instead we re-decode the thing, for simplicity
     */

    KSTART(temp_private_bb);
    LOG(THREAD, LOG_MONITOR, 4,
        "Creating private copy of F%d (" PFX ") for trace creation\n", f->id, f->tag);

    ASSERT(dr_get_isa_mode(dcontext) ==
           FRAG_ISA_MODE(f->flags)
               IF_X86_64(||
                         (dr_get_isa_mode(dcontext) == DR_ISA_IA32 &&
                          !FRAG_IS_32(f->flags) && DYNAMO_OPTION(x86_to_x64))));

    /* only keep one private copy around at a time
     * we delete here, when we add a new copy, and not in internal_restore_last
     * since if we do it there we'll clobber last_exit too early
     */
    if (md->last_copy != NULL)
        delete_private_copy(dcontext);

    /* PR 213760/PR 299808: rather than decode_fragment(), which is expensive for
     * frozen coarse fragments, we re-build from app code (which also simplifies
     * our client trace model).  If the existing f was flushed/deleted, that won't
     * stop us from creating a new one for our trace.
     */
    /* emitting could clobber last_fragment, which will abort this trace,
     * unfortunately -- need to be transactional so we finish building this guy,
     * and then just stop (will delete on next trace build)
     */
    md->last_fragment = build_basic_block_fragment(
        dcontext, f->tag, FRAG_TEMP_PRIVATE, true /*link*/,
        /* for clients we make temp-private even when
         * thread-private versions already exist, so
         * we have to make them invisible */
        false, true /*for_trace*/, md->pass_to_client ? &md->unmangled_bb_ilist : NULL);
    md->last_copy = md->last_fragment;

    STATS_INC(num_trace_private_copies);
    LOG(THREAD, LOG_MONITOR, 4,
        "Created private copy F%d of original F%d (" PFX ") for trace creation\n",
        md->last_fragment->id, f->id, f->tag);
    DOLOG(2, LOG_INTERP, {
        disassemble_fragment(dcontext, md->last_fragment, d_r_stats->loglevel <= 3);
    });
    KSTOP(temp_private_bb);
    ASSERT(!TEST(FRAG_CANNOT_BE_TRACE, md->last_fragment->flags));
}

static void
extend_unmangled_ilist(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    if (md->pass_to_client) {
        instr_t *inst;
        /* FIXME: pass out exit_type from build_basic_block_fragment instead
         * of walking exit stubs here?
         * FIXME: remove once we have PR 307284.
         */
        linkstub_t *l;
        ASSERT(md->last_copy != NULL);
        ASSERT(!TEST(FRAG_COARSE_GRAIN, md->last_copy->flags));
        for (l = FRAGMENT_EXIT_STUBS(md->last_copy); LINKSTUB_NEXT_EXIT(l) != NULL;
             l = LINKSTUB_NEXT_EXIT(l))
            ; /* nothing */
        md->final_exit_flags = l->flags;
        LOG(THREAD, LOG_MONITOR, 2, "final exit flags: %x\n", md->final_exit_flags);

        /* PR 299808: we need to keep a list of pre-mangled code */
        ASSERT(md->unmangled_bb_ilist != NULL);
        if (instrlist_first(md->unmangled_bb_ilist) != NULL) {
            instrlist_append(&md->unmangled_ilist,
                             instrlist_first(md->unmangled_bb_ilist));
        }
        DOLOG(4, LOG_INTERP, {
            LOG(THREAD, LOG_INTERP, 4, "unmangled ilist with F%d(" PFX "):\n",
                md->last_copy->id, md->last_copy->tag);
            instrlist_disassemble(dcontext, md->trace_tag, &md->unmangled_ilist, THREAD);
        });

        /* PR 299808: we need the end pc for boundary finding later */
        ASSERT(md->num_blks < md->blk_info_length);
        inst = instrlist_last(md->unmangled_bb_ilist);

        md->blk_info[md->num_blks].vmlist = NULL;
        if (inst != NULL) { /* PR 366232: handle empty bbs */
            vm_area_add_to_list(dcontext, f->tag, &(md->blk_info[md->num_blks].vmlist),
                                md->trace_flags, f, false /*have no locks*/);
            md->blk_info[md->num_blks].final_cti =
                instr_is_cti(instrlist_last(md->unmangled_bb_ilist));
        } else
            md->blk_info[md->num_blks].final_cti = false;

        instrlist_init(md->unmangled_bb_ilist); /* clear fields to make destroy happy */
        instrlist_destroy(dcontext, md->unmangled_bb_ilist);
        md->unmangled_bb_ilist = NULL;
    }
    /* If any constituent block wants to store (or the final trace hook wants to),
     * then store for the trace.
     */
    if (md->last_copy != NULL && TEST(FRAG_HAS_TRANSLATION_INFO, md->last_copy->flags))
        md->trace_flags |= FRAG_HAS_TRANSLATION_INFO;
}

bool
mangle_trace_at_end(void)
{
    /* There's no reason to keep an unmangled list and mangle at the end
     * unless there's a client bb or trace hook, for a for-cache trace
     * or a recreate-state trace.
     */
    return (dr_bb_hook_exists() || dr_trace_hook_exists());
}

/* Initialization */
/* thread-shared init does nothing, thread-private init does it all */
void
d_r_monitor_init()
{
    /* to reduce memory, we use ushorts for some offsets in fragment bodies,
     * so we have to stop a trace at that size
     * this does not include exit stubs
     */
    ASSERT(MAX_TRACE_BUFFER_SIZE <= MAX_FRAGMENT_SIZE);
}

/* re-initializes non-persistent memory */
void
monitor_thread_reset_init(dcontext_t *dcontext)
{
}

/* frees all non-persistent memory */
void
monitor_thread_reset_free(dcontext_t *dcontext)
{
    trace_abort_and_delete(dcontext);
}

void
trace_abort_and_delete(dcontext_t *dcontext)
{
    /* remove any MultiEntries */
    trace_abort(dcontext);
    /* case 8083: we have to explicitly remove last copy since it can't be
     * removed in trace_abort (at least until -safe_translate_flushed is on)
     */
    delete_private_copy(dcontext);
}

void
d_r_monitor_exit()
{
    LOG(GLOBAL, LOG_MONITOR | LOG_STATS, 1, "Trace fragments generated: %d\n",
        GLOBAL_STAT(num_traces));
    DELETE_LOCK(trace_building_lock);
}

static void
thcounter_free(dcontext_t *dcontext, void *p)
{
    COUNTER_FREE(dcontext, p, sizeof(trace_head_counter_t) HEAPACCT(ACCT_THCOUNTER));
}

void
monitor_thread_init(dcontext_t *dcontext)
{
    monitor_data_t *md;

    md = (monitor_data_t *)heap_alloc(dcontext,
                                      sizeof(monitor_data_t) HEAPACCT(ACCT_TRACE));
    dcontext->monitor_field = (void *)md;
    memset(md, 0, sizeof(monitor_data_t));
    reset_trace_state(dcontext, false /* link lock not needed */);

    /* case 7966: don't initialize un-needed things for hotp_only & thin_client
     * FIXME: could set initial sizes to 0 for all configurations, instead
     * FIXME: we can optimize even more to not allocate md at all, but would need
     * to have hotp_only checks in monitor_cache_exit(), etc.
     */
    if (RUNNING_WITHOUT_CODE_CACHE() || DYNAMO_OPTION(disable_traces))
        return;

    md->thead_table = generic_hash_create(
        dcontext, INIT_COUNTER_TABLE_SIZE, COUNTER_TABLE_LOAD,
        /* persist the trace head counts for improved
         * traces and trace-building efficiency
         */
        HASHTABLE_PERSISTENT, thcounter_free _IF_DEBUG("trace heads"));
    md->thead_table->hash_func = HASH_FUNCTION_MULTIPLY_PHI;
}

/* atexit cleanup */
void
monitor_thread_exit(dcontext_t *dcontext)
{
    DEBUG_DECLARE(monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;)

    /* For non-debug we do fast exit path and don't free local heap.
     * We call trace_abort so that in case the thread is terminated in
     * the middle of trace building from a shared trace head, it has a
     * chance to clear the FRAG_TRACE_BUILDING flag. Otherwise, a trace
     * can never be built from that particular trace head.
     */
    trace_abort(dcontext);
#ifdef DEBUG
    if (md->trace_buf != NULL) {
        heap_reachable_free(dcontext, md->trace_buf,
                            md->trace_buf_size HEAPACCT(ACCT_TRACE));
    }
    if (md->blk_info != NULL) {
        heap_free(dcontext, md->blk_info,
                  md->blk_info_length * sizeof(trace_bb_build_t) HEAPACCT(ACCT_TRACE));
    }
    if (md->thead_table != NULL)
        generic_hash_destroy(dcontext, md->thead_table);
    heap_free(dcontext, md, sizeof(monitor_data_t) HEAPACCT(ACCT_TRACE));
#endif
}

static trace_head_counter_t *
thcounter_lookup(dcontext_t *dcontext, app_pc tag)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    return (trace_head_counter_t *)generic_hash_lookup(dcontext, md->thead_table,
                                                       (ptr_uint_t)tag);
}

static trace_head_counter_t *
thcounter_add(dcontext_t *dcontext, app_pc tag)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    trace_head_counter_t *e = thcounter_lookup(dcontext, tag);
    if (e == NULL) {
        e = COUNTER_ALLOC(dcontext,
                          sizeof(trace_head_counter_t) HEAPACCT(ACCT_THCOUNTER));
        e->tag = tag;
        e->counter = 0;
        generic_hash_add(dcontext, md->thead_table, (ptr_uint_t)tag, e);
    }
    return e;
}

/* Deletes all trace head entries in [start,end) */
void
thcounter_range_remove(dcontext_t *dcontext, app_pc start, app_pc end)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    if (md->thead_table != NULL) {
        generic_hash_range_remove(dcontext, md->thead_table, (ptr_uint_t)start,
                                  (ptr_uint_t)end);
    }
}

bool
is_building_trace(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    return (md->trace_tag != NULL);
}

app_pc
cur_trace_tag(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    return md->trace_tag;
}

void *
cur_trace_vmlist(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    return md->trace_vmlist;
}

static void
reset_trace_state(dcontext_t *dcontext, bool grab_link_lock)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    uint i;
    /* reset the trace buffer */
    instrlist_init(&(md->trace));
    if (instrlist_first(&md->unmangled_ilist) != NULL)
        instrlist_clear(dcontext, &md->unmangled_ilist);
    instrlist_init(&md->unmangled_ilist);
    if (md->unmangled_bb_ilist != NULL)
        instrlist_clear_and_destroy(dcontext, md->unmangled_bb_ilist);
    md->unmangled_bb_ilist = NULL;
    md->trace_buf_top = 0;
    ASSERT(md->trace_vmlist == NULL);
    for (i = 0; i < md->num_blks; i++) {
        vm_area_destroy_list(dcontext, md->blk_info[i].vmlist);
        md->blk_info[i].vmlist = NULL;
    }
    md->num_blks = 0;

    /* If shared BBs are being used to build a shared trace, we may have
     * FRAG_TRACE_BUILDING set on a shared BB w/the same tag (if there is a
     * BB present -- it could've been deleted for cache management or cache
     * consistency). Unset the flag so that a trace can be built from it
     * in the future.
     */
    if (TEST(FRAG_SHARED, md->trace_flags) && DYNAMO_OPTION(shared_bbs)) {
        /* Look in the shared BB table only since we're only interested
         * if a shared BB is present. */
        fragment_t *bb = fragment_lookup_shared_bb(dcontext, md->trace_tag);
        /* FRAG_TRACE_BUILDING may not be set if the BB was regenerated, so
         * we can't expect it to be set simply because the BB is shared. Check
         * just for the trace bulding flag.
         */
        if (grab_link_lock)
            acquire_recursive_lock(&change_linking_lock);
        if (bb != NULL && TEST(FRAG_TRACE_BUILDING, bb->flags)) {
            /* The regenerate scenario is still racy w/respect to clearing the
             * flag. The regenerated fragment could have another thread building
             * a trace from it so the clear would be for the the wrong thread
             * here. It doesn't cause a correctness problem because the
             * emit-time race detection logic will catch it. (In testing w/IIS,
             * we've seen very, very few emit-time aborts -- < 1% of all races.)
             */
            ASSERT(TESTALL(FRAG_SHARED | FRAG_IS_TRACE_HEAD, bb->flags));
            STATS_INC(num_trace_building_ip_cleared);
            bb->flags &= ~FRAG_TRACE_BUILDING;
        }
#ifdef DEBUG
        /* As noted above, the trace head BB may no longer be present. This
         * should be rare in most apps but we'll track it w/a counter in case
         * we see lots of emit-time aborts.
         */
        else {
            STATS_INC(num_reset_trace_no_trace_head);
            /* The shared BB may been evicted during trace building and subsequently
             * re-genned and so wouldn't be marked as FRAG_TRACE_BUILDING. It might
             * be marked as a trace head, though, so we don't assert anything about
             * that trait.
             * FIXME We could add a strong ASSERT about the regen case if we added
             * a trace_head_id field to monitor_data_t. The field would store the id
             * of the shared BB trace head that caused trace building to begin. If
             * a shared trace head isn't found but a shared BB is, the shared BB
             * id should be greater than trace_head_id.
             */
        }
#endif
        if (grab_link_lock)
            release_recursive_lock(&change_linking_lock);
    }
    md->trace_tag = NULL; /* indicate return to search mode */
    md->trace_flags = 0;
    md->emitted_size = 0;
    /* flags may not match, e.g., if frag was marked as trace head */
    ASSERT(md->last_fragment == NULL ||
           (md->last_fragment_flags & (FRAG_CANNOT_DELETE | FRAG_LINKED_OUTGOING)) ==
               (md->last_fragment->flags & (FRAG_CANNOT_DELETE | FRAG_LINKED_OUTGOING)));
    md->last_fragment_flags = 0;
    /* we don't delete last_copy here to avoid issues w/ trace_abort deleting
     * a fragment we're examining (seg fault, etc.)
     */
    md->last_fragment = NULL;
    /* note that we don't delete last_copy here as it's needed for pc translation
     * (at least until -safe_translate_flushed is on) (xref case 8083)
     */
#ifdef CUSTOM_TRACES_RET_REMOVAL
    dcontext->call_depth = 0;
#endif
}

bool
monitor_delete_would_abort_trace(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md;
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return false;
    md = (monitor_data_t *)dcontext->monitor_field;
    return ((md->last_fragment == f || dcontext->last_fragment == f) &&
            md->trace_tag != NULL);
}

/* called when a fragment is deleted */
void
monitor_remove_fragment(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md;
    /* may be a global fragment -- but we still want our local trace data */
    if (dcontext == GLOBAL_DCONTEXT) {
        ASSERT(TEST(FRAG_SHARED, f->flags));
        dcontext = get_thread_private_dcontext();
        /* may still be null if exiting process -- in which case a nop for us */
        if (dcontext == NULL) {
            if (dynamo_exited)
                return;
            ASSERT_NOT_REACHED();
            return; /* safe default */
        }
    }
    md = (monitor_data_t *)dcontext->monitor_field;
    if (md->last_copy == f) {
        md->last_copy = NULL; /* no other action required */
        STATS_INC(num_trace_private_deletions);
    }
    /* Must check to see if the last fragment, which was added to the
     * trace, is being deleted before we're done with it.
     * This can happen due to a flush from self-modifying code,
     * or an munmap.
     * Must check both last_fragment and last_exit.
     * May come here before last_exit is set, or may come here after
     * last_fragment is restored but before last_exit is used.
     * FIXME: if we do manage to remove the check for last_fragment
     * here, remove the last_exit clear in end_and_emit_trace
     */
    /* FIXME: case 5593 we may also unnecessarily abort a trace that
     * starts at the next_tag and last_fragment is really not
     * related.
     */
    if ((md->last_fragment == f || dcontext->last_fragment == f) &&
        !TEST(FRAG_TEMP_PRIVATE, f->flags)) {
        if (md->trace_tag != NULL) {
            LOG(THREAD, LOG_MONITOR, 2, "Aborting current trace since F%d was deleted\n",
                f->id);
            /* abort current trace, we've lost a link */
            trace_abort(dcontext);
        }
        /* trace_abort clears last_fragment -- and if not in trace-building
         * mode, it should not be set!
         */
        ASSERT(md->last_fragment == NULL);
        if (dcontext->last_fragment == f)
            last_exit_deleted(dcontext);
    }
}

/* Unlink the trace head fragment from any IBT tables in which it is in */
static inline void
unlink_ibt_trace_head(dcontext_t *dcontext, fragment_t *f)
{
    ASSERT(TEST(FRAG_IS_TRACE_HEAD, f->flags));
    if (DYNAMO_OPTION(shared_bb_ibt_tables)) {
        ASSERT(TEST(FRAG_SHARED, f->flags));
        if (fragment_prepare_for_removal(GLOBAL_DCONTEXT, f)) {
            LOG(THREAD, LOG_FRAGMENT, 3, "  F%d(" PFX ") removed as trace head IBT\n",
                f->id, f->tag);
            STATS_INC(num_th_bb_ibt_unlinked);
        }
    } else {

        /* To preserve the current paradigm of trace head-ness as a shared
         * property, we must unlink the fragment from every thread's IBT tables.
         * This is a heavyweight operation compared to the use of a shared table
         * and requires additional changes -- for example, get_list_of_threads()
         * can't currently be called from here. If we change trace head-ness
         * to a private property, this becomes very easy and more performant
         * than the use of a shared table. (Case 3530 discusses private vs shared
         * trace head-ness.)
         */
        thread_record_t **threads;
        int num_threads;
        int i;

        ASSERT_NOT_IMPLEMENTED(false);
        /* fragment_prepare_for_removal will unlink from shared ibt; we cannot
         * remove completely here */
        fragment_remove_from_ibt_tables(dcontext, f, false /*leave in shared ibt*/);
        /* Remove the fragment from other thread's tables. */
        d_r_mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num_threads);
        d_r_mutex_unlock(&thread_initexit_lock);
        for (i = 0; i < num_threads; i++) {
            dcontext_t *tgt_dcontext = threads[i]->dcontext;
            LOG(THREAD, LOG_FRAGMENT, 2, "  considering thread %d/%d = " TIDFMT "\n",
                i + 1, num_threads, threads[i]->id);
            ASSERT(is_thread_known(tgt_dcontext->owning_thread));
            fragment_prepare_for_removal(
                TEST(FRAG_SHARED, f->flags) ? GLOBAL_DCONTEXT : tgt_dcontext, f);
        }
        global_heap_free(
            threads, num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    }
}

/* if f is shared, caller MUST hold the change_linking_lock */
void
mark_trace_head(dcontext_t *dcontext_in, fragment_t *f, fragment_t *src_f,
                linkstub_t *src_l)
{
    bool protected = false;
    cache_pc coarse_stub = NULL, coarse_body = NULL;
    coarse_info_t *info = NULL;
    /* Case 9021: handle GLOBAL_DCONTEXT coming in via flush_fragments_synchall
     * removing a fine trace that triggers a shift to its shadowed coarse trace
     * head and a link_fragment_incoming on that head.
     * Using the flushing thread's dcontext for the trace head counter is fine
     * and shouldn't limit its becoming a new trace again.
     */
    dcontext_t *dcontext =
        (dcontext_in == GLOBAL_DCONTEXT) ? get_thread_private_dcontext() : dcontext_in;
    ASSERT(dcontext != NULL);

    LOG(THREAD, LOG_MONITOR, 3, "marking F%d (" PFX ") as trace head\n", f->id, f->tag);
    ASSERT(!TEST(FRAG_IS_TRACE, f->flags));
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));

    if (thcounter_lookup(dcontext, f->tag) == NULL) {
        protected = local_heap_protected(dcontext);
        if (protected) {
            /* unprotect local heap */
            protect_local_heap(dcontext, WRITABLE);
        }
        /* FIXME: private counter tables are used even for !shared_bbs since the
         * counter field is not in fragment_t...
         * Move counters to Future for all uses, giving us persistent counters too!
         */
        thcounter_add(dcontext, f->tag);
    } else {
        /* This does happen for resurrected fragments and coarse-grain fragments */
        STATS_INC(trace_head_remark);
    }
    LOG(THREAD, LOG_MONITOR, 4, "mark_trace_head: flags 0x%08x\n", f->flags);
    f->flags |= FRAG_IS_TRACE_HEAD;
    LOG(THREAD, LOG_MONITOR, 4, "\tnow, flags 0x%08x\n", f->flags);
    /* must unlink incoming links so that the counter will increment */
    LOG(THREAD, LOG_MONITOR, 4, "unlinking incoming for new trace head F%d (" PFX ")\n",
        f->id, f->tag);

    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* For coarse trace heads, trace headness depends on the path taken
         * (more specifically, on the entrance stub taken).  If we don't have
         * any info on src_f we use f's unit.
         */
        info = get_fragment_coarse_info(src_f == NULL ? f : src_f);
        if (info == NULL) {
            /* case 8632: A fine source may not be in a coarse region,
             * so there is nothing to unlink.
             */
        } else {
            /* See if there is an entrance stub for this target in the source unit */
            fragment_coarse_lookup_in_unit(dcontext, info, f->tag, &coarse_stub,
                                           &coarse_body);
            /* FIXME: don't allow marking for frozen units w/ no src info:
             * shouldn't happen, except perhaps with clients.
             */
            ASSERT(src_f != NULL || !info->frozen);
            if (src_f != NULL && TEST(FRAG_COARSE_GRAIN, src_f->flags) && src_l != NULL &&
                LINKSTUB_NORMAL_DIRECT(src_l->flags)) {
                direct_linkstub_t *dl = (direct_linkstub_t *)src_l;
                if (dl->stub_pc != NULL && coarse_is_entrance_stub(dl->stub_pc)) {
                    if (coarse_stub == NULL) {
                        /* Case 9708: For a new fragment whose target exists but
                         * is another unit and does not yet have an entrance
                         * stub in the new fragment's unit, we will come here
                         * w/o that entrance stub being in the htable.  We rely
                         * on dl->stub_pc being set to that entrance stub.
                         */
                        coarse_stub = dl->stub_pc;
                    } else
                        ASSERT(dl->stub_pc == NULL || dl->stub_pc == coarse_stub);
                }
            }
            if (coarse_stub != NULL) {
                ASSERT(coarse_is_entrance_stub(coarse_stub));
                /* FIXME: our coarse lookups do not always mark trace headness
                 * (in particular, fragment_coarse_link_wrapper() calling
                 * fragment_coarse_lookup_wrapper() does not), and we
                 * un-mark as trace heads when linking incoming (case 8907),
                 * so we may get here for an existing trace head.
                 */
                if (!coarse_is_trace_head_in_own_unit(dcontext, f->tag, coarse_stub,
                                                      coarse_body, true,
                                                      (src_f == NULL) ? info : NULL)) {
                    ASSERT(coarse_body == NULL /* new fragment, or in other unit */ ||
                           entrance_stub_jmp_target(coarse_stub) == coarse_body);
                    if (coarse_body == NULL &&
                        /* if stub is from tag's own unit */
                        (src_f == NULL || get_fragment_coarse_info(f) == info)) {
                        /* if marking new fragment, not in htable yet */
                        coarse_body = FCACHE_ENTRY_PC(f);
                    }
                    coarse_mark_trace_head(dcontext, f, info, coarse_stub, coarse_body);
                }
            } else {
                LOG(THREAD, LOG_MONITOR, 4, "\tno local stub, deferring th unlink\n");
                /* Could be that this is a new fragment, in which case its entrance
                 * stub will be unlinked and its body pc added to the th table in
                 * link_new_coarse_grain_fragment(); or the source is a fine
                 * fragment corresponding to another unit and thus no entrance stub
                 * or htable changes are necessary.
                 */
                STATS_INC(coarse_th_from_fine);
                /* id comparison could have a race w/ private frag gen so a curiosity */
                ASSERT_CURIOSITY(
                    GLOBAL_STAT(num_fragments) == f->id ||
                    (src_f != NULL && !TEST(FRAG_COARSE_GRAIN, src_f->flags)));
            }
        }
    } else
        unlink_fragment_incoming(dcontext, f);

    if (DYNAMO_OPTION(bb_ibl_targets))
        unlink_ibt_trace_head(dcontext, f);
#ifdef TRACE_HEAD_CACHE_INCR
    /* we deliberately link to THCI in two steps (unlink and then
     * re-link), since combined they aren't atomic, separate atomic
     * steps w/ ok intermediate (go back to DR) is fine
     */
    /* must re-link incoming links to point to trace_head_incr routine
     * FIXME: we get called in the middle of linking new fragments, so
     * we end up linking some incoming links twice (no harm done except
     * a waste of time) -- how fix it?
     * When fix it, change link_branch to assert that !already linked
     */
    link_fragment_incoming(dcontext, f, false /*not new*/);
#endif
    STATS_INC(num_trace_heads_marked);
    /* caller is either d_r_dispatch or inside emit_fragment, they take care of
     * re-protecting fcache
     */
    if (protected) {
        /* re-protect local heap */
        protect_local_heap(dcontext, READONLY);
    }
}

/* can ONLY be called by should_be_trace_head_internal, separated out
 * to avoid recursion when re-verifying with change_linking_lock held
 */
static bool
should_be_trace_head_internal_unsafe(dcontext_t *dcontext, fragment_t *from_f,
                                     linkstub_t *from_l, app_pc to_tag, uint to_flags,
                                     bool trace_sysenter_exit)
{
    app_pc from_tag;
    uint from_flags;

    if (DYNAMO_OPTION(disable_traces) || TEST(FRAG_IS_TRACE, to_flags) ||
        TEST(FRAG_IS_TRACE_HEAD, to_flags) || TEST(FRAG_CANNOT_BE_TRACE, to_flags))
        return false;

    /* We know that the to_flags pass the test. */
    if (trace_sysenter_exit)
        return true;

    from_tag = from_f->tag;
    from_flags = from_f->flags;

    /* A trace head is either
     *   1) a link from a trace, or
     *   2) a backward direct branch
     * Watch out -- since we stop building traces at trace heads,
     * too many can hurt performance, especially if bbs do not follow
     * direct ctis.  We can use shadowed bbs to go through trace
     * head and trace boundaries for custom traces.
     */
    /* trace heads can be created across private/shared cache bounds */
    if (TEST(FRAG_IS_TRACE, from_flags) ||
        (to_tag <= from_tag && LINKSTUB_DIRECT(from_l->flags)))
        return true;

    DOSTATS({
        if (!DYNAMO_OPTION(disable_traces) && !TEST(FRAG_IS_TRACE, to_flags) &&
            !TEST(FRAG_IS_TRACE_HEAD, to_flags) &&
            !TEST(FRAG_CANNOT_BE_TRACE, to_flags)) {
            STATS_INC(num_wannabe_traces);
        }
    });
    return false;
}

/* Returns TRACE_HEAD_* flags indicating whether to_tag should be a
 * trace head based on fragment traits and/or control flow between the
 * link stub and the to_tag/to_flags.
 *
 * For -shared_bbs, will return TRACE_HEAD_OBTAINED_LOCK if the
 * change_linking_lock is not already held (meaning from_l->fragment is
 * private) and the to_tag is FRAG_SHARED and TRACE_HEAD_YES is being returned,
 * since the change_linking_lock must be held and the TRACE_HEAD_YES result
 * re-verified.  In that case the caller must free the change_linking_lock.
 * If trace_sysenter_exit = true, control flow rules are not checked, i.e., the
 * from_l and to_tag params are not checked. This is provided to capture
 * the case where the most recent cache exit was prior to a non-ignorable
 * syscall via a SYSENTER instruction. See comments in monitor_cache_exit for
 * details. This is the exception, not the norm.
 *
 * If the link stub is non-NULL, trace_sysenter_exit does NOT need to
 * be set.
 *
 * FIXME This is a stopgap soln. The long-term fix is to not count on
 * a link stub being passed in but rather pass in the most recent fragment's
 * flags & tag explicitly. The flags & tag can be stored in a dcontext-private
 * monitor structure, one that is not shared across call backs.
 */
static uint
should_be_trace_head_internal(dcontext_t *dcontext, fragment_t *from_f,
                              linkstub_t *from_l, app_pc to_tag, uint to_flags,
                              bool have_link_lock, bool trace_sysenter_exit)
{
    uint result = 0;
    if (should_be_trace_head_internal_unsafe(dcontext, from_f, from_l, to_tag, to_flags,
                                             trace_sysenter_exit)) {
        result |= TRACE_HEAD_YES;
        ASSERT(!have_link_lock || self_owns_recursive_lock(&change_linking_lock));
        if (!have_link_lock) {
            /* If the target is shared, we must obtain the change_linking_lock and
             * re-verify that it hasn't already been marked.
             * If source is also shared then lock should already be held .
             */
            ASSERT(from_l == NULL || !NEED_SHARED_LOCK(from_f->flags));
            if (NEED_SHARED_LOCK(to_flags)) {
                acquire_recursive_lock(&change_linking_lock);
                if (should_be_trace_head_internal_unsafe(dcontext, from_f, from_l, to_tag,
                                                         to_flags, trace_sysenter_exit)) {
                    result |= TRACE_HEAD_OBTAINED_LOCK;
                } else {
                    result &= ~TRACE_HEAD_YES;
                    release_recursive_lock(&change_linking_lock);
                }
            }
        }
    }
    return result;
}

/* Returns TRACE_HEAD_* flags indicating whether to_tag should be a
 * trace head based on fragment traits and/or control flow between the
 * link stub and the to_tag/to_flags.
 *
 * For -shared_bbs, will return TRACE_HEAD_OBTAINED_LOCK if the
 * change_linking_lock is not already held (meaning from_l->fragment is
 * private) and the to_tag is FRAG_SHARED and TRACE_HEAD_YES is being returned,
 * since the change_linking_lock must be held and the TRACE_HEAD_YES result
 * re-verified.  In that case the caller must free the change_linking_lock.
 */
uint
should_be_trace_head(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
                     app_pc to_tag, uint to_flags, bool have_link_lock)
{
    return should_be_trace_head_internal(dcontext, from_f, from_l, to_tag, to_flags,
                                         have_link_lock, false);
}

/* If upgrades to_f to a trace head, returns true, else returns false.
 */
static bool
check_for_trace_head(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
                     fragment_t *to_f, bool have_link_lock, bool trace_sysenter_exit)
{
    if (!DYNAMO_OPTION(disable_traces)) {
        uint th = should_be_trace_head_internal(dcontext, from_f, from_l, to_f->tag,
                                                to_f->flags, have_link_lock,
                                                trace_sysenter_exit);
        if (TEST(TRACE_HEAD_YES, th)) {
            mark_trace_head(dcontext, to_f, from_f, from_l);
            if (TEST(TRACE_HEAD_OBTAINED_LOCK, th))
                release_recursive_lock(&change_linking_lock);
            return true;
        }
    }
    return false;
}

/* Linkability rules involving traces and trace heads.
 * This routines also marks new traces heads if mark_new_trace_head is true.
 * The current implementation of this routine assumes that we don't
 * want to link potential trace heads.  A potential trace head is any
 * block fragment that is reached by a backward (direct) branch.
 */
bool
monitor_is_linkable(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
                    fragment_t *to_f, bool have_link_lock, bool mark_new_trace_head)
{
    /* common case: both traces */
    if (TEST(FRAG_IS_TRACE, from_f->flags) && TEST(FRAG_IS_TRACE, to_f->flags))
        return true;
    if (DYNAMO_OPTION(disable_traces))
        return true;
#ifndef TRACE_HEAD_CACHE_INCR
    /* no link case -- block is a trace head */
    if (TEST(FRAG_IS_TRACE_HEAD, to_f->flags) && !DYNAMO_OPTION(disable_traces))
        return false;
#endif
    if (mark_new_trace_head) {
        uint th = should_be_trace_head(dcontext, from_f, from_l, to_f->tag, to_f->flags,
                                       have_link_lock);
        if (TEST(TRACE_HEAD_YES, th)) {
            mark_trace_head(dcontext, to_f, from_f, from_l);
            if (TEST(TRACE_HEAD_OBTAINED_LOCK, th))
                release_recursive_lock(&change_linking_lock);
#ifdef TRACE_HEAD_CACHE_INCR
            /* fine to link to trace head
             * link will end up pointing not to fcache_return but to trace_head_incr
             */
            return true;
#else
            return false;
#endif
        }
    }
    return true; /* otherwise */
}

/* If necessary, re-allocates the trace buffer to a larger size to
 * hold add_size more bytes.
 * If the resulting size will exceed the maximum trace
 * buffer size, returns false, else returns true.
 * FIXME: now that we have a real max limit on emitted trace size,
 * should we have an unbounded trace buffer size?
 * Also increases the size of the block array if necessary.
 */
static bool
make_room_in_trace_buffer(dcontext_t *dcontext, uint add_size, fragment_t *f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    uint size;
    uint new_blks;
    ssize_t realloc_shift;
    instr_t *instr;
    instrlist_t *trace = &md->trace;

    size = md->trace_buf_size;
    if (add_size > (size - md->trace_buf_top)) {
        byte *new_tbuf;

        if (size == 0)
            size = INITIAL_TRACE_BUFFER_SIZE;
        while (add_size > (size - md->trace_buf_top))
            size *= 2;
        if (size > MAX_TRACE_BUFFER_SIZE) {
            LOG(THREAD, LOG_MONITOR, 2, "Not letting trace buffer grow to %d bytes\n",
                size);
            return false;
        }
        /* Re-allocate trace buf.  It must be reachable for rip-rel re-relativization. */
        new_tbuf = heap_reachable_alloc(dcontext, size HEAPACCT(ACCT_TRACE));
        if (md->trace_buf != NULL) {
            /* copy entire thing, just in case */
            IF_X64(ASSERT_NOT_REACHED()); /* can't copy w/o re-relativizing! */
            memcpy(new_tbuf, md->trace_buf, md->trace_buf_size);
            heap_reachable_free(dcontext, md->trace_buf,
                                md->trace_buf_size HEAPACCT(ACCT_TRACE));
            realloc_shift = new_tbuf - md->trace_buf;
            /* need to walk through trace instr_t list and update addresses */
            instr = instrlist_first(trace);
            while (instr != NULL) {
                byte *b = instr_get_raw_bits(instr);
                if (b >= md->trace_buf && b < md->trace_buf + md->trace_buf_size)
                    instr_shift_raw_bits(instr, realloc_shift);
                instr = instr_get_next(instr);
            }
        }
        LOG(THREAD, LOG_MONITOR, 3,
            "\nRe-allocated trace buffer from %d @" PFX " to %d bytes @" PFX "\n",
            md->trace_buf_size, md->trace_buf, size, new_tbuf);
        md->trace_buf = new_tbuf;
        md->trace_buf_size = size;
    }
    if ((f->flags & FRAG_IS_TRACE) != 0) {
        trace_only_t *t = TRACE_FIELDS(f);
        new_blks = t->num_bbs;
    } else
        new_blks = 1;
    if (md->num_blks + new_blks >= md->blk_info_length) {
        trace_bb_build_t *new_buf;
        uint new_len = md->blk_info_length;
        if (new_len == 0)
            new_len = INITIAL_NUM_BLKS;
        do {
            new_len *= 2;
        } while (md->num_blks + new_blks >= new_len);
        new_buf = (trace_bb_build_t *)HEAP_ARRAY_ALLOC(dcontext, trace_bb_build_t,
                                                       new_len, ACCT_TRACE, true);
        /* PR 306761 relies on being zeroed, as does reset_trace_state to free vmlists */
        memset(new_buf, 0, sizeof(trace_bb_build_t) * new_len);
        LOG(THREAD, LOG_MONITOR, 3, "\nRe-allocating trace blks from %d to %d\n",
            md->blk_info_length, new_len);
        if (md->blk_info != NULL) {
            memcpy(new_buf, md->blk_info, md->blk_info_length * sizeof(trace_bb_build_t));
            HEAP_ARRAY_FREE(dcontext, md->blk_info, trace_bb_build_t, md->blk_info_length,
                            ACCT_TRACE, true);
        }
        md->blk_info = new_buf;
        md->blk_info_length = new_len;
    }
    return true;
}

static int
trace_exit_stub_size_diff(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    int size = 0;
    linkstub_t *l;
    for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        if (linkstub_shares_next_stub(dcontext, f, l)) {
            /* add stub size back in since we don't know if trace will also
             * share (if client adds custom code, etc.)
             * this also makes fixup_last_cti() code simpler since it can
             * blindly remove and ignore sharing.
             * if the trace does share for a final bb, we remove in
             * end_and_emit_trace().
             */
            size += local_exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l),
                                         md->trace_flags);
        } else {
            /* f's stub size will be considered as part of f->size so we need
             * the difference here, not the absolute new size
             */
            size += local_exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l),
                                         md->trace_flags) -
                local_exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l), f->flags);
        }
    }
    return size;
}

/* don't build a single trace more than 1/8 of max trace cache size */
enum { MAX_TRACE_FRACTION_OF_CACHE = 8 };

/* Estimates the increase in the emitted size of the current trace if f were
 * to be added to it.
 * If that size exceeds the maximum fragment size, or a fraction of the maximum
 * trace cache size, returns false.
 * Returns the size calculations in two different parts:
 * res_add_size is the accurate value of the body and exit stubs addition, while
 * res_prev_mangle_size is an upper bound estimate of the change in size when
 * the prior block in the trace is mangled to connect to f.
 */
static bool
get_and_check_add_size(dcontext_t *dcontext, fragment_t *f, uint *res_add_size,
                       uint *res_prev_mangle_size)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    uint add_size = f->size - fragment_prefix_size(f->flags) +
        trace_exit_stub_size_diff(dcontext, f) +
        (PAD_FRAGMENT_JMPS(md->trace_flags) ? extend_trace_pad_bytes(f) : 0);
    /* we estimate the size change from mangling the previous block to
     * connect to this block if we were to add it
     */
    uint prev_mangle_size = TRACE_CTI_MANGLE_SIZE_UPPER_BOUND;
    uint total_size = md->emitted_size + add_size + prev_mangle_size;
    /* check whether adding f will push the trace over the edge */
    bool ok = (total_size <= MAX_FRAGMENT_SIZE);
    ASSERT(!TEST(FRAG_SELFMOD_SANDBOXED, f->flags)); /* no support for selfmod */
    ASSERT(!TEST(FRAG_IS_TRACE, f->flags));          /* no support for traces */
    LOG(THREAD, LOG_MONITOR, 4,
        "checking trace size: currently %d, add estimate %d\n"
        "\t(body: %d, stubs: %d, pad: %d, mangle est: %d)\n"
        "\t=> %d vs %d, %d vs %d\n",
        md->emitted_size, add_size + prev_mangle_size,
        f->size - fragment_prefix_size(f->flags), trace_exit_stub_size_diff(dcontext, f),
        (PAD_FRAGMENT_JMPS(md->trace_flags) ? extend_trace_pad_bytes(f) : 0),
        prev_mangle_size, total_size, MAX_FRAGMENT_SIZE,
        total_size * MAX_TRACE_FRACTION_OF_CACHE, DYNAMO_OPTION(cache_trace_max));
    /* don't create traces anywhere near max trace cache size */
    if (ok && DYNAMO_OPTION(cache_trace_max) > 0 &&
        total_size * MAX_TRACE_FRACTION_OF_CACHE > DYNAMO_OPTION(cache_trace_max))
        ok = false;
    if (res_add_size != NULL)
        *res_add_size = add_size;
    if (res_prev_mangle_size != NULL)
        *res_prev_mangle_size = prev_mangle_size;
    return ok;
}

/* propagate flags from a non-head bb component of a trace to the trace itself */
static inline uint
trace_flags_from_component_flags(uint flags)
{
    return (flags &
            (FRAG_HAS_SYSCALL |
             FRAG_HAS_DIRECT_CTI IF_X86_64(
                 | FRAG_32_BIT IF_LINUX(| FRAG_HAS_RSEQ_ENDPOINT))));
}

static inline uint
trace_flags_from_trace_head_flags(uint head_flags)
{
    uint trace_flags = 0;
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_prefix)) {
        trace_flags |= (head_flags & FRAG_WRITES_EFLAGS_6);
        trace_flags |= (head_flags & FRAG_WRITES_EFLAGS_OF);
    }
    trace_flags |= FRAG_IS_TRACE;
    trace_flags |= trace_flags_from_component_flags(head_flags);
    if (DYNAMO_OPTION(shared_traces)) {
        /* for now, all traces are shared */
        trace_flags |= FRAG_SHARED;
    }
    return trace_flags;
}

/* Be careful with the case where the current fragment f to be executed
 * has the same tag as the one we're emitting as a trace.
 */
static fragment_t *
end_and_emit_trace(dcontext_t *dcontext, fragment_t *cur_f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    fragment_t *trace_head_f = NULL;
    app_pc tag = md->trace_tag;
    app_pc cur_f_tag = cur_f->tag; /* grab now before potential cur_f deletion */
    instrlist_t *trace = &md->trace;
    fragment_t *trace_f;
    trace_only_t *trace_tr;
    bool replace_trace_head = false;
    fragment_t wrapper;
    uint i;
    /* was the trace passed through optimizations or the client interface? */
    DEBUG_DECLARE(bool externally_mangled = false;)
    /* we cannot simply upgrade a basic block fragment
     * to a trace b/c traces have prefixes that basic blocks don't!
     */

    DOSTATS({
        /* static count last_exit statistics case 4817 */
        if (LINKSTUB_INDIRECT(dcontext->last_exit->flags)) {
            STATS_INC(num_traces_end_at_ibl);
            if (EXIT_IS_CALL(dcontext->last_exit->flags)) {
                STATS_INC(num_traces_end_at_ibl_ind_call);
            } else if (EXIT_IS_JMP(dcontext->last_exit->flags)) {
                /* shared system call (case 4995) */
                if (IS_SHARED_SYSCALLS_LINKSTUB(dcontext->last_exit))
                    STATS_INC(num_traces_end_at_ibl_syscall);
                else
                    STATS_INC(num_traces_end_at_ibl_ind_jump);
            } else if (TEST(LINK_RETURN, dcontext->last_exit->flags)) {
                STATS_INC(num_traces_end_at_ibl_return);
            };
        }
    });

    if (md->pass_to_client) {
        /* PR 299808: we pass the unmangled ilist we've been maintaining to the
         * client, and we have to then re-mangle and re-connect.
         */
        dr_emit_flags_t emitflags =
            instrument_trace(dcontext, tag, &md->unmangled_ilist, false /*!recreating*/);
        DODEBUG(externally_mangled = true;);
        if (TEST(DR_EMIT_STORE_TRANSLATIONS, emitflags)) {
            /* PR 214962: let client request storage instead of recreation */
            md->trace_flags |= FRAG_HAS_TRANSLATION_INFO;
        } /* else, leave translation flag if any bb requested it */

        /* We now have to re-mangle and re-chain */
        if (!mangle_trace(dcontext, &md->unmangled_ilist, md)) {
            trace_abort(dcontext);
            STATS_INC(num_aborted_traces_client);
            trace_f = NULL;
            goto end_and_emit_trace_return;
        }
        instrlist_clear(dcontext, &md->trace);
        md->trace = md->unmangled_ilist;
        instrlist_init(&md->unmangled_ilist);
    }

    if (INTERNAL_OPTION(cbr_single_stub) &&
        final_exit_shares_prev_stub(dcontext, trace, md->trace_flags)) {
        /* while building, we re-add shared stub since not sure if
         * trace will also share -- here we find out and adjust
         */
        instr_t *last = instrlist_last(trace);
        app_pc target;
        ASSERT(last != NULL && instr_is_exit_cti(last));
        target = opnd_get_pc(instr_get_target(last));
        md->emitted_size -= local_exit_stub_size(dcontext, target, md->trace_flags);
    }

    /* XXX i#5062 In the future this call should be placed inside mangle_trace() */
    IF_AARCH64(md->emitted_size += fixup_indirect_trace_exit(dcontext, trace));

    if (DYNAMO_OPTION(speculate_last_exit)
#ifdef HASHTABLE_STATISTICS
        || INTERNAL_OPTION(speculate_last_exit_stats) ||
        INTERNAL_OPTION(stay_on_trace_stats)
#endif
    ) {
        /* FIXME: speculation of last exit (case 4817) is currently
         * only implemented for traces.  If we have a sharable version
         * of fixup_last_cti() to pass that information based on instr
         * list information about last exit we can use in
         * emit_fragment_common().  That way both bb's and traces may
         * have speculation added.
         */
        if (TEST(FRAG_MUST_END_TRACE, cur_f->flags)) {
            /* This routine may be also reached on MUST_END_TRACE
             * and in that case we haven't executed yet the last
             * bb, so don't really know how to fix the last IBL
             */
            /* FIXME: add a stat when such are ending at an IBL */

            ASSERT_CURIOSITY(dcontext->next_tag == cur_f->tag);
            STATS_INC(num_traces_at_must_end_trace);
        } else {
            /* otherwise last_exit is the last trace BB and next_tag
             * is the current IBL target that we'll always speculate */
            if (LINKSTUB_INDIRECT(dcontext->last_exit->flags)) {
                LOG(THREAD, LOG_MONITOR, 2,
                    "Last trace IBL exit (trace " PFX ", next_tag " PFX ")\n", tag,
                    dcontext->next_tag);
                ASSERT_CURIOSITY(dcontext->next_tag != NULL);
                if (DYNAMO_OPTION(speculate_last_exit)) {
                    app_pc speculate_next_tag = dcontext->next_tag;
#ifdef SPECULATE_LAST_EXIT_STUDY
                    /* for a performance study: add overhead on
                     * all IBLs that never hit by comparing to a 0xbad tag */
                    speculate_next_tag = 0xbad;
#endif
                    md->emitted_size += append_trace_speculate_last_ibl(
                        dcontext, trace, speculate_next_tag, false);
                } else {
#ifdef HASHTABLE_STATISTICS
                    ASSERT(INTERNAL_OPTION(stay_on_trace_stats) ||
                           INTERNAL_OPTION(speculate_last_exit_stats));
                    DOSTATS({
                        md->emitted_size += append_ib_trace_last_ibl_exit_stat(
                            dcontext, trace,
                            INTERNAL_OPTION(speculate_last_exit_stats)
                                ? dcontext->next_tag
                                : NULL);
                    });
#endif
                }
            }
        }
    }

    DOLOG(2, LOG_MONITOR, {
        LOG(THREAD, LOG_MONITOR, 2, "Ending and emitting hot trace (tag " PFX ")\n", tag);
        if (d_r_stats->loglevel >= 4) {
            instrlist_disassemble(dcontext, md->trace_tag, trace, THREAD);
            LOG(THREAD, LOG_MONITOR, 4, "\n");
        }
        LOG(THREAD, LOG_MONITOR, 2, "Trace blocks are:\n");
        for (i = 0; i < md->num_blks; i++) {
            LOG(THREAD, LOG_MONITOR, 2, "\tblock %3d == " PFX " (%d exit(s))\n", i,
                md->blk_info[i].info.tag,
                IF_RETURN_AFTER_CALL_ELSE(md->blk_info[i].info.num_exits, 0));
        }
    });

    /* WARNING: if you change how optimizations are performed, you
     * must change recreate_app_state in arch/arch.c as well
     */

#ifdef INTERNAL
    if (dynamo_options.optimize
#    ifdef SIDELINE
        && !dynamo_options.sideline
#    endif
    ) {
        optimize_trace(dcontext, tag, trace);
        DODEBUG(externally_mangled = true;);
    }
#endif /* INTERNAL */

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        /* space was already reserved in buffer and in md->emitted_size */
        add_profile_call(dcontext);
    }
#endif

#ifdef SIDELINE
    if (dynamo_options.sideline) {
        /* FIXME: add size to emitted_size when start building trace to
         * ensure room in buffer and in cache
         */
        add_sideline_prefix(dcontext, trace);
    }
#endif

    /* delete any private copy now and use its space for this trace
     * for private traces:
     *   this way we use the head of FIFO for all our private copies, and
     *   then replace w/ the trace, avoiding any fragmentation from the copies.
     * for shared traces: FIXME: case 5137: move temps to private bb cache?
     */
    if (md->last_copy != NULL) {
        if (cur_f == md->last_copy)
            cur_f = NULL;
        delete_private_copy(dcontext);
    }

    /* Shared trace synchronization model:
     * We can't hold locks across cache executions, and we wouldn't want to have a
     * massive trace building lock anyway, so we only grab a lock at the final emit
     * moment and if there's a conflict the loser tosses his trace.
     * We hold the lock across the trace head removal as well to avoid races there.
     */
    if (TEST(FRAG_SHARED, md->trace_flags)) {
        ASSERT(DYNAMO_OPTION(shared_traces));
        d_r_mutex_lock(&trace_building_lock);
        /* we left the bb there, so we rely on any shared trace shadowing it */
        trace_f = fragment_lookup_trace(dcontext, tag);
        if (trace_f != NULL) {
            /* someone beat us to it!  tough luck -- throw it all away */
            ASSERT(TEST(FRAG_IS_TRACE, trace_f->flags));
            d_r_mutex_unlock(&trace_building_lock);
            trace_abort(dcontext);
            STATS_INC(num_aborted_traces_race);
#ifdef DEBUG
            /* We expect to see this very rarely since we expect to detect
             * practically all races (w/shared BBs anyway) much earlier.
             * FIXME case 8769: we may need another way to prevent races w/
             * -coarse_units!
             */
            if (DYNAMO_OPTION(shared_bbs) && !DYNAMO_OPTION(coarse_units))
                ASSERT_CURIOSITY(false);
#endif
            /* deliberately leave trace_f as it is */
            goto end_and_emit_trace_return;
        }
    }

    /* Delete existing fragment(s) with tag value.
     *
     * For shared traces, if -no_remove_shared_trace_heads, we do not remove
     * shared trace heads and only transfer their links
     * over to the new trace (and if the trace is deleted we transfer the
     * links back).  We leave them alone otherwise, shadowed in both the DR
     * lookup tables and ibl tables.
     * FIXME: trace head left w/ no incoming -- will this break assumptions?
     * What if someone who held ptr before trace emit, or does a different
     * lookup, tries to mess w/ trace head's links?
     */
    if (cur_f != NULL && cur_f->tag == tag) {
        /* Optimization: could repeat for shared as well but we don't bother */
        if (!TEST(FRAG_SHARED, cur_f->flags))
            trace_head_f = cur_f;
        /* Yipes, we're deleting the fragment we're supposed to execute next.
         * Set cur_f to NULL even if not deleted, since we want to
         * execute the trace in preference to the trace head.
         */
        cur_f = NULL;
    }
    /* remove private trace head fragment, if any */
    if (trace_head_f == NULL) /* from cur_f */
        trace_head_f = fragment_lookup_same_sharing(dcontext, tag, 0 /*FRAG_PRIVATE*/);
    /* We do not go through other threads and delete their private trace heads,
     * presuming that they have them for a reason and don't want this shared trace
     */
    if (trace_head_f != NULL) {
        LOG(THREAD, LOG_MONITOR, 4, "deleting private trace head fragment\n");
        /* we have to manually check last_exit -- can't have fragment_delete()
         * call monitor_remove_fragment() to avoid aborting our trace
         */
        if (trace_head_f == dcontext->last_fragment)
            last_exit_deleted(dcontext);
        /* If the trace is private, don't delete the head: the trace will simply
         * shadow it.  If the trace is shared, we have to delete it.  We'll re-create
         * the head as a shared bb if we ever do build a custom trace through it.
         */
        if (!TEST(FRAG_SHARED, md->trace_flags)) {
            replace_trace_head = true;
            /* we can't have our trace_head_f clobbered below */
            CLIENT_ASSERT(!DYNAMO_OPTION(shared_bbs),
                          "invalid private trace head and "
                          "private traces but -shared_bbs for custom traces");
        } else {
            fragment_delete(dcontext, trace_head_f,
                            FRAGDEL_NO_OUTPUT | FRAGDEL_NO_MONITOR);
        }
        if (!replace_trace_head) {
            trace_head_f = NULL;
            STATS_INC(num_fragments_deleted_trace_heads);
        }
    }
    /* find shared trace head fragment, if any */
    if (DYNAMO_OPTION(shared_bbs)) {
        trace_head_f = fragment_lookup_fine_and_coarse_sharing(dcontext, tag, &wrapper,
                                                               NULL, FRAG_SHARED);
        if (!TEST(FRAG_SHARED, md->trace_flags)) {
            /* trace is private, so we can emit as a shadow of trace head */
        } else if (trace_head_f != NULL) {
            /* we don't remove until after emitting a shared trace to avoid races
             * with trace head being re-created before the trace is visible
             */
            replace_trace_head = true;
            if (!TEST(FRAG_IS_TRACE_HEAD, trace_head_f->flags)) {
                ASSERT(TEST(FRAG_COARSE_GRAIN, trace_head_f->flags));
                /* local wrapper so change_linking_lock not needed to change flags */
                trace_head_f->flags |= FRAG_IS_TRACE_HEAD;
            }
        }
    }

    /* Prevent deletion of last_fragment, which may be in the same
     * cache as our trace (esp. w/ a MUST_END_TRACE trace head, since then the
     * last_fragment can be another trace) from clobbering our trace!
     * FIXME: would be cleaner to remove the need to abort the trace if
     * last_fragment is deleted, but tricky to do that (see
     * monitor_remove_fragment).  Could also use a special monitor_data_t field
     * saying "ignore last_exit, I'm emitting now."
     */
    if (!LINKSTUB_FAKE(dcontext->last_exit)) /* head delete may have already done this */
        last_exit_deleted(dcontext);
    ASSERT(md->last_fragment == NULL);
    ASSERT(md->last_copy == NULL);
    /* ensure trace was NOT aborted */
    ASSERT(md->trace_tag == tag);

    /* emit trace fragment into fcache with tag value */
    if (replace_trace_head) {
        trace_f = emit_fragment_as_replacement(dcontext, tag, trace, md->trace_flags,
                                               md->trace_vmlist, trace_head_f);
    } else {
        trace_f = emit_fragment(dcontext, tag, trace, md->trace_flags, md->trace_vmlist,
                                true /*link*/);
    }
    ASSERT(trace_f != NULL);
    /* our estimate should be conservative
     * if externally mangled, all bets are off for now --
     * FIXME: would be nice to gracefully handle opt or client
     * making the trace too big, and pass back an error msg?
     * Perhaps have lower size bounds when optimization or client
     * interface are on.
     */
    LOG(THREAD, LOG_MONITOR, 3, "Trace estimated size %d vs actual size %d\n",
        md->emitted_size, trace_f->size);
    ASSERT(trace_f->size <= md->emitted_size || externally_mangled);
    /* our calculations should be exact, actually */
    /* with -pad_jmps not exact anymore, we should be able to figure out
     * by how much though FIXME */
    ASSERT_CURIOSITY(trace_f->size == md->emitted_size || externally_mangled ||
                     PAD_FRAGMENT_JMPS(trace_f->flags));
    trace_tr = TRACE_FIELDS(trace_f);
    trace_tr->num_bbs = md->num_blks;
    trace_tr->bbs = (trace_bb_info_t *)nonpersistent_heap_alloc(
        FRAGMENT_ALLOC_DC(dcontext, trace_f->flags),
        md->num_blks * sizeof(trace_bb_info_t) HEAPACCT(ACCT_TRACE));
    for (i = 0; i < md->num_blks; i++)
        trace_tr->bbs[i] = md->blk_info[i].info;

    if (TEST(FRAG_SHARED, md->trace_flags))
        d_r_mutex_unlock(&trace_building_lock);

    RSTATS_INC(num_traces);
    DOSTATS(
        { IF_X86_64(if (FRAG_IS_32(trace_f->flags)) { STATS_INC(num_32bit_traces); }) });
    STATS_ADD(num_bbs_in_all_traces, md->num_blks);
    STATS_TRACK_MAX(max_bbs_in_a_trace, md->num_blks);
    DOLOG(2, LOG_MONITOR, {
        LOG(THREAD, LOG_MONITOR, 1, "Generated trace fragment #%d for tag " PFX "\n",
            GLOBAL_STAT(num_traces), tag);
        disassemble_fragment(dcontext, trace_f, d_r_stats->loglevel < 3);
    });

#ifdef INTERNAL
    DODEBUG({
        if (INTERNAL_OPTION(stress_recreate_pc)) {
            /* verify trace recreation - done here after bb_tag[] is in place */
            stress_test_recreate(dcontext, trace_f, trace);
        }
    });
#endif

    /* we can't call reset_trace_state() until after -remove_trace_components,
     * but we must clear these two before enter_nolinking so that a flusher
     * doesn't access them in an inconsistent state (trace_vmlist is invalid
     * once also pointers are transferred to real fragment)
     */
    md->trace_vmlist = NULL;
    md->trace_tag = NULL;

    /* these calls to fragment_remove_shared_no_flush may become
     * nolinking, meaning we need to hold no locks here, and that when
     * we get back our local fragment_t pointers may be invalid.
     */
    /* remove shared trace head fragment */
    if (trace_head_f != NULL && DYNAMO_OPTION(shared_bbs) &&
        TEST(FRAG_SHARED, md->trace_flags) &&
        /* We leave the head in the coarse table and let the trace shadow it.
         * If we were to remove it we would need a solution to finding it for
         * pc translation, which currently walks the htable.
         */
        !TEST(FRAG_COARSE_GRAIN, trace_head_f->flags) &&
        /* if both shared only remove if option on, and no custom tracing */
        !dr_end_trace_hook_exists() && INTERNAL_OPTION(remove_shared_trace_heads)) {
        fragment_remove_shared_no_flush(dcontext, trace_head_f);
        trace_head_f = NULL;
    }

    if (DYNAMO_OPTION(remove_trace_components)) {
        fragment_t *f;
        /* use private md values, don't trust trace_tr */
        for (i = 1 /*skip trace head*/; i < md->num_blks; i++) {
            f = fragment_lookup_bb(dcontext, md->blk_info[i].info.tag);
            if (f != NULL) {
                if (TEST(FRAG_SHARED, f->flags) && !TEST(FRAG_COARSE_GRAIN, f->flags)) {
                    /* FIXME: grab locks up front instead of on each delete */
                    fragment_remove_shared_no_flush(dcontext, f);
                    trace_head_f = NULL; /* be safe */
                } else
                    fragment_delete(dcontext, f, FRAGDEL_NO_OUTPUT | FRAGDEL_NO_MONITOR);
                STATS_INC(trace_components_deleted);
            }
        }
    }

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, trace);

    md->trace_tag = tag; /* reinstate for reset */
    reset_trace_state(dcontext, true /* might need change_linking_lock */);

#ifdef DEBUG
    /* If we're building shared traces and using shared BBs, FRAG_TRACE_BUILDING
     * shouldn't be set on the trace head fragment. If we're not using shared
     * BBs or are not building shared traces, the flag shouldn't be set then
     * either. Basically, it should never be set at this point, after the call
     * to reset_trace_state() just above.
     */
    if (trace_head_f != NULL)
        ASSERT(!TEST(FRAG_TRACE_BUILDING, trace_head_f->flags));
#endif

end_and_emit_trace_return:
    if (cur_f == NULL && cur_f_tag == tag)
        return trace_f;
    else {
        /* emitting the new trace may have deleted the next fragment to execute
         * best way to find out is to re-look-up the next fragment (this only
         * happens when emitting trace, so rare enough)
         */
        cur_f = fragment_lookup(dcontext, cur_f_tag);
        return cur_f;
    }
}

/* Note: The trace being built currently can be emitted in
 * internal_extend_trace() rather than the next time into monitor_cache_enter()
 * if fragment results in a system call (sysenter) or callback (int 2b), i.e.,
 * is marked FRAG_MUST_END_TRACE.
 */
static fragment_t *
internal_extend_trace(dcontext_t *dcontext, fragment_t *f, linkstub_t *prev_l,
                      uint add_size)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    bool have_locks = false;
    DEBUG_DECLARE(uint pre_emitted_size = md->emitted_size;)

    extend_unmangled_ilist(dcontext, f);

    /* if prev_l is fake, NULL it out */
    if (is_ibl_sourceless_linkstub((const linkstub_t *)prev_l)) {
        ASSERT(!DYNAMO_OPTION(indirect_stubs));
        prev_l = NULL;
    }
    ASSERT(prev_l == NULL || !LINKSTUB_FAKE(prev_l) ||
           /* we track the ordinal of the del linkstub so it's ok */
           prev_l == get_deleted_linkstub(dcontext));

    if (TEST(FRAG_SHARED, f->flags)) {
        /* Case 8419: we must hold a lock to ensure f is not
         * fragment_remove_shared_no_flush()-ed underneath us, eliminating its
         * also fields needed for vm_area_add_to_list() (plus w/ the also field
         * re-used for case 3559 we have crash potential).
         */
        have_locks = true;
        /* lock rank order requires cll before shared_vm_areas */
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
        acquire_vm_areas_lock(dcontext, f->flags);
    }
    if (TEST(FRAG_WAS_DELETED, f->flags)) {
        /* We cannot continue if f is FRAG_WAS_DELETED (case 8419) since
         * fragment_t.also is now invalid!
         */
        STATS_INC(num_trace_next_bb_deleted);
        ASSERT(have_locks);
        if (have_locks) {
            release_vm_areas_lock(dcontext, f->flags);
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
        }
        return end_and_emit_trace(dcontext, f);
    }

    /* We have to calculate the added size before we extend, so we
     * have that passed in, though without the estimate for the mangling
     * of the previous block (thus including only f->size and the exit stub
     * size changes), which we calculate in extend_trace.
     * Existing custom stub code should already be in f->size.
     * FIXME: if we ever have decode_fragment() convert, say, dcontext
     * save/restore to tls, then we'll have to add in its size increases
     * as well.
     */
    md->emitted_size += add_size;

    md->trace_flags |= trace_flags_from_component_flags(f->flags);

    /* call routine in interp.c */
    md->emitted_size += extend_trace(dcontext, f, prev_l);

    LOG(THREAD, LOG_MONITOR, 3, "extending added %d to size of trace => %d total\n",
        md->emitted_size - pre_emitted_size, md->emitted_size);

    vm_area_add_to_list(dcontext, md->trace_tag, &(md->trace_vmlist), md->trace_flags, f,
                        have_locks);
    if (have_locks) {
        /* We must give up change_linking_lock in order to execute
         * create_private_copy (it calls emit()) but we're at a stable state
         * now.
         */
        release_vm_areas_lock(dcontext, f->flags);
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
    }

    DOLOG(3, LOG_MONITOR, {
        LOG(THREAD, LOG_MONITOR, 4, "After extending, trace looks like this:\n");
        instrlist_disassemble(dcontext, md->trace_tag, &md->trace, THREAD);
    });
    /* trace_t extended;  prepare bb for execution to find where to go next. */

    /* For FRAG_MUST_END_TRACE fragments emit trace immediately to prevent
     * trace aborts due to syscalls and callbacks.  See case 3541.
     */
    if (TEST(FRAG_MUST_END_TRACE, f->flags)) {
        /* We don't need to unlink f, but we would need to set FRAG_CANNOT_DELETE to
         * prevent its deletion during emitting from clobbering the trace in the case
         * that last_fragment==f (requires that f targets itself, and f is
         * private like traces -- not possible w/ today's syscall-only MUST_END_TRACE
         * fragments but could happen in the future) -- except that that's a general
         * problem handled by clearing last_exit in end_and_emit_trace, so we do
         * nothing here.
         */
        return end_and_emit_trace(dcontext, f);
    }

    ASSERT(!TEST(FRAG_SHARED, f->flags));
    if (TEST(FRAG_TEMP_PRIVATE, f->flags)) {
        /* We make a private copy earlier for everything other than a normal
         * thread private fragment.
         */
        ASSERT(md->last_fragment == f);
        ASSERT(md->last_copy != NULL);
        ASSERT(md->last_copy->tag == f->tag);
        ASSERT(md->last_fragment == md->last_copy);
    } else {
        /* must store this fragment, and also duplicate its flags so know what
         * to restore.  can't rely on last_exit for restoring since could end up
         * not coming out of cache from last_fragment (e.g., if hit sigreturn)
         */
        md->last_fragment = f;
    }

    /* hold lock across cannot delete changes too, and store of flags */
    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);

    md->last_fragment_flags = f->flags;
    if ((f->flags & FRAG_CANNOT_DELETE) == 0) {
        /* don't let this fragment be deleted, we'll need it as
         * dcontext->last_exit for extend_trace
         */
        f->flags |= FRAG_CANNOT_DELETE;
        LOG(THREAD, LOG_MONITOR, 4, "monitor marked F%d (" PFX ") as un-deletable\n",
            f->id, f->tag);
    }

    /* may end up going through trace head, etc. that isn't linked */
    if ((f->flags & FRAG_LINKED_OUTGOING) != 0) {
        /* unlink so monitor invoked on fragment exit */
        unlink_fragment_outgoing(dcontext, f);
        LOG(THREAD, LOG_MONITOR | LOG_LINKS, 4, "monitor unlinked F%d (" PFX ")\n", f->id,
            f->tag);
    }

    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);

    return f;
}

/* we use last_fragment to hold bb that needs to be restored.
 * it's a field used only by us.
 */
static void
internal_restore_last(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    if (md->last_fragment == NULL)
        return;
    /* must restore fragment used to extend trace to pre-trace-building state.
     * sometimes we come in here from trace_abort and we've already restored
     * the last exit, so check before linking.
     */
    /* need to hold lock for any shared link modification */
    SHARED_FLAGS_RECURSIVE_LOCK(md->last_fragment->flags, acquire, change_linking_lock);
    if ((md->last_fragment_flags & FRAG_LINKED_OUTGOING) != 0 &&
        (md->last_fragment->flags & FRAG_LINKED_OUTGOING) == 0) {
        LOG(THREAD, LOG_MONITOR, 4, "internal monitor: relinking last fragment F%d\n",
            md->last_fragment->id);
        link_fragment_outgoing(dcontext, md->last_fragment, false);
    }
    if ((md->last_fragment_flags & FRAG_CANNOT_DELETE) == 0 &&
        (md->last_fragment->flags & FRAG_CANNOT_DELETE) != 0) {
        LOG(THREAD, LOG_MONITOR, 4,
            "internal monitor: re-marking last fragment F%d as deletable\n",
            md->last_fragment->id);
        md->last_fragment->flags &= ~FRAG_CANNOT_DELETE;
    }
    /* flags may not match, e.g., if frag was marked as trace head */
    ASSERT((md->last_fragment_flags & (FRAG_CANNOT_DELETE | FRAG_LINKED_OUTGOING)) ==
           (md->last_fragment->flags & (FRAG_CANNOT_DELETE | FRAG_LINKED_OUTGOING)));
    /* hold lock across FRAG_CANNOT_DELETE changes and all other flag checks, too */
    SHARED_FLAGS_RECURSIVE_LOCK(md->last_fragment->flags, release, change_linking_lock);

    /* last_fragment is ONLY used for restoring, so kill now, else our own
     * deletion of trace head will cause use to abort single-bb trace
     * (see monitor_remove_fragment)
     *
     * Do NOT reset last_fragment_flags as that field is needed prior to the
     * cache entry and is referenced in monitor_cache_enter().
     */
    if (!TEST(FRAG_TEMP_PRIVATE, md->last_fragment->flags))
        md->last_fragment = NULL;
}

/* if we are building a trace, unfreezes and relinks the last_fragment */
void
monitor_cache_exit(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    /* where processing */
    ASSERT(dcontext->whereami == DR_WHERE_DISPATCH);
    dcontext->whereami = DR_WHERE_MONITOR;
    if (md->trace_tag != NULL && md->last_fragment != NULL) {
        /* unprotect local heap */
        SELF_PROTECT_LOCAL(dcontext, WRITABLE);
        /* must restore fragment to pre-trace-building state */
        internal_restore_last(dcontext);
        /* re-protect local heap */
        SELF_PROTECT_LOCAL(dcontext, READONLY);
    } else if (md->trace_tag == NULL) {
        /* Capture the case where the most recent cache exit was prior to a
         * non-ignorable syscall that used the SYSENTER instruction, which
         * we've seen on XP and 2003. The 'ret' after the SYSENTER executes
         * natively, and this piece of control flow isn't captured during
         * linking so link-time trace head marking doesn't work. (The exit
         * stub is marked as a direct exit.)  The exit stub is reset during
         * syscall handling so indirect-exit trace head marking isn't
         * possible either, so we have to use a dedicated var to capture
         * this case.
         *
         * We need to set trace_sysenter_exit to true or false to prevent a
         * stale value from reaching a later read of the flag.
         *
         * FIXME Rework this to store the last (pre-syscall) exit's fragment flags & tag
         * in a dcontext-private place such as non-shared monitor data.
         * Such a general mechanism will permit us to capture all
         * trace head marking within should_be_trace_head().
         */
        dcontext->trace_sysenter_exit =
            (TEST(FRAG_IS_TRACE, dcontext->last_fragment->flags) &&
             TEST(LINK_NI_SYSCALL, dcontext->last_exit->flags));
    }
    dcontext->whereami = DR_WHERE_DISPATCH;
}

static void
check_fine_to_coarse_trace_head(dcontext_t *dcontext, fragment_t *f)
{
    /* Case 8632: When a fine fragment targets a coarse trace head, we have
     * no way to indicate that (there is no entrance stub for the fine
     * fragments, as once the coarse unit is frozen we can't use its
     * entrance stub).  So we assume that an exit is due to trace headness
     * discovered at link time iff it would now be considered a trace head.
     * FIXME: any cleaner way?
     */
    if (TEST(FRAG_COARSE_GRAIN, f->flags) && !TEST(FRAG_IS_TRACE_HEAD, f->flags) &&
        /* FIXME: We rule out empty fragments -- but in so doing we rule out deleted
         * fragments.  Oh well.
         */
        !TESTANY(FRAG_COARSE_GRAIN | FRAG_FAKE, dcontext->last_fragment->flags)) {
        /* we lock up front since check_for_trace_head() expects it for shared2shared */
        acquire_recursive_lock(&change_linking_lock);
        if (check_for_trace_head(dcontext, dcontext->last_fragment, dcontext->last_exit,
                                 f, true /*have lock*/, false /*not sysenter exit*/)) {
            STATS_INC(num_exits_fine2th_coarse);
        } else {
            /* This does happen: e.g., if we abort a trace, we came from a private fine
             * bb and may target a coarse bb
             */
            STATS_INC(num_exits_fine2non_th_coarse);
        }
        release_recursive_lock(&change_linking_lock);
    }
}

/* This routine maintains the statistics that identify hot code
 * regions, and it controls the building and installation of trace
 * fragments.
 */
fragment_t *
monitor_cache_enter(dcontext_t *dcontext, fragment_t *f)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    bool start_trace = false;
    bool end_trace = false;
    dr_custom_trace_action_t client = CUSTOM_TRACE_DR_DECIDES;
    trace_head_counter_t *ctr;
    uint add_size = 0, prev_mangle_size = 0; /* NOTE these aren't set if end_trace */

    if (DYNAMO_OPTION(disable_traces) || f == NULL) {
        /* nothing to do */
        ASSERT(md->trace_tag == NULL);
        return f;
    }

    /* where processing */
    ASSERT(dcontext->whereami == DR_WHERE_DISPATCH);
    dcontext->whereami = DR_WHERE_MONITOR;

    /* default internal routine */

    /* Ensure we know whether f is a trace head, before we do anything else
     * (xref bug 8637 on not terminating traces b/c we marked as head too late)
     */
    check_fine_to_coarse_trace_head(dcontext, f);

    if (md->trace_tag != NULL) { /* in trace selection mode */

        KSTART(trace_building);

        /* unprotect local heap */
        SELF_PROTECT_LOCAL(dcontext, WRITABLE);
        /* should have restored last fragment on cache exit */
        ASSERT(md->last_fragment == NULL ||
               TEST(FRAG_TEMP_PRIVATE, md->last_fragment->flags));

        /* check for trace ending conditions that can be overridden by client */
        end_trace = (end_trace || TEST(FRAG_IS_TRACE, f->flags) ||
                     TEST(FRAG_IS_TRACE_HEAD, f->flags));
        if (dr_end_trace_hook_exists()) {
            client = instrument_end_trace(dcontext, md->trace_tag, f->tag);
            /* Return values:
             *   CUSTOM_TRACE_DR_DECIDES = use standard termination criteria
             *   CUSTOM_TRACE_END_NOW    = end trace
             *   CUSTOM_TRACE_CONTINUE   = do not end trace
             */
            if (client == CUSTOM_TRACE_END_NOW) {
                DOSTATS({
                    if (!end_trace) {
                        LOG(THREAD, LOG_MONITOR, 3,
                            "Client ending 0x%08x trace early @0x%08x\n", md->trace_tag,
                            f->tag);
                        STATS_INC(custom_traces_stop_early);
                    }
                });
                end_trace = true;
            } else if (client == CUSTOM_TRACE_CONTINUE) {
                DOSTATS({
                    if (end_trace) {
                        LOG(THREAD, LOG_MONITOR, 3,
                            "Client not ending 0x%08x trace @ normal stop @0x%08x\n",
                            md->trace_tag, f->tag);
                        STATS_INC(custom_traces_stop_late);
                    }
                });
                end_trace = false;
            }
            LOG(THREAD, LOG_MONITOR, 4, "Client instrument_end_trace returned %d\n",
                client);
        }
        /* check for conditions signaling end of trace regardless of client */
        end_trace = end_trace || TEST(FRAG_CANNOT_BE_TRACE, f->flags);

#if defined(X86) && defined(X64)
        /* no traces that mix 32 and 64: decode_fragment not set up for it */
        if (TEST(FRAG_32_BIT, f->flags) != TEST(FRAG_32_BIT, md->trace_flags))
            end_trace = true;
#endif

        if (!end_trace) {
            /* we need a regular bb here, not a trace */
            if (TEST(FRAG_IS_TRACE, f->flags)) {
                /* We create an official, shared bb (we DO want to call the client bb
                 * hook, right?).  We do not link the new, shadowed bb.
                 */
                fragment_t *head = NULL;
                if (USE_BB_BUILDING_LOCK())
                    d_r_mutex_lock(&bb_building_lock);
                if (DYNAMO_OPTION(coarse_units)) {
                    /* the existing lookup routines will shadow a coarse bb so we do
                     * a custom lookup
                     */
                    head = fragment_coarse_lookup_wrapper(dcontext, f->tag, &md->wrapper);
                }
                if (head == NULL)
                    head = fragment_lookup_bb(dcontext, f->tag);
                if (head == NULL) {
                    LOG(THREAD, LOG_MONITOR, 3,
                        "Client custom trace 0x%08x requiring shadow bb 0x%08x\n",
                        md->trace_tag, f->tag);
                    SELF_PROTECT_LOCAL(dcontext, WRITABLE);
                    /* We need to mark as trace head to hit the shadowing checks
                     * and asserts when adding to fragment htable and unlinking
                     * on delete.
                     */
                    head = build_basic_block_fragment(
                        dcontext, f->tag, FRAG_IS_TRACE_HEAD, false /*do not link*/,
                        true /*visible*/, true /*for trace*/, NULL);
                    SELF_PROTECT_LOCAL(dcontext, READONLY);
                    STATS_INC(custom_traces_bbs_built);
                    ASSERT(head != NULL);
                    /* If it's not shadowing we should have linked before htable add.
                     * We shouldn't end up w/ a bb of different sharing than the
                     * trace: custom traces rule out private traces and shared bbs,
                     * and if circumstances changed since the original trace head bb
                     * was made then the trace should have been flushed.
                     */
                    ASSERT((head->flags & FRAG_SHARED) == (f->flags & FRAG_SHARED));
                    if (TEST(FRAG_COARSE_GRAIN, head->flags)) {
                        /* we need a local copy before releasing the lock.
                         * FIXME: share this code sequence w/ d_r_dispatch().
                         */
                        ASSERT(USE_BB_BUILDING_LOCK());
                        fragment_coarse_wrapper(&md->wrapper, f->tag,
                                                FCACHE_ENTRY_PC(head));
                        md->wrapper.flags |= FRAG_IS_TRACE_HEAD;
                        head = &md->wrapper;
                    }
                }
                if (USE_BB_BUILDING_LOCK())
                    d_r_mutex_unlock(&bb_building_lock);
                /* use the bb from here on out */
                f = head;
            }
            if (TEST(FRAG_COARSE_GRAIN, f->flags) || TEST(FRAG_SHARED, f->flags) ||
                md->pass_to_client) {
                /* We need linkstub_t info for trace_exit_stub_size_diff() so we go
                 * ahead and make a private copy here.
                 * For shared fragments, we make a private copy of f to avoid
                 * synch issues with other threads modifying its linkage before
                 * we get back here.  We do it up front now (i#940) to avoid
                 * determinism issues that arise when check_thread_vm_area()
                 * changes its mind over time.
                 */
                create_private_copy(dcontext, f);
                /* operate on new f from here on */
                if (md->trace_tag == NULL) {
                    /* trace was aborted b/c our new fragment clobbered
                     * someone (see comments in create_private_copy) --
                     * when emitting our private bb we can kill the
                     * last_fragment): just exit now
                     */
                    LOG(THREAD, LOG_MONITOR, 4,
                        "Private copy ended up aborting trace!\n");
                    STATS_INC(num_trace_private_copy_abort);
                    /* trace abort happened in emit_fragment, so we went and
                     * undid the clearing of last_fragment by assigning it
                     * to last_copy, must re-clear!
                     */
                    md->last_fragment = NULL;
                    KSTOP(trace_building);
                    return f;
                }
                f = md->last_fragment;
            }

            if (!end_trace &&
                !get_and_check_add_size(dcontext, f, &add_size, &prev_mangle_size)) {
                STATS_INC(num_max_trace_size_enforced);
                end_trace = true;
            }
        }
        if (DYNAMO_OPTION(max_trace_bbs) > 0 &&
            md->num_blks >= DYNAMO_OPTION(max_trace_bbs) && !end_trace) {
            end_trace = true;
            STATS_INC(num_max_trace_bbs_enforced);
        }
        end_trace =
            (end_trace ||
             /* mangling may never use trace buffer memory but just in case */
             !make_room_in_trace_buffer(dcontext, add_size + prev_mangle_size, f));

        if (end_trace && client == CUSTOM_TRACE_CONTINUE) {
            /* had to overide client, log */
            LOG(THREAD, LOG_MONITOR, 2,
                PRODUCT_NAME
                " ignoring Client's decision to "
                "continue trace (cannot trace through next fragment), ending trace "
                "now\n");
        }

        if (end_trace) {
            LOG(THREAD, LOG_MONITOR, 3,
                "NOT extending hot trace (tag " PFX ") with F%d (" PFX ")\n",
                md->trace_tag, f->id, f->tag);

            f = end_and_emit_trace(dcontext, f);
            LOG(THREAD, LOG_MONITOR, 3, "Returning to search mode f=" PFX "\n", f);
        } else {
            LOG(THREAD, LOG_MONITOR, 3,
                "Extending hot trace (tag " PFX ") with F%d (" PFX ")\n", md->trace_tag,
                f->id, f->tag);
            /* add_size is set when !end_trace */
            f = internal_extend_trace(dcontext, f, dcontext->last_exit, add_size);
        }
        dcontext->whereami = DR_WHERE_DISPATCH;
        /* re-protect local heap */
        SELF_PROTECT_LOCAL(dcontext, READONLY);
        KSTOP(trace_building);
        return f;
    }

    /* if got here, md->trace_tag == NULL */

    /* searching for a hot trace head */

    if (TEST(FRAG_IS_TRACE, f->flags)) {
        /* nothing to do */
        dcontext->whereami = DR_WHERE_DISPATCH;
        return f;
    }

    if (!TEST(FRAG_IS_TRACE_HEAD, f->flags)) {

        bool trace_head;

        /* Dynamic marking of trace heads for:
         * - indirect exits
         * - an exit from a trace that ends just before a SYSENTER.
         * - private secondary trace heads targeted by shared traces
         *
         * FIXME Rework this to use the last exit's fragment flags & tag that were
         * stored in a dcontext-private place such as non-shared monitor data.
         */
        if (LINKSTUB_INDIRECT(dcontext->last_exit->flags) ||
            dcontext->trace_sysenter_exit ||
            /* mark private secondary trace heads from shared traces */
            (TESTALL(FRAG_SHARED | FRAG_IS_TRACE, dcontext->last_fragment->flags) &&
             !TESTANY(FRAG_SHARED | FRAG_IS_TRACE, f->flags))) {

            bool need_lock = NEED_SHARED_LOCK(dcontext->last_fragment->flags);
            if (need_lock)
                acquire_recursive_lock(&change_linking_lock);

            /* The exit stub is fake if trace_sysenter_exit is true, but the
             * path thru check_for_trace_head() accounts for that.
             */
            trace_head = check_for_trace_head(dcontext, dcontext->last_fragment,
                                              dcontext->last_exit, f, need_lock,
                                              dcontext->trace_sysenter_exit);

            if (need_lock)
                release_recursive_lock(&change_linking_lock);

            /* link routines will unprotect as necessary, we then re-protect
             * entire fcache
             */
            SELF_PROTECT_CACHE(dcontext, NULL, READONLY);
        } else {
            /* whether direct or fake, not marking a trace head */
            trace_head = false;
        }

        if (!trace_head) {
            dcontext->whereami = DR_WHERE_DISPATCH;
            return f;
        }
    }

    /* Found a trace head, increment its counter */
    ctr = thcounter_lookup(dcontext, f->tag);
    /* May not have been added for this thread yet */
    if (ctr == NULL)
        ctr = thcounter_add(dcontext, f->tag);
    ASSERT(ctr != NULL);

    if (ctr->counter == TH_COUNTER_CREATED_TRACE_VALUE()) {
        /* trace_t head counter values are persistent, so we do not remove them on
         * deletion.  However, when a trace is deleted we clear the counter, to
         * prevent the new bb from immediately being considered hot, to help
         * with phased execution (trace may no longer be hot).  To avoid having
         * walk every thread for every trace deleted we use a lazy strategy,
         * recognizing a counter that has already reached the threshold with a
         * sentinel value.
         */
        ctr->counter = INTERNAL_OPTION(trace_counter_on_delete);
        STATS_INC(th_counter_reset);
    }

    ctr->counter++;
    /* Should never be > here (assert is down below) but we check just in case */
    if (ctr->counter >= INTERNAL_OPTION(trace_threshold)) {
        /* if cannot delete fragment, do not start trace -- wait until
         * can delete it (w/ exceptions, deletion status changes). */
        if (!TEST(FRAG_CANNOT_DELETE, f->flags)) {
            if (!DYNAMO_OPTION(shared_traces))
                start_trace = true;
            /* FIXME To detect a trace building race w/private BBs at this point,
             * we need a presence table to mark that a tag is being used for trace
             * building. Generic hashtables can help with this (case 6206).
             */
            else if (!DYNAMO_OPTION(shared_bbs) || !TEST(FRAG_SHARED, f->flags))
                start_trace = true;
            else {
                /* Check if trace building is in progress and act accordingly. */
                ASSERT(TEST(FRAG_SHARED, f->flags));
                /* Hold the change linking lock for flags changes. */
                acquire_recursive_lock(&change_linking_lock);
                if (TEST(FRAG_TRACE_BUILDING, f->flags)) {
                    /* trace_t building w/this tag is already in-progress. */
                    LOG(THREAD, LOG_MONITOR, 3,
                        "Not going to start trace with already-in-trace-progress F%d "
                        "(tag " PFX ")\n",
                        f->id, f->tag);
                    STATS_INC(num_trace_building_race);
                } else {
                    LOG(THREAD, LOG_MONITOR, 3,
                        "Going to start trace with F%d (tag " PFX ")\n", f->id, f->tag);
                    f->flags |= FRAG_TRACE_BUILDING;
                    start_trace = true;
                }
                release_recursive_lock(&change_linking_lock);
            }
        }
    }

    if (start_trace) {
        /* We need to set pass_to_client before cloning */
        /* PR 299808: cache whether we need to re-build bbs for clients up front,
         * to be consistent across whole trace.  If client later unregisters bb
         * hook then it will miss our call on constituent bbs: that's its problem.
         * We document that trace and bb hooks should not be unregistered.
         */
        md->pass_to_client = mangle_trace_at_end();
        /* should already be initialized */
        ASSERT(instrlist_first(&md->unmangled_ilist) == NULL);
    }
    if (start_trace &&
        (TEST(FRAG_COARSE_GRAIN, f->flags) || TEST(FRAG_SHARED, f->flags) ||
         md->pass_to_client)) {
        ASSERT(TEST(FRAG_IS_TRACE_HEAD, f->flags));
        /* We need linkstub_t info for trace_exit_stub_size_diff() so we go
         * ahead and make a private copy here.
         * For shared fragments, we make a private copy of f to avoid
         * synch issues with other threads modifying its linkage before
         * we get back here.  We do it up front now (i#940) to avoid
         * determinism issues that arise when check_thread_vm_area()
         * changes its mind over time.
         */
        create_private_copy(dcontext, f);
        /* operate on new f from here on */
        f = md->last_fragment;
    }
    if (!start_trace && ctr->counter >= INTERNAL_OPTION(trace_threshold)) {
        /* Back up the counter by one. This ensures that the
         * counter will be == trace_threshold if this thread is later
         * able to start building a trace w/this tag and ensures
         * that our one-up sentinel works for lazy clearing.
         */
        LOG(THREAD, LOG_MONITOR, 3, "Backing up F%d counter from %d\n", f->id,
            ctr->counter);
        ctr->counter--;
        ASSERT(ctr->counter < INTERNAL_OPTION(trace_threshold));
    }
    if (start_trace) {
        KSTART(trace_building);
        /* ensure our sentinel counter value for counter clearing will work */
        ASSERT(ctr->counter == INTERNAL_OPTION(trace_threshold));
        ctr->counter = TH_COUNTER_CREATED_TRACE_VALUE();
        /* Found a hot trace head.  Switch this thread into trace
           selection mode, and initialize the instrlist_t for the new
           trace fragment with this block fragment.  Leave the
           trace head entry locked so no one else tries to build
           a trace from it.  Assume that a trace would never
           contain just one block, and thus we don't have to check
           for end of trace condition here. */
        /* unprotect local heap */
        SELF_PROTECT_LOCAL(dcontext, WRITABLE);
#ifdef TRACE_HEAD_CACHE_INCR
        /* we don't have to worry about skipping the cache incr routine link
         * in the future since we can only encounter the trace head in our
         * no-link trace-building mode, then we will delete it
         */
#endif
        md->trace_tag = f->tag;
        md->trace_flags = trace_flags_from_trace_head_flags(f->flags);
        md->emitted_size = fragment_prefix_size(md->trace_flags);
#ifdef PROFILE_RDTSC
        if (dynamo_options.profile_times)
            md->emitted_size += profile_call_size();
#endif
        LOG(THREAD, LOG_MONITOR, 2, "Found hot trace head F%d (tag " PFX ")\n", f->id,
            f->tag);
        LOG(THREAD, LOG_MONITOR, 3, "Entering trace selection mode\n");
        /* allocate trace buffer space */
        /* we should have a bb here, since if a trace can't also be a trace head */
        ASSERT(!TEST(FRAG_IS_TRACE, f->flags));
        if (!get_and_check_add_size(dcontext, f, &add_size, &prev_mangle_size) ||
            /* mangling may never use trace buffer memory but just in case */
            !make_room_in_trace_buffer(
                dcontext, md->emitted_size + add_size + prev_mangle_size, f)) {
            LOG(THREAD, LOG_MONITOR, 1, "bb %d (" PFX ") too big (%d) %s\n", f->id,
                f->tag, f->size,
                get_and_check_add_size(dcontext, f, NULL, NULL)
                    ? "trace buffer"
                    : "trace body limit / trace cache size");
            /* turn back into a non-trace head */
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
            f->flags &= ~FRAG_IS_TRACE_HEAD;
            /* make sure not marked as trace head again */
            f->flags |= FRAG_CANNOT_BE_TRACE;
            STATS_INC(num_huge_fragments);
            /* have to relink incoming frags */
            link_fragment_incoming(dcontext, f, false /*not new*/);
            /* call reset_trace_state while holding the lock since it
             * may manipulate frag flags */
            reset_trace_state(dcontext, false /* already own change_linking_lock */);
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
            /* FIXME: set CANNOT_BE_TRACE when first create a too-big fragment?
             * export the size expansion factors considered?
             */
            /* now return */
            dcontext->whereami = DR_WHERE_DISPATCH;
            /* link unprotects on demand, we then re-protect all */
            SELF_PROTECT_CACHE(dcontext, NULL, READONLY);
            /* re-protect local heap */
            SELF_PROTECT_LOCAL(dcontext, READONLY);
            KSTOP(trace_building);
            return f;
        }
        f = internal_extend_trace(dcontext, f, NULL, add_size);

        /* re-protect local heap */
        SELF_PROTECT_LOCAL(dcontext, READONLY);
        KSTOP(trace_building);
    } else {
        /* Not yet hot */
        KSWITCH(monitor_enter_thci);
    }

    /* release rest of state */
    dcontext->whereami = DR_WHERE_DISPATCH;
    return f;
}

/* This routine internally calls enter_couldbelinking, thus it is safe
 * to call from any linking state. Restores linking to previous state at exit.
 * If calling on another thread, caller should be synchronized with that thread
 * (either via flushing synch or thread_synch methods) FIXME : verify all users
 * on other threads are properly synchronized
 */
void
trace_abort(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    instrlist_t *trace;
    bool prevlinking = true;

    if (md->trace_tag == NULL && md->last_copy == NULL)
        return; /* NOT in trace selection mode */

    /* we're changing linking state -- and we're often called from
     * non-could-be-linking locations, so we synch w/ flusher here.
     * additionally we are changing trace state that the flusher
     * reads, and we could have a race condition, so we consider
     * that to be a linking change as well. If we are the flusher
     * then the synch is unnecessary and could even cause a livelock.
     */
    if (!is_self_flushing()) {
        if (!is_couldbelinking(dcontext)) {
            prevlinking = false;
            enter_couldbelinking(dcontext, NULL, false /*not a cache transition*/);
        }
    }

    /* must relink unlinked trace-extending fragment
     * cannot use last_exit, must use our own last_fragment just for this
     * purpose, b/c may not exit cache from last_fragment
     * (e.g., if hit sigreturn!)
     */
    if (md->last_fragment != NULL) {
        internal_restore_last(dcontext);
    }

    /* i#791: We can't delete last copy yet because we could still be executing
     * in that fragment.  For example, a client could have a clean call that
     * flushes.  We'll delete the last_copy when we start the next trace or at
     * thread exit instead.
     */

    /* free the instrlist_t elements */
    trace = &md->trace;
    instrlist_clear(dcontext, trace);

    if (md->trace_vmlist != NULL) {
        vm_area_destroy_list(dcontext, md->trace_vmlist);
        md->trace_vmlist = NULL;
    }
    STATS_INC(num_aborted_traces);
    STATS_ADD(num_bbs_in_all_aborted_traces, md->num_blks);
    reset_trace_state(dcontext, true /* might need change_linking_lock */);

    if (!prevlinking)
        enter_nolinking(dcontext, NULL, false /*not a cache transition*/);
}

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/* PR 204770: use trace component bb tag for RCT source address */
app_pc
get_trace_exit_component_tag(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    linkstub_t *stub;
    uint exitnum = 0;
    uint i, num;
    app_pc tag = f->tag;
    bool found = false;
    trace_only_t *t = TRACE_FIELDS(f);
    ASSERT(TEST(FRAG_IS_TRACE, f->flags));
    ASSERT(linkstub_fragment(dcontext, l) == f);
    for (stub = FRAGMENT_EXIT_STUBS(f); stub != NULL; stub = LINKSTUB_NEXT_EXIT(stub)) {
        if (stub == l) {
            found = true;
            break;
        }
        exitnum++;
    }
    ASSERT(found);
    if (!found) {
        LOG(THREAD, LOG_MONITOR, 2,
            "get_trace_exit_component_tag F%d(" PFX "): can't find exit!\n", f->id,
            f->tag);
        return f->tag;
    }
    ASSERT(exitnum < t->num_bbs);
    /* If we have coarse bbs, or max_elide_* is 0, we won't elide during bb building
     * but we will during trace building.  Rather than recreate each bb and figure
     * out how many exits it contributed, we store that information.
     */
    found = false;
    for (i = 0, num = 0; i < t->num_bbs; i++) {
        if (exitnum < num + t->bbs[i].num_exits) {
            found = true;
            tag = t->bbs[i].tag;
            break;
        }
        num += t->bbs[i].num_exits;
    }
    ASSERT(found);
    LOG(THREAD, LOG_MONITOR, 4,
        "get_trace_exit_component_tag F%d(" PFX ") => bb #%d (exit #%d): " PFX "\n",
        f->id, f->tag, i, exitnum, tag);
    return tag;
}
#endif /* defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH) */
