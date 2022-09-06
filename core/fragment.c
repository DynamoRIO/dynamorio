/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

/*
 * fragment.c - fragment related routines
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "emit.h"
#include "monitor.h"
#include "instrument.h"
#include <stddef.h> /* for offsetof */
#include <limits.h> /* UINT_MAX */
#include "perscache.h"
#include "synch.h"
#ifdef UNIX
#    include "nudge.h"
#endif

/* FIXME: make these runtime parameters */
#define INIT_HTABLE_SIZE_SHARED_BB (DYNAMO_OPTION(coarse_units) ? 5 : 10)
#define INIT_HTABLE_SIZE_SHARED_TRACE 10
/* the only private bbs will be selfmod, so start small */
#define INIT_HTABLE_SIZE_BB (DYNAMO_OPTION(shared_bbs) ? 5 : 10)
/* coarse-grain fragments do not use futures */
#define INIT_HTABLE_SIZE_SHARED_FUTURE (DYNAMO_OPTION(coarse_units) ? 5 : 10)
#ifdef RETURN_AFTER_CALL
/* we have small per-module hashtables */
#    define INIT_HTABLE_SIZE_AFTER_CALL 5
#endif
/* private futures are only used when we have private fragments */
#define INIT_HTABLE_SIZE_FUTURE \
    ((DYNAMO_OPTION(shared_bbs) && DYNAMO_OPTION(shared_traces)) ? 5 : 9)

/* per-module htables */
#define INIT_HTABLE_SIZE_COARSE 5
#define INIT_HTABLE_SIZE_COARSE_TH 4

#ifdef RCT_IND_BRANCH
#    include "rct.h"
/* we have small per-module hashtables */
#    define INIT_HTABLE_SIZE_RCT_IBT 7

#    ifndef RETURN_AFTER_CALL
#        error RCT_IND_BRANCH requires RETURN_AFTER_CALL since it reuses data types
#    endif
#endif

/* if shared traces, we currently have no private traces so make table tiny
 * FIMXE: should start out w/ no table at all
 */
#define INIT_HTABLE_SIZE_TRACE (DYNAMO_OPTION(shared_traces) ? 6 : 9)
/* for small table sizes resize is not an expensive operation and we start smaller */

/* Current flusher, protected by thread_initexit_lock. */
DECLARE_FREQPROT_VAR(static dcontext_t *flusher, NULL);
/* Current allsynch-flusher, protected by thread_initexit_lock. */
DECLARE_FREQPROT_VAR(static dcontext_t *allsynch_flusher, NULL);
/* Current flush base and size, protected by thread_initexit_lock. */
DECLARE_FREQPROT_VAR(static app_pc flush_base, NULL);
DECLARE_FREQPROT_VAR(static size_t flush_size, 0);

/* These global tables are kept on the heap for selfprot (case 7957) */

/* synchronization to these tables is accomplished via read-write locks,
 * where the writers are removal and resizing -- addition is atomic to
 * readers.
 * for now none of these are read from ibl routines so we only have to
 * synch with other DR routines
 */
static fragment_table_t *shared_bb;
static fragment_table_t *shared_trace;

/* if we have either shared bbs or shared traces we need this shared: */
static fragment_table_t *shared_future;

/* Thread-shared tables are allocated in a shared per_thread_t.
 * The structure is also used if we're dumping shared traces.
 * Kept on the heap for selfprot (case 7957)
 */
static per_thread_t *shared_pt;

#define USE_SHARED_PT() \
    (SHARED_IBT_TABLES_ENABLED() || (TRACEDUMP_ENABLED() && DYNAMO_OPTION(shared_traces)))

/* We keep track of "old" IBT target tables in a linked list and
 * deallocate them in fragment_exit(). */
/* FIXME Deallocate tables more aggressively using a distributed, refcounting
 * algo as is used for shared deletion. */
typedef struct _dead_fragment_table_t {
    fragment_entry_t *table_unaligned;
    uint table_flags;
    uint capacity;
    uint ref_count;
    struct _dead_fragment_table_t *next;
} dead_fragment_table_t;

/* We keep these list pointers on the heap for selfprot (case 8074). */
typedef struct _dead_table_lists_t {
    dead_fragment_table_t *dead_tables;
    dead_fragment_table_t *dead_tables_tail;
} dead_table_lists_t;

static dead_table_lists_t *dead_lists;

DECLARE_CXTSWPROT_VAR(static mutex_t dead_tables_lock, INIT_LOCK_FREE(dead_tables_lock));

#ifdef RETURN_AFTER_CALL
/* High level lock for an atomic lookup+add operation on the
 * after call tables. */
DECLARE_CXTSWPROT_VAR(static mutex_t after_call_lock, INIT_LOCK_FREE(after_call_lock));
/* We use per-module tables and only need this table for non-module code;
 * on Linux though this is the only table used, until we have a module list.
 */
static rct_module_table_t rac_non_module_table;
#endif

/* allows independent sequences of flushes and delayed deletions,
 * though with -syscalls_synch_flush additions we now hold this
 * throughout a flush.
 */
DECLARE_CXTSWPROT_VAR(mutex_t shared_cache_flush_lock,
                      INIT_LOCK_FREE(shared_cache_flush_lock));
/* Global count of flushes, used as a timestamp for shared deletion.
 * Reads may be done w/o a lock, but writes can only be done
 * via increment_global_flushtime() while holding shared_cache_flush_lock.
 */
DECLARE_FREQPROT_VAR(uint flushtime_global, 0);

DECLARE_CXTSWPROT_VAR(mutex_t client_flush_request_lock,
                      INIT_LOCK_FREE(client_flush_request_lock));
DECLARE_CXTSWPROT_VAR(client_flush_req_t *client_flush_requests, NULL);

#if defined(RCT_IND_BRANCH) && defined(UNIX)
/* On Win32 we use per-module tables; on Linux we use a single global table,
 * until we have a module list.
 */
rct_module_table_t rct_global_table;
#endif

#define NULL_TAG ((app_pc)PTR_UINT_0)
/* FAKE_TAG is used as a deletion marker for unlinked entries */
#define FAKE_TAG ((app_pc)PTR_UINT_MINUS_1)

/* instead of an empty hashtable slot containing NULL, we fill it
 * with a pointer to this constant fragment, which we give a tag
 * of 0.
 * PR 305731: rather than having a start_pc of 0, which causes
 * an app targeting 0 to crash at 0, we point at a handler that
 * sends the app to an ibl miss.
 */
byte *hashlookup_null_target;
#define HASHLOOKUP_NULL_START_PC ((cache_pc)hashlookup_null_handler)
static const fragment_t null_fragment = {
    NULL_TAG, 0, 0, 0, 0, HASHLOOKUP_NULL_START_PC,
};
/* to avoid range check on fast path using an end of table sentinel fragment */
static const fragment_t sentinel_fragment = {
    NULL_TAG, 0, 0, 0, 0, HASHLOOKUP_SENTINEL_START_PC,
};

/* Shared fragment IBTs: We need to preserve the open addressing traversal
 * in the hashtable while marking a table entry as unlinked.
 * A null_fragment won't work since it terminates the traversal,
 * so we use an unlinked marker. The lookup table entry for
 * an unlinked entry *always* has its start_pc_fragment set to
 * an IBL target_delete entry.
 */
static const fragment_t unlinked_fragment = {
    FAKE_TAG,
};

/* macro used in the code from time of deletion markers */
/* Shared fragment IBTs: unlinked_fragment isn't a real fragment either. So they
 * are naturally deleted during a table resize. */
#define REAL_FRAGMENT(fragment)                                          \
    ((fragment) != &null_fragment && (fragment) != &unlinked_fragment && \
     (fragment) != &sentinel_fragment)

#define GET_PT(dc)                                                  \
    ((dc) == GLOBAL_DCONTEXT ? (USE_SHARED_PT() ? shared_pt : NULL) \
                             : (per_thread_t *)(dc)->fragment_field)

#define TABLE_PROTECTED(ptable) \
    (!TABLE_NEEDS_LOCK(ptable) || READWRITE_LOCK_HELD(&(ptable)->rwlock))

/* everything except the invisible table is in here */
#define GET_FTABLE_HELPER(pt, flags, otherwise)                               \
    (TEST(FRAG_IS_TRACE, (flags))                                             \
         ? (TEST(FRAG_SHARED, (flags)) ? shared_trace : &pt->trace)           \
         : (TEST(FRAG_SHARED, (flags))                                        \
                ? (TEST(FRAG_IS_FUTURE, (flags)) ? shared_future : shared_bb) \
                : (TEST(FRAG_IS_FUTURE, (flags)) ? &pt->future : (otherwise))))

#define GET_FTABLE(pt, flags) GET_FTABLE_HELPER(pt, (flags), &pt->bb)

/* indirect branch table per target type (bb vs trace) and indirect branch type */
#define GET_IBT_TABLE(pt, flags, branch_type)                                       \
    (TEST(FRAG_IS_TRACE, (flags))                                                   \
         ? (DYNAMO_OPTION(shared_trace_ibt_tables)                                  \
                ? &shared_pt->trace_ibt[(branch_type)]                              \
                : &(pt)->trace_ibt[(branch_type)])                                  \
         : (DYNAMO_OPTION(shared_bb_ibt_tables) ? &shared_pt->bb_ibt[(branch_type)] \
                                                : &(pt)->bb_ibt[(branch_type)]))

/********************************** STATICS ***********************************/
static uint
fragment_heap_size(uint flags, int direct_exits, int indirect_exits);

static void
fragment_free_future(dcontext_t *dcontext, future_fragment_t *fut);

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
static void
coarse_persisted_fill_ibl(dcontext_t *dcontext, coarse_info_t *info,
                          ibl_branch_type_t branch_type);
#endif

static void
process_client_flush_requests(dcontext_t *dcontext, dcontext_t *alloc_dcontext,
                              client_flush_req_t *req, bool flush);

/* trace logging and synch for shared trace file: */
DECLARE_CXTSWPROT_VAR(static mutex_t tracedump_mutex, INIT_LOCK_FREE(tracedump_mutex));
DECLARE_FREQPROT_VAR(static stats_int_t tcount, 0); /* protected by tracedump_mutex */
static void
exit_trace_file(per_thread_t *pt);
static void
output_trace(dcontext_t *dcontext, per_thread_t *pt, fragment_t *f,
             stats_int_t deleted_at);
static void
init_trace_file(per_thread_t *pt);

#define SHOULD_OUTPUT_FRAGMENT(flags)                                     \
    (TEST(FRAG_IS_TRACE, (flags)) && !TEST(FRAG_TRACE_OUTPUT, (flags)) && \
     TRACEDUMP_ENABLED())

#define FRAGMENT_COARSE_WRAPPER_FLAGS                                    \
    FRAG_FAKE | FRAG_SHARED | FRAG_COARSE_GRAIN | FRAG_LINKED_OUTGOING | \
        FRAG_LINKED_INCOMING

/* We use temporary fragment_t + linkstub_t structs to more easily
 * use existing code when emitting coarse-grain fragments.
 * Only 1-ind-exit or 1 or 2 dir exit bbs can be coarse-grain.
 * The bb_building_lock protects use of this.
 */
DECLARE_FREQPROT_VAR(
    static struct {
        fragment_t f;
        union {
            struct {
                direct_linkstub_t dir_exit_1;
                direct_linkstub_t dir_exit_2;
            } dir_exits;
            indirect_linkstub_t ind_exit;
        } exits;
    } coarse_emit_fragment,
    { { 0 } });

#ifdef SHARING_STUDY
/***************************************************************************
 * fragment_t sharing study
 * Only used with -fragment_sharing_study
 * When the option is off we go ahead and waste the 4 static vars
 * below so we don't have to have a define and separate build.
 */
typedef struct _thread_list_t {
    uint thread_num;
    uint count;
    struct _thread_list_t *next;
} thread_list_t;

typedef struct _shared_entry_t {
    app_pc tag;
    uint num_threads;
    thread_list_t *threads;
    uint heap_size;
    uint cache_size;
    struct _shared_entry_t *next;
} shared_entry_t;
#    define SHARED_HASH_BITS 16
static shared_entry_t **shared_blocks;
DECLARE_CXTSWPROT_VAR(static mutex_t shared_blocks_lock,
                      INIT_LOCK_FREE(shared_blocks_lock));
static shared_entry_t **shared_traces;
DECLARE_CXTSWPROT_VAR(static mutex_t shared_traces_lock,
                      INIT_LOCK_FREE(shared_traces_lock));

/* assumes caller holds table's lock! */
static shared_entry_t *
shared_block_lookup(shared_entry_t **table, fragment_t *f)
{
    shared_entry_t *e;
    uint hindex;

    hindex = HASH_FUNC_BITS((ptr_uint_t)f->tag, SHARED_HASH_BITS);
    /* using collision chains */
    for (e = table[hindex]; e != NULL; e = e->next) {
        if (e->tag == f->tag) {
            return e;
        }
    }
    return NULL;
}

static void
reset_shared_block_table(shared_entry_t **table, mutex_t *lock)
{
    shared_entry_t *e, *nxte;
    uint i;
    uint size = HASHTABLE_SIZE(SHARED_HASH_BITS);
    d_r_mutex_lock(lock);
    for (i = 0; i < size; i++) {
        for (e = table[i]; e != NULL; e = nxte) {
            thread_list_t *tl = e->threads;
            thread_list_t *tlnxt;
            nxte = e->next;
            while (tl != NULL) {
                tlnxt = tl->next;
                global_heap_free(tl, sizeof(thread_list_t) HEAPACCT(ACCT_OTHER));
                tl = tlnxt;
            }
            global_heap_free(e, sizeof(shared_entry_t) HEAPACCT(ACCT_OTHER));
        }
    }
    global_heap_free(table, size * sizeof(shared_entry_t *) HEAPACCT(ACCT_OTHER));
    d_r_mutex_unlock(lock);
}

static void
add_shared_block(shared_entry_t **table, mutex_t *lock, fragment_t *f)
{
    shared_entry_t *e;
    uint hindex;
    int num_direct = 0, num_indirect = 0;
    linkstub_t *l = FRAGMENT_EXIT_STUBS(f);
    /* use num to avoid thread_id_t recycling problems */
    uint tnum = get_thread_num(d_r_get_thread_id());

    d_r_mutex_lock(lock);
    e = shared_block_lookup(table, f);
    if (e != NULL) {
        thread_list_t *tl = e->threads;
        for (; tl != NULL; tl = tl->next) {
            if (tl->thread_num == tnum) {
                tl->count++;
                LOG(GLOBAL, LOG_ALL, 2,
                    "add_shared_block: tag " PFX ", but re-add #%d for thread #%d\n",
                    e->tag, tl->count, tnum);
                d_r_mutex_unlock(lock);
                return;
            }
        }
        tl = global_heap_alloc(sizeof(thread_list_t) HEAPACCT(ACCT_OTHER));
        tl->thread_num = tnum;
        tl->count = 1;
        tl->next = e->threads;
        e->threads = tl;
        e->num_threads++;
        LOG(GLOBAL, LOG_ALL, 2,
            "add_shared_block: tag " PFX " thread #%d => %d threads\n", e->tag, tnum,
            e->num_threads);
        d_r_mutex_unlock(lock);
        return;
    }

    /* get num stubs to find heap size */
    for (; l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        if (LINKSTUB_DIRECT(l->flags))
            num_direct++;
        else {
            ASSERT(LINKSTUB_INDIRECT(l->flags));
            num_indirect++;
        }
    }

    /* add entry to thread hashtable */
    e = (shared_entry_t *)global_heap_alloc(sizeof(shared_entry_t) HEAPACCT(ACCT_OTHER));
    e->tag = f->tag;
    e->num_threads = 1;
    e->heap_size = fragment_heap_size(f->flags, num_direct, num_indirect);
    e->cache_size = (f->size + f->fcache_extra);
    e->threads = global_heap_alloc(sizeof(thread_list_t) HEAPACCT(ACCT_OTHER));
    e->threads->thread_num = tnum;
    e->threads->count = 1;
    e->threads->next = NULL;
    LOG(GLOBAL, LOG_ALL, 2,
        "add_shared_block: tag " PFX ", heap %d, cache %d, thread #%d\n", e->tag,
        e->heap_size, e->cache_size, e->threads->thread_num);

    hindex = HASH_FUNC_BITS((ptr_uint_t)f->tag, SHARED_HASH_BITS);
    e->next = table[hindex];
    table[hindex] = e;
    d_r_mutex_unlock(lock);
}

static void
print_shared_table_stats(shared_entry_t **table, mutex_t *lock, const char *name)
{
    uint i;
    shared_entry_t *e;
    uint size = HASHTABLE_SIZE(SHARED_HASH_BITS);
    uint tot = 0, shared_tot = 0, shared = 0, heap = 0, cache = 0, creation_count = 0;

    d_r_mutex_lock(lock);
    for (i = 0; i < size; i++) {
        for (e = table[i]; e != NULL; e = e->next) {
            thread_list_t *tl = e->threads;
            tot++;
            shared_tot += e->num_threads;
            for (; tl != NULL; tl = tl->next)
                creation_count += tl->count;
            if (e->num_threads > 1) {
                shared++;
                /* assume similar size for each thread -- cache padding
                 * only real difference
                 */
                heap += (e->heap_size * e->num_threads);
                cache += (e->cache_size * e->num_threads);
            }
        }
    }
    d_r_mutex_unlock(lock);
    LOG(GLOBAL, LOG_ALL, 1, "Shared %s statistics:\n", name);
    LOG(GLOBAL, LOG_ALL, 1, "\ttotal blocks:   %10d\n", tot);
    LOG(GLOBAL, LOG_ALL, 1, "\tcreation count: %10d\n", creation_count);
    LOG(GLOBAL, LOG_ALL, 1, "\tshared count:   %10d\n", shared_tot);
    LOG(GLOBAL, LOG_ALL, 1, "\tshared blocks:  %10d\n", shared);
    LOG(GLOBAL, LOG_ALL, 1, "\tshared heap:    %10d\n", heap);
    LOG(GLOBAL, LOG_ALL, 1, "\tshared cache:   %10d\n", cache);
}

void
print_shared_stats()
{
    print_shared_table_stats(shared_blocks, &shared_blocks_lock, "basic block");
    print_shared_table_stats(shared_traces, &shared_traces_lock, "trace");
}
#endif /* SHARING_STUDY ***************************************************/

#ifdef FRAGMENT_SIZES_STUDY /*****************************************/
#    include <math.h>
/* don't bother to synchronize these */
static int bb_sizes[200000];
static int trace_sizes[40000];
static int num_bb = 0;
static int num_traces = 0;

void
record_fragment_size(int size, bool is_trace)
{
    if (is_trace) {
        trace_sizes[num_traces] = size;
        num_traces++;
        ASSERT(num_traces < 40000);
    } else {
        bb_sizes[num_bb] = size;
        num_bb++;
        ASSERT(num_bb < 200000);
    }
}

void
print_size_results()
{
    LOG(GLOBAL, LOG_ALL, 1, "Basic block sizes (bytes):\n");
    print_statistics(bb_sizes, num_bb);
    LOG(GLOBAL, LOG_ALL, 1, "Trace sizes (bytes):\n");
    print_statistics(trace_sizes, num_traces);
}
#endif /* FRAGMENT_SIZES_STUDY */ /*****************************************/

#define FRAGTABLE_WHICH_HEAP(flags)                                             \
    (TESTALL(FRAG_TABLE_INCLUSIVE_HIERARCHY | FRAG_TABLE_IBL_TARGETED, (flags)) \
         ? ACCT_IBLTABLE                                                        \
         : ACCT_FRAG_TABLE)

#ifdef HASHTABLE_STATISTICS
#    define UNPROT_STAT(stats) unprot_stats->stats
/* FIXME: either put in nonpersistent heap as appropriate, or
 * preserve across resets
 */
#    define ALLOC_UNPROT_STATS(dcontext, table)                               \
        do {                                                                  \
            (table)->unprot_stats = HEAP_TYPE_ALLOC(                          \
                (dcontext), unprot_ht_statistics_t,                           \
                FRAGTABLE_WHICH_HEAP((table)->table_flags), UNPROTECTED);     \
            memset((table)->unprot_stats, 0, sizeof(unprot_ht_statistics_t)); \
        } while (0)
#    define DEALLOC_UNPROT_STATS(dcontext, table)                                 \
        HEAP_TYPE_FREE((dcontext), (table)->unprot_stats, unprot_ht_statistics_t, \
                       FRAGTABLE_WHICH_HEAP((table)->table_flags), UNPROTECTED)
#    define CHECK_UNPROT_STATS(table) ASSERT(table.unprot_stats != NULL)

static void
check_stay_on_trace_stats_overflow(dcontext_t *dcontext, ibl_branch_type_t branch_type)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    hashtable_statistics_t *lookup_stats =
        &pt->trace_ibt[branch_type].unprot_stats->trace_ibl_stats[branch_type];
    if (lookup_stats->ib_stay_on_trace_stat < lookup_stats->ib_stay_on_trace_stat_last) {
        lookup_stats->ib_stay_on_trace_stat_ovfl++;
    }
    lookup_stats->ib_stay_on_trace_stat_last = lookup_stats->ib_stay_on_trace_stat;
    /* FIXME: ib_trace_last_ibl_exit should have an overflow check as well */
}
#endif /* HASHTABLE_STATISTICS */

/* init/update the tls slots storing this table's mask and lookup base
 * N.B.: for thread-shared the caller must call for each thread
 */
/* currently we don't support a mixture */
static inline void
update_lookuptable_tls(dcontext_t *dcontext, ibl_table_t *table)
{
    /* use dcontext->local_state, rather than get_local_state(), to support
     * being called from other threads!
     */
    local_state_extended_t *state = (local_state_extended_t *)dcontext->local_state;

    ASSERT(state != NULL);
    ASSERT(DYNAMO_OPTION(ibl_table_in_tls));
    /* We must hold at least the read lock here, else we could grab
     * an inconsistent mask/lookuptable pair if another thread is in the middle
     * of resizing the table (case 10405).
     */
    ASSERT_TABLE_SYNCHRONIZED(table, READWRITE);
    /* case 10296: for shared tables we must update the table
     * before the mask, as the ibl lookup code accesses the mask first,
     * and old mask + new table is ok since it will de-ref within the
     * new table (we never shrink tables) and be a miss, whereas
     * new mask + old table can de-ref beyond the end of the table,
     * crashing or worse.
     */
    state->table_space.table[table->branch_type].lookuptable = table->table;
    /* Perform a Store-Release, which when combined with a Load-Acquire of the mask
     * in the IBL itself, ensures the prior store to lookuptable is always
     * observed before this store to hash_mask on weakly ordered arches.
     */
    ATOMIC_PTRSZ_ALIGNED_WRITE(&state->table_space.table[table->branch_type].hash_mask,
                               table->hash_mask, false);
}

#ifdef DEBUG
static const char *ibl_bb_table_type_names[IBL_BRANCH_TYPE_END] = { "ret_bb",
                                                                    "indcall_bb",
                                                                    "indjmp_bb" };
static const char *ibl_trace_table_type_names[IBL_BRANCH_TYPE_END] = { "ret_trace",
                                                                       "indcall_trace",
                                                                       "indjmp_trace" };
#endif

#ifdef DEBUG
static inline void
dump_lookuptable_tls(dcontext_t *dcontext)
{
    /* use dcontext->local_state, rather than get_local_state(), to support
     * being called from other threads!
     */
    if (DYNAMO_OPTION(ibl_table_in_tls)) {

        local_state_extended_t *state = (local_state_extended_t *)dcontext->local_state;
        ibl_branch_type_t branch_type;

        ASSERT(state != NULL);
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            LOG(THREAD, LOG_FRAGMENT, 1, "\t Table %s, table " PFX ", mask " PFX "\n",
                !SHARED_BB_ONLY_IB_TARGETS() ? ibl_trace_table_type_names[branch_type]
                                             : ibl_bb_table_type_names[branch_type],
                state->table_space.table[branch_type].lookuptable,
                state->table_space.table[branch_type].hash_mask);
        }
    }
}
#endif

/*******************************************************************************
 * IBL HASHTABLE INSTANTIATION
 */
#define FRAGENTRY_FROM_FRAGMENT(f)                                      \
    {                                                                   \
        (f)->tag, PC_AS_JMP_TGT(FRAG_ISA_MODE(f->flags), (f)->start_pc) \
    }

/* macros w/ name and types are duplicated in fragment.h -- keep in sync */
#define NAME_KEY ibl
#define ENTRY_TYPE fragment_entry_t
/* not defining HASHTABLE_USE_LOOKUPTABLE */
/* compiler won't let me use null_fragment.tag here */
static const fragment_entry_t fe_empty = { NULL_TAG, HASHLOOKUP_NULL_START_PC };
static const fragment_entry_t fe_sentinel = { NULL_TAG, HASHLOOKUP_SENTINEL_START_PC };
#define ENTRY_TAG(fe) ((ptr_uint_t)(fe).tag_fragment)
#define ENTRY_EMPTY (fe_empty)
#define ENTRY_SENTINEL (fe_sentinel)
#define IBL_ENTRY_IS_EMPTY(fe)                     \
    ((fe).tag_fragment == fe_empty.tag_fragment && \
     (fe).start_pc_fragment == fe_empty.start_pc_fragment)
#define IBL_ENTRY_IS_INVALID(fe) ((fe).tag_fragment == FAKE_TAG)
#define IBL_ENTRY_IS_SENTINEL(fe)                     \
    ((fe).tag_fragment == fe_sentinel.tag_fragment && \
     (fe).start_pc_fragment == fe_sentinel.start_pc_fragment)
#define ENTRY_IS_EMPTY(fe) IBL_ENTRY_IS_EMPTY(fe)
#define ENTRY_IS_SENTINEL(fe) IBL_ENTRY_IS_SENTINEL(fe)
#define ENTRY_IS_INVALID(fe) IBL_ENTRY_IS_INVALID(fe)
#define IBL_ENTRIES_ARE_EQUAL(fe1, fe2) ((fe1).tag_fragment == (fe2).tag_fragment)
#define ENTRIES_ARE_EQUAL(table, fe1, fe2) IBL_ENTRIES_ARE_EQUAL(fe1, fe2)
/* We set start_pc_fragment first to avoid races in a shared table where
 * another thread matches the tag but then jumps to a bogus address. */
#define ENTRY_SET_TO_ENTRY(e, f)                                                      \
    (e).start_pc_fragment = (f).start_pc_fragment;                                    \
    /* Ensure the start_pc_fragment store completes before the tag_fragment store: */ \
    MEMORY_STORE_BARRIER();                                                           \
    (e).tag_fragment = (f).tag_fragment
#define HASHTABLE_WHICH_HEAP(flags) FRAGTABLE_WHICH_HEAP(flags)
#define HTLOCK_RANK table_rwlock
#define HASHTABLE_ENTRY_STATS 1

#include "hashtablex.h"
/* all defines are undef-ed at end of hashtablex.h */

/* required routines for hashtable interface that we don't need for this instance */

static void
hashtable_ibl_free_entry(dcontext_t *dcontext, ibl_table_t *table, fragment_entry_t entry)
{
    /* nothing to do, data is inlined */
}

/*******************************************************************************
 * FRAGMENT HASHTABLE INSTANTIATION
 */

/* macros w/ name and types are duplicated in fragment.h -- keep in sync */
#define NAME_KEY fragment
#define ENTRY_TYPE fragment_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */

#define ENTRY_TAG(f) ((ptr_uint_t)(f)->tag)
/* instead of setting to 0, point at null_fragment */
#define ENTRY_EMPTY ((fragment_t *)&null_fragment)
#define ENTRY_SENTINEL ((fragment_t *)&sentinel_fragment)
#define ENTRY_IS_EMPTY(f) ((f) == (fragment_t *)&null_fragment)
#define ENTRY_IS_SENTINEL(f) ((f) == (fragment_t *)&sentinel_fragment)
#define ENTRY_IS_INVALID(f) ((f) == (fragment_t *)&unlinked_fragment)
#define ENTRIES_ARE_EQUAL(t, f, g) ((f) == (g))
#define HASHTABLE_WHICH_HEAP(flags) FRAGTABLE_WHICH_HEAP(flags)
#define HTLOCK_RANK table_rwlock

#include "hashtablex.h"
/* all defines are undef-ed at end of hashtablex.h */

static void
hashtable_fragment_resized_custom(dcontext_t *dcontext, fragment_table_t *table,
                                  uint old_capacity, fragment_t **old_table,
                                  fragment_t **old_table_unaligned, uint old_ref_count,
                                  uint old_table_flags)
{
    /* nothing */
}

static void
hashtable_fragment_init_internal_custom(dcontext_t *dcontext, fragment_table_t *table)
{
    /* nothing */
}

#ifdef DEBUG
static void
hashtable_fragment_study_custom(dcontext_t *dcontext, fragment_table_t *table,
                                uint entries_inc /*amnt table->entries was pre-inced*/)
{
    /* nothing */
}
#endif

/* callers should use either hashtable_ibl_preinit or hashtable_resize instead */
static void
hashtable_ibl_init_internal_custom(dcontext_t *dcontext, ibl_table_t *table)
{
    ASSERT(null_fragment.tag == NULL_TAG);
    ASSERT(null_fragment.start_pc == HASHLOOKUP_NULL_START_PC);
    ASSERT(FAKE_TAG != NULL_TAG);

    ASSERT(sentinel_fragment.tag == NULL_TAG);
    ASSERT(sentinel_fragment.start_pc == HASHLOOKUP_SENTINEL_START_PC);
    ASSERT(HASHLOOKUP_SENTINEL_START_PC != HASHLOOKUP_NULL_START_PC);

    ASSERT(TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags));
    ASSERT(TEST(FRAG_TABLE_INCLUSIVE_HIERARCHY, table->table_flags));

    /* every time we resize a table we reset the flush threshold,
     * since it is cleared in place after one flush
     */
    table->groom_factor_percent = TEST(FRAG_TABLE_TRACE, table->table_flags)
        ? DYNAMO_OPTION(trace_ibt_groom)
        : DYNAMO_OPTION(bb_ibt_groom);
    table->max_capacity_bits = TEST(FRAG_TABLE_TRACE, table->table_flags)
        ? DYNAMO_OPTION(private_trace_ibl_targets_max)
        : DYNAMO_OPTION(private_bb_ibl_targets_max);

#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        if (table->unprot_stats == NULL) {
            /* first time, not a resize */
            ALLOC_UNPROT_STATS(dcontext, table);
        } /* else, keep original */
    }
#endif /* HASHTABLE_STATISTICS */

    if (SHARED_IB_TARGETS() && !TEST(FRAG_TABLE_SHARED, table->table_flags)) {
        /* currently we don't support a mixture */
        ASSERT(TEST(FRAG_TABLE_TARGET_SHARED, table->table_flags));
        ASSERT(TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags));
        ASSERT(table->branch_type != IBL_NONE);
        /* Only data for one set of tables is stored in TLS -- for the trace
         * tables in the default config OR the BB tables in shared BBs
         * only mode.
         */
        if ((TEST(FRAG_TABLE_TRACE, table->table_flags) || SHARED_BB_ONLY_IB_TARGETS()) &&
            DYNAMO_OPTION(ibl_table_in_tls))
            update_lookuptable_tls(dcontext, table);
    }
}

/* We need our own routines to init/free our added fields */
static void
hashtable_ibl_myinit(dcontext_t *dcontext, ibl_table_t *table, uint bits,
                     uint load_factor_percent, hash_function_t func, uint hash_offset,
                     ibl_branch_type_t branch_type, bool use_lookup,
                     uint table_flags _IF_DEBUG(const char *table_name))
{
    uint flags = table_flags;
    ASSERT(dcontext != GLOBAL_DCONTEXT || TEST(FRAG_TABLE_SHARED, flags));
    /* flags shared by all ibl tables */
    flags |= FRAG_TABLE_INCLUSIVE_HIERARCHY;
    flags |= FRAG_TABLE_IBL_TARGETED;
    flags |= HASHTABLE_ALIGN_TABLE;
    /* use entry stats with all our ibl-targeted tables */
    flags |= HASHTABLE_USE_ENTRY_STATS;
#ifdef HASHTABLE_STATISTICS
    /* indicate this is first time, not a resize */
    table->unprot_stats = NULL;
#endif
    table->branch_type = branch_type;
    hashtable_ibl_init(dcontext, table, bits, load_factor_percent, func, hash_offset,
                       flags _IF_DEBUG(table_name));

    /* PR 305731: rather than having a start_pc of 0, which causes an
     * app targeting 0 to crash at 0, we point at a handler that sends
     * the app to an ibl miss via target_delete, which restores
     * registers saved in the found path.
     */
    if (dcontext != GLOBAL_DCONTEXT && hashlookup_null_target == NULL) {
        ASSERT(!dynamo_initialized);
        hashlookup_null_target =
            PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_target_delete_entry_pc(dcontext, table));
#if !defined(X64) && defined(LINUX)
        /* see comments in x86.asm: we patch to avoid text relocations */
        byte *pc = (byte *)hashlookup_null_handler;
        byte *page_start = (byte *)PAGE_START(pc);
        byte *page_end = (byte *)ALIGN_FORWARD(
            pc IF_ARM(+ARM_INSTR_SIZE) + JMP_LONG_LENGTH, PAGE_SIZE);
        make_writable(page_start, page_end - page_start);
#    ifdef X86
        insert_relative_target(pc + 1, hashlookup_null_target, NOT_HOT_PATCHABLE);
#    elif defined(ARM)
        /* We use a pc-rel load w/ the data right after the load */
        *(byte **)(pc + ARM_INSTR_SIZE) = hashlookup_null_target;
#    endif
        make_unwritable(page_start, page_end - page_start);
#endif
    }
}

static void
hashtable_ibl_myfree(dcontext_t *dcontext, ibl_table_t *table)
{
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        ASSERT(TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags));
        DEALLOC_UNPROT_STATS(dcontext, table);
    }
#endif /* HASHTABLE_STATISTICS */
    hashtable_ibl_free(dcontext, table);
}

static void
hashtable_fragment_free_entry(dcontext_t *dcontext, fragment_table_t *table,
                              fragment_t *f)
{
    if (TEST(FRAG_TABLE_INCLUSIVE_HIERARCHY, table->table_flags)) {
        ASSERT_NOT_REACHED(); /* case 7691 */
    } else {
        if (TEST(FRAG_IS_FUTURE, f->flags))
            fragment_free_future(dcontext, (future_fragment_t *)f);
        else
            fragment_free(dcontext, f);
    }
}

static inline bool
fragment_add_to_hashtable(dcontext_t *dcontext, fragment_t *e, fragment_table_t *table)
{
    /* When using shared IBT tables w/trace building and BB2BB IBL, there is a
     * race between adding a BB target to a table and having it marked by
     * another thread as a trace head. The race exists because the two functions
     * do not use a common lock.
     * The race does NOT cause a correctness problem since a) the marking thread
     * removes the trace head from the table and b) any subsequent add attempt
     * is caught in add_ibl_target(). The table lock is used during add and
     * remove operations and FRAG_IS_TRACE_HEAD is checked while holding
     * the lock. So although a trace head may be present in a table temporarily --
     * it's being marked while an add operation that has passed the frag flags
     * check is in progress -- it will be subsequently removed by the marking
     * thread.
     * However, the existence of the race does mean that
     * we cannot ASSERT(!(FRAG_IS_TRACE_HEAD,...)) at arbitrary spots along the
     * add_ibl_target() path since such an assert could fire due to the race.
     * What is likely a safe point to assert is when there is only a single
     * thread in the process.
     */
    DOCHECK(1, {
        if (TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags) &&
            d_r_get_num_threads() == 1)
            ASSERT(!TEST(FRAG_IS_TRACE_HEAD, e->flags));
    });

    return hashtable_fragment_add(dcontext, e, table);
}

/* updates all fragments in a given fragment table which may
 * have IBL routine heads inlined in the indirect exit stubs
 *
 * FIXME: [perf] should add a filter of which branch types need updating if
 * updating all is a noticeable performance hit.
 *
 * FIXME: [perf] Also it maybe better to traverse all fragments in an fcache
 * unit instead of entries in a half-empty hashtable
 */
static void
update_indirect_exit_stubs_from_table(dcontext_t *dcontext, fragment_table_t *ftable)
{
    fragment_t *f;
    linkstub_t *l;
    uint i;

    for (i = 0; i < ftable->capacity; i++) {
        f = ftable->table[i];
        if (!REAL_FRAGMENT(f))
            continue;
        for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
            if (LINKSTUB_INDIRECT(l->flags)) {
                /* FIXME: should add a filter of which branch types need updating */
                update_indirect_exit_stub(dcontext, f, l);
                LOG(THREAD, LOG_FRAGMENT, 5,
                    "\tIBL target table resizing: updating F%d\n", f->id);
                STATS_INC(num_ibl_stub_resize_updates);
            }
        }
    }
}

static void
safely_nullify_tables(dcontext_t *dcontext, ibl_table_t *new_table,
                      fragment_entry_t *table, uint capacity)
{
    uint i;
    cache_pc target_delete =
        PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_target_delete_entry_pc(dcontext, new_table));

    ASSERT(target_delete != NULL);
    ASSERT_TABLE_SYNCHRONIZED(new_table, WRITE);
    for (i = 0; i < capacity; i++) {
        if (IBL_ENTRY_IS_SENTINEL(table[i])) {
            ASSERT(i == capacity - 1);
            continue;
        }
        /* We need these writes to be atomic, so check that they're aligned. */
        ASSERT(ALIGNED(&table[i].tag_fragment, sizeof(table[i].tag_fragment)));
        ASSERT(ALIGNED(&table[i].start_pc_fragment, sizeof(table[i].start_pc_fragment)));
        /* We cannot set the tag to fe_empty.tag_fragment to break the hash chain
         * as the target_delete path relies on acquiring the tag from the table entry,
         * so we leave it alone.
         */
        /* We set the payload to target_delete to induce a cache exit.
         *
         * The target_delete path leads to a loss of information -- we can't
         * tell what the src fragment was (the one that transitioned to the
         * IBL code) and this in principle could weaken our RCT checks (see case
         * 5085). In practical terms, RCT checks are unaffected since they
         * are not employed on in-cache transitions such as an IBL hit.
         * (All transitions to target_delete are a race along the hit path.)
         * If we still want to preserve the src info, we can leave the payload
         * as-is, possibly pointing to a cache address. The effect is that
         * any thread accessing the old table on the IBL hit path will not exit
         * the cache as early. (We should leave the fragment_t* value in the
         * table untouched also so that the fragment_table_t is in a consistent
         * state.)
         */
        /* For weakly ordered arches: we leave this as a weak (atomic-untorn b/c it's
         * aligned) store, which should eventually be seen by the target thread.
         */
        table[i].start_pc_fragment = target_delete;
    }
    STATS_INC(num_shared_ibt_table_flushes);
}

/* Add an item to the dead tables list */
static inline void
add_to_dead_table_list(dcontext_t *alloc_dc, ibl_table_t *ftable, uint old_capacity,
                       fragment_entry_t *old_table_unaligned, uint old_ref_count,
                       uint old_table_flags)
{
    dead_fragment_table_t *item = (dead_fragment_table_t *)heap_alloc(
        GLOBAL_DCONTEXT, sizeof(dead_fragment_table_t) HEAPACCT(ACCT_IBLTABLE));

    LOG(GLOBAL, LOG_FRAGMENT, 2, "add_to_dead_table_list %s " PFX " capacity %d\n",
        ftable->name, old_table_unaligned, old_capacity);
    ASSERT(old_ref_count >= 1); /* someone other the caller must be holding a reference */
    /* write lock must be held so that ref_count is copied accurately */
    ASSERT_TABLE_SYNCHRONIZED(ftable, WRITE);
    item->capacity = old_capacity;
    item->table_unaligned = old_table_unaligned;
    item->table_flags = old_table_flags;
    item->ref_count = old_ref_count;
    item->next = NULL;
    /* Add to the end of list. We use a FIFO because generally we'll be
     * decrementing ref-counts for older tables before we do so for
     * younger tables. A FIFO will yield faster searches than, say, a
     * stack.
     */
    d_r_mutex_lock(&dead_tables_lock);
    if (dead_lists->dead_tables == NULL) {
        ASSERT(dead_lists->dead_tables_tail == NULL);
        dead_lists->dead_tables = item;
    } else {
        ASSERT(dead_lists->dead_tables_tail != NULL);
        ASSERT(dead_lists->dead_tables_tail->next == NULL);
        dead_lists->dead_tables_tail->next = item;
    }
    dead_lists->dead_tables_tail = item;
    d_r_mutex_unlock(&dead_tables_lock);
    STATS_ADD_PEAK(num_dead_shared_ibt_tables, 1);
    STATS_INC(num_total_dead_shared_ibt_tables);
}

/* forward decl */
static inline void
update_private_ptr_to_shared_ibt_table(dcontext_t *dcontext,
                                       ibl_branch_type_t branch_type, bool trace,
                                       bool adjust_old_ref_count, bool lock_table);
static void
hashtable_ibl_resized_custom(dcontext_t *dcontext, ibl_table_t *table, uint old_capacity,
                             fragment_entry_t *old_table,
                             fragment_entry_t *old_table_unaligned, uint old_ref_count,
                             uint old_table_flags)
{
    dcontext_t *alloc_dc = FRAGMENT_TABLE_ALLOC_DC(dcontext, table->table_flags);
    per_thread_t *pt = GET_PT(dcontext);
    bool shared_ibt_table =
        TESTALL(FRAG_TABLE_TARGET_SHARED | FRAG_TABLE_SHARED, table->table_flags);
    ASSERT(TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags));

    /* If we change an ibl-targeted table, must patch up every
     * inlined indirect exit stub that targets it.
     * For our per-type ibl tables however we don't bother updating
     * fragments _targeted_ by the resized table, instead we need to
     * update all fragments that may be a source of an inlined IBL.
     */

    /* private inlined IBL heads targeting this table need to be updated */
    if (DYNAMO_OPTION(inline_trace_ibl) && PRIVATE_TRACES_ENABLED()) {
        /* We'll get here on a trace table resize, while we
         * need to patch only when the trace_ibt tables are resized.
         */
        /* We assume we don't inline IBL lookup targeting tables of basic blocks
         * and so shouldn't need to do this for now. */
        ASSERT(dcontext != GLOBAL_DCONTEXT && pt != NULL); /* private traces */
        if (TESTALL(FRAG_TABLE_INCLUSIVE_HIERARCHY | FRAG_TABLE_TRACE,
                    table->table_flags)) {
            /* need to update all traces that could be targeting the
             * currently resized table */
            LOG(THREAD, LOG_FRAGMENT, 2,
                "\tIBL target table resizing: updating all private trace fragments\n");
            update_indirect_exit_stubs_from_table(dcontext, &pt->trace);
        }
    }

    /* if we change the trace table (or an IBL target trace
     * table), must patch up every inlined indirect exit stub
     * in all bb fragments in case the inlined target is the
     * resized table
     */
    if (DYNAMO_OPTION(inline_bb_ibl)) {
        LOG(THREAD, LOG_FRAGMENT, 3,
            "\tIBL target table resizing: updating bb fragments\n");
        update_indirect_exit_stubs_from_table(dcontext, &pt->bb);
    }

    /* don't need to update any inlined lookups in shared fragments */

    if (shared_ibt_table) {
        if (old_ref_count > 0) {
            /* The old table should be nullified ASAP. Since threads update
             * their table pointers on-demand only when they exit the cache
             * after a failed IBL lookup, they could have IBL targets for
             * stale entries. This would likely occur only when there's an
             * app race but in the future could occur due to cache
             * management.
             */
            safely_nullify_tables(dcontext, table, old_table, old_capacity);
            add_to_dead_table_list(alloc_dc, table, old_capacity, old_table_unaligned,
                                   old_ref_count, table->table_flags);
        }
        /* Update the resizing thread's private ptr. */
        update_private_ptr_to_shared_ibt_table(dcontext, table->branch_type,
                                               TEST(FRAG_TABLE_TRACE, table->table_flags),
                                               false, /* no adjust
                                                       * old ref-count */
                                               false /* already hold lock */);
        ASSERT(table->ref_count == 1);
    }

    /* CHECK: is it safe to update the table without holding the lock? */
    /* Using the table flags to drive the update of generated code may
     * err on the side of caution, but it's the best way to guarantee
     * that all of the necessary code is updated.
     * We may perform extra unnecessary updates when a table that's
     * accessed off of the dcontext/per_thread_t is grown, but that doesn't
     * cause correctness problems and likely doesn't hurt peformance.
     */
    STATS_INC(num_ibt_table_resizes);
    update_generated_hashtable_access(dcontext);
}

#ifdef DEBUG
static void
hashtable_ibl_study_custom(dcontext_t *dcontext, ibl_table_t *table,
                           uint entries_inc /*amnt table->entries was pre-inced*/)
{
#    ifdef HASHTABLE_STATISTICS
    /* For trace table(s) only, use stats from emitted ibl routines */
    if (TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags) &&
        INTERNAL_OPTION(hashtable_ibl_stats)) {
        per_thread_t *pt = GET_PT(dcontext);
        ibl_branch_type_t branch_type;

        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            /* This is convoluted since given a table we have to
             * recover its branch type.
             * FIXME: should simplify these assumptions one day
             */
            /* Current table should be targeted only by one of the IBL routines */
            if (!((!DYNAMO_OPTION(disable_traces) &&
                   table == &pt->trace_ibt[branch_type]) ||
                  (DYNAMO_OPTION(bb_ibl_targets) && table == &pt->bb_ibt[branch_type])))
                continue;
            /* stats for lookup routines from bb's & trace's targeting current table */
            print_hashtable_stats(dcontext, entries_inc == 0 ? "Total" : "Current",
                                  table->name, "trace ibl ",
                                  get_branch_type_name(branch_type),
                                  &table->UNPROT_STAT(trace_ibl_stats[branch_type]));
            print_hashtable_stats(dcontext, entries_inc == 0 ? "Total" : "Current",
                                  table->name, "bb ibl ",
                                  get_branch_type_name(branch_type),
                                  &table->UNPROT_STAT(bb_ibl_stats[branch_type]));
        }
    }
#    endif /* HASHTABLE_STATISTICS */
}
#endif /* DEBUG */

/* filter specifies flags for fragments which are OK to be freed */
/* NOTE - if this routine is ever used for non DEBUG purposes be aware that
 * because of case 7697 we don't unlink when we free the hashtable elements.
 * As such, if we aren't also freeing all fragments that could possibly link
 * to fragments in this table at the same time (synchronously) we'll have
 * problems (for ex. a trace only reset would need to unlink incoming, or
 * allowing private->shared linking would need to ulink outgoing).
 */
static void
hashtable_fragment_reset(dcontext_t *dcontext, fragment_table_t *table)
{
    int i;
    fragment_t *f;

    /* case 7691: we now use separate ibl table types */
    ASSERT(!TEST(FRAG_TABLE_INCLUSIVE_HIERARCHY, table->table_flags));
    LOG(THREAD, LOG_FRAGMENT, 2, "hashtable_fragment_reset\n");
    DOLOG(1, LOG_FRAGMENT | LOG_STATS,
          { hashtable_fragment_load_statistics(dcontext, table); });
    if (TEST(FRAG_TABLE_SHARED, table->table_flags) &&
        TEST(FRAG_TABLE_IBL_TARGETED, table->table_flags)) {
        DOLOG(5, LOG_FRAGMENT, { hashtable_fragment_dump_table(dcontext, table); });
    }
    DODEBUG({
        hashtable_fragment_study(dcontext, table, 0 /*table consistent*/);
        /* ensure write lock is held if the table is shared, unless exiting
         * or resetting (N.B.: if change reset model to not suspend all in-DR
         * threads, will have to change this and handle rank order issues)
         */
        if (!dynamo_exited && !dynamo_resetting)
            ASSERT_TABLE_SYNCHRONIZED(table, WRITE);
    });
#ifndef DEBUG
    /* We need to walk the table if either we need to notify clients, or we
     * need to free stubs that are not in the regular heap or cache units.
     */
    if (!dr_fragment_deleted_hook_exists() && !DYNAMO_OPTION(separate_private_stubs))
        return;
    /* i#4226: Avoid the slow deletion code and just invoke the event. */
    for (i = 0; i < (int)table->capacity; i++) {
        f = table->table[i];
        if (!REAL_FRAGMENT(f))
            continue;
        /* This is a full delete (i.e., it is neither FRAGDEL_NO_HEAP nor
         * FRAGDEL_NO_FCACHE: see the fragment_delete() call in the debug path
         * below) so we call the event for every (real) fragment.
         */
        instrument_fragment_deleted(dcontext, f->tag, f->flags);
    }
    if (!DYNAMO_OPTION(separate_private_stubs))
        return;
#endif
    /* Go in reverse order (for efficiency) since using
     * hashtable_fragment_remove_helper to keep all reachable, which is required
     * for dynamo_resetting where we unlink fragments here and need to be able to
     * perform lookups.
     */
    i = table->capacity - 1 - 1 /* sentinel */;
    while (i >= 0) {
        f = table->table[i];
        if (f == &null_fragment) {
            i--;
        } else { /* i stays put */
            /* The shared BB table is reset at process reset or shutdown, so
             * trace_abort() has already been called by (or for) every thread.
             * If shared traces is true, by this point none of the shared BBs
             * should have FRAG_TRACE_BUILDING set since the flag is cleared
             * by trace_abort(). Of course, the flag shouldn't be present
             * if shared traces is false so we don't need to conditionalize
             * the assert.
             */
            ASSERT(!TEST(FRAG_TRACE_BUILDING, f->flags));
            hashtable_fragment_remove_helper(table, i, &table->table[i]);
            if (!REAL_FRAGMENT(f))
                continue;
            /* make sure no other hashtable has shared fragments in it
             * this routine is called on shared table, but only after dynamo_exited
             * the per-thread IBL tables contain pointers to shared fragments
             * and are OK
             */
            ASSERT(dynamo_exited || !TEST(FRAG_SHARED, f->flags) || dynamo_resetting);

            if (TEST(FRAG_IS_FUTURE, f->flags)) {
                DODEBUG({ ((future_fragment_t *)f)->incoming_stubs = NULL; });
                fragment_free_future(dcontext, (future_fragment_t *)f);
            } else {
                DOSTATS({
                    if (dynamo_resetting)
                        STATS_INC(num_fragments_deleted_reset);
                    else
                        STATS_INC(num_fragments_deleted_exit);
                });
                /* Xref 7697 - unlinking the fragments here can screw up the
                 * future table as we are walking in hash order, so we don't
                 * unlink.  See note at top of routine for issues with not
                 * unlinking here if this code is ever used in non debug
                 * builds. */
                fragment_delete(dcontext, f,
                                FRAGDEL_NO_HTABLE | FRAGDEL_NO_UNLINK |
                                    FRAGDEL_NEED_CHLINK_LOCK |
                                    (dynamo_resetting ? 0 : FRAGDEL_NO_OUTPUT));
            }
        }
    }
    table->entries = 0;
    table->unlinked_entries = 0;
}

/*
 *******************************************************************************/

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/*******************************************************************************
 * APP_PC HASHTABLE INSTANTIATION
 */
/* FIXME: RCT tables no longer use future_fragment_t and can be moved out of
 * fragment.c */

/* The ENTRY_* defines are undef-ed at end of hashtablex.h so we make our own.
 * Would be nice to re-use ENTRY_IS_EMPTY, etc., though w/ multiple htables
 * in same file can't realistically get away w/o custom defines like these:
 */
#    define APP_PC_EMPTY (NULL)
/* assume 1 is always invalid address */
#    define APP_PC_SENTINEL ((app_pc)PTR_UINT_1)
#    define APP_PC_ENTRY_IS_EMPTY(pc) ((pc) == APP_PC_EMPTY)
#    define APP_PC_ENTRY_IS_SENTINEL(pc) ((pc) == APP_PC_SENTINEL)
#    define APP_PC_ENTRY_IS_REAL(pc) \
        (!APP_PC_ENTRY_IS_EMPTY(pc) && !APP_PC_ENTRY_IS_SENTINEL(pc))
/* 2 macros w/ name and types are duplicated in fragment.h -- keep in sync */
#    define NAME_KEY app_pc
#    define ENTRY_TYPE app_pc
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#    define ENTRY_TAG(f) ((ptr_uint_t)(f))
#    define ENTRY_EMPTY APP_PC_EMPTY
#    define ENTRY_SENTINEL APP_PC_SENTINEL
#    define ENTRY_IS_EMPTY(f) APP_PC_ENTRY_IS_EMPTY(f)
#    define ENTRY_IS_SENTINEL(f) APP_PC_ENTRY_IS_SENTINEL(f)
#    define ENTRY_IS_INVALID(f) (false) /* no invalid entries */
#    define ENTRIES_ARE_EQUAL(t, f, g) ((f) == (g))
#    define HASHTABLE_WHICH_HEAP(flags) (ACCT_AFTER_CALL)
#    define HTLOCK_RANK app_pc_table_rwlock
#    define HASHTABLE_SUPPORT_PERSISTENCE 1

#    include "hashtablex.h"
/* all defines are undef-ed at end of hashtablex.h */

/* required routines for hashtable interface that we don't need for this instance */

static void
hashtable_app_pc_init_internal_custom(dcontext_t *dcontext, app_pc_table_t *htable)
{ /* nothing */
}

static void
hashtable_app_pc_resized_custom(dcontext_t *dcontext, app_pc_table_t *htable,
                                uint old_capacity, app_pc *old_table,
                                app_pc *old_table_unaligned, uint old_ref_count,
                                uint old_table_flags)
{ /* nothing */
}

#    ifdef DEBUG
static void
hashtable_app_pc_study_custom(dcontext_t *dcontext, app_pc_table_t *htable,
                              uint entries_inc /*amnt table->entries was pre-inced*/)
{ /* nothing */
}
#    endif

static void
hashtable_app_pc_free_entry(dcontext_t *dcontext, app_pc_table_t *htable, app_pc entry)
{
    /* nothing to do, data is inlined */
}

#endif /* defined(RETURN_AFTER_CALL) || defined (RCT_IND_BRANCH) */
/*******************************************************************************/

bool
fragment_initialized(dcontext_t *dcontext)
{
    return (dcontext != GLOBAL_DCONTEXT && dcontext->fragment_field != NULL);
}

/* thread-shared initialization that should be repeated after a reset */
void
fragment_reset_init(void)
{
    /* case 7966: don't initialize at all for hotp_only & thin_client */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    d_r_mutex_lock(&shared_cache_flush_lock);
    /* ASSUMPTION: a reset frees all deletions that use flushtimes, so we can
     * reset the global flushtime here
     */
    flushtime_global = 0;
    d_r_mutex_unlock(&shared_cache_flush_lock);

    if (SHARED_FRAGMENTS_ENABLED()) {
        if (DYNAMO_OPTION(shared_bbs)) {
            hashtable_fragment_init(
                GLOBAL_DCONTEXT, shared_bb, INIT_HTABLE_SIZE_SHARED_BB,
                INTERNAL_OPTION(shared_bb_load),
                (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
                FRAG_TABLE_SHARED | FRAG_TABLE_TARGET_SHARED _IF_DEBUG("shared_bb"));
        }
        if (DYNAMO_OPTION(shared_traces)) {
            hashtable_fragment_init(
                GLOBAL_DCONTEXT, shared_trace, INIT_HTABLE_SIZE_SHARED_TRACE,
                INTERNAL_OPTION(shared_trace_load),
                (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
                FRAG_TABLE_SHARED | FRAG_TABLE_TARGET_SHARED _IF_DEBUG("shared_trace"));
        }
        /* init routine will work for future_fragment_t* same as for fragment_t* */
        hashtable_fragment_init(
            GLOBAL_DCONTEXT, shared_future, INIT_HTABLE_SIZE_SHARED_FUTURE,
            INTERNAL_OPTION(shared_future_load),
            (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
            FRAG_TABLE_SHARED | FRAG_TABLE_TARGET_SHARED _IF_DEBUG("shared_future"));
    }

    if (SHARED_IBT_TABLES_ENABLED()) {

        ibl_branch_type_t branch_type;

        ASSERT(USE_SHARED_PT());

        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (DYNAMO_OPTION(shared_trace_ibt_tables)) {
                hashtable_ibl_myinit(GLOBAL_DCONTEXT, &shared_pt->trace_ibt[branch_type],
                                     DYNAMO_OPTION(shared_ibt_table_trace_init),
                                     DYNAMO_OPTION(shared_ibt_table_trace_load),
                                     HASH_FUNCTION_NONE,
                                     HASHTABLE_IBL_OFFSET(branch_type), branch_type,
                                     false, /* no lookup table */
                                     FRAG_TABLE_SHARED | FRAG_TABLE_TARGET_SHARED |
                                         FRAG_TABLE_TRACE _IF_DEBUG(
                                             ibl_trace_table_type_names[branch_type]));
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    CHECK_UNPROT_STATS(&shared_pt->trace_ibt[branch_type]);
                    /* for compatibility using an entry in the per-branch type stats */
                    INIT_HASHTABLE_STATS(shared_pt->trace_ibt[branch_type].UNPROT_STAT(
                        trace_ibl_stats[branch_type]));
                } else {
                    shared_pt->trace_ibt[branch_type].unprot_stats = NULL;
                }
#endif /* HASHTABLE_STATISTICS */
            }

            if (DYNAMO_OPTION(shared_bb_ibt_tables)) {
                hashtable_ibl_myinit(GLOBAL_DCONTEXT, &shared_pt->bb_ibt[branch_type],
                                     DYNAMO_OPTION(shared_ibt_table_bb_init),
                                     DYNAMO_OPTION(shared_ibt_table_bb_load),
                                     HASH_FUNCTION_NONE,
                                     HASHTABLE_IBL_OFFSET(branch_type), branch_type,
                                     false, /* no lookup table */
                                     FRAG_TABLE_SHARED |
                                         FRAG_TABLE_TARGET_SHARED _IF_DEBUG(
                                             ibl_bb_table_type_names[branch_type]));
                /* mark as inclusive table for bb's - we in fact currently
                 * keep only frags that are not FRAG_IS_TRACE_HEAD */
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    /* for compatibility using an entry in the per-branch type stats */
                    CHECK_UNPROT_STATS(&shared_pt->bb_ibt[branch_type]);
                    /* FIXME: we don't expect trace_ibl_stats yet */
                    INIT_HASHTABLE_STATS(shared_pt->bb_ibt[branch_type].UNPROT_STAT(
                        bb_ibl_stats[branch_type]));
                } else {
                    shared_pt->bb_ibt[branch_type].unprot_stats = NULL;
                }
#endif /* HASHTABLE_STATISTICS */
            }
        }
    }

#ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study)) {
        uint size = HASHTABLE_SIZE(SHARED_HASH_BITS) * sizeof(shared_entry_t *);
        shared_blocks = (shared_entry_t **)global_heap_alloc(size HEAPACCT(ACCT_OTHER));
        memset(shared_blocks, 0, size);
        shared_traces = (shared_entry_t **)global_heap_alloc(size HEAPACCT(ACCT_OTHER));
        memset(shared_traces, 0, size);
    }
#endif
}

/* thread-shared initialization */
void
fragment_init()
{
    /* case 7966: don't initialize at all for hotp_only & thin_client
     * FIXME: could set initial sizes to 0 for all configurations, instead
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    /* make sure fields are at same place */
    ASSERT(offsetof(fragment_t, flags) == offsetof(future_fragment_t, flags));
    ASSERT(offsetof(fragment_t, tag) == offsetof(future_fragment_t, tag));

    /* ensure we can read this w/o a lock: no cache line crossing, please */
    ASSERT(ALIGNED(&flushtime_global, 4));

    if (SHARED_FRAGMENTS_ENABLED()) {
        /* tables are persistent across resets, only on heap for selfprot (case 7957) */
        if (DYNAMO_OPTION(shared_bbs)) {
            shared_bb = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fragment_table_t,
                                        ACCT_FRAG_TABLE, PROTECTED);
        }
        if (DYNAMO_OPTION(shared_traces)) {
            shared_trace = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fragment_table_t,
                                           ACCT_FRAG_TABLE, PROTECTED);
        }
        shared_future = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fragment_table_t,
                                        ACCT_FRAG_TABLE, PROTECTED);
    }

    if (USE_SHARED_PT())
        shared_pt = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, per_thread_t, ACCT_OTHER, PROTECTED);

    if (SHARED_IBT_TABLES_ENABLED()) {
        dead_lists =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dead_table_lists_t, ACCT_OTHER, PROTECTED);
        memset(dead_lists, 0, sizeof(*dead_lists));
    }

    fragment_reset_init();

    if (TRACEDUMP_ENABLED() && DYNAMO_OPTION(shared_traces)) {
        ASSERT(USE_SHARED_PT());
        shared_pt->tracefile = open_log_file("traces-shared", NULL, 0);
        ASSERT(shared_pt->tracefile != INVALID_FILE);
        init_trace_file(shared_pt);
    }
}

/* Free all thread-shared state not critical to forward progress;
 * fragment_reset_init() will be called before continuing.
 */
void
fragment_reset_free(void)
{
    /* case 7966: don't initialize at all for hotp_only & thin_client */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    /* We must study the ibl tables before the trace/bb tables so that we're
     * not looking at freed entries
     */
    if (SHARED_IBT_TABLES_ENABLED()) {

        ibl_branch_type_t branch_type;
        dead_fragment_table_t *current, *next;
        DEBUG_DECLARE(int table_count = 0;)
        DEBUG_DECLARE(stats_int_t dead_tables = GLOBAL_STAT(num_dead_shared_ibt_tables);)

        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (DYNAMO_OPTION(shared_trace_ibt_tables)) {
                DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                    hashtable_ibl_load_statistics(GLOBAL_DCONTEXT,
                                                  &shared_pt->trace_ibt[branch_type]);
                });
                hashtable_ibl_myfree(GLOBAL_DCONTEXT, &shared_pt->trace_ibt[branch_type]);
            }
            if (DYNAMO_OPTION(shared_bb_ibt_tables)) {
                DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                    hashtable_ibl_load_statistics(GLOBAL_DCONTEXT,
                                                  &shared_pt->bb_ibt[branch_type]);
                });
                hashtable_ibl_myfree(GLOBAL_DCONTEXT, &shared_pt->bb_ibt[branch_type]);
            }
        }

        /* Delete dead tables. */
        /* grab lock for consistency, although we expect a single thread */
        d_r_mutex_lock(&dead_tables_lock);
        current = dead_lists->dead_tables;
        while (current != NULL) {
            DODEBUG({ table_count++; });
            next = current->next;
            LOG(GLOBAL, LOG_FRAGMENT, 2,
                "fragment_reset_free: dead table " PFX " cap %d, freeing\n",
                current->table_unaligned, current->capacity);
            hashtable_ibl_free_table(GLOBAL_DCONTEXT, current->table_unaligned,
                                     current->table_flags, current->capacity);
            heap_free(GLOBAL_DCONTEXT, current,
                      sizeof(dead_fragment_table_t) HEAPACCT(ACCT_IBLTABLE));
            STATS_DEC(num_dead_shared_ibt_tables);
            STATS_INC(num_dead_shared_ibt_tables_freed);
            current = next;
            DODEBUG({
                if (dynamo_exited)
                    STATS_INC(num_dead_shared_ibt_tables_freed_at_exit);
            });
        }
        dead_lists->dead_tables = dead_lists->dead_tables_tail = NULL;
        ASSERT(table_count == dead_tables);
        d_r_mutex_unlock(&dead_tables_lock);
    }

    /* FIXME: Take in a flag "permanent" that controls whether exiting or
     * resetting.  If resetting only, do not free unprot stats and entry stats
     * (they're already in persistent heap, but we explicitly free them).
     * This will be easy w/ unprot but will take work for entry stats
     * since they resize themselves.
     * Or, move them both to a new unprot and nonpersistent heap so we can
     * actually free the memory back to the os, if we don't care to keep
     * the stats across the reset.
     */
    /* N.B.: to avoid rank order issues w/ shared_vm_areas lock being acquired
     * after table_rwlock we do NOT grab the write lock before calling
     * reset on the shared tables!  We assume that reset involves suspending
     * all other threads in DR and there will be no races.  If the reset model
     * changes, the lock order will have to be addressed.
     */
    if (SHARED_FRAGMENTS_ENABLED()) {
        /* clean up pending delayed deletion, if any */
        vm_area_check_shared_pending(GLOBAL_DCONTEXT /*== safe to free all*/, NULL);

        if (DYNAMO_OPTION(coarse_units)) {
            /* We need to free coarse units earlier than vm_areas_exit() so we
             * call it here.  Must call before we free fine fragments so coarse
             * can clean up incoming pointers.
             */
            vm_area_coarse_units_reset_free();
        }

#ifndef DEBUG
        if (dr_fragment_deleted_hook_exists()) {
#endif
            if (DYNAMO_OPTION(shared_bbs))
                hashtable_fragment_reset(GLOBAL_DCONTEXT, shared_bb);
            if (DYNAMO_OPTION(shared_traces))
                hashtable_fragment_reset(GLOBAL_DCONTEXT, shared_trace);
            DODEBUG({ hashtable_fragment_reset(GLOBAL_DCONTEXT, shared_future); });
#ifndef DEBUG
        }
#endif

        if (DYNAMO_OPTION(shared_bbs))
            hashtable_fragment_free(GLOBAL_DCONTEXT, shared_bb);
        if (DYNAMO_OPTION(shared_traces))
            hashtable_fragment_free(GLOBAL_DCONTEXT, shared_trace);
        hashtable_fragment_free(GLOBAL_DCONTEXT, shared_future);
        /* Do NOT free RAC table as its state cannot be rebuilt.
         * We also do not free other RCT tables to avoid the time to rebuild them.
         */
    }

#ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study)) {
        print_shared_stats();
        reset_shared_block_table(shared_blocks, &shared_blocks_lock);
        reset_shared_block_table(shared_traces, &shared_traces_lock);
    }
#endif
}

/* free all state */
void
fragment_exit()
{
    /* case 7966: don't initialize at all for hotp_only & thin_client
     * FIXME: could set initial sizes to 0 for all configurations, instead
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        goto cleanup;

    if (TRACEDUMP_ENABLED() && DYNAMO_OPTION(shared_traces)) {
        /* write out all traces prior to deleting any, so links print nicely */
        uint i;
        fragment_t *f;
        /* change_linking_lock is required for output_trace(), though there
         * won't be any races at this point of exiting.
         */
        acquire_recursive_lock(&change_linking_lock);
        TABLE_RWLOCK(shared_trace, read, lock);
        for (i = 0; i < shared_trace->capacity; i++) {
            f = shared_trace->table[i];
            if (!REAL_FRAGMENT(f))
                continue;
            if (SHOULD_OUTPUT_FRAGMENT(f->flags))
                output_trace(GLOBAL_DCONTEXT, shared_pt, f, -1);
        }
        TABLE_RWLOCK(shared_trace, read, unlock);
        release_recursive_lock(&change_linking_lock);
        exit_trace_file(shared_pt);
    }

#ifdef FRAGMENT_SIZES_STUDY
    DOLOG(1, (LOG_FRAGMENT | LOG_STATS), { print_size_results(); });
#endif

    fragment_reset_free();

#ifdef RETURN_AFTER_CALL
    if (dynamo_options.ret_after_call && rac_non_module_table.live_table != NULL) {
        DODEBUG({
            DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                hashtable_app_pc_load_statistics(GLOBAL_DCONTEXT,
                                                 rac_non_module_table.live_table);
            });
            hashtable_app_pc_study(GLOBAL_DCONTEXT, rac_non_module_table.live_table,
                                   0 /*table consistent*/);
        });
        hashtable_app_pc_free(GLOBAL_DCONTEXT, rac_non_module_table.live_table);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, rac_non_module_table.live_table, app_pc_table_t,
                       ACCT_AFTER_CALL, PROTECTED);
        rac_non_module_table.live_table = NULL;
    }
    ASSERT(rac_non_module_table.persisted_table == NULL);
    DELETE_LOCK(after_call_lock);
#endif

#if defined(RCT_IND_BRANCH) && defined(UNIX)
    /* we do not free these tables in fragment_reset_free() b/c we
     * would just have to build them all back up again in order to
     * continue execution
     */
    if ((TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
         TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) &&
        rct_global_table.live_table != NULL) {
        DODEBUG({
            DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                hashtable_app_pc_load_statistics(GLOBAL_DCONTEXT,
                                                 rct_global_table.live_table);
            });
            hashtable_app_pc_study(GLOBAL_DCONTEXT, rct_global_table.live_table,
                                   0 /*table consistent*/);
        });
        hashtable_app_pc_free(GLOBAL_DCONTEXT, rct_global_table.live_table);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, rct_global_table.live_table, app_pc_table_t,
                       ACCT_AFTER_CALL, PROTECTED);
        rct_global_table.live_table = NULL;
    } else
        ASSERT(rct_global_table.live_table == NULL);
    ASSERT(rct_global_table.persisted_table == NULL);
#endif /* RCT_IND_BRANCH */

    if (SHARED_FRAGMENTS_ENABLED()) {
        /* tables are persistent across resets, only on heap for selfprot (case 7957) */
        if (DYNAMO_OPTION(shared_bbs)) {
            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_bb, fragment_table_t, ACCT_FRAG_TABLE,
                           PROTECTED);
            shared_bb = NULL;
        } else
            ASSERT(shared_bb == NULL);
        if (DYNAMO_OPTION(shared_traces)) {
            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_trace, fragment_table_t,
                           ACCT_FRAG_TABLE, PROTECTED);
            shared_trace = NULL;
        } else
            ASSERT(shared_trace == NULL);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_future, fragment_table_t, ACCT_FRAG_TABLE,
                       PROTECTED);
        shared_future = NULL;
    }

    if (SHARED_IBT_TABLES_ENABLED()) {
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dead_lists, dead_table_lists_t, ACCT_OTHER,
                       PROTECTED);
        dead_lists = NULL;
    } else
        ASSERT(dead_lists == NULL);

    if (USE_SHARED_PT()) {
        ASSERT(shared_pt != NULL);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_pt, per_thread_t, ACCT_OTHER, PROTECTED);
        shared_pt = NULL;
    } else
        ASSERT(shared_pt == NULL);

    if (SHARED_IBT_TABLES_ENABLED())
        DELETE_LOCK(dead_tables_lock);
#ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study)) {
        DELETE_LOCK(shared_blocks_lock);
        DELETE_LOCK(shared_traces_lock);
    }
#endif
cleanup:
    /* FIXME: we shouldn't need these locks anyway for hotp_only & thin_client */
    DELETE_LOCK(tracedump_mutex);
    process_client_flush_requests(NULL, GLOBAL_DCONTEXT, client_flush_requests,
                                  false /* no flush */);
    DELETE_LOCK(client_flush_request_lock);
}

void
fragment_exit_post_sideline(void)
{
    DELETE_LOCK(shared_cache_flush_lock);
}

/* Decrement the ref-count for any reference to table that the
 * per_thread_t contains.  If could_be_live is true, will acquire write
 * locks for the currently live tables. */
/* NOTE: Can't inline in release build -- too many call sites? */
static /* inline */ void
dec_table_ref_count(dcontext_t *dcontext, ibl_table_t *table, bool could_be_live)
{
    ibl_table_t *live_table = NULL;
    ibl_branch_type_t branch_type;

    /* Search live tables. A live table's ref-count is decremented
     * during a thread exit. */
    /* FIXME If the table is more likely to be dead, we can reverse the order
     * and search dead tables first. */
    if (!DYNAMO_OPTION(ref_count_shared_ibt_tables))
        return;
    ASSERT(TESTALL(FRAG_TABLE_SHARED | FRAG_TABLE_IBL_TARGETED, table->table_flags));
    if (could_be_live) {
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            /* We match based on lookup table addresses. We need to lock the table
             * during the compare and hold the lock during the ref-count dec to
             * prevent a race with it being moved to the dead list.
             */
            ibl_table_t *sh_table_ptr = TEST(FRAG_TABLE_TRACE, table->table_flags)
                ? &shared_pt->trace_ibt[branch_type]
                : &shared_pt->bb_ibt[branch_type];
            TABLE_RWLOCK(sh_table_ptr, write, lock);
            if (table->table == sh_table_ptr->table) {
                live_table = sh_table_ptr;
                break;
            }
            TABLE_RWLOCK(sh_table_ptr, write, unlock);
        }
    }
    if (live_table != NULL) {
        /* During shutdown, the ref-count can reach 0. The table is freed
         * in the fragment_exit() path. */
        ASSERT(live_table->ref_count >= 1);
        live_table->ref_count--;
        TABLE_RWLOCK(live_table, write, unlock);
    } else { /* Search the dead tables list. */
        dead_fragment_table_t *current = dead_lists->dead_tables;
        dead_fragment_table_t *prev = NULL;

        ASSERT(dead_lists->dead_tables != NULL);
        ASSERT(dead_lists->dead_tables_tail != NULL);
        /* We expect to be removing from the head of the list but due to
         * races could be removing from the middle, i.e., if a preceding
         * entry is about to be removed by another thread but the
         * dead_tables_lock hasn't been acquired yet by that thread.
         */
        d_r_mutex_lock(&dead_tables_lock);
        for (current = dead_lists->dead_tables; current != NULL;
             prev = current, current = current->next) {
            if (current->table_unaligned == table->table_unaligned) {
                ASSERT_CURIOSITY(current->ref_count >= 1);
                current->ref_count--;
                if (current->ref_count == 0) {
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "dec_table_ref_count: table " PFX " cap %d at ref 0, freeing\n",
                        current->table_unaligned, current->capacity);
                    /* Unlink this table from the list. */
                    if (prev != NULL)
                        prev->next = current->next;
                    if (current == dead_lists->dead_tables) {
                        /* remove from the front */
                        ASSERT(prev == NULL);
                        dead_lists->dead_tables = current->next;
                    }
                    if (current == dead_lists->dead_tables_tail)
                        dead_lists->dead_tables_tail = prev;
                    hashtable_ibl_free_table(GLOBAL_DCONTEXT, current->table_unaligned,
                                             current->table_flags, current->capacity);
                    heap_free(GLOBAL_DCONTEXT, current,
                              sizeof(dead_fragment_table_t) HEAPACCT(ACCT_IBLTABLE));
                    STATS_DEC(num_dead_shared_ibt_tables);
                    STATS_INC(num_dead_shared_ibt_tables_freed);
                }
                break;
            }
        }
        d_r_mutex_unlock(&dead_tables_lock);
        ASSERT(current != NULL);
    }
}

/* Decrement the ref-count for every shared IBT table that the
 * per_thread_t has a reference to. */
static void
dec_all_table_ref_counts(dcontext_t *dcontext, per_thread_t *pt)
{
    /* We can also decrement ref-count for dead shared tables here. */
    if (SHARED_IBT_TABLES_ENABLED()) {
        ibl_branch_type_t branch_type;
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (DYNAMO_OPTION(shared_trace_ibt_tables)) {
                ASSERT(pt->trace_ibt[branch_type].table != NULL);
                dec_table_ref_count(dcontext, &pt->trace_ibt[branch_type],
                                    true /*check live*/);
            }
            if (DYNAMO_OPTION(shared_bb_ibt_tables)) {
                ASSERT(pt->bb_ibt[branch_type].table != NULL);
                dec_table_ref_count(dcontext, &pt->bb_ibt[branch_type],
                                    true /*check live*/);
            }
        }
    }
}

/* re-initializes non-persistent memory */
void
fragment_thread_reset_init(dcontext_t *dcontext)
{
    per_thread_t *pt;
    ibl_branch_type_t branch_type;

    /* case 7966: don't initialize at all for hotp_only & thin_client */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    pt = (per_thread_t *)dcontext->fragment_field;

    /* important to init w/ cur timestamp to avoid this thread dec-ing ref
     * count when it wasn't included in ref count init value!
     * assumption: don't need lock to read flushtime_global atomically.
     * when resetting, though, thread free & re-init is done before global free,
     * so we have to explicitly set to 0 for that case.
     */
    if (dynamo_resetting)
        pt->flushtime_last_update = 0;
    else
        ATOMIC_4BYTE_ALIGNED_READ(&flushtime_global, &pt->flushtime_last_update);

    /* set initial hashtable sizes */
    hashtable_fragment_init(
        dcontext, &pt->bb, INIT_HTABLE_SIZE_BB, INTERNAL_OPTION(private_bb_load),
        (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0, 0 _IF_DEBUG("bblock"));

    /* init routine will work for future_fragment_t* same as for fragment_t* */
    hashtable_fragment_init(dcontext, &pt->future, INIT_HTABLE_SIZE_FUTURE,
                            INTERNAL_OPTION(private_future_load),
                            (hash_function_t)INTERNAL_OPTION(alt_hash_func),
                            0 /* hash_mask_offset */, 0 _IF_DEBUG("future"));

    /* The trace table now is not used by IBL routines, and
     * therefore doesn't need a lookup table, we can also use the
     * alternative hash functions and use a higher load.
     */
    if (PRIVATE_TRACES_ENABLED()) {
        hashtable_fragment_init(dcontext, &pt->trace, INIT_HTABLE_SIZE_TRACE,
                                INTERNAL_OPTION(private_trace_load),
                                (hash_function_t)INTERNAL_OPTION(alt_hash_func),
                                0 /* hash_mask_offset */,
                                FRAG_TABLE_TRACE _IF_DEBUG("trace"));
    }

    /* We'll now have more control over hashtables based on branch
     * type.  The most important of all is of course the return
     * target table.  These tables should be populated only when
     * we know that the entry is a valid target, a trace is
     * created, and it is indeed targeted by an IBL.
     */
    /* These tables are targeted by both bb and trace routines */
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        if (!DYNAMO_OPTION(disable_traces)
            /* If no traces and no bb ibl targets we point ibl at
             * an empty trace table */
            || !DYNAMO_OPTION(bb_ibl_targets)) {
            if (!DYNAMO_OPTION(shared_trace_ibt_tables)) {
                hashtable_ibl_myinit(
                    dcontext, &pt->trace_ibt[branch_type],
                    DYNAMO_OPTION(private_trace_ibl_targets_init),
                    DYNAMO_OPTION(private_ibl_targets_load), HASH_FUNCTION_NONE,
                    HASHTABLE_IBL_OFFSET(branch_type), branch_type,
                    false, /* no lookup table */
                    (DYNAMO_OPTION(shared_traces) ? FRAG_TABLE_TARGET_SHARED : 0) |
                        FRAG_TABLE_TRACE _IF_DEBUG(
                            ibl_trace_table_type_names[branch_type]));
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    CHECK_UNPROT_STATS(pt->trace_ibt[branch_type]);
                    /* for compatibility using an entry in the per-branch type stats */
                    INIT_HASHTABLE_STATS(pt->trace_ibt[branch_type].UNPROT_STAT(
                        trace_ibl_stats[branch_type]));
                } else {
                    pt->trace_ibt[branch_type].unprot_stats = NULL;
                }
#endif /* HASHTABLE_STATISTICS */
            } else {
                /* ensure table from last time (if we had a reset) not still there */
                memset(&pt->trace_ibt[branch_type], 0,
                       sizeof(pt->trace_ibt[branch_type]));
                update_private_ptr_to_shared_ibt_table(dcontext, branch_type,
                                                       true,  /* trace = yes */
                                                       false, /* no adjust old
                                                               * ref-count */
                                                       true /* lock */);
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    ALLOC_UNPROT_STATS(dcontext, &pt->trace_ibt[branch_type]);
                    CHECK_UNPROT_STATS(pt->trace_ibt[branch_type]);
                    INIT_HASHTABLE_STATS(pt->trace_ibt[branch_type].UNPROT_STAT(
                        trace_ibl_stats[branch_type]));
                } else {
                    pt->trace_ibt[branch_type].unprot_stats = NULL;
                }
#endif
            }
        }

        /* When targetting BBs, currently the source is assumed to be only a
         * bb since traces going to a bb for the first time should mark it
         * as a trace head.  Therefore the tables are currently only
         * targeted by bb IBL routines.  It will be possible to later
         * deal with trace heads and allow a trace to target a BB with
         * the intent of modifying its THCI.
         *
         * (FIXME: having another table for THCI IBLs seems better than
         * adding a counter (starting at -1) to all blocks and
         * trapping when 0 for marking a trace head and again at 50
         * for creating a trace.  And that is all of course after proving
         * that doing it in DR has significant impact.)
         *
         * Note that private bb2bb transitions are not captured when
         * we run with -shared_bbs.
         */

        /* These tables should be populated only when we know that the
         * entry is a valid target, and it is indeed targeted by an
         * IBL.  They have to be per-type so that our security
         * policies are properly checked.
         */
        if (DYNAMO_OPTION(bb_ibl_targets)) {
            if (!DYNAMO_OPTION(shared_bb_ibt_tables)) {
                hashtable_ibl_myinit(
                    dcontext, &pt->bb_ibt[branch_type],
                    DYNAMO_OPTION(private_bb_ibl_targets_init),
                    DYNAMO_OPTION(private_bb_ibl_targets_load), HASH_FUNCTION_NONE,
                    HASHTABLE_IBL_OFFSET(branch_type), branch_type,
                    false, /* no lookup table */
                    (DYNAMO_OPTION(shared_bbs) ? FRAG_TABLE_TARGET_SHARED : 0)_IF_DEBUG(
                        ibl_bb_table_type_names[branch_type]));
                /* mark as inclusive table for bb's - we in fact currently
                 * keep only frags that are not FRAG_IS_TRACE_HEAD */
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    /* for compatibility using an entry in the per-branch type stats */
                    CHECK_UNPROT_STATS(pt->bb_ibt[branch_type]);
                    /* FIXME: we don't expect trace_ibl_stats yet */
                    INIT_HASHTABLE_STATS(
                        pt->bb_ibt[branch_type].UNPROT_STAT(bb_ibl_stats[branch_type]));
                } else {
                    pt->bb_ibt[branch_type].unprot_stats = NULL;
                }
#endif /* HASHTABLE_STATISTICS */
            } else {
                /* ensure table from last time (if we had a reset) not still there */
                memset(&pt->bb_ibt[branch_type], 0, sizeof(pt->bb_ibt[branch_type]));
                update_private_ptr_to_shared_ibt_table(dcontext, branch_type,
                                                       false, /* trace = no */
                                                       false, /* no adjust old
                                                               * ref-count */
                                                       true /* lock */);
#ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    ALLOC_UNPROT_STATS(dcontext, &pt->bb_ibt[branch_type]);
                    CHECK_UNPROT_STATS(pt->bb_ibt[branch_type]);
                    INIT_HASHTABLE_STATS(pt->bb_ibt[branch_type].UNPROT_STAT(
                        trace_ibl_stats[branch_type]));
                } else {
                    pt->bb_ibt[branch_type].unprot_stats = NULL;
                }
#endif
            }
        }
    }
    ASSERT(IBL_BRANCH_TYPE_END == 3);

    update_generated_hashtable_access(dcontext);
}

void
fragment_thread_init(dcontext_t *dcontext)
{
    /* we allocate per_thread_t in the global heap solely for self-protection,
     * even when turned off, since even with a lot of threads this isn't a lot of
     * pressure on the global heap
     */
    per_thread_t *pt;

    /* case 7966: don't initialize un-needed data for hotp_only & thin_client.
     * FIXME: could set htable initial sizes to 0 for all configurations, instead.
     * per_thread_t is pretty big, so we avoid it, though it costs us checks for
     * hotp_only in the islinking-related routines.
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    pt = (per_thread_t *)global_heap_alloc(sizeof(per_thread_t) HEAPACCT(ACCT_OTHER));
    dcontext->fragment_field = (void *)pt;

    fragment_thread_reset_init(dcontext);

    if (TRACEDUMP_ENABLED() && PRIVATE_TRACES_ENABLED()) {
        pt->tracefile = open_log_file("traces", NULL, 0);
        ASSERT(pt->tracefile != INVALID_FILE);
        init_trace_file(pt);
    }
    ASSIGN_INIT_LOCK_FREE(pt->fragment_delete_mutex, fragment_delete_mutex);

    pt->could_be_linking = false;
    pt->wait_for_unlink = false;
    pt->about_to_exit = false;
    pt->flush_queue_nonempty = false;
    pt->waiting_for_unlink = create_event();
    pt->finished_with_unlink = create_event();
    ASSIGN_INIT_LOCK_FREE(pt->linking_lock, linking_lock);
    pt->finished_all_unlink = create_event();
    pt->soon_to_be_linking = false;
    pt->at_syscall_at_flush = false;
}

static bool
check_flush_queue(dcontext_t *dcontext, fragment_t *was_I_flushed);

/* frees all non-persistent memory */
void
fragment_thread_reset_free(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    DEBUG_DECLARE(ibl_branch_type_t branch_type;)

    /* case 7966: don't initialize at all for hotp_only & thin_client */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    /* Dec ref count on any shared tables that are pointed to. */
    dec_all_table_ref_counts(dcontext, pt);

#ifdef DEBUG
    /* for non-debug we do fast exit path and don't free local heap */
    SELF_PROTECT_CACHE(dcontext, NULL, WRITABLE);

    /* we remove flushed fragments from the htable, and they can be
     * flushed after enter_threadexit() due to os_thread_stack_exit(),
     * so we need to check the flush queue here
     */
    d_r_mutex_lock(&pt->linking_lock);
    check_flush_queue(dcontext, NULL);
    d_r_mutex_unlock(&pt->linking_lock);

    /* For consistency we remove entries from the IBL targets
     * tables before we remove them from the trace table.  However,
     * we cannot free any fragments because for sure all of them will
     * be present in the trace table.
     */
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        if (!DYNAMO_OPTION(disable_traces)
            /* If no traces and no bb ibl targets we point ibl at
             * an empty trace table */
            || !DYNAMO_OPTION(bb_ibl_targets)) {
            if (!DYNAMO_OPTION(shared_trace_ibt_tables)) {
                DOLOG(4, LOG_FRAGMENT, {
                    hashtable_ibl_dump_table(dcontext, &pt->trace_ibt[branch_type]);
                });
                DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                    hashtable_ibl_load_statistics(dcontext, &pt->trace_ibt[branch_type]);
                });
                hashtable_ibl_myfree(dcontext, &pt->trace_ibt[branch_type]);
            } else {
#    ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    print_hashtable_stats(dcontext, "Total",
                                          shared_pt->trace_ibt[branch_type].name,
                                          "trace ibl ", get_branch_type_name(branch_type),
                                          &pt->trace_ibt[branch_type].UNPROT_STAT(
                                              trace_ibl_stats[branch_type]));
                    DEALLOC_UNPROT_STATS(dcontext, &pt->trace_ibt[branch_type]);
                }
#    endif
                memset(&pt->trace_ibt[branch_type], 0,
                       sizeof(pt->trace_ibt[branch_type]));
            }
        }
        if (DYNAMO_OPTION(bb_ibl_targets)) {
            if (!DYNAMO_OPTION(shared_bb_ibt_tables)) {
                DOLOG(4, LOG_FRAGMENT,
                      { hashtable_ibl_dump_table(dcontext, &pt->bb_ibt[branch_type]); });
                DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
                    hashtable_ibl_load_statistics(dcontext, &pt->bb_ibt[branch_type]);
                });
                hashtable_ibl_myfree(dcontext, &pt->bb_ibt[branch_type]);
            } else {
#    ifdef HASHTABLE_STATISTICS
                if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                    print_hashtable_stats(
                        dcontext, "Total", shared_pt->bb_ibt[branch_type].name, "bb ibl ",
                        get_branch_type_name(branch_type),
                        &pt->bb_ibt[branch_type].UNPROT_STAT(bb_ibl_stats[branch_type]));
                    DEALLOC_UNPROT_STATS(dcontext, &pt->bb_ibt[branch_type]);
                }
#    endif
                memset(&pt->bb_ibt[branch_type], 0, sizeof(pt->bb_ibt[branch_type]));
            }
        }
    }

    /* case 7653: we can't free the main tables prior to freeing the contents
     * of all of them, as link freeing involves looking up in the other tables.
     */
    if (PRIVATE_TRACES_ENABLED()) {
        DOLOG(1, LOG_FRAGMENT | LOG_STATS,
              { hashtable_fragment_load_statistics(dcontext, &pt->trace); });
        hashtable_fragment_reset(dcontext, &pt->trace);
    }
    DOLOG(1, LOG_FRAGMENT | LOG_STATS,
          { hashtable_fragment_load_statistics(dcontext, &pt->bb); });
    hashtable_fragment_reset(dcontext, &pt->bb);
    DOLOG(1, LOG_FRAGMENT | LOG_STATS,
          { hashtable_fragment_load_statistics(dcontext, &pt->future); });
    hashtable_fragment_reset(dcontext, &pt->future);

    if (PRIVATE_TRACES_ENABLED())
        hashtable_fragment_free(dcontext, &pt->trace);
    hashtable_fragment_free(dcontext, &pt->bb);
    hashtable_fragment_free(dcontext, &pt->future);

    SELF_PROTECT_CACHE(dcontext, NULL, READONLY);

#else
    /* Case 10807: Clients need to be informed of fragment deletions
     * so we'll reset the relevant hash tables for CI release builds.
     */
    if (PRIVATE_TRACES_ENABLED())
        hashtable_fragment_reset(dcontext, &pt->trace);
    hashtable_fragment_reset(dcontext, &pt->bb);

#endif /* !DEBUG */
}

/* atexit cleanup */
void
fragment_thread_exit(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;

    /* case 7966: don't initialize at all for hotp_only & thin_client */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    if (TRACEDUMP_ENABLED() && PRIVATE_TRACES_ENABLED()) {
        /* write out all traces prior to deleting any, so links print nicely */
        uint i;
        fragment_t *f;
        for (i = 0; i < pt->trace.capacity; i++) {
            f = pt->trace.table[i];
            if (!REAL_FRAGMENT(f))
                continue;
            if (SHOULD_OUTPUT_FRAGMENT(f->flags))
                output_trace(dcontext, pt, f, -1);
        }
        exit_trace_file(pt);
    }

    fragment_thread_reset_free(dcontext);

    /* events are global */
    destroy_event(pt->waiting_for_unlink);
    destroy_event(pt->finished_with_unlink);
    destroy_event(pt->finished_all_unlink);
    DELETE_LOCK(pt->linking_lock);

    DELETE_LOCK(pt->fragment_delete_mutex);

    global_heap_free(pt, sizeof(per_thread_t) HEAPACCT(ACCT_OTHER));
    dcontext->fragment_field = NULL;
}

bool
fragment_thread_exited(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    return (pt == NULL || pt->about_to_exit);
}

#ifdef UNIX
void
fragment_fork_init(dcontext_t *dcontext)
{
    /* FIXME: what about global file? */
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    if (TRACEDUMP_ENABLED() && PRIVATE_TRACES_ENABLED()) {
        /* new log dir has already been created, so just open a new log file */
        pt->tracefile = open_log_file("traces", NULL, 0);
        ASSERT(pt->tracefile != INVALID_FILE);
        init_trace_file(pt);
    }
}
#endif

/* fragment_t heap layout looks like this:
 *
 *   fragment_t/trace_t
 *   translation_info_t*, if necessary
 *   array composed of different sizes of linkstub_t subclasses:
 *     direct_linkstub_t
 *     cbr_fallthrough_linkstub_t
 *     indirect_linkstub_t
 *   post_linkstub_t, if necessary
 */
static uint
fragment_heap_size(uint flags, int direct_exits, int indirect_exits)
{
    uint total_sz;
    ASSERT((direct_exits + indirect_exits > 0) || TEST(FRAG_COARSE_GRAIN, flags));
    total_sz = FRAGMENT_STRUCT_SIZE(flags) +
        linkstubs_heap_size(flags, direct_exits, indirect_exits);
    /* we rely on a small heap size for our ushort offset at the end */
    ASSERT(total_sz <= USHRT_MAX);
    return total_sz;
}

/* Allocates memory for a fragment_t and linkstubs and initializes them, but
 * does not do any fcache-related initialization.
 */
static fragment_t *
fragment_create_heap(dcontext_t *dcontext, int direct_exits, int indirect_exits,
                     uint flags)
{
    dcontext_t *alloc_dc = FRAGMENT_ALLOC_DC(dcontext, flags);
    uint heapsz = fragment_heap_size(flags, direct_exits, indirect_exits);
    /* linkstubs are in an array immediately after the fragment_t/trace_t struct */
    fragment_t *f = (fragment_t *)nonpersistent_heap_alloc(
        alloc_dc,
        heapsz HEAPACCT(TEST(FRAG_IS_TRACE, flags) ? ACCT_TRACE : ACCT_FRAGMENT));
    LOG(THREAD, LOG_FRAGMENT, 5,
        "fragment heap size for flags 0x%08x, exits %d %d, is %d => " PFX "\n", flags,
        direct_exits, indirect_exits, heapsz, f);

    return f;
}

static void
fragment_init_heap(fragment_t *f, app_pc tag, int direct_exits, int indirect_exits,
                   uint flags)
{
    ASSERT(f != NULL);
    f->flags = flags; /* MUST set before calling fcache_add_fragment or
                       * FRAGMENT_EXIT_STUBS */
    f->tag = tag;
    /* Let fragment_create() fill in; other users are building fake fragments */
    DODEBUG({ f->id = -1; });
    f->next_vmarea = NULL;      /* must be set by caller */
    f->prev_vmarea = NULL;      /* must be set by caller */
    f->also.also_vmarea = NULL; /* must be set by caller */

    linkstubs_init(FRAGMENT_EXIT_STUBS(f), direct_exits, indirect_exits, f);

    /* initialize non-ibt entry to top of fragment (caller responsible for
     * setting up prefix)
     */
    f->prefix_size = 0;
#ifdef FRAGMENT_SIZES_STUDY
    record_fragment_size(f->size, (flags & FRAG_IS_TRACE) != 0);
#endif

    f->in_xlate.incoming_stubs = NULL;
#ifdef CUSTOM_TRACES_RET_REMOVAL
    f->num_calls = 0;
    f->num_rets = 0;
#endif

    /* trace-only fields */
    if (TEST(FRAG_IS_TRACE, flags)) {
        trace_only_t *t = TRACE_FIELDS(f);
        t->bbs = NULL;
        /* real num_bbs won't be set until after the trace is emitted,
         * but we need a non-zero value for linkstub_fragment()
         */
        t->num_bbs = 1;
#ifdef PROFILE_RDTSC
        t->count = 0UL;
        t->total_time = (uint64)0;
#endif
    }
}

/* Create a new fragment_t with empty prefix and return it.
 * The fragment_t is allocated on the global or local heap, depending on the flags,
 * unless FRAG_COARSE_GRAIN is set, in which case the fragment_t is a unique
 * temporary struct that is NOT heap allocated and is only safe to use
 * so long as the bb_building_lock is held!
 */
fragment_t *
fragment_create(dcontext_t *dcontext, app_pc tag, int body_size, int direct_exits,
                int indirect_exits, int exits_size, uint flags)
{
    fragment_t *f;
    DEBUG_DECLARE(stats_int_t next_id;)
    DOSTATS({
        /* should watch this stat and if it gets too high need to re-do
         * who needs the post-linkstub offset
         */
        if (linkstub_frag_offs_at_end(flags, direct_exits, indirect_exits))
            STATS_INC(num_fragment_post_linkstub);
    });

    /* ensure no races during a reset */
    ASSERT(!dynamo_resetting);

    if (TEST(FRAG_COARSE_GRAIN, flags)) {
        ASSERT(DYNAMO_OPTION(coarse_units));
        ASSERT_OWN_MUTEX(USE_BB_BUILDING_LOCK(), &bb_building_lock);
        ASSERT(!TEST(FRAG_IS_TRACE, flags));
        ASSERT(TEST(FRAG_SHARED, flags));
        ASSERT(fragment_prefix_size(flags) == 0);
        ASSERT((direct_exits == 0 && indirect_exits == 1) ||
               (indirect_exits == 0 && (direct_exits == 1 || direct_exits == 2)));
        /* FIXME: eliminate this temp fragment and linkstubs and
         * have custom emit and link code that does not require such data
         * structures?  It would certainly be faster code.
         * But would still want to record each exit's target in a convenient
         * data structure, for later linking, unless we try to link in
         * the same pass in which we emit indirect stubs.
         * We could also use fragment_create() and free the resulting struct
         * somewhere and switch to a wrapper at that point.
         */
        memset(&coarse_emit_fragment, 0, sizeof(coarse_emit_fragment));
        f = (fragment_t *)&coarse_emit_fragment;
        /* We do not mark as FRAG_FAKE since this is pretty much a real
         * fragment_t, and we do want to walk its linkstub_t structs, which
         * are present.
         */
    } else {
        f = fragment_create_heap(dcontext, direct_exits, indirect_exits, flags);
    }

    fragment_init_heap(f, tag, direct_exits, indirect_exits, flags);

    /*  To make debugging easier we assign coarse-grain ids in the same namespace
     * as fine-grain fragments, though we won't remember them at all.
     */
    STATS_INC_ASSIGN(num_fragments, next_id);
    IF_X64(ASSERT_TRUNCATE(f->id, int, next_id));
    DOSTATS({ f->id = (int)next_id; });
    DO_GLOBAL_STATS({
        if (!TEST(FRAG_IS_TRACE, f->flags)) {
            RSTATS_INC(num_bbs);
            IF_X64(if (FRAG_IS_32(f->flags)) { STATS_INC(num_32bit_bbs); })
        }
    });
    DOSTATS({
        /* avoid double-counting for adaptive working set */
        if (!fragment_lookup_deleted(dcontext, tag) && !TEST(FRAG_COARSE_GRAIN, flags))
            STATS_INC(num_unique_fragments);
    });
    if (GLOBAL_STATS_ON() &&
        /* num_fragments is debug-only so we use the two release-build stats. */
        (uint)GLOBAL_STAT(num_bbs) + GLOBAL_STAT(num_traces) ==
            INTERNAL_OPTION(reset_at_fragment_count)) {
        ASSERT(INTERNAL_OPTION(reset_at_fragment_count) != 0);
        schedule_reset(RESET_ALL);
    }
    DODEBUG({
        if ((uint)GLOBAL_STAT(num_fragments) == INTERNAL_OPTION(log_at_fragment_count)) {
            /* we started at loglevel 1 and now we raise to the requested level */
            options_make_writable();
            d_r_stats->loglevel = DYNAMO_OPTION(stats_loglevel);
            options_restore_readonly();
            SYSLOG_INTERNAL_INFO("hit -log_at_fragment_count %d, raising loglevel to %d",
                                 INTERNAL_OPTION(log_at_fragment_count),
                                 DYNAMO_OPTION(stats_loglevel));
        }
    });

    /* size is a ushort
     * our offsets are ushorts as well: they assume body_size is small enough, not size
     */
    if (body_size + exits_size + fragment_prefix_size(flags) > MAX_FRAGMENT_SIZE) {
        FATAL_USAGE_ERROR(INSTRUMENTATION_TOO_LARGE, 2, get_application_name(),
                          get_application_pid());
    }
    ASSERT(body_size + exits_size + fragment_prefix_size(flags) <= MAX_FRAGMENT_SIZE);
    /* currently MAX_FRAGMENT_SIZE is USHRT_MAX, but future proofing */
    ASSERT_TRUNCATE(f->size, ushort,
                    (body_size + exits_size + fragment_prefix_size(flags)));
    f->size = (ushort)(body_size + exits_size + fragment_prefix_size(flags));

    /* fcache_add will fill in start_pc, next_fcache,
     * prev_fcache, and fcache_extra
     */
    fcache_add_fragment(dcontext, f);

    /* after fcache_add_fragment so we can call get_fragment_coarse_info */
    DOSTATS({
        if (TEST(FRAG_SHARED, flags)) {
            STATS_INC(num_shared_fragments);
            if (TEST(FRAG_IS_TRACE, flags))
                STATS_INC(num_shared_traces);
            else if (TEST(FRAG_COARSE_GRAIN, flags)) {
                coarse_info_t *info = get_fragment_coarse_info(f);
                if (get_executable_area_coarse_info(f->tag) != info)
                    STATS_INC(num_coarse_secondary);
                STATS_INC(num_coarse_fragments);
            } else
                STATS_INC(num_shared_bbs);
        } else {
            STATS_INC(num_private_fragments);
            if (TEST(FRAG_IS_TRACE, flags))
                STATS_INC(num_private_traces);
            else
                STATS_INC(num_private_bbs);
        }
    });

    /* wait until initialized fragment completely before dumping any stats */
    DOLOG(1, LOG_FRAGMENT | LOG_VMAREAS, {
        if (INTERNAL_OPTION(global_stats_interval) &&
            (f->id % INTERNAL_OPTION(global_stats_interval) == 0)) {
            LOG(GLOBAL, LOG_FRAGMENT, 1, "Created %d fragments\n", f->id);
            dump_global_stats(false);
        }
        if (INTERNAL_OPTION(thread_stats_interval) && INTERNAL_OPTION(thread_stats)) {
            /* FIXME: why do we need a new dcontext? */
            dcontext_t *cur_dcontext = get_thread_private_dcontext();
            if (THREAD_STATS_ON(cur_dcontext) &&
                THREAD_STAT(cur_dcontext, num_fragments) %
                        INTERNAL_OPTION(thread_stats_interval) ==
                    0) {
                dump_thread_stats(cur_dcontext, false);
            }
        }
    });

#ifdef WINDOWS
    DOLOG(1, LOG_FRAGMENT | LOG_VMAREAS, {
        if (f->id % 50000 == 0) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "50K fragment check point: here are the loaded modules:\n");
            print_modules(GLOBAL, DUMP_NOT_XML);
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "50K fragment check point: here are the executable areas:\n");
            print_executable_areas(GLOBAL);
        }
    });
#endif

    return f;
}

/* Creates a new fragment_t+linkstubs from the passed-in fragment and
 * fills in linkstub_t and fragment_t fields, copying the fcache-related fields
 * from the passed-in fragment (so be careful how the fields are used).
 * Meant to be used to create a full fragment from a coarse-grain fragment.
 * Caller is responsible for freeing via fragment_free() w/ the same dcontext
 * passed in here.
 */
fragment_t *
fragment_recreate_with_linkstubs(dcontext_t *dcontext, fragment_t *f_src)
{
    uint num_dir, num_indir;
    uint size;
    fragment_t *f_tgt;
    instrlist_t *ilist;
    linkstub_t *l;
    cache_pc body_end_pc;
    /* Not FAKE since has linkstubs, but still fake in a sense since no fcache
     * slot -- need to mark that?
     */
    uint flags = (f_src->flags & ~FRAG_FAKE);
    ASSERT_CURIOSITY(TEST(FRAG_COARSE_GRAIN, f_src->flags)); /* only use so far */
    /* FIXME case 9325: build from tag here?  Need to exactly re-mangle + re-instrument.
     * We use _exact to get any elided final jmp not counted in size
     */
    ilist = decode_fragment_exact(dcontext, f_src, NULL, NULL, f_src->flags, &num_dir,
                                  &num_indir);
    f_tgt = fragment_create_heap(dcontext, num_dir, num_indir, flags);
    fragment_init_heap(f_tgt, f_src->tag, num_dir, num_indir, flags);

    f_tgt->start_pc = f_src->start_pc;
    /* Can't call this until we have start_pc set */
    body_end_pc = set_linkstub_fields(dcontext, f_tgt, ilist, num_dir, num_indir,
                                      false /*do not emit*/);
    /* Calculate total size */
    IF_X64(ASSERT_TRUNCATE(size, uint, (body_end_pc - f_tgt->start_pc)));
    size = (uint)(body_end_pc - f_tgt->start_pc);
    for (l = FRAGMENT_EXIT_STUBS(f_tgt); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        if (!EXIT_HAS_LOCAL_STUB(l->flags, f_tgt->flags))
            continue; /* it's kept elsewhere */
        size += linkstub_size(dcontext, f_tgt, l);
    }
    ASSERT_TRUNCATE(f_tgt->size, ushort, size);
    f_tgt->size = (ushort)size;
    ASSERT(TEST(FRAG_FAKE, f_src->flags) || size == f_src->size);
    ASSERT_TRUNCATE(f_tgt->prefix_size, byte, fragment_prefix_size(f_src->flags));
    f_tgt->prefix_size = (byte)fragment_prefix_size(f_src->flags);
    ASSERT(TEST(FRAG_FAKE, f_src->flags) || f_src->prefix_size == f_tgt->prefix_size);
    f_tgt->fcache_extra = f_src->fcache_extra;

    instrlist_clear_and_destroy(dcontext, ilist);

    return f_tgt;
}

/* Frees the storage associated with f.
 * Callers should use fragment_delete() instead of this routine, unless they
 * obtained their fragment_t from fragment_recreate_with_linkstubs().
 */
void
fragment_free(dcontext_t *dcontext, fragment_t *f)
{
    dcontext_t *alloc_dc = FRAGMENT_ALLOC_DC(dcontext, f->flags);
    uint heapsz;
    int direct_exits = 0;
    int indirect_exits = 0;
    linkstub_t *l = FRAGMENT_EXIT_STUBS(f);
    for (; l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        if (LINKSTUB_DIRECT(l->flags))
            direct_exits++;
        else {
            ASSERT(LINKSTUB_INDIRECT(l->flags));
            indirect_exits++;
        }
    }
    heapsz = fragment_heap_size(f->flags, direct_exits, indirect_exits);

    STATS_INC(num_fragments_deleted);

    if (HAS_STORED_TRANSLATION_INFO(f)) {
        ASSERT(FRAGMENT_TRANSLATION_INFO(f) != NULL);
        translation_info_free(dcontext, FRAGMENT_TRANSLATION_INFO(f));
    } else
        ASSERT(FRAGMENT_TRANSLATION_INFO(f) == NULL);

    /* N.B.: monitor_remove_fragment() was called in fragment_delete,
     * which is assumed to have been called prior to fragment_free
     */

    linkstub_free_exitstubs(dcontext, f);

    if ((f->flags & FRAG_IS_TRACE) != 0) {
        trace_only_t *t = TRACE_FIELDS(f);
        if (t->bbs != NULL) {
            nonpersistent_heap_free(alloc_dc, t->bbs,
                                    t->num_bbs *
                                        sizeof(trace_bb_info_t) HEAPACCT(ACCT_TRACE));
        }
        nonpersistent_heap_free(alloc_dc, f, heapsz HEAPACCT(ACCT_TRACE));
    } else {
        nonpersistent_heap_free(alloc_dc, f, heapsz HEAPACCT(ACCT_FRAGMENT));
    }
}

/* Returns the end of the fragment body + any local stubs (excluding selfmod copy) */
cache_pc
fragment_stubs_end_pc(fragment_t *f)
{
    if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags))
        return FRAGMENT_SELFMOD_COPY_PC(f);
    else
        return f->start_pc + f->size;
}

/* Returns the end of the fragment body (excluding exit stubs and selfmod copy) */
cache_pc
fragment_body_end_pc(dcontext_t *dcontext, fragment_t *f)
{
    linkstub_t *l;
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (EXIT_HAS_LOCAL_STUB(l->flags, f->flags)) {
            return EXIT_STUB_PC(dcontext, f, l);
        }
    }
    /* must be no stubs after fragment body */
    return fragment_stubs_end_pc(f);
}

/* synchronization routines needed for sideline threads so they don't get
 * fragments they are referencing deleted */

void
fragment_get_fragment_delete_mutex(dcontext_t *dcontext)
{
    if (dynamo_exited || dcontext == GLOBAL_DCONTEXT)
        return;
    d_r_mutex_lock(&(((per_thread_t *)dcontext->fragment_field)->fragment_delete_mutex));
}

void
fragment_release_fragment_delete_mutex(dcontext_t *dcontext)
{
    if (dynamo_exited || dcontext == GLOBAL_DCONTEXT)
        return;
    d_r_mutex_unlock(
        &(((per_thread_t *)dcontext->fragment_field)->fragment_delete_mutex));
}

/* cleaner to have own flags since there are no negative versions
 * of FRAG_SHARED and FRAG_IS_TRACE for distinguishing from "don't care"
 */
enum {
    LOOKUP_TRACE = 0x001,
    LOOKUP_BB = 0x002,
    LOOKUP_PRIVATE = 0x004,
    LOOKUP_SHARED = 0x008,
};

/* A lookup constrained by bb/trace and/or shared/private */
static inline fragment_t *
fragment_lookup_type(dcontext_t *dcontext, app_pc tag, uint lookup_flags)
{
    fragment_t *f;

    LOG(THREAD, LOG_MONITOR, 6, "fragment_lookup_type " PFX " 0x%x\n", tag, lookup_flags);
    if (dcontext != GLOBAL_DCONTEXT && TEST(LOOKUP_PRIVATE, lookup_flags)) {
        /* FIXME: add a hashtablex.h wrapper that checks #entries and
         * grabs lock for us for all lookups?
         */
        /* look at private tables */
        per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
        /* case 147: traces take precedence over bbs */
        if (PRIVATE_TRACES_ENABLED() && TEST(LOOKUP_TRACE, lookup_flags)) {
            /* now try trace table */
            f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, &pt->trace);
            if (f->tag != NULL) {
                ASSERT(f->tag == tag);
                DOLOG(2, LOG_FRAGMENT, {
                    if (DYNAMO_OPTION(shared_traces)) {
                        /* ensure private trace never shadows shared trace */
                        fragment_t *sf;
                        d_r_read_lock(&shared_trace->rwlock);
                        sf = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag,
                                                       shared_trace);
                        d_r_read_unlock(&shared_trace->rwlock);
                        ASSERT(sf->tag == NULL);
                    }
                });
                ASSERT(!TESTANY(FRAG_FAKE | FRAG_COARSE_GRAIN, f->flags));
                return f;
            }
        }
        if (TEST(LOOKUP_BB, lookup_flags) && pt->bb.entries > 0) {
            /* basic block table last */
            f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, &pt->bb);
            if (f->tag != NULL) {
                ASSERT(f->tag == tag);
                DOLOG(2, LOG_FRAGMENT, {
                    if (DYNAMO_OPTION(shared_bbs)) {
                        /* ensure private bb never shadows shared bb, except for
                         * temp privates for trace building
                         */
                        fragment_t *sf;
                        d_r_read_lock(&shared_bb->rwlock);
                        sf = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag,
                                                       shared_bb);
                        d_r_read_unlock(&shared_bb->rwlock);
                        ASSERT(sf->tag == NULL || TEST(FRAG_TEMP_PRIVATE, f->flags));
                    }
                });
                ASSERT(!TESTANY(FRAG_FAKE | FRAG_COARSE_GRAIN, f->flags));
                return f;
            }
        }
    }

    if (TEST(LOOKUP_SHARED, lookup_flags)) {
        if (DYNAMO_OPTION(shared_traces) && TEST(LOOKUP_TRACE, lookup_flags)) {
            /* MUST look at shared trace table before shared bb table,
             * since a shared trace can shadow a shared trace head
             */
            d_r_read_lock(&shared_trace->rwlock);
            f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, shared_trace);
            d_r_read_unlock(&shared_trace->rwlock);
            if (f->tag != NULL) {
                ASSERT(f->tag == tag);
                ASSERT(!TESTANY(FRAG_FAKE | FRAG_COARSE_GRAIN, f->flags));
                return f;
            }
        }

        if (DYNAMO_OPTION(shared_bbs) && TEST(LOOKUP_BB, lookup_flags)) {
            /* MUST look at private trace table before shared bb table,
             * since a private trace can shadow a shared trace head
             */
            d_r_read_lock(&shared_bb->rwlock);
            f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, shared_bb);
            d_r_read_unlock(&shared_bb->rwlock);
            if (f->tag != NULL) {
                ASSERT(f->tag == tag);
                ASSERT(!TESTANY(FRAG_FAKE | FRAG_COARSE_GRAIN, f->flags));
                return f;
            }
        }
    }

    return NULL;
}

/* lookup a fragment tag */
fragment_t *
fragment_lookup(dcontext_t *dcontext, app_pc tag)
{
    return fragment_lookup_type(
        dcontext, tag, LOOKUP_TRACE | LOOKUP_BB | LOOKUP_PRIVATE | LOOKUP_SHARED);
}

/* lookup a fragment tag, but only look in trace tables
 * N.B.: because of shadowing this may not return what fragment_lookup() returns!
 */
fragment_t *
fragment_lookup_trace(dcontext_t *dcontext, app_pc tag)
{
    return fragment_lookup_type(dcontext, tag,
                                LOOKUP_TRACE | LOOKUP_PRIVATE | LOOKUP_SHARED);
}

/* lookup a fragment tag, but only look in bb tables
 * N.B.: because of shadowing this may not return what fragment_lookup() returns!
 */
fragment_t *
fragment_lookup_bb(dcontext_t *dcontext, app_pc tag)
{
    return fragment_lookup_type(dcontext, tag,
                                LOOKUP_BB | LOOKUP_PRIVATE | LOOKUP_SHARED);
}

/* lookup a fragment tag, but only look in shared bb table
 * N.B.: because of shadowing this may not return what fragment_lookup() returns!
 */
fragment_t *
fragment_lookup_shared_bb(dcontext_t *dcontext, app_pc tag)
{
    return fragment_lookup_type(dcontext, tag, LOOKUP_BB | LOOKUP_SHARED);
}

/* lookup a fragment tag, but only look in tables that are the same shared-ness
 * as flags.
 * N.B.: because of shadowing this may not return what fragment_lookup() returns!
 */
fragment_t *
fragment_lookup_same_sharing(dcontext_t *dcontext, app_pc tag, uint flags)
{
    return fragment_lookup_type(
        dcontext, tag,
        LOOKUP_TRACE | LOOKUP_BB |
            (TEST(FRAG_SHARED, flags) ? LOOKUP_SHARED : LOOKUP_PRIVATE));
}

#ifdef DEBUG /*currently only used for debugging */
static fragment_t *
hashtable_pclookup(dcontext_t *dcontext, fragment_table_t *table, cache_pc pc)
{
    uint i;
    fragment_t *f;
    ASSERT_TABLE_SYNCHRONIZED(table, READWRITE); /* lookup requires read or write lock */
    for (i = 0; i < table->capacity; i++) {
        f = table->table[i];
        if (!REAL_FRAGMENT(f))
            continue;
        if (pc >= f->start_pc && pc < (f->start_pc + f->size)) {
            return f;
        }
    }
    return NULL;
}

/* lookup a fragment pc in the fcache by walking all hashtables.
 * we have more efficient methods (fcache_fragment_pclookup) so this is only
 * used for debugging.
 */
fragment_t *
fragment_pclookup_by_htable(dcontext_t *dcontext, cache_pc pc, fragment_t *wrapper)
{
    /* if every fragment is guaranteed to end in 1+ stubs (which
     * is not true for DYNAMO_OPTION(separate_private_stubs) we can
     * simply decode forward until we hit the stub and recover
     * the linkstub_t* from there -- much more efficient than walking
     * all the hashtables, plus nicely handles invisible & removed frags!
     * FIXME: measure perf hit of pclookup, implement this decode strategy.
     * also we can miss invisible or removed fragments (case 122) so we
     * may want this regardless of performance -- see also FIXME below.
     */
    fragment_t *f;
    per_thread_t *pt = NULL;
    if (dcontext != GLOBAL_DCONTEXT) {
        pt = (per_thread_t *)dcontext->fragment_field;
        /* look at private traces first */
        if (PRIVATE_TRACES_ENABLED()) {
            f = hashtable_pclookup(dcontext, &pt->trace, pc);
            if (f != NULL)
                return f;
        }
    }
    if (DYNAMO_OPTION(shared_traces)) {
        /* then shared traces */
        d_r_read_lock(&shared_trace->rwlock);
        f = hashtable_pclookup(dcontext, shared_trace, pc);
        d_r_read_unlock(&shared_trace->rwlock);
        if (f != NULL)
            return f;
    }
    if (DYNAMO_OPTION(shared_bbs)) {
        /* then shared basic blocks */
        d_r_read_lock(&shared_bb->rwlock);
        f = hashtable_pclookup(dcontext, shared_bb, pc);
        d_r_read_unlock(&shared_bb->rwlock);
        if (f != NULL)
            return f;
    }
    if (dcontext != GLOBAL_DCONTEXT) {
        /* now private basic blocks */
        f = hashtable_pclookup(dcontext, &pt->bb, pc);
        if (f != NULL)
            return f;
    }
    if (DYNAMO_OPTION(coarse_units)) {
        coarse_info_t *info = get_executable_area_coarse_info(pc);
        while (info != NULL) { /* loop over primary and secondary unit */
            cache_pc body;
            app_pc tag = fragment_coarse_pclookup(dcontext, info, pc, &body);
            if (tag != NULL) {
                ASSERT(wrapper != NULL);
                fragment_coarse_wrapper(wrapper, tag, body);
                return wrapper;
            }
            ASSERT(info->frozen || info->non_frozen == NULL);
            info = info->non_frozen;
            ASSERT(info == NULL || !info->frozen);
        }
    }
    /* FIXME: shared fragment may have been removed from hashtable but
     * still be in cache, and e.g. handle_modified_code still needs to know about it --
     * should walk deletion vector
     */
    return NULL;
}
#endif /* DEBUG */

/* lookup a fragment pc in the fcache */
fragment_t *
fragment_pclookup(dcontext_t *dcontext, cache_pc pc, fragment_t *wrapper)
{
    /* Rather than walk every single hashtable, including the invisible table,
     * and the pending-deletion list (case 3567), we find the fcache unit
     * and walk it.
     * An even more efficient alternative would be to decode backward, but
     * that's not doable in general.
     *
     * If every fragment is guaranteed to end in 1+ stubs (which
     * is not true for DYNAMO_OPTION(separate_{private,shared}_stubs) we can
     * simply decode forward until we hit the stub and recover
     * the linkstub_t* from there.
     * Or we can decode until we hit a jmp and if it's to a linked fragment_t,
     * search its incoming list.
     * Stub decoding is complicated by custom stubs.
     */
    return fcache_fragment_pclookup(dcontext, pc, wrapper);
}

/* Performs a pclookup and if the result is a coarse-grain fragment, allocates
 * a new fragment_t+linkstubs.
 * Returns in alloc whether the returned fragment_t was allocated and needs to be
 * freed by the caller via fragment_free().
 * If no result is found, alloc is set to false.
 * FIXME: use FRAG_RECREATED flag to indicate allocated instead?
 */
fragment_t *
fragment_pclookup_with_linkstubs(dcontext_t *dcontext, cache_pc pc,
                                 /*OUT*/ bool *alloc)
{
    fragment_t wrapper;
    fragment_t *f = fragment_pclookup(dcontext, pc, &wrapper);
    ASSERT(alloc != NULL);
    if (f != NULL && TEST(FRAG_COARSE_GRAIN, f->flags)) {
        ASSERT(f == &wrapper);
        f = fragment_recreate_with_linkstubs(dcontext, f);
        *alloc = true;
    } else
        *alloc = false;
    return f;
}

/* add f to the ftable */
void
fragment_add(dcontext_t *dcontext, fragment_t *f)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    fragment_table_t *table = GET_FTABLE(pt, f->flags);
    /* no future frags! */
    ASSERT(!TEST(FRAG_IS_FUTURE, f->flags));

    DOCHECK(1, {
        fragment_t *existing = fragment_lookup(dcontext, f->tag);
        ASSERT(existing == NULL ||
               /* For custom traces, we create and persist shadowed trace heads. */
               TEST(FRAG_IS_TRACE_HEAD, f->flags) ||
               TEST(FRAG_IS_TRACE_HEAD, existing->flags) ||
               /* private trace or temp can shadow shared bb */
               (TESTANY(FRAG_IS_TRACE | FRAG_TEMP_PRIVATE, f->flags) &&
                TEST(FRAG_SHARED, f->flags) != TEST(FRAG_SHARED, existing->flags)) ||
               /* shared trace can shadow shared trace head, even with
                * -remove_shared_trace_heads */
               (TESTALL(FRAG_IS_TRACE | FRAG_SHARED, f->flags) &&
                !TEST(FRAG_IS_TRACE, existing->flags) &&
                TESTALL(FRAG_SHARED | FRAG_IS_TRACE_HEAD, existing->flags)));
    });

    /* We'd like the shared fragment table synch to be independent of the
     * bb building synch (which may become more fine-grained in the future),
     * so an add needs to hold the write lock to prevent conflicts with
     * other adds.
     * We may be able to have a scheme where study() and remove() are writers
     * but add() is a reader -- but that's confusing and prone to errors in
     * the future.
     * We assume that synchronizing addition of the same tag is done through
     * other means -- we cannot grab this while performing the lookup
     * w/o making our read locks check to see if we're the writer,
     * which is a perf hit.  Only the actual hashtable add is a "write".
     */
    TABLE_RWLOCK(table, write, lock);
    fragment_add_to_hashtable(dcontext, f, table);
    TABLE_RWLOCK(table, write, unlock);

    /* After resizing a table that is targeted by inlined IBL heads
     * the current fragment will need to be repatched; but, we don't have
     * to update the stubs when using per-type trace tables since the
     * trace table itself is not targeted therefore resizing it doesn't matter.
     */

#ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study)) {
        if (TEST(FRAG_IS_TRACE, f->flags))
            add_shared_block(shared_traces, &shared_traces_lock, f);
        else
            add_shared_block(shared_blocks, &shared_blocks_lock, f);
    }
#endif
}

/* Many options, use macros in fragment.h for readability
 * If output:
 *   dumps f to trace file
 * If remove:
 *   removes f from ftable
 * If unlink:
 *   if f is linked, unlinks f
 *   removes f from incoming link tables
 * If fcache:
 *   deletes f from fcache unit
 */
void
fragment_delete(dcontext_t *dcontext, fragment_t *f, uint actions)
{
    bool acquired_shared_vm_lock = false;
    bool acquired_fragdel_lock = false;
    LOG(THREAD, LOG_FRAGMENT, 3,
        "fragment_delete: *" PFX " F%d(" PFX ")." PFX " %s 0x%x\n", f, f->id, f->tag,
        f->start_pc, TEST(FRAG_IS_TRACE, f->flags) ? "trace" : "bb", actions);
    DOLOG(1, LOG_FRAGMENT, {
        if ((f->flags & FRAG_CANNOT_DELETE) != 0) {
            LOG(THREAD, LOG_FRAGMENT, 2,
                "ERROR: trying to delete undeletable F%d(" PFX ") 0x%x\n", f->id, f->tag,
                actions);
        }
    });
    ASSERT((f->flags & FRAG_CANNOT_DELETE) == 0);
    ASSERT((f->flags & FRAG_IS_FUTURE) == 0);

    /* ensure the actual free of a shared fragment is done only
     * after a multi-stage flush or a reset
     */
    ASSERT(!TEST(FRAG_SHARED, f->flags) || TEST(FRAG_WAS_DELETED, f->flags) ||
           dynamo_exited || dynamo_resetting || is_self_allsynch_flushing());

    /* need to protect ability to reference frag fields and fcache space */
    /* all other options are mostly notification */
    if (monitor_delete_would_abort_trace(dcontext, f) && DYNAMO_OPTION(shared_traces)) {
        /* must acquire shared_vm_areas lock before fragment_delete_mutex (PR 596371) */
        acquired_shared_vm_lock = true;
        acquire_recursive_lock(&change_linking_lock);
        acquire_vm_areas_lock(dcontext, FRAG_SHARED);
    }
    /* XXX: I added the test for FRAG_WAS_DELETED for i#759: does sideline
     * look at fragments after that is set?  If so need to resolve rank order
     * w/ shared_cache_lock.
     */
    if (!TEST(FRAG_WAS_DELETED, f->flags) &&
        (!TEST(FRAGDEL_NO_HEAP, actions) || !TEST(FRAGDEL_NO_FCACHE, actions))) {
        acquired_fragdel_lock = true;
        fragment_get_fragment_delete_mutex(dcontext);
    }

    if (!TEST(FRAGDEL_NO_OUTPUT, actions)) {
        if (TEST(FRAGDEL_NEED_CHLINK_LOCK, actions) && TEST(FRAG_SHARED, f->flags))
            acquire_recursive_lock(&change_linking_lock);
        else {
            ASSERT(!TEST(FRAG_SHARED, f->flags) ||
                   self_owns_recursive_lock(&change_linking_lock));
        }
        fragment_output(dcontext, f);
        if (TEST(FRAGDEL_NEED_CHLINK_LOCK, actions) && TEST(FRAG_SHARED, f->flags))
            release_recursive_lock(&change_linking_lock);
    }

    if (!TEST(FRAGDEL_NO_MONITOR, actions))
        monitor_remove_fragment(dcontext, f);

    if (!TEST(FRAGDEL_NO_UNLINK, actions)) {
        if (TEST(FRAGDEL_NEED_CHLINK_LOCK, actions) && TEST(FRAG_SHARED, f->flags))
            acquire_recursive_lock(&change_linking_lock);
        else {
            ASSERT(!TEST(FRAG_SHARED, f->flags) ||
                   self_owns_recursive_lock(&change_linking_lock));
        }
        if ((f->flags & FRAG_LINKED_INCOMING) != 0)
            unlink_fragment_incoming(dcontext, f);
        if ((f->flags & FRAG_LINKED_OUTGOING) != 0)
            unlink_fragment_outgoing(dcontext, f);
        incoming_remove_fragment(dcontext, f);
        if (TEST(FRAGDEL_NEED_CHLINK_LOCK, actions) && TEST(FRAG_SHARED, f->flags))
            release_recursive_lock(&change_linking_lock);
    }

#ifdef LINUX
    if (TEST(FRAG_HAS_RSEQ_ENDPOINT, f->flags))
        rseq_remove_fragment(dcontext, f);
#endif

    if (!TEST(FRAGDEL_NO_HTABLE, actions))
        fragment_remove(dcontext, f);

    if (!TEST(FRAGDEL_NO_VMAREA, actions))
        vm_area_remove_fragment(dcontext, f);

    if (!TEST(FRAGDEL_NO_FCACHE, actions)) {
        fcache_remove_fragment(dcontext, f);
    }

#ifdef SIDELINE
    if (dynamo_options.sideline)
        sideline_fragment_delete(f);
#endif
    /* For exit-time deletion we invoke instrument_fragment_deleted() directly from
     * hashtable_fragment_reset().  If we add any further conditions on when it should
     * be invoked we should keep the two calls in synch.
     */
    if (dr_fragment_deleted_hook_exists() &&
        (!TEST(FRAGDEL_NO_HEAP, actions) || !TEST(FRAGDEL_NO_FCACHE, actions)))
        instrument_fragment_deleted(dcontext, f->tag, f->flags);
#ifdef UNIX
    if (INTERNAL_OPTION(profile_pcs))
        pcprofile_fragment_deleted(dcontext, f);
#endif
    if (!TEST(FRAGDEL_NO_HEAP, actions)) {
        fragment_free(dcontext, f);
    }
    if (acquired_fragdel_lock)
        fragment_release_fragment_delete_mutex(dcontext);
    if (acquired_shared_vm_lock) {
        release_vm_areas_lock(dcontext, FRAG_SHARED);
        release_recursive_lock(&change_linking_lock);
    }
}

/* Record translation info.  Typically used for pending-delete fragments
 * whose original app code cannot be trusted as it has been modified (case
 * 3559).
 * Caller is required to take care of synch (typically this is called
 * during a flush or during fragment emit)
 */
void
fragment_record_translation_info(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist)
{
    ASSERT(!NEED_SHARED_LOCK(f->flags) || !USE_BB_BUILDING_LOCK() ||
           OWN_MUTEX(&bb_building_lock) || OWN_MUTEX(&trace_building_lock) ||
           is_self_flushing());
    /* We require that either the FRAG_WAS_DELETED flag is set, to
     * indicate there is allocated memory in the live field that needs
     * to be freed, or that the FRAG_HAS_TRANSLATION_INFO field is
     * set, indicating that there is a special appended field pointing
     * to the translation info.
     */
    if (TEST(FRAG_HAS_TRANSLATION_INFO, f->flags)) {
        ASSERT(!TEST(FRAG_WAS_DELETED, f->flags));
        *(FRAGMENT_TRANSLATION_INFO_ADDR(f)) =
            record_translation_info(dcontext, f, ilist);
        ASSERT(FRAGMENT_TRANSLATION_INFO(f) != NULL);
        STATS_INC(num_fragment_translation_stored);
    } else if (TEST(FRAG_WAS_DELETED, f->flags)) {
        ASSERT(f->in_xlate.incoming_stubs == NULL);
        if (INTERNAL_OPTION(safe_translate_flushed)) {
            f->in_xlate.translation_info = record_translation_info(dcontext, f, ilist);
            ASSERT(f->in_xlate.translation_info != NULL);
            ASSERT(FRAGMENT_TRANSLATION_INFO(f) == f->in_xlate.translation_info);
            STATS_INC(num_fragment_translation_stored);
#ifdef INTERNAL
            DODEBUG({
                if (INTERNAL_OPTION(stress_recreate_pc)) {
                    /* verify recreation */
                    stress_test_recreate(dcontext, f, NULL);
                }
            });
#endif
        } else
            f->in_xlate.translation_info = NULL;
    } else
        ASSERT_NOT_REACHED();
}

/* Removes the shared fragment f from all lookup tables in a safe
 * manner that does not require a full flush synch.
 * This routine can be called without synchronizing with other threads.
 */
void
fragment_remove_shared_no_flush(dcontext_t *dcontext, fragment_t *f)
{
    DEBUG_DECLARE(bool shared_ibt_table_used = !TEST(FRAG_IS_TRACE, f->flags)
                      ? DYNAMO_OPTION(shared_bb_ibt_tables)
                      : DYNAMO_OPTION(shared_trace_ibt_tables);)

    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_COARSE_GRAIN, f->flags));

    LOG(GLOBAL, LOG_FRAGMENT, 4, "Remove shared %s " PFX " (@" PFX ")\n",
        fragment_type_name(f), f->tag, f->start_pc);

    /* Strategy: ensure no races in updating table or links by grabbing the high-level
     * locks that are used to synchronize additions to the table itself.
     * Then, simply remove directly from DR-only tables, and safely from ib tables.
     * FIXME: There are still risks that the fragment's link state may change
     */
    LOG(THREAD, LOG_FRAGMENT, 3, "fragment_remove_shared_no_flush: F%d\n", f->id);
    ASSERT(TEST(FRAG_SHARED, f->flags));
    if (TEST(FRAG_IS_TRACE, f->flags)) {
        d_r_mutex_lock(&trace_building_lock);
    }
    /* grab bb building lock even for traces to further prevent link changes */
    d_r_mutex_lock(&bb_building_lock);

    if (TEST(FRAG_WAS_DELETED, f->flags)) {
        /* since caller can't grab locks, we can have a race where someone
         * else deletes first -- in that case nothing to do
         */
        STATS_INC(shared_delete_noflush_race);
        d_r_mutex_unlock(&bb_building_lock);
        if (TEST(FRAG_IS_TRACE, f->flags))
            d_r_mutex_unlock(&trace_building_lock);
        return;
    }

    /* FIXME: try to share code w/ fragment_unlink_for_deletion() */

    /* Make link changes atomic.  We also want vm_area_remove_fragment and
     * marking as deleted to be atomic so we grab vm_areas lock up front.
     */
    acquire_recursive_lock(&change_linking_lock);
    acquire_vm_areas_lock(dcontext, f->flags);

    /* FIXME: share all this code w/ vm_area_unlink_fragments()
     * The work there is just different enough to make that hard, though.
     */
    if (TEST(FRAG_LINKED_OUTGOING, f->flags))
        unlink_fragment_outgoing(GLOBAL_DCONTEXT, f);
    if (TEST(FRAG_LINKED_INCOMING, f->flags))
        unlink_fragment_incoming(GLOBAL_DCONTEXT, f);
    incoming_remove_fragment(GLOBAL_DCONTEXT, f);

    /* remove from ib lookup tables in a safe manner. this removes the
     * frag only from this thread's tables OR from shared tables.
     */
    fragment_prepare_for_removal(GLOBAL_DCONTEXT, f);
    /* fragment_remove ignores the ibl tables for shared fragments */
    fragment_remove(GLOBAL_DCONTEXT, f);
    /* FIXME: we don't currently remove from thread-private ibl tables as that
     * requires walking all of the threads. */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(opt_jit) || !IS_IBL_TARGET(f->flags) ||
                           shared_ibt_table_used);

    vm_area_remove_fragment(dcontext, f);
    /* case 8419: make marking as deleted atomic w/ fragment_t.also_vmarea field
     * invalidation, so that users of vm_area_add_to_list() can rely on this
     * flag to determine validity
     */
    f->flags |= FRAG_WAS_DELETED;

    release_vm_areas_lock(dcontext, f->flags);
    release_recursive_lock(&change_linking_lock);

    /* if a flush occurs, this fragment will be ignored -- so we must store
     * translation info now, just in case
     */
    if (!TEST(FRAG_HAS_TRANSLATION_INFO, f->flags))
        fragment_record_translation_info(dcontext, f, NULL);

    /* try to catch any potential races */
    ASSERT(!TEST(FRAG_LINKED_OUTGOING, f->flags));
    ASSERT(!TEST(FRAG_LINKED_INCOMING, f->flags));

    d_r_mutex_unlock(&bb_building_lock);
    if (TEST(FRAG_IS_TRACE, f->flags)) {
        d_r_mutex_unlock(&trace_building_lock);
    }

    /* no locks can be held when calling this, but f is already unreachable,
     * so can do this outside of locks
     */
    add_to_lazy_deletion_list(dcontext, f);
}

/* Prepares a fragment for delayed deletion by unlinking it.
 * Caller is responsible for calling vm_area_remove_fragment().
 * Caller must hold the change_linking_lock if f is shared.
 */
void
fragment_unlink_for_deletion(dcontext_t *dcontext, fragment_t *f)
{
    ASSERT(!TEST(FRAG_SHARED, f->flags) ||
           self_owns_recursive_lock(&change_linking_lock));
    /* this is not an error since fcache unit flushing puts lazily-deleted
     * fragments onto its list to ensure they are in the same pending
     * delete entry as the normal fragments -- so this routine becomes
     * a nop for them
     */
    if (TEST(FRAG_WAS_DELETED, f->flags)) {
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 5,
            "NOT unlinking F%d(" PFX ") for deletion\n", f->id, f->start_pc);
        STATS_INC(deleted_frags_re_deleted);
        return;
    }
    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 5, "unlinking F%d(" PFX ") for deletion\n",
        f->id, f->start_pc);
    /* we output now to avoid problems reading component blocks
     * of traces after source modules are unloaded
     */
    fragment_output(dcontext, f);
    if (TEST(FRAG_LINKED_OUTGOING, f->flags))
        unlink_fragment_outgoing(dcontext, f);
    if (TEST(FRAG_LINKED_INCOMING, f->flags))
        unlink_fragment_incoming(dcontext, f);
    /* need to remove outgoings from others' incoming and
     * redirect others' outgoing to a future.  former must be
     * done before we remove from hashtable, and latter must be
     * done now to avoid other fragments jumping into stale code,
     * so we do it here and not when we do the real fragment_delete().
     * we don't need to do this for private fragments, but we do anyway
     * so that we can use the fragment_t.incoming_stubs field as a union.
     */
    incoming_remove_fragment(dcontext, f);

    if (TEST(FRAG_SHARED, f->flags)) {
        /* we shouldn't need to worry about someone else changing the
         * link status, since nobody else is allowed to be in DR now,
         * and afterward they all must invalidate any ptrs they hold
         * to flushed fragments, and flushed fragments are not
         * reachable via hashtable or incoming lists!
         */
        /* ASSUMPTION: monitor_remove_fragment does NOT need to
         * be called for all threads, since private trace head
         * ctrs are cleared lazily (only relevant here for
         * -shared_traces) and invalidating last_{exit,fragment}
         * is done by the trace overlap and abort in the main
         * flush loop.
         */
    }

    /* need to remove from htable
     * we used to only do fragment_prepare_for_removal() (xref case 1808)
     * for private fragments, but for case 3559 we want to free up the
     * incoming field at unlink time, and we must do all 3 of unlink,
     * vmarea, and htable freeing at once.
     */
    fragment_remove(dcontext, f);

    /* let recreate_fragment_ilist() know that this fragment is
     * pending deletion and might no longer match the app's state.
     * for shared fragments, also lets people know f is not in a normal
     * vmarea anymore (though actually moving f is up to the caller).
     * additionally the flag indicates that translation info was allocated
     * for this fragment.
     */
    f->flags |= FRAG_WAS_DELETED;

    /* the original app code cannot be used to recreate state, so we must
     * store translation info now
     */
    if (!TEST(FRAG_HAS_TRANSLATION_INFO, f->flags))
        fragment_record_translation_info(dcontext, f, NULL);

    STATS_INC(fragments_unlinked_for_deletion);
}

/* When shared IBT tables are used, update thread-private state
 * to reflect the current parameter values -- hash mask, table address --
 * for the shared ftable.
 */
static bool
update_private_ibt_table_ptrs(
    dcontext_t *dcontext, ibl_table_t *ftable _IF_DEBUG(fragment_entry_t **orig_table))
{
    bool table_change = false;

    if (TEST(FRAG_TABLE_SHARED, ftable->table_flags)) {

        per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;

        if (TEST(FRAG_TABLE_TRACE, ftable->table_flags) &&
            ftable->table != pt->trace_ibt[ftable->branch_type].table) {
            DODEBUG({
                if (orig_table != NULL) {
                    *orig_table = pt->trace_ibt[ftable->branch_type].table;
                }
            });
            table_change = true;
        } else if (DYNAMO_OPTION(bb_ibl_targets) &&
                   !TEST(FRAG_TABLE_TRACE, ftable->table_flags) &&
                   ftable->table != pt->bb_ibt[ftable->branch_type].table) {
            DODEBUG({
                if (orig_table != NULL) {
                    *orig_table = pt->bb_ibt[ftable->branch_type].table;
                }
            });
            table_change = true;
        }
        if (table_change) {
            update_private_ptr_to_shared_ibt_table(
                dcontext, ftable->branch_type,
                TEST(FRAG_TABLE_TRACE, ftable->table_flags), true, /* adjust old
                                                                    * ref-count */
                true /* lock */);
            DODEBUG({
                if (orig_table != NULL)
                    ASSERT(ftable->table != *orig_table);
            });
        }
#ifdef DEBUG
        else if (orig_table != NULL)
            *orig_table = NULL;
#endif
    }
    return table_change;
}

/* Update the thread-private ptrs for the dcontext to point to the
 * currently "live" shared IBT table for branch_type.
 * When adjust_ref_count==true, adjust the ref-count for the old table
 * that the dcontext currently points to.
 * When lock_table==true, lock the shared table prior to manipulating
 * it. If this is false, the caller must have locked the table already.
 * NOTE: If adjust_ref_count=true, lock_table should be true also and
 * the caller should NOT hold the table lock, since the underlying
 * routines that manipulate the ref count lock the table.
 */
static inline void
update_private_ptr_to_shared_ibt_table(dcontext_t *dcontext,
                                       ibl_branch_type_t branch_type, bool trace,
                                       bool adjust_old_ref_count, bool lock_table)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    ibl_table_t *sh_table_ptr =
        trace ? &shared_pt->trace_ibt[branch_type] : &shared_pt->bb_ibt[branch_type];
    ibl_table_t *pvt_table_ptr =
        trace ? &pt->trace_ibt[branch_type] : &pt->bb_ibt[branch_type];

    /* Point to the new table. The shared table must be locked prior to
     * accessing any of its fields. */
    if (lock_table)
        TABLE_RWLOCK(sh_table_ptr, write, lock);
    ASSERT_OWN_WRITE_LOCK(true, &sh_table_ptr->rwlock);

    /* We can get here multiple times due to callers being racy */
    if (pvt_table_ptr->table == sh_table_ptr->table) {
        SYSLOG_INTERNAL_WARNING_ONCE("racy private ptr to shared table update");
        if (lock_table)
            TABLE_RWLOCK(sh_table_ptr, write, unlock);
        return;
    }

    /* Decrement the ref-count for any old table that is pointed to. */
    if (adjust_old_ref_count) {
        dec_table_ref_count(dcontext, pvt_table_ptr, false /*can't be live*/);
    }

    /* We must hold at least the read lock when writing, else we could grab
     * an inconsistent mask/lookuptable pair if another thread is in the middle
     * of resizing the table (case 10405).
     */
    /* Only data for one set of tables is stored in TLS -- for the trace
     * tables in the default config OR the BB tables in shared BBs
     * only mode.
     */
    if ((trace || SHARED_BB_ONLY_IB_TARGETS()) && DYNAMO_OPTION(ibl_table_in_tls))
        update_lookuptable_tls(dcontext, sh_table_ptr);

    ASSERT(pvt_table_ptr->table != sh_table_ptr->table);
    pvt_table_ptr->table = sh_table_ptr->table;
    pvt_table_ptr->hash_mask = sh_table_ptr->hash_mask;
    /* We copy the unaligned value over also because it's used for matching
     * in the dead table list. */
    pvt_table_ptr->table_unaligned = sh_table_ptr->table_unaligned;
    pvt_table_ptr->table_flags = sh_table_ptr->table_flags;
    sh_table_ptr->ref_count++;
    ASSERT(sh_table_ptr->ref_count > 0);

    DODEBUG({
        LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 2,
            "update_table_ptrs %s-%s table: addr " PFX ", mask " PIFX "\n",
            trace ? "trace" : "BB", sh_table_ptr->name, sh_table_ptr->table,
            sh_table_ptr->hash_mask);
        if ((trace || SHARED_BB_ONLY_IB_TARGETS()) && DYNAMO_OPTION(ibl_table_in_tls)) {
            local_state_extended_t *state =
                (local_state_extended_t *)dcontext->local_state;
            LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 2,
                "TLS state %s-%s table: addr " PFX ", mask " PIFX "\n",
                trace ? "trace" : "BB", sh_table_ptr->name,
                state->table_space.table[branch_type].lookuptable,
                state->table_space.table[branch_type].hash_mask);
        }
    });
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_entry_stats)) {
        pvt_table_ptr->entry_stats_to_lookup_table =
            sh_table_ptr->entry_stats_to_lookup_table;
    } else
        pvt_table_ptr->entry_stats_to_lookup_table = 0;
#endif
    if (lock_table)
        TABLE_RWLOCK(sh_table_ptr, write, unlock);

    /* We don't need the lock for this, and holding it will have rank order
     * issues with disassembling in debug builds */
    if (PRIVATE_TRACES_ENABLED() || DYNAMO_OPTION(bb_ibl_targets))
        update_generated_hashtable_access(dcontext);

    STATS_INC(num_shared_ibt_table_ptr_resets);
}

/* When shared IBT tables are used, update thread-private state
 * to reflect the current parameter values -- hash mask, table address --
 * for all tables.
 */
static bool
update_all_private_ibt_table_ptrs(dcontext_t *dcontext, per_thread_t *pt)
{
    bool rc = false;

    if (SHARED_IBT_TABLES_ENABLED()) {
        ibl_branch_type_t branch_type;

        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (DYNAMO_OPTION(shared_trace_ibt_tables)) {
                if (update_private_ibt_table_ptrs(
                        dcontext, &shared_pt->trace_ibt[branch_type] _IF_DEBUG(NULL)))
                    rc = true;
            }
            if (DYNAMO_OPTION(shared_bb_ibt_tables)) {
                if (update_private_ibt_table_ptrs(
                        dcontext, &shared_pt->bb_ibt[branch_type] _IF_DEBUG(NULL)))
                    rc = true;
            }
        }
    }
    return rc;
}

/* Prepares for removal of f from ftable (does not delete f) by pointing the
 * fragment's lookup table entry to an entry point that leads to a cache exit.
 * This routine is needed for safe removal of a fragment by a thread while
 * another thread may be about to jump to it via an IBL. The lookuptable is
 * left in a slightly inconsistent state but one that is accepted by the
 * consistency check. See the note on hashtable_fragment_check_consistency
 * in the routine.
 *
 * Returns true if the fragment was found & removed.
 */
static bool
fragment_prepare_for_removal_from_table(dcontext_t *dcontext, fragment_t *f,
                                        ibl_table_t *ftable)
{
    uint hindex;
    fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);
    fragment_entry_t *pg;

    /* We need the write lock since the start_pc is modified (though we
     * technically may be ok in all scenarios there) and to avoid problems
     * with parallel prepares (shouldn't count on the bb building lock).
     * Grab the lock after all private ptrs are updated since that
     * operation might grab the same lock, if this remove is from a
     * shared IBT table.
     */
    /* FIXME: why do we need to update here? */
    update_private_ibt_table_ptrs(dcontext, ftable _IF_DEBUG(NULL));
    TABLE_RWLOCK(ftable, write, lock);
    pg = hashtable_ibl_lookup_for_removal(fe, ftable, &hindex);
    if (pg != NULL) {
        /* Note all IBL routines that could be looking up an entry
         * in this table have to exit with equivalent register
         * state.  It is possible to enter a private bb IBL
         * lookup, shared bb IBL lookup or trace bb IBL lookup and
         * if a delete race is hit then they would all go to the
         * pending_delete_pc that we'll now supply. They HAVE to
         * be all equivalent independent of the source fragment for this to work.
         *
         * On the other hand we can provide different start_pc
         * values if we have different tables.  We currently don't
         * take advantage of this but we'll leave the power in place.
         */

        /* FIXME: [perf] we could memoize this value in the table itself */
        cache_pc pending_delete_pc =
            PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_target_delete_entry_pc(dcontext, ftable));

        ASSERT(IBL_ENTRIES_ARE_EQUAL(*pg, fe));
        ASSERT(pending_delete_pc != NULL);
        LOG(THREAD, LOG_FRAGMENT, 3,
            "fragment_prepare: remove F%d(" PFX ") from %s[%u] (table addr " PFX "), "
            "set to " PFX "\n",
            f->id, f->tag, ftable->name, hindex, ftable->table, pending_delete_pc);

        /* start_pc_fragment will not match start_pc for the table
         * consistency checks. However, the hashtable_fragment_check_consistency
         * routine verifies that either start_pc/start_pc_fragment match OR that
         * the start_pc_fragment is set to the correct target_delete
         * entry point.
         *
         * We change the tag to FAKE_TAG, which preserves linear probing.
         * In a thread-shared table, this ensures that the same tag will never
         * be present in more than one entry in a table (1 real entry &
         * 1+ target_delete entries).
         * This isn't needed in a thread-private table but doesn't hurt.
         */
        ftable->table[hindex].start_pc_fragment = pending_delete_pc;
        ftable->table[hindex].tag_fragment = FAKE_TAG;
        /* FIXME In a shared table, this means that the entry cannot
         * be overwritten for a fragment with the same tag. */
        ftable->unlinked_entries++;
        ftable->entries--;
        TABLE_RWLOCK(ftable, write, unlock);
        ASSERT(!TEST(FRAG_CANNOT_DELETE, f->flags));
        return true;
    }
    TABLE_RWLOCK(ftable, write, unlock);
    return false;
}

/* Prepares fragment f for removal from all IBL routine targeted tables.
 * Does not actually remove the entry from the table
 * as that can only be done through proper cross-thread synchronization.
 *
 * Returns true if the fragment was found & removed.
 */
bool
fragment_prepare_for_removal(dcontext_t *dcontext, fragment_t *f)
{
    per_thread_t *pt;
    bool prepared = false;
    ibl_branch_type_t branch_type;
    if (!IS_IBL_TARGET(f->flags)) {
        /* nothing to do */
        return false;
    }

    ASSERT(TEST(FRAG_SHARED, f->flags) || dcontext != GLOBAL_DCONTEXT);
    /* We need a real per_thread_t & context below so make sure we have one. */
    if (dcontext == GLOBAL_DCONTEXT) {
        dcontext = get_thread_private_dcontext();
        ASSERT(dcontext != NULL);
    }
    pt = GET_PT(dcontext);
    /* FIXME: as an optimization we could test if IS_IBL_TARGET() is
     * set before looking it up
     */

    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        per_thread_t *local_pt = pt;
        /* We put traces into the trace tables and BBs into the BB tables
         * and sometimes put traces into BB tables also. We never put
         * BBs into a trace table.
         */
        if (TEST(FRAG_IS_TRACE, f->flags)) {
            if (DYNAMO_OPTION(shared_trace_ibt_tables))
                local_pt = shared_pt;
            if (fragment_prepare_for_removal_from_table(
                    dcontext, f, &local_pt->trace_ibt[branch_type]))
                prepared = true;
        }
        if (DYNAMO_OPTION(bb_ibl_targets) &&
            (!TEST(FRAG_IS_TRACE, f->flags) ||
             DYNAMO_OPTION(bb_ibt_table_includes_traces))) {
            if (DYNAMO_OPTION(shared_bb_ibt_tables))
                local_pt = shared_pt;
            if (fragment_prepare_for_removal_from_table(dcontext, f,
                                                        &local_pt->bb_ibt[branch_type])) {
#ifdef DEBUG
                ibl_table_t *ibl_table = GET_IBT_TABLE(pt, f->flags, branch_type);
                fragment_entry_t current;

                TABLE_RWLOCK(ibl_table, read, lock);
                current = hashtable_ibl_lookup(dcontext, (ptr_uint_t)f->tag, ibl_table);
                ASSERT(IBL_ENTRY_IS_EMPTY(current));
                TABLE_RWLOCK(ibl_table, read, unlock);
#endif
                prepared = true;
            }
        }
    }
    return prepared;
}

#ifdef DEBUG
/* FIXME: hashtable_fragment_reset() needs to walk the tables to get these
 * stats, but then we'd need to subtract 1 from all smaller counts -
 * e.g. if an entry is found in 3 tables we can add (1,-1,0) then
 * we'll find it again and we should add (0,1,-1) and one more time
 * when we should add (0,0,1).  In total all will be accounted for to
 * (1,0,0) without messing much else.
 */
static inline void
fragment_ibl_stat_account(uint flags, uint ibls_targeted)
{
    if (TEST(FRAG_IS_TRACE, flags)) {
        switch (ibls_targeted) {
        case 0: break; /* doesn't have to be a target of any IBL routine */
        case 1: STATS_INC(num_traces_in_1_ibl_tables); break;
        case 2: STATS_INC(num_traces_in_2_ibl_tables); break;
        case 3: STATS_INC(num_traces_in_3_ibl_tables); break;
        default: ASSERT_NOT_REACHED();
        }
    } else {
        switch (ibls_targeted) {
        case 0: break; /* doesn't have to be a target of any IBL routine */
        case 1: STATS_INC(num_bbs_in_1_ibl_tables); break;
        case 2: STATS_INC(num_bbs_in_2_ibl_tables); break;
        case 3: STATS_INC(num_bbs_in_3_ibl_tables); break;
        default: ASSERT_NOT_REACHED();
        }
    }
}
#endif

/* Removes f from any IBT tables it is in.
 * If f is in a shared table, only removes if from_shared is true, in
 * which case dcontext must be GLOBAL_DCONTEXT and we must have
 * dynamo_all_threads_synched (case 10137).
 */
void
fragment_remove_from_ibt_tables(dcontext_t *dcontext, fragment_t *f, bool from_shared)
{
    bool shared_ibt_table =
        (!TEST(FRAG_IS_TRACE, f->flags) && DYNAMO_OPTION(shared_bb_ibt_tables)) ||
        (TEST(FRAG_IS_TRACE, f->flags) && DYNAMO_OPTION(shared_trace_ibt_tables));
    fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);

    ASSERT(!from_shared || !shared_ibt_table || !IS_IBL_TARGET(f->flags) ||
           (dcontext == GLOBAL_DCONTEXT && dynamo_all_threads_synched));
    if (((!shared_ibt_table && dcontext != GLOBAL_DCONTEXT) ||
         (from_shared && dcontext == GLOBAL_DCONTEXT && dynamo_all_threads_synched)) &&
        IS_IBL_TARGET(f->flags)) {

        /* trace_t tables should be all private and any deletions should follow strict
         * two step deletion process, we don't need to be holding nested locks when
         * removing any cached entries from the per-type IBL target tables.
         */
        /* FIXME: the stats on ibls_targeted are not quite correct - we need to
         * gather these independently */
        DEBUG_DECLARE(uint ibls_targeted = 0;)
        ibl_branch_type_t branch_type;
        per_thread_t *pt = GET_PT(dcontext);

        ASSERT(TEST(FRAG_IS_TRACE, f->flags) || DYNAMO_OPTION(bb_ibl_targets));
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            /* assuming a single tag can't be both a trace and bb */
            ibl_table_t *ibtable = GET_IBT_TABLE(pt, f->flags, branch_type);

            ASSERT(!TEST(FRAG_TABLE_SHARED, ibtable->table_flags) ||
                   dynamo_all_threads_synched);
            TABLE_RWLOCK(ibtable, write, lock); /* satisfy asserts, even if allsynch */
            if (hashtable_ibl_remove(fe, ibtable)) {
                LOG(THREAD, LOG_FRAGMENT, 2, "  removed F%d(" PFX ") from IBT table %s\n",
                    f->id, f->tag,
                    TEST(FRAG_TABLE_TRACE, ibtable->table_flags)
                        ? ibl_trace_table_type_names[branch_type]
                        : ibl_bb_table_type_names[branch_type]);

                DOSTATS({ ibls_targeted++; });
            }
            TABLE_RWLOCK(ibtable, write, unlock);
        }
        DOSTATS({ fragment_ibl_stat_account(f->flags, ibls_targeted); });
    }
}

/* Removes ibl entries whose tags are in [start,end) */
static uint
fragment_remove_ibl_entries_in_region(dcontext_t *dcontext, app_pc start, app_pc end,
                                      uint frag_flags)
{
    uint total_removed = 0;
    per_thread_t *pt = GET_PT(dcontext);
    ibl_branch_type_t branch_type;
    ASSERT(pt != NULL);
    ASSERT(TEST(FRAG_IS_TRACE, frag_flags) || DYNAMO_OPTION(bb_ibl_targets));
    ASSERT(dcontext == get_thread_private_dcontext() || dynamo_all_threads_synched);
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        ibl_table_t *ibtable = GET_IBT_TABLE(pt, frag_flags, branch_type);
        uint removed = 0;
        TABLE_RWLOCK(ibtable, write, lock);
        if (ibtable->entries > 0) {
            removed = hashtable_ibl_range_remove(dcontext, ibtable, (ptr_uint_t)start,
                                                 (ptr_uint_t)end, NULL);
            /* Ensure a full remove gets everything */
            ASSERT(start != UNIVERSAL_REGION_BASE || end != UNIVERSAL_REGION_END ||
                   (ibtable->entries == 0 &&
                    is_region_memset_to_char(
                        (app_pc)ibtable->table,
                        (ibtable->capacity - 1) * sizeof(fragment_entry_t), 0)));
        }
        LOG(THREAD, LOG_FRAGMENT, 2,
            "  removed %d entries (%d left) in " PFX "-" PFX " from IBT table %s\n",
            removed, ibtable->entries, start, end,
            TEST(FRAG_TABLE_TRACE, ibtable->table_flags)
                ? ibl_trace_table_type_names[branch_type]
                : ibl_bb_table_type_names[branch_type]);
        TABLE_RWLOCK(ibtable, write, unlock);
        total_removed += removed;
    }
    return total_removed;
}

/* Removes shared (and incidentally private, but if no shared targets
 * can exist, may remove nothing) ibl entries whose tags are in
 * [start,end) from all tables associated w/ dcontext.  If
 * dcontext==GLOBAL_DCONTEXT, uses the shared tables, if they exist;
 * else, uses the private tables, if any.
 */
uint
fragment_remove_all_ibl_in_region(dcontext_t *dcontext, app_pc start, app_pc end)
{
    uint removed = 0;
    if (DYNAMO_OPTION(bb_ibl_targets) &&
        ((dcontext == GLOBAL_DCONTEXT && DYNAMO_OPTION(shared_bb_ibt_tables)) ||
         (dcontext != GLOBAL_DCONTEXT && !DYNAMO_OPTION(shared_bb_ibt_tables)))) {
        removed +=
            fragment_remove_ibl_entries_in_region(dcontext, start, end, 0 /*bb table*/);
    }
    if (DYNAMO_OPTION(shared_traces) &&
        ((dcontext == GLOBAL_DCONTEXT && DYNAMO_OPTION(shared_trace_ibt_tables)) ||
         (dcontext != GLOBAL_DCONTEXT && !DYNAMO_OPTION(shared_trace_ibt_tables)))) {
        removed +=
            fragment_remove_ibl_entries_in_region(dcontext, start, end, FRAG_IS_TRACE);
    }
    return removed;
}

/* Removes f from any hashtables -- BB, trace, or future -- and IBT tables
 * it is in, except for shared IBT tables. */
void
fragment_remove(dcontext_t *dcontext, fragment_t *f)
{
    per_thread_t *pt = GET_PT(dcontext);
    fragment_table_t *table = GET_FTABLE(pt, f->flags);

    ASSERT(TEST(FRAG_SHARED, f->flags) || dcontext != GLOBAL_DCONTEXT);
    /* For consistency we remove entries from the IBT
     * tables before we remove them from the trace table.
     */
    fragment_remove_from_ibt_tables(dcontext, f, false /*leave in shared*/);

    /* We need the write lock since deleting shifts elements around (though we
     * technically may be ok in all scenarios there) and to avoid problems with
     * multiple removes at once (shouldn't count on the bb building lock)
     */
    TABLE_RWLOCK(table, write, lock);
    if (hashtable_fragment_remove(f, table)) {
        LOG(THREAD, LOG_FRAGMENT, 4,
            "fragment_remove: removed F%d(" PFX ") from fcache lookup table\n", f->id,
            f->tag);
        TABLE_RWLOCK(table, write, unlock);
        return;
    }
    TABLE_RWLOCK(table, write, unlock);

    /* ok to not find a trace head used to start a trace -- fine to have deleted
     * the trace head
     */
    ASSERT(cur_trace_tag(dcontext) == f->tag
           /* PR 299808: we have invisible temp trace bbs */
           || TEST(FRAG_TEMP_PRIVATE, f->flags));
}

/* Remove f from ftable, replacing it in the hashtable with new_f,
 * which has an identical tag.
 * f's next field is left intact so this can be done while owner is in fcache
 * f is NOT deleted in any other way!
 * To delete later, caller must call fragment_delete w/ remove=false
 */
void
fragment_replace(dcontext_t *dcontext, fragment_t *f, fragment_t *new_f)
{
    per_thread_t *pt = GET_PT(dcontext);
    fragment_table_t *table = GET_FTABLE(pt, f->flags);
    TABLE_RWLOCK(table, write, lock);
    if (hashtable_fragment_replace(f, new_f, table)) {
        fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);
        fragment_entry_t new_fe = FRAGENTRY_FROM_FRAGMENT(new_f);
        LOG(THREAD, LOG_FRAGMENT, 4,
            "removed F%d from fcache lookup table (replaced with F%d) " PFX "->~" PFX
            "," PFX "\n",
            f->id, new_f->id, f->tag, f->start_pc, new_f->start_pc);
        /* Need to replace all entries from the IBL tables that may have this entry */
        if (IS_IBL_TARGET(f->flags)) {
            ibl_branch_type_t branch_type;
            for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
                 branch_type++) {
                ibl_table_t *ibtable = GET_IBT_TABLE(pt, f->flags, branch_type);
                /* currently we don't have shared ib target tables,
                 * otherwise a write lock would come in the picture here
                 */
                ASSERT(!TEST(FRAG_TABLE_SHARED, ibtable->table_flags));
                hashtable_ibl_replace(fe, new_fe, ibtable);
            }
        }
    } else
        ASSERT_NOT_REACHED();
    TABLE_RWLOCK(table, write, unlock);

    /* tell monitor f has disappeared, but do not delete from incoming table
     * or from fcache, also do not dump to trace file
     */
    monitor_remove_fragment(dcontext, f);
}

void
fragment_shift_fcache_pointers(dcontext_t *dcontext, fragment_t *f, ssize_t shift,
                               cache_pc start, cache_pc end, size_t old_size)
{
    per_thread_t *pt = GET_PT(dcontext);

    IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* must re-relativize when copying! */

    /* need to shift all stored cache_pcs.
     * do not need to shift relative pcs pointing to other fragments -- they're
     * all getting shifted too!
     * just need to re-pc-relativize jmps to fixed locations, namely
     * cti's in exit stubs, and call instructions inside fragments.
     */

    LOG(THREAD, LOG_FRAGMENT, 2, "fragment_shift_fcache_pointers: F%d + " SSZFMT "\n",
        f->id, shift);

    ASSERT(!TEST(FRAG_IS_FUTURE, f->flags)); /* only in-cache frags */

    f->start_pc += shift;

    /* Should shift cached lookup entries in all IBL target tables,
     * order doesn't matter here: either way we'll be inconsistent, can't do this
     * within the cache.
     */
    if (IS_IBL_TARGET(f->flags)) {
        ibl_branch_type_t branch_type;
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            /* Of course, we need to shift only pointers into the
             * cache that is getting shifted!
             */
            ibl_table_t *ibtable = GET_IBT_TABLE(pt, f->flags, branch_type);
            fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);
            fragment_entry_t *pg;
            uint hindex;
            TABLE_RWLOCK(ibtable, read, lock);
            pg = hashtable_ibl_lookup_for_removal(fe, ibtable, &hindex);
            if (pg != NULL)
                pg->start_pc_fragment += shift;
            TABLE_RWLOCK(ibtable, read, unlock);
            LOG(THREAD, LOG_FRAGMENT, 2,
                "fragment_table_shift_fcache_pointers: %s ibt %s shifted by %d\n",
                TEST(FRAG_IS_TRACE, f->flags) ? "trace" : "BB", ibtable->name, shift);
        }
    }

    linkstubs_shift(dcontext, f, shift);

    DOLOG(6, LOG_FRAGMENT, { /* print after start_pc updated so get actual code */
                             LOG(THREAD, LOG_FRAGMENT, 6,
                                 "before shifting F%d (" PFX ")\n", f->id, f->tag);
                             disassemble_fragment(dcontext, f, d_r_stats->loglevel < 3);
    });

#ifdef X86
    if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
        /* just re-finalize to update */
        finalize_selfmod_sandbox(dcontext, f);
    }
#endif

    /* inter-cache links must be redone, but all fragment entry pcs must be
     * fixed up first, so that's done separately
     */

    /* re-do pc-relative targets outside of cache */
    shift_ctis_in_fragment(dcontext, f, shift, start, end, old_size);
#ifdef CHECK_RETURNS_SSE2
    finalize_return_check(dcontext, f);
#endif

    DOLOG(6, LOG_FRAGMENT, {
        LOG(THREAD, LOG_FRAGMENT, 6, "after shifting F%d (" PFX ")\n", f->id, f->tag);
        disassemble_fragment(dcontext, f, d_r_stats->loglevel < 3);
    });
}

/* this routine only copies data structures like bbs and statistics
 */
void
fragment_copy_data_fields(dcontext_t *dcontext, fragment_t *f_src, fragment_t *f_dst)
{
    if ((f_src->flags & FRAG_IS_TRACE) != 0) {
        trace_only_t *t_src = TRACE_FIELDS(f_src);
        trace_only_t *t_dst = TRACE_FIELDS(f_dst);
        ASSERT((f_dst->flags & FRAG_IS_TRACE) != 0);
        if (t_src->bbs != NULL) {
            t_dst->bbs = nonpersistent_heap_alloc(
                dcontext, t_src->num_bbs * sizeof(trace_bb_info_t) HEAPACCT(ACCT_TRACE));
            memcpy(t_dst->bbs, t_src->bbs, t_src->num_bbs * sizeof(trace_bb_info_t));
            t_dst->num_bbs = t_src->num_bbs;
        }

#ifdef PROFILE_RDTSC
        t_dst->count = t_src->count;
        t_dst->total_time = t_src->total_time;
#endif
    }
}

#if defined(DEBUG) && defined(INTERNAL)
static void
dump_lookup_table(dcontext_t *dcontext, ibl_table_t *ftable)
{
    uint i;
    cache_pc target_delete =
        PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_target_delete_entry_pc(dcontext, ftable));

    ASSERT(target_delete != NULL);
    ASSERT(ftable->table != NULL);
    LOG(THREAD, LOG_FRAGMENT, 1, "%6s %10s %10s -- %s\n", "i", "tag", "target",
        ftable->name);
    /* need read lock to traverse the table */
    TABLE_RWLOCK(ftable, read, lock);
    for (i = 0; i < ftable->capacity; i++) {
        if (ftable->table[i].tag_fragment != 0) {
            if (ftable->table[i].start_pc_fragment == target_delete) {
                LOG(THREAD, LOG_FRAGMENT, 1, "%6x " PFX " target_delete\n", i,
                    ftable->table[i].tag_fragment);
                ASSERT(ftable->table[i].tag_fragment == FAKE_TAG);
            } else {
                LOG(THREAD, LOG_FRAGMENT, 1, "%6x " PFX " " PFX "\n", i,
                    ftable->table[i].tag_fragment, ftable->table[i].start_pc_fragment);
            }
        }
        DOCHECK(1, { hashtable_ibl_check_consistency(dcontext, ftable, i); });
    }
    TABLE_RWLOCK(ftable, read, unlock);
}
#endif

#ifdef DEBUG
/* used only for debugging purposes, check if IBL routine leaks due to wraparound  */
/* interesting only when not using INTERNAL_OPTION(ibl_sentinel_check) */
static bool
is_fragment_index_wraparound(dcontext_t *dcontext, ibl_table_t *ftable, fragment_t *f)
{
    uint hindex = HASH_FUNC((ptr_uint_t)f->tag, ftable);
    uint found_at_hindex;
    fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);
    fragment_entry_t *pg = hashtable_ibl_lookup_for_removal(fe, ftable, &found_at_hindex);
    ASSERT(pg != NULL);
    ASSERT(IBL_ENTRIES_ARE_EQUAL(*pg, fe));
    LOG(THREAD, LOG_FRAGMENT, 3,
        "is_fragment_index_wraparound F%d, tag " PFX ", found_at_hindex 0x%x, "
        "preferred 0x%x\n",
        f->id, f->tag, found_at_hindex, hindex);
    return (found_at_hindex < hindex); /* wraparound */
}
#endif /* DEBUG */

void
fragment_update_ibl_tables(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    DEBUG_DECLARE(bool tables_updated =)
    update_all_private_ibt_table_ptrs(dcontext, pt);
    DODEBUG({
        if (tables_updated)
            STATS_INC(num_shared_tables_updated_delete);
    });
}

static void
fragment_add_ibl_target_helper(dcontext_t *dcontext, fragment_t *f,
                               ibl_table_t *ibl_table)
{
    fragment_entry_t current;
    fragment_entry_t fe = FRAGENTRY_FROM_FRAGMENT(f);

    /* Never add a BB to a trace table. */
    ASSERT(!(!TEST(FRAG_IS_TRACE, f->flags) &&
             TEST(FRAG_TABLE_TRACE, ibl_table->table_flags)));

    /* adding is a write operation */
    TABLE_RWLOCK(ibl_table, write, lock);
    /* This is the last time the table lock is grabbed before adding the frag so
     * check here to account for the race in the time between the
     * FRAG_IS_TRACE_HEAD check in add_ibl_target() and now. We never add trace
     * heads to an IBT target table.
     */
    if (TEST(FRAG_IS_TRACE_HEAD, f->flags)) {
        TABLE_RWLOCK(ibl_table, write, unlock);
        STATS_INC(num_th_bb_ibt_add_race);
        return;
    }
    /* For shared tables, check again in case another thread snuck in
     * before the preceding lock and added the target. */
    if (TEST(FRAG_TABLE_SHARED, ibl_table->table_flags)) {
        current = hashtable_ibl_lookup(dcontext, (ptr_uint_t)f->tag, ibl_table);
        if (IBL_ENTRY_IS_EMPTY(current))
            hashtable_ibl_add(dcontext, fe, ibl_table);
        /* We don't ever expect to find a like-tagged fragment. A BB
         * can be unlinked due to eviction or when it's marked as a trace
         * head. Eviction (for example, due to cache consistency)
         * sets start_pc_fragment to FAKE_TAG, so there can't be
         * a tag match; &unlinked_fragment is returned, and this
         * applies to traces also. For trace head marking, FAKE_TAG
         * is also set so &unlinked_fragment is returned.
         *
         * If we didn't set FAKE_TAG for an unlinked entry, then it
         * could be clobbered with a new fragment w/the same tag.
         * In a shared table, unlinked entries cannot be clobbered
         * except by fragments w/the same tags, so this could help
         * limit the length of collision chains.
         */
    } else {
        hashtable_ibl_add(dcontext, fe, ibl_table);
    }
    TABLE_RWLOCK(ibl_table, write, unlock);
    DOSTATS({
        if (!TEST(FRAG_IS_TRACE, f->flags))
            STATS_INC(num_bbs_ibl_targets);
        /* We assume that traces can be added to trace and BB IBT tables but
         * not just to BB tables. We count only traces added to trace tables
         * so that we don't double increment.
         */
        else if (TEST(FRAG_IS_TRACE, f->flags) &&
                 TEST(FRAG_TABLE_TRACE, ibl_table->table_flags))
            STATS_INC(num_traces_ibl_targets);
    });

    /* Adding current exit to help calculate an estimated
     * indirect branch fan-out, that is function fan-in for
     * returns.  Note that other IBL hits to the same place
     * will not have exits associated.
     */
    LOG(THREAD, LOG_FRAGMENT, 2,
        "fragment_add_ibl_target added F%d(" PFX "), branch %d, to %s, on exit from " PFX
        "\n",
        f->id, f->tag, ibl_table->branch_type, ibl_table->name,
        LINKSTUB_FAKE(dcontext->last_exit)
            ? 0
            : EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit));
    DOLOG(5, LOG_FRAGMENT, {
        dump_lookuptable_tls(dcontext);
        hashtable_ibl_dump_table(dcontext, ibl_table);
        dump_lookup_table(dcontext, ibl_table);
    });
    DODEBUG({
        if (TEST(FRAG_SHARED, f->flags) && !TEST(FRAG_IS_TRACE, f->flags)) {
            LOG(THREAD, LOG_FRAGMENT, 2, "add_ibl_target: shared BB F%d(" PFX ") added\n",
                f->id, f->tag);
        }
    });
}

/* IBL targeted fragments per branch type */
void
fragment_add_ibl_target(dcontext_t *dcontext, app_pc tag, ibl_branch_type_t branch_type)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    fragment_t *f = NULL;
    fragment_t wrapper;

    if (SHARED_BB_ONLY_IB_TARGETS()) {
        f = fragment_lookup_bb(dcontext, tag);
        if (f == NULL) {
            f = fragment_coarse_lookup_wrapper(dcontext, tag, &wrapper);
            if (f != NULL) {
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
                if (TEST(COARSE_FILL_IBL_MASK(branch_type),
                         DYNAMO_OPTION(coarse_fill_ibl))) {
                    /* On-demand per-type ibl filling from the persisted RAC/RCT
                     * table.  We limit to the first thread to ask for it by
                     * clearing the coarse_info_t pending_table fields.
                     */
                    /* FIXME: combine w/ the coarse lookup to do this once only */
                    coarse_info_t *coarse = get_fragment_coarse_info(f);
                    ASSERT(coarse != NULL);
                    if (coarse->persisted &&
                        exists_coarse_ibl_pending_table(dcontext, coarse, branch_type)) {
                        bool in_persisted_ibl = false;
                        d_r_mutex_lock(&coarse->lock);
                        if (exists_coarse_ibl_pending_table(dcontext, coarse,
                                                            branch_type)) {
                            ibl_table_t *ibl_table =
                                GET_IBT_TABLE(pt, f->flags, branch_type);
                            coarse_persisted_fill_ibl(dcontext, coarse, branch_type);
                            TABLE_RWLOCK(ibl_table, read, lock);
                            if (!IBL_ENTRY_IS_EMPTY(hashtable_ibl_lookup(
                                    dcontext, (ptr_uint_t)tag, ibl_table)))
                                in_persisted_ibl = true;
                            TABLE_RWLOCK(ibl_table, read, unlock);
                            if (in_persisted_ibl) {
                                d_r_mutex_unlock(&coarse->lock);
                                return;
                            }
                        }
                        d_r_mutex_unlock(&coarse->lock);
                    }
                }
#endif /* defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH) */
            }
        }
    } else {
        f = fragment_lookup_trace(dcontext, tag);
        if (f == NULL && DYNAMO_OPTION(bb_ibl_targets)) {
            /* Populate with bb's that are not trace heads */
            f = fragment_lookup_bb(dcontext, tag);
            /* We don't add trace heads OR when a trace is targetting a BB. In the
             * latter case, the BB will shortly be marked as a trace head and
             * removed from the IBT table so we don't needlessly add it.
             */
            if (f != NULL &&
                (TEST(FRAG_IS_TRACE_HEAD, f->flags) ||
                 TEST(FRAG_IS_TRACE, dcontext->last_fragment->flags))) {
                /* XXX: change the logic if trace headness becomes a private property */
                f = NULL;                    /* ignore fragment */
                STATS_INC(num_ib_th_target); /* counted in num_ibt_cold_misses */
            }
        }
    }

    LOG(THREAD, LOG_FRAGMENT, 3,
        "fragment_add_ibl_target tag " PFX ", branch %d, F%d %s\n", tag, branch_type,
        f != NULL ? f->id : 0,
        (f != NULL && TEST(FRAG_IS_TRACE, f->flags)) ? "existing trace" : "");

    /* a valid IBT fragment exists */
    if (f != NULL) {
        ibl_table_t *ibl_table = GET_IBT_TABLE(pt, f->flags, branch_type);
        DEBUG_DECLARE(fragment_entry_t *orig_lookuptable = NULL;)
        fragment_entry_t current;

        /* Make sure this thread's local ptrs & state is current in case a
         * shared table resize occurred while it was in the cache. We update
         * only during an IBL miss, since that's the first time that
         * accessing the old table inflicted a cost (a context switch).
         */
        /* NOTE We could be more aggressive and update the private ptrs for
         * all tables, not just the one being added to, by calling
         * update_all_private_ibt_table_ptrs(). We could be even more
         * aggressive by updating all ptrs on every cache exit in
         * enter_couldbelinking() but that could also prove to be more
         * expensive by invoking the update logic when an IBL miss didn't
         * occur. However, more frequent updates could lead to old tables
         * being freed earlier. We can revisit this if we see old tables
         * piling up and not being freed in a timely manner.
         */
        update_private_ibt_table_ptrs(dcontext, ibl_table _IF_DEBUG(&orig_lookuptable));

        /* We can't place a private fragment into a thread-shared table.
         * Nothing prevents a sandboxed or ignore syscalls frag from being
         * the target of an IB. This is covered by case 5836.
         *
         * We don't need to re-check in the add_ibl_target_helper because
         * the shared/private property applies to all IBT tables -- either
         * all are shared or none are.
         */
        if (TEST(FRAG_TABLE_SHARED, ibl_table->table_flags) &&
            !TEST(FRAG_SHARED, f->flags)) {
            STATS_INC(num_ibt_shared_private_conflict);
            return;
        }

        ASSERT(TEST(FRAG_IS_TRACE, f->flags) ==
               TEST(FRAG_TABLE_TRACE, ibl_table->table_flags));
        /* We can't assert that an IBL target isn't a trace head due to a race
         * between trace head marking and adding to a table. See the comments
         * in fragment_add_to_hashtable().
         */
        TABLE_RWLOCK(ibl_table, read, lock);
        current = hashtable_ibl_lookup(dcontext, (ptr_uint_t)tag, ibl_table);
        TABLE_RWLOCK(ibl_table, read, unlock);
        /* Now that we set the fragment_t* for any unlinked entry to
         * &unlinked_fragment -- regardless of why it was unlinked --  and also
         * set the lookup table tag to FAKE_TAG, we should never find a fragment
         * with the same tag and should never have an unlinked marker returned
         * here.
         */
        ASSERT(!IBL_ENTRY_IS_INVALID(current));
        if (IBL_ENTRY_IS_EMPTY(current)) {
            DOLOG(5, LOG_FRAGMENT, {
                dump_lookuptable_tls(dcontext);
                hashtable_ibl_dump_table(dcontext, ibl_table);
                dump_lookup_table(dcontext, ibl_table);
            });
            fragment_add_ibl_target_helper(dcontext, f, ibl_table);
            /* When using BB2BB IBL w/trace building, we add trace targets
             * to the BB table. (We always add a trace target to the trace
             * table.) We fool the helper routine into using the BB
             * table by passing in a non-trace value for the flags argument.
             */
            if (TEST(FRAG_IS_TRACE, f->flags) && DYNAMO_OPTION(bb_ibl_targets) &&
                DYNAMO_OPTION(bb_ibt_table_includes_traces)) {
                ibl_table_t *ibl_table_too =
                    GET_IBT_TABLE(pt, f->flags & ~FRAG_IS_TRACE, branch_type);

                ASSERT(ibl_table_too != NULL);
                ASSERT(!TEST(FRAG_TABLE_TRACE, ibl_table_too->table_flags));
                /* Make sure this thread's local ptrs & state is up to
                 * date in case a resize occurred while it was in the cache. */
                update_private_ibt_table_ptrs(dcontext, ibl_table_too _IF_DEBUG(NULL));
                fragment_add_ibl_target_helper(dcontext, f, ibl_table_too);
            }
        } else {
            DEBUG_DECLARE(const char *reason;)
#ifdef DEBUG
            if (is_building_trace(dcontext)) {
                reason = "trace building";
                STATS_INC(num_ibt_exit_trace_building);
            } else if (TEST(FRAG_WAS_DELETED, dcontext->last_fragment->flags)) {
                reason = "src unlinked (frag deleted)";
                STATS_INC(num_ibt_exit_src_unlinked_frag_deleted);
            } else if (!TEST(LINK_LINKED, dcontext->last_exit->flags) &&
                       TESTALL(FRAG_SHARED | FRAG_IS_TRACE_HEAD,
                               dcontext->last_fragment->flags) &&
                       fragment_lookup_type(dcontext, dcontext->last_fragment->tag,
                                            LOOKUP_TRACE | LOOKUP_SHARED) != NULL) {
                /* Another thread unlinked src as part of replacing it with
                 * a new trace while this thread was in there (see case 5634
                 * for details) */
                reason = "src unlinked (shadowed)";
                STATS_INC(num_ibt_exit_src_unlinked_shadowed);
            } else if (!INTERNAL_OPTION(ibl_sentinel_check) &&
                       is_fragment_index_wraparound(dcontext, ibl_table, f)) {
                reason = "sentinel";
                STATS_INC(num_ibt_leaks_likely_sentinel);
            } else if (TEST(FRAG_SELFMOD_SANDBOXED, dcontext->last_fragment->flags)) {
                reason = "src sandboxed";
                STATS_INC(num_ibt_exit_src_sandboxed);
            } else if (TEST(FRAG_TABLE_SHARED, ibl_table->table_flags) &&
                       orig_lookuptable != ibl_table->table) {
                /* A table resize could cause a miss when the target is
                 * in the new table. */
                reason = "shared IBT table resize";
                STATS_INC(num_ibt_exit_shared_table_resize);
            } else if (DYNAMO_OPTION(bb_ibl_targets) &&
                       IS_SHARED_SYSCALLS_LINKSTUB(dcontext->last_exit) &&
                       !DYNAMO_OPTION(disable_traces) && !TEST(FRAG_IS_TRACE, f->flags)) {
                reason = "shared syscall exit cannot target BBs";
                STATS_INC(num_ibt_exit_src_trace_shared_syscall);
            } else if (DYNAMO_OPTION(bb_ibl_targets) && TEST(FRAG_IS_TRACE, f->flags) &&
                       !DYNAMO_OPTION(bb_ibt_table_includes_traces)) {
                reason = "BBs do not target traces";
                STATS_INC(num_ibt_exit_src_trace_shared_syscall);
            } else if (!INTERNAL_OPTION(link_ibl)) {
                reason = "-no_link_ibl prevents ibl";
                STATS_INC(num_ibt_exit_nolink);
            } else if (DYNAMO_OPTION(disable_traces) &&
                       !TEST(FRAG_LINKED_OUTGOING, dcontext->last_fragment->flags)) {
                reason = "IBL fragment unlinked in signal handler";
                STATS_INC(num_ibt_exit_src_unlinked_signal);
            } else {
                reason = "BAD leak?";
                DOLOG(3, LOG_FRAGMENT, {
                    hashtable_ibl_dump_table(dcontext, ibl_table);
                    hashtable_ibl_study(dcontext, ibl_table, 0 /*table consistent*/);
                });
                STATS_INC(num_ibt_exit_unknown);
                ASSERT_CURIOSITY_ONCE(false && "fragment_add_ibl_target unknown reason");
            }
            /* nothing to do, just sanity checking */
            LOG(THREAD, LOG_FRAGMENT, 2,
                "fragment_add_ibl_target tag " PFX ", F%d already added - %s\n", tag,
                f->id, reason);
#endif
        }
    } else {
        STATS_INC(num_ibt_cold_misses);
    }
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(stay_on_trace_stats)) {
        /* best effort: adjust for 32bit counter overflow occasionally
         * we'll get a hashtable leak only when not INTERNAL_OPTION(ibl_sentinel_check)
         */
        check_stay_on_trace_stats_overflow(dcontext, branch_type);
    }
#endif /* HASHTABLE_STATISTICS */
    DOLOG(4, LOG_FRAGMENT, { dump_lookuptable_tls(dcontext); });
}

/**********************************************************************/
/* FUTURE FRAGMENTS */

/* create a new fragment with empty prefix and return it
 */
static future_fragment_t *
fragment_create_future(dcontext_t *dcontext, app_pc tag, uint flags)
{
    dcontext_t *alloc_dc = FRAGMENT_ALLOC_DC(dcontext, flags);
    future_fragment_t *fut = (future_fragment_t *)nonpersistent_heap_alloc(
        alloc_dc, sizeof(future_fragment_t) HEAPACCT(ACCT_FRAG_FUTURE));
    ASSERT(!NEED_SHARED_LOCK(flags) || self_owns_recursive_lock(&change_linking_lock));
    LOG(THREAD, LOG_FRAGMENT, 4, "Created future fragment " PFX " w/ flags 0x%08x\n", tag,
        flags | FRAG_FAKE | FRAG_IS_FUTURE);
    STATS_INC(num_future_fragments);
    DOSTATS({
        if (TEST(FRAG_SHARED, flags))
            STATS_INC(num_shared_future_fragments);
    });
    fut->tag = tag;
    fut->flags = flags | FRAG_FAKE | FRAG_IS_FUTURE;
    fut->incoming_stubs = NULL;
    return fut;
}

static void
fragment_free_future(dcontext_t *dcontext, future_fragment_t *fut)
{
    dcontext_t *alloc_dc = FRAGMENT_ALLOC_DC(dcontext, fut->flags);
    LOG(THREAD, LOG_FRAGMENT, 4, "Freeing future fragment " PFX "\n", fut->tag);
    ASSERT(fut->incoming_stubs == NULL);
    nonpersistent_heap_free(alloc_dc, fut,
                            sizeof(future_fragment_t) HEAPACCT(ACCT_FRAG_FUTURE));
}

future_fragment_t *
fragment_create_and_add_future(dcontext_t *dcontext, app_pc tag, uint flags)
{
    per_thread_t *pt = GET_PT(dcontext);
    future_fragment_t *fut = fragment_create_future(dcontext, tag, flags);
    fragment_table_t *futtable = GET_FTABLE(pt, fut->flags);
    ASSERT(!NEED_SHARED_LOCK(flags) || self_owns_recursive_lock(&change_linking_lock));
    /* adding to the table is a write operation */
    TABLE_RWLOCK(futtable, write, lock);
    fragment_add_to_hashtable(dcontext, (fragment_t *)fut, futtable);
    TABLE_RWLOCK(futtable, write, unlock);
    return fut;
}

void
fragment_delete_future(dcontext_t *dcontext, future_fragment_t *fut)
{
    per_thread_t *pt = GET_PT(dcontext);
    fragment_table_t *futtable = GET_FTABLE(pt, fut->flags);
    ASSERT(!NEED_SHARED_LOCK(fut->flags) ||
           self_owns_recursive_lock(&change_linking_lock));
    /* removing from the table is a write operation */
    TABLE_RWLOCK(futtable, write, lock);
    hashtable_fragment_remove((fragment_t *)fut, futtable);
    TABLE_RWLOCK(futtable, write, unlock);
    fragment_free_future(dcontext, fut);
}

/* We do not want to remove futures from a flushed region if they have
 * incoming links (i#609).
 */
static bool
fragment_delete_future_filter(fragment_t *f)
{
    future_fragment_t *fut = (future_fragment_t *)f;
    ASSERT(TEST(FRAG_IS_FUTURE, f->flags));
    return (fut->incoming_stubs == NULL);
}

static uint
fragment_delete_futures_in_region(dcontext_t *dcontext, app_pc start, app_pc end)
{
    per_thread_t *pt = GET_PT(dcontext);
    uint flags = FRAG_IS_FUTURE | (dcontext == GLOBAL_DCONTEXT ? FRAG_SHARED : 0);
    fragment_table_t *futtable = GET_FTABLE(pt, flags);
    uint removed;
    /* Higher-level lock needed since we do lookup+add w/o holding table lock between */
    ASSERT(!NEED_SHARED_LOCK(flags) || self_owns_recursive_lock(&change_linking_lock));
    TABLE_RWLOCK(futtable, write, lock);
    removed =
        hashtable_fragment_range_remove(dcontext, futtable, (ptr_uint_t)start,
                                        (ptr_uint_t)end, fragment_delete_future_filter);
    TABLE_RWLOCK(futtable, write, unlock);
    return removed;
}

future_fragment_t *
fragment_lookup_future(dcontext_t *dcontext, app_pc tag)
{
    /* default is to lookup shared, since private only sometimes exists,
     * and often only care about trace head, for which always use shared
     */
    uint flags = SHARED_FRAGMENTS_ENABLED() ? FRAG_SHARED : 0;
    per_thread_t *pt = GET_PT(dcontext);
    fragment_table_t *futtable = GET_FTABLE(pt, FRAG_IS_FUTURE | flags);
    fragment_t *f;
    TABLE_RWLOCK(futtable, read, lock);
    f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, futtable);
    TABLE_RWLOCK(futtable, read, unlock);
    if (f != &null_fragment)
        return (future_fragment_t *)f;
    return NULL;
}

future_fragment_t *
fragment_lookup_private_future(dcontext_t *dcontext, app_pc tag)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    fragment_table_t *futtable = GET_FTABLE(pt, FRAG_IS_FUTURE);
    fragment_t *f = hashtable_fragment_lookup(dcontext, (ptr_uint_t)tag, futtable);
    if (f != &null_fragment)
        return (future_fragment_t *)f;
    return NULL;
}

/* END FUTURE FRAGMENTS
 **********************************************************************/

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/* FIXME: move to rct.c when we move the whole app_pc table there */

#    define STATS_RCT_ADD(which, stat, val) \
        DOSTATS({                           \
            if ((which) == RCT_RAC)         \
                STATS_ADD(rac_##stat, val); \
            else                            \
                STATS_ADD(rct_##stat, val); \
        })

static inline bool
rct_is_global_table(rct_module_table_t *permod)
{
    return (permod == &rac_non_module_table ||
            IF_UNIX_ELSE(permod == &rct_global_table, false));
}

static inline rct_module_table_t *
rct_get_table(app_pc tag, rct_type_t which)
{
    rct_module_table_t *permod = os_module_get_rct_htable(tag, which);
    if (permod == NULL) { /* not a module */
        if (which == RCT_RAC)
            permod = &rac_non_module_table;
    }
    return permod;
}

/* returns NULL if not found */
static app_pc
rct_table_lookup_internal(dcontext_t *dcontext, app_pc tag, rct_module_table_t *permod)
{
    app_pc actag = NULL;
    ASSERT(os_get_module_info_locked());
    if (permod != NULL) {
        /* Check persisted table first as it's likely to be larger and
         * it needs no read lock
         */
        if (permod->persisted_table != NULL) {
            actag = hashtable_app_pc_rlookup(dcontext, (ptr_uint_t)tag,
                                             permod->persisted_table);
        }
        if (actag == NULL && permod->live_table != NULL) {
            actag =
                hashtable_app_pc_rlookup(dcontext, (ptr_uint_t)tag, permod->live_table);
        }
    }
    return actag;
}

/* returns NULL if not found */
static app_pc
rct_table_lookup(dcontext_t *dcontext, app_pc tag, rct_type_t which)
{
    app_pc actag = NULL;
    rct_module_table_t *permod;
    ASSERT(which >= 0 && which < RCT_NUM_TYPES);
    os_get_module_info_lock();
    permod = rct_get_table(tag, which);
    actag = rct_table_lookup_internal(dcontext, tag, permod);
    os_get_module_info_unlock();
    return actag;
}

/* Caller must hold the higher-level lock.
 * Returns whether added a new entry or not.
 */
static bool
rct_table_add(dcontext_t *dcontext, app_pc tag, rct_type_t which)
{
    rct_module_table_t *permod;
    /* we use a higher-level lock to synchronize the lookup + add
     * combination with other simultaneous adds as well as with removals
     */
    /* FIXME We could use just the table lock for the lookup+add. This is cleaner
     * than using another lock during the entire routine and acquiring & releasing
     * the table lock in read mode for the lookup and then acquiring & releasing
     * it again in write mode for the add. Also, any writes to the table outside
     * of this routine would be blocked (as is desired). The down side is that
     * reads would be blocked during the entire operation.
     * The #ifdef DEBUG lookup would need to be moved to after the table lock
     * is released to avoid a rank order violation (all table locks have the
     * same rank). That's not problematic since it's only stat code.
     */
    /* If we no longer hold this high-level lock for adds+removes we need
     * to hold the new add/remove lock across persist_size->persist
     */
    ASSERT_OWN_MUTEX(true, (which == RCT_RAC ? &after_call_lock : &rct_module_lock));
    os_get_module_info_lock();
    permod = rct_get_table(tag, which);
    /* Xref case 9717, on a partial image mapping we may try to add locations
     * (specifically the entry point) that our outside of any module. Technically this
     * is also possible on a full mapping since we've seen entry points redirected
     * (and there's nothing requiring that they be re-directed to another dll or, if
     * at dr init, that we've already processed that target module, xref case 10693. */
    ASSERT_CURIOSITY(permod != NULL || EXEMPT_TEST("win32.partial_map.exe"));
    if (permod == NULL || rct_table_lookup_internal(dcontext, tag, permod) != NULL) {
        os_get_module_info_unlock();
        return false;
    }
    if (permod->live_table == NULL) {
        /* lazily initialized */
        if (rct_is_global_table(permod))
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        permod->live_table =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, app_pc_table_t, ACCT_AFTER_CALL, PROTECTED);
        if (rct_is_global_table(permod)) {
            /* For global tables we would have to move to heap, or
             * else unprot every time, to maintain min and max: but
             * the min-max optimization isn't going to help global
             * tables so we just don't bother.
             */
            permod->live_min = NULL;
            permod->live_max = (app_pc)POINTER_MAX;
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        }
        hashtable_app_pc_init(
            GLOBAL_DCONTEXT, permod->live_table,
            which == RCT_RAC ? INIT_HTABLE_SIZE_AFTER_CALL : INIT_HTABLE_SIZE_RCT_IBT,
            which == RCT_RAC ? DYNAMO_OPTION(shared_after_call_load)
                             : DYNAMO_OPTION(global_rct_ind_br_load),
            (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
            (SHARED_FRAGMENTS_ENABLED() ? HASHTABLE_ENTRY_SHARED : 0) | HASHTABLE_SHARED |
                HASHTABLE_PERSISTENT
                /* I'm seeing a number of high-ave-collision
                 * cases on both rac and rct; there's no easy
                 * way to estimate final size, so going to
                 * relax a little as not perf-critical */
                | HASHTABLE_RELAX_CLUSTER_CHECKS _IF_DEBUG(
                      which == RCT_RAC ? "after_call_targets" : "rct_ind_targets"));
        STATS_RCT_ADD(which, live_tables, 1);
    }
    ASSERT(permod->live_table != NULL);
    /* adding is a write operation */
    TABLE_RWLOCK(permod->live_table, write, lock);
    hashtable_app_pc_add(dcontext, tag, permod->live_table);
    TABLE_RWLOCK(permod->live_table, write, unlock);
    /* case 7628: used for persistence optimization: but watch overhead */
    if (!rct_is_global_table(permod)) {
        /* See comments above */
        if (permod->live_min == NULL || tag < permod->live_min)
            permod->live_min = tag;
        if (tag > permod->live_max)
            permod->live_max = tag;
    }
    os_get_module_info_unlock();
    STATS_RCT_ADD(which, live_entries, 1);
    DOSTATS({
        if (permod == &rac_non_module_table)
            STATS_INC(rac_non_module_entries);
    });
    return true;
}

static void
rct_table_flush_entry(dcontext_t *dcontext, app_pc tag, rct_type_t which)
{
    rct_module_table_t *permod;
    /* need higher level lock to properly synchronize with lookup+add */
    ASSERT_OWN_MUTEX(true, (which == RCT_RAC ? &after_call_lock : &rct_module_lock));
    os_get_module_info_lock();
    permod = rct_get_table(tag, which);
    ASSERT(permod != NULL);
    /* We should have removed any persist info before calling this routine */
    ASSERT(permod->persisted_table == NULL);
    ASSERT(permod->live_table != NULL);
    if (permod->live_table != NULL) {
        /* removing is a write operation */
        TABLE_RWLOCK(permod->live_table, write, lock);
        hashtable_app_pc_remove(tag, permod->live_table);
        TABLE_RWLOCK(permod->live_table, write, unlock);
    }
    os_get_module_info_unlock();
}

/* Invalidates all after call or indirect branch targets from given
 * range [text_start,text_end) which must be either completely
 * contained in a single module or not touch any modules.
 * Assuming any existing fragments that were added to IBL tables will
 * be flushed independently: this routine only flushes the policy
 * information.
 * Note this needs to be called on app_memory_deallocation() for RAC
 * (potentially from DGC), and rct_process_module_mmap() for other RCT
 * entries which should be only in modules.
 *
 * Returns entries flushed.
 */
static uint
rct_table_invalidate_range(dcontext_t *dcontext, rct_type_t which, app_pc text_start,
                           app_pc text_end)
{
    uint entries_removed = 0;
    rct_module_table_t *permod;
    /* need higher level lock to properly synchronize with lookup+add */
    ASSERT_OWN_MUTEX(true, (which == RCT_RAC ? &after_call_lock : &rct_module_lock));
    ASSERT(text_start < text_end);

    if (DYNAMO_OPTION(rct_sticky)) {
        /* case 5329 - leaving for bug-compatibility with previous releases */
        /* trade-off is spurious RCT violations vs memory leak */
        return 0;
    }

    /* We only support removing from within a single module or not touching
     * any modules */
    ASSERT(get_module_base(text_start) == get_module_base(text_end));

    os_get_module_info_lock();
    permod = rct_get_table(text_start, which);
    ASSERT(permod != NULL);
    /* We should have removed any persist info before calling this routine */
    ASSERT(permod->persisted_table == NULL);
    ASSERT(permod->live_table != NULL);
    if (permod != NULL && permod->live_table != NULL) {
        TABLE_RWLOCK(permod->live_table, write, lock);
        entries_removed = hashtable_app_pc_range_remove(dcontext, permod->live_table,
                                                        (ptr_uint_t)text_start,
                                                        (ptr_uint_t)text_end, NULL);

        DOCHECK(1, {
            uint second_pass = hashtable_app_pc_range_remove(dcontext, permod->live_table,
                                                             (ptr_uint_t)text_start,
                                                             (ptr_uint_t)text_end, NULL);
            ASSERT(second_pass == 0 && "nothing should be missed");
            /* simplest sanity check that hashtable_app_pc_range_remove() works */
        });
        TABLE_RWLOCK(permod->live_table, write, unlock);
    }
    os_get_module_info_unlock();
    return entries_removed;
}

static void
rct_table_free_internal(dcontext_t *dcontext, app_pc_table_t *table)
{
    hashtable_app_pc_free(dcontext, table);
    ASSERT(TEST(HASHTABLE_PERSISTENT, table->table_flags));
    HEAP_TYPE_FREE(dcontext, table, app_pc_table_t, ACCT_AFTER_CALL, PROTECTED);
}

void
rct_table_free(dcontext_t *dcontext, app_pc_table_t *table, bool free_data)
{
    DODEBUG({
        DOLOG(1, LOG_FRAGMENT | LOG_STATS,
              { hashtable_app_pc_load_statistics(dcontext, table); });
        hashtable_app_pc_study(dcontext, table, 0 /*table consistent*/);
    });
    if (!free_data) {
        /* We don't need the free_data param anymore */
        ASSERT(table->table_unaligned == NULL); /* don't try to free, part of mmap */
    }
    rct_table_free_internal(GLOBAL_DCONTEXT, table);
}

app_pc_table_t *
rct_table_copy(dcontext_t *dcontext, app_pc_table_t *src)
{
    if (src == NULL)
        return NULL;
    else
        return hashtable_app_pc_copy(GLOBAL_DCONTEXT, src);
}

app_pc_table_t *
rct_table_merge(dcontext_t *dcontext, app_pc_table_t *src1, app_pc_table_t *src2)
{
    if (src1 == NULL) {
        if (src2 == NULL)
            return NULL;
        return hashtable_app_pc_copy(GLOBAL_DCONTEXT, src2);
    } else if (src2 == NULL)
        return hashtable_app_pc_copy(GLOBAL_DCONTEXT, src1);
    else
        return hashtable_app_pc_merge(GLOBAL_DCONTEXT, src1, src2);
}

/* Up to caller to synchronize access to table. */
uint
rct_table_persist_size(dcontext_t *dcontext, app_pc_table_t *table)
{
    /* Don't persist zero-entry tables */
    if (table == NULL || table->entries == 0)
        return 0;
    else
        return hashtable_app_pc_persist_size(dcontext, table);
}

/* Up to caller to synchronize access to table.
 * Returns true iff all writes succeeded.
 */
bool
rct_table_persist(dcontext_t *dcontext, app_pc_table_t *table, file_t fd)
{
    bool success = true;
    ASSERT(fd != INVALID_FILE);
    ASSERT(table != NULL); /* caller shouldn't call us o/w */
    if (table != NULL)
        success = hashtable_app_pc_persist(dcontext, table, fd);
    return success;
}

app_pc_table_t *
rct_table_resurrect(dcontext_t *dcontext, byte *mapped_table, rct_type_t which)
{
    return hashtable_app_pc_resurrect(GLOBAL_DCONTEXT,
                                      mapped_table _IF_DEBUG(which == RCT_RAC
                                                                 ? "after_call_targets"
                                                                 : "rct_ind_targets"));
}

void
rct_module_table_free(dcontext_t *dcontext, rct_module_table_t *permod, app_pc modpc)
{
    ASSERT(os_get_module_info_locked());
    if (permod->live_table != NULL) {
        rct_table_free(GLOBAL_DCONTEXT, permod->live_table, true);
        permod->live_table = NULL;
    }
    if (permod->persisted_table != NULL) {
        /* persisted table: table data is from disk, but header is on heap */
        rct_table_free(GLOBAL_DCONTEXT, permod->persisted_table, false);
        permod->persisted_table = NULL;
        /* coarse_info_t has a duplicated pointer to the persisted table,
         * but it should always be flushed before we get here
         */
        ASSERT(get_executable_area_coarse_info(modpc) == NULL);
    }
}

void
rct_module_table_persisted_invalidate(dcontext_t *dcontext, app_pc modpc)
{
    rct_module_table_t *permod;
    uint i;
    os_get_module_info_lock();
    for (i = 0; i < RCT_NUM_TYPES; i++) {
        permod = rct_get_table(modpc, i);
        ASSERT(permod != NULL);
        if (permod != NULL && permod->persisted_table != NULL) {
            /* If the persisted table contains entries beyond what we will discover
             * when we re-build its cache we must transfer those to the live table
             * now.  At first I was only keeping entire-module RCT entries, but we
             * must keep all RAC and RCT entries since we may not see the triggering
             * code before we hit the check point (may reset at a ret so we won't see
             * the call before the ret check; same for Borland SEH).  We can only not
             * keep them on a module unload.
             */
            /* Optimization: don't transfer if about to unload.  This assumes
             * there will be no persistence of a later coarse unit in a different
             * region of the same module, which case 9651 primary_for_module
             * currently ensures!  (We already have read lock; ok to grab again.)
             */
            if (!os_module_get_flag(modpc, MODULE_BEING_UNLOADED) && !dynamo_exited) {
                /* FIXME case 10362: we could leave the file mapped in and
                 * use the persisted RCT table independently of the cache
                 */
                /* Merging will remove any dups, though today we never re-load
                 * pcaches so there shouldn't be any
                 */
                app_pc_table_t *merged =
                    /* all modinfo RCT tables are on global heap */
                    rct_table_merge(GLOBAL_DCONTEXT, permod->live_table,
                                    permod->persisted_table);
                if (permod->live_table != NULL)
                    rct_table_free(GLOBAL_DCONTEXT, permod->live_table, true);
                permod->live_table = merged;
                LOG(THREAD, LOG_FRAGMENT, 2,
                    "rct_module_table_persisted_invalidate " PFX ": not unload, so "
                    "moving persisted %d entries to live table\n",
                    modpc, permod->persisted_table->entries);
#    ifdef WINDOWS
                /* We leave the MODULE_RCT_LOADED flag */
#    endif
                STATS_INC(rct_persisted_outlast_cache);
            }
            /* we rely on coarse_unit_reset_free() freeing the persisted table struct */
            permod->persisted_table = NULL;
        }
    }
    os_get_module_info_unlock();
}

/* Produces a new hashtable that contains all entries in the live and persisted
 * tables for the module containing modpc that are within [limit_start, limit_end)
 */
app_pc_table_t *
rct_module_table_copy(dcontext_t *dcontext, app_pc modpc, rct_type_t which,
                      app_pc limit_start, app_pc limit_end)
{
    app_pc_table_t *merged = NULL;
    rct_module_table_t *permod;
    mutex_t *lock = (which == RCT_RAC) ? &after_call_lock : &rct_module_lock;
    d_r_mutex_lock(lock);
    if (which == RCT_RAC) {
        ASSERT(DYNAMO_OPTION(ret_after_call));
        if (!DYNAMO_OPTION(ret_after_call))
            return NULL;
    } else {
        ASSERT(TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
               TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump)));
        if (!TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) &&
            !TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump)))
            return NULL;
    }
    os_get_module_info_lock();
    permod = rct_get_table(modpc, which);
    ASSERT(permod != NULL);
    if (permod != NULL) {
        /* FIXME: we could pass the limit range down to hashtable_app_pc_{copy,merge}
         * for more efficiency and to avoid over-sizing the table, but this
         * should be rare w/ -persist_rct_entire and single-+x-section modules
         */
        merged = rct_table_merge(dcontext, permod->live_table, permod->persisted_table);
        if (merged != NULL) {
            DEBUG_DECLARE(uint removed = 0;)
            TABLE_RWLOCK(merged, write, lock);
            if (limit_start > permod->live_min) {
                DEBUG_DECLARE(removed +=)
                hashtable_app_pc_range_remove(dcontext, merged,
                                              (ptr_uint_t)permod->live_min,
                                              (ptr_uint_t)limit_start, NULL);
            }
            if (limit_end <= permod->live_max) {
                DEBUG_DECLARE(removed +=)
                hashtable_app_pc_range_remove(dcontext, merged, (ptr_uint_t)limit_end,
                                              (ptr_uint_t)permod->live_max + 1, NULL);
            }
            TABLE_RWLOCK(merged, write, unlock);
            STATS_RCT_ADD(which, module_persist_out_of_range, removed);
        }
    }
    os_get_module_info_unlock();
    d_r_mutex_unlock(lock);
    return merged;
}

/* We return the persisted table so we can keep a pointer to it in the
 * loaded coarse_info_t, but we must be careful to do a coordinated
 * free of the duplicated pointer.
 */
bool
rct_module_table_set(dcontext_t *dcontext, app_pc modpc, app_pc_table_t *table,
                     rct_type_t which)
{
    rct_module_table_t *permod;
    bool used = false;
    mutex_t *lock = (which == RCT_RAC) ? &after_call_lock : &rct_module_lock;
    d_r_mutex_lock(lock);
    os_get_module_info_lock();
    permod = rct_get_table(modpc, which);
    ASSERT(permod != NULL);
    ASSERT(permod->persisted_table == NULL); /* can't resurrect twice */
    ASSERT(table != NULL);
    /* Case 9834: avoid double-add from earlier entire-module resurrect */
    ASSERT(which == RCT_RAC || !os_module_get_flag(modpc, MODULE_RCT_LOADED));
    /* FIXME case 8648: we're loosening security by allowing ret to target any
     * after-call executed in any prior run of this app or whatever
     * app is producing, instead of just this run.
     */
    if (permod != NULL && permod->persisted_table == NULL) {
        used = true;
        /* There could be dups in persisted table that are already in
         * live table (particularly from unloading and re-loading a pcache,
         * though that never happens today).  Everything should work fine,
         * and if we do unload the pcache prior to module unload the merge
         * into the live entries will remove the dups.
         */
        permod->persisted_table = table;
        ASSERT(permod->persisted_table->entries > 0);
        /* FIXME: for case 9639 if we had the ibl table set up (say, for shared
         * ibl tables) and stored the cache pc (though I guess coarse htable is
         * set up) we could fill the ibl table here as well (but then we'd
         * have to abort for hotp conflicts earlier in coarse_unit_load()).
         * Instead we delay and do it in rac_persisted_fill_ibl().
         */
        LOG(THREAD, LOG_FRAGMENT, 2, "rct_module_table_resurrect: added %d %s entries\n",
            permod->persisted_table->entries, which == RCT_RAC ? "RAC" : "RCT");
        STATS_RCT_ADD(which, persisted_tables, 1);
        STATS_RCT_ADD(which, persisted_entries, permod->persisted_table->entries);
    }
    os_get_module_info_unlock();
    d_r_mutex_unlock(lock);
    return used;
}

bool
rct_module_persisted_table_exists(dcontext_t *dcontext, app_pc modpc, rct_type_t which)
{
    bool exists = false;
    rct_module_table_t *permod;
    os_get_module_info_lock();
    permod = rct_get_table(modpc, which);
    exists = (permod != NULL && permod->persisted_table != NULL);
    os_get_module_info_unlock();
    return exists;
}

uint
rct_module_live_entries(dcontext_t *dcontext, app_pc modpc, rct_type_t which)
{
    uint entries = 0;
    rct_module_table_t *permod;
    os_get_module_info_lock();
    permod = rct_get_table(modpc, which);
    if (permod != NULL && permod->live_table != NULL)
        entries = permod->live_table->entries;
    os_get_module_info_unlock();
    return entries;
}

static void
coarse_persisted_fill_ibl_helper(dcontext_t *dcontext, ibl_table_t *ibl_table,
                                 coarse_info_t *info, app_pc_table_t *ptable,
                                 ibl_branch_type_t branch_type)
{
    uint i;
    fragment_t wrapper;
    app_pc tag;
    cache_pc body_pc;
    DEBUG_DECLARE(uint added = 0;)
    ASSERT(ptable != NULL);
    if (ptable == NULL)
        return;
    ASSERT(os_get_module_info_locked());

    /* Make sure this thread's local ptrs & state are up to
     * date in case a resize occurred while it was in the cache. */
    update_private_ibt_table_ptrs(dcontext, ibl_table _IF_DEBUG(NULL));

    /* Avoid hash collision asserts while adding by sizing up front;
     * FIXME: we may over-size for INDJMP table
     */
    TABLE_RWLOCK(ibl_table, write, lock);
    hashtable_ibl_check_size(dcontext, ibl_table, 0, ptable->entries);
    TABLE_RWLOCK(ibl_table, write, unlock);

    /* FIXME: we should hold ptable's read lock but it's lower ranked
     * than the fragment table's lock, so we rely on os module lock
     */
    for (i = 0; i < ptable->capacity; i++) {
        tag = ptable->table[i];
        if (APP_PC_ENTRY_IS_REAL(tag)) {
            /* FIXME: should we persist the cache pcs to save time here?  That won't
             * be ideal if we ever use the mmapped table directly (for per-module
             * tables: case 9672).  We could support both tag-only and tag-cache
             * pairs by having the 1st entry be a flags word.
             */
            fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL, &body_pc);
            /* may not be present, given no checks in rct_entries_in_region() */
            if (body_pc != NULL &&
                /* We can have same entry in both RAC and RCT table, and we use
                 * both tables to fill the INDJMP table
                 */
                (branch_type != IBL_INDJMP ||
                 !IBL_ENTRY_IS_EMPTY(
                     hashtable_ibl_lookup(dcontext, (ptr_uint_t)tag, ibl_table)))) {
                fragment_coarse_wrapper(&wrapper, tag, body_pc);
                fragment_add_ibl_target_helper(dcontext, &wrapper, ibl_table);
                DOSTATS({ added++; });
            }
        }
    }
    LOG(THREAD, LOG_FRAGMENT, 2, "coarse_persisted_fill_ibl %s: added %d of %d entries\n",
        get_branch_type_name(branch_type), added, ptable->entries);
    STATS_ADD(perscache_ibl_prefill, added);
}

/* Case 9639: fill ibl table from persisted RAC/RCT table entries */
static void
coarse_persisted_fill_ibl(dcontext_t *dcontext, coarse_info_t *info,
                          ibl_branch_type_t branch_type)
{
    per_thread_t *pt = GET_PT(dcontext);
    ibl_table_t *ibl_table;
    rct_module_table_t *permod;

    /* Caller must hold info lock */
    ASSERT_OWN_MUTEX(true, &info->lock);
    ASSERT(exists_coarse_ibl_pending_table(dcontext, info, branch_type));
    ASSERT(TEST(COARSE_FILL_IBL_MASK(branch_type), DYNAMO_OPTION(coarse_fill_ibl)));
    if (!exists_coarse_ibl_pending_table(dcontext, info, branch_type))
        return;

    os_get_module_info_lock();
    ibl_table = GET_IBT_TABLE(pt, FRAG_SHARED | FRAG_COARSE_GRAIN, branch_type);
    if (branch_type == IBL_RETURN || branch_type == IBL_INDJMP) {
        permod = rct_get_table(info->base_pc, RCT_RAC);
        ASSERT(permod != NULL && permod->persisted_table != NULL);
        if (permod != NULL && permod->persisted_table != NULL) {
            LOG(THREAD, LOG_FRAGMENT, 2,
                "coarse_persisted_fill_ibl %s: adding RAC %d entries\n",
                get_branch_type_name(branch_type), permod->persisted_table->entries);
            coarse_persisted_fill_ibl_helper(dcontext, ibl_table, info,
                                             permod->persisted_table, branch_type);
        }
    }
    if (branch_type == IBL_INDCALL || branch_type == IBL_INDJMP) {
        permod = rct_get_table(info->base_pc, RCT_RCT);
        ASSERT(permod != NULL && permod->persisted_table != NULL);
        if (permod != NULL && permod->persisted_table != NULL) {
            LOG(THREAD, LOG_FRAGMENT, 2,
                "coarse_persisted_fill_ibl %s: adding RCT %d entries\n",
                get_branch_type_name(branch_type), permod->persisted_table->entries);
            coarse_persisted_fill_ibl_helper(dcontext, ibl_table, info,
                                             permod->persisted_table, branch_type);
        }
    }
    os_get_module_info_unlock();
    /* We only fill for the 1st thread (if using per-thread ibl
     * tables).  We'd need per-thread flags to do otherwise, and the
     * goal is only to help startup performance.
     * FIXME case 9639: later threads may do startup work in various apps,
     * and we may want a better solution here.
     */
    info->ibl_pending_used |= COARSE_FILL_IBL_MASK(branch_type);
}

#endif /* defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH) */

#ifdef RETURN_AFTER_CALL

/* returns NULL if not found */
app_pc
fragment_after_call_lookup(dcontext_t *dcontext, app_pc tag)
{
    return rct_table_lookup(dcontext, tag, RCT_RAC);
}

void
fragment_add_after_call(dcontext_t *dcontext, app_pc tag)
{
    d_r_mutex_lock(&after_call_lock);
    if (!rct_table_add(dcontext, tag, RCT_RAC))
        STATS_INC(num_existing_after_call);
    else
        STATS_INC(num_future_after_call);
    d_r_mutex_unlock(&after_call_lock);
}

/* flushing a fragment invalidates the after call entry */
void
fragment_flush_after_call(dcontext_t *dcontext, app_pc tag)
{
    d_r_mutex_lock(&after_call_lock);
    rct_table_flush_entry(dcontext, tag, RCT_RAC);
    d_r_mutex_unlock(&after_call_lock);
    STATS_INC(num_future_after_call_removed);
    STATS_DEC(num_future_after_call);
}

/* see comments in rct_table_invalidate_range() */
uint
invalidate_after_call_target_range(dcontext_t *dcontext, app_pc text_start,
                                   app_pc text_end)
{
    uint entries_removed;
    d_r_mutex_lock(&after_call_lock);
    entries_removed = rct_table_invalidate_range(dcontext, RCT_RAC, text_start, text_end);
    d_r_mutex_unlock(&after_call_lock);

    STATS_ADD(num_future_after_call_removed, entries_removed);
    STATS_SUB(num_future_after_call, entries_removed);

    LOG(THREAD, LOG_FRAGMENT, 2,
        "invalidate_rct_target_range " PFX "-" PFX ": removed %d entries\n", text_start,
        text_end, entries_removed);

    return entries_removed;
}

#endif /* RETURN_AFTER_CALL */

/***********************************************************************/
#ifdef RCT_IND_BRANCH
/*
 * RCT indirect branch policy bookkeeping.  Mostly a set of wrappers
 * around the basic hashtable functionality.
 *
 * FIXME: all of these routines should be moved to rct.c after we move
 * the hashtable primitives to fragment.h as static inline's
 */

/* returns NULL if not found */
app_pc
rct_ind_branch_target_lookup(dcontext_t *dcontext, app_pc tag)
{
    return rct_table_lookup(dcontext, tag, RCT_RCT);
}

/* returns true if a new entry for target was added,
 * or false if target was already known
 */
/* Note - entries are expected to be within MEM_IMAGE */
bool
rct_add_valid_ind_branch_target(dcontext_t *dcontext, app_pc tag)
{
    ASSERT_OWN_MUTEX(true, &rct_module_lock);
    DOLOG(2, LOG_FRAGMENT,
          {
              /* FIXME: would be nice to add a heavy weight check that we're
               * really only a PE IMAGE via is_in_code_section()
               */
          });
    if (!rct_table_add(dcontext, tag, RCT_RCT))
        return false;
    else {
        STATS_INC(rct_ind_branch_entries);
        return true; /* new entry */
    }
}

/* invalidate an indirect branch target and free any associated memory */
/* FIXME: note that this is currently not used and
 * invalidate_ind_branch_target_range() will be the likely method to
 * use for most cases when a whole module range is invalidated.
 */
void
rct_flush_ind_branch_target_entry(dcontext_t *dcontext, app_pc tag)
{
    ASSERT_OWN_MUTEX(true, &rct_module_lock); /* synch with adding */
    rct_table_flush_entry(dcontext, tag, RCT_RCT);
    STATS_DEC(rct_ind_branch_entries);
    STATS_INC(rct_ind_branch_entries_removed);
}

/* see comments in rct_table_invalidate_range() */
uint
invalidate_ind_branch_target_range(dcontext_t *dcontext, app_pc text_start,
                                   app_pc text_end)
{
    uint entries_removed;
    ASSERT_OWN_MUTEX(true, &rct_module_lock); /* synch with adding */

    entries_removed = rct_table_invalidate_range(dcontext, RCT_RCT, text_start, text_end);
    STATS_ADD(rct_ind_branch_entries_removed, entries_removed);
    STATS_SUB(rct_ind_branch_entries, entries_removed);

    return entries_removed;
}
#endif /* RCT_IND_BRANCH */

/****************************************************************************/
/* CACHE CONSISTENCY */

/* Handle exits from the cache from our self-modifying code sandboxing
 * instrumentation.
 */
void
fragment_self_write(dcontext_t *dcontext)
{
    ASSERT(!is_self_couldbelinking());
    /* need to delete just this fragment, then start interpreting
     * at the instr after the self-write instruction
     */
    dcontext->next_tag =
        EXIT_TARGET_TAG(dcontext, dcontext->last_fragment, dcontext->last_exit);
    LOG(THREAD, LOG_ALL, 2, "Sandboxing exit from fragment " PFX " @" PFX "\n",
        dcontext->last_fragment->tag,
        EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit));
    LOG(THREAD, LOG_ALL, 2, "\tset next_tag to " PFX "\n", dcontext->next_tag);
    /* We come in here both for actual selfmod and for exec count thresholds,
     * to avoid needing separate LINK_ flags.
     */
    if (DYNAMO_OPTION(sandbox2ro_threshold) > 0) {
        if (vm_area_selfmod_check_clear_exec_count(dcontext, dcontext->last_fragment)) {
            /* vm_area_* deleted this fragment by flushing so nothing more to do */
            return;
        }
    }
    LOG(THREAD, LOG_ALL, 1, "WARNING: fragment " PFX " @" PFX " overwrote its own code\n",
        dcontext->last_fragment->tag,
        EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit));
    STATS_INC(num_self_writes);
    if (TEST(FRAG_WAS_DELETED, dcontext->last_fragment->flags)) {
        /* Case 8177: case 3559 unionized fragment_t.in_xlate, so we cannot delete a
         * fragment that has already been unlinked in the first stage of a flush.
         * The flush queue check, which comes after this (b/c we want to be
         * nolinking), will delete.
         */
        ASSERT(((per_thread_t *)dcontext->fragment_field)->flush_queue_nonempty);
        STATS_INC(num_self_writes_after_flushes);
    } else {
#ifdef PROGRAM_SHEPHERDING
        /* we can't call fragment_delete if vm_area_t deletes it by flushing */
        if (!vm_area_fragment_self_write(dcontext, dcontext->last_fragment->tag)) {
#endif
            fragment_delete(dcontext, dcontext->last_fragment, FRAGDEL_ALL);
            STATS_INC(num_fragments_deleted_selfmod);
#ifdef PROGRAM_SHEPHERDING
        }
#endif
    }
}

/* coarse_grain says to use just the min_pc and max_pc of f.
 * otherwise a full walk of the original code is done to see if
 * any piece of the fragment really does overlap.
 * returns the tag of the bb that actually overlaps (i.e., finds the
 * component bb of a trace that does the overlapping).
 */
bool
fragment_overlaps(dcontext_t *dcontext, fragment_t *f, byte *region_start,
                  byte *region_end, bool coarse_grain, overlap_info_t *info_res,
                  app_pc *bb_tag)
{
    overlap_info_t info;
    info.overlap = false;
    if ((f->flags & FRAG_IS_TRACE) != 0) {
        uint i;
        trace_only_t *t = TRACE_FIELDS(f);
        /* look through all blocks making up the trace */
        ASSERT(t->bbs != NULL);
        /* trace should have at least one bb */
        ASSERT(t->num_bbs > 0);
        for (i = 0; i < t->num_bbs; i++) {
            if (app_bb_overlaps(dcontext, t->bbs[i].tag, f->flags, region_start,
                                region_end, &info)) {
                if (bb_tag != NULL)
                    *bb_tag = t->bbs[i].tag;
                break;
            }
        }
    } else {
        app_bb_overlaps(dcontext, f->tag, f->flags, region_start, region_end, &info);
        if (info.overlap && bb_tag != NULL)
            *bb_tag = f->tag;
    }
    if (info_res != NULL)
        *info_res = info;
    return info.overlap;
}

#ifdef DEBUG
void
study_all_hashtables(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    ibl_branch_type_t branch_type;

    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        if (!DYNAMO_OPTION(disable_traces)) {
            per_thread_t *ibl_pt = pt;
            if (DYNAMO_OPTION(shared_trace_ibt_tables))
                ibl_pt = shared_pt;
            hashtable_ibl_study(dcontext, &ibl_pt->trace_ibt[branch_type],
                                0 /*table consistent*/);
        }
        if (DYNAMO_OPTION(bb_ibl_targets)) {
            per_thread_t *ibl_pt = pt;
            if (DYNAMO_OPTION(shared_bb_ibt_tables))
                ibl_pt = shared_pt;
            hashtable_ibl_study(dcontext, &ibl_pt->bb_ibt[branch_type],
                                0 /*table consistent*/);
        }
    }
    if (PRIVATE_TRACES_ENABLED())
        hashtable_fragment_study(dcontext, &pt->trace, 0 /*table consistent*/);
    hashtable_fragment_study(dcontext, &pt->bb, 0 /*table consistent*/);
    hashtable_fragment_study(dcontext, &pt->future, 0 /*table consistent*/);
    if (DYNAMO_OPTION(shared_bbs))
        hashtable_fragment_study(dcontext, shared_bb, 0 /*table consistent*/);
    if (DYNAMO_OPTION(shared_traces))
        hashtable_fragment_study(dcontext, shared_trace, 0 /*table consistent*/);
    if (SHARED_FRAGMENTS_ENABLED())
        hashtable_fragment_study(dcontext, shared_future, 0 /*table consistent*/);
#    ifdef RETURN_AFTER_CALL
    if (dynamo_options.ret_after_call && rac_non_module_table.live_table != NULL) {
        hashtable_app_pc_study(dcontext, rac_non_module_table.live_table,
                               0 /*table consistent*/);
    }
#    endif
#    if defined(RCT_IND_BRANCH) && defined(UNIX)
    if ((TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
         TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) &&
        rct_global_table.live_table != NULL) {
        hashtable_app_pc_study(dcontext, rct_global_table.live_table,
                               0 /*table consistent*/);
    }
#    endif /* RCT_IND_BRANCH */
#    if defined(WIN32) && (defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH))
    {
        module_iterator_t *mi = module_iterator_start();
        uint i;
        rct_module_table_t *permod;
        while (module_iterator_hasnext(mi)) {
            module_area_t *data = module_iterator_next(mi);
            for (i = 0; i < RCT_NUM_TYPES; i++) {
                permod = os_module_get_rct_htable(data->start, i);
                ASSERT(permod != NULL);
                if (permod->persisted_table != NULL) {
                    LOG(THREAD, LOG_FRAGMENT, 2,
                        "%s persisted hashtable for %s " PFX "-" PFX "\n",
                        i == RCT_RAC ? "RAC" : "RCT", GET_MODULE_NAME(&data->names),
                        data->start, data->end);
                    hashtable_app_pc_study(dcontext, permod->persisted_table,
                                           0 /*table consistent*/);
                }
                if (permod->live_table != NULL) {
                    LOG(THREAD, LOG_FRAGMENT, 2,
                        "%s live hashtable for %s " PFX "-" PFX "\n",
                        i == RCT_RAC ? "RAC" : "RCT", GET_MODULE_NAME(&data->names),
                        data->start, data->end);
                    hashtable_app_pc_study(dcontext, permod->live_table,
                                           0 /*table consistent*/);
                }
            }
        }
        module_iterator_stop(mi);
    }
#    endif /* defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH) */
}
#endif /* DEBUG */

/****************************************************************************
 * FLUSHING
 *
 * Two-stage freeing of fragments via immediate unlinking followed by lazy
 * deletion.
 */

uint
get_flushtime_last_update(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    return pt->flushtime_last_update;
}

void
set_flushtime_last_update(dcontext_t *dcontext, uint val)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    pt->flushtime_last_update = val;
}

void
set_at_syscall(dcontext_t *dcontext, bool val)
{
    ASSERT(dcontext != GLOBAL_DCONTEXT);
    dcontext->upcontext_ptr->at_syscall = val;
}

bool
get_at_syscall(dcontext_t *dcontext)
{
    ASSERT(dcontext != GLOBAL_DCONTEXT);
    return dcontext->upcontext_ptr->at_syscall;
}

/* Assumes caller takes care of synchronization.
 * Returns false iff was_I_flushed ends up being deleted right now from
 * a private cache OR was_I_flushed has been flushed from a shared cache
 * and is pending final deletion.
 */
static bool
check_flush_queue(dcontext_t *dcontext, fragment_t *was_I_flushed)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    bool not_flushed = true;
    ASSERT_OWN_MUTEX(true, &pt->linking_lock);
    /* first check private queue and act on pending deletions */
    if (pt->flush_queue_nonempty) {
        bool local_prot = local_heap_protected(dcontext);
        if (local_prot)
            SELF_PROTECT_LOCAL(dcontext, WRITABLE);
        /* remove local vm areas on t->Q queue and all frags in their lists */
        not_flushed = not_flushed && vm_area_flush_fragments(dcontext, was_I_flushed);
        pt->flush_queue_nonempty = false;
        LOG(THREAD, LOG_FRAGMENT, 2, "Hashtable state after flushing the queue:\n");
        DOLOG(2, LOG_FRAGMENT, { study_all_hashtables(dcontext); });
        if (local_prot)
            SELF_PROTECT_LOCAL(dcontext, READONLY);
    }
    /* now check shared queue to dec ref counts */
    uint local_flushtime_global;
    /* No lock needed: any racy incs to global are in safe direction, and our inc
     * is atomic so we shouldn't see any partial-word-updated values here.  This
     * check is our shared deletion algorithm's only perf hit when there's no
     * actual shared flushing.
     */
    ATOMIC_4BYTE_ALIGNED_READ(&flushtime_global, &local_flushtime_global);
    if (DYNAMO_OPTION(shared_deletion) &&
        pt->flushtime_last_update < local_flushtime_global) {
#ifdef LINUX
        rseq_shared_fragment_flushtime_update(dcontext);
#endif
        /* dec ref count on any pending shared areas */
        not_flushed =
            not_flushed && vm_area_check_shared_pending(dcontext, was_I_flushed);
        /* Remove unlinked markers if called for.
         * FIXME If a thread's flushtime is updated due to shared syscall sync,
         * its tables won't be rehashed here -- the thread's flushtime will be
         * equal to the global flushtime so the 'if' isn't entered. We have
         * multiple options as to other points for rehashing -- a table add, a
         * table delete, any entry into DR. For now, we've chosen a table add
         * (see check_table_size()).
         */
        if (SHARED_IB_TARGETS() &&
            (INTERNAL_OPTION(rehash_unlinked_threshold) < 100 ||
             INTERNAL_OPTION(rehash_unlinked_always))) {

            ibl_branch_type_t branch_type;

            for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
                 branch_type++) {

                ibl_table_t *table = &pt->bb_ibt[branch_type];

                if (table->unlinked_entries > 0 &&
                    (INTERNAL_OPTION(rehash_unlinked_threshold) <
                         (100 * table->unlinked_entries /
                          (table->unlinked_entries + table->entries)) ||
                     INTERNAL_OPTION(rehash_unlinked_always))) {
                    STATS_INC(num_ibt_table_rehashes);
                    LOG(THREAD, LOG_FRAGMENT, 1,
                        "Rehash table %s: linked %u, unlinked %u\n", table->name,
                        table->entries, table->unlinked_entries);
                    hashtable_ibl_unlinked_remove(dcontext, table);
                }
            }
        }
    }

    /* FIXME This is the ideal location for inserting refcounting logic
     * for freeing a resized shared IBT table, as is done for shared
     * deletion above.
     */

    return not_flushed;
}

/* Note that an all-threads-synch flush does NOT set the self-flushing flag,
 * so use is_self_allsynch_flushing() instead.
 */
bool
is_self_flushing()
{
    /* race condition w/ flusher being updated -- but since only testing vs self,
     * if flusher update is atomic, should be safe
     */
    return (get_thread_private_dcontext() == flusher);
}

bool
is_self_allsynch_flushing()
{
    /* race condition w/ allsynch_flusher being updated -- but since only testing
     * vs self, if flusher update is atomic, should be safe
     */
    return (allsynch_flusher != NULL &&
            get_thread_private_dcontext() == allsynch_flusher);
}

/* N.B.: only accurate if called on self (else a race condition) */
bool
is_self_couldbelinking()
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* if no dcontext yet then can't be couldbelinking */
    return (dcontext != NULL && !RUNNING_WITHOUT_CODE_CACHE() /*case 7966: has no pt*/ &&
            is_couldbelinking(dcontext));
}

/* N.B.: can only call if target thread is self, suspended, or waiting for flush */
bool
is_couldbelinking(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    /* the lock is only needed for self writing or another thread reading
     * but if different thread we require thread is suspended or waiting for
     * flush so is ok */
    /* FIXME : add an assert that the thread that owns the dcontext is either
     * the caller, at the flush sync wait, or is suspended by thread_synch
     * routines */
    return (!RUNNING_WITHOUT_CODE_CACHE() /*case 7966: has no pt*/ &&
            pt != NULL /*PR 536058: no pt*/ && pt->could_be_linking);
}

static void
wait_for_flusher_nolinking(dcontext_t *dcontext)
{
    /* FIXME: can have livelock w/ these types of synch loops,
     * any way to work into deadlock-avoidance?
     */
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    ASSERT(!pt->could_be_linking);
    while (pt->wait_for_unlink) {
        LOG(THREAD, LOG_DISPATCH | LOG_THREADS, 2,
            "Thread " TIDFMT " waiting for flush (flusher is %d @flushtime %d)\n",
            /* safe to deref flusher since flusher is waiting for our signal */
            dcontext->owning_thread, flusher->owning_thread, flushtime_global);
        d_r_mutex_unlock(&pt->linking_lock);
        STATS_INC(num_wait_flush);
        wait_for_event(pt->finished_all_unlink, 0);
        LOG(THREAD, LOG_DISPATCH | LOG_THREADS, 2,
            "Thread " TIDFMT " resuming after flush\n", dcontext->owning_thread);
        d_r_mutex_lock(&pt->linking_lock);
    }
}

static void
wait_for_flusher_linking(dcontext_t *dcontext)
{
    /* FIXME: can have livelock w/ these types of synch loops,
     * any way to work into deadlock-avoidance?
     */
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    ASSERT(pt->could_be_linking);
    while (pt->wait_for_unlink) {
        LOG(THREAD, LOG_DISPATCH | LOG_THREADS, 2,
            "Thread " TIDFMT " waiting for flush (flusher is %d @flushtime %d)\n",
            /* safe to deref flusher since flusher is waiting for our signal */
            dcontext->owning_thread, flusher->owning_thread, flushtime_global);
        d_r_mutex_unlock(&pt->linking_lock);
        signal_event(pt->waiting_for_unlink);
        STATS_INC(num_wait_flush);
        wait_for_event(pt->finished_with_unlink, 0);
        LOG(THREAD, LOG_DISPATCH | LOG_THREADS, 2,
            "Thread " TIDFMT " resuming after flush\n", dcontext->owning_thread);
        d_r_mutex_lock(&pt->linking_lock);
    }
}

#ifdef DEBUG
static void
check_safe_for_flush_synch(dcontext_t *dcontext)
{
    /* We cannot hold any locks at synch points that wait for flushers, as we
     * could prevent forward progress of a couldbelinking thread that the
     * flusher will wait for.
     */
    /* FIXME: will fail w/ -single_thread_in_DR, along w/ the other similar
     * asserts for flushing and cache entering
     */
#    ifdef DEADLOCK_AVOIDANCE
    ASSERT(
        thread_owns_no_locks(dcontext) ||
        /* if thread exits while a trace is in progress (case 8055) we're
         * holding thread_initexit_lock, which prevents any flusher from
         * proceeding and hitting a deadlock point, so we should be safe
         */
        thread_owns_one_lock(dcontext, &thread_initexit_lock) ||
        /* it is safe to be the all-thread-syncher and hit a flush synch
         * point, as no flusher can be active (all threads should be suspended
         * except this thread)
         */
        thread_owns_two_locks(dcontext, &thread_initexit_lock, &all_threads_synch_lock));
#    endif /* DEADLOCK_AVOIDANCE */
}
#endif /* DEBUG */

static void
process_client_flush_requests(dcontext_t *dcontext, dcontext_t *alloc_dcontext,
                              client_flush_req_t *req, bool flush)
{
    client_flush_req_t *iter = req;
    while (iter != NULL) {
        client_flush_req_t *next = iter->next;
        if (flush) {
            /* Note that we don't free futures from potentially linked-to region b/c we
             * don't have lazy linking (xref case 2236) */
            /* FIXME - if there's more then one of these would be nice to batch them
             * especially for the synch all ones. */
            if (iter->flush_callback != NULL) {
                /* FIXME - for implementation simplicity we do a synch-all flush so
                 * that we can inform the client right away, it might be nice to use
                 * the more performant regular flush when possible. */
                flush_fragments_from_region(
                    dcontext, iter->start, iter->size, true /*force synchall*/,
                    NULL /*flush_completion_callback*/, NULL /*user_data*/);
                (*iter->flush_callback)(iter->flush_id);
            } else {
                /* do a regular flush */
                flush_fragments_from_region(
                    dcontext, iter->start, iter->size, false /*don't force synchall*/,
                    NULL /*flush_completion_callback*/, NULL /*user_data*/);
            }
        }
        HEAP_TYPE_FREE(alloc_dcontext, iter, client_flush_req_t, ACCT_CLIENT,
                       UNPROTECTED);
        iter = next;
    }
}

/* Returns false iff was_I_flushed ends up being deleted
 * if cache_transition is true, assumes entering the cache now.
 */
bool
enter_nolinking(dcontext_t *dcontext, fragment_t *was_I_flushed, bool cache_transition)
{

    /* Handle any pending low on memory events */
    vmm_heap_handle_pending_low_on_memory_event_trigger();

    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    bool not_flushed = true;

    /*case 7966: has no pt, no flushing either */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return true;

    DOCHECK(1, { check_safe_for_flush_synch(dcontext); });

    /* FIXME: once we have this working correctly, come up with scheme
     * that avoids synch in common case
     */
    d_r_mutex_lock(&pt->linking_lock);
    ASSERT(pt->could_be_linking);

    wait_for_flusher_linking(dcontext);
    not_flushed = not_flushed && check_flush_queue(dcontext, was_I_flushed);
    pt->could_be_linking = false;
    d_r_mutex_unlock(&pt->linking_lock);

    if (!cache_transition)
        return not_flushed;

    /* now we act on pending actions that can only be done while nolinking
     * FIXME: optimization if add more triggers here: use a single
     * main trigger as a first test to avoid testing all
     * conditionals every time
     */

    if (reset_pending != 0) {
        d_r_mutex_lock(&reset_pending_lock);
        if (reset_pending != 0) {
            uint target = reset_pending;
            reset_pending = 0;
            /* fcache_reset_all_caches_proactively() will unlock */
            fcache_reset_all_caches_proactively(target);
            LOG(THREAD, LOG_DISPATCH, 2, "Just reset all caches, next_tag is " PFX "\n",
                dcontext->next_tag);
            /* fragment is gone for sure, so return false */
            return false;
        }
        d_r_mutex_unlock(&reset_pending_lock);
    }

    /* FIXME: perf opt: make global flag can check w/ making a call,
     * or at least inline the call
     */
    if (fcache_is_flush_pending(dcontext)) {
        not_flushed = not_flushed && fcache_flush_pending_units(dcontext, was_I_flushed);
    }

#ifdef UNIX
    /* i#61/PR 211530: nudges on Linux do not use separate threads */
    while (dcontext->nudge_pending != NULL) {
        /* handle_nudge may not return, so we can't call it w/ inconsistent state */
        pending_nudge_t local = *dcontext->nudge_pending;
        heap_free(dcontext, dcontext->nudge_pending, sizeof(local) HEAPACCT(ACCT_OTHER));
        dcontext->nudge_pending = local.next;
        if (dcontext->interrupted_for_nudge != NULL) {
            fragment_t *f = dcontext->interrupted_for_nudge;
            LOG(THREAD, LOG_ASYNCH, 3, "\tre-linking outgoing for interrupted F%d\n",
                f->id);
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
            link_fragment_outgoing(dcontext, f, false);
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
            if (TEST(FRAG_HAS_SYSCALL, f->flags)) {
                mangle_syscall_code(dcontext, f, EXIT_CTI_PC(f, dcontext->last_exit),
                                    true /*skip exit cti*/);
            }
            dcontext->interrupted_for_nudge = NULL;
        }
        handle_nudge(dcontext, &local.arg);
        /* we may have done a reset, so do not enter cache now */
        return false;
    }
#endif

    /* Handle flush requests queued via dr_flush_fragments()/dr_delay_flush_region() */
    /* thread private list */
    process_client_flush_requests(dcontext, dcontext, dcontext->client_data->flush_list,
                                  true /*flush*/);
    dcontext->client_data->flush_list = NULL;
    /* global list */
    if (client_flush_requests != NULL) { /* avoid acquiring lock every cxt switch */
        client_flush_req_t *req;
        d_r_mutex_lock(&client_flush_request_lock);
        req = client_flush_requests;
        client_flush_requests = NULL;
        d_r_mutex_unlock(&client_flush_request_lock);
        /* NOTE - we must release the lock before doing the flush. */
        process_client_flush_requests(dcontext, GLOBAL_DCONTEXT, req, true /*flush*/);
        /* FIXME - this is an ugly, yet effective, hack.  The problem is there is no
         * good way to tell currently if we flushed was_I_flushed.  Since it could be
         * gone by now we pretend that it was flushed if we did any flushing at all.
         * Dispatch should refind the fragment if it wasn't flushed. */
        if (req != NULL)
            not_flushed = false;
    }

    return not_flushed;
}

/* Returns false iff was_I_flushed ends up being deleted */
bool
enter_couldbelinking(dcontext_t *dcontext, fragment_t *was_I_flushed,
                     bool cache_transition)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    bool not_flushed;

    /*case 7966: has no pt, no flushing either */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return true;
    ASSERT(pt != NULL); /* i#1989: API callers should check fragment_thread_exited() */

    DOCHECK(1, { check_safe_for_flush_synch(dcontext); });

    d_r_mutex_lock(&pt->linking_lock);
    ASSERT(!pt->could_be_linking);
    /* ensure not still marked at_syscall */
    ASSERT(!DYNAMO_OPTION(syscalls_synch_flush) || !get_at_syscall(dcontext) ||
           /* i#2026: this can happen during detach but there it's innocuous */
           doing_detach);

    /* for thread-shared flush and thread-private flush+execareas atomicity,
     * to avoid non-properly-nested locks (could have flusher hold
     * pt->linking_lock for the entire flush) we need an additional
     * synch point here for shared flushing to synch w/ all threads.
     * I suppose switching from non-nested locks to this loop isn't
     * nec. helping deadlock avoidance -- can still hang -- but this
     * should be better performance-wise (only a few writes and a
     * conditional in the common case here, no extra locks)
     */
    pt->soon_to_be_linking = true;
    wait_for_flusher_nolinking(dcontext);
    pt->soon_to_be_linking = false;

    pt->could_be_linking = true;
    not_flushed = check_flush_queue(dcontext, was_I_flushed);
    d_r_mutex_unlock(&pt->linking_lock);

    return not_flushed;
}

/* NOTE - this routine may be called more then one time for the same exiting thread,
 * xref case 8047.  Any once only reference counting, cleanup, etc. should be done in
 * fragment_thread_exit().  This routine is just a stripped down version of
 * enter_nolinking() to keep an exiting thread from deadlocking with flushing. */
void
enter_threadexit(dcontext_t *dcontext)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;

    /*case 7966: has no pt, no flushing either */
    if (RUNNING_WITHOUT_CODE_CACHE() || pt == NULL /*PR 536058: no pt*/)
        return;

    d_r_mutex_lock(&pt->linking_lock);
    /* must dec ref count on shared regions before we die */
    check_flush_queue(dcontext, NULL);
    pt->could_be_linking = false;
    if (pt->wait_for_unlink) {
        /* make sure don't get into deadlock w/ flusher */
        pt->about_to_exit = true;             /* let flusher know can ignore us */
        signal_event(pt->waiting_for_unlink); /* wake flusher up */
    }
    d_r_mutex_unlock(&pt->linking_lock);
}

/* caller must hold shared_cache_flush_lock */
void
increment_global_flushtime()
{
    ASSERT_OWN_MUTEX(true, &shared_cache_flush_lock);
    /* reset will turn flushtime_global back to 0, so we schedule one
     * when we're approaching overflow
     */
    uint local_flushtime_global;
    ATOMIC_4BYTE_ALIGNED_READ(&flushtime_global, &local_flushtime_global);
    if (local_flushtime_global == UINT_MAX / 2) {
        ASSERT_NOT_TESTED(); /* FIXME: add -stress_flushtime_global_max */
        SYSLOG_INTERNAL_WARNING("flushtime_global approaching UINT_MAX, resetting");
        schedule_reset(RESET_ALL);
    }
    ASSERT(local_flushtime_global < UINT_MAX);

    /* compiler should 4-byte-align so no cache line crossing
     * (asserted in fragment_init()
     */
    atomic_add_exchange_int((volatile int *)&flushtime_global, 1);
    LOG(GLOBAL, LOG_VMAREAS, 2, "new flush timestamp: %u\n", flushtime_global);
}

/* The main flusher routines are split into 3:
 *   stage1) flush_fragments_synch_unlink_priv
 *   stage2) flush_fragments_unlink_shared
 *   stage3) flush_fragments_end_synch
 * which MUST be used together.  They are split to allow the caller to
 * perform custom operations at certain synchronization points in the
 * middle of flushing without using callback routines.  The stages and
 * points are:
 *
 *   stage1: head synch, priv flushee unlink
 *     here caller can produce a custom list of fragments to flush while
 *       all threads are fully synched
 *   stage2: shared flushee unlink
 *
 *    between stage2 and stage3, a region flush performs additional actions:
 *     region flush: exec area lock
 *       here caller can do custom exec area manipulations
 *     region flush: exec area unlock
 *
 *   stage3: tail synch
 *
 * When doing a region flush with no custom region removals (the default is
 * removing [base,base+size)), use the routine flush_fragments_and_remove_region().
 * When doing a region flush with custom removals, use
 * flush_fragments_in_region_start() and flush_fragments_in_region_finish().
 * When doing a flush with no region removals at all use flush_fragments_from_region().
 *
 * The thread requesting a flush must be !couldbelinking and must not
 * be holding any locks that are ever grabbed while a thread is
 * couldbelinking (pretty much all locks), as it must wait for other
 * threads who are couldbelinking.  The thread_initexit_lock is held
 * from stage 1 to stage 3 (if there are fragments to flush), and
 * region flushing also holds the executable_areas lock between stage
 * 2 and stage 3.  After stage 1 no thread is couldbelinking() until
 * the synchronization release in stage 3.
 *
 * The general delayed-deletion flushing strategy involves first
 * freezing the threads via the thread_initexit_lock.
 * It then walks the thread list and marks all vm areas that overlap
 * base...base+size as to-be-deleted, along with unlinking all fragments in those
 * vm areas and unlinking shared_syscall for that thread.
 * Fragments in the target area are not actually deleted until their owning thread
 * checks its pending deletion queue, which it does prior to entering the fcache.
 * Also flushes shared fragments, in a similar manner, with deletion delayed
 * until all threads are out of the shared cache.
 */

/* Variables shared between the 3 flush stage routines
 * flush_fragments_synch_unlink_priv(),
 * flush_fragments_unlink_shared(), and flush_fragments_end_synch.
 * As there can only be one flush at a time, we only need one copy,
 * protected by the thread_initexit_lock.
 */
/* Not persistent across code cache execution, so unprotected */
DECLARE_NEVERPROT_VAR(static thread_record_t **flush_threads, NULL);
DECLARE_NEVERPROT_VAR(static int flush_num_threads, 0);
DECLARE_NEVERPROT_VAR(static int pending_delete_threads, 0);
DECLARE_NEVERPROT_VAR(static int shared_flushed, 0);
DECLARE_NEVERPROT_VAR(static bool flush_synchall, false);
#ifdef DEBUG
DECLARE_NEVERPROT_VAR(static int num_flushed, 0);
DECLARE_NEVERPROT_VAR(static int flush_last_stage, 0);
#endif

static void
flush_fragments_free_futures(app_pc base, size_t size)
{
    int i;
    dcontext_t *tgt_dcontext;
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT((allsynch_flusher == NULL && flusher == get_thread_private_dcontext()) ||
           (flusher == NULL && allsynch_flusher == get_thread_private_dcontext()));
    ASSERT(flush_num_threads > 0);
    ASSERT(flush_threads != NULL);
    if (DYNAMO_OPTION(free_unmapped_futures) && !RUNNING_WITHOUT_CODE_CACHE()) {
        /* We need to free the futures after all fragments have been unlinked,
         * as unlinking will create new futures
         */
        acquire_recursive_lock(&change_linking_lock);
        for (i = 0; i < flush_num_threads; i++) {
            tgt_dcontext = flush_threads[i]->dcontext;
            if (tgt_dcontext != NULL) {
                fragment_delete_futures_in_region(tgt_dcontext, base, base + size);
                thcounter_range_remove(tgt_dcontext, base, base + size);
            }
        }
        if (SHARED_FRAGMENTS_ENABLED())
            fragment_delete_futures_in_region(GLOBAL_DCONTEXT, base, base + size);
        release_recursive_lock(&change_linking_lock);
    } /* else we leak them */
}

/* This routine begins a flush that requires full thread synch: currently,
 * it is used for flushing coarse-grain units and for dr_flush_region()
 */
static void
flush_fragments_synchall_start(dcontext_t *ignored, app_pc base, size_t size,
                               bool exec_invalid)
{
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    app_pc exec_start = NULL, exec_end = NULL;
    bool all_synched = true;
    int i;
    const thread_synch_state_t desired_state =
        THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER;
    DEBUG_DECLARE(bool ok;)
    KSTART(synchall_flush);
    LOG(GLOBAL, LOG_FRAGMENT, 2,
        "\nflush_fragments_synchall_start: thread " TIDFMT " suspending all threads\n",
        d_r_get_thread_id());

    STATS_INC(flush_synchall);
    /* suspend all DR-controlled threads at safe locations */
    DEBUG_DECLARE(ok =)
    synch_with_all_threads(desired_state, &flush_threads, &flush_num_threads,
                           THREAD_SYNCH_NO_LOCKS_NO_XFER,
                           /* if we fail to suspend a thread (e.g., for
                            * privilege reasons), ignore it since presumably
                            * the failed thread is some injected thread not
                            * running to-be-flushed code, so we continue
                            * with the flush!
                            */
                           THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
    ASSERT(ok);
    /* now we own the thread_initexit_lock */
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));

    /* We do NOT set flusher as is_self_flushing() is all about
     * couldbelinking, while our synch is of a different nature.  The
     * trace_abort() is_self_flushing() check is the only worrisome
     * one: FIXME.
     */
    ASSERT(flusher == NULL);
    ASSERT(allsynch_flusher == NULL);
    allsynch_flusher = my_dcontext;
    flush_synchall = true;
    ASSERT(flush_last_stage == 0);
    DODEBUG({ flush_last_stage = 1; });

    LOG(GLOBAL, LOG_FRAGMENT, 2, "flush_fragments_synchall_start: walking the threads\n");
    /* We rely on coarse fragments not touching more than one vmarea region
     * for our ibl invalidation.  It's
     * ok to invalidate more than we need to so we don't care if there are
     * multiple coarse units within this range.  We just need the exec areas
     * bounds that overlap the flush region.
     */
    if (!executable_area_overlap_bounds(base, base + size, &exec_start, &exec_end, 0,
                                        true /*doesn't matter w/ 0*/)) {
        /* caller checks for overlap but lock let go so can get here; go ahead
         * and do synch per flushing contract.
         */
        exec_start = base;
        exec_end = base + size;
    }
    LOG(GLOBAL, LOG_FRAGMENT, 2,
        "flush_fragments_synchall_start: from " PFX "-" PFX " => coarse " PFX "-" PFX
        "\n",
        base, base + size, exec_start, exec_end);

    /* FIXME: share some of this code that I duplicated from reset */
    for (i = 0; i < flush_num_threads; i++) {
        dcontext_t *dcontext = flush_threads[i]->dcontext;
        if (dcontext != NULL) { /* include my_dcontext here */
            DEBUG_DECLARE(uint removed;)
            LOG(GLOBAL, LOG_FRAGMENT, 2, "\tconsidering thread #%d " TIDFMT "\n", i,
                flush_threads[i]->id);
            if (dcontext != my_dcontext) {
                /* must translate BEFORE freeing any memory! */
                if (!thread_synch_successful(flush_threads[i])) {
                    /* FIXME case 9480: if we do get here for low-privilege handles
                     * or exceeding our synch try count, best to not move the thread
                     * as we won't get a clean translation.  Chances are it is
                     * not in the region being flushed.
                     */
                    SYSLOG_INTERNAL_ERROR_ONCE("failed to synch with thread during "
                                               "synchall flush");
                    LOG(THREAD, LOG_FRAGMENT | LOG_SYNCH, 2,
                        "failed to synch with thread #%d\n", i);
                    STATS_INC(flush_synchall_fail);
                    all_synched = false;
                } else if (is_thread_currently_native(flush_threads[i])) {
                    /* native_exec's regain-control point is in our DLL,
                     * and lost-control threads are truly native, so no
                     * state to worry about except for hooks -- and we're
                     * not freeing the interception buffer.
                     */
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "\tcurrently native so no translation needed\n");
                } else if (thread_synch_state_no_xfer(dcontext)) {
                    /* Case 6821: do not translate other synch-all-thread users.
                     * They have no fragment state, so leave alone.
                     * All flush call points are in syscalls or from nudges.
                     * Xref case 8901 on ensuring nudges don't try to return
                     * to the code cache.
                     */
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "\tat THREAD_SYNCH_NO_LOCKS_NO_XFER so no translation needed\n");
                    STATS_INC(flush_synchall_races);
                } else {
                    translate_from_synchall_to_dispatch(flush_threads[i], desired_state);
                }
            }
            if (dcontext == my_dcontext || thread_synch_successful(flush_threads[i])) {
                last_exit_deleted(dcontext);
                /* case 7394: need to abort other threads' trace building
                 * since the reset xfer to d_r_dispatch will disrupt it.
                 * also, with PR 299808, we now have thread-shared
                 * "undeletable" trace-temp fragments, so we need to abort
                 * all traces.
                 */
                if (is_building_trace(dcontext)) {
                    LOG(THREAD, LOG_FRAGMENT, 2, "\tsquashing trace of thread #%d\n", i);
                    trace_abort(dcontext);
                }
            }
            /* Since coarse fragments never cross coarse/non-coarse executable region
             * bounds, we can bound their bodies by taking
             * executable_area_distinct_bounds().  This lets us remove by walking the
             * ibl tables and looking only at tags, rather than walking the htable of
             * each coarse unit.  FIXME: not clear this is a perf win: I arbitrarily
             * picked it under assumption that ibl tables are relatively small.  It
             * would be a clearer win if we could do the fine fragments this way
             * also, but fine fragments are not constrained and could be missed using
             * only a tag-based range remove.
             */
            DEBUG_DECLARE(removed =)
            fragment_remove_all_ibl_in_region(dcontext, exec_start, exec_end);
            LOG(THREAD, LOG_FRAGMENT, 2, "\tremoved %d ibl entries in " PFX "-" PFX "\n",
                removed, exec_start, exec_end);
            /* Free any fine private fragments in the region */
            vm_area_allsynch_flush_fragments(dcontext, dcontext, base, base + size,
                                             exec_invalid, all_synched /*ignored*/);
            if (!SHARED_IBT_TABLES_ENABLED() && SHARED_FRAGMENTS_ENABLED()) {
                /* Remove shared fine fragments from private ibl tables */
                vm_area_allsynch_flush_fragments(dcontext, GLOBAL_DCONTEXT, base,
                                                 base + size, exec_invalid,
                                                 all_synched /*ignored*/);
            }
        }
    }
    /* Removed shared coarse fragments from ibl tables, before freeing any */
    if (SHARED_IBT_TABLES_ENABLED() && SHARED_FRAGMENTS_ENABLED())
        fragment_remove_all_ibl_in_region(GLOBAL_DCONTEXT, exec_start, exec_end);
    /* Free coarse units and shared fine fragments, as well as removing shared fine
     * entries in any shared ibl tables
     */
    if (SHARED_FRAGMENTS_ENABLED()) {
        vm_area_allsynch_flush_fragments(GLOBAL_DCONTEXT, GLOBAL_DCONTEXT, base,
                                         base + size, exec_invalid, all_synched);
    }
}

static void
flush_fragments_synchall_end(dcontext_t *ignored)
{
    thread_record_t **temp_threads = flush_threads;
    DEBUG_DECLARE(dcontext_t *my_dcontext = get_thread_private_dcontext();)
    LOG(GLOBAL, LOG_FRAGMENT, 2, "flush_fragments_synchall_end: resuming all threads\n");

    /* We need to clear this before we release the locks.  We use a temp var
     * so we can use end_synch_with_all_threads.
     */
    flush_threads = NULL;
    ASSERT(flusher == NULL);
    flush_synchall = false;
    ASSERT(dynamo_all_threads_synched);
    ASSERT(allsynch_flusher == my_dcontext);
    allsynch_flusher = NULL;
    end_synch_with_all_threads(temp_threads, flush_num_threads, true /*resume*/);
    KSTOP(synchall_flush);
}

/* Relink shared sycalls and/or special IBL transfer for thread-private scenario. */
static void
flush_fragments_relink_thread_syscalls(dcontext_t *dcontext, dcontext_t *tgt_dcontext,
                                       per_thread_t *tgt_pt)
{
#ifdef WINDOWS
    if (DYNAMO_OPTION(shared_syscalls)) {
        if (SHARED_FRAGMENTS_ENABLED()) {
            /* we cannot re-link shared_syscall here as that would allow
             * the target thread to enter to-be-flushed fragments prior
             * to their being unlinked and removed from ibl tables -- so
             * we force this thread to re-link in check_flush_queue.
             * we could re-link after unlink/removal of fragments,
             * if it's worth optimizing == case 6194/PR 210655.
             */
            tgt_pt->flush_queue_nonempty = true;
            STATS_INC(num_flushq_relink_syscall);
        } else if (!IS_SHARED_SYSCALL_THREAD_SHARED) {
            /* no shared fragments, and no private ones being flushed,
             * so this thread is all set
             */
            link_shared_syscall(tgt_dcontext);
        }
    }
#endif
    if (special_ibl_xfer_is_thread_private()) {
        if (SHARED_FRAGMENTS_ENABLED()) {
            /* see shared_syscall relink comment: we have to delay the relink */
            tgt_pt->flush_queue_nonempty = true;
            STATS_INC(num_flushq_relink_special_ibl_xfer);
        } else
            link_special_ibl_xfer(dcontext);
    }
}

static bool
flush_fragments_thread_unlink(dcontext_t *dcontext, int thread_index,
                              dcontext_t *tgt_dcontext)
{
    per_thread_t *tgt_pt = (per_thread_t *)tgt_dcontext->fragment_field;

    /* if a trace-in-progress crosses this region, must squash the trace
     * (all traces are essentially frozen now since threads stop in d_r_dispatch)
     */
    if (flush_size > 0 /* else, no region to cross */ &&
        is_building_trace(tgt_dcontext)) {
        void *trace_vmlist = cur_trace_vmlist(tgt_dcontext);
        if (trace_vmlist != NULL &&
            vm_list_overlaps(tgt_dcontext, trace_vmlist, flush_base,
                             flush_base + flush_size)) {
            LOG(THREAD, LOG_FRAGMENT, 2, "\tsquashing trace of thread " TIDFMT "\n",
                tgt_dcontext->owning_thread);
            trace_abort(tgt_dcontext);
        }
    }

    /* Optimization for shared deletion strategy: perform flush work
     * for a thread waiting at a system call on behalf of that thread
     * (which we do before the flush tail synch, as if we did it now we
     * would need to inc flushtime_global up front, and then we'd have synch
     * issues trying to prevent thread we hadn't synched with from checking
     * the pending list before we put flushed fragments on it).
     * We do count these threads in the ref count as we check the pending
     * queue on their behalf after adding the newly flushed fragments to
     * the queue, so the ref count gets decremented right away.
     *
     * We must do this AFTER unlinking shared_syscall's post-syscall ibl, to
     * avoid races -- the thread will hit a real synch point before accessing
     * any fragments or link info.
     * Do this BEFORE checking whether fragments in region to catch all threads.
     */
    ASSERT(!tgt_pt->at_syscall_at_flush);
    if (DYNAMO_OPTION(syscalls_synch_flush) && get_at_syscall(tgt_dcontext)) {
        /* we have to know exactly which threads were at_syscall here when
         * we get to post-flush, so we cache in this special bool
         */
        DEBUG_DECLARE(bool tables_updated;)
        tgt_pt->at_syscall_at_flush = true;
#ifdef DEBUG
        tables_updated =
#endif
            update_all_private_ibt_table_ptrs(tgt_dcontext, tgt_pt);
        STATS_INC(num_shared_flush_atsyscall);
        DODEBUG({
            if (tables_updated)
                STATS_INC(num_shared_tables_updated_atsyscall);
        });
    }

    /* don't need to go any further if thread has no frags in region */
    if (flush_size == 0 ||
        !thread_vm_area_overlap(tgt_dcontext, flush_base, flush_base + flush_size)) {
        LOG(THREAD, LOG_FRAGMENT, 2,
            "\tthread " TIDFMT " has no fragments in region to flush\n",
            tgt_dcontext->owning_thread);
        return true; /* true: relink syscalls now b/c skipping vm_area_flush_fragments */
    }

    LOG(THREAD, LOG_FRAGMENT, 2, "\tflushing fragments for thread " TIDFMT "\n",
        flush_threads[thread_index]->id);
    DOLOG(2, LOG_FRAGMENT, {
        if (tgt_dcontext != dcontext) {
            LOG(tgt_dcontext->logfile, LOG_FRAGMENT, 2,
                "thread " TIDFMT " is flushing our fragments\n", dcontext->owning_thread);
        }
    });

    if (flush_size > 0) {
        /* unlink all frags in overlapping regions, and mark regions for deletion */
        tgt_pt->flush_queue_nonempty = true;
#ifdef DEBUG
        num_flushed +=
#endif
            vm_area_unlink_fragments(tgt_dcontext, flush_base, flush_base + flush_size,
                                     0 _IF_DGCDIAG(written_pc));
    }

    return false; /* false: syscalls remain unlinked until vm_area_flush_fragments */
}

/* This routine begins a flush of the group of fragments in the memory
 * region [base, base+size) by synchronizing with each thread and
 * invoking thread_synch_callback().
 *
 * If shared syscalls and/or special IBL transfer are thread-private, they will
 * be unlinked for each thread prior to invocation of the callback. The return
 * value of the callback indicates whether to relink these routines (true), or
 * wait for a later operation (such as vm_area_flush_fragments()) to relink
 * them later (false).
 */
void
flush_fragments_synch_priv(dcontext_t *dcontext, app_pc base, size_t size,
                           bool own_initexit_lock,
                           bool (*thread_synch_callback)(dcontext_t *dcontext,
                                                         int thread_index,
                                                         dcontext_t *thread_dcontext)
                               _IF_DGCDIAG(app_pc written_pc))
{
    dcontext_t *tgt_dcontext;
    per_thread_t *tgt_pt;
    int i;

    /* our flushing design requires that flushers are NOT couldbelinking
     * and are not holding any locks
     */
    ASSERT(!is_self_couldbelinking());
#if defined(DEADLOCK_AVOIDANCE) && defined(DEBUG)
    if (own_initexit_lock) {
        /* We can get here while holding the all_threads_synch_lock
         * for detach (& prob. TerminateProcess(0) with other threads
         * still active) on XP and 2003 via os_thread_stack_exit()
         * (for other thread exit) if we have executed off that stack.
         * Can be seen with Araktest detach on violation using the stack
         * attack button. */
        ASSERT(thread_owns_first_or_both_locks_only(dcontext, &thread_initexit_lock,
                                                    &all_threads_synch_lock));
    } else {
        ASSERT_OWN_NO_LOCKS();
    }
#endif

    /* Take a snapshot of the threads in the system.
     * Grab the thread lock to prevent threads from being created or
     * exited for the duration of this routine
     * FIXME -- is that wise?  could be a relatively long time!
     * It's unlikely a new thread will run code in a region being
     * unmapped...but we would like to prevent threads from exiting
     * while we're messing with their data.
     * FIXME: need to special-case every instance where thread_initexit_lock
     * is grabbed, to avoid deadlocks!  we already do for a thread exiting.
     */
    if (!own_initexit_lock)
        d_r_mutex_lock(&thread_initexit_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    flusher = dcontext;
    get_list_of_threads(&flush_threads, &flush_num_threads);

    ASSERT(flush_last_stage == 0);
    DODEBUG({ flush_last_stage = 1; });

    /* FIXME: we can optimize this even more to not grab thread_initexit_lock */
    if (RUNNING_WITHOUT_CODE_CACHE()) /* case 7966: nothing to flush, ever */
        return;

    flush_base = base;
    flush_size = size;

    /* Set the ref count of threads who may be using a deleted fragment.  We
     * include ourselves in the ref count as we could be invoked with a cache
     * return point before any synch (as done w/ hotpatches) and should NOT
     * consider ourselves done w/ all cache fragments right now.  We assume we
     * will soon hit a real synch point to dec the count for us, and don't try
     * to specially handle the single-threaded case here.
     */
    pending_delete_threads = flush_num_threads;

    DODEBUG({ num_flushed = 0; });

#ifdef WINDOWS
    /* Make sure exit fcache if currently inside syscall.  For thread-shared
     * shared_syscall, we re-link after the shared fragments have all been
     * unlinked and removed from the ibl tables.  All we lose is the bounded time
     * on a thread checking its private flush queue, which in a thread-shared
     * configuration doesn't seem so bad.  For thread-private this will work as
     * well, with the same time bound lost, but we're not as worried about memory
     * usage of thread-private configurations.
     */
    if (DYNAMO_OPTION(shared_syscalls) && IS_SHARED_SYSCALL_THREAD_SHARED)
        unlink_shared_syscall(GLOBAL_DCONTEXT);
#endif

    /* i#849: unlink while we clear out ibt */
    if (!special_ibl_xfer_is_thread_private())
        unlink_special_ibl_xfer(GLOBAL_DCONTEXT);

    for (i = 0; i < flush_num_threads; i++) {
        tgt_dcontext = flush_threads[i]->dcontext;
        tgt_pt = (per_thread_t *)tgt_dcontext->fragment_field;
        LOG(THREAD, LOG_FRAGMENT, 2, "  considering thread #%d/%d = " TIDFMT "\n", i + 1,
            flush_num_threads, flush_threads[i]->id);
        ASSERT(is_thread_known(tgt_dcontext->owning_thread));

        /* can't do anything, even check if thread has any vm areas overlapping flush
         * region, until sure thread is in fcache or somewhere that won't change
         * vm areas or linking state
         */
        d_r_mutex_lock(&tgt_pt->linking_lock);
        /* Must explicitly check for self and avoid synch then, o/w will lock up
         * if ever called from a could_be_linking location (currently only
         * happens w/ app syscalls)
         */
        if (tgt_dcontext != dcontext && tgt_pt->could_be_linking) {
            /* remember we have a global lock, thread_initexit_lock, so two threads
             * cannot be here at the same time!
             */
            LOG(THREAD, LOG_FRAGMENT, 2, "\twaiting for thread " TIDFMT "\n",
                tgt_dcontext->owning_thread);
            tgt_pt->wait_for_unlink = true;
            d_r_mutex_unlock(&tgt_pt->linking_lock);
            wait_for_event(tgt_pt->waiting_for_unlink, 0);
            d_r_mutex_lock(&tgt_pt->linking_lock);
            tgt_pt->wait_for_unlink = false;
            LOG(THREAD, LOG_FRAGMENT, 2, "\tdone waiting for thread " TIDFMT "\n",
                tgt_dcontext->owning_thread);
        } else {
            LOG(THREAD, LOG_FRAGMENT, 2, "\tthread " TIDFMT " synch not required\n",
                tgt_dcontext->owning_thread);
        }

        /* it is now safe to access link, vm, and trace info in tgt_dcontext
         * => FIXME: rename, since not just accessing linking info?
         * FIXME: if includes vm access, are syscalls now bad?
         * what about handle_modified_code, considered nolinking since straight
         * from cache via fault handler?  reading should be ok, but exec area splits
         * and whatnot?
         */

        if (tgt_pt->about_to_exit) {
            /* thread is about to exit, it's waiting for us to give up
             * thread_initexit_lock -- we don't need to flush it
             */
        } else {
#ifdef WINDOWS
            /* Make sure exit fcache if currently inside syscall.  If thread has no
             * vm area overlap, will be re-linked down below before going to the
             * next thread, unless we have shared fragments, in which case we force
             * the thread to check_flush_queue() on its next synch point.  It will
             * re-link in vm_area_flush_fragments().
             */
            if (DYNAMO_OPTION(shared_syscalls) && !IS_SHARED_SYSCALL_THREAD_SHARED)
                unlink_shared_syscall(tgt_dcontext);
#endif
            /* i#849: unlink while we clear out ibt */
            if (special_ibl_xfer_is_thread_private())
                unlink_special_ibl_xfer(tgt_dcontext);

            if (thread_synch_callback(dcontext, i, tgt_dcontext))
                flush_fragments_relink_thread_syscalls(dcontext, tgt_dcontext, tgt_pt);
        }

        /* for thread-shared, we CANNOT let any thread become could_be_linking, for normal
         * flushing synch -- for thread-private, we can, but we CANNOT let any thread
         * that we've already synched with for flushing go and change the exec areas
         * vector!  the simplest solution is to have thread-private, like thread-shared,
         * stop all threads at a cache exit point.
         * FIXME: optimize thread-private by allowing already-synched threads to
         * continue but not grab executable_areas lock, while letting
         * to-be-synched threads grab the lock (otherwise could wait forever)
         */
        /* Since we must let go of lock for proper nesting, we use a new
         * synch to stop threads at cache exit, since we need them all
         * out of DR for duration of shared flush.
         */
        if (tgt_dcontext != dcontext && !tgt_pt->could_be_linking)
            tgt_pt->wait_for_unlink = true; /* stop at cache exit */
        d_r_mutex_unlock(&tgt_pt->linking_lock);
    }
}

/* This routine begins a flush of the group of fragments in the memory
 * region [base, base+size) by synchronizing with each thread and unlinking
 * all private fragments in the region.
 *
 * The exec_invalid parameter must be set to indicate whether the
 * executable area is being invalidated as well or this is just a capacity
 * flush (or a flush to change instrumentation).
 *
 * If size==0 then no unlinking occurs; however, the full synch is
 * performed (so the caller should check size if no action is desired on
 * zero size).
 *
 * If size>0 and there is no executable area overlap, then no synch is
 * performed and false is returned.  The caller must acquire the executable
 * areas lock and re-check the overlap if exec area manipulation is to be
 * performed.  Returns true otherwise.
 */
bool
flush_fragments_synch_unlink_priv(dcontext_t *dcontext, app_pc base, size_t size,
                                  /* WARNING: case 8572: the caller owning this lock
                                   * is incompatible w/ suspend-the-world flushing!
                                   */
                                  bool own_initexit_lock, bool exec_invalid,
                                  bool force_synchall _IF_DGCDIAG(app_pc written_pc))
{
    LOG(THREAD, LOG_FRAGMENT, 2,
        "FLUSH STAGE 1: synch_unlink_priv(thread " TIDFMT " flushtime %d): " PFX "-" PFX
        "\n",
        dcontext->owning_thread, flushtime_global, base, base + size);
    /* Case 9750: to specify a region of size 0, do not pass in NULL as the base!
     * Use EMPTY_REGION_{BASE,SIZE} instead.
     */
    ASSERT(base != NULL || size != 0);

    ASSERT(dcontext == get_thread_private_dcontext());

    /* quick check for overlap first by using read lock and avoiding
     * thread_initexit_lock:
     * if no overlap, must hold lock through removal of region
     * if overlap, ok to release for a bit
     */
    if (size > 0 && !executable_vm_area_executed_from(base, base + size)) {
        /* Only a curiosity since we can have a race (not holding exec areas lock) */
        ASSERT_CURIOSITY((!SHARED_FRAGMENTS_ENABLED() ||
                          !thread_vm_area_overlap(GLOBAL_DCONTEXT, base, base + size)) &&
                         !thread_vm_area_overlap(dcontext, base, base + size));
        return false;
    }
    /* Only a curiosity since we can have a race (not holding exec areas lock) */
    ASSERT_CURIOSITY(size == 0 ||
                     executable_vm_area_overlap(base, base + size, false /*no lock*/));

    STATS_INC(num_flushes);

    if (force_synchall ||
        (size > 0 && executable_vm_area_coarse_overlap(base, base + size))) {
        /* Coarse units do not support individual unlinking (though prior to
         * freezing they do, we ignore that) and instead require all-thread-synch
         * in order to flush.  For that we cannot be already holding
         * thread_initexit_lock!  FIXME case 8572: the only caller who does hold it
         * now is os_thread_stack_exit().  For now relying on that stack not
         * overlapping w/ any coarse regions.
         */
        ASSERT(!own_initexit_lock);
        /* The synchall will flush fine as well as coarse so we'll be done */
        flush_fragments_synchall_start(dcontext, base, size, exec_invalid);
        return true;
    }

    flush_fragments_synch_priv(dcontext, base, size, own_initexit_lock,
                               flush_fragments_thread_unlink _IF_DGCDIAG(written_pc));

    return true;
}

/* This routine continues a flush of one of two groups of fragments:
 * 1) if list!=NULL, the list of shared fragments beginning at list and
 *    chained by next_vmarea (we assume that private fragments are
 *    easily deleted by the owning thread and do not require a flush).
 *    Caller should call vm_area_remove_fragment() for each target fragment,
 *    building a custom list chained by next_vmarea, and pass that list to this
 *    routine.
 * 2) otherwise, all fragments in the memory region [base, base+size).
 *
 * This routine MUST be called after flush_fragments_synch_unlink_priv(), and
 * must be followed with flush_fragments_end_synch().
 */
void
flush_fragments_unlink_shared(dcontext_t *dcontext, app_pc base, size_t size,
                              fragment_t *list _IF_DGCDIAG(app_pc written_pc))
{
    /* we would assert that size > 0 || list != NULL but we have to pass
     * in 0 and NULL for unit flushing when no live fragments are in the unit
     */

    LOG(THREAD, LOG_FRAGMENT, 2,
        "FLUSH STAGE 2: unlink_shared(thread " TIDFMT "): flusher is " TIDFMT "\n",
        dcontext->owning_thread, (flusher == NULL) ? -1 : flusher->owning_thread);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT(flush_threads != NULL);
    ASSERT(flush_num_threads > 0);
    ASSERT(flush_last_stage == 1);
    DODEBUG({ flush_last_stage = 2; });

    if (RUNNING_WITHOUT_CODE_CACHE()) /* case 7966: nothing to flush, ever */
        return;
    if (flush_synchall) /* no more flush work to do */
        return;

    if (SHARED_FRAGMENTS_ENABLED()) {
        /* Flushing shared fragments: the strategy is again immediate
         * unlinking (plus hashtable removal and vm area list removal)
         * with delayed deletion.  Unlinking is atomic, and hashtable
         * removal is for now since there are no shared bbs in ibls from
         * the cache.  But deletion needs to ensure that every thread who may
         * hold a pointer to a shared fragment or may be inside a shared
         * fragment is ok with the fragment being deleted.  We use a reference count
         * for each flushed region to ensure that every thread has reached a synch
         * point before we actually free the fragments in the region.
         *
         * We do have pathological cases where a single thread at a wait syscall or
         * an infinite loop in the cache will prevent all freeing.  One solution
         * to the syscall problem is to store a flag saying that a thread is at
         * the shared_syscall routine.  That routine is private and unlinked so
         * we know the thread will hit a synch routine if it exits, so no race.
         * We then wouldn't bother to inc the ref count for that thread.
         *
         * Our fallback proactive deletion is to do a full reset when the size
         * of the pending deletion list gets too long, though we could be more
         * efficient by only resetting the pending list, and by only suspending
         * those threads that haven't dec-ed a ref count in a particular region.
         */
        LOG(THREAD, LOG_FRAGMENT, 2, "  flushing shared fragments\n");
        if (DYNAMO_OPTION(shared_deletion)) {
            /* We use shared_cache_flush_lock to make atomic the increment of
             * flushtime_global and the adding of pending deletion fragments with
             * that flushtime, wrt other threads checking the pending list.
             */
            d_r_mutex_lock(&shared_cache_flush_lock);
        }
        /* Increment flush count for shared deletion algorithm and for list-based
         * flushing (such as for shared cache units).  We could wait
         * and only do this if we actually flush shared fragments, but it's
         * simpler when done in this routine, and nearly every flush does flush
         * shared fragments.
         */
        increment_global_flushtime();
        /* Both vm_area_unlink_fragments and unlink_fragments_for_deletion call
         * back to flush_invalidate_ibl_shared_target to remove shared
         * fragments from private/shared ibl tables
         */
        if (list == NULL) {
            shared_flushed =
                vm_area_unlink_fragments(GLOBAL_DCONTEXT, base, base + size,
                                         pending_delete_threads _IF_DGCDIAG(written_pc));
        } else {
            shared_flushed = unlink_fragments_for_deletion(GLOBAL_DCONTEXT, list,
                                                           pending_delete_threads);
        }
        if (DYNAMO_OPTION(shared_deletion))
            d_r_mutex_unlock(&shared_cache_flush_lock);

        DODEBUG({
            num_flushed += shared_flushed;
            if (shared_flushed > 0)
                STATS_INC(num_shared_flushes);
        });
    }

#ifdef WINDOWS
    /* Re-link thread-shared shared_syscall */
    if (DYNAMO_OPTION(shared_syscalls) && IS_SHARED_SYSCALL_THREAD_SHARED)
        link_shared_syscall(GLOBAL_DCONTEXT);
#endif

    if (!special_ibl_xfer_is_thread_private())
        link_special_ibl_xfer(GLOBAL_DCONTEXT);

    STATS_ADD(num_flushed_fragments, num_flushed);
    DODEBUG({
        if (num_flushed > 0) {
            LOG(THREAD, LOG_FRAGMENT, 1, "Flushed %5d fragments from " PFX "-" PFX "\n",
                num_flushed, base, ((char *)base) + size);
        } else {
            STATS_INC(num_empty_flushes);
            LOG(THREAD, LOG_FRAGMENT, 2, "Flushed     0 fragments from " PFX "-" PFX "\n",
                base, ((char *)base) + size);
        }
    });
}

/* Invalidates (does not remove) shared fragment f from the private/shared
 * ibl tables.  Can only be called in flush stage 2.
 */
void
flush_invalidate_ibl_shared_target(dcontext_t *dcontext, fragment_t *f)
{
    ASSERT(is_self_flushing());
    ASSERT(!flush_synchall);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT(flush_threads != NULL);
    ASSERT(flush_num_threads > 0);
    ASSERT(flush_last_stage == 2);
    ASSERT(TEST(FRAG_SHARED, f->flags));
    /*case 7966: has no pt, no flushing either */
    if (RUNNING_WITHOUT_CODE_CACHE()) {
        ASSERT_NOT_REACHED(); /* shouldn't get here */
        return;
    }
    if (!SHARED_IB_TARGETS())
        return;
    if (SHARED_IBT_TABLES_ENABLED()) {
        /* Remove from shared ibl tables.
         * dcontext need not be GLOBAL_DCONTEXT.
         */
        fragment_prepare_for_removal(dcontext, f);
    } else {
        /* We can't tell afterward which entries to remove from the private
         * ibl tables, as we no longer keep fragment_t* pointers (we used to
         * look for FRAG_WAS_DELETED) and we can't do a range remove (tags
         * don't map regularly to regions).  So we must invalidate each
         * fragment as we process it.  It's ok to walk the thread list
         * here since we're post-synch for all threads.
         */
        int i;
        for (i = 0; i < flush_num_threads; i++) {
            fragment_prepare_for_removal(flush_threads[i]->dcontext, f);
        }
    }
}

/* Must ONLY be called as the third part of flushing (after
 * flush_fragments_synch_unlink_priv() and flush_fragments_unlink_shared()).
 * Uses the static shared variables flush_threads and flush_num_threads.
 */
void
flush_fragments_end_synch(dcontext_t *dcontext, bool keep_initexit_lock)
{
    dcontext_t *tgt_dcontext;
    per_thread_t *tgt_pt;
    int i;

    LOG(THREAD, LOG_FRAGMENT, 2,
        "FLUSH STAGE 3: end_synch(thread " TIDFMT "): flusher is " TIDFMT "\n",
        dcontext->owning_thread, (flusher == NULL) ? -1 : flusher->owning_thread);

    if (!is_self_flushing() && !flush_synchall /* doesn't set flusher */) {
        LOG(THREAD, LOG_FRAGMENT, 2, "\tnothing was flushed\n");
        ASSERT_DO_NOT_OWN_MUTEX(!keep_initexit_lock, &thread_initexit_lock);
        ASSERT_OWN_MUTEX(keep_initexit_lock, &thread_initexit_lock);
        return;
    }

    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);

    ASSERT(flush_threads != NULL);
    ASSERT(flush_num_threads > 0);
    ASSERT(flush_last_stage == 2);
    DODEBUG({ flush_last_stage = 0; });

    if (flush_synchall) {
        flush_fragments_synchall_end(dcontext);
        return;
    }

    /* now can let all threads at DR synch point go
     * FIXME: if implement thread-private optimization above, this would turn into
     * re-setting exec areas lock to treat all threads uniformly
     */
    for (i = flush_num_threads - 1; i >= 0; i--) {
        /*case 7966: has no pt, no flushing either */
        if (RUNNING_WITHOUT_CODE_CACHE())
            continue;

        tgt_dcontext = flush_threads[i]->dcontext;
        tgt_pt = (per_thread_t *)tgt_dcontext->fragment_field;
        /* re-acquire lock */
        d_r_mutex_lock(&tgt_pt->linking_lock);

        /* Optimization for shared deletion strategy: perform flush work
         * for a thread waiting at a system call, as we didn't add it to the
         * ref count in the pre-flush synch.
         * We assume that shared_syscall is still unlinked at this point and
         * will not be relinked until we let the thread go.
         */
        if (DYNAMO_OPTION(syscalls_synch_flush) && tgt_pt->at_syscall_at_flush) {
            /* Act on behalf of the thread as though it's at a synch point, but
             * only wrt shared fragments (we don't free private fragments here,
             * though we could -- should we?  may make flush time while holding
             * lock take too long?  FIXME)
             * Currently this works w/ syscalls from d_r_dispatch, and w/
             * -shared_syscalls by using unprotected storage (thus a slight hole
             * but very hard to exploit for security purposes: can get stale code
             * executed, but we already have that window, or crash us).
             * FIXME: Does not work w/ -ignore_syscalls, but those are private
             * for now.
             */
#ifdef DEBUG
            uint pre_flushtime;
            ATOMIC_4BYTE_ALIGNED_READ(&flushtime_global, &pre_flushtime);
#endif
            vm_area_check_shared_pending(tgt_dcontext, NULL);
            /* lazy deletion may inc flushtime_global, so may have a higher
             * value than our cached one, but should never be lower
             */
            ASSERT(tgt_pt->flushtime_last_update >= pre_flushtime);
            tgt_pt->at_syscall_at_flush = false;
        }

        if (tgt_dcontext != dcontext) {
            if (tgt_pt->could_be_linking) {
                signal_event(tgt_pt->finished_with_unlink);
            } else {
                /* we don't need to wait on a !could_be_linking thread
                 * so we use this bool to tell whether we should signal
                 * the event.
                 * FIXME: really we want a pulse that wakes ALL waiting
                 * threads and then resets the event!
                 */
                tgt_pt->wait_for_unlink = false;
                if (tgt_pt->soon_to_be_linking)
                    signal_event(tgt_pt->finished_all_unlink);
            }
        }
        d_r_mutex_unlock(&tgt_pt->linking_lock);
    }

    /* thread init/exit can proceed now */
    flusher = NULL;
    global_heap_free(flush_threads,
                     flush_num_threads *
                         sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    flush_threads = NULL;
    if (!keep_initexit_lock)
        d_r_mutex_unlock(&thread_initexit_lock);
}

/* This routine performs flush stages 1 and 2 (synch_unlink_priv()
 * and unlink_shared()) and then returns after grabbing the
 * executable_areas lock so that removal of this area from the global list
 * is atomic w/ the flush and local removals, before letting the threads go
 * -- we return while holding both the thread_initexit_lock and the exec
 * areas lock
 *
 * FIXME: this only helps thread-shared: for thread-private, should let
 * already-synched threads continue but not grab executable_areas lock, while
 * letting to-be-synched threads grab the lock.
 *
 * returning while holding locks isn't ideal -- but other choices are worse:
 * 1) grab exec areas lock and then let go of initexit lock -- bad nesting
 * 2) do all work here, which has to include:
 *    A) remove region (nearly all uses) == flush_fragments_and_remove_region
 *    B) change region from rw to r (splitting if nec) (vmareas.c app rw->r)
 *    C) restore writability and change to selfmod (splitting if nec)
 *       (vmareas.c handle_modified_code)
 *    D) remove region and then look up a certain pc and return whether
 *       flushing overlapped w/ it (vmareas.c handle_modified_code)
 */
void
flush_fragments_in_region_start(dcontext_t *dcontext, app_pc base, size_t size,
                                bool own_initexit_lock, bool free_futures,
                                bool exec_invalid,
                                bool force_synchall _IF_DGCDIAG(app_pc written_pc))
{
    KSTART(flush_region);
    while (true) {
        if (flush_fragments_synch_unlink_priv(dcontext, base, size, own_initexit_lock,
                                              exec_invalid,
                                              force_synchall _IF_DGCDIAG(written_pc))) {
            break;
        } else {
            /* grab lock and then re-check overlap */
            executable_areas_lock();
            if (!executable_vm_area_executed_from(base, base + size)) {
                LOG(THREAD, LOG_FRAGMENT, 2,
                    "\tregion not executable, so no fragments to flush\n");
                /* caller will release lock! */
                STATS_INC(num_noncode_flushes);
                return;
            }
            /* unlock and try again */
            executable_areas_unlock();
        }
    }

    flush_fragments_unlink_shared(dcontext, base, size, NULL _IF_DGCDIAG(written_pc));

    /* We need to free the futures after all fragments have been unlinked */
    if (free_futures) {
        flush_fragments_free_futures(base, size);
    }

    executable_areas_lock();
}

/* must ONLY be called as the second half of flush_fragments_in_region_start().
 * uses the shared variables flush_threads and flush_num_threads
 */
void
flush_fragments_in_region_finish(dcontext_t *dcontext, bool keep_initexit_lock)
{
    /* done w/ exec areas lock; also free any non-executed coarse units */
    free_nonexec_coarse_and_unlock();

    flush_fragments_end_synch(dcontext, keep_initexit_lock);
    KSTOP(flush_region);
}

/* flush and remove region from exec list, atomically */
void
flush_fragments_and_remove_region(dcontext_t *dcontext, app_pc base, size_t size,
                                  bool own_initexit_lock, bool free_futures)
{
    flush_fragments_in_region_start(dcontext, base, size, own_initexit_lock, free_futures,
                                    true /*exec invalid*/,
                                    false /*don't force synchall*/ _IF_DGCDIAG(NULL));
    /* ok to call on non-exec region, so don't need to test return value
     * both flush routines will return quickly if nothing to flush/was flushed
     */
    remove_executable_region(base, size, true /*have lock*/);
    flush_fragments_in_region_finish(dcontext, own_initexit_lock);

    /* verify initexit lock is in the right state */
    ASSERT_OWN_MUTEX(own_initexit_lock, &thread_initexit_lock);
    ASSERT_DO_NOT_OWN_MUTEX(!own_initexit_lock, &thread_initexit_lock);
}

/* Flushes fragments from the region without any changes to the exec list.
 * Does not free futures and caller can't be holding the initexit lock.
 * Invokes the given callback after flushing and before resuming threads.
 * FIXME - add argument parameters (free futures etc.) as needed. */
void
flush_fragments_from_region(dcontext_t *dcontext, app_pc base, size_t size,
                            bool force_synchall,
                            void (*flush_completion_callback)(void *user_data),
                            void *user_data)
{
    /* we pass false to flush_fragments_in_region_start() below for owning the initexit
     * lock */
    ASSERT_DO_NOT_OWN_MUTEX(true, &thread_initexit_lock);

    /* ok to call on non-exec region, so don't need to test return value
     * both flush routines will return quickly if nothing to flush/was flushed */
    flush_fragments_in_region_start(dcontext, base, size, false /*don't own initexit*/,
                                    false /*don't free futures*/, false /*exec valid*/,
                                    force_synchall _IF_DGCDIAG(NULL));
    if (flush_completion_callback != NULL) {
        (*flush_completion_callback)(user_data);
    }

    flush_fragments_in_region_finish(dcontext, false);
}

/* Invalidate all fragments in all caches.  Currently executed
 * fragments may be alive until they reach an exit.
 *
 * Note not necessarily immediately freeing memory like
 * fcache_reset_all_caches_proactively().
 */
void
invalidate_code_cache()
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    LOG(GLOBAL, LOG_FRAGMENT, 2, "invalidate_code_cache()\n");
    flush_fragments_in_region_start(dcontext, UNIVERSAL_REGION_BASE,
                                    UNIVERSAL_REGION_SIZE, false,
                                    true /* remove futures */, false /*exec valid*/,
                                    false /*don't force synchall*/
                                    _IF_DGCDIAG(NULL));
    flush_fragments_in_region_finish(dcontext, false);
}

/* Flushes all areas stored in the vector toflush.
 * Synchronization of toflush is up to caller, but as locks cannot be
 * held when flushing, toflush must be thread-private.
 * Currently only used for pcache hotp interop (case 9970).
 */
void
flush_vmvector_regions(dcontext_t *dcontext, vm_area_vector_t *toflush, bool free_futures,
                       bool exec_invalid)
{
    vmvector_iterator_t vmvi;
    app_pc start, end;
    ASSERT(toflush != NULL && !TEST(VECTOR_SHARED, toflush->flags));
    ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
    ASSERT(DYNAMO_OPTION(coarse_units) &&
           DYNAMO_OPTION(use_persisted) IF_HOTP(&&DYNAMO_OPTION(hot_patching)));
    if (vmvector_empty(toflush))
        return;
    vmvector_iterator_start(toflush, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        vmvector_iterator_next(&vmvi, &start, &end);
        /* FIXME case 10086: optimization: batch the flushes together so we only
         * synch once.  Currently this is rarely used, and with few areas in the
         * vector, so we don't bother.  To batch them we'd need to assume the
         * worst and do a synchall up front, which could be more costly than 2
         * or 3 separate flushes that do not involve coarse code.
         */
        ASSERT_OWN_NO_LOCKS();
        flush_fragments_in_region_start(dcontext, start, end - start, false /*no lock*/,
                                        free_futures, exec_invalid,
                                        false /*don't force synchall*/ _IF_DGCDIAG(NULL));
        flush_fragments_in_region_finish(dcontext, false /*no lock*/);
        STATS_INC(num_flush_vmvector);
    }
    vmvector_iterator_stop(&vmvi);
}

/****************************************************************************/

void
fragment_output(dcontext_t *dcontext, fragment_t *f)
{
    ASSERT(!TEST(FRAG_SHARED, f->flags) ||
           self_owns_recursive_lock(&change_linking_lock));
    if (SHOULD_OUTPUT_FRAGMENT(f->flags)) {
        per_thread_t *pt = (dcontext == GLOBAL_DCONTEXT)
            ? shared_pt
            : (per_thread_t *)dcontext->fragment_field;
        output_trace(dcontext, pt, f,
                     IF_DEBUG_ELSE(GLOBAL_STAT(num_fragments),
                                   GLOBAL_STAT(num_traces) + GLOBAL_STAT(num_bbs)) +
                         1);
    }
}

void
init_trace_file(per_thread_t *pt)
{
    if (INTERNAL_OPTION(tracedump_binary)) {
        /* First 4 bytes in binary file gives size of linkcounts, which are
         * no longer supported: we always set to 0 to indicate no linkcounts.
         */
        tracedump_file_header_t hdr = { CURRENT_API_VERSION, IF_X64_ELSE(true, false),
                                        0 };
        os_write(pt->tracefile, &hdr, sizeof(hdr));
    }
}

void
exit_trace_file(per_thread_t *pt)
{
    close_log_file(pt->tracefile);
}

/* Binary trace dump is used to save time and space.
 * The format is in fragment.h.
 * FIXME: add general symbol table support to disassembly?
 *   We'd dump num_targets, then fcache_enter, ibl, trace_cache_incr addrs?
 *   Reader would then add exit stubs & other traces to table?
 *   But links will target cache pcs...
 */

#define TRACEBUF_SIZE 2048

#define TRACEBUF_MAKE_ROOM(p, buf, sz)               \
    do {                                             \
        if (p + sz >= &buf[TRACEBUF_SIZE]) {         \
            os_write(pt->tracefile, buf, (p - buf)); \
            p = buf;                                 \
        }                                            \
    } while (0)

static void
output_trace_binary(dcontext_t *dcontext, per_thread_t *pt, fragment_t *f,
                    stats_int_t trace_num)
{
    /* FIXME:
     * We do not support PROFILE_RDTSC or various small fields
     */
    /* FIXME: should allocate buffer elsewhere */
    byte buf[TRACEBUF_SIZE];
    byte *p = buf;
    trace_only_t *t = TRACE_FIELDS(f);
    linkstub_t *l;
    tracedump_trace_header_t hdr = {
        (int)trace_num,
        f->tag,
        f->start_pc,
        f->prefix_size,
        0,
        f->size,
        (INTERNAL_OPTION(tracedump_origins) ? t->num_bbs : 0),
        IF_X86_64_ELSE(!TEST(FRAG_32_BIT, f->flags), false),
    };
    tracedump_stub_data_t stub;
    /* Should we widen the identifier? */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(trace_num)));

    for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l))
        hdr.num_exits++;

    TRACEBUF_MAKE_ROOM(p, buf, sizeof(hdr));
    *((tracedump_trace_header_t *)p) = hdr;
    p += sizeof(hdr);

    if (INTERNAL_OPTION(tracedump_origins)) {
        uint i;
        for (i = 0; i < t->num_bbs; i++) {
            instr_t *inst;
            instrlist_t *ilist;
            int size = 0;

            TRACEBUF_MAKE_ROOM(p, buf, sizeof(app_pc));
            *((app_pc *)p) = t->bbs[i].tag;
            p += sizeof(app_pc);

            /* we assume that the target is readable, since we dump prior
             * to unloading of modules on flush events
             */
            ilist = build_app_bb_ilist(dcontext, t->bbs[i].tag, INVALID_FILE);
            for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
                size += instr_length(dcontext, inst);
            }

            TRACEBUF_MAKE_ROOM(p, buf, sizeof(int));
            *((int *)p) = size;
            p += sizeof(int);

            for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
                TRACEBUF_MAKE_ROOM(p, buf, instr_length(dcontext, inst));
                /* PR 302353: we can't use instr_encode() as it will
                 * try to re-relativize rip-rel instrs, which may fail
                 */
                ASSERT(instr_get_raw_bits(inst) != NULL);
                memcpy(p, instr_get_raw_bits(inst), instr_length(dcontext, inst));
                p += instr_length(dcontext, inst);
            }
            /* free the instrlist_t elements */
            instrlist_clear_and_destroy(dcontext, ilist);
        }
    }

    ASSERT(SEPARATE_STUB_MAX_SIZE == DIRECT_EXIT_STUB_SIZE(0));

    for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        cache_pc stub_pc = EXIT_STUB_PC(dcontext, f, l);
        stub.cti_offs = l->cti_offset;
        stub.stub_pc = stub_pc;
        stub.target = EXIT_TARGET_TAG(dcontext, f, l);
        stub.linked = TEST(LINK_LINKED, l->flags);
        stub.stub_size = EXIT_HAS_STUB(l->flags, f->flags)
            ? DIRECT_EXIT_STUB_SIZE(f->flags)
            : 0 /* no stub neededd: -no_indirect_stubs */;
        ASSERT(DIRECT_EXIT_STUB_SIZE(f->flags) <= SEPARATE_STUB_MAX_SIZE);

        TRACEBUF_MAKE_ROOM(p, buf, STUB_DATA_FIXED_SIZE);
        memcpy(p, &stub, STUB_DATA_FIXED_SIZE);
        p += STUB_DATA_FIXED_SIZE;

        if (TEST(LINK_SEPARATE_STUB, l->flags) && stub_pc != NULL) {
            TRACEBUF_MAKE_ROOM(p, buf, DIRECT_EXIT_STUB_SIZE(f->flags));
            ASSERT(stub_pc < f->start_pc || stub_pc >= f->start_pc + f->size);
            memcpy(p, stub_pc, DIRECT_EXIT_STUB_SIZE(f->flags));
            p += DIRECT_EXIT_STUB_SIZE(f->flags);
        } else { /* ensure client's method of identifying separate stubs works */
            ASSERT(stub_pc == NULL /* no stub at all */ ||
                   (stub_pc >= f->start_pc && stub_pc < f->start_pc + f->size));
        }
    }

    if (f->size >= TRACEBUF_SIZE) {
        os_write(pt->tracefile, buf, (p - buf));
        p = buf;
        os_write(pt->tracefile, f->start_pc, f->size);
    } else {
        /* single syscall for all but largest traces */
        TRACEBUF_MAKE_ROOM(p, buf, f->size);
        memcpy(p, f->start_pc, f->size);
        p += f->size;
        os_write(pt->tracefile, buf, (p - buf));
        p = buf;
    }
}

/* Output the contents of the specified trace.
 * Does full disassembly of every instruction.
 * If deleted_at != -1, it is taken to be the fragment id that
 * caused the flushing of this fragment from the cache
 * If f is shared, pt argument must be shared_pt, and caller must hold
 * the change_linking_lock.
 */
static void
output_trace(dcontext_t *dcontext, per_thread_t *pt, fragment_t *f,
             stats_int_t deleted_at)
{
    trace_only_t *t = TRACE_FIELDS(f);
#ifdef WINDOWS
    char buf[MAXIMUM_PATH];
#endif
    stats_int_t trace_num;
    bool locked_vmareas = false;
    dr_isa_mode_t old_mode;
    ASSERT(SHOULD_OUTPUT_FRAGMENT(f->flags));
    ASSERT(TEST(FRAG_IS_TRACE, f->flags));
    ASSERT(!TEST(FRAG_SELFMOD_SANDBOXED, f->flags)); /* no support for selfmod */
    /* already been output?  caller should check this flag, just like trace flag */
    ASSERT(!TEST(FRAG_TRACE_OUTPUT, f->flags));
    ASSERT(!TEST(FRAG_SHARED, f->flags) ||
           self_owns_recursive_lock(&change_linking_lock));
    f->flags |= FRAG_TRACE_OUTPUT;

    LOG(THREAD, LOG_FRAGMENT, 4, "output_trace: F%d(" PFX ")\n", f->id, f->tag);
    /* Recreate in same mode as original fragment */
    DEBUG_DECLARE(bool ok =)
    dr_set_isa_mode(dcontext, FRAG_ISA_MODE(f->flags), &old_mode);
    ASSERT(ok);

    /* xref 8131/8202 if dynamo_resetting we don't need to grab the tracedump
     * mutex to ensure we're the only writer and grabbing here on reset path
     * can lead to a rank-order violation. */
    if (!dynamo_resetting) {
        /* We must grab shared_vm_areas lock first to avoid rank order (i#1157) */
        if (SHARED_FRAGMENTS_ENABLED())
            locked_vmareas = acquire_vm_areas_lock_if_not_already(dcontext, FRAG_SHARED);
        d_r_mutex_lock(&tracedump_mutex);
    }
    trace_num = tcount;
    tcount++;
    if (!TEST(FRAG_SHARED, f->flags)) {
        /* No lock is needed because we use thread-private files.
         * If dumping traces for a different thread (dynamo_other_thread_exit
         * for ex.) caller is responsible for the necessary synchronization. */
        ASSERT(pt != shared_pt);
        if (!dynamo_resetting) {
            d_r_mutex_unlock(&tracedump_mutex);
            if (locked_vmareas) {
                locked_vmareas = false;
                release_vm_areas_lock(dcontext, FRAG_SHARED);
            }
        }
    } else {
        ASSERT(pt == shared_pt);
    }

    /* binary dump requested? */
    if (INTERNAL_OPTION(tracedump_binary)) {
        output_trace_binary(dcontext, pt, f, trace_num);
        goto output_trace_done;
    }

    /* just origins => just bb tags in text */
    if (INTERNAL_OPTION(tracedump_origins) && !INTERNAL_OPTION(tracedump_text)) {
        uint i;
        print_file(pt->tracefile, "Trace %d\n", tcount);
#ifdef DEBUG
        print_file(pt->tracefile, "Fragment %d\n", f->id);
#endif
        for (i = 0; i < t->num_bbs; i++) {
            print_file(pt->tracefile, "\tbb %d = " PFX "\n", i, t->bbs[i].tag);
        }
        print_file(pt->tracefile, "\n");
        goto output_trace_done;
    }

    /* full text dump */

    print_file(pt->tracefile,
               "=============================================="
               "=============================\n\n");
    print_file(pt->tracefile, "TRACE # %d\n", tcount);
#ifdef DEBUG
    print_file(pt->tracefile, "Fragment # %d\n", f->id);
#endif
    print_file(pt->tracefile, "Tag = " PFX "\n", f->tag);
    print_file(pt->tracefile, "Thread = %d\n", d_r_get_thread_id());
    if (deleted_at > -1) {
        print_file(pt->tracefile, "*** Flushed from cache when top fragment id was %d\n",
                   deleted_at);
    }

#ifdef WINDOWS
    /* FIXME: for fragments flushed by unloaded modules, naturally we
     * won't be able to get the module name b/c by now it is unloaded,
     * the only fix is to keep a listing of mapped modules and only
     * unmap in our listing at this point
     */
    get_module_name(f->tag, buf, sizeof(buf));
    if (buf[0] != '\0')
        print_file(pt->tracefile, "Module of basic block 0 = %s\n", buf);
    else
        print_file(pt->tracefile, "Module of basic block 0 = <unknown>\n");
#endif

    if (INTERNAL_OPTION(tracedump_origins)) {
        uint i;
        print_file(pt->tracefile, "\nORIGINAL CODE:\n");
        for (i = 0; i < t->num_bbs; i++) {
            /* we only care about the printing that the build routine does */
            print_file(pt->tracefile, "basic block # %d: ", i);
            /* we assume that the target is readable, since we dump prior
             * to unloading of modules on flush events
             */
            ASSERT(is_readable_without_exception(t->bbs[i].tag, sizeof(t->bbs[i])));
            disassemble_app_bb(dcontext, t->bbs[i].tag, pt->tracefile);
        }
        print_file(pt->tracefile, "END ORIGINAL CODE\n\n");
    }

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        /* Cycle counts are too high due to extra instructions needed
         * to save regs and store time, so we subtract a constant for
         * each fragment entry.
         * Experiments using large-iter loops show that:
         *   empty loop overhead = 2 cycles
         *   2 pushes, 2 moves, 2 pops = 7 cycles - 2 cycles = 5 cycles
         *   add in 1 rdtsc = 38 cycles - 2 cycles = 36 cycles
         *   add in 2 rdtsc = 69 cycles - 2 cycles = 67 cycles
         * If we assume the time transfer happens in the middle of the rdtsc, we
         * should use 36 cycles.
         */
        trace_only_t *tr = TRACE_FIELDS(f);
        const int adjustment = 37;
        uint64 real_time = tr->total_time;
        uint64 temp;
        uint time_top, time_bottom;
        print_file(pt->tracefile, "Size = %d (+ %d for profiling)\n",
                   f->size - profile_call_size(), profile_call_size());
        print_file(pt->tracefile, "Profiling:\n");
        print_file(pt->tracefile, "\tcount = " UINT64_FORMAT_STRING "\n", tr->count);
        print_file(pt->tracefile, "\tmeasured cycles = " PFX "\n", real_time);
        temp = tr->count * (uint64)adjustment;
        if (real_time < temp) {
            print_file(
                pt->tracefile,
                "\t  ERROR: adjustment too large, cutting off at 0, should use < %d\n",
                (int)(real_time / tr->count));
            real_time = 0;
        } else {
            real_time -= temp;
        }
        print_file(pt->tracefile, "\tadjusted cycles = " PFX "\n", real_time);
        divide_uint64_print(real_time, kilo_hertz, false, 6, &time_top, &time_bottom);
        print_file(pt->tracefile, "\ttime  = %u.%.6u ms\n", time_top, time_bottom);
    } else {
        print_file(pt->tracefile, "Size = %d\n", f->size);
    }
#else
    print_file(pt->tracefile, "Size = %d\n", f->size);
#endif /* PROFILE_RDTSC */

    print_file(pt->tracefile, "Body:\n");
    disassemble_fragment_body(dcontext, f, pt->tracefile);

    print_file(pt->tracefile, "END TRACE %d\n\n", tcount);

output_trace_done:
    dr_set_isa_mode(dcontext, old_mode, NULL);
    if (TEST(FRAG_SHARED, f->flags) && !dynamo_resetting) {
        ASSERT_OWN_MUTEX(true, &tracedump_mutex);
        d_r_mutex_unlock(&tracedump_mutex);
        if (locked_vmareas)
            release_vm_areas_lock(dcontext, FRAG_SHARED);
    } else {
        ASSERT_DO_NOT_OWN_MUTEX(true, &tracedump_mutex);
    }
}

/****************************************************************************/

#ifdef PROFILE_RDTSC
/* This routine is called at the start of every trace
 * it assumes the caller has saved the caller-saved registers
 * (eax, ecx, edx)
 *
 * See insert_profile_call() for the assembly code prefix that calls
 * this routine.   The end time is computed in the assembly code and passed
 * in to have as accurate times as possible (don't want the profiling overhead
 * added into the times).  Also, the assembly code computes the new start
 * time after this routine returns.
 *
 * We could generate a custom copy of this routine
 * for every thread, but we don't do that now -- don't need to.
 */
void
profile_fragment_enter(fragment_t *f, uint64 end_time)
{
    dcontext_t *dcontext;
#    ifdef WINDOWS
    /* must get last error prior to getting dcontext */
    int error_code = get_last_error();
#    endif
    trace_only_t *t = TRACE_FIELDS(f);

    /* this is done in assembly:   uint64 end_time = get_time() */
    dcontext = get_thread_private_dcontext();

    /* increment this fragment's execution count */
    t->count++;

    /***********************************************************************/
    /* all-in-cache sequence profiling */

    /* top ten cache times */
    dcontext->cache_frag_count++;

    /***********************************************************************/

    /* we rely on d_r_dispatch being the only way to enter the fcache.
     * d_r_dispatch sets prev_fragment to null prior to entry */
    if (dcontext->prev_fragment != NULL) {
        trace_only_t *last_t = TRACE_FIELDS(dcontext->prev_fragment);
        ASSERT((dcontext->prev_fragment->flags & FRAG_IS_TRACE) != 0);
        /* charge time to last fragment */
        last_t->total_time += (end_time - dcontext->start_time);
    }

    /* set up for next fragment */
    dcontext->prev_fragment = f;
    /* this is done in assembly:   dcontext->start_time = get_time() */

#    ifdef WINDOWS
    /* retore app's error code */
    set_last_error(error_code);
#    endif
}

/* this routine is called from d_r_dispatch after exiting the fcache
 * it finishes up the final fragment's time slot
 */
void
profile_fragment_dispatch(dcontext_t *dcontext)
{
    /* end time slot ASAP, should we try to move this to assembly routines exiting
     * the cache?  probably not worth it, in a long-running program shouldn't
     * be exiting the cache that often */
    uint64 end_time = get_time();
    bool tagtable = LINKSTUB_INDIRECT(dcontext->last_exit->flags);
    if (dcontext->prev_fragment != NULL &&
        (dcontext->prev_fragment->flags & FRAG_IS_TRACE) != 0) {
        /* end time slot, charge time to last fragment
         * there's more overhead here than other time endings, so subtract
         * some time off.  these numbers are pretty arbitrary:
         */
        trace_only_t *last_t = TRACE_FIELDS(dcontext->prev_fragment);
        const uint64 adjust = (tagtable) ? 72 : 36;
        uint64 add = end_time - dcontext->start_time;
        if (add < adjust) {
            SYSLOG_INTERNAL_ERROR("ERROR: profile_fragment_dispatch: add was %d, "
                                  "tagtable %d",
                                  (int)add, tagtable);
            add = 0;
        } else
            add -= adjust;
        ASSERT((dcontext->prev_fragment->flags & FRAG_IS_TRACE) != 0);
        last_t->total_time += add;
    }
}
#endif /* PROFILE_RDTSC */

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT HASHTABLE INSTANTIATION
 */

/* Synch model: the htable lock should be held during all
 * app_to_cache_t manipulations, to be consistent (not strictly
 * necessary as there are no removals from the htable except in
 * all-thread-synch flushes).  Copies of a looked-up cache_pc are safe
 * to use w/o the htable lock, or any lock -- they're pointers into
 * cache units, and there are no deletions of cache units, except in
 * all-thread-synch flushes.
 */

/* 2 macros w/ name and types are duplicated in fragment.h -- keep in sync */
#define NAME_KEY coarse
#define ENTRY_TYPE app_to_cache_t
static app_to_cache_t a2c_empty = { NULL, NULL };
static app_to_cache_t a2c_sentinel = { /*assume invalid*/ (app_pc)PTR_UINT_MINUS_1,
                                       NULL };
/* FIXME: want to inline the app_to_cache_t struct just like lookuptable
 * does and use it for main table -- no support for that right now
 */
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define ENTRY_TAG(f) ((ptr_uint_t)(f).app)
#define ENTRY_EMPTY (a2c_empty)
#define ENTRY_SENTINEL (a2c_sentinel)
#define ENTRY_IS_EMPTY(f) ((f).app == a2c_empty.app)
#define ENTRY_IS_SENTINEL(f) ((f).app == a2c_sentinel.app)
#define ENTRY_IS_INVALID(f) (false) /* no invalid entries */
#define ENTRIES_ARE_EQUAL(t, f, g) ((f).app == (g).app)
#define HASHTABLE_WHICH_HEAP(flags) (ACCT_FRAG_TABLE)
/* note that we give the th table a lower-ranked coarse_th_htable_rwlock */
#define COARSE_HTLOCK_RANK coarse_table_rwlock /* for use after hashtablex.h */
#define HTLOCK_RANK COARSE_HTLOCK_RANK
#define HASHTABLE_SUPPORT_PERSISTENCE 1

#include "hashtablex.h"
/* All defines are undef-ed at end of hashtablex.h
 * Would be nice to re-use ENTRY_IS_EMPTY, etc., though w/ multiple htables
 * in same file can't realistically get away w/o custom defines like these:
 */
#define A2C_ENTRY_IS_EMPTY(a2c) ((a2c).app == NULL)
#define A2C_ENTRY_IS_SENTINEL(a2c) ((a2c).app == a2c_sentinel.app)
#define A2C_ENTRY_IS_REAL(a2c) (!A2C_ENTRY_IS_EMPTY(a2c) && !A2C_ENTRY_IS_SENTINEL(a2c))

/* required routines for hashtable interface that we don't need for this instance */

static void
hashtable_coarse_init_internal_custom(dcontext_t *dcontext, coarse_table_t *htable)
{ /* nothing */
}

static void
hashtable_coarse_resized_custom(dcontext_t *dcontext, coarse_table_t *htable,
                                uint old_capacity, app_to_cache_t *old_table,
                                app_to_cache_t *old_table_unaligned, uint old_ref_count,
                                uint old_table_flags)
{ /* nothing */
}

#ifdef DEBUG
static void
hashtable_coarse_study_custom(dcontext_t *dcontext, coarse_table_t *htable,
                              uint entries_inc /*amnt table->entries was pre-inced*/)
{ /* nothing */
}
#endif

static void
hashtable_coarse_free_entry(dcontext_t *dcontext, coarse_table_t *htable,
                            app_to_cache_t entry)
{
    /* nothing to do, data is inlined */
}

/* i#670: To handle differing app addresses from different module
 * bases across different executions, we store the persist-time abs
 * addrs in our tables and always shift on lookup.  For frozen exit
 * stubs, we have the frozen fcache return convert to a cur-base abs
 * addr so this shift will then restore to persist-time.
 *
 * XXX: this is needed for -persist_trust_textrel, but for Windows
 * bases will only be different when we need to apply relocations, and
 * there we could just add an extra relocation per exit stub, which
 * would end up being lower overhead for long-running apps, but may be
 * higher than the overhead of shifting for short-running.  For now,
 * I'm enabling this as cross-platform, and we can do perf
 * measurements later if desired.
 *
 * Storing persist-time abs addr versus storing module offsets: note
 * that whatever we do we want a frozen-unit-only solution so we don't
 * want to, say, always store module offsets for non-frozen units.
 * This is because we support mixing coarse and fine and we have to
 * use abs addrs in fine tables b/c they're not per-module.  We could
 * store offsets in frozen only, but then we'd have to do extra work
 * when freezing.  Plus, by storing absolute, when the module base
 * lines up we can avoid the shift on the exit stubs (although today
 * we only avoid exit stub overhead for trace heads when the base
 * matches).
 */
static inline app_to_cache_t
coarse_lookup_internal(dcontext_t *dcontext, app_pc tag, coarse_table_t *table)
{
    /* note that for mod_shift we don't need to compare to bounds b/c
     * this is a table for this module only
     */
    app_to_cache_t a2c =
        hashtable_coarse_lookup(dcontext, (ptr_uint_t)(tag + table->mod_shift), table);
    if (table->mod_shift != 0 && A2C_ENTRY_IS_REAL(a2c))
        a2c.app -= table->mod_shift;
    return a2c;
}
/* I would #define hashtable_coarse_lookup as DO_NOT_USE but we have to use for
 * pclookup
 */

/* Pass 0 for the initial capacity to use the default.
 * Initial capacities are number of entires and NOT bits in mask or anything.
 */
void
fragment_coarse_htable_create(coarse_info_t *info, uint init_capacity,
                              uint init_th_capacity)
{
    coarse_table_t *th_htable;
    coarse_table_t *htable;
    uint init_size;
    ASSERT(SHARED_FRAGMENTS_ENABLED());

    /* Case 9537: If we start the new table small and grow it we have large
     * collision chains as we map the lower address space of a large table into
     * the same lower fraction of a smaller table, so we create our table fully
     * sized up front.
     */
    if (init_capacity != 0) {
        init_size = hashtable_bits_given_entries(init_capacity,
                                                 DYNAMO_OPTION(coarse_htable_load));
    } else
        init_size = INIT_HTABLE_SIZE_COARSE;
    LOG(GLOBAL, LOG_FRAGMENT, 2, "Coarse %s htable %d capacity => %d bits\n",
        info->module, init_capacity, init_size);
    htable =
        NONPERSISTENT_HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, coarse_table_t, ACCT_FRAG_TABLE);
    hashtable_coarse_init(
        GLOBAL_DCONTEXT, htable, init_size, DYNAMO_OPTION(coarse_htable_load),
        (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
        HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
            HASHTABLE_RELAX_CLUSTER_CHECKS _IF_DEBUG("coarse htable"));
    htable->mod_shift = 0;
    info->htable = (void *)htable;

    /* We could create th_htable lazily independently of htable but not worth it */
    if (init_th_capacity != 0) {
        init_size = hashtable_bits_given_entries(init_th_capacity,
                                                 DYNAMO_OPTION(coarse_th_htable_load));
    } else
        init_size = INIT_HTABLE_SIZE_COARSE_TH;
    LOG(GLOBAL, LOG_FRAGMENT, 2, "Coarse %s th htable %d capacity => %d bits\n",
        info->module, init_th_capacity, init_size);
    th_htable =
        NONPERSISTENT_HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, coarse_table_t, ACCT_FRAG_TABLE);
    hashtable_coarse_init(
        GLOBAL_DCONTEXT, th_htable, init_size, DYNAMO_OPTION(coarse_th_htable_load),
        (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
        HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
            HASHTABLE_RELAX_CLUSTER_CHECKS _IF_DEBUG("coarse th htable"));
    th_htable->mod_shift = 0;
    /* We give th table a lower lock rank for coarse_body_from_htable_entry().
     * FIXME: add param to init() that takes in lock rank?
     */
    ASSIGN_INIT_READWRITE_LOCK_FREE(th_htable->rwlock, coarse_th_table_rwlock);
    info->th_htable = (void *)th_htable;
}

/* Adds all entries from stable into dtable, offsetting by dst_cache_offset,
 * which is the offset from dst->cache_start_pc at which
 * the src cache has been placed.
 */
static void
fragment_coarse_htable_merge_helper(dcontext_t *dcontext, coarse_info_t *dst,
                                    coarse_table_t *dtable, coarse_info_t *src,
                                    coarse_table_t *stable, ssize_t dst_cache_offset)
{
    app_to_cache_t a2c, look_a2c;
    uint i;
    /* assumption: dtable is private to this thread and so does not need synch */
    DODEBUG({ dtable->is_local = true; });
    TABLE_RWLOCK(stable, read, lock);
    for (i = 0; i < stable->capacity; i++) {
        a2c = stable->table[i];
        if (A2C_ENTRY_IS_REAL(a2c)) {
            look_a2c = coarse_lookup_internal(dcontext, a2c.app, dtable);
            if (A2C_ENTRY_IS_EMPTY(look_a2c)) {
                a2c.cache += dst_cache_offset;
                if (!dst->frozen) { /* adjust absolute value */
                    ASSERT_NOT_TESTED();
                    a2c.cache += (dst->cache_start_pc - src->cache_start_pc);
                }
                hashtable_coarse_add(dcontext, a2c, dtable);
            } else {
                /* Our merging-with-dups strategy requires that we not
                 * merge them in this early
                 */
                ASSERT_NOT_REACHED();
            }
        }
    }
    TABLE_RWLOCK(stable, read, unlock);
    DODEBUG({ dtable->is_local = false; });
}

/* Merges the main and th htables from info1 and info2 into new htables for dst.
 * If !add_info2, makes room for but does not add entries from info2.
 * If !add_th_htable, creates but does not add entries to dst->th_htable.
 * FIXME: we can't use hashtable_coarse_merge() b/c we don't want to add
 * entries from the 2nd table, and we do custom mangling of entries when adding.
 * Is it worth parametrizing hashtable_coarse_merge() to share anything?
 */
void
fragment_coarse_htable_merge(dcontext_t *dcontext, coarse_info_t *dst,
                             coarse_info_t *info1, coarse_info_t *info2, bool add_info2,
                             bool add_th_htable)
{
    coarse_table_t *thht_dst, *thht1, *thht2;
    coarse_table_t *ht_dst, *ht1, *ht2;
    uint merged_entries = 0;

    ASSERT(SHARED_FRAGMENTS_ENABLED());
    ASSERT(info1 != NULL && info2 != NULL);
    ht1 = (coarse_table_t *)info1->htable;
    ht2 = (coarse_table_t *)info2->htable;
    thht1 = (coarse_table_t *)info1->th_htable;
    thht2 = (coarse_table_t *)info2->th_htable;
    ASSERT(dst != NULL && dst->htable == NULL && dst->th_htable == NULL);

    /* We go to the trouble of determining non-dup total entries to
     * avoid repeatedly increasing htable size on merges and hitting
     * collision asserts.  FIXME: should we shrink afterward instead?
     * Or just ignore collision asserts until at full size?
     */
    merged_entries = hashtable_coarse_num_unique_entries(dcontext, ht1, ht2);
    STATS_ADD(coarse_merge_dups, ht1->entries + ht2->entries - merged_entries);
    LOG(THREAD, LOG_FRAGMENT, 2, "Merging %s: %d + %d => %d (%d unique) entries\n",
        info1->module, ht1->entries, ht2->entries, ht1->entries + ht2->entries,
        merged_entries);

    /* We could instead copy ht1 and then add ht2; for simplicity we re-use
     * our create-empty routine and add both
     */
    fragment_coarse_htable_create(dst, merged_entries,
                                  /* Heuristic to never over-size, yet try to avoid
                                   * collision asserts while resizing the table;
                                   * FIXME: if we shrink the main htable afterward
                                   * could do the same here and start at sum of
                                   * entries */
                                  MAX(thht1->entries, thht2->entries));
    ht_dst = (coarse_table_t *)dst->htable;
    thht_dst = (coarse_table_t *)dst->th_htable;
    ASSERT(ht_dst != NULL && thht_dst != NULL);

    /* For now we only support frozen tables; else will have to change
     * the offsets to be stubs for main table and cache for th table
     */
    ASSERT(info1->frozen && info2->frozen);
    fragment_coarse_htable_merge_helper(dcontext, dst, ht_dst, info1, ht1, 0);
    if (add_info2) {
        fragment_coarse_htable_merge_helper(dcontext, dst, ht_dst, info2, ht2,
                                            info1->cache_end_pc - info1->cache_start_pc);
    }
    if (add_th_htable) {
        ASSERT_NOT_TESTED();
        fragment_coarse_htable_merge_helper(dcontext, dst, thht_dst, info1, thht1, 0);
        fragment_coarse_htable_merge_helper(dcontext, dst, thht_dst, info2, thht2,
                                            /* stubs_end_pc is not allocated end */
                                            info1->mmap_pc + info1->mmap_size -
                                                info1->stubs_start_pc);
    }
}

#ifndef DEBUG
/* not in header file unless debug, in which case it's static */
static void
coarse_body_from_htable_entry(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                              cache_pc res, cache_pc *stub_pc_out /*OUT*/,
                              cache_pc *body_pc_out /*OUT*/);
#endif

static void
study_and_free_coarse_htable(coarse_info_t *info, coarse_table_t *htable,
                             bool never_persisted _IF_DEBUG(const char *name))
{
    LOG(GLOBAL, LOG_FRAGMENT, 1, "Coarse %s %s hashtable stats:\n", info->module, name);
    DOLOG(1, LOG_FRAGMENT | LOG_STATS,
          { hashtable_coarse_load_statistics(GLOBAL_DCONTEXT, htable); });
    DODEBUG({ hashtable_coarse_study(GLOBAL_DCONTEXT, htable, 0 /*table consistent*/); });
    DOLOG(3, LOG_FRAGMENT, { hashtable_coarse_dump_table(GLOBAL_DCONTEXT, htable); });
    /* Only raise deletion events if client saw creation events: so no persisted
     * units
     */
    if (!info->persisted && htable == info->htable && dr_fragment_deleted_hook_exists()) {
        app_to_cache_t a2c;
        uint i;
        dcontext_t *dcontext = get_thread_private_dcontext();
        cache_pc body = NULL;
        TABLE_RWLOCK(htable, read, lock);
        for (i = 0; i < htable->capacity; i++) {
            a2c = htable->table[i];
            if (A2C_ENTRY_IS_REAL(a2c)) {
                if (info->frozen)
                    body = a2c.cache;
                else { /* make sure not an entrance stub w/ no body */
                    coarse_body_from_htable_entry(dcontext, info, a2c.app, a2c.cache,
                                                  NULL, &body);
                }
                if (body != NULL) {
                    instrument_fragment_deleted(get_thread_private_dcontext(), a2c.app,
                                                FRAGMENT_COARSE_WRAPPER_FLAGS);
                }
            }
        }
        TABLE_RWLOCK(htable, read, unlock);
    }
    /* entries are inlined so nothing external to free */
    if (info->persisted && !never_persisted) {
        /* ensure won't try to free (part of mmap) */
        ASSERT(htable->table_unaligned == NULL);
    }
    hashtable_coarse_free(GLOBAL_DCONTEXT, htable);
    NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, htable, coarse_table_t,
                                 ACCT_FRAG_TABLE);
}

void
fragment_coarse_free_entry_pclookup_table(dcontext_t *dcontext, coarse_info_t *info)
{
    if (info->pclookup_htable != NULL) {
        ASSERT(DYNAMO_OPTION(coarse_pclookup_table));
        study_and_free_coarse_htable(info, (coarse_table_t *)info->pclookup_htable,
                                     true /*never persisted*/ _IF_DEBUG("pclookup"));
        info->pclookup_htable = NULL;
    }
}

void
fragment_coarse_htable_free(coarse_info_t *info)
{
    ASSERT_OWN_MUTEX(!info->is_local, &info->lock);
    if (info->htable == NULL) {
        /* lazily initialized, so common to have empty units */
        ASSERT(info->th_htable == NULL);
        ASSERT(info->pclookup_htable == NULL);
        return;
    }
    study_and_free_coarse_htable(info, (coarse_table_t *)info->htable,
                                 false _IF_DEBUG("main"));
    info->htable = NULL;
    study_and_free_coarse_htable(info, (coarse_table_t *)info->th_htable,
                                 false _IF_DEBUG("tracehead"));
    info->th_htable = NULL;
    if (info->pclookup_last_htable != NULL) {
        generic_hash_destroy(GLOBAL_DCONTEXT, info->pclookup_last_htable);
        info->pclookup_last_htable = NULL;
    }
    fragment_coarse_free_entry_pclookup_table(GLOBAL_DCONTEXT, info);
}

uint
fragment_coarse_num_entries(coarse_info_t *info)
{
    coarse_table_t *htable;
    ASSERT(info != NULL);
    htable = (coarse_table_t *)info->htable;
    if (htable == NULL)
        return 0;
    return htable->entries;
}

/* Add coarse fragment represented by wrapper f to the hashtable
 * for unit info.
 */
void
fragment_coarse_add(dcontext_t *dcontext, coarse_info_t *info, app_pc tag, cache_pc cache)
{
    coarse_table_t *htable;
    app_to_cache_t a2c = { tag, cache };

    ASSERT(info != NULL && info->htable != NULL);
    htable = (coarse_table_t *)info->htable;

    DOCHECK(1, {
        /* We have lock rank order problems b/c this lookup may acquire
         * the th table read lock, and we can't split its rank from
         * the main table's.  So we live w/ a racy assert that could
         * have false positives or negatives.
         */
        cache_pc stub;
        cache_pc body;
        /* OK to have dup entries for entrance stubs in other units,
         * or a stub already present for a trace head */
        fragment_coarse_lookup_in_unit(dcontext, info, tag, &stub, &body);
        ASSERT(body == NULL);
        ASSERT(stub == NULL ||
               coarse_is_trace_head_in_own_unit(dcontext, tag, stub,
                                                /* case 10876: pass what we're adding */
                                                (ptr_uint_t)cache + info->cache_start_pc,
                                                true, info));
        /* There can only be one body.  But similarly to above,
         * fragment_coarse_lookup may look in the secondary unit, so
         * we can't do this lookup holding the write lock.
         */
        if (!coarse_is_entrance_stub(cache)) {
            coarse_info_t *xinfo = get_executable_area_coarse_info(tag);
            fragment_t *f;
            ASSERT(xinfo != NULL);
            /* If an official unit, tag should not exist in any other official unit */
            ASSERT((info != xinfo && info != xinfo->non_frozen) ||
                   fragment_coarse_lookup(dcontext, tag) == NULL);
            f = fragment_lookup(dcontext, tag);
            /* ok for trace to shadow coarse trace head */
            ASSERT(f == NULL || TEST(FRAG_IS_TRACE, f->flags));
        }
    });
    TABLE_RWLOCK(htable, write, lock);
    /* Table is not an ibl table so we ignore resize return value */
    hashtable_coarse_add(dcontext, a2c, htable);
    TABLE_RWLOCK(htable, write, unlock);

#ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study)) {
        ASSERT_NOT_IMPLEMENTED(false && "need to pass f in to add_shared_block");
    }
#endif
}

/* Returns the body pc of the coarse trace head fragment corresponding to tag, or
 * NULL if not found.  Caller must hold the th table's read or write lock!
 */
static cache_pc
fragment_coarse_th_lookup(dcontext_t *dcontext, coarse_info_t *info, app_pc tag)
{
    cache_pc res = NULL;
    app_to_cache_t a2c;
    coarse_table_t *htable;
    ASSERT(info != NULL);
    ASSERT(info->htable != NULL);
    htable = (coarse_table_t *)info->th_htable;
    ASSERT(TABLE_PROTECTED(htable));
    a2c = coarse_lookup_internal(dcontext, tag, htable);
    if (!A2C_ENTRY_IS_EMPTY(a2c)) {
        ASSERT(BOOLS_MATCH(info->frozen, info->stubs_start_pc != NULL));
        /* for frozen, th_htable only holds stubs */
        res = ((ptr_uint_t)a2c.cache) + info->stubs_start_pc;
    }
    return res;
}

/* Performs two actions while holding the trace head table's write lock,
 * making them atomic (solving the race in case 8795):
 * 1) unlinks the coarse fragment's entrance pc and points it at the
 *    trace head exit routine;
 * 2) adds the coarse fragment's body pc to the trace head hashtable.
 *
 * Case 8628: if we split the main htable into stubs and bodies then
 * we can eliminate the th htable as it will be part of the body table.
 *
 * We could merge this info in to the thcounter table, and assume that
 * most trace heads are coarse so we're not wasting much memory with
 * the extra field.  But then we have to walk entire stub table on
 * cache flush and individually clear body pcs from monitor table, so
 * better to have own th table?
 */
void
fragment_coarse_th_unlink_and_add(dcontext_t *dcontext, app_pc tag, cache_pc stub_pc,
                                  cache_pc body_pc)
{
    ASSERT(stub_pc != NULL);
    if (body_pc != NULL) {
        /* trace head is in this unit, so we have to add it to our th htable */
        coarse_info_t *info = get_fcache_coarse_info(body_pc);
        coarse_table_t *th_htable;
        app_to_cache_t a2c = { tag, body_pc };
        ASSERT(info != NULL && info->th_htable != NULL);
        ASSERT(!info->frozen);
        th_htable = (coarse_table_t *)info->th_htable;
        TABLE_RWLOCK(th_htable, write, lock);
        ASSERT(fragment_coarse_th_lookup(dcontext, info, tag) == NULL);
        unlink_entrance_stub(dcontext, stub_pc, FRAG_IS_TRACE_HEAD, info);
        /* Table is not an ibl table so we ignore resize return value */
        hashtable_coarse_add(dcontext, a2c, th_htable);
        TABLE_RWLOCK(th_htable, write, unlock);
        LOG(THREAD, LOG_FRAGMENT, 4,
            "adding to coarse th table for %s: " PFX "->" PFX "\n", info->module, tag,
            body_pc);
    } else {
        /* ensure lives in another unit */
        ASSERT(fragment_coarse_lookup(dcontext, tag) != stub_pc);
        unlink_entrance_stub(dcontext, stub_pc, FRAG_IS_TRACE_HEAD, NULL);
    }
}

/* Only use when building up a brand-new table.  Otherwise use
 * fragment_coarse_th_unlink_and_add().
 */
void
fragment_coarse_th_add(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                       cache_pc cache)
{
    coarse_table_t *th_htable;
    app_to_cache_t a2c = { tag, cache };
    ASSERT(info != NULL && info->th_htable != NULL);
    ASSERT(info->frozen); /* only used when merging units */
    th_htable = (coarse_table_t *)info->th_htable;
    TABLE_RWLOCK(th_htable, write, lock);
    ASSERT(fragment_coarse_th_lookup(dcontext, info, tag) == NULL);
    /* Table is not an ibl table so we ignore resize return value */
    hashtable_coarse_add(dcontext, a2c, th_htable);
    TABLE_RWLOCK(th_htable, write, unlock);
}

/* The input here is the result of a lookup in the main htable.
 * For a frozen unit, this actually looks up the stub pc since res is
 * always the body pc.
 * For a non-frozen unit this determines where to obtain the body pc.
 * FIXME: case 8628 will simplify this whole thing
 */
/* exported only for assert in push_pending_freeze() */
IF_DEBUG_ELSE(, static)
void
coarse_body_from_htable_entry(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                              cache_pc res, cache_pc *stub_pc_out /*OUT*/,
                              cache_pc *body_pc_out /*OUT*/)
{
    cache_pc stub_pc = NULL, body_pc = NULL;
    /* Should be passing absolute pc, not offset */
    ASSERT(!info->frozen || res == NULL || res >= info->cache_start_pc);
    if (info->frozen) {
        /* We still need the stub table for lazy linking and for
           shifting links from a trace head to a trace.
         */
        body_pc = res;
        if (stub_pc_out != NULL) {
            TABLE_RWLOCK((coarse_table_t *)info->th_htable, read, lock);
            stub_pc = fragment_coarse_th_lookup(dcontext, info, tag);
            TABLE_RWLOCK((coarse_table_t *)info->th_htable, read, unlock);
        }
    } else {
        /* In a non-frozen unit, htable entries are always stubs */
        DOCHECK(CHKLVL_DEFAULT + 1, { ASSERT(coarse_is_entrance_stub(res)); });
        stub_pc = res;
        if (body_pc_out != NULL) {
            /* keep the th table entry and stub link status linked atomically */
            TABLE_RWLOCK((coarse_table_t *)info->th_htable, read, lock);
            body_pc = fragment_coarse_th_lookup(dcontext, info, tag);
            if (body_pc != NULL) {
                /* We can't just check coarse_is_trace_head(res) because a
                 * shadowed trace head is directly linked to its trace.  The
                 * trace has lookup order precedence, but the head must show up
                 * if asked about here!
                 */
                /* FIXME: we could have a flags OUT param and set FRAG_IS_TRACE_HEAD
                 * to help out fragment_lookup_fine_and_coarse*().
                 */
            } else {
                if (entrance_stub_linked(res, info)) {
                    /* We only want to set body_pc if it is present in this unit */
                    cache_pc tgt = entrance_stub_jmp_target(res);
                    if (get_fcache_coarse_info(tgt) == info)
                        body_pc = tgt;
                    else
                        body_pc = NULL;
                } else
                    body_pc = NULL;
                DOCHECK(CHKLVL_DEFAULT + 1, {
                    ASSERT(!coarse_is_trace_head(res) ||
                           /* allow targeting trace head in another unit */
                           body_pc == NULL);
                });
            }
            TABLE_RWLOCK((coarse_table_t *)info->th_htable, read, unlock);
        }
    }
    if (stub_pc_out != NULL)
        *stub_pc_out = stub_pc;
    if (body_pc_out != NULL)
        *body_pc_out = body_pc;
}

/* Coarse fragments have two entrance points: the actual fragment
 * body, and the entrance stub that is used for indirection and
 * convenient incremental building for intra-unit non-frozen linking
 * as well as always used at the source end for inter-unit linking.
 * This routine returns both.  If the stub_pc returned is non-NULL, it
 * only indicates that there is an outgoing link from a fragment
 * present in this unit that targets the queried tag (and for
 * intra-unit links in a frozen unit, such a link may exist but
 * stub_pc will be NULL as the entrance stub indirection will have
 * been replaced with a direct link).  The fragment corresponding to
 * the tag is only present in this unit if the body_pc returned is
 * non-NULL.
 */
void
fragment_coarse_lookup_in_unit(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                               /* FIXME: have separate type for stub pc vs body pc? */
                               cache_pc *stub_pc_out /*OUT*/,
                               cache_pc *body_pc_out /*OUT*/)
{
    cache_pc res = NULL;
    cache_pc stub_pc = NULL, body_pc = NULL;
    app_to_cache_t a2c;
    coarse_table_t *htable;
    ASSERT(info != NULL);
    if (info->htable == NULL) /* not initialized yet, so no code there */
        goto coarse_lookup_return;
    htable = (coarse_table_t *)info->htable;
    TABLE_RWLOCK(htable, read, lock);
    a2c = coarse_lookup_internal(dcontext, tag, htable);
    if (!A2C_ENTRY_IS_EMPTY(a2c)) {
        LOG(THREAD, LOG_FRAGMENT, 5,
            "%s: %s %s tag=" PFX " => app=" PFX " cache=" PFX "\n", __FUNCTION__,
            info->module, info->frozen ? "frozen" : "", tag, a2c.app, a2c.cache);
        ASSERT(BOOLS_MATCH(info->frozen, info->cache_start_pc != NULL));
        /* for frozen, htable only holds body pc */
        res = ((ptr_uint_t)a2c.cache) + info->cache_start_pc;
    }
    if (res != NULL)
        coarse_body_from_htable_entry(dcontext, info, tag, res, &stub_pc, &body_pc);
    else if (info->frozen) /* need a separate lookup for stub */
        coarse_body_from_htable_entry(dcontext, info, tag, res, &stub_pc, &body_pc);
    TABLE_RWLOCK(htable, read, unlock);
    /* cannot have both a shared coarse and shared fine bb for same tag
     * (can have shared trace shadowing shared coarse trace head bb, or
     * private bb shadowing shared coarse bb)
     */
    ASSERT(body_pc == NULL || fragment_lookup_shared_bb(dcontext, tag) == NULL);
coarse_lookup_return:
    if (stub_pc_out != NULL)
        *stub_pc_out = stub_pc;
    if (body_pc_out != NULL)
        *body_pc_out = body_pc;
}

/* Returns the body pc of the coarse fragment corresponding to tag, or
 * NULL if not found
 */
cache_pc
fragment_coarse_lookup(dcontext_t *dcontext, app_pc tag)
{
    cache_pc res = NULL;
    coarse_info_t *info = get_executable_area_coarse_info(tag);
    /* We must check each unit in turn */
    while (info != NULL) { /* loop over primary and secondary unit */
        fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL, &res);
        if (res != NULL)
            return res;
        ASSERT(info->frozen || info->non_frozen == NULL);
        info = info->non_frozen;
        ASSERT(info == NULL || !info->frozen);
    }
    return NULL;
}

/* It's up to the caller to hold locks preventing simultaneous writes to wrapper */
void
fragment_coarse_wrapper(fragment_t *wrapper, app_pc tag, cache_pc body_pc)
{
    ASSERT(wrapper != NULL);
    if (wrapper == NULL)
        return;
    ASSERT(tag != NULL);
    ASSERT(body_pc != NULL);
    /* FIXME: fragile to rely on other routines not inspecting other
     * fields of fragment_t -- but setting to 0 perhaps no better than garbage
     * in that case.  We do need to set prefix_size, incoming_stubs, and
     * {next,prev}_vmarea to NULL, for sure.
     */
    memset(wrapper, 0, sizeof(*wrapper));
    wrapper->tag = tag;
    wrapper->start_pc = body_pc;
    wrapper->flags = FRAGMENT_COARSE_WRAPPER_FLAGS;
    /* We don't have stub pc so can't fill in FRAG_IS_TRACE_HEAD -- so we rely
     * on callers passing src info to fragment_lookup_fine_and_coarse()
     */
}

/* If finds a coarse fragment for tag, returns wrapper; else returns NULL */
fragment_t *
fragment_coarse_lookup_wrapper(dcontext_t *dcontext, app_pc tag, fragment_t *wrapper)
{
    cache_pc coarse;
    ASSERT(wrapper != NULL);
    coarse = fragment_coarse_lookup(dcontext, tag);
    if (coarse != NULL) {
        fragment_coarse_wrapper(wrapper, tag, coarse);
        return wrapper;
    }
    return NULL;
}

/* Takes in last_exit in order to mark trace headness.
 * FIXME case 8600: should we replace all other lookups with this one?
 * Fragile to only have select callers use this and everyone else
 * ignore coarse fragments...
 */
fragment_t *
fragment_lookup_fine_and_coarse(dcontext_t *dcontext, app_pc tag, fragment_t *wrapper,
                                linkstub_t *last_exit)
{
    fragment_t *res = fragment_lookup(dcontext, tag);
    if (DYNAMO_OPTION(coarse_units)) {
        ASSERT(wrapper != NULL);
        if (res == NULL) {
            res = fragment_coarse_lookup_wrapper(dcontext, tag, wrapper);
            /* FIXME: no way to know if source is a fine fragment! */
            if (res != NULL && last_exit == get_coarse_trace_head_exit_linkstub())
                res->flags |= FRAG_IS_TRACE_HEAD;
        } else {
            /* cannot have both coarse and fine shared bb for same tag
             * (can have shared trace shadowing shared coarse trace head bb,
             * or private bb with same tag)
             */
            ASSERT(TEST(FRAG_IS_TRACE, res->flags) || !TEST(FRAG_SHARED, res->flags) ||
                   fragment_coarse_lookup(dcontext, tag) == NULL);
        }
    }
    return res;
}

/* Takes in last_exit in order to mark trace headness.
 * FIXME: should we replace all other same_sharing lookups with this one?
 * Fragile to only have select callers use this and everyone else
 * ignore coarse fragments...
 */
fragment_t *
fragment_lookup_fine_and_coarse_sharing(dcontext_t *dcontext, app_pc tag,
                                        fragment_t *wrapper, linkstub_t *last_exit,
                                        uint share_flags)
{
    fragment_t *res = fragment_lookup_same_sharing(dcontext, tag, share_flags);
    if (DYNAMO_OPTION(coarse_units) && TEST(FRAG_SHARED, share_flags)) {
        ASSERT(wrapper != NULL);
        if (res == NULL) {
            res = fragment_coarse_lookup_wrapper(dcontext, tag, wrapper);
            if (res != NULL && last_exit == get_coarse_trace_head_exit_linkstub())
                res->flags |= FRAG_IS_TRACE_HEAD;
        }
    }
    return res;
}

/* Returns the owning unit of f (versus get_executable_area_coarse_info(f->tag)
 * which may return a frozen unit that must be checked for f along with its
 * secondary unfrozen unit).
 */
coarse_info_t *
get_fragment_coarse_info(fragment_t *f)
{
    /* We have multiple potential units per vmarea region, so we use the fcache
     * pc to get the unambiguous owning unit
     */
    if (!TEST(FRAG_COARSE_GRAIN, f->flags))
        return NULL;
    ASSERT(FCACHE_ENTRY_PC(f) != NULL);
    return get_fcache_coarse_info(FCACHE_ENTRY_PC(f));
}

/* Pass in info if you know it; else this routine will look it up.
 * Checks for stub targeting a trace head, or targeting a trace thus
 * indicating that this is a shadowed trace head.
 * If body_in or info_in is NULL, this routine will look them up.
 */
bool
coarse_is_trace_head_in_own_unit(dcontext_t *dcontext, app_pc tag, cache_pc stub,
                                 cache_pc body_in, bool body_valid,
                                 coarse_info_t *info_in)
{
    coarse_info_t *info = info_in;
    ASSERT(stub != NULL);
    if (coarse_is_trace_head(stub))
        return true;
    if (info == NULL)
        info = get_stub_coarse_info(stub);
    if (info == NULL)
        return false;
    /* If a coarse stub is linked to a fine fragment and there exists
     * a body for that target tag in the same unit as the stub, we assume that
     * we have a shadowed coarse trace head.
     */
    if (entrance_stub_linked(stub, info) &&
        /* Target is fine if no info for it */
        get_fcache_coarse_info(entrance_stub_jmp_target(stub)) == NULL) {
        cache_pc body = body_in;
        if (!body_valid) {
            ASSERT(body == NULL);
            fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL, &body);
        }
        if (body != NULL)
            return true;
    }
    return false;
}

/* Returns whether an entry exists. */
bool
fragment_coarse_replace(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                        cache_pc new_value)
{
    app_to_cache_t old_entry = { tag, NULL /*doesn't matter*/ };
    app_to_cache_t new_entry = { tag, new_value };
    coarse_table_t *htable;
    bool res = false;
    ASSERT(info != NULL && info->htable != NULL);
    htable = (coarse_table_t *)info->htable;
    TABLE_RWLOCK(htable, read, lock);
    res = hashtable_coarse_replace(old_entry, new_entry, info->htable);
    TABLE_RWLOCK(htable, read, unlock);
    return res;
}

/**************************************************
 * PC LOOKUP
 */

/* Table for storing results of prior coarse pclookups (i#658) */
#define PCLOOKUP_LAST_HTABLE_INIT_SIZE 6 /*should remain small*/

/* Alarm signals can result in many pclookups from a variety of places
 * (unlike usual DGC patterns) so we bound the size of the table.
 * We want to err on the side of using more memory, since failing to
 * cache all the instrs that are writing DGC can result in 2x slowdowns.
 * It's not worth fancy replacement: we just clear the table if it gets
 * really large and start over.  Remember that this is per-coarse-unit.
 */
#define PCLOOKUP_LAST_HTABLE_MAX_ENTRIES 8192

typedef struct _pclookup_last_t {
    app_pc tag;
    cache_pc entry;
} pclookup_last_t;

static void
pclookup_last_free(dcontext_t *dcontext, void *last)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, last, pclookup_last_t, ACCT_FRAG_TABLE, PROTECTED);
}

/* Returns the tag for the coarse fragment whose body contains pc.
 * If that returned tag != NULL, also returns the body pc in the optional OUT param.
 * FIXME: verify not called too often: watch the kstat.
 */
app_pc
fragment_coarse_pclookup(dcontext_t *dcontext, coarse_info_t *info, cache_pc pc,
                         /*OUT*/ cache_pc *body_out)
{
    app_to_cache_t a2c;
    coarse_table_t *htable;
    generic_table_t *pc_htable;
    pclookup_last_t *last;
    cache_pc body_pc;
    ssize_t closest_distance = SSIZE_T_MAX;
    app_pc closest = NULL;
    uint i;
    ASSERT(info != NULL);
    if (info->htable == NULL)
        return NULL;
    KSTART(coarse_pclookup);
    htable = (coarse_table_t *)info->htable;

    if (info->pclookup_last_htable == NULL) {
        /* lazily allocate table of all pclookups to avoid htable walk
         * on frequent codemod instrs (i#658).
         * I tried using a small array instead but chrome v8 needs at least
         * 12 entries, and rather than LFU or LRU to avoid worst-case,
         * it seems reasonable to simply store all lookups.
         * Then the worst case is some extra memory, not 4x slowdowns.
         */
        d_r_mutex_lock(&info->lock);
        if (info->pclookup_last_htable == NULL) {
            /* coarse_table_t isn't quite enough b/c we need the body pc,
             * which would require an extra lookup w/ coarse_table_t
             */
            pc_htable =
                generic_hash_create(GLOBAL_DCONTEXT, PCLOOKUP_LAST_HTABLE_INIT_SIZE,
                                    80 /* load factor: not perf-critical */,
                                    HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                        HASHTABLE_RELAX_CLUSTER_CHECKS,
                                    pclookup_last_free _IF_DEBUG("pclookup last table"));
            /* Only when fully initialized can we set it, as we hold no lock for it */
            info->pclookup_last_htable = (void *)pc_htable;
        }
        d_r_mutex_unlock(&info->lock);
    }

    pc_htable = (generic_table_t *)info->pclookup_last_htable;
    ASSERT(pc_htable != NULL);
    TABLE_RWLOCK(pc_htable, read, lock);
    last = (pclookup_last_t *)generic_hash_lookup(GLOBAL_DCONTEXT, pc_htable,
                                                  (ptr_uint_t)pc);
    if (last != NULL) {
        closest = last->tag;
        ASSERT(pc >= last->entry);
        closest_distance = pc - last->entry;
    }
    TABLE_RWLOCK(pc_htable, read, unlock);

    if (closest == NULL) {
        /* do the htable walk */
        TABLE_RWLOCK(htable, read, lock);
        for (i = 0; i < htable->capacity; i++) {
            a2c = htable->table[i];
            /* must check for sentinel */
            if (A2C_ENTRY_IS_REAL(a2c)) {
                a2c.app -= htable->mod_shift;
                ASSERT(BOOLS_MATCH(info->frozen, info->cache_start_pc != NULL));
                /* for frozen, htable only holds body pc */
                a2c.cache += (ptr_uint_t)info->cache_start_pc;
                /* We have no body length so we must walk entire table */
                coarse_body_from_htable_entry(dcontext, info, a2c.app, a2c.cache, NULL,
                                              &body_pc);
                if (body_pc != NULL && body_pc <= pc &&
                    (pc - body_pc) < closest_distance) {
                    closest_distance = pc - body_pc;
                    closest = a2c.app;
                }
            }
        }
        if (closest != NULL) {
            /* Update the cache of results. Note that since this is coarse we
             * don't have to do anything special to invalidate on codemod as the
             * whole coarse unit will be thrown out.
             */
            TABLE_RWLOCK(pc_htable, write, lock);
            /* Check for race (i#1191) */
            last = (pclookup_last_t *)generic_hash_lookup(GLOBAL_DCONTEXT, pc_htable,
                                                          (ptr_uint_t)pc);
            if (last != NULL) {
                closest = last->tag;
                ASSERT(pc >= last->entry);
                closest_distance = pc - last->entry;
            } else {
                last = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, pclookup_last_t, ACCT_FRAG_TABLE,
                                       PROTECTED);
                last->tag = closest;
                last->entry = pc - closest_distance;

                if (pc_htable->entries >= PCLOOKUP_LAST_HTABLE_MAX_ENTRIES) {
                    /* See notes above: we don't want an enormous table.
                     * We just clear rather than a fancy replacement policy.
                     */
                    generic_hash_clear(GLOBAL_DCONTEXT, pc_htable);
                }

                generic_hash_add(GLOBAL_DCONTEXT, pc_htable, (ptr_uint_t)pc,
                                 (void *)last);
                STATS_INC(coarse_pclookup_cached);
            }
            TABLE_RWLOCK(pc_htable, write, unlock);
        }
        TABLE_RWLOCK(htable, read, unlock);
    }

    if (body_out != NULL)
        *body_out = pc - closest_distance;
    KSTOP(coarse_pclookup);
    LOG(THREAD, LOG_FRAGMENT, 4, "%s: " PFX " => " PFX "\n", __FUNCTION__, pc, closest);
    return closest;
}

/* Creates a reverse lookup table.  For a non-frozen unit, the caller should only
 * do this while all threads are suspended, and should free the table before
 * resuming other threads.
 */
void
fragment_coarse_create_entry_pclookup_table(dcontext_t *dcontext, coarse_info_t *info)
{
    app_to_cache_t main_a2c;
    app_to_cache_t pc_a2c;
    coarse_table_t *main_htable;
    coarse_table_t *pc_htable;
    cache_pc body_pc;
    uint i;
    ASSERT(info != NULL);
    if (info->htable == NULL)
        return;
    if (info->pclookup_htable == NULL) {
        d_r_mutex_lock(&info->lock);
        if (info->pclookup_htable == NULL) {
            /* set up reverse lookup table */
            main_htable = (coarse_table_t *)info->htable;
            pc_htable = NONPERSISTENT_HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, coarse_table_t,
                                                      ACCT_FRAG_TABLE);
            hashtable_coarse_init(
                GLOBAL_DCONTEXT, pc_htable, main_htable->hash_bits,
                DYNAMO_OPTION(coarse_pclookup_htable_load),
                (hash_function_t)INTERNAL_OPTION(alt_hash_func), 0 /* hash_mask_offset */,
                HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                    HASHTABLE_RELAX_CLUSTER_CHECKS _IF_DEBUG("coarse pclookup htable"));
            pc_htable->mod_shift = 0;
            /* We give pc table a lower lock rank so we can add below
             * while holding the lock, though the table is still local
             * while we hold info->lock and do not write it out to its
             * info-> field, so we could update the add() and
             * deadlock-avoidance routines instead (xref case 9522).
             * FIXME: add param to init() that takes in lock rank?
             */
            ASSIGN_INIT_READWRITE_LOCK_FREE(pc_htable->rwlock,
                                            coarse_pclookup_table_rwlock);
            TABLE_RWLOCK(main_htable, read, lock);
            TABLE_RWLOCK(pc_htable, write, lock);
            for (i = 0; i < main_htable->capacity; i++) {
                main_a2c = main_htable->table[i];
                /* must check for sentinel */
                if (A2C_ENTRY_IS_REAL(main_a2c)) {
                    main_a2c.app -= main_htable->mod_shift;
                    ASSERT(BOOLS_MATCH(info->frozen, info->cache_start_pc != NULL));
                    coarse_body_from_htable_entry(
                        dcontext, info, main_a2c.app,
                        main_a2c.cache
                            /* frozen htable only holds body pc */
                            + (ptr_uint_t)info->cache_start_pc,
                        NULL, &body_pc);
                    if (body_pc != NULL) {
                        /* We can have two tags with the same cache pc, if
                         * one is a single jmp that was elided (but we only
                         * do that if -unsafe_freeze_elide_sole_ubr).
                         * It's unsafe b/c of case 9677: when translating back
                         * to app pc we will just take 1st one in linear walk of
                         * htable, which may be the wrong one!
                         */
                        pc_a2c = hashtable_coarse_lookup(dcontext, (ptr_uint_t)body_pc,
                                                         pc_htable);
                        if (A2C_ENTRY_IS_EMPTY(pc_a2c)) {
                            pc_a2c.app = body_pc;
                            pc_a2c.cache = main_a2c.app;
                            hashtable_coarse_add(dcontext, pc_a2c, pc_htable);
                        } else {
                            ASSERT(DYNAMO_OPTION(unsafe_freeze_elide_sole_ubr));
                        }
                    }
                }
            }
            TABLE_RWLOCK(pc_htable, write, unlock);
            TABLE_RWLOCK(main_htable, read, unlock);
            /* Only when fully initialized can we set it, as we hold no lock for it */
            info->pclookup_htable = (void *)pc_htable;
        }
        d_r_mutex_unlock(&info->lock);
    }
}

/* Returns the tag for the coarse fragment whose body _begins at_ pc */
app_pc
fragment_coarse_entry_pclookup(dcontext_t *dcontext, coarse_info_t *info, cache_pc pc)
{
    app_to_cache_t pc_a2c;
    coarse_table_t *pc_htable;
    cache_pc body_pc;
    app_pc res = NULL;
    ASSERT(info != NULL);
    if (info->htable == NULL)
        return NULL;
    /* FIXME: we could use this table for non-frozen if we updated it
     * when we add to main htable; for now we only support frozen use
     */
    if (!DYNAMO_OPTION(coarse_pclookup_table) ||
        /* we do create a table for non-frozen while we're freezing (i#735) */
        (!info->frozen && info->pclookup_htable == NULL)) {
        res = fragment_coarse_pclookup(dcontext, info, pc, &body_pc);
        if (body_pc == pc) {
            LOG(THREAD, LOG_FRAGMENT, 4, "%s: " PFX " => " PFX "\n", __FUNCTION__, pc,
                res);
            return res;
        } else
            return NULL;
    }
    KSTART(coarse_pclookup);

    if (info->pclookup_htable == NULL) {
        fragment_coarse_create_entry_pclookup_table(dcontext, info);
    }

    pc_htable = (coarse_table_t *)info->pclookup_htable;
    ASSERT(pc_htable != NULL);
    TABLE_RWLOCK(pc_htable, read, lock);
    pc_a2c = hashtable_coarse_lookup(dcontext, (ptr_uint_t)pc, pc_htable);
    if (!A2C_ENTRY_IS_EMPTY(pc_a2c))
        res = pc_a2c.cache;
    TABLE_RWLOCK(pc_htable, read, unlock);
    KSTOP(coarse_pclookup);
    LOG(THREAD, LOG_FRAGMENT, 4, "%s: " PFX " => " PFX "\n", __FUNCTION__, pc, res);
    return res;
}

/* case 9900: must have dynamo_all_threads_synched since we haven't resolved
 * lock rank ordering issues with the hashtable locks
 */
static void
fragment_coarse_entry_freeze(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                             pending_freeze_t *pending)
{
    app_to_cache_t frozen_a2c;
    app_to_cache_t looka2c = { 0, 0 };
    coarse_table_t *frozen_htable;
    cache_pc tgt;
    if (pending->entrance_stub) {
        frozen_htable = (coarse_table_t *)freeze_info->dst_info->th_htable;
        /* case 9900: rank order conflict with coarse_info_incoming_lock:
         * do not grab frozen_htable write lock (only need read but we were
         * grabbing write to match assert): mark is_local instead and rely
         * on dynamo_all_threads_synched
         */
        DODEBUG({ frozen_htable->is_local = true; });
        ASSERT_NOT_IMPLEMENTED(dynamo_all_threads_synched && "case 9900");
    } else {
        frozen_htable = (coarse_table_t *)freeze_info->dst_info->htable;
    }
    ASSERT_OWN_WRITE_LOCK(!frozen_htable->is_local, &frozen_htable->rwlock);
    looka2c = coarse_lookup_internal(dcontext, pending->tag, frozen_htable);
    if (A2C_ENTRY_IS_EMPTY(looka2c)) {
        frozen_a2c.app = pending->tag;
        if (pending->entrance_stub) {
            /* add inter-unit/trace-head stub to htable */
            LOG(THREAD, LOG_FRAGMENT, 4,
                "  adding pending stub " PFX "." PFX " => " PFX "\n", pending->tag,
                pending->cur_pc, freeze_info->stubs_cur_pc);
            frozen_a2c.cache =
                (cache_pc)(freeze_info->stubs_cur_pc - freeze_info->stubs_start_pc);
            hashtable_coarse_add(dcontext, frozen_a2c, frozen_htable);
            /* copy to new stubs area */
            transfer_coarse_stub(dcontext, freeze_info, pending->cur_pc,
                                 pending->trace_head, true /*replace*/);
        } else {
            /* fall-through optimization */
            if (DYNAMO_OPTION(coarse_freeze_elide_ubr) &&
                pending->link_cti_opnd != NULL &&
                pending->link_cti_opnd + 4 == freeze_info->cache_cur_pc &&
                pending->elide_ubr) {
                ASSERT(!pending->trace_head);
                LOG(THREAD, LOG_FRAGMENT, 4, "  fall-through opt from prev fragment\n");
                freeze_info->cache_cur_pc -= JMP_LONG_LENGTH;
                pending->link_cti_opnd = NULL;
                STATS_INC(coarse_freeze_fallthrough);
                DODEBUG({ freeze_info->num_elisions++; });
            }
            LOG(THREAD, LOG_FRAGMENT, 4,
                "  adding pending %sfragment " PFX "." PFX " => " PFX "\n",
                pending->trace_head ? "trace head " : "", pending->tag, pending->cur_pc,
                freeze_info->cache_cur_pc);
            /* add to htable the offset from start of cache */
            frozen_a2c.cache =
                (cache_pc)(freeze_info->cache_cur_pc - freeze_info->cache_start_pc);
            hashtable_coarse_add(dcontext, frozen_a2c, frozen_htable);
            /* copy to new cache */
            transfer_coarse_fragment(dcontext, freeze_info, pending->cur_pc);
        }
        tgt = frozen_a2c.cache;
    } else {
        tgt = looka2c.cache;
        /* Should not hit any links to TH, so should hit once, from htable walk */
        ASSERT(!pending->trace_head || pending->entrance_stub);
        /* May have added entrance stub for intra-unit TH as non-TH if it was
         * linked to a trace, since didn't have body pc at the time, so we
         * fix up here on the proactive add when adding its body
         */
        if (pending->entrance_stub && pending->trace_head && freeze_info->unlink) {
            cache_pc abs_tgt = tgt + (ptr_uint_t)freeze_info->stubs_start_pc;
            transfer_coarse_stub_fix_trace_head(dcontext, freeze_info, abs_tgt);
        }
    }
    if (pending->link_cti_opnd != NULL) {
        /* fix up incoming link */
        cache_pc patch_tgt =
            (cache_pc)(((ptr_uint_t)(pending->entrance_stub
                                         ? freeze_info->stubs_start_pc
                                         : freeze_info->cache_start_pc)) +
                       tgt);
        ASSERT(!pending->trace_head || pending->entrance_stub);
        LOG(THREAD, LOG_FRAGMENT, 4, "  patch link " PFX " => " PFX "." PFX "%s\n",
            pending->link_cti_opnd, pending->tag, patch_tgt,
            pending->entrance_stub ? " stub" : "");
        insert_relative_target(pending->link_cti_opnd, patch_tgt, NOT_HOT_PATCHABLE);
    }
    if (pending->entrance_stub) {
        DODEBUG({ frozen_htable->is_local = false; });
    }
}

/* There are several strategies for copying each fragment and non-inter-unit stub
 * to new, compact storage.  Here we use a cache-driven approach, necessarily
 * augmented with the htable as we cannot find tags for arbitrary fragments in
 * the cache.  We use a pending-add stack to avoid a second pass.
 * By adding ubr targets last, we can elide fall-through jmps.
 * FIXME case 9428:
 *   - Sorting entrance stubs for faster lazy linking lookup
 *   - Use 8-bit-relative jmps when possible for compaction, though this
 *     requires pc translation support and probably an extra pass when freezing.
 *
 * Tasks:
 * Copy each new fragment and non-inter-unit stub to the new region
 * Unlink all inter-unit entrance stubs, unless freezing and not
 *   writing to disk.
 * Re-target intra-unit jmps from old entrance stub to new fragment location
 * Re-target jmp to stub
 * Re-target indirect stubs to new prefix
 * Re-target inter-unit and trace head stubs to new prefix
 *
 * case 9900: must have dynamo_all_threads_synched since we haven't resolved
 * lock rank ordering issues with the hashtable locks
 */
void
fragment_coarse_unit_freeze(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info)
{
    pending_freeze_t pending_local;
    pending_freeze_t *pending;
    app_to_cache_t a2c;
    coarse_table_t *htable;
    cache_pc body_pc;
    uint i;
    ASSERT(freeze_info != NULL && freeze_info->src_info != NULL);
    if (freeze_info->src_info->htable == NULL)
        return;
    LOG(THREAD, LOG_FRAGMENT, 2, "freezing fragments in %s\n",
        freeze_info->src_info->module);

    /* we walk just the main htable and find trace heads by asking for the body pc */
    htable = (coarse_table_t *)freeze_info->src_info->htable;
    DOSTATS({
        LOG(THREAD, LOG_ALL, 1, "htable pre-freezing %s\n",
            freeze_info->src_info->module);
        hashtable_coarse_study(dcontext, htable, 0 /*clean state*/);
    });
    DEBUG_DECLARE(coarse_table_t *frozen_htable =
                      (coarse_table_t *)freeze_info->dst_info->htable;)
    /* case 9900: rank order conflict with coarse_info_incoming_lock:
     * do not grab frozen_htable write lock: mark is_local instead and rely
     * on dynamo_all_threads_synched
     */
    DODEBUG({ frozen_htable->is_local = true; });
    ASSERT_NOT_IMPLEMENTED(dynamo_all_threads_synched && "case 9900");
    /* FIXME case 9522: not grabbing TABLE_RWLOCK(htable, read, lock) due to
     * rank order violation with frozen_htable write lock!  We go w/ the write
     * lock since lookup routines check for it.  To solve we'll need a way to
     * tell deadlock checking that frozen_htable is private to this thread and
     * that no other thread can hold its lock.  For now we only support
     * all-synch, but if later we want !in_place that needs no synch we'll need
     * a solution.
     */
    ASSERT_NOT_IMPLEMENTED(dynamo_all_threads_synched && "case 9522");
    /* FIXME: we're doing htable order, better to use either tag (original app)
     * or cache (execution) order?
     */
    for (i = 0; i < htable->capacity || freeze_info->pending != NULL; i++) {

        /* Process pending entries first; then continue through htable */
        while (freeze_info->pending != NULL) {
            pending = freeze_info->pending;
            freeze_info->pending = pending->next;
            fragment_coarse_entry_freeze(dcontext, freeze_info, pending);
            HEAP_TYPE_FREE(dcontext, pending, pending_freeze_t,
                           ACCT_MEM_MGT /*appropriate?*/, UNPROTECTED);
        }

        if (i < htable->capacity) {
            a2c = htable->table[i];
            /* must check for sentinel */
            if (!A2C_ENTRY_IS_REAL(a2c))
                continue;
        } else
            continue;

        LOG(THREAD, LOG_FRAGMENT, 4, " %d app=" PFX ", cache=" PFX "\n", i, a2c.app,
            a2c.cache);
        coarse_body_from_htable_entry(dcontext, freeze_info->src_info, a2c.app, a2c.cache,
                                      NULL, &body_pc);

        if (body_pc == NULL) {
            /* We add only when targeted by fragments, so we don't have to
             * figure out multiple times whether intra-unit or not
             */
            LOG(THREAD, LOG_FRAGMENT, 4, "  ignoring entrance stub " PFX "\n", a2c.cache);
        } else {
            pending_local.tag = a2c.app;
            pending_local.cur_pc = body_pc;
            pending_local.entrance_stub = false;
            pending_local.link_cti_opnd = NULL;
            pending_local.elide_ubr = true; /* doesn't matter since no link */
            pending_local.trace_head = coarse_is_trace_head_in_own_unit(
                dcontext, a2c.app, a2c.cache, body_pc, true, freeze_info->src_info);
            pending_local.next = NULL;
            fragment_coarse_entry_freeze(dcontext, freeze_info, &pending_local);

            if (pending_local.trace_head) {
                /* we do need to proactively add the entrance stub, in
                 * case it is only targeted by an indirect branch
                 */
                LOG(THREAD, LOG_FRAGMENT, 4,
                    "  adding trace head entrance stub " PFX "\n", a2c.cache);
                pending_local.tag = a2c.app;
                pending_local.cur_pc = a2c.cache;
                pending_local.entrance_stub = true;
                pending_local.link_cti_opnd = NULL;
                pending_local.elide_ubr = true; /* doesn't matter since no link */
                pending_local.trace_head = true;
                fragment_coarse_entry_freeze(dcontext, freeze_info, &pending_local);
            }
        }
    }
    DODEBUG({ frozen_htable->is_local = false; });
    DOSTATS({
        /* The act of freezing tends to improve hashtable layout */
        LOG(THREAD, LOG_ALL, 1, "htable post-freezing %s\n",
            freeze_info->src_info->module);
        hashtable_coarse_study(dcontext, frozen_htable, 0 /*clean state*/);
    });
}

uint
fragment_coarse_htable_persist_size(dcontext_t *dcontext, coarse_info_t *info,
                                    bool cache_table)
{
    coarse_table_t *htable =
        (coarse_table_t *)(cache_table ? info->htable : info->th_htable);
    return hashtable_coarse_persist_size(dcontext, htable);
}

/* Returns true iff all writes succeeded. */
bool
fragment_coarse_htable_persist(dcontext_t *dcontext, coarse_info_t *info,
                               bool cache_table, file_t fd)
{
    coarse_table_t *htable =
        (coarse_table_t *)(cache_table ? info->htable : info->th_htable);
    ASSERT(fd != INVALID_FILE);
    return hashtable_coarse_persist(dcontext, htable, fd);
}

void
fragment_coarse_htable_resurrect(dcontext_t *dcontext, coarse_info_t *info,
                                 bool cache_table, byte *mapped_table)
{
    coarse_table_t **htable =
        (coarse_table_t **)(cache_table ? &info->htable : &info->th_htable);
    ASSERT(info->frozen);
    ASSERT(mapped_table != NULL);
    ASSERT(*htable == NULL);
    *htable = hashtable_coarse_resurrect(
        dcontext,
        mapped_table _IF_DEBUG(cache_table ? "persisted cache htable"
                                           : "persisted stub htable"));
    (*htable)->mod_shift = info->mod_shift;
    /* generally want to keep basic alignment */
    ASSERT_CURIOSITY(ALIGNED((*htable)->table, sizeof(app_pc)));
}

/*******************************************************************************/
