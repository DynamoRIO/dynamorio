/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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
 * hashtablex.h
 *
 * Generic hashtable support.
 *
 * Implementation is an open-address hashtable with sentinel.
 * Supports invalid slots for concurrent-access usage, in particular
 * lockless read concurrent access (such as for indirect branch lookup tables).
 * Also supports a synchronized lookup table.
 *
 * Usage: define the following macros, and then include this file.
 * It is meant to be included multiple times.
 * Define HASHTABLEX_HEADER to get the data struct and no implementation;
 * else, you'll get implementation but NOT the data struct.
 *
 * set HASHTABLE_USE_LOOKUPTABLE if using lookuptable
 * set HASHTABLE_ENTRY_STATS if want entry stats
 *
 * for struct:
 *   token NAME_KEY
 *   type ENTRY_TYPE
 *  if HASHTABLE_USE_LOOKUPTABLE is defined:
 *   type AUX_ENTRY_TYPE
 *  endif
 *   fields CUSTOM_FIELDS
 *
 * for main table:
 *   ptr_uint_t ENTRY_TAG(entry)
 *   bool ENTRY_IS_EMPTY(entry)
 *   bool ENTRY_IS_SENTINEL(entry)
 *   bool ENTRY_IS_INVALID(entry)
 *     assumption: invalid entries are only used with
 *     HASHTABLE_LOCKLESS_ACCESS tables
 *   bool ENTRIES_ARE_EQUAL(table, entry1, entry2)
 *     if using pointers, pointer equality is fine
 *   void ENTRY_SET_TO_ENTRY(table_entry, new_entry)
 *     This is optional; if omitted, "table_entry = new_entry" is used.
 *   ENTRY_TYPE ENTRY_EMPTY
 *   ENTRY_TYPE ENTRY_SENTINEL
 *     FIXME: to support structs we'll need lhs like AUX_ENTRY_SET_TO_SENTINEL
 *   (up to caller to ever set an entry to invalid)
 *   HTLOCK_RANK
 *     table_rwlock will work for most users.
 *     Needs higher rank than memory alloc locks.
 * optional for main table:
 *    bool TAGS_ARE_EQUAL(table, tag1, tag2)
 *
 * for lookuptable:
 *  if HASHTABLE_USE_LOOKUPTABLE is defined:
 *   ptr_uint_t AUX_ENTRY_TAG(aux_entry)
 *   bool AUX_ENTRY_IS_EMPTY(aux_entry)
 *     empty entry is assumed to be all zeros!
 *   bool AUX_ENTRY_IS_SENTINEL(aux_entry)
 *   bool AUX_ENTRY_IS_INVALID(aux_entry)
 *   bool AUX_PAYLOAD_IS_INVALID(dcontext, table, aux_entry)
 *   AUX_ENTRY_SET_TO_SENTINEL(aux_entry)
 *     takes lhs since may be struct and C only allows setting to {} in decl
 *   AUX_ENTRY_IS_SET_TO_ENTRY(aux_entry, entry)
 *   AUX_ENTRY_SET_TO_ENTRY(aux_entry, entry)
 *     takes lhs since may be struct and C only allows setting to {} in decl
 *   string AUX_ENTRY_FORMAT_STR
 *   printf-args AUX_ENTRY_FORMAT_ARGS(aux_entry)
 *  endif
 * we assume value semantics, i.e., we can copy entries via the = operator
 *
 * for HEAP_ACCOUNTING:
 *   heapacct-enum HASHTABLE_WHICH_HEAP(table_flags)
 *
 * to obtain persistence routines, define
 *   HASHTABLE_SUPPORT_PERSISTENCE
 *
 * for custom behavior we assume that these routines exist:
 *
 *    static void
 *    HTNAME(hashtable_,NAME_KEY,_init_internal_custom)
 *         (dcontext_t *dcontext,
 *          HTNAME(,NAME_KEY,_table_t) *htable
 *          _IFLOOKUP(bool use_lookup));
 *
 *    static void
 *    HTNAME(hashtable_,NAME_KEY,_resized_custom)
 *         (dcontext_t *dcontext,
 *          HTNAME(,NAME_KEY,_table_t) *htable,
 *          uint old_capacity, ENTRY_TYPE *old_table,
 *          ENTRY_TYPE *old_table_unaligned
 *          _IFLOOKUP(AUX_ENTRY_TYPE* lookuptable)
 *          _IFLOOKUP(byte *old_lookup_table_unaligned),
 *          uint old_ref_count,
 *          uint old_table_flags);
 *
 *   #ifdef DEBUG
 *    static void
 *    HTNAME(hashtable_,NAME_KEY,_study_custom)
 *         (dcontext_t *dcontext,
 *          HTNAME(,NAME_KEY,_table_t) *htable,
 *          uint entries_inc);
 *   #endif
 *
 *    static void
 *    HTNAME(hashtable_,NAME_KEY,_free_entry)
 *         (dcontext_t *dcontext, HTNAME(,NAME_KEY,_table_t) *table,
 *          ENTRY_TYPE entry);
 *
 * FIXME:
 * - use defines to weed out unused static routines
 * - rename unlinked_entries to invalid_entries
 * - remove references to "ibl"/"ibt" and make them general comments
 * - same with references to "fragment_t" or "f"
 * - caller uses expanded names "hashtable_fragment_init" -- should we
 *   provide a macro HASHTABLE_INIT(NAME_KEY, args...) or ok to mix
 *   the generated names here w/ hardcoded names in instantiations?
 * - provide wrapper routines for lookup, etc. that lock and unlock?
 *   see app_pc table usage for after call and rct target tables.
 * - eliminate the HASHTABLE_ENTRY_SHARED flag?
 */

#define EXPANDKEY(pre, key, post) pre##key##post
#define HTNAME(pre, key, post) EXPANDKEY(pre, key, post)
#define KEY_STRING STRINGIFY(NAME_KEY)

#define ENTRY_IS_REAL(e) \
    (!ENTRY_IS_EMPTY(e) && !ENTRY_IS_SENTINEL(e) && !ENTRY_IS_INVALID(e))

#ifdef HASHTABLE_USE_LOOKUPTABLE
#    define _IFLOOKUP(x) , x
#    define IFLOOKUP_ELSE(x, y) (x)
#else
#    define _IFLOOKUP(x) /* nothing */
#    define IFLOOKUP_ELSE(x, y) (y)
#endif

#ifndef TAGS_ARE_EQUAL
#    define TAGS_ARE_EQUAL(table, t1, t2) ((t1) == (t2))
#endif

/****************************************************************************/
#ifdef HASHTABLEX_HEADER

/* N.B.: if you change any fields here you must increase
 * PERSISTENT_CACHE_VERSION!
 */
typedef struct HTNAME(_, NAME_KEY, _table_t) {
    /* entries used from shared private IBL routines copy come first:
     * used to be lookuptable, now table for case 7691 */
    /* preferred location of a given tag is then at
     * lookuptable[(hash_func(tag) & hash_mask) >> hash_offset]
     */
    ptr_uint_t hash_mask; /* mask selects the index bits of hash value */
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    AUX_ENTRY_TYPE *lookuptable; /* allocation aligned within lookuptable_unaligned */
#    endif
    ENTRY_TYPE *table; /* hash_bits-bit addressed hash table */

    uint ref_count; /* when table is shared -- HASHTABLE_SHARED --
                     * # threads with active ptrs to the table */
    uint hash_bits;
    hash_function_t hash_func; /* selects hash function */
    uint hash_mask_offset;     /* ignores given number of LSB bits */
    uint capacity;             /* = 2^hash_bits + 1 sentinel */
    uint entries;
    /* FIXME: rename to invalid_entries to be more general */
    uint unlinked_entries;

    uint load_factor_percent; /* \alpha = load_factor_percent/100 */
    uint resize_threshold;    /*  = capacity * load_factor */

    uint groom_factor_percent; /* \gamma = groom_factor_percent/100 */
    uint groom_threshold;      /*  = capacity * groom_factor_percent */

    uint max_capacity_bits; /* log_2 of maximum size to grow table */

#    ifdef HASHTABLE_STATISTICS
    /* These refer only to accesses from DR lookups */
    hashtable_statistics_t drlookup_stats;

#        ifdef HASHTABLE_ENTRY_STATS
    /* Accesses from ibl */
    fragment_stat_entry_t *entry_stats;
    /* precomputed (entry_stats - lookup_table) for easy parallel
     * table access from IBL routines
     */
    uint entry_stats_to_lookup_table;
    uint added_since_dumped; /* clock handle - usually the same as
                              * delta in entries, unless we have removed
                              */
#        endif
#    endif                       /* HASHTABLE_STATISTICS */
    uint table_flags;            /* the HASHTABLE_* values are used here */
    read_write_lock_t rwlock;    /* shared tables should use a read/write lock */
    ENTRY_TYPE *table_unaligned; /* real alloc for table if HASHTABLE_ALIGN_TABLE */
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    byte *lookup_table_unaligned; /* real allocation unit for lookuptable */
#    endif
#    ifdef DEBUG
    const char *name;
    bool is_local; /* no lock needed since only known to this thread */
#    endif
    CUSTOM_FIELDS
} HTNAME(, NAME_KEY, _table_t);

/****************************************************************************/
#else /* implementation */

/* forward decls */
static bool HTNAME(hashtable_, NAME_KEY,
                   _check_size)(dcontext_t *dcontext,
                                HTNAME(, NAME_KEY, _table_t) * htable, uint add_now,
                                uint add_later);

static void HTNAME(hashtable_, NAME_KEY,
                   _init_internal_custom)(dcontext_t *dcontext,
                                          HTNAME(, NAME_KEY, _table_t) *
                                              htable _IFLOOKUP(bool use_lookup));

static void HTNAME(hashtable_, NAME_KEY, _resized_custom)(
    dcontext_t *dcontext, HTNAME(, NAME_KEY, _table_t) * htable, uint old_capacity,
    ENTRY_TYPE *old_table,
    ENTRY_TYPE *old_table_unaligned _IFLOOKUP(AUX_ENTRY_TYPE *lookuptable)
        _IFLOOKUP(byte *old_lookup_table_unaligned),
    uint old_ref_count, uint old_table_flags);

static uint HTNAME(hashtable_, NAME_KEY,
                   _unlinked_remove)(dcontext_t *dcontext,
                                     HTNAME(, NAME_KEY, _table_t) * table);

static void HTNAME(hashtable_, NAME_KEY,
                   _groom_table)(dcontext_t *dcontext,
                                 HTNAME(, NAME_KEY, _table_t) * table);

static inline void HTNAME(hashtable_, NAME_KEY,
                          _groom_helper)(dcontext_t *dcontext,
                                         HTNAME(, NAME_KEY, _table_t) * table);

static void HTNAME(hashtable_, NAME_KEY,
                   _free_entry)(dcontext_t *dcontext,
                                HTNAME(, NAME_KEY, _table_t) * table, ENTRY_TYPE entry);

#    ifdef DEBUG
static void HTNAME(hashtable_, NAME_KEY, _study)(dcontext_t *dcontext,
                                                 HTNAME(, NAME_KEY, _table_t) * htable,
                                                 uint entries_inc);

static void HTNAME(hashtable_, NAME_KEY,
                   _study_custom)(dcontext_t *dcontext,
                                  HTNAME(, NAME_KEY, _table_t) * htable,
                                  uint entries_inc);

#        ifdef HASHTABLE_ENTRY_STATS
void HTNAME(hashtable_, NAME_KEY,
            _dump_entry_stats)(dcontext_t *dcontext,
                               HTNAME(, NAME_KEY, _table_t) * htable);
#        endif

void HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext_t *dcontext,
                                               HTNAME(, NAME_KEY, _table_t) * htable);
#    endif /* DEBUG */

#    if defined(DEBUG) && defined(INTERNAL)
static void HTNAME(hashtable_, NAME_KEY,
                   _load_statistics)(dcontext_t *dcontext,
                                     HTNAME(, NAME_KEY, _table_t) * htable);
#    endif

