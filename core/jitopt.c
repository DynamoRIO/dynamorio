/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#include "globals.h"
#include "fragment.h"
#include "hashtable.h"
#include "instrument.h" // hackish...
#include "monitor.h"
#include "jitopt.h"

#ifdef JITOPT

#define BUCKET_BIT_SIZE 6
#define BUCKET_MASK 0x3f
#define BUCKET_BBS 3
#define BUCKET_OFFSET_SENTINEL 1
#define BUCKET_ID(pc) (((ptr_uint_t) (pc)) >> BUCKET_BIT_SIZE)
#define DGC_REF_COUNT_BITS 0xa
#define DGC_REF_COUNT_MASK 0x3ff
#define DGC_MAX_TAG_REMOVAL 0x50
#define DGC_MAX_TRACE_REMOVAL 0x100

typedef struct _dgc_trace_t {
    app_pc tags[2];
    struct _dgc_trace_t *next_trace;
} dgc_trace_t;

/* x86: 16 bytes | x64: 32 bytes */
typedef struct _dgc_bb_t dgc_bb_t;
struct _dgc_bb_t {
    app_pc start;
    int ref_count;
    union {
        ptr_uint_t span;
        struct _dgc_bb_t *head;
    };
    dgc_bb_t *next;
    dgc_trace_t *containing_trace_list;
};

/* x86: 68 bytes | x64 128 bytes */
typedef struct _dgc_bucket_t {
    dgc_bb_t blocks[BUCKET_BBS];
    uint offset_sentinel;
    ptr_uint_t bucket_id;
    struct _dgc_bucket_t *head;
    struct _dgc_bucket_t *chain;
} dgc_bucket_t;

#define DGC_BUCKET_GC_CAPACITY 0x80
typedef struct _dgc_bucket_gc_list_t {
    dgc_bucket_t *staging[DGC_BUCKET_GC_CAPACITY];
    dgc_bucket_t *removals[DGC_BUCKET_GC_CAPACITY];
    uint staging_count;
    uint removal_count;
    bool allow_immediate_gc;
    const char *current_operation;
} dgc_bucket_gc_list_t;

static dgc_bucket_gc_list_t *dgc_bucket_gc_list;
static generic_table_t *dgc_table;

typedef struct _dgc_thread_state_t {
    int count;
    uint version;
    thread_record_t **threads;
} dgc_thread_state_t;

static dgc_thread_state_t *thread_state;

typedef struct _dgc_fragment_intersection_t {
    app_pc *bb_tags;
    uint bb_tag_max;
    app_pc *trace_tags;
    uint trace_tag_max;
} dgc_fragment_intersection_t;

static dgc_fragment_intersection_t *fragment_intersection;

typedef struct _dgc_stats_t {
    uint allocated_bucket_count;
    uint freed_bucket_count;
    uint live_bucket_count;
    uint allocated_trace_count;
    uint freed_trace_count;
    uint live_trace_count;
} dgc_stats_t;

static dgc_stats_t *dgc_stats;

#ifdef DEBUG
# define RELEASE_LOG(file, category, level, ...) LOG(file, category, level, __VA_ARGS__)
# define RELEASE_ASSERT(cond, msg, ...) \
    if (!(cond)) \
        LOG(GLOBAL, LOG_FRAGMENT, 1, "Fail: "#cond" \""msg"\"\n", ##__VA_ARGS__)
#else
# define RELEASE_LOG(file, category, level, ...)
//dr_printf(__VA_ARGS__)
# define RELEASE_ASSERT(cond, msg, ...) \
    if (!(cond)) \
        dr_printf("Fail: "#cond" \""msg"\"\n", ##__VA_ARGS__)
#endif

static void
free_dgc_bucket_chain(void *p);

void
jitopt_init()
{
    dgc_table = generic_hash_create(GLOBAL_DCONTEXT, 7, 80,
                                    HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                    HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                    free_dgc_bucket_chain _IF_DEBUG("DGC Coverage Table"));

    dgc_bucket_gc_list = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_gc_list_t,
                                         ACCT_OTHER, UNPROTECTED);
    thread_state = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_thread_state_t,
                                   ACCT_OTHER, UNPROTECTED);
    memset(thread_state, 0, sizeof(dgc_thread_state_t));
    fragment_intersection = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_fragment_intersection_t,
                                            ACCT_OTHER, UNPROTECTED);
    fragment_intersection->bb_tag_max = 0x20;
    fragment_intersection->bb_tags =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, fragment_intersection->bb_tag_max,
                         ACCT_OTHER, UNPROTECTED);
    fragment_intersection->trace_tag_max = 0x20;
    fragment_intersection->trace_tags =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, fragment_intersection->trace_tag_max,
                         ACCT_OTHER, UNPROTECTED);
    dgc_stats = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_stats_t, ACCT_OTHER, UNPROTECTED);
    memset(dgc_stats, 0, sizeof(dgc_stats_t));
}

