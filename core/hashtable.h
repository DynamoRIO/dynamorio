/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/*
 * hashtable.h
 *
 * Hashtable support common among all hashtables
 *
 */

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_ 1

/* Flags stored in the table_flags bitfield that are common to
 * all hashtables
 */
/* Is the table itself shared? */
#define HASHTABLE_SHARED 0x00000001
/* Are the entries in the table shared?
 * FIXME: right now this is never used independently from LOCKLESS_ACCESS,
 * perhaps get rid of it
 */
#define HASHTABLE_ENTRY_SHARED 0x00000002
/* Is the table accessed for reads concurrently without a lock? */
#define HASHTABLE_LOCKLESS_ACCESS 0x00000004
/* Is the table the primary, main list of its contents? */
#define HASHTABLE_NOT_PRIMARY_STORAGE 0x00000004
/* Is the table allocated in persistent memory? */
#define HASHTABLE_PERSISTENT 0x00000008
/* For debug builds: use per-entry statistics? */
#define HASHTABLE_USE_ENTRY_STATS 0x00000010
/* For non-performance-critical tables where we'd rather save memory */
#define HASHTABLE_RELAX_CLUSTER_CHECKS 0x00000020
/* Disallow adding or removing from the table, and acquire no locks on lookups
 * Only used for persisted coarse units.
 */
#define HASHTABLE_READ_ONLY 0x00000040
/* Align the main table to the cache line */
#define HASHTABLE_ALIGN_TABLE 0x00000080

/* Specific tables can add their own flags starting with this value
 * FIXME: any better way? how know when hit limit with <<?
 */
#define HASHTABLE_CUSTOM_FLAGS_START 0x00010000

/* Flags to NOT propagate to copies */
#define HASHTABLE_COPY_IGNORE_FLAGS (HASHTABLE_READ_ONLY)

/* single_thread_in_DR does not need rw locks since we do not access shared
 * tables from ibl while holding locks (we do so in a lockless manner)
 */
#define TABLE_NEEDS_LOCK(ptable)                      \
    (TEST(HASHTABLE_SHARED, (ptable)->table_flags) && \
     !TEST(HASHTABLE_READ_ONLY, (ptable)->table_flags))

/* is_local is currently debug-only and only affects asserts */
#define ASSERT_TABLE_SYNCHRONIZED(ptable, RW)                                 \
    ASSERT_OWN_##RW##_LOCK(TABLE_NEEDS_LOCK(ptable) && !(ptable)->is_local && \
                               !INTERNAL_OPTION(single_thread_in_DR),         \
                           &(ptable)->rwlock)

#define TABLE_RWLOCK(ptable, rw, lock)            \
    do {                                          \
        if (TABLE_NEEDS_LOCK(ptable))             \
            d_r_##rw##_##lock(&(ptable)->rwlock); \
    } while (0)