/* get size in bytes padded for later cache alignment */
static inline size_t
HTNAME(hashtable_, NAME_KEY, _table_aligned_size)(uint table_capacity, uint flags)
{
    size_t size;
    /* we assume table size allows 32-bit indices */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(table_capacity * sizeof(ENTRY_TYPE))));
    size = table_capacity * sizeof(ENTRY_TYPE);
    if (TEST(HASHTABLE_ALIGN_TABLE, flags)) {
        /* aligned at least at 4, and may be aligned */
        size += proc_get_cache_line_size() - 4;
    }
    return size;
}

#    ifdef HASHTABLE_USE_LOOKUPTABLE
/* get size in bytes padded for later cache alignment */
static inline size_t
HTNAME(hashtable_, NAME_KEY, _lookuptable_aligned_size)(uint table_capacity)
{

    return table_capacity * sizeof(AUX_ENTRY_TYPE) + proc_get_cache_line_size() -
        4; /* aligned at least at 4, and may be aligned */
}
#    endif

/* callers should use either hashtable_init or hashtable_resize instead */
static void
HTNAME(hashtable_, NAME_KEY,
       _init_internal)(dcontext_t *dcontext, HTNAME(, NAME_KEY, _table_t) * table,
                       uint bits, uint load_factor_percent, hash_function_t func,
                       uint hash_mask_offset _IFLOOKUP(bool use_lookup))
{
    uint i;
    uint sentinel_index;
    size_t alloc_size;

    table->hash_bits = bits;
    table->hash_func = func;
    table->hash_mask_offset = hash_mask_offset;
    table->hash_mask = HASH_MASK(table->hash_bits) << hash_mask_offset;
    table->capacity = HASHTABLE_SIZE(table->hash_bits);

    /*
     * add an extra null_fragment at end to allow critical collision path
     * not to worry about table overwrap
     * FIXME: case 2147 to stay at power of 2 should use last element instead
     */
    table->capacity++;
    sentinel_index = table->capacity - 1;

    table->entries = 0;
    table->unlinked_entries = 0;

    /* STUDY: try different ratios for the load factor \alpha
     * It may save memory and it will not necessarily hurt performance:
     * better cache utilization may help us even if touch more entries
     * on a cache line.
     */
    table->load_factor_percent = load_factor_percent;
    /* be careful with integer overflows if ever let this in non-debug versions
     * ASSERT is not enough if this is a user controlled release option - needs
     * sanity check always
     */
    ASSERT(table->load_factor_percent > 0 && table->load_factor_percent < 100);
    table->resize_threshold = table->capacity * table->load_factor_percent / 100;

    table->groom_factor_percent = 0; /* grooming disabled */
    table->max_capacity_bits = 0;    /* unlimited */

    ASSERT(table->groom_factor_percent >= 0 /* 0 == disabled */);
    ASSERT(table->groom_factor_percent < 100);
    ASSERT(table->groom_factor_percent <= table->load_factor_percent);
    /* when groom factor < load/2 then we'd have to groom during table rehashing */
    ASSERT(table->groom_factor_percent == 0 ||
           table->groom_factor_percent * 2 > table->load_factor_percent);

    table->groom_threshold = table->capacity * table->groom_factor_percent / 100;

    alloc_size = HTNAME(hashtable_, NAME_KEY, _table_aligned_size)(table->capacity,
                                                                   table->table_flags);
    table->table_unaligned = (ENTRY_TYPE *)TABLE_MEMOP(table->table_flags, alloc)(
        dcontext, alloc_size HEAPACCT(HASHTABLE_WHICH_HEAP(table->table_flags)));
    if (TEST(HASHTABLE_ALIGN_TABLE, table->table_flags)) {
        ASSERT(ALIGNED(table->table_unaligned, 4)); /* guaranteed by heap_alloc */
        table->table = (ENTRY_TYPE *)ALIGN_FORWARD(table->table_unaligned,
                                                   proc_get_cache_line_size());
        ASSERT(ALIGNED(table->table, proc_get_cache_line_size()));
        ASSERT(ALIGNED(table->table, sizeof(ENTRY_TYPE)));
    } else
        table->table = table->table_unaligned;
    for (i = 0; i < table->capacity; i++)
        table->table[i] = ENTRY_EMPTY;
    /* overwrite last element to be a sentinel, reached only by assembly routines */
    table->table[sentinel_index] = ENTRY_SENTINEL;

    table->ref_count = 0;
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    if (use_lookup) {
        /* We need to allocate aligned size, yet there is no point in
         * calling heap_mmap for small sizes.  Instead we use normal
         * heap and guarantee alignment manually by padding.  For
         * larger sizes, we're already wasting a page beyond the table
         * size, so this will not waste more memory.
         */
        size_t lookup_table_allocation =
            HTNAME(hashtable_, NAME_KEY, _lookuptable_aligned_size)(table->capacity);

        table->lookup_table_unaligned = TABLE_MEMOP(table->table_flags, alloc)(
            dcontext,
            lookup_table_allocation HEAPACCT(HASHTABLE_WHICH_HEAP(table->table_flags)));
        ASSERT(ALIGNED(table->lookup_table_unaligned, 4)); /* guaranteed by heap_alloc */

        table->lookuptable = (AUX_ENTRY_TYPE *)ALIGN_FORWARD(
            table->lookup_table_unaligned, proc_get_cache_line_size());

        /* When the table is used for IBL, for correctness we need to
         * make sure it's allocated at a 4 byte aligned address. This
         * is required so that writes to the start_pc field during a
         * flush are atomic. If the start address is 4-byte aligned
         * then each 8 byte entry -- and the entry's two 4 byte fields
         * -- will also be aligned.  This should be guaranteed by
         * heap_alloc and we even align the table start address at
         * cache line.
         */
        ASSERT(ALIGNED(table->lookuptable, 4));

        /* make sure an entry doesn't cross a cache line boundary for performance */
        ASSERT(ALIGNED(table->lookuptable, sizeof(AUX_ENTRY_TYPE)));

        /* A minor point: save one extra cache line on the whole table
         * by making sure first entry is aligned to a cache line
         * boundary, otherwise if straddling we would need
         * 1 + table_size/cache_line_size to fit the whole table in d-cache
         */
        ASSERT(ALIGNED(table->lookuptable, proc_get_cache_line_size()));

        LOG(THREAD, LOG_HTABLE, 2,
            "hashtable_" KEY_STRING "_init %s lookup unaligned=" PFX " "
            "aligned=" PFX " allocated=%d\n",
            table->name, table->lookup_table_unaligned, table->lookuptable,
            lookup_table_allocation);

        /* set all to null_fragment {tag : 0, start_pc : 0} */
        memset(table->lookuptable, 0, table->capacity * sizeof(AUX_ENTRY_TYPE));
        ASSERT(AUX_ENTRY_IS_EMPTY(table->lookuptable[0]));
        /* set last to sentinel_fragment {tag : 0, start_pc : 1} */
        AUX_ENTRY_SET_TO_SENTINEL(table->lookuptable[sentinel_index]);
    } else {
        /* TODO: emit_utils assumes lookuptable will exist, but we can't match
         * the initializations.
         */
        table->lookup_table_unaligned = NULL;
        table->lookuptable = NULL;
    }
#    endif

    ASSERT(ENTRY_IS_EMPTY(ENTRY_EMPTY));
    ASSERT(ENTRY_IS_SENTINEL(ENTRY_SENTINEL));

    LOG(THREAD, LOG_HTABLE, 1,
        "hashtable_" KEY_STRING "_init %s htable=" PFX " bits=%d size=%d "
        "mask=" PFX " offset=%d load=%d%% resize=%d\n"
        "               %s %s " PFX " %s " PFX "  groom=%d%% groom_at=%d\n",
        table->name, table, bits, table->capacity, table->hash_mask,
        table->hash_mask_offset, table->load_factor_percent, table->resize_threshold,
        table->name, "table", table->table, IFLOOKUP_ELSE(use_lookup ? "lookup" : "", ""),
        IFLOOKUP_ELSE(table->lookuptable, NULL), table->groom_factor_percent,
        table->groom_threshold);

#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
    if (INTERNAL_OPTION(hashtable_ibl_entry_stats)) {
        if (TEST(HASHTABLE_USE_ENTRY_STATS, table->table_flags)) {
#            ifdef HASHTABLE_USE_LOOKUPTABLE
            ASSERT(sizeof(fragment_stat_entry_t) == sizeof(AUX_ENTRY_TYPE));
#            else
            ASSERT(sizeof(fragment_stat_entry_t) == sizeof(ENTRY_TYPE));
#            endif
            if (table->entry_stats != NULL) { /* resize */
                /* assuming resize is always doubling the table */
                /* FIXME: too error prone we should pass old capacity
                 * somewhere if case 2147 changes the table size
                 */
                uint old_capacity =
                    HASHTABLE_SIZE(table->hash_bits - 1) + 1 /* sentinel */;
                /* make sure we've printed the old stats, now losing them */
                HEAP_ARRAY_FREE(dcontext, table->entry_stats, fragment_stat_entry_t,
                                old_capacity, HASHTABLE_WHICH_HEAP(table->table_flags),
                                UNPROTECTED);
            }
            /* FIXME: either put in nonpersistent heap as appropriate, or
             * preserve across resets
             */
            table->entry_stats =
                HEAP_ARRAY_ALLOC(dcontext, fragment_stat_entry_t, table->capacity,
                                 HASHTABLE_WHICH_HEAP(table->table_flags), UNPROTECTED);
            ASSERT_TRUNCATE(table->entry_stats_to_lookup_table, uint,
                            ((byte *)table->entry_stats - (byte *)table->table));
            table->entry_stats_to_lookup_table = (uint)
#            ifdef HASHTABLE_USE_LOOKUPTABLE
                ((byte *)table->entry_stats - (byte *)table->lookuptable);
#            else
                ((byte *)table->entry_stats - (byte *)table->table);
#            endif
            memset(table->entry_stats, 0,
                   table->capacity * sizeof(fragment_stat_entry_t));
        }
    }
#        endif
#    endif

    HTNAME(hashtable_, NAME_KEY, _init_internal_custom)
    (dcontext, table _IFLOOKUP(use_lookup));
}

static void
HTNAME(hashtable_, NAME_KEY, _init)(dcontext_t *dcontext,
                                    HTNAME(, NAME_KEY, _table_t) * table, uint bits,
                                    uint load_factor_percent, hash_function_t func,
                                    uint hash_offset
                                        /* FIXME: turn this bool into a HASHTABLE_ flag */
                                        _IFLOOKUP(bool use_lookup),
                                    uint table_flags _IF_DEBUG(const char *table_name))
{
#    ifdef DEBUG
    table->name = table_name;
    table->is_local = false;
#    endif
    table->table_flags = table_flags;
#    ifdef HASHTABLE_STATISTICS
    /* indicate this is first time, not a resize */
#        ifdef HASHTABLE_ENTRY_STATS
    table->entry_stats = NULL;
    table->added_since_dumped = 0;
#        endif
#    endif
    ASSERT(dcontext != GLOBAL_DCONTEXT || TEST(HASHTABLE_SHARED, table_flags));
    HTNAME(hashtable_, NAME_KEY, _init_internal)
    (dcontext, table, bits, load_factor_percent, func, hash_offset _IFLOOKUP(use_lookup));
    ASSIGN_INIT_READWRITE_LOCK_FREE(table->rwlock, HTLOCK_RANK);
#    ifdef HASHTABLE_STATISTICS
    INIT_HASHTABLE_STATS(table->drlookup_stats);
#    endif
}

/* caller is responsible for any needed synchronization */
static void
HTNAME(hashtable_, NAME_KEY, _resize)(dcontext_t *dcontext,
                                      HTNAME(, NAME_KEY, _table_t) * table)
{
    HTNAME(hashtable_, NAME_KEY, _init_internal)
    (dcontext, table, table->hash_bits, table->load_factor_percent, table->hash_func,
     table->hash_mask_offset
         /* keep using lookup if used so far */
         _IFLOOKUP(table->lookuptable != 0));
}