void
jitopt_exit()
{
    generic_hash_destroy(GLOBAL_DCONTEXT, dgc_table);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_bucket_gc_list, dgc_bucket_gc_list_t,
                   ACCT_OTHER, UNPROTECTED);
    if (thread_state->threads != NULL) {
        global_heap_free(thread_state->threads,
                         thread_state->count*sizeof(thread_record_t*)
                         HEAPACCT(ACCT_THREAD_MGT));
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, thread_state, dgc_thread_state_t,
                   ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, fragment_intersection->bb_tags, app_pc,
                    fragment_intersection->bb_tag_max, ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, fragment_intersection->trace_tags, app_pc,
                    fragment_intersection->trace_tag_max, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, fragment_intersection, dgc_fragment_intersection_t,
                   ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_stats, dgc_stats_t,
                   ACCT_OTHER, UNPROTECTED);
}

static void
dgc_stat_report(uint count)
{
    uint i = 1, log = count / 10;
    while (log > 0) {
        log /= 10;
        i *= 10;
    }
    if (count == i) {
        dr_printf("stats: ab %d | fb %d | lb %d || at %d | ft %d | lt %d\n",
                  dgc_stats->allocated_bucket_count, dgc_stats->freed_bucket_count,
                  dgc_stats->live_bucket_count, dgc_stats->allocated_trace_count,
                  dgc_stats->freed_trace_count, dgc_stats->live_trace_count);
    }
}

static inline bool
dgc_bb_is_head(dgc_bb_t *bb)
{
    return (ptr_uint_t)bb->span < 0x1000;
}

static inline dgc_bb_t *
dgc_bb_head(dgc_bb_t *bb)
{
    return dgc_bb_is_head(bb) ? bb : bb->head;
}

static inline dgc_trace_t *
dgc_bb_traces(dgc_bb_t *bb)
{
    return dgc_bb_head(bb)->containing_trace_list;
}

static inline app_pc
dgc_bb_start(dgc_bb_t *bb)
{
    return bb->start;
}

static inline app_pc
dgc_bb_end(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return (app_pc)((ptr_uint_t)head->start + (ptr_uint_t)head->span + 1);
}

static inline ptr_uint_t
dgc_bb_start_bucket_id(dgc_bb_t *bb)
{
    return BUCKET_ID(bb->start);
}

static inline ptr_uint_t
dgc_bb_end_bucket_id(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return BUCKET_ID((ptr_uint_t)head->start + (ptr_uint_t)head->span);
}

static inline dgc_bucket_t *
dgc_get_containing_bucket(dgc_bb_t *bb)
{
    if (*(ptr_uint_t *)(bb + 1) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)(bb - 2);
    if (*(ptr_uint_t *)(bb + 2) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)(bb - 1);
    if (*(ptr_uint_t *)(bb + 3) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)bb;
    ASSERT(false);
    return NULL;
}

static inline bool
dgc_bb_overlaps(dgc_bb_t *bb, app_pc start, app_pc end)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return (ptr_uint_t)head->start < (ptr_uint_t)end ||
           ((ptr_uint_t)head->start + (ptr_uint_t)head->span) >= (ptr_uint_t)start;
}

/*
static uint
dgc_change_ref_count(dgc_bucket_t *bucket, uint i, int delta)
{
    uint shift = (i * DGC_REF_COUNT_BITS);
    uint mask = DGC_REF_COUNT_MASK << shift;
    int ref_count = (bucket->ref_counts & mask) >> shift;
    ref_count += delta;
    ASSERT(i < BUCKET_BBS);
    ASSERT(ref_count >= 0);
    if (ref_count > 0x2ff) {
        LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: 0x%x refs to bb "PFX"\n",
            ref_count, bucket->blocks[i].start);
    }
    ASSERT(ref_count < DGC_REF_COUNT_MASK);
    bucket->ref_counts &= ~mask;
    bucket->ref_counts |= (ref_count << shift);
    return ref_count;
}

static void
dgc_set_ref_count(dgc_bucket_t *bucket, uint i, uint value)
{
    uint shift = (i * DGC_REF_COUNT_BITS);
    uint mask = DGC_REF_COUNT_MASK << shift;
    ASSERT(value == 1);
    bucket->ref_counts &= ~mask;
    bucket->ref_counts |= (value << shift);
}
*/