#define TABLE_MEMOP(table_flags, op) \
    (TEST((table_flags), HASHTABLE_PERSISTENT) ? heap_##op : nonpersistent_heap_##op)

#define TABLE_TYPE_MEMOP(table_flags, op, dc, type, which, protected) \
    (TEST((table_flags), HASHTABLE_PERSISTENT)                        \
         ? HEAP_TYPE_##op(dc, type, which, protected)                 \
         : NONPERSISTENT_HEAP_TYPE_##op(dc, type, which))

/* table capacity includes a sentinel so this is equivalent to
 * hash_index % (ftable->capacity - 1)
 */
#define HASH_INDEX_WRAPAROUND(hash_index, ftable) \
    ((hash_index) & (uint)(ftable->hash_mask >> ftable->hash_mask_offset))

#ifdef HASHTABLE_STATISTICS
/* Just a typechecking memset() wrapper */
#    define INIT_HASHTABLE_STATS(lookup_stats)                        \
        do {                                                          \
            memset(&lookup_stats, 0, sizeof(hashtable_statistics_t)); \
            ASSERT(lookup_stats.hit_stat == 0);                       \
        } while (0)
#    define HTABLE_STAT_INC(ftable, event) ftable->drlookup_stats.event##_stat++
#else
#    define HTABLE_STAT_INC(ftable, event) ((void)0)
#endif

#ifdef HASHTABLE_STATISTICS
typedef struct _hashtable_statistics_t {
    uint hit_stat;           /* hit count of hash table lookups */
    uint collision_hit_stat; /* lookups which collided but were hits */
    uint collision_stat;     /* total collision lookups (including misses) */
    uint miss_stat;          /* lookups which still ended up with a context switch */
    uint overwrap_stat;      /* lookups whose collision chain crosses overwrap sentinel */
    uint race_condition_stat; /* number of inilned ibl unlinking race condition cases */
    uint unlinked_count_stat; /* number of times unlinked */

    /* Stay on trace check combined with hashtable lookups gives the
     * dynamic count of indirect branches
     * FIXME: case 4817 where bb's could also cache one target
     * or see the CGO03 paper for multiple cached locations with similar stats
     */
    uint ib_stay_on_trace_stat; /* hash table lookup avoided - cached single target
                                 * location */
    /* This is likely to overflow in many cases, yet 64-bit in cache
     * increment is too heavy weight, should instead save last value
     * assuming that some rare, yet common enough event happens
     * between every 4 billion increments
     */
    uint ib_stay_on_trace_stat_last; /* last value */
    uint ib_stay_on_trace_stat_ovfl; /* number of times last was bigger than current */

    uint ib_trace_last_ibl_exit; /* hash table lookup for last exit - not cached */
    /* FIXME: add last and ovfl flag here as well */

    /* case 4817: add counter on extra check for converting IB into conditional
     * and add its success rate
     */
    uint ib_trace_last_ibl_speculate_success; /* stay-on-trace check success on last
                                               * exit */
    /* FIXME: add last and ovfl flag here as well */
} hashtable_statistics_t;

/* A table used by IBL parallel to the lookuptable to collect statistics */
typedef struct _fragment_stat_entry_t {
    /* hits * static collision length should equal */
    uint hits; /* hits in IBL */
    /* we need a filler for in-cache lookups */
    uint age; /* used to measure for how long an entry hasn't been used */
} fragment_stat_entry_t;
#endif /* HASHTABLE_STATISTICS */

/* Returns the proper number of hash bits to have a capacity with the
 * given load for the given number of entries
 */
uint
hashtable_bits_given_entries(uint entries, uint load);

/*******************************************************************************
 * GENERIC HASHTABLE INSTANTIATION
 */

/* To support arbitrary payloads versus tags we have 3 choices:
 * 1) assume payload struct has tag as pointer-sized field at offs 0
 * 2) wrap struct w/ tag,payload pair
 * 3) pass table_t to ENTRY_TAG and _ ARE_EQUAL
 * We go with #2 as it is the cleanest.  If users really need to save space
 * they can templatize their own hashtable code.
 */
typedef struct _generic_entry_t {
    ptr_uint_t key; /* called "tag" in hashtablex.h */
    void *payload;
} generic_entry_t;

/* macros w/ name and types are duplicated in utils.c -- keep in sync */
#define NAME_KEY generic
#define ENTRY_TYPE generic_entry_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define CUSTOM_FIELDS void (*free_payload_func)(dcontext_t *, void *);
#define HASHTABLEX_HEADER 1
#include "hashtablex.h"
#undef HASHTABLEX_HEADER

generic_table_t *
generic_hash_create(dcontext_t *dcontext, uint bits, uint load_factor_percent,
                    uint table_flags,
                    void (*free_payload_func)(dcontext_t *, void *)
                        _IF_DEBUG(const char *table_name));

void
generic_hash_destroy(dcontext_t *dcontext, generic_table_t *htable);

void
generic_hash_clear(dcontext_t *dcontext, generic_table_t *htable);

void *
generic_hash_lookup(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key);

void
generic_hash_add(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key,
                 void *payload);

bool
generic_hash_remove(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t key);

/* removes the key range [start,end) and returns the number of entries removed */
uint
generic_hash_range_remove(dcontext_t *dcontext, generic_table_t *htable, ptr_uint_t start,
                          ptr_uint_t end);

/* pass 0 to start.  returns -1 when there are no more entries. */
int
generic_hash_iterate_next(dcontext_t *dcontext, generic_table_t *htable, int iter,
                          OUT ptr_uint_t *key, OUT void **payload);

/* removes from the hashtable in a safe way during iteration.  returns an
 * updated iteration index to pass to generic_hash_iterate_next().
 */
int
generic_hash_iterate_remove(dcontext_t *dcontext, generic_table_t *htable, int iter,
                            ptr_uint_t key);

/*******************************************************************************
 * STRING KEY HASHTABLE INSTANTIATION
 */

typedef struct _strhash_entry_t {
    /* The keys are assumed to be allocated persistently by the user.
     * The table makes no copy of its own.
     */
    const char *key;
    void *payload;
} strhash_entry_t;

#define NAME_KEY strhash
#define ENTRY_TYPE strhash_entry_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define CUSTOM_FIELDS void (*free_payload_func)(void *);
#define HASHTABLEX_HEADER 1
#include "hashtablex.h"
#undef HASHTABLEX_HEADER

strhash_table_t *
strhash_hash_create(dcontext_t *dcontext, uint bits, uint load_factor_percent,
                    uint table_flags,
                    void (*free_payload_func)(void *) _IF_DEBUG(const char *table_name));

void
strhash_hash_destroy(dcontext_t *dcontext, strhash_table_t *htable);

void
strhash_hash_clear(dcontext_t *dcontext, strhash_table_t *htable);

void *
strhash_hash_lookup(dcontext_t *dcontext, strhash_table_t *htable, const char *key);

void
strhash_hash_add(dcontext_t *dcontext, strhash_table_t *htable, const char *key,
                 void *payload);

bool
strhash_hash_remove(dcontext_t *dcontext, strhash_table_t *htable, const char *key);

/*******************************************************************************/

#ifdef HASHTABLE_STATISTICS
/* caller is responsible for any needed synchronization */
void
print_hashtable_stats(dcontext_t *dcontext, const char *is_final_str,
                      const char *is_trace_str, const char *lookup_routine_str,
                      const char *brtype_str, hashtable_statistics_t *lookup_stats);
#endif

#endif /* _HASHTABLE_H_ */