static inline void
HTNAME(hashtable_, NAME_KEY,
       _free_table)(dcontext_t *alloc_dc,
                    ENTRY_TYPE *table_unaligned _IFLOOKUP(byte *lookup_table_unaligned),
                    uint flags, uint capacity)
{
    if (table_unaligned != NULL) {
        TABLE_MEMOP(flags, free)
        (alloc_dc, table_unaligned,
         HTNAME(hashtable_, NAME_KEY, _table_aligned_size)(capacity, flags)
             HEAPACCT(HASHTABLE_WHICH_HEAP(flags)));
    }
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    if (lookup_table_unaligned != NULL) {
        TABLE_MEMOP(flags, free)
        (alloc_dc, lookup_table_unaligned,
         HTNAME(hashtable_, NAME_KEY, _lookuptable_aligned_size)(capacity)
             HEAPACCT(HASHTABLE_WHICH_HEAP(flags)));
    }
#    endif
}

static void
HTNAME(hashtable_, NAME_KEY, _free)(dcontext_t *dcontext,
                                    HTNAME(, NAME_KEY, _table_t) * table)
{
    LOG(THREAD, LOG_HTABLE, 1,
        "hashtable_" KEY_STRING "_free %s table=" PFX " bits=%d size=%d "
        "load=%d%% resize=%d %s groom=%d%% groom_at=%d\n",
        table->name, table->table, table->hash_bits, table->capacity,
        table->load_factor_percent, table->resize_threshold,
        IFLOOKUP_ELSE((table->lookuptable != NULL) ? "use lookup" : "", ""),
        table->groom_factor_percent, table->groom_threshold);

#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
    if (INTERNAL_OPTION(hashtable_ibl_entry_stats)) {
        if (TEST(HASHTABLE_USE_ENTRY_STATS, table->table_flags)) {
            HEAP_ARRAY_FREE(dcontext, table->entry_stats, fragment_stat_entry_t,
                            table->capacity, HASHTABLE_WHICH_HEAP(table->table_flags),
                            UNPROTECTED);
        } else
            ASSERT(table->entry_stats == NULL);
    }
#        endif
#    endif /* HASHTABLE_STATISTICS */

    HTNAME(hashtable_, NAME_KEY, _free_table)
    (dcontext, table->table_unaligned _IFLOOKUP(table->lookup_table_unaligned),
     table->table_flags, table->capacity);
    table->table = NULL;
    table->table_unaligned = NULL;
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    table->lookuptable = NULL;
    table->lookup_table_unaligned = NULL;
#    endif
    DELETE_READWRITE_LOCK(table->rwlock);
}

/* Need to keep the cached start_pc_fragment consistent between lookuptable and
 * the htable.
 * Shared fragment IBTs: Unlinked lookup table entries are marked with
 * unlinked_fragment and
 * are expected to target a target_delete_entry.
 */
#    ifdef DEBUG
static inline void
HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext_t *dcontext,
                                                 HTNAME(, NAME_KEY, _table_t) * htable,
                                                 uint hindex)
{
#        ifdef HASHTABLE_USE_LOOKUPTABLE
    if (htable->lookuptable == NULL) {
        LOG(THREAD, LOG_HTABLE, 6, "[%u] tag=" PFX ")\n", hindex,
            ENTRY_TAG(htable->table[hindex]));
    } else {
        LOG(THREAD, LOG_HTABLE, 6, "[%u] " AUX_ENTRY_FORMAT_STR " tag=" PFX ")\n", hindex,
            AUX_ENTRY_FORMAT_ARGS(htable->lookuptable[hindex]),
            ENTRY_IS_REAL(htable->table[hindex]) ? ENTRY_TAG(htable->table[hindex]) : 0);
#        else
    LOG(THREAD, LOG_HTABLE, 6, "[%u] tag=" PFX ")\n", hindex,
        ENTRY_IS_REAL(htable->table[hindex]) ? ENTRY_TAG(htable->table[hindex]) : 0);
#        endif
        /* We can't assert that an IBL target isn't a trace head due to a race
         * between trace head marking and adding to a table. See the comments
         * in fragment_add_to_hashtable().
         */
        if (ENTRY_IS_INVALID(htable->table[hindex])) {
            ASSERT(TEST(HASHTABLE_NOT_PRIMARY_STORAGE, htable->table_flags));
            ASSERT(TEST(HASHTABLE_LOCKLESS_ACCESS, htable->table_flags));
#        ifdef HASHTABLE_USE_LOOKUPTABLE
            ASSERT(AUX_ENTRY_IS_INVALID(htable->lookuptable[hindex]));
#        endif
        }
#        ifdef HASHTABLE_USE_LOOKUPTABLE
        /* "Inclusive hierarachy" lookup tables -- per-type tables not attached
         * to a table such as the BB table -- are simpler to reason about
         * since we have more latitude setting fragment_t ptrs and so can ensure
         * that the entry is always sync-ed with the corresponding fragment_t*.
         * For a non-"inclusive hierarachy" table, only when the entry has not
         * been unlinked (and so doesn't point to target_delete) can we expect
         * the lookup table and fragment_t* to be in-sync.
         */
        else if (TEST(HASHTABLE_NOT_PRIMARY_STORAGE, htable->table_flags) ||
                 AUX_PAYLOAD_IS_INVALID(dcontext, htable, htable->lookuptable[hindex])) {
            ASSERT(AUX_ENTRY_IS_SET_TO_ENTRY(htable->lookuptable[hindex],
                                             htable->table[hindex]));
        }
        /* Shouldn't be needed but could catch errors so leaving in. */
        else {
            ASSERT(AUX_PAYLOAD_IS_INVALID(dcontext, htable, htable->lookuptable[hindex]));
        }
    }
#        endif
}
#    endif

/* Returns entry if tag found;
 *  Otherwise returns an "empty" entry -- does NOT return NULL!
 * Shared tables: It can be the case that an "invalid" entry is returned,
 *     which also has a tag of 0. This can occur when htable has a lookup table
 *     and 'tag' is for a fragment that was unlinked due to shared deletion.
 */
/* CHECK this routine is partially specialized, otherwise turn it into a macro */
static /* want to inline but > limit in fragment_lookup */ ENTRY_TYPE
HTNAME(hashtable_, NAME_KEY, _lookup)(dcontext_t *dcontext, ptr_uint_t tag,
                                      HTNAME(, NAME_KEY, _table_t) * htable)
{
    ENTRY_TYPE e;
    ptr_uint_t ftag;
    uint hindex = HASH_FUNC(tag, htable);
#    ifdef HASHTABLE_STATISTICS
    uint collision_len = 0;
#    endif
    ASSERT_TABLE_SYNCHRONIZED(htable, READWRITE); /* requires read (or write) lock */
    e = htable->table[hindex];

    DODEBUG({
        HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, htable, hindex);
    });
    while (!ENTRY_IS_EMPTY(e)) {
        DODEBUG({
            HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, htable, hindex);
        });
        /* If a FAKE_TAG is present in lookuptable as an unlinked marker, use the
         * table entry. */
#    ifdef HASHTABLE_USE_LOOKUPTABLE
        ftag = (htable->lookuptable != NULL &&
                !AUX_ENTRY_IS_INVALID(htable->lookuptable[hindex]))
            ? AUX_ENTRY_TAG(htable->lookuptable[hindex])
            : ENTRY_TAG(e);
#    else
        ftag = ENTRY_TAG(e);
#    endif
        /* FIXME future frags have a 0 tag and that's why we have
         * to compare with null_fragment for end of chain in table[]
         * whenever future frags go to their own table, this code should
         * be reworked to touch lookuptable only -- i.e. become while
         * (ftag != NULL_TAG).
         */
        if (TAGS_ARE_EQUAL(htable, ftag, tag)) {
#    ifdef HASHTABLE_STATISTICS
            if (collision_len > 0)
                HTABLE_STAT_INC(htable, collision_hit);
            HTABLE_STAT_INC(htable, hit);
#    endif
            return e;
        }
        /* collision */
#    ifdef HASHTABLE_STATISTICS
        LOG(THREAD_GET, LOG_HTABLE, 6,
            "(hashtable_" KEY_STRING "_lookup: collision sequence " PFX " "
            "%u [len=%u])\n",
            tag, hindex, collision_len);
        collision_len++;
        HTABLE_STAT_INC(htable, collision);
#    endif
        hindex = HASH_INDEX_WRAPAROUND(hindex + 1, htable);
        e = htable->table[hindex];
        DODEBUG({
            HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, htable, hindex);
        });
    }
    HTABLE_STAT_INC(htable, miss);
    return e;
}

/* Convenience routine that grabs the read lock and does a lookup */
static inline ENTRY_TYPE
HTNAME(hashtable_, NAME_KEY, _rlookup)(dcontext_t *dcontext, ptr_uint_t tag,
                                       HTNAME(, NAME_KEY, _table_t) * htable)
{
    ENTRY_TYPE e;
    TABLE_RWLOCK(htable, read, lock);
    e = HTNAME(hashtable_, NAME_KEY, _lookup)(dcontext, tag, htable);
    TABLE_RWLOCK(htable, read, unlock);
    return e;
}

/* add f to a fragment table
 * returns whether resized the table or not
 * N.B.: this routine will recursively call itself via check_table_size if the
 * table is resized
 */
static inline bool
HTNAME(hashtable_, NAME_KEY, _add)(dcontext_t *dcontext, ENTRY_TYPE e,
                                   HTNAME(, NAME_KEY, _table_t) * table)
{
    uint hindex;
    bool resized;
    DEBUG_DECLARE(uint cluster_len = 0;)

    ASSERT_TABLE_SYNCHRONIZED(table, WRITE); /* add requires write lock */

    ASSERT(!TEST(HASHTABLE_READ_ONLY, table->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, table->table_flags))
        return false;

    /* ensure higher-level synch not allowing any races between lookup and add */
    /* Shared fragment IBTs: Tolerate unlinked markers. */
    ASSERT(!ENTRY_IS_REAL(
        HTNAME(hashtable_, NAME_KEY, _lookup)(dcontext, ENTRY_TAG(e), table)));

    /* check_table_size increments table->entries for us and ensures
     * we have enough space.  don't grab any properties of table prior to this
     * call, like hindex, as it will change if resized.
     */
    resized = !HTNAME(hashtable_, NAME_KEY, _check_size)(dcontext, table, 1, 0);

    hindex = HASH_FUNC(ENTRY_TAG(e), table);
    /* find an empty null slot */
    do {
        LOG(THREAD_GET, LOG_HTABLE, 4,
            "hashtable_" KEY_STRING "_add(" PFX ") mask=" PFX " offset=%d trying " PFX
            ":\n",
            ENTRY_TAG(e), table->hash_mask, table->hash_mask_offset, hindex);

        /* We could use !ENTRY_IS_REAL() here but it could make what we're doing
         * more confusing since the &unlinked_fragment is handled a little
         * differently. So we enumerate the cases to make things more
         * apparent.
         */
        if (ENTRY_IS_EMPTY(table->table[hindex]))
            break;
        /* Replace pending deletion entries in a private table but not
         * in a shared table. */
        if (ENTRY_IS_INVALID(table->table[hindex])) {
            /* FIXME We cannot blindly overwrite an unlinked entry for a shared
             * table. We overwrite an entry only when we know that since the
             * unlink, every thread has exited the cache at least once (just as
             * with shared deletion). Refcounting on a per-entry basis is
             * very likely not worth the potential gain.
             *
             * A broader approach that may be effective enough piggybacks
             * on shared deletion. When a flusher unlinks entries in a
             * shared table, the table is marked FRAG_NO_CLOBBER_UNLINK --
             * this prevents unlinked entries from being replaced. After a
             * thread decrements a shared deletion refcount via
             * check_flush_queue(), it checks if the shared deletion queuee
             * is empty. If the queue is empty, then we know that since the
             * last flush (and table unlinks) all threads have
             * exited the cache at least once, so any shared tables marked
             * as FRAG_NO_CLOBBER_UNLINK can have that flag cleared.
             */
            if (!TEST(HASHTABLE_SHARED, table->table_flags)) {
                ASSERT(table->unlinked_entries > 0);
                ASSERT(TEST(HASHTABLE_LOCKLESS_ACCESS, table->table_flags));
#    ifdef HASHTABLE_USE_LOOKUPTABLE
                ASSERT(
                    AUX_PAYLOAD_IS_INVALID(dcontext, table, table->lookuptable[hindex]));
                ASSERT(AUX_ENTRY_IS_INVALID(table->lookuptable[hindex]));
                LOG(THREAD_GET, LOG_HTABLE, 4,
                    "   replace target_delete "
                    "entry " AUX_ENTRY_FORMAT_STR "\n",
                    AUX_ENTRY_FORMAT_ARGS(table->lookuptable[hindex]));
#    endif
                STATS_INC(num_ibt_replace_unlinked_fragments);
                break;
            }
        }
        DODEBUG({ ++cluster_len; });
        hindex = HASH_INDEX_WRAPAROUND(hindex + 1, table);
    } while (1);

    /* FIXME: case 4814 we may want to flush the table if we are running into a too long
     * collision cluster
     */
    DOLOG(1, LOG_HTABLE, {
        if (cluster_len > HASHTABLE_SIZE((1 + table->hash_bits) / 2)) {
            LOG(THREAD_GET, LOG_HTABLE,
                cluster_len > HASHTABLE_SIZE((1 + table->hash_bits) / 2 + 1) ? 1U : 2U,
                "hashtable_" KEY_STRING "_add: long collision sequence len=%u"
                "for " PFX " %s table[%u] capacity=%d entries=%d)\n",
                cluster_len, ENTRY_TAG(e), table->name, hindex, table->capacity,
                table->entries);
        }
    });

    /* If we had uniformly distributed hash functions, expected max length is
     * sqrt(capacity.\pi/8) see Knuth vol.3, FIXME we double below because this
     * sometimes ASSERTS for the shared_future_table at the 10 to 11 bits
     * transition (seems to be fine at larger sizes)
     * bug2241 we add an additional 64 to handle problems in private future
     * tables at small sizes, for bug2271 we disable for tables using the
     * _NONE hash function (currently private bb and trace) when we have no
     * shared fragments
     */
#    ifdef DEBUG
    if (!TEST(HASHTABLE_RELAX_CLUSTER_CHECKS, table->table_flags) &&
        (table->hash_func != HASH_FUNCTION_NONE || SHARED_FRAGMENTS_ENABLED())) {
        uint max_cluster_len =
            HASHTABLE_SIZE((1 + table->hash_bits) / 2 + 1 /* double */) + 64;
        if (!(cluster_len <= max_cluster_len)) {
            DO_ONCE({ /* once reach this may fire many times in a row */
                      /* always want to know which table this is */
                      SYSLOG_INTERNAL_WARNING(
                          "cluster length assert: %s cluster=%d vs %d,"
                          " cap=%d, entries=%d",
                          table->name, cluster_len, max_cluster_len, table->capacity,
                          table->entries);
                      DOLOG(3, LOG_HTABLE, {
                          HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table);
                      });
                      ASSERT_CURIOSITY(false && "table collision cluster is too large");
            });
        }
    }