static dgc_bb_t *
dgc_table_find_bb(app_pc tag, dgc_bucket_t **out_bucket, uint *out_i)
{
    uint i;
    ptr_uint_t bucket_id = BUCKET_ID(tag);
    dgc_bucket_t *bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
    // assert table lock
    while (bucket != NULL) {
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start == tag) {
                if (out_bucket != NULL)
                    *out_bucket = bucket;
                if (out_i != NULL)
                    *out_i = i;
                return &bucket->blocks[i];
            }
        }
        bucket = bucket->chain;
    }
    return NULL;
}

static void
free_dgc_traces(dgc_bb_t *bb) {
    dgc_trace_t *next_trace, *trace = bb->containing_trace_list;
    ASSERT(bb->start == NULL);
    ASSERT(dgc_bb_head(bb)->start == NULL);
    while (trace != NULL) {
        next_trace = trace->next_trace;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, trace, dgc_trace_t, ACCT_OTHER, UNPROTECTED);
        dgc_stats->live_trace_count--;
        dgc_stat_report(++dgc_stats->freed_trace_count);
        trace = next_trace;
    }
}

static void
dgc_table_bucket_gc(dgc_bucket_t *bucket)
{
    if (bucket != NULL) {
        uint i;
        bool is_empty, all_empty = true;
        ptr_uint_t bucket_id = bucket->bucket_id;
        dgc_bucket_t *anchor = NULL;
        do {
            RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
            is_empty = true;
            for (i = 0; i < BUCKET_BBS; i++) {
                if (bucket->blocks[i].start != NULL) {
                    is_empty = false;
                    break;
                }
            }
            if (is_empty) {
                if (anchor == NULL) {
                    dgc_bucket_t *walk;
                    if (bucket->chain == NULL)
                        break;
                    anchor = bucket->chain;
                    bucket->chain = NULL;
                    generic_hash_remove(GLOBAL_DCONTEXT, dgc_table, bucket->bucket_id);
                    RELEASE_ASSERT(anchor->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
                    generic_hash_add(GLOBAL_DCONTEXT, dgc_table, anchor->bucket_id, anchor);
                    walk = anchor;
                    do {
                        walk->head = anchor;
                        walk = walk->chain;
                    } while (walk != NULL);
                    bucket = anchor;
                    anchor = NULL;
                    /* do not advance to the next bucket--this one has not been checked */
                } else {
                    anchor->chain = bucket->chain;
                    RELEASE_ASSERT(bucket != bucket->head, "Freeing the head bucket w/o removing it!\n");
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, bucket, dgc_bucket_t,
                                   ACCT_OTHER, UNPROTECTED);
                    dgc_stats->live_bucket_count--;
                    dgc_stat_report(++dgc_stats->freed_bucket_count);
                    bucket = anchor->chain;
                }
            } else {
                all_empty = false;
                anchor = bucket;
                bucket = bucket->chain;
            }
        } while (bucket != NULL);
        if (all_empty)
            generic_hash_remove(GLOBAL_DCONTEXT, dgc_table, bucket_id);
    }
}

static inline void
dgc_bucket_gc_list_init(bool allow_immediate_gc, const char *current_operation)
{
    dgc_bucket_gc_list->staging_count = 0;
    dgc_bucket_gc_list->removal_count = 0;
    dgc_bucket_gc_list->allow_immediate_gc = allow_immediate_gc;
    dgc_bucket_gc_list->current_operation = current_operation;
}

static bool
dgc_bucket_is_packed(dgc_bucket_t *bucket)
{
    uint i;
    bool packed;

    if ((ptr_uint_t)bucket == 0xcdcdcdcdUL ||
        bucket->offset_sentinel != BUCKET_OFFSET_SENTINEL) { // freed
        return true;
    }

    while (bucket != NULL) {
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start != NULL) {
                packed = true;
                break;
            }
        }
        if (!packed)
            return false;
        bucket = bucket->chain;
    }
    return true;
}

static void
dgc_bucket_gc()
{
    uint i, j;
    for (i = 0; i < dgc_bucket_gc_list->removal_count; i++) {
        for (j = 0; j < dgc_bucket_gc_list->staging_count; j++) {
            if (dgc_bucket_gc_list->staging[j] == dgc_bucket_gc_list->removals[i]) {
                dgc_bucket_gc_list->staging[j] = NULL;
                break;
            }
        }
        generic_hash_remove(GLOBAL_DCONTEXT, dgc_table,
                            dgc_bucket_gc_list->removals[i]->bucket_id);
    }

    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i] != NULL)
            dgc_table_bucket_gc(dgc_bucket_gc_list->staging[i]);
    }

    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i] != NULL)
            RELEASE_ASSERT(dgc_bucket_is_packed(dgc_bucket_gc_list->staging[i]), "not packed");
    }
}

