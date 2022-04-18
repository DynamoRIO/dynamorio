/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2006-2007 Determina Corp. */

#include "globals.h"
#include "hashtable.h"

/* Returns the proper number of hash bits to have a capacity with the
 * given load for the given number of entries
 */
uint
hashtable_bits_given_entries(uint entries, uint load)
{
    /* Add 1 for the sentinel */
    return hashtable_num_bits(((entries + 1) * 100) / load);
}

/*******************************************************************************
 * GENERIC HASHTABLE INSTANTIATION
 *
 * We save code space by having hashtables that don't need special inlining
 * use the same code.  We also provide a more "normal" hashtable interface,
 * with key and payload separation and payload freeing.
 *
 * We only support caller synchronization currently (the caller should
 * use TABLE_RWLOCK) but we could provide a flag specifying whether
 * synch is intra-routine or not.
 */

/* 2 macros w/ name and types are duplicated in fragment.h -- keep in sync */
#define NAME_KEY generic
#define ENTRY_TYPE generic_entry_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define ENTRY_TAG(f) ((f)->key)
#define ENTRY_EMPTY NULL
/* we assume that 1 is not a valid value: that keys are in fact pointers */
#define ENTRY_SENTINEL ((generic_entry_t *)(ptr_uint_t)1)
#define ENTRY_IS_EMPTY(f) ((f) == NULL)
#define ENTRY_IS_SENTINEL(f) ((f) == ENTRY_SENTINEL)
#define ENTRY_IS_INVALID(f) (false) /* no invalid entries */
#define ENTRIES_ARE_EQUAL(t, f, g) (ENTRY_TAG(f) == ENTRY_TAG(g))
/* if usage becomes widespread we may need to parameterize this */
#define HASHTABLE_WHICH_HEAP(flags) (ACCT_OTHER)
#define HTLOCK_RANK table_rwlock
#define HASHTABLE_SUPPORT_PERSISTENCE 0

#include "hashtablex.h"
/* all defines are undef-ed at end of hashtablex.h */
#define GENERIC_ENTRY_IS_REAL(e) ((e) != NULL && (e) != (generic_entry_t *)(ptr_uint_t)1)

/* required routines for hashtable interface */

static void
hashtable_generic_init_internal_custom(dcontext_t *dcontext, generic_table_t *htable)
{ /* nothing */
}

static void
hashtable_generic_resized_custom(dcontext_t *dcontext, generic_table_t *htable,
                                 uint old_capacity, generic_entry_t **old_table,
                                 generic_entry_t **old_table_unaligned,
                                 uint old_ref_count, uint old_table_flags)
{ /* nothing */
}

#ifdef DEBUG
static void
hashtable_generic_study_custom(dcontext_t *dcontext, generic_table_t *htable,
                               uint entries_inc /*amnt table->entries was pre-inced*/)
{ /* nothing */
}
#endif

static void
hashtable_generic_free_entry(dcontext_t *dcontext, generic_table_t *htable,
                             generic_entry_t *entry)
{
    if (htable->free_payload_func != NULL)
        (*htable->free_payload_func)(dcontext, entry->payload);
    HEAP_TYPE_FREE(dcontext, entry, generic_entry_t, ACCT_OTHER, PROTECTED);
}

/* Wrapper routines to implement our generic_entry_t and free-func layer */

generic_table_t *
generic_hash_create(dcontext_t *dcontext, uint bits, uint load_factor_percent,
                    uint table_flags,
                    void (*free_payload_func)(dcontext_t *, void *)
                        _IF_DEBUG(const char *table_name))
{
    generic_table_t *table =
        HEAP_TYPE_ALLOC(dcontext, generic_table_t, ACCT_OTHER, PROTECTED);
    hashtable_generic_init(dcontext, table, bits, load_factor_percent,
                           (hash_function_t)INTERNAL_OPTION(alt_hash_func),
                           0 /* hash_mask_offset */, table_flags _IF_DEBUG(table_name));
    table->free_payload_func = free_payload_func;
    return table;
}

void
generic_hash_clear(dcontext_t *dcontext, generic_table_t *htable)
{
    hashtable_generic_clear(dcontext, htable);
}