#    endif
    /* actually add the entry
     * add to lookuptable first to avoid race spoiling ASSERT_CONSISTENCY -- better
     * for other thread to miss in lookup
     */
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    if (table->lookuptable != NULL) {
        AUX_ENTRY_SET_TO_ENTRY(table->lookuptable[hindex], e);
        ASSERT(!AUX_ENTRY_IS_INVALID(table->lookuptable[hindex]));
    }
#    endif
    if (ENTRY_IS_INVALID(table->table[hindex]))
        table->unlinked_entries--;
#    ifdef ENTRY_SET_TO_ENTRY
    ENTRY_SET_TO_ENTRY(table->table[hindex], e);
#    else
    table->table[hindex] = e;
#    endif
    ASSERT(!ENTRY_IS_INVALID(table->table[hindex]));
    LOG(THREAD_GET, LOG_HTABLE, 4,
        "hashtable_" KEY_STRING "_add: added " PFX " to %s at table[%u]\n", ENTRY_TAG(e),
        table->name, hindex);

    return resized;
}

/* Ensures that the table has enough room for add_now+add_later new entries.
 * If not, the table is resized and rehashed into its new space.
 * The table's entry count is incremented by add_now but nothing changes
 * with respect to add_later other than making space.
 * Returns false if it had to re-size the table.
 * Caller must hold the write lock, if this is a shared table!
 * N.B.: this routine will recursively be called when resizing, b/c it
 * calls fragment_add_to_hashtable, which calls back, but should never
 * trigger another resize.
 */
static bool
HTNAME(hashtable_, NAME_KEY, _check_size)(dcontext_t *dcontext,
                                          HTNAME(, NAME_KEY, _table_t) * table,
                                          uint add_now, uint add_later)
{
    dcontext_t *alloc_dc = FRAGMENT_TABLE_ALLOC_DC(dcontext, table->table_flags);
    uint entries;
    bool shared_lockless =
        TESTALL(HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED | HASHTABLE_LOCKLESS_ACCESS,
                table->table_flags);
    /* FIXME: too many assumptions here that if lockless, then a lookuptable
     * is always used, etc.
     */
    bool lockless =
        TESTALL(HASHTABLE_LOCKLESS_ACCESS | HASHTABLE_ENTRY_SHARED, table->table_flags);

    /* write lock must be held */
    ASSERT_TABLE_SYNCHRONIZED(table, WRITE);

    ASSERT(!TEST(HASHTABLE_READ_ONLY, table->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, table->table_flags))
        return false;

    /* check flush threshold to see if we'd want to flush hashtable */
    if (table->entries > table->groom_threshold && table->groom_threshold > 0) {
        HTNAME(hashtable_, NAME_KEY, _groom_table)(dcontext, table);
        /* FIXME Grooming a table in-place doesn't work for a shared IBT table.
         * To make it work, we should a) realloc a same-sized table
         * b) re-add all entries in the old table, c) add the old
         * to the dead list, and d) then call groom_table(). (b) is
         * needed because we can't assume that groom_table() will
         * always flush the entire table.
         *
         * Or we could groom the old table -- making sure that it's done
         * thread-safely -- and then copy into the new table, requiring
         * fewer re-adds.
         *
         * This is covered by case 5838.
         */
    }
    /* FIXME: case 4814 we should move clock handles here
     */
#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
    /* dump per entry hit statistics regularly to see working set */
    if (table->entry_stats != NULL) {
        table->added_since_dumped += add_now;
        if (table->added_since_dumped >= INTERNAL_OPTION(hashtable_ibl_study_interval)) {
            LOG(THREAD, LOG_HTABLE, 2, "dump_and_clean_entry_statistics %s added %d\n",
                table->name, table->added_since_dumped);
            HTNAME(hashtable_, NAME_KEY, _dump_entry_stats)(dcontext, table);
            table->added_since_dumped = 0;
        }
    }
#        endif
#    endif /* HASHTABLE_STATISTICS */

    /* Pretend we had full # entries; we'll lower later */
    table->entries += add_now + add_later;

    /* check resize threshold to see if a larger hashtable is needed
     * or that we may want to reset the table.
     *
     * For an IBT table, the # unlinked entries needs to be checked also.
     * For a shared table, they cannot be replaced so they are effectively
     * real entries. For a private table, they can be replaced but those
     * that remain present can lengthen collision chain lengths.
     *
     * NOTE: unlinked_entries is used only when shared targets are
     * stored in IBT tables, since unlinking in those tables is NOT
     * followed up by a full removal of the unlinked entries.
     */
    entries = lockless ? table->entries + table->unlinked_entries : table->entries;
    if (entries > table->resize_threshold) {
        ENTRY_TYPE *old_table = table->table;
        ENTRY_TYPE *old_table_unaligned = table->table_unaligned;
        uint old_capacity = table->capacity;
#    ifdef HASHTABLE_USE_LOOKUPTABLE
        AUX_ENTRY_TYPE *old_lookuptable_to_nullify = table->lookuptable;
        byte *old_lookup_table_unaligned = table->lookup_table_unaligned;
#    endif
        uint old_ref_count = table->ref_count - 1; /* remove this thread's
                                                    * reference to the table */
        uint i;
        DEBUG_DECLARE(uint old_entries = table->entries - add_later;)

        DODEBUG({
            /* study before resizing */
            HTNAME(hashtable_, NAME_KEY, _study)(dcontext, table, add_now + add_later);
        });

#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
        if (table->entry_stats != NULL) {
            LOG(THREAD, LOG_HTABLE, 2, "dump_and_clean_entry_statistics %s resized\n",
                table->name);
            HTNAME(hashtable_, NAME_KEY, _dump_entry_stats)(dcontext, table);
        }
#        endif
#    endif /* HASHTABLE_STATISTICS */

        /* For a shared IBT table, the flushing/grooming is done after
         * a resize -- we can't groom in-place. */
        if (table->hash_bits == table->max_capacity_bits && !shared_lockless) {
            STATS_INC(num_ibt_max_capacity);
            HTNAME(hashtable_, NAME_KEY, _groom_helper)(dcontext, table);
            return true; /* == did not resize the table */
        }

        /* For an IBT table, if the # unlinked entries is what kicked
         * in the resize then check the # actual entries and do
         * a same-size realloc to eliminate the unlinked entries. Also,
         * if we've reached the max size, do a same-size realloc.
         *
         * Otherwise, double the table size.
         */
        if (lockless &&
            (table->entries <= table->resize_threshold ||
             table->hash_bits == table->max_capacity_bits)) {
            STATS_INC(num_same_size_ibt_table_resizes);
        } else {
            DEBUG_DECLARE(uint old_bits = table->hash_bits;)
            ASSERT(table->resize_threshold ==
                   (HASHTABLE_SIZE(table->hash_bits) + 1 /*sentinel*/) *
                       table->load_factor_percent / 100);
            while (entries > (HASHTABLE_SIZE(table->hash_bits) + 1 /*sentinel*/) *
                           table->load_factor_percent / 100 &&
                   table->hash_bits != table->max_capacity_bits) {
                table->hash_bits++; /* double the size */
            }
            ASSERT(table->hash_bits > old_bits);
        }

        HTNAME(hashtable_, NAME_KEY, _resize)(alloc_dc, table);
        /* will be incremented by rehashing below -- in fact, by
         * recursive calls to this routine from
         * fragment_add_to_hashtable -- see warning below
         */
        table->entries = 0;
        table->unlinked_entries = 0;
        ASSERT(table->ref_count == 0);

        /* can't just memcpy, must rehash */
        /* For open address table rehash should first find an empty
         * slot and start from there so that we make sure that entries
         * that used to find a hit on the first lookup continue to do
         * so instead of creating even longer collision parking lots.
         * XXX: can we do better?
         */
        for (i = 0; i < old_capacity; i++) {
            ENTRY_TYPE e = old_table[i];
            if (!ENTRY_IS_REAL(e))
                continue;

            /* Don't carry over frags that point to target_delete. This can
             * happen in any IBT table, shared or private, that targets
             * shared fragments.
             */
            if (lockless && ENTRY_IS_INVALID(old_table[i])) {
                LOG(GLOBAL, LOG_HTABLE, 1,
                    "Don't copy tag " PFX " in %s[%u] across a resize\n", ENTRY_TAG(e),
                    table->name, i);
                DODEBUG({ old_entries--; });
                STATS_INC(num_ibt_unlinked_entries_not_moved);
                continue;
            }
#    ifdef HASHTABLE_USE_LOOKUPTABLE
            if (lockless && old_lookuptable_to_nullify != NULL &&
                AUX_ENTRY_IS_INVALID(old_lookuptable_to_nullify[i])) {
                LOG(GLOBAL, LOG_HTABLE, 1,
                    "Don't copy tag " PFX " in %s[%u] across a resize\n", ENTRY_TAG(e),
                    table->name, i);
                ASSERT(AUX_PAYLOAD_IS_INVALID(dcontext, table,
                                              old_lookuptable_to_nullify[i]));
                DODEBUG({ old_entries--; });
                STATS_INC(num_ibt_unlinked_entries_not_moved);
                continue;
            }
#    endif

            /* N.B.: this routine will call us again, but we assume the
             * resize will NEVER be triggered, since we hold the write lock
             * and set table->entries to 0.
             * We could have a special add routine that doesn't call us,
             * I suppose.
             */
            HTNAME(hashtable_, NAME_KEY, _add)(dcontext, e, table);
        }
        table->entries += add_now; /* for about-to-be-added fragment(s) */
        /* (add_later will be added later though calls to this same routine) */
        /* For a shared IBT table, the flushing/grooming is done after a
         * same-size realloc -- we can't groom the original table in-place.
         * Groom when we know that the table was just bumped up in size
         * earlier in the routine -- compare the old and new sizes to
         * determine.
         */
        if (table->hash_bits == table->max_capacity_bits &&
            old_capacity == table->capacity) {
            STATS_INC(num_ibt_max_capacity);
            ASSERT(shared_lockless);
            HTNAME(hashtable_, NAME_KEY, _groom_helper)(dcontext, table);
        } else {
            /* should have rehashed all old entries into new table */
            ASSERT(table->entries == old_entries);
        }

        LOG(THREAD, LOG_HTABLE, 2,
            "%s hashtable resized at %d entries from capacity %d to %d\n", table->name,
            table->entries, old_capacity, table->capacity);

        /* Since readers now synchronize with writers of shared htables,
         * we can now delete old htable even when sharing.
         */

        DODEBUG({
            LOG(THREAD, LOG_HTABLE, 1, "Rehashed %s table\n", table->name);
            /* study after rehashing
             * ok to become reader for study while a writer
             */
            HTNAME(hashtable_, NAME_KEY, _study)(dcontext, table, add_now);

            DOLOG(3, LOG_HTABLE, {
                HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table);
            });
        });

        /* Shared IBT tables are resized at safe points, not here, since
         * they are accessed while in-cache, unlike other shared tables
         * such as the shared BB or shared trace table.
         */
        if (!shared_lockless) {
            HTNAME(hashtable_, NAME_KEY, _free_table)
            (alloc_dc, old_table_unaligned _IFLOOKUP(old_lookup_table_unaligned),
             table->table_flags, old_capacity);
        } else {
            if (old_ref_count == 0) {
                /* Note that a write lock is held on the table, so no danger of
                 * a double free. */
                HTNAME(hashtable_, NAME_KEY, _free_table)
                (GLOBAL_DCONTEXT,
                 old_table_unaligned _IFLOOKUP(old_lookup_table_unaligned),
                 table->table_flags, old_capacity);
                STATS_INC(num_shared_ibt_tables_freed_immediately);
            }
        }

        HTNAME(hashtable_, NAME_KEY, _resized_custom)
        (dcontext, table, old_capacity, old_table,
         old_table_unaligned _IFLOOKUP(old_lookuptable_to_nullify)
             _IFLOOKUP(old_lookup_table_unaligned),
         old_ref_count, table->table_flags);

        return false; /* == resized the table */
    }
    /* If there are too many unlinked markers cluttering the table, remove them. */
    else if (table->unlinked_entries > 0 &&
             ((INTERNAL_OPTION(rehash_unlinked_threshold) < 100 &&
               INTERNAL_OPTION(rehash_unlinked_threshold) <
                   (100 * table->unlinked_entries /
                    (table->entries + table->unlinked_entries))) ||
              INTERNAL_OPTION(rehash_unlinked_always))) {
        /* Currently, such markers should be present only when shared BBs are IB
         * targets or traces are shared. */
        ASSERT(SHARED_IB_TARGETS());
        ASSERT(TESTALL(HASHTABLE_ENTRY_SHARED | HASHTABLE_LOCKLESS_ACCESS,
                       table->table_flags));
        STATS_INC(num_ibt_table_rehashes);
        LOG(THREAD, LOG_HTABLE, 1, "Rehash table %s: linked %u, unlinked %u\n",
            table->name, table->entries, table->unlinked_entries);
        /* we will inc for these at a "later" time */
        table->entries -= add_later;
        /* entries was incremented earlier but the new entry hasn't been
         * added yet. We decrement before and re-inc after so that any calls
         * to study_hashtable in the remove routine succeed.
         */
        DODEBUG({ table->entries -= add_now; });
        HTNAME(hashtable_, NAME_KEY, _unlinked_remove)(dcontext, table);
        DODEBUG({ table->entries += add_now; });
    } else {
        /* we will inc for these at a "later" time */
        table->entries -= add_later;
    }
    return true; /* == did not resize the table */
}