static void
dgc_stage_bucket_for_gc(dgc_bucket_t *bucket)
{
    uint i;
    bool found = false;
    if (bucket == NULL)
        return;
    RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
    RELEASE_ASSERT(bucket == bucket->head, "No!");
    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i]->bucket_id == bucket->bucket_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (i >= (DGC_BUCKET_GC_CAPACITY-1)) {
            if (dgc_bucket_gc_list->allow_immediate_gc) {
                dgc_bucket_gc();
                i = 0;
            } else {
                RELEASE_ASSERT(false, "GC staging list too full (%d) during %s",
                               i, dgc_bucket_gc_list->current_operation);
            }
        }
        dgc_bucket_gc_list->staging[dgc_bucket_gc_list->staging_count++] = bucket;
    }
}

static inline void
dgc_stage_bucket_id_for_gc(ptr_uint_t bucket_id)
{
    dgc_stage_bucket_for_gc(generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id));
}

static void
dgc_set_all_slots_empty(dgc_bb_t *bb)
{
    dgc_bb_t *next_bb;
    dgc_bucket_t *bucket;
    if (bb->start == NULL)
        return; // already gc'd
    bb = dgc_bb_head(bb);
    bb->start = NULL;
    free_dgc_traces(bb);
    do {
        next_bb = bb->next;
        bucket = dgc_get_containing_bucket(bb)->head;
        dgc_stage_bucket_for_gc(bucket);
        bb->start = NULL;
        ASSERT(dgc_bb_head(bb)->start == NULL);
        DODEBUG(bb->span = 0;);
        bb = next_bb;
    } while (bb != NULL);
}

static void
free_dgc_bucket_chain(void *p)
{
    uint i;
    dgc_bucket_t *next_bucket, *bucket = (dgc_bucket_t *)p;
    ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL);
    do {
        next_bucket = bucket->chain;
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start != NULL && dgc_bb_is_head(&bucket->blocks[i])) {
                bucket->blocks[i].start = NULL;
                free_dgc_traces(&bucket->blocks[i]);
            }
        }
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, bucket, dgc_bucket_t, ACCT_OTHER, UNPROTECTED);
        dgc_stats->live_bucket_count--;
        dgc_stat_report(++dgc_stats->freed_bucket_count);
        bucket = next_bucket;
    } while (bucket != NULL);
}

void
dgc_table_dereference_bb(app_pc tag)
{
    TABLE_RWLOCK(dgc_table, write, lock);
    dgc_bb_t *bb = dgc_table_find_bb(tag, NULL, NULL);
    if (bb != NULL) {
        bb = dgc_bb_head(bb);
        if ((--bb->ref_count) == 0) {
            dgc_bucket_gc_list_init(false, "dgc_table_dereference_bb");
            dgc_set_all_slots_empty(bb); // _IF_DEBUG("refcount"));
            dgc_bucket_gc();
        } else {
            ASSERT(bb->ref_count >= 0);
        }
    }
    TABLE_RWLOCK(dgc_table, write, unlock);
}

static void
dgc_stage_removal_gc_outliers(ptr_uint_t bucket_id)
{
    uint i;
    bool found = false;
    dgc_bucket_t *bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
    if (bucket != NULL) {
        dgc_bucket_t *head_bucket = bucket;
        RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
        RELEASE_ASSERT(bucket == bucket->head, "No!");

        do {
            ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL);
            for (i = 0; i < BUCKET_BBS; i++) {
                if (bucket->blocks[i].start != NULL)
                    dgc_set_all_slots_empty(&bucket->blocks[i]);
            }
            bucket = bucket->chain;
        } while (bucket != NULL);

        bucket = head_bucket;
        for (i = 0; i < dgc_bucket_gc_list->removal_count; i++) {
            if (dgc_bucket_gc_list->removals[i]->bucket_id == bucket->bucket_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (i >= (DGC_BUCKET_GC_CAPACITY-1)) {
                RELEASE_ASSERT(false, "GC removal list too full (%d) during %s",
                               i, dgc_bucket_gc_list->current_operation);
            }
            dgc_bucket_gc_list->removals[dgc_bucket_gc_list->removal_count++] = bucket;
        }
    }
}