void
generic_hash_destroy(dcontext_t *dcontext, generic_table_t *htable)
{
    /* FIXME: why doesn't hashtablex.h walk the table and call the free routine
     * in free() or in remove()?  It only seems to do it for range_remove().
     */
    uint i;
    generic_entry_t *e;
    for (i = 0; i < htable->capacity; i++) {
        e = htable->table[i];
        /* must check for sentinel */
        if (GENERIC_ENTRY_IS_REAL(e)) {
            hashtable_generic_free_entry(dcontext, htable, e);
        }
    }
    hashtable_generic_free(dcontext, htable);
    HEAP_TYPE_FREE(dcontext, htable, generic_table_t, ACCT_OTHER, PROTECTED);
}

void *
generic_hash_lookup(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key)
{
    generic_entry_t *e = hashtable_generic_lookup(dcontext, key, htable);
    if (e != NULL)
        return e->payload;
    return NULL;
}

void
generic_hash_add(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key,
                 void *payload)
{
    generic_entry_t *e =
        HEAP_TYPE_ALLOC(dcontext, generic_entry_t, ACCT_OTHER, PROTECTED);
    e->key = key;
    e->payload = payload;
    hashtable_generic_add(dcontext, e, htable);
}

bool
generic_hash_remove(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key)
{
    /* There is no remove routine that takes in a tag, nor one that frees the
     * payload, so we construct it.
     */
    generic_entry_t *e = hashtable_generic_lookup(dcontext, key, htable);
    if (e != NULL && hashtable_generic_remove(e, htable)) {
        hashtable_generic_free_entry(dcontext, htable, e);
        return true;
    }
    return false;
}

uint
generic_hash_range_remove(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t start,
                          ptr_uint_t end)
{
    return hashtable_generic_range_remove(dcontext, htable, start, end, NULL);
}

/* pass 0 to start.  returns -1 when there are no more entries. */
int
generic_hash_iterate_next(dcontext_t *dcontext, generic_table_t *htable, int iter,
                          OUT ptr_uint_t *key, OUT void **payload)
{
    int i;
    generic_entry_t *e = NULL;
    for (i = iter; i < (int)htable->capacity; i++) {
        e = htable->table[i];
        if (!GENERIC_ENTRY_IS_REAL(e))
            continue;
        else
            break;
    }
    if (i >= (int)htable->capacity)
        return -1;
    ASSERT(e != NULL);
    if (key != NULL)
        *key = e->key;
    if (payload != NULL)
        *payload = e->payload;
    return i + 1;
}

/* removes from the hashtable in a safe way during iteration.  returns an
 * updated iteration index to pass to generic_hash_iterate_next().
 */
int
generic_hash_iterate_remove(dcontext_t *dcontext, generic_table_t *htable, int iter,
                            ptr_uint_t key)
{
    generic_entry_t *e = hashtable_generic_lookup(dcontext, key, htable);
    uint hindex;
    generic_entry_t **rm = hashtable_generic_lookup_for_removal(e, htable, &hindex);
    int res = iter;
    if (rm != NULL) {
        if (hashtable_generic_remove_helper(htable, hindex, rm)) {
            /* pulled entry from start to here so skip it as we've already seen it */
        } else {
            /* pulled entry from below us, so step back */
            res--;
        }
        hashtable_generic_free_entry(dcontext, htable, e);
    }
    return res;
}

/*******************************************************************************
 * STRING KEY HASHTABLE INSTANTIATION
 *
 * We only support caller synchronization currently (the caller should
 * use TABLE_RWLOCK).
 */

/* 2 macros w/ name and types are duplicated in fragment.h -- keep in sync */
#define NAME_KEY strhash
#define ENTRY_TYPE strhash_entry_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define ENTRY_TAG(f) (ptr_uint_t)((f)->key)
#define ENTRY_EMPTY NULL
/* we assume that 1 is not a valid value: that keys are in fact pointers */
#define STRHASH_SENTINEL ((strhash_entry_t *)(ptr_uint_t)1)
#define ENTRY_SENTINEL STRHASH_SENTINEL
#define ENTRY_IS_EMPTY(f) ((f) == NULL)
#define ENTRY_IS_SENTINEL(f) ((f) == STRHASH_SENTINEL)
#define ENTRY_IS_INVALID(f) (false) /* no invalid entries */
#define TAGS_ARE_EQUAL(table, s1, s2) \
    (strhash_key_cmp(table, (const char *)(s1), (const char *)(s2)))
#define ENTRIES_ARE_EQUAL(table, f, g) (TAGS_ARE_EQUAL(table, (f)->key, (g)->key))
/* if usage becomes widespread we may need to parameterize this */
#define HASHTABLE_WHICH_HEAP(flags) (ACCT_OTHER)
#define HTLOCK_RANK table_rwlock
#define HASHTABLE_SUPPORT_PERSISTENCE 0