/* Return pointer to the record for fragment in its collision chain (table or next field)
   or NULL if not found */
static inline ENTRY_TYPE *
HTNAME(hashtable_, NAME_KEY, _lookup_for_removal)(ENTRY_TYPE fr,
                                                  HTNAME(, NAME_KEY, _table_t) * htable,
                                                  uint *rhindex)
{
    uint hindex = HASH_FUNC(ENTRY_TAG(fr), htable);
    ENTRY_TYPE *pg = &htable->table[hindex];
    while (!ENTRY_IS_EMPTY(*pg)) {
        if (ENTRIES_ARE_EQUAL(htable, fr, *pg)) {
            *rhindex = hindex;
            return pg;
        }
        /* collision */
        LOG(THREAD_GET, LOG_HTABLE, 6,
            "(hashtable_" KEY_STRING "_lookup_for_"
            "removal: collision sequence " PFX " %u)\n",
            ENTRY_TAG(fr), hindex);
        hindex = HASH_INDEX_WRAPAROUND(hindex + 1, htable);
        pg = &htable->table[hindex];
    }
    *rhindex = 0; /* to make sure always initialized */
    return NULL;
}

#    ifdef HASHTABLE_USE_LOOKUPTABLE
/* FIXME: figure out what weight function I tipped off so this is too much too inline */
static INLINE_FORCED void
HTNAME(hashtable_, NAME_KEY, _update_lookup)(HTNAME(, NAME_KEY, _table_t) * htable,
                                             uint hindex)
{
    if (htable->lookuptable != NULL) {
        AUX_ENTRY_SET_TO_ENTRY(htable->lookuptable[hindex], htable->table[hindex]);
        LOG(THREAD_GET, LOG_HTABLE, 4,
            "hashtable_" KEY_STRING "_update_lookup:"
            "updated " PFX " at table[%u]\n",
            AUX_ENTRY_TAG(htable->lookuptable[hindex]), hindex);
    }
}
#    endif

/* This is based on Algorithm R from Knuth's Vol.3, section 6.4 */
/* Deletion markers tend to form clusters ("parking lot effect"),  */
/* and in steady state the hashtable will always be full.   */
/* This slightly more complicated deletion scheme solves these undesired effects, */
/* with the final arrangement being as if the elements were never inserted. */
/* Returns whether it copied from the start of the table to the end (wraparound).
 * FIXME: if callers need more info, could return the final hindex, or the final
 * hole.
 */
static uint
HTNAME(hashtable_, NAME_KEY,
       _remove_helper_open_address)(HTNAME(, NAME_KEY, _table_t) * htable, uint hindex,
                                    ENTRY_TYPE *prevg)
{
    bool wrapped = false;
    /* Assumptions:
     * We have to move the htable->table and lookuptable elements.  It
     * is OK to do so since the address of these structures is never
     * passed back to clients, instead, all clients can only hold onto
     * a fragment_t* itself, not to the indirection here.
     */
    LOG(THREAD_GET, LOG_HTABLE, 4,
        "hashtable_" KEY_STRING "_remove_helper_open_address(table=" PFX ", "
        "hindex=%u)\n",
        htable, hindex);

    for (;;) {
        uint preferred;
        uint hole = hindex;

        /* first go ahead and set entry to null */
        htable->table[hole] = ENTRY_EMPTY;
#    ifdef HASHTABLE_USE_LOOKUPTABLE
        HTNAME(hashtable_, NAME_KEY, _update_lookup)(htable, hole);
#    endif
        do {
            /* positive probing to get the rest in the same cache line
               also gains from +1 unit stride HW prefetching
            */
            hindex = HASH_INDEX_WRAPAROUND(hindex + 1, htable);

            /* no orphaned elements, we're done */
            /* Note that an &unlinked_fragment will get moved since it
             * and its lookup table entry are designed to preserve linear
             * probing. See the comment after fragment_update_lookup()
             * for the implications.
             */
            if (ENTRY_IS_EMPTY(htable->table[hindex]))
                return wrapped;

            preferred = HASH_FUNC(ENTRY_TAG(htable->table[hindex]), htable);

            /* Verify if it will be lost if we leave a hole behind its preferred addr */
            /* [preferred] <= [hole] < [hindex]  : BAD */
            /* [hindex] < [preferred] <= [hole]  : BAD [after wraparound]  */
            /* [hole] < [hindex] < [preferred]   : BAD [after wraparound]  */
            /* Note the <='s: hole != hindex, but it is possible that preferred == hole */
        } while (!(((preferred <= hole) && (hole < hindex)) ||
                   ((hindex < preferred) && (preferred <= hole)) ||
                   ((hole < hindex) && (hindex < preferred))));

        LOG(THREAD_GET, LOG_HTABLE, 3,
            "hashtable_" KEY_STRING "_remove_helper_open_address: moving " PFX " "
            "from table[%u] into table[%u], preferred=%u\n",
            ENTRY_TAG(htable->table[hindex]), hindex, hole, preferred);

        /* need to move current entry into the hole */
        htable->table[hole] = htable->table[hindex];
        if (hindex < hole)
            wrapped = true;
#    ifdef HASHTABLE_USE_LOOKUPTABLE
        HTNAME(hashtable_, NAME_KEY, _update_lookup)(htable, hole);
        /* Since an &unlinked entry can be moved into a hole, we take
         * special care to sync the lookup table to preserve the assert-able
         * conditions that an unlinked entry has a lookup table entry
         * w/a FAKE_TAG tag and a target_delete start_pc. FAKE_TAG is
         * copied via the preceding update_lookup() but that also sets
         * start_pc to 0. We manually set start_pc to the start_pc value of
         * the old entry, to carry over the target_delete value.
         *
         * We also block the call to fragment_update_lookup() in the
         * caller.
         */
        /* FIXME We can remove this specialized code and simplify the
         * ASSERT_CONSISTENCY by using a dedicated unlinked fragment per
         * table. Each unlinked fragment can have its start_pc set to
         * the corresponding target_delete value for the table.
         */
        if (ENTRY_IS_INVALID(htable->table[hole])) {
            htable->lookuptable[hole] = htable->lookuptable[hindex];
            ASSERT(!AUX_ENTRY_IS_EMPTY(htable->lookuptable[hole]));
            LOG(THREAD_GET, LOG_HTABLE, 4, "   re-set " PFX " at table[%u]\n",
                AUX_ENTRY_TAG(htable->lookuptable[hindex]), hole);
        }
#    endif
    }
    return wrapped;
}

/* Returns whether it copied from the start of the table to the end (wraparound). */
static inline bool
HTNAME(hashtable_, NAME_KEY, _remove_helper)(HTNAME(, NAME_KEY, _table_t) * htable,
                                             uint hindex, ENTRY_TYPE *prevg)
{
    /* Non-trivial for open addressed scheme */
    /* just setting elements to null will make unreachable any following elements */
    /* better solution is to move entries that would become unreachable  */
    int iwrapped =
        HTNAME(hashtable_, NAME_KEY, _remove_helper_open_address)(htable, hindex, prevg);
    bool wrapped = CAST_TO_bool(iwrapped);

#    ifdef HASHTABLE_USE_LOOKUPTABLE
    /* Don't sync the lookup table for unlinked fragments -- see the comments
     * above in fragment_remove_helper_open_address(). */
    if (!ENTRY_IS_INVALID(htable->table[hindex]))
        HTNAME(hashtable_, NAME_KEY, _update_lookup)(htable, hindex);
#    endif
    htable->entries--;
    return wrapped;
}

/* Removes f from htable (does not delete f)
 * returns true if fragment found and removed
 */
static inline bool
HTNAME(hashtable_, NAME_KEY, _remove)(ENTRY_TYPE fr,
                                      HTNAME(, NAME_KEY, _table_t) * htable)
{
    uint hindex;
    ENTRY_TYPE *pg;
    ASSERT_TABLE_SYNCHRONIZED(htable, WRITE); /* remove requires write lock */
    ASSERT(!TEST(HASHTABLE_READ_ONLY, htable->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, htable->table_flags))
        return false;
    pg = HTNAME(hashtable_, NAME_KEY, _lookup_for_removal)(fr, htable, &hindex);
    if (pg != NULL) {
        HTNAME(hashtable_, NAME_KEY, _remove_helper)(htable, hindex, pg);
        return true;
    }
    return false;
}

/* Replaces a fragment in hashtable assuming tag is preserved
 * returns true if fragment found and removed
 */