void
dgc_notify_region_cleared(app_pc start, app_pc end)
{
    ptr_uint_t first_bucket_id = BUCKET_ID(start);
    ptr_uint_t last_bucket_id = BUCKET_ID(end - 1);
    bool is_start_of_bucket = (((ptr_uint_t)start & BUCKET_MASK) == 0);
    bool is_end_of_bucket = (((ptr_uint_t)end & BUCKET_MASK) == 0);
    ptr_uint_t bucket_id = first_bucket_id;

    TABLE_RWLOCK(dgc_table, write, lock);
    dgc_bucket_gc_list_init(true, "dgc_notify_region_cleared");
    if (is_start_of_bucket && (is_end_of_bucket || bucket_id < last_bucket_id))
        dgc_stage_removal_gc_outliers(bucket_id);
    else
        dgc_stage_bucket_id_for_gc(bucket_id);
    for (++bucket_id; bucket_id < last_bucket_id; bucket_id++)
        dgc_stage_removal_gc_outliers(bucket_id);
    if (bucket_id == last_bucket_id) {
        if (is_end_of_bucket && (bucket_id > first_bucket_id))
            dgc_stage_removal_gc_outliers(bucket_id);
        else
            dgc_stage_bucket_id_for_gc(bucket_id);
    }
    dgc_bucket_gc();
    TABLE_RWLOCK(dgc_table, write, unlock);
}

void
dgc_cache_reset()
{
    TABLE_RWLOCK(dgc_table, write, lock);
    generic_hash_clear(GLOBAL_DCONTEXT, dgc_table);
    TABLE_RWLOCK(dgc_table, write, unlock);
}

void
add_patchable_bb(app_pc start, app_pc end)
{
    bool found = false;
    uint i;
    ptr_uint_t bucket_id;
    dgc_bb_t *last_bb = NULL, *first_bb = NULL;
    dgc_bucket_t *bucket;

    if (!is_app_managed_code(start))
        return;

    LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: add bb "PFX"-"PFX"\n", start, end);

    TABLE_RWLOCK(dgc_table, write, lock);
    for (bucket_id = BUCKET_ID(start); bucket_id <= BUCKET_ID(end - 1); bucket_id++) {
        bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
        if (bucket == NULL) {
            bucket = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t, ACCT_OTHER, UNPROTECTED);
            memset(bucket, 0, sizeof(dgc_bucket_t));
            generic_hash_add(GLOBAL_DCONTEXT, dgc_table, bucket_id, bucket);
            bucket->offset_sentinel = BUCKET_OFFSET_SENTINEL;
            bucket->bucket_id = bucket_id;
            bucket->head = bucket;
            i = 0;
            dgc_stats->live_bucket_count++;
            dgc_stat_report(++dgc_stats->allocated_bucket_count);
        } else {
            dgc_bucket_t *head_bucket = bucket, *available_bucket = NULL;
            uint available_slot = 0xff;
            RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
            RELEASE_ASSERT(bucket == bucket->head, "No!");
            while (true) {
                for (i = 0; i < BUCKET_BBS; i++) {
                    if (bucket->blocks[i].start == start) {
//#ifdef DEBUG
                        if (dgc_bb_end(&bucket->blocks[i]) == end) {
//#endif
                            found = true;
                            break;
//#ifdef DEBUG
                        } else {
                            dr_printf("DGC: stale bb "
                                PFX"-"PFX"!\n", start, dgc_bb_end(&bucket->blocks[i]));
                            /*
                            LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: stale bb "
                                PFX"-"PFX"!\n", start, dgc_bb_end(&bucket->blocks[i]));
                            //bucket->blocks[i].start = NULL;
                            dgc_bucket_gc_list_init(false, "add_patchable_bb");
                            dgc_set_all_slots_empty(&bucket->blocks[i]);
                            dgc_bucket_gc();
                            */
                        }
//#endif
                    }
                    if (available_bucket == NULL && bucket->blocks[i].start == NULL) {
                        available_bucket = bucket;
                        available_slot = i;
                    }
                }
                if (found || bucket->chain == NULL)
                    break;
                bucket = bucket->chain;
            }
            if (found) {
                bucket->blocks[i].ref_count++;
                //dgc_change_ref_count(bucket, i, 1);
                ASSERT(bucket->blocks[i].ref_count > 1);
                ASSERT(bucket->blocks[i].ref_count < 0x10000000);
                ASSERT(first_bb == NULL);
                break;
            }
            if (available_bucket == NULL) {
                dgc_bucket_t *new_bucket = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t,
                                                           ACCT_OTHER, UNPROTECTED);
                memset(new_bucket, 0, sizeof(dgc_bucket_t));
                ASSERT(bucket->chain == NULL);
                new_bucket->bucket_id = bucket_id;
                new_bucket->head = head_bucket;
                new_bucket->offset_sentinel = BUCKET_OFFSET_SENTINEL;
                bucket->chain = new_bucket;
                bucket = new_bucket;
                i = 0;
                dgc_stats->live_bucket_count++;
                dgc_stat_report(++dgc_stats->allocated_bucket_count);
            } else {
                bucket = available_bucket;
                i = available_slot;
            }
        }
        if (found)
            break;
        bucket->blocks[i].start = start;
        if (first_bb == NULL) {
            first_bb = &bucket->blocks[i];
            bucket->blocks[i].span = (end - start) - 1;
            bucket->blocks[i].containing_trace_list = NULL;
            bucket->blocks[i].ref_count = 1;
        } else {
            bucket->blocks[i].head = first_bb;
            last_bb->next = &bucket->blocks[i];
        }
        last_bb = &bucket->blocks[i];
    }
    if (!found)
        last_bb->next = NULL;
    TABLE_RWLOCK(dgc_table, write, unlock);
}