/* case sensitive by default */
#define STRHASH_CASE_INSENSITIVE HASHTABLE_CUSTOM_FLAGS_START

static inline bool
strhash_key_cmp(strhash_table_t *htable, const char *s1, const char *s2)
{
    if (TEST(STRHASH_CASE_INSENSITIVE, htable->table_flags))
        return strcasecmp(s1, s2) == 0;
    else
        return strcmp(s1, s2) == 0;
}

#include "hashtablex.h"
/* all defines are undef-ed at end of hashtablex.h */
#define STRHASH_ENTRY_IS_REAL(e) ((e) != NULL && (e) != STRHASH_SENTINEL)

/* required routines for hashtable interface */

static void
hashtable_strhash_init_internal_custom(dcontext_t *dcontext, strhash_table_t *htable)
{ /* nothing */
}

static void
hashtable_strhash_resized_custom(dcontext_t *dcontext, strhash_table_t *htable,
                                 uint old_capacity, strhash_entry_t **old_table,
                                 strhash_entry_t **old_table_unaligned,
                                 uint old_ref_count, uint old_table_flags)
{ /* nothing */
}

#ifdef DEBUG
static void
hashtable_strhash_study_custom(dcontext_t *dcontext, strhash_table_t *htable,
                               uint entries_inc /*amnt table->entries was pre-inced*/)
{ /* nothing */
}
#endif

static void
hashtable_strhash_free_entry(dcontext_t *dcontext, strhash_table_t *htable,
                             strhash_entry_t *entry)
{
    if (htable->free_payload_func != NULL)
        (*htable->free_payload_func)(entry->payload);
    HEAP_TYPE_FREE(dcontext, entry, strhash_entry_t, ACCT_OTHER, PROTECTED);
}

/* Wrapper routines to implement our strhash_entry_t and free-func layer */

strhash_table_t *
strhash_hash_create(dcontext_t *dcontext, uint bits, uint load_factor_percent,
                    uint table_flags,
                    void (*free_payload_func)(void *) _IF_DEBUG(const char *table_name))
{
    strhash_table_t *table =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, strhash_table_t, ACCT_OTHER, PROTECTED);
    hashtable_strhash_init(GLOBAL_DCONTEXT, table, bits, load_factor_percent,
                           TEST(STRHASH_CASE_INSENSITIVE, table_flags)
                               ? HASH_FUNCTION_STRING_NOCASE
                               : HASH_FUNCTION_STRING,
                           0 /* hash_mask_offset */, table_flags _IF_DEBUG(table_name));
    table->free_payload_func = free_payload_func;
    return table;
}

void
strhash_hash_clear(dcontext_t *dcontext, strhash_table_t *htable)
{
    hashtable_strhash_clear(dcontext, htable);
}

void
strhash_hash_destroy(dcontext_t *dcontext, strhash_table_t *htable)
{
    /* FIXME: why doesn't hashtablex.h walk the table and call the free routine
     * in free() or in remove()?  It only seems to do it for range_remove().
     */
    uint i;
    strhash_entry_t *e;
    for (i = 0; i < htable->capacity; i++) {
        e = htable->table[i];
        /* must check for sentinel */
        if (STRHASH_ENTRY_IS_REAL(e)) {
            hashtable_strhash_free_entry(dcontext, htable, e);
        }
    }
    hashtable_strhash_free(dcontext, htable);
    HEAP_TYPE_FREE(dcontext, htable, strhash_table_t, ACCT_OTHER, PROTECTED);
}

void *
strhash_hash_lookup(dcontext_t *dcontext, strhash_table_t *htable, const char *key)
{
    strhash_entry_t *e = hashtable_strhash_lookup(dcontext, (ptr_uint_t)key, htable);
    if (e != NULL)
        return e->payload;
    return NULL;
}

void
strhash_hash_add(dcontext_t *dcontext, strhash_table_t *htable, const char *key,
                 void *payload)
{
    strhash_entry_t *e =
        HEAP_TYPE_ALLOC(dcontext, strhash_entry_t, ACCT_OTHER, PROTECTED);
    e->key = key;
    e->payload = payload;
    hashtable_strhash_add(dcontext, e, htable);
}