static inline bool
HTNAME(hashtable_, NAME_KEY, _replace)(ENTRY_TYPE old_e, ENTRY_TYPE new_e,
                                       HTNAME(, NAME_KEY, _table_t) * htable)
{
    uint hindex;
    ENTRY_TYPE *pg =
        HTNAME(hashtable_, NAME_KEY, _lookup_for_removal)(old_e, htable, &hindex);
    /* replace requires write lock only because we have readers who
     * need global consistency and replace requires two local writes!
     */
    ASSERT_TABLE_SYNCHRONIZED(htable, WRITE);
    ASSERT(!TEST(HASHTABLE_READ_ONLY, htable->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, htable->table_flags))
        return false;
    if (pg != NULL) {
        ASSERT(ENTRY_TAG(old_e) == ENTRY_TAG(new_e));
        ASSERT(ENTRIES_ARE_EQUAL(htable, *pg, old_e));
        *pg = new_e;

#    ifdef HASHTABLE_USE_LOOKUPTABLE
        if (htable->lookuptable != NULL) {
            /* TODO: update
             * tag doesn't change, only start_pc may change */
            ASSERT(AUX_ENTRY_TAG(htable->lookuptable[hindex]) ==
                   ENTRY_TAG(htable->table[hindex]));
            AUX_ENTRY_SET_TO_ENTRY(htable->lookuptable[hindex], htable->table[hindex]);
        }
#    endif
        return true;
    }

    return false;
}

/* removes all entries and resets the table but keeps the same capacity */
/* not static b/c not used by all tables */
void
HTNAME(hashtable_, NAME_KEY, _clear)(dcontext_t *dcontext,
                                     HTNAME(, NAME_KEY, _table_t) * table)
{
    uint i;
    ENTRY_TYPE e;

    ASSERT(!TEST(HASHTABLE_READ_ONLY, table->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, table->table_flags))
        return;
    LOG(THREAD, LOG_HTABLE, 2, "hashtable_" KEY_STRING "_clear\n");
    DOLOG(2, LOG_HTABLE | LOG_STATS, {
        HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext, table);
    });

    for (i = 0; i < table->capacity; i++) {
        e = table->table[i];
        /* must check for sentinel */
        if (ENTRY_IS_REAL(e)) {
            HTNAME(hashtable_, NAME_KEY, _free_entry)(dcontext, table, e);
        }
        table->table[i] = ENTRY_EMPTY;
    }
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    memset(table->lookuptable, 0, table->capacity * sizeof(AUX_ENTRY_TYPE));
#    endif
#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
    table->added_since_dumped = 0;
    if (INTERNAL_OPTION(hashtable_ibl_entry_stats) && table->entry_stats != NULL) {
        if (TEST(HASHTABLE_USE_ENTRY_STATS, table->table_flags)) {
            memset(table->entry_stats, 0,
                   table->capacity * sizeof(fragment_stat_entry_t));
        }
    }
#        endif
#    endif
    table->entries = 0;
    table->unlinked_entries = 0;
}

/* removes all entries within a specified range of tags
 *
 * should generalize hashtable_reset to do this in all cases,
 * yet we haven't had an instance where this is necessary
 *
 * FIXME: note that we don't do a full type dispatch here, while
 * hashtable_reset is not properly moving elements hence can't be used
 * for removing subsets, and is inefficient!
 */
static uint
HTNAME(hashtable_, NAME_KEY, _range_remove)(dcontext_t *dcontext,
                                            HTNAME(, NAME_KEY, _table_t) * table,
                                            ptr_uint_t tag_start, ptr_uint_t tag_end,
                                            bool (*filter)(ENTRY_TYPE e))
{
    int i;
    ENTRY_TYPE e;
    uint entries_removed = 0;
    DEBUG_DECLARE(uint entries_initial = 0;)

    ASSERT(!TEST(HASHTABLE_READ_ONLY, table->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, table->table_flags))
        return 0;
    LOG(THREAD, LOG_HTABLE, 2, "hashtable_" KEY_STRING "_range_remove\n");
    DOLOG(2, LOG_HTABLE | LOG_STATS, {
        HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext, table);
    });
    DODEBUG({
        HTNAME(hashtable_, NAME_KEY, _study)(dcontext, table, 0 /*table consistent*/);
        /* ensure write lock is held if the table is shared, unless exiting */
        if (!dynamo_exited)
            ASSERT_TABLE_SYNCHRONIZED(table, WRITE); /* remove requires write lock */
        entries_initial = table->entries;
    });

    /* Deletion in hashtable_NAME_KEY_remove_helper has to move entries
     * in order to keep all reachable.  We go in reverse order to
     * efficiently delete all entries */
    i = table->capacity - 1 - 1 /* sentinel */;
    while (i >= 0) {
        e = table->table[i];
        if (!ENTRY_IS_EMPTY(e) && ENTRY_TAG(e) >= tag_start && ENTRY_TAG(e) < tag_end &&
            (filter == NULL || filter(e))) {
            if (HTNAME(hashtable_, NAME_KEY, _remove_helper)(table, i,
                                                             &table->table[i])) {
                /* Pulled a chain across the wraparound, so we must start over
                 * at the end as otherwise we will never look at that last
                 * element (case 10384).
                 */
                i = table->capacity - 1 - 1 /* sentinel */;
            } else {
                /* we can assume deletion doesn't move any entries ahead of i
                 * to smaller values, so i stays here
                 */
            }
            entries_removed++;
            /* de-allocate payload */
            HTNAME(hashtable_, NAME_KEY, _free_entry)(dcontext, table, e);
        } else {
            i--;
        }
    }
    ASSERT(table->entries + entries_removed == entries_initial);
    return entries_removed;
}

/* removes all unlinked entries from table's lookup table */
static uint
HTNAME(hashtable_, NAME_KEY, _unlinked_remove)(dcontext_t *dcontext,
                                               HTNAME(, NAME_KEY, _table_t) * table)
{
    int i;
    ENTRY_TYPE e UNUSED;
    uint entries_removed = 0;

    ASSERT(!TEST(HASHTABLE_READ_ONLY, table->table_flags));
    if (TEST(HASHTABLE_READ_ONLY, table->table_flags))
        return 0;
    LOG(THREAD, LOG_HTABLE, 2, "hashtable_" KEY_STRING "_unlinked_remove\n");
    /* body based on hashtable_range_remove() */

    ASSERT(TEST(HASHTABLE_LOCKLESS_ACCESS, table->table_flags));
    DOLOG(2, LOG_HTABLE | LOG_STATS, {
        HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext, table);
    });
    DODEBUG({
        /* ensure write lock is held if the table is shared, unless exiting */
        if (!dynamo_exited)
            ASSERT_TABLE_SYNCHRONIZED(table, WRITE); /* remove requires write lock */
    });
    /* Deletion in fragment_remove_fragment_helper has to move entries
     * in order to keep all reachable.  We go in reverse order to
     * efficiently delete all entries
     */
    i = (int)table->capacity - 1 - 1 /* sentinel */;
    while (i >= 0) {
        e = table->table[i];
        if (ENTRY_IS_INVALID(e)) {
            LOG(THREAD, LOG_HTABLE | LOG_STATS, 2, "unlinked_remove(%s) -- %u\n",
                table->name, i);
            if (HTNAME(hashtable_, NAME_KEY, _remove_helper)(table, i,
                                                             &table->table[i])) {
                /* Pulled a chain across the wraparound, so we must start over
                 * at the end as otherwise we will never look at that last
                 * element (case 10384).
                 */
                i = table->capacity - 1 - 1 /* sentinel */;
            } else {
                /* we can assume deletion doesn't move any entries ahead of i
                 * to smaller values, so i stays here
                 */
            }
            entries_removed++;
        } else {
            i--;
        }
    }
    /* fragment_remove_fragment_helper decrements table->entries but we
     * want only unlinked_entries decremented, so adjust entries. */
    table->entries += entries_removed;
    LOG(THREAD, LOG_HTABLE | LOG_STATS, 1, "unlinked_remove(%s) -- %d deletions\n",
        table->name, entries_removed);
    ASSERT(entries_removed == table->unlinked_entries);
#    ifdef HASHTABLE_USE_LOOKUPTABLE
    DODEBUG({
        /* Check that there are no remnants of unlinked fragments in the table. */
        for (i = 0; i < (int)table->capacity; i++) {
            ASSERT(!ENTRY_IS_INVALID(table->table[i]));
            ASSERT(!AUX_ENTRY_IS_INVALID(table->lookuptable[i]));
            ASSERT(!AUX_PAYLOAD_IS_INVALID(dcontext, table, table->lookuptable[i]));
        }
    });
#    else
    DODEBUG({
        /* Check that there are no remnants of unlinked fragments in the table. */
        for (i = 0; i < (int)table->capacity; i++) {
            ASSERT(!ENTRY_IS_INVALID(table->table[i]));
        }
    });
#    endif
    table->unlinked_entries = 0;
    DOLOG(3, LOG_HTABLE, { HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table); });
    DODEBUG({
        HTNAME(hashtable_, NAME_KEY, _study)(dcontext, table, 0 /*table consistent*/);
    });
    return entries_removed;
}

/* we should clean the table from entries that are not frequently used
 */
static void
HTNAME(hashtable_, NAME_KEY, _groom_table)(dcontext_t *dcontext,
                                           HTNAME(, NAME_KEY, _table_t) * table)
{
    DOLOG(1, LOG_STATS, { d_r_print_timestamp(THREAD); });
    LOG(THREAD, LOG_HTABLE, 1, "hashtable_" KEY_STRING "_groom_table %s\n", table->name);

    /* flush only tables caching data persistent in another table */
    ASSERT(TEST(HASHTABLE_NOT_PRIMARY_STORAGE, table->table_flags));

    DOLOG(3, LOG_HTABLE, { HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table); });

    LOG(THREAD, LOG_HTABLE, 2, "%s hashtable flushing at %d entries capacity %d\n",
        table->name, table->entries, table->capacity);

    /* most simple grooming technique - just flush everyone */
    HTNAME(hashtable_, NAME_KEY, _range_remove)(dcontext, table, 0, UINT_MAX, NULL);
    ASSERT(table->entries == 0);

    /* FIXME: we should do better - we can tag fragments that have
     * been readded after getting flushed, so that they are not
     * flushed next time we do this Some kind of aging that can be
     * used also if we do a real clock working set.
     */

    /* will not flush again until table gets resized */
    table->groom_threshold = 0;

    /* FIXME: we may want to do this more often - so that we can catch
     * phases and that we don't even have to resize if working set
     * does in fact fit here.  In that case we may want to
     * do have a step function,
     * e.g. (groom 50, resize 80, groom step 10) translating into
     * 50 - groom, 60 - groom, 70 - groom, 80 - resize
     */

    STATS_INC(num_ibt_groomed);
    LOG(THREAD, LOG_HTABLE, 1,
        "hashtable_" KEY_STRING "_groom_table %s totally groomed - "
        "should be empty\n",
        table->name);
    DOLOG(3, LOG_HTABLE, { HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table); });
}

static inline void
HTNAME(hashtable_, NAME_KEY, _groom_helper)(dcontext_t *dcontext,
                                            HTNAME(, NAME_KEY, _table_t) * table)
{
    /* flush only tables caching data persistent in another table */
    ASSERT(TEST(HASHTABLE_NOT_PRIMARY_STORAGE, table->table_flags));

    ASSERT(table->hash_bits != 0);
    /* can't double size, and there is no point in resizing
     * we have to flush it.
     */
    LOG(THREAD, LOG_HTABLE, 1,
        "hashtable_" KEY_STRING "_check_size reached maximum size %s\n", table->name);
    /* Currently groom_table() resets the whole table, but if
     * it gets smarter, we may want to invoke the reset all
     * logic here.
     */
    table->entries--; /* entry not added yet */
    HTNAME(hashtable_, NAME_KEY, _groom_table)(dcontext, table);
    table->entries++;
    /* can't make forward progress if groom_table() doesn't
     * remove at least one entry
     */
    ASSERT(table->entries <= table->resize_threshold);
}

#    ifdef DEBUG
/* The above dyn.avg.coll is a little off: we can't show average
 * succesfull search time, since some collisions are for misses but
 * indcalls_miss_stat includes misses both with and without
 * collisions.
 */