void
add_patchable_trace(app_pc trace_tag, app_pc bb_tag)
{
    bool found_trace = false;
    dgc_bb_t *bb;

    if (!is_app_managed_code(bb_tag))
        return;

    LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: add bb "PFX" to trace "PFX"\n", bb_tag, trace_tag);

    TABLE_RWLOCK(dgc_table, write, lock);
    bb = dgc_table_find_bb(bb_tag, NULL, NULL);
    if (bb != NULL) {
        dgc_trace_t *trace = bb->containing_trace_list;
        while (trace != NULL) {
            if (trace->tags[0] == trace_tag) {
                found_trace = true;
                break;
            }
            if (trace->tags[1] == NULL)
                break;
            if (trace->tags[1] == trace_tag) {
                found_trace = true;
                break;
            }
            trace = trace->next_trace;
        }
        if (!found_trace) {
            if (trace != NULL) {
                ASSERT(trace->tags[1] == NULL);
                trace->tags[1] = trace_tag;
            } else {
                trace = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_trace_t,
                                        ACCT_OTHER, UNPROTECTED);
                trace->tags[0] = trace_tag;
                trace->tags[1] = NULL;
                trace->next_trace = bb->containing_trace_list;
                bb->containing_trace_list = trace;
                dgc_stats->live_trace_count++;
                dgc_stat_report(++dgc_stats->allocated_trace_count);
            }
        }
    }
    TABLE_RWLOCK(dgc_table, write, unlock);
}

static void
safe_delete_fragment(dcontext_t *dcontext, fragment_t *f)
{
    if (TEST(FRAG_CANNOT_DELETE, f->flags)) {
        dr_printf("Cannot delete fragment "PFX" with flags 0x%x!\n", f->tag, f->flags);
        return;
    }

    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
    acquire_vm_areas_lock(dcontext, f->flags);
    fragment_delete(dcontext, f, FRAGDEL_NO_OUTPUT | FRAGDEL_NO_MONITOR | FRAGDEL_NO_HEAP |
                                 FRAGDEL_NO_FCACHE);
    release_vm_areas_lock(dcontext, f->flags);
    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);

    f->flags |= FRAG_WAS_DELETED;
    fragment_delete(dcontext, f, FRAGDEL_NO_OUTPUT | FRAGDEL_NO_VMAREA | FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE);
}