bool
strhash_hash_remove(dcontext_t *dcontext, strhash_table_t *htable, const char *key)
{
    /* There is no remove routine that takes in a tag, nor one that frees the
     * payload, so we construct it
     */
    strhash_entry_t *e = hashtable_strhash_lookup(dcontext, (ptr_uint_t)key, htable);
    if (e != NULL && hashtable_strhash_remove(e, htable)) {
        hashtable_strhash_free_entry(dcontext, htable, e);
        return true;
    }
    return false;
}

/*******************************************************************************/

#ifdef HASHTABLE_STATISTICS

/* caller is responsible for any needed synchronization */
void
print_hashtable_stats(dcontext_t *dcontext, const char *is_final_str,
                      const char *is_trace_str, const char *lookup_routine_str,
                      const char *brtype_str, hashtable_statistics_t *lookup_stats)
{
    uint64 hits_stat = lookup_stats->hit_stat;
    uint64 total_lookups;
    if (lookup_stats->hit_stat < lookup_stats->collision_hit_stat) {
        /* HACK OVERFLOW: we special case here the case of a single overflow */
        /* assuming only one overflow, which is the only case on spec (GAP)  */
        hits_stat = (uint64)lookup_stats->hit_stat + UINT_MAX + 1;
    }
    total_lookups =
        hits_stat + lookup_stats->miss_stat + lookup_stats->collision_hit_stat;

    DOLOG(1, LOG_FRAGMENT | LOG_STATS, {
        uint miss_top = 0;
        uint miss_bottom = 0;
        uint hit_top = 0;
        uint hit_bottom = 0;
        uint col_top = 0;
        uint col_bottom = 0;
        if (total_lookups > 0) {
            divide_uint64_print(lookup_stats->miss_stat, total_lookups, false, 4,
                                &miss_top, &miss_bottom);
        }
        if (hits_stat > 0) {
            divide_uint64_print(lookup_stats->collision_hit_stat, hits_stat, false, 4,
                                &hit_top, &hit_bottom);
            divide_uint64_print(hits_stat + lookup_stats->collision_stat, hits_stat,
                                false, 4, &col_top, &col_bottom);
        }
        LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
            "%s %s table %s%s lookup hits%s: " UINT64_FORMAT_STRING ", "
            "misses: %u, total: " UINT64_FORMAT_STRING ", miss%%=%u.%.4u\n",
            is_final_str, is_trace_str, lookup_routine_str, brtype_str,
            (lookup_stats->hit_stat < lookup_stats->collision_hit_stat) ? "[OVFL]" : "",
            hits_stat, lookup_stats->miss_stat, total_lookups, miss_top, miss_bottom);
        LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
            "%s %s table %s%s "
            "collisions: %u, collision hits: %u, "
            ">2_or_miss: %u, overwrap: %u\n",
            is_final_str, is_trace_str, lookup_routine_str, brtype_str,
            lookup_stats->collision_stat,
            /* FIXME: collision hit stats are updated only when inlining IBL head */
            lookup_stats->collision_hit_stat,
            lookup_stats->collision_stat - lookup_stats->collision_hit_stat,
            lookup_stats->overwrap_stat);
        LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
            "%s %s table %s%s lookup "
            " coll%%=%u.%.4u, dyn.avgcoll=%u.%.4u\n",
            is_final_str, is_trace_str, lookup_routine_str, brtype_str, hit_top,
            hit_bottom, col_top, col_bottom);
        if (lookup_stats->race_condition_stat || lookup_stats->unlinked_count_stat) {
            LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
                "%s %s table %s%s inlined ibl unlinking races: %d, unlinked: %d\n",
                is_final_str, is_trace_str, lookup_routine_str, brtype_str,
                lookup_stats->race_condition_stat, lookup_stats->unlinked_count_stat);
        }
        if (lookup_stats->ib_stay_on_trace_stat) {
            uint ontrace_top = 0;
            uint ontrace_bottom = 0;
            uint lastexit_top = 0;
            uint lastexit_bottom = 0;
            uint speculate_lastexit_top = 0;
            uint speculate_lastexit_bottom = 0;
            /* indirect branch lookups */
            uint64 total_dynamic_ibs = total_lookups +
                lookup_stats->ib_stay_on_trace_stat +
                (DYNAMO_OPTION(speculate_last_exit)
                     ? lookup_stats->ib_trace_last_ibl_speculate_success
                     : 0);

            /* FIXME: add ib_stay_on_trace_stat_ovfl here */

            if (total_dynamic_ibs > 0) {
                divide_uint64_print(lookup_stats->ib_stay_on_trace_stat,
                                    total_dynamic_ibs, false, 4, &ontrace_top,
                                    &ontrace_bottom);
                divide_uint64_print(lookup_stats->ib_trace_last_ibl_exit,
                                    total_dynamic_ibs, false, 4, &lastexit_top,
                                    &lastexit_bottom);
                divide_uint64_print(lookup_stats->ib_trace_last_ibl_speculate_success,
                                    total_dynamic_ibs, false, 4, &speculate_lastexit_top,
                                    &speculate_lastexit_bottom);
            }

            /* all percentages here relative to IB lookups */
            /* ontrace is how often we stay on trace wrt all indirect branches */
            /* ontrace_fail is assuming IBL's that are not the last exit are due to
             * ontrace failures
             */
            /* lastexit is how many are on last IBL when it is not speculated */
            /* lastexit_speculate is how many are speculatively hit (as of all IBs) */
            LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
                "%s %s table %s%s stay on trace hit:%s %u, last_ibl: %u, "
                "ontrace%%=%u.%.4u, lastexit%%=%u.%.4u\n",
                is_final_str, is_trace_str, lookup_routine_str, brtype_str,
                lookup_stats->ib_stay_on_trace_stat_ovfl ? " OVFL" : "",
                lookup_stats->ib_stay_on_trace_stat, lookup_stats->ib_trace_last_ibl_exit,
                ontrace_top, ontrace_bottom, lastexit_top, lastexit_bottom);
            LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
                "%s %s table %s%s last trace exit speculation hit: %u, "
                "lastexit_ontrace%%=%u.%.4u(%%IB)\n",
                is_final_str, is_trace_str, lookup_routine_str, brtype_str,
                lookup_stats->ib_trace_last_ibl_speculate_success, speculate_lastexit_top,
                speculate_lastexit_bottom);
        }

        if (lookup_stats->ib_trace_last_ibl_exit > 0) {
            /* ignoring indirect branches that stayed on trace */
            /* FIXME: add ib_stay_on_trace_stat_ovfl here */
            uint speculate_only_lastexit_top = 0;
            uint speculate_only_lastexit_bottom = 0;
            uint lastexit_ibl_top = 0;
            uint lastexit_ibl_bottom = 0;
            uint speculate_lastexit_ibl_top = 0;
            uint speculate_lastexit_ibl_bottom = 0;

            uint64 total_dynamic_ibl_no_trace =
                total_lookups + lookup_stats->ib_trace_last_ibl_exit;
            if (total_dynamic_ibl_no_trace > 0) {
                divide_uint64_print(lookup_stats->ib_trace_last_ibl_exit,
                                    total_dynamic_ibl_no_trace, false, 4,
                                    &lastexit_ibl_top, &lastexit_ibl_bottom);
                divide_uint64_print(lookup_stats->ib_trace_last_ibl_speculate_success,
                                    total_dynamic_ibl_no_trace, false, 4,
                                    &speculate_lastexit_ibl_top,
                                    &speculate_lastexit_ibl_bottom);
            }

            /* ib_trace_last_ibl_exit includes all ib_trace_last_ibl_speculate_success */
            divide_uint64_print(lookup_stats->ib_trace_last_ibl_speculate_success,
                                lookup_stats->ib_trace_last_ibl_exit, false, 4,
                                &speculate_only_lastexit_top,
                                &speculate_only_lastexit_bottom);

            LOG(THREAD, LOG_FRAGMENT | LOG_STATS, 1,
                "%s %s table %s%s last trace exit speculation hit: %u, speculation miss: "
                "%u, lastexit%%=%u.%.4u(%%IBL), lastexit_succ%%=%u.%.4u(%%IBL), "
                "spec hit%%=%u.%.4u(%%last exit)\n",
                is_final_str, is_trace_str, lookup_routine_str, brtype_str,
                lookup_stats->ib_trace_last_ibl_speculate_success,
                lookup_stats->ib_trace_last_ibl_exit -
                    lookup_stats->ib_trace_last_ibl_speculate_success,
                lastexit_ibl_top, lastexit_ibl_bottom, speculate_lastexit_ibl_top,
                speculate_lastexit_ibl_bottom, speculate_only_lastexit_top,
                speculate_only_lastexit_bottom);
        }
    });
}

#endif /* HASHTABLE_STATISTICS */