static void
HTNAME(hashtable_, NAME_KEY,
       _study)(dcontext_t *dcontext, HTNAME(, NAME_KEY, _table_t) * table,
               uint entries_inc /*amnt table->entries was pre-inced*/)
{
    /* hashtable sparseness study */
    uint i;
    uint max = 0;
    uint num = 0, len = 0, num_collisions = 0;
    uint total_len = 0;
    uint hindex;
    const char *name = table->name;
    uint ave_len_threshold;

    uint overwraps = 0;
    ENTRY_TYPE e;
    bool lockless_access = TEST(HASHTABLE_LOCKLESS_ACCESS, table->table_flags);

    if (!INTERNAL_OPTION(hashtable_study))
        return;

    /* studying needs the entire table to be in a consistent state.
     * we considered having read mean local read/write and write mean global
     * read/write, thus making study() a writer and add() a reader,
     * but we do want add() and remove() to be exclusive w/o relying on the
     * bb building lock, so we have them as writers and study() as a reader.
     */
    TABLE_RWLOCK(table, read, lock);

    for (i = 0; i < table->capacity; i++) {
        e = table->table[i];
        DODEBUG({
            HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, table, i);
        });

        if (ENTRY_IS_EMPTY(e))
            continue;

        if (ENTRY_IS_SENTINEL(e)) {
            ASSERT(i == table->capacity - 1);
            /* don't count in collision length - not a real fragment */
            continue;
        }

        hindex = HASH_FUNC(ENTRY_TAG(e), table);
        if (i < hindex) {
            len = i + (table->capacity - hindex - 1) + 1; /* counting the sentinel */
            overwraps++;
            LOG(THREAD, LOG_HTABLE | LOG_STATS, 2,
                "WARNING: hashtable_" KEY_STRING "_study: overwrap[%d] of "
                "len=%d, F tag=" PFX ",i=%d,hindex=%d\n",
                overwraps, len, ENTRY_TAG(e), i, hindex);
        } else {
            len = i - hindex + 1;
        }

        if (ENTRY_IS_INVALID(e)) {
            ASSERT(lockless_access);
            LOG(THREAD, LOG_HTABLE | LOG_STATS, 2,
                "hashtable_" KEY_STRING "_study: entry not found in %s[%u]\n",
                table->name, i);
            len = 0;
            continue;
        }
        /* check for unique entries - the entry found by
         * fragment_lookup_in_hashtable should be us! */
        else {
            uint found_at_hindex;
            ENTRY_TYPE *pg = HTNAME(hashtable_, NAME_KEY,
                                    _lookup_for_removal)(e, table, &found_at_hindex);
            ASSERT(pg != NULL);
            ASSERT(ENTRIES_ARE_EQUAL(table, *pg, e));
            ASSERT(found_at_hindex == i && "duplicate entry found");
        }
        if (len > 0) {
            if (len > max)
                max = len;
            total_len += len;
            num++;
            if (len > 1) {
                num_collisions++;
            }
        }
    }

    DOLOG(1, LOG_HTABLE | LOG_STATS, {
        uint st_top = 0;
        uint st_bottom = 0;
        if (num != 0)
            divide_uint64_print(total_len, num, false, 2, &st_top, &st_bottom);
        LOG(THREAD, LOG_HTABLE | LOG_STATS, 1,
            "%s %s hashtable statistics: num=%d, max=%d, #>1=%d, st.avg=%u.%.2u\n",
            entries_inc == 0 ? "Total" : "Current", name, num, max, num_collisions,
            st_top, st_bottom);
    });

    /* static average length is supposed to be under 5 even up
     * to load factors of 90% see Knuth vol.3 or in CLR (p.238-9 in
     * first edition) but of course only if we had uniformly
     * distributed hash functions
     */
    /* for bug 2271 we make more lenient for non trace tables using the
     * _NONE hash function (i.e. private bb) when we are !SHARED_FRAGMENTS_ENABLED().
     */
    if (!TEST(HASHTABLE_RELAX_CLUSTER_CHECKS, table->table_flags) &&
        (lockless_access || table->hash_func != HASH_FUNCTION_NONE ||
         SHARED_FRAGMENTS_ENABLED()))
        ave_len_threshold = 5;
    else
        ave_len_threshold = 10;
    DOLOG(1, LOG_HTABLE | LOG_STATS, {
        /* This happens enough that it's good to get some info on it */
        if (!(total_len <= ave_len_threshold * num ||
              (TEST(HASHTABLE_RELAX_CLUSTER_CHECKS, table->table_flags) &&
               table->capacity <= 513))) {
            /* Hash table high average collision length */
            LOG(THREAD, LOG_HTABLE | LOG_STATS, 1,
                "WARNING: high average collision length for htable %s\n"
                "  ave len: tot=%d <= %d, cap=%d entr=%d fac=%d\n",
                name, total_len, ave_len_threshold * num, table->capacity, table->entries,
                table->load_factor_percent);
            HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table);
        }
    });
    DOLOG(3, LOG_HTABLE, { HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext, table); });

    {
        uint doublecheck = num + entries_inc;

        LOG(THREAD, LOG_HTABLE | LOG_STATS, 2,
            "\t%s doublecheck %u (unlinked %u) == %u %d\n", name, table->entries,
            table->unlinked_entries, doublecheck, entries_inc);
        ASSERT(table->entries == doublecheck);
    }

#        ifdef HASHTABLE_STATISTICS
    print_hashtable_stats(dcontext, entries_inc == 0 ? "Total" : "Current", name,
                          "fragment_lookup", "", &table->drlookup_stats);
#        endif

    HTNAME(hashtable_, NAME_KEY, _study_custom)(dcontext, table, entries_inc);

    TABLE_RWLOCK(table, read, unlock);
}

void
HTNAME(hashtable_, NAME_KEY, _dump_table)(dcontext_t *dcontext,
                                          HTNAME(, NAME_KEY, _table_t) * htable)
{
    uint i;
    bool track_cache_lines;
    bool entry_size;
    size_t line_size = proc_get_cache_line_size();
    uint cache_lines_used = 0;
    bool cache_line_in_use = false;

#        ifdef HASHTABLE_USE_LOOKUPTABLE
    track_cache_lines = true;
    entry_size = sizeof(AUX_ENTRY_TYPE);
#        else
    track_cache_lines = TEST(HASHTABLE_ALIGN_TABLE, htable->table_flags);
    entry_size = sizeof(ENTRY_TYPE);
#        endif

    DOLOG(1, LOG_HTABLE | LOG_STATS, {
        HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext, htable);
    });
    LOG(THREAD, LOG_HTABLE, 1, "  i      tag     coll     hits  age %s dump\n",
        htable->name);
    /* need read lock to traverse the table */
    TABLE_RWLOCK(htable, read, lock);
    for (i = 0; i < htable->capacity; i++) {
        if (track_cache_lines && (i * entry_size % line_size == 0)) {
            if (cache_line_in_use)
                cache_lines_used++;
            cache_line_in_use = false;
        }

        if (ENTRY_IS_INVALID(htable->table[i])) {
            LOG(THREAD, LOG_HTABLE, 1, "%6x *** unlinked marker ***\n", i);
        } else if (ENTRY_IS_REAL(htable->table[i])) {
            uint preferred = HASH_FUNC(ENTRY_TAG(htable->table[i]), htable);
            uint collision_len = (preferred <= i)
                ? i - preferred /* collision */
                : (htable->capacity + i - preferred) /* with overwrap */;
            /* overwrap count should include the sentinel to total length */
#        if defined(HASHTABLE_STATISTICS) && defined(HASHTABLE_ENTRY_STATS)
            LOG(THREAD, LOG_HTABLE, 1, "%6x " PFX " %3d %c  %8d  %3d\n", i,
                ENTRY_TAG(htable->table[i]), collision_len + 1,
                (preferred <= i) ? ' ' : 'O' /* overwrap */,
                htable->entry_stats ? htable->entry_stats[i].hits : 0,
                htable->entry_stats ? htable->entry_stats[i].age : 0);
#        else  /* !(HASHTABLE_STATISTICS && HASHTABLE_ENTRY_STATS) */
            LOG(THREAD, LOG_HTABLE, 1, "%6x " PFX " %3d %c \n", i,
                ENTRY_TAG(htable->table[i]), collision_len + 1,
                (preferred <= i) ? ' ' : 'O' /* overwrap */
            );
#        endif /* (HASHTABLE_STATISTICS && HASHTABLE_ENTRY_STATS) */
            if (track_cache_lines)
                cache_line_in_use = true;
        } else {
            /* skip null_fragment entries */
        }

        DOLOG(2, LOG_HTABLE, {
            /* print full table */
            if (ENTRY_IS_EMPTY(htable->table[i])) {
                LOG(THREAD, LOG_HTABLE, 2, "%6x " PFX "\n", i, 0);
            }
        });
        DOLOG(2, LOG_HTABLE, {
            if (track_cache_lines && (i + 1) * entry_size % line_size == 0 &&
                cache_line_in_use) {
                LOG(THREAD, LOG_HTABLE, 1, "----cache line----\n");
            }
        });
        DODEBUG({
            HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, htable, i);
        });
    }
    if (track_cache_lines) {
        if (cache_line_in_use)
            cache_lines_used++;
        if (cache_lines_used) {
            LOG(THREAD, LOG_HTABLE, 1,
                "%s %d%% cache density, cache_lines_used=%d "
                "(%dKB), minimum needed %d (%dKB)\n",
                htable->name,
                100 * htable->entries * entry_size / (cache_lines_used * line_size),
                cache_lines_used, cache_lines_used * line_size / 1024,
                htable->entries * entry_size / line_size,
                htable->entries * entry_size / 1024);
        }
    }
    TABLE_RWLOCK(htable, read, unlock);
}
#    endif     /* DEBUG */

#    if defined(DEBUG) && defined(INTERNAL)
static void
HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext_t *dcontext,
                                               HTNAME(, NAME_KEY, _table_t) * table)
{
    LOG(THREAD, LOG_HTABLE | LOG_STATS, 1,
        "%s hashtable: %d entries, %d unlinked entries, %d capacity,"
        " %d%% load\n",
        table->name, table->entries, table->unlinked_entries, table->capacity,
        (100 * table->entries) / table->capacity);
}
#    endif

#    ifdef HASHTABLE_STATISTICS
#        ifdef HASHTABLE_ENTRY_STATS
void
HTNAME(hashtable_, NAME_KEY, _dump_entry_stats)(dcontext_t *dcontext,
                                                HTNAME(, NAME_KEY, _table_t) * htable)
{
    /* mostly a copy of dump_table but printing only entries with non 0 stats */
    uint i;
    uint max_age = 0;
    DOLOG(1, LOG_STATS, { d_r_print_timestamp(THREAD); });
    LOG(THREAD, LOG_HTABLE, 1, "dump_and_clean_entry_statistics: %s\n", htable->name);

    DOLOG(1, LOG_HTABLE | LOG_STATS, {
        HTNAME(hashtable_, NAME_KEY, _load_statistics)(dcontext, htable);

        /* TODO: should preserve a copy of the old hashtable_statistics_t
         * so that the difference between the two can be matched with
         * the per-entry hits and collisions.  though a Perl script will do.
         */
    });
    LOG(THREAD, LOG_HTABLE, 1, "  i      tag     coll       hits   age %s\n",
        htable->name);
    /* need read lock to traverse the table */
    TABLE_RWLOCK(htable, read, lock);
    for (i = 0; i < htable->capacity; i++) {
        if (!ENTRY_IS_EMPTY(htable->table[i]) && !ENTRY_IS_INVALID(htable->table[i])) {
            uint preferred = HASH_FUNC(ENTRY_TAG(htable->table[i]), htable);
            uint collision_len = (preferred <= i)
                ? i - preferred /* collision */
                : (htable->capacity + i - preferred) /* with overwrap */;

            if (htable->entry_stats[i].hits == 0) {
                /* no hits in a row */
                htable->entry_stats[i].age++;
                if (max_age < htable->entry_stats[i].age)
                    max_age = htable->entry_stats[i].age;
            }

            LOG(THREAD, LOG_HTABLE, htable->entry_stats[i].hits ? 1U : 2U, /* only hits */
                "%6x " PFX " %3d %c %8d   %3d\n", i, ENTRY_TAG(htable->table[i]),
                collision_len + 1, (preferred <= i) ? ' ' : 'O' /* overwrap */,
                htable->entry_stats[i].hits, htable->entry_stats[i].age);

        } else {
            /* skip null_fragment entries */
        }
        DODEBUG({
            HTNAME(hashtable_, NAME_KEY, _check_consistency)(dcontext, htable, i);
        });
    }
    if (max_age > 0) {
        LOG(THREAD, LOG_HTABLE, 1,
            "hashtable_" KEY_STRING "dump_entry_stats: %s max_age:%d\n", htable->name,
            max_age);
    }

    TABLE_RWLOCK(htable, read, unlock);
}
#        endif
#    endif /* HASHTABLE_STATISTICS */