static void
remove_patchable_fragment_list(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end)
{
    uint i, j;
    fragment_t *f;
    app_pc *bb_tag, *trace_tag;
    bool thread_has_fragment;
    per_thread_t *tgt_pt;
    dcontext_t *tgt_dcontext;

    for (i=0; i < thread_state->count; i++) {
        tgt_dcontext = thread_state->threads[i]->dcontext;

        thread_has_fragment = false;

        // why doesn't this work??
        //if (!thread_vm_area_overlap(tgt_dcontext, patch_start, patch_end))
        //    continue;

        // TODO: could put the fragments on the fragment_intersection, to avoid
        // looking them up repeatedly.
        for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
            if (fragment_lookup(tgt_dcontext, *bb_tag) != NULL) {
                thread_has_fragment = true;
                break;
            }
        }

        if (!thread_has_fragment) {
            trace_tag = fragment_intersection->trace_tags;
            for (; *trace_tag != NULL; trace_tag++) {
                if (fragment_lookup_trace(tgt_dcontext, *trace_tag) != NULL) {
                    thread_has_fragment = true;
                    break;
                }
            }
        }

        if (!thread_has_fragment)
            continue;

        tgt_pt = (per_thread_t *) tgt_dcontext->fragment_field;

        if (tgt_dcontext != dcontext) {
            mutex_lock(&tgt_pt->linking_lock);
            if (tgt_pt->could_be_linking) {
                /* remember we have a global lock, thread_initexit_lock, so two threads
                 * cannot be here at the same time!
                 */
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\twaiting for thread "TIDFMT"\n", tgt_dcontext->owning_thread);
                tgt_pt->wait_for_unlink = true;
                mutex_unlock(&tgt_pt->linking_lock);
                wait_for_event(tgt_pt->waiting_for_unlink);
                mutex_lock(&tgt_pt->linking_lock);
                tgt_pt->wait_for_unlink = false;
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\tdone waiting for thread "TIDFMT"\n", tgt_dcontext->owning_thread);
            } else {
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\tthread "TIDFMT" synch not required\n", tgt_dcontext->owning_thread);
            }
            mutex_unlock(&tgt_pt->linking_lock);
        }

        if (is_building_trace(tgt_dcontext)) {
            /* not locking b/c a race should at worst abort a valid trace */
            bool clobbered = false;
            monitor_data_t *md = (monitor_data_t *) dcontext->monitor_field;
            for (j = 0; j < md->blk_info_length && !clobbered; j++) {
                for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
                    if (*bb_tag == md->blk_info[j].info.tag) {
                        clobbered = true;
                        break;
                    }
                }
            }
            if (clobbered) {
                dr_printf("Warning! Squashing trace "PFX" because it overlaps removal bb "
                           PFX"\n", md->trace_tag, *bb_tag);
                trace_abort(tgt_dcontext);
            }
        }
        //if (DYNAMO_OPTION(syscalls_synch_flush) && get_at_syscall(tgt_dcontext)) {
#ifdef CLIENT_INTERFACE
            //dr_printf("Warning! Thread is at syscall while removing frags from that thread.\n");
#endif
            // does this matter??
            /* we have to know exactly which threads were at_syscall here when
             * we get to post-flush, so we cache in this special bool
             */
        //}

        for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
            f = fragment_lookup_trace(dcontext, *bb_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
            f = fragment_lookup_shared_trace(dcontext, *bb_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
            f = fragment_lookup_bb(dcontext, *bb_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
            f = fragment_lookup_shared_bb(dcontext, *bb_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
        }
        trace_tag = fragment_intersection->trace_tags;
        for (; *trace_tag != NULL; trace_tag++) {
            f = fragment_lookup_trace(dcontext, *trace_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
            f = fragment_lookup_shared_trace(dcontext, *trace_tag);
            if (f != NULL)
                safe_delete_fragment(tgt_dcontext, f);
        }

        if (tgt_dcontext != dcontext) {
            tgt_pt = (per_thread_t *) tgt_dcontext->fragment_field;
            mutex_lock(&tgt_pt->linking_lock);
            if (tgt_pt->could_be_linking) {
                signal_event(tgt_pt->finished_with_unlink);
            } else {
                /* we don't need to wait on a !could_be_linking thread
                 * so we use this bool to tell whether we should signal
                 * the event.
                 */
                if (tgt_pt->soon_to_be_linking)
                    signal_event(tgt_pt->finished_all_unlink);
            }
            mutex_unlock(&tgt_pt->linking_lock);
        }
    }
}

static void
update_thread_state()
{
    uint thread_state_version = get_thread_state_version();
    if (thread_state->threads == NULL) {
        thread_state->version = thread_state_version;
        get_list_of_threads(&thread_state->threads, &thread_state->count);
    } else if (thread_state_version != thread_state->version) {
        thread_state->version = thread_state_version;
        global_heap_free(thread_state->threads,
                         thread_state->count*sizeof(thread_record_t*)
                         HEAPACCT(ACCT_THREAD_MGT));
        get_list_of_threads(&thread_state->threads, &thread_state->count);
    }
}

static inline bool
has_tag(app_pc tag, app_pc *tags, uint count)
{
    uint i;
    for (i = 0; i < count; i++) {
        if (tags[i] == tag)
            return true;
    }
    return false;
}

static bool
buckets_exist_in_range(ptr_uint_t start, ptr_uint_t end)
{
    ptr_uint_t i;
    for (i = start; i < end; i++) {
        if (generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, i) != NULL)
            return true;
    }
    return false;
}

static void
expand_intersection_array(app_pc **array, uint *max_size)
{
    app_pc *original_array = *array;

    *array = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, *max_size * 2,
                              ACCT_OTHER, UNPROTECTED);
    memcpy(*array, original_array, *max_size * sizeof(app_pc));
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, original_array, app_pc, *max_size,
                    ACCT_OTHER, UNPROTECTED);
    *max_size *= 2;
}