#    ifdef HASHTABLE_SUPPORT_PERSISTENCE
uint
HTNAME(hashtable_, NAME_KEY, _persist_size)(dcontext_t *dcontext,
                                            HTNAME(, NAME_KEY, _table_t) * htable)
{
    if (htable == NULL)
        return 0;
    return (sizeof(*htable) + (htable->capacity * sizeof(ENTRY_TYPE)));
}

/* We need enough of the htable struct fields that we simply persist
 * the entire struct to avoid defining a separate subset struct.
 * The version field of the entire file suffices to version the htable struct.
 * The table pointer (and stats in debug) need to be writable,
 * so at load time we do not directly use the mmapped copy, but we could
 * with a little work (case 10349: see comments below as well) (not worth
 * making COW and having the whole page private: case 9582).
 * Returns true iff all writes succeeded.
 */
bool
HTNAME(hashtable_, NAME_KEY, _persist)(dcontext_t *dcontext,
                                       HTNAME(, NAME_KEY, _table_t) * htable, file_t fd)
{
    uint size;
    ASSERT(htable != NULL);
    if (htable == NULL)
        return false;
    ASSERT(fd != INVALID_FILE);
    size = sizeof(*htable);
    if (os_write(fd, htable, size) != (int)size)
        return false;
    /* We don't bother to align */
    size = htable->capacity * sizeof(ENTRY_TYPE);
    if (os_write(fd, htable->table, size) != (int)size)
        return false;
    return true;
}

static uint
HTNAME(hashtable_, NAME_KEY, _num_unique_entries)(dcontext_t *dcontext,
                                                  HTNAME(, NAME_KEY, _table_t) * src1,
                                                  HTNAME(, NAME_KEY, _table_t) * src2)
{
    HTNAME(, NAME_KEY, _table_t) * big, *small;
    uint unique, i;
    ENTRY_TYPE e;
    ASSERT(src1 != NULL && src2 != NULL);
    if (src1->entries >= src2->entries) {
        big = src1;
        small = src2;
    } else {
        big = src2;
        small = src1;
    }
    unique = big->entries;
    /* Deadlock-avoidance won't let us grab both locks; for now we only
     * use this when all-synched so we let that solve the problem.
     * N.B.: we assume that on suspend failure for flush synchall
     * (which ignores such failures) we do not come here as we abort
     * coarse freezing/merging/persist.  FIXME: should we export
     * another variable, or not set dynamo_all_threads_synched?
     */
    ASSERT(dynamo_all_threads_synched);
    DODEBUG({ big->is_local = true; });
    TABLE_RWLOCK(small, read, lock);
    for (i = 0; i < small->capacity; i++) {
        e = small->table[i];
        if (ENTRY_IS_REAL(e)) {
            if (ENTRY_IS_EMPTY(
                    HTNAME(hashtable_, NAME_KEY, _lookup(dcontext, ENTRY_TAG(e), big))))
                unique++;
        }
    }
    TABLE_RWLOCK(small, read, unlock);
    DODEBUG({ big->is_local = false; });
    return unique;
}

/* Adds all entries from src to dst */
static void
HTNAME(hashtable_, NAME_KEY,
       _add_all)(dcontext_t *dcontext, HTNAME(, NAME_KEY, _table_t) * dst,
                 HTNAME(, NAME_KEY, _table_t) * src, bool check_dups)
{
    ENTRY_TYPE e;
    uint i;
    ASSERT(dst != NULL && src != NULL);
    /* assumption: dst is private to this thread and so does not need synch */
    DODEBUG({ dst->is_local = true; });
    TABLE_RWLOCK(src, read, lock);
    for (i = 0; i < src->capacity; i++) {
        e = src->table[i];
        if (ENTRY_IS_REAL(e)) {
            if (!check_dups ||
                ENTRY_IS_EMPTY(
                    HTNAME(hashtable_, NAME_KEY, _lookup(dcontext, ENTRY_TAG(e), dst)))) {
                /* add will also add lookuptable entry, if any */
                HTNAME(hashtable_, NAME_KEY, _add)(dcontext, e, dst);
            }
        }
    }
    TABLE_RWLOCK(src, read, unlock);
    DODEBUG({ dst->is_local = false; });
}

/* Creates a new hashtable that contains the union of src1 and src2 (removing
 * the duplicates)
 */
HTNAME(, NAME_KEY, _table_t) *
    HTNAME(hashtable_, NAME_KEY, _merge)(dcontext_t *dcontext,
                                         HTNAME(, NAME_KEY, _table_t) * src1,
                                         HTNAME(, NAME_KEY, _table_t) * src2)
{
    HTNAME(, NAME_KEY, _table_t) * dst;
    uint merged_entries, init_bits;
    ASSERT(src1 != NULL && src2 != NULL);
    /* We only support merging the same type of table */
    ASSERT((src1->table_flags & ~(HASHTABLE_COPY_IGNORE_FLAGS)) ==
           (src2->table_flags & ~(HASHTABLE_COPY_IGNORE_FLAGS)));
    ASSERT(src1->load_factor_percent == src2->load_factor_percent);
    ASSERT(src1->hash_func == src2->hash_func);
    dst = (HTNAME(, NAME_KEY, _table_t) *)TABLE_TYPE_MEMOP(
        src1->table_flags, ALLOC, dcontext, HTNAME(, NAME_KEY, _table_t),
        HASHTABLE_WHICH_HEAP(src1->table_flags), PROTECTED);
    merged_entries =
        HTNAME(hashtable_, NAME_KEY, _num_unique_entries)(dcontext, src1, src2);
    LOG(THREAD, LOG_HTABLE, 2,
        "hashtable_" KEY_STRING "_merge: %d + %d => %d unique entries\n", src1->entries,
        src2->entries, merged_entries);
    init_bits = hashtable_bits_given_entries(merged_entries, src1->load_factor_percent);
    HTNAME(hashtable_, NAME_KEY, _init)
    (dcontext, dst, init_bits, src1->load_factor_percent, src1->hash_func,
     src1->hash_mask_offset _IFLOOKUP(use_lookup),
     src1->table_flags & ~(HASHTABLE_COPY_IGNORE_FLAGS)_IF_DEBUG(src1->name));
    HTNAME(hashtable_, NAME_KEY, _add_all)
    (dcontext, dst, src1, false /*don't check dups*/);
    HTNAME(hashtable_, NAME_KEY, _add_all)(dcontext, dst, src2, true /*check dups*/);
    return dst;
}

/* Performs a deep copy (struct plus table) of src */
HTNAME(, NAME_KEY, _table_t) *
    HTNAME(hashtable_, NAME_KEY, _copy)(dcontext_t *dcontext,
                                        HTNAME(, NAME_KEY, _table_t) * src)
{
    HTNAME(, NAME_KEY, _table_t) * dst;
    ASSERT(src != NULL);
    dst = (HTNAME(, NAME_KEY, _table_t) *)TABLE_TYPE_MEMOP(
        src->table_flags, ALLOC, dcontext, HTNAME(, NAME_KEY, _table_t),
        HASHTABLE_WHICH_HEAP(src->table_flags), PROTECTED);
    /* use init() rather than memcpy of header, so we get table and/or lookuptable
     * allocated to proper alignment
     */
    HTNAME(hashtable_, NAME_KEY, _init)
    (dcontext, dst, src->hash_bits, src->load_factor_percent, src->hash_func,
     src->hash_mask_offset _IFLOOKUP(use_lookup),
     src->table_flags & ~(HASHTABLE_COPY_IGNORE_FLAGS)_IF_DEBUG(src->name));
    dst->entries = src->entries;
    dst->unlinked_entries = src->unlinked_entries;
    if (dst->table != NULL)
        memcpy(dst->table, src->table, dst->capacity * sizeof(ENTRY_TYPE));
#        ifdef HASHTABLE_USE_LOOKUPTABLE
    if (dst->lookuptable != NULL) {
        memcpy(dst->lookuptable, src->lookuptable,
               dst->capacity * sizeof(AUX_ENTRY_TYPE));
    }
#        endif
    return dst;
}

/* See comments in HTNAME(hashtable_,NAME_KEY,_persist)
 * Returns a newly allocated struct on the heap.
 */
HTNAME(, NAME_KEY, _table_t) *
    HTNAME(hashtable_, NAME_KEY,
           _resurrect)(dcontext_t *dcontext,
                       byte *mapped_table _IF_DEBUG(const char *table_name))
{
    HTNAME(, NAME_KEY, _table_t) * htable;
    uint flags;
    ASSERT(mapped_table != NULL);
    flags = ((HTNAME(, NAME_KEY, _table_t) *)mapped_table)->table_flags;
    /* FIXME: the free, and the init alloc, are in client code: would be better
     * to have all in same file using more-easily-kept-consistent alloc routines
     */
    htable = (HTNAME(, NAME_KEY, _table_t) *)TABLE_TYPE_MEMOP(
        flags, ALLOC, dcontext, HTNAME(, NAME_KEY, _table_t),
        HASHTABLE_WHICH_HEAP(table->table_flags), PROTECTED);
    /* FIXME case 10349: we could directly use the mmapped struct when
     * !HASHTABLE_STATISTICS if we supported calculating where the table lies
     * in all the htable routines, and set HASHTABLE_READ_ONLY when persisting
     */
    memcpy(htable, mapped_table, sizeof(*htable));
    htable->table_flags |= HASHTABLE_READ_ONLY;
    htable->table = (ENTRY_TYPE *)(mapped_table + sizeof(*htable));
    htable->table_unaligned = NULL;
    ASSIGN_INIT_READWRITE_LOCK_FREE(htable->rwlock, HTLOCK_RANK);
    DODEBUG({ htable->name = table_name; });
#        ifdef HASHTABLE_STATISTICS
    INIT_HASHTABLE_STATS(htable->drlookup_stats);
#        endif
    return htable;
}
#    endif /* HASHTABLE_SUPPORT_PERSISTENCE */

#endif
/****************************************************************************/

/* make it easier to have multiple instantiations in a file
 * user must re-specify name and types for struct and impl, though
 */
#undef NAME_KEY
#undef ENTRY_TYPE
#undef AUX_ENTRY_TYPE
#undef CUSTOM_FIELDS

#undef ENTRY_TAG
#undef ENTRY_IS_EMPTY
#undef ENTRY_IS_SENTINEL
#undef ENTRY_IS_INVALID
#undef ENTRIES_ARE_EQUAL
#undef ENTRY_SET_TO_ENTRY
#undef ENTRY_EMPTY
#undef ENTRY_SENTINEL
#undef TAGS_ARE_EQUAL

#undef AUX_ENTRY_TAG
#undef AUX_ENTRY_IS_EMPTY
#undef AUX_ENTRY_IS_SENTINEL
#undef AUX_ENTRY_IS_INVALID
#undef AUX_PAYLOAD_IS_INVALID
#undef AUX_ENTRY_SET_TO_SENTINEL
#undef AUX_ENTRY_IS_SET_TO_ENTRY
#undef AUX_ENTRY_SET_TO_ENTRY
#undef AUX_ENTRY_FORMAT_STR
#undef AUX_ENTRY_FORMAT_ARGS

#undef HASHTABLE_WHICH_HEAP
#undef HASHTABLE_USE_LOOKUPTABLE
#undef HASHTABLE_ENTRY_STATS
#undef HASHTABLE_SUPPORT_PERSISTENCE
#undef HTLOCK_RANK

#undef _IFLOOKUP
#undef IFLOOKUP_ELSE