static uint
add_trace_intersection(dgc_trace_t *trace, uint i)
{
    if (!has_tag(trace->tags[0], fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[0];
    if (i == fragment_intersection->trace_tag_max) {
        expand_intersection_array(&fragment_intersection->trace_tags,
                                  &fragment_intersection->trace_tag_max);
    }
    if (trace->tags[1] != NULL && !has_tag(trace->tags[1],
                                           fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[1];
    if (i == fragment_intersection->trace_tag_max) {
        expand_intersection_array(&fragment_intersection->trace_tags,
                                  &fragment_intersection->trace_tag_max);
    }
    return i;
}

uint
extract_fragment_intersection(app_pc patch_start, app_pc patch_end)
{
    ptr_uint_t bucket_id;
    ptr_uint_t start_bucket = BUCKET_ID(patch_start);
    ptr_uint_t end_bucket = BUCKET_ID(patch_end - 1);
    bool is_patch_start_bucket = true;
    DEBUG_DECLARE(bool found = false;)
    uint i, i_bb = 0, i_trace = 0;
    uint fragment_total = 0;
    dgc_bucket_t *bucket;
    dgc_trace_t *trace;
    dgc_bb_t *bb;

    TABLE_RWLOCK(dgc_table, write, lock);
    dgc_bucket_gc_list_init(false, "remove_patchable_fragments");
    for (bucket_id = start_bucket; bucket_id <= end_bucket; bucket_id++) {
        bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
        while (bucket != NULL) {
            for (i = 0; i < BUCKET_BBS; i++) {
                bb = &bucket->blocks[i];
                if (bb->start != NULL &&
                    dgc_bb_overlaps(bb, patch_start, patch_end) &&
                    (is_patch_start_bucket || dgc_bb_is_head(bb))) {
                    if (!has_tag(bb->start, fragment_intersection->bb_tags, i_bb))
                        fragment_intersection->bb_tags[i_bb++] = bb->start;
                    if (i_bb == DGC_MAX_TAG_REMOVAL) {
                        expand_intersection_array(&fragment_intersection->bb_tags,
                                                  &fragment_intersection->bb_tag_max);
                    }
                    trace = dgc_bb_head(bb)->containing_trace_list;
                    while (trace != NULL) {
                        i_trace = add_trace_intersection(trace, i_trace);
                        trace = trace->next_trace;
                    }
                    dgc_set_all_slots_empty(bb);
                    DODEBUG(found = true;);
                }
            }
            bucket = bucket->chain;
        }
        is_patch_start_bucket = false;
    }
    fragment_intersection->bb_tags[i_bb] = NULL;
    fragment_intersection->trace_tags[i_trace] = NULL;
    dgc_bucket_gc();
    RELEASE_ASSERT(!buckets_exist_in_range(start_bucket+1, end_bucket), "buckets exist");
    fragment_total = i_bb + i_trace;
    TABLE_RWLOCK(dgc_table, write, unlock);

    return fragment_total;
}

/* returns the number of fragments removed */
uint
remove_patchable_fragments(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end)
{
    uint fragment_intersection_count;

    if (RUNNING_WITHOUT_CODE_CACHE()) /* case 7966: nothing to flush, ever */
        return 0;

    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1,
        "DGC: remove all fragments containing "PFX"-"PFX":\n",
        patch_start, patch_end);

    mutex_lock(&thread_initexit_lock);

    fragment_intersection_count = extract_fragment_intersection(patch_start, patch_end);
    if (fragment_intersection_count > 0) {
        update_thread_state();
        enter_couldbelinking(dcontext, NULL, false);

        remove_patchable_fragment_list(dcontext, patch_start, patch_end);

        RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "DGC: done removing %d fragments in "PFX"-"PFX"%s\n",
                    fragment_intersection_count, patch_start, patch_end);

        enter_nolinking(dcontext, NULL, false);
    } else {
        RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: no fragments found in "PFX"-"PFX"%s\n",
                    patch_start, patch_end);
    }
    mutex_unlock(&thread_initexit_lock);

    return fragment_intersection_count;
}
#endif /* JIJTOPT */


