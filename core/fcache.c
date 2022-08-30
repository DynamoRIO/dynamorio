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
 * fcache.c - fcache manager
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "monitor.h"
#ifdef HOT_PATCHING_INTERFACE
#    include "hotpatch.h"
#endif
#include <stddef.h> /* for offsetof */
#include <limits.h> /* for UCHAR_MAX */
#include "perscache.h"
#include "synch.h"
#include "instrument.h"

/* A code cache is made up of multiple separate mmapped units
 * We grow a unit by resizing, shifting, and relinking, up to a maximum size,
 * at which point we create a separate unit if we need more space
 * Cache is extremely flexible in allowing resizing (hard to support)
 * and separate units of different sizes, in any combination.
 * We will build a unit larger than cache_{bb,trace}_unit_max for a single large request,
 * up to the max cache size.
 * To save memory, we don't make units larger than 64KB.  Not much advantage
 * to have huge units.
 */

/* to make it easy to switch to INTERNAL_OPTION */
#define FCACHE_OPTION(o) dynamo_options.o

/*
 * unit initial size is FCACHE_OPTION(cache_{bb,trace}_unit_init, default is 32*1024
 * it grows by 4X steps up to FCACHE_OPTION(cache_{bb,trace}_unit_quadruple, default
 * is 32*1024
 * unit max size is FCACHE_OPTION(cache_{bb,trace}_unit_max, default is 64*1024
 * once at max size, we make new units, all of max size
 *
 * thus default is to do no quadrupling, just a single doubling and then no more resizing
 * FIXME: should we stop resizing altogether and just have variable-sized
 * separate units?  it's not like a 32K unit is too small to keep around...
 * OTOH, we want the flexibility of resizing, for servers apps with lots of threads
 * we may move the initial unit size smaller
 */

/* Invariant: a cache unit is always at least this constant times the largest
 * fragment inside it in size (this can make it larger than cache_{bb,trace}_unit_max)
 */
#define MAX_SINGLE_MULTIPLE 2

/* adaptive working set reactive cache expansion default parameters:
 * first expansion(s) free
 *   FCACHE_OPTION(cache_{bb,trace}_unit_upgrade, default is 64KB so 32=>64 is free
 * after that, only expand if regenerated/replaced ratio matches these
 * numbers (no floating-point, so we use 2 ints):
 *   dynamo_options.cache_{bb,trace}_regen default = 10
 *   dynamo_options.cache_{bb,trace}_replace default = 50
 * special cases:
 *   if cache_{bb,trace}_regen == 0, never increases cache size after free upgrade
 *   if cache_{bb,trace}_replace == 0, always increases (effectively
 *     disabling adaptive working set, although the nice way to disable
 *     is to use -no_finite_{bb,trace}_cache)
 */

/*
 * Maximum cache sizes are stored in these two options:
 *   dynamo_options.cache_bb_max
 *   dynamo_options.cache_trace_max
 * A value of 0 means "infinite"
 */

/* Minimum size to leave as an empty hole, which will be prepended to FIFO
 * for private cache and so will be filled even if it means bumping adjacent guys.
 * For shared cache, we currently don't fill empty slots, but once we do we will
 * only fill with a fragment that fits (case 4485).  Shared empties are not prepended
 * but rather are placed in the same location in the FIFO as the deleted guy.
 * Should be the minimum common fragment size.
 */
#define MIN_EMPTY_HOLE(cache)                                                       \
    MAX(((cache)->is_trace                                                          \
             ? 64U                                                                  \
             : ((!(cache)->is_shared && DYNAMO_OPTION(separate_private_stubs))      \
                    ? 20U                                                           \
                    : (((cache)->is_shared && DYNAMO_OPTION(separate_shared_stubs)) \
                           ? 20U                                                    \
                           : 64U))),                                                \
        MIN_FCACHE_SLOT_SIZE(cache))

/* Minimum end-of-cache hole size -- anything smaller and the cache is "full"
 * This is 2x the smallest fragment size
 * FIXME: use larger size for trace cache?
 */
#define MIN_UNIT_END_HOLE(cache) ((uint)2 * MIN_EMPTY_HOLE(cache))

/* This is ignored for coarse fragments */
#define START_PC_ALIGNMENT 4

/* Alignment: we assume basic blocks don't care so much about alignment, we go
 * to 4 to avoid sub-word fetches. NOTE we also need at least START_PC_ALIGNMENT
 * byte alignment for the start_pc padding for -pad_jmps_shift_{bb,trace} support
 * (need at least that much even without the option since we back align the
 * start_pc to get the header)
 */
#define SLOT_ALIGNMENT(cache)                                                    \
    ((cache)->is_trace ? DYNAMO_OPTION(cache_trace_align)                        \
                       : ((cache)->is_coarse ? DYNAMO_OPTION(cache_coarse_align) \
                                             : DYNAMO_OPTION(cache_bb_align)))

/* We use a header to have a backpointer to the fragment_t */
/* FIXME: currently this abstraction type is unused, we rather use
 * *(fragment_t **) when working with the backpointer.  If are to add
 * more fields should use this.  Although fragment_t may be a better
 * place to keep such information.  */
typedef struct _live_header_t {
    /* FIXME: sizeof(live_header_t) should match HEADER_SIZE */
    fragment_t *f;
} live_header_t;

/* NOTE this must be a multiple of START_PC_ALIGNMENT bytes so that the
 * start_pc is properly aligned (for -pad_jmps_shift_{bb,trace} support) */
#define HEADER_SIZE(f) (TEST(FRAG_COARSE_GRAIN, (f)->flags) ? 0 : (sizeof(fragment_t *)))
#define HEADER_SIZE_FROM_CACHE(cache) ((cache)->is_coarse ? 0 : (sizeof(fragment_t *)))

/**************************************************
 * We use a FIFO replacement strategy
 * Empty slots in the cache are always at the front of the list, and they
 * take the form of the empty_slot_t struct.
 * Target fragments to delete are represented in the FIFO by their fragment_t
 * struct, using the next_fcache field to chain them.
 * We use a header for each fragment so we can delete adjacent-in-cache
 * (different from indirected FIFO list) to make contiguous space available.
 * Padding for alignment is always at the end of the fragment, so it can be
 * combined with empty space eaten up when deleting a fragment but only needing
 * some of its room.  We assume everything starts out aligned, and the same alignment
 * on the end of each slot keeps it aligned.
 * Thus:
 *           ------
 *           header
 *           <up to START_PC_ALIGNMENT-1 bytes padding, stored in alignment of start_pc>
 * start_pc: prefix
 *           body
 *           stubs
 *           <padding for alignment>
 *           ------
 *           header
 *           <up to START_PC_ALIGNMENT-1 bytes padding, stored in alignment of start_pc>
 * start_pc: ...
 */
typedef struct _empty_slot_t {
    cache_pc start_pc; /* very top of location in fcache */
    /* flags MUST be at same location as fragment_t->flags
     * we use flags==FRAG_IS_EMPTY_SLOT to indicate an empty slot
     */
    uint flags;
    fragment_t *next_fcache; /* for chaining fragments in fcache unit */
    fragment_t *prev_fcache; /* for chaining fragments in fcache unit */
    uint fcache_size;        /* size rounded up to cache line boundaries;
                              * not just ushort so we can merge adjacents;
                              * not size_t since each unit assumed <4GB. */
} empty_slot_t;

/* macros to make dealing with both fragment_t and empty_slot_t easier */
#define FRAG_EMPTY(f) (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))

#define FRAG_START(f)                                                       \
    (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags) ? ((empty_slot_t *)(f))->start_pc \
                                          : (f)->start_pc)

/* N.B.: must hold cache lock across any set of a fragment's start_pc or size
 * once that fragment is in a cache, as contig-cache-walkers need a consistent view!
 * FIXME: we can't assert as we can't do a unit lookup at all use sites
 */
#define FRAG_START_ASSIGN(f, val)                    \
    do {                                             \
        if (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))    \
            ((empty_slot_t *)(f))->start_pc = (val); \
        else                                         \
            (f)->start_pc = (val);                   \
    } while (0)

/* for -pad_jmps_shift_{bb,trace} we may have shifted the start_pc forward by up
 * to START_PC_ALIGNMENT-1 bytes, back align to get the right header pointer */
#define FRAG_START_PADDING(f)                                                      \
    ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags) || !PAD_JMPS_SHIFT_START((f)->flags))   \
         ? 0                                                                       \
         : (ASSERT(CHECK_TRUNCATE_TYPE_uint(                                       \
                (ptr_uint_t)((f)->start_pc -                                       \
                             ALIGN_BACKWARD((f)->start_pc, START_PC_ALIGNMENT)))), \
            (uint)((ptr_uint_t)(f)->start_pc -                                     \
                   ALIGN_BACKWARD((f)->start_pc, START_PC_ALIGNMENT))))

#define FRAG_HDR_START(f) (FRAG_START(f) - HEADER_SIZE(f) - FRAG_START_PADDING(f))

#define FRAG_SIZE(f)                          \
    ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))   \
         ? ((empty_slot_t *)(f))->fcache_size \
         : (f)->size + (uint)(f)->fcache_extra + FRAG_START_PADDING(f))

/* N.B.: must hold cache lock across any set of a fragment's start_pc or size
 * once that fragment is in a cache, as contig-cache-walkers need a consistent view!
 * FIXME: we can't assert as we can't do a unit lookup at all use sites
 */
#define FRAG_SIZE_ASSIGN(f, val)                                            \
    do {                                                                    \
        if (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags)) {                         \
            ASSERT_TRUNCATE(((empty_slot_t *)(f))->fcache_size, uint, val); \
            ((empty_slot_t *)(f))->fcache_size = (val);                     \
        } else {                                                            \
            /* cl has string limit so need temp to get ASSERT to compile */ \
            uint extra_tmp = ((val) - ((f)->size + FRAG_START_PADDING(f))); \
            ASSERT_TRUNCATE((f)->fcache_extra, byte, extra_tmp);            \
            (f)->fcache_extra = (byte)extra_tmp;                            \
        }                                                                   \
    } while (0)

#define FIFO_NEXT(f)                                \
    ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))         \
         ? ((empty_slot_t *)(f))->next_fcache       \
         : (ASSERT(!TEST(FRAG_SHARED, (f)->flags)), \
            ((private_fragment_t *)(f))->next_fcache))

#define FIFO_NEXT_ASSIGN(f, val)                              \
    do {                                                      \
        if (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))             \
            ((empty_slot_t *)(f))->next_fcache = (val);       \
        else {                                                \
            ASSERT(!TEST(FRAG_SHARED, (f)->flags));           \
            ((private_fragment_t *)(f))->next_fcache = (val); \
        }                                                     \
    } while (0)

#define FIFO_PREV(f)                                \
    ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))         \
         ? ((empty_slot_t *)(f))->prev_fcache       \
         : (ASSERT(!TEST(FRAG_SHARED, (f)->flags)), \
            ((private_fragment_t *)(f))->prev_fcache))

#define FIFO_PREV_ASSIGN(f, val)                              \
    do {                                                      \
        if (TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))             \
            ((empty_slot_t *)(f))->prev_fcache = (val);       \
        else {                                                \
            ASSERT(!TEST(FRAG_SHARED, (f)->flags));           \
            ((private_fragment_t *)(f))->prev_fcache = (val); \
        }                                                     \
    } while (0)

#define FRAG_TAG(f) \
    ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags)) ? ((empty_slot_t *)(f))->start_pc : (f)->tag)

#ifdef DEBUG
#    define FRAG_ID(f) ((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags)) ? -1 : (f)->id)
#endif

#define FIFO_UNIT(f)                                            \
    (fcache_lookup_unit((TEST(FRAG_IS_EMPTY_SLOT, (f)->flags))  \
                            ? (((empty_slot_t *)(f))->start_pc) \
                            : ((f)->start_pc)))

/* Shared fragments do NOT use a FIFO as they cannot easily replace existing
 * fragments.  Instead they use a free list (below).
 */
#define USE_FIFO(f) (!TEST(FRAG_SHARED, (f)->flags))
#define USE_FREE_LIST(f) \
    (TEST(FRAG_SHARED, (f)->flags) && !TEST(FRAG_COARSE_GRAIN, (f)->flags))
#define USE_FIFO_FOR_CACHE(c) (!(c)->is_shared)
#define USE_FREE_LIST_FOR_CACHE(c) ((c)->is_shared && !(c)->is_coarse)

/**************************************************
 * Free empty slot lists of different sizes for shared caches, where we
 * cannot easily delete victims to make room in too-small slots.
 */
static const uint FREE_LIST_SIZES[] = {
    /* Since we have variable sizes, and we do not pad to bucket
     * sizes, each bucket contains free slots that are in
     * [SIZE[bucket], SIZE[bucket+1]) where the top one is infinite.
     * case 7318 about further extensions
     *
     * Free slots in the current unit are added only if in the middle
     * of the unit (the last ones are turned back into unclaimed space).
     *
     * Tuned for bbs: smallest are 40 (with stubs), plurality are 56, distribution trails
     * off slowly beyond that.  Note that b/c of pad_jmps we request more than
     * the final sizes.
     * FIXME: these #s are for inlined stubs, should re-tune w/ separate stubs
     * (case 7163)
     */
    0, 44, 52, 56, 64, 72, 80, 112, 172
};
#define FREE_LIST_SIZES_NUM (BUFFER_SIZE_ELEMENTS(FREE_LIST_SIZES))

/* To support physical cache contiguity walking we store both a
 * next-free and prev-free pointer and a size at the top of the empty slot, and
 * don't waste memory with an empty_slot_t data structure.
 * We also use flags field to allow distinguishing free list slots
 * from live fragment_t's (see notes by flags field below).
 *
 * Our free list coalescing assumes that a fragment_t that follows a
 * free list entry has the FRAG_FOLLOWS_FREE_ENTRY flag set.
 *
 * FIXME: could avoid heap w/ normal empty_slot_t scheme: if don't have
 * separate stubs, empty_slot_t @ 20 bytes (no start_pc) should fit in
 * any cache slot.  Could save a few MB of heap on large apps.
 * (This is case 4937.)
 *
 * FIXME: If free lists work well we could try using for private
 * caches as well, instead of the empty_slot_t-struct-on-FIFO scheme.
 *
 * FIXME: unit pointer may be useful to avoid fcache_lookup_unit
 */
typedef struct _free_list_header_t {
    struct _free_list_header_t *next;
    /* We arrange this so that the FRAG_FCACHE_FREE_LIST flag will be set
     * at the proper bit as though this were a "uint flags" at the same offset
     * in the struct as fragment_t.flags.  Since no one else examines a free list
     * as though it might be a fragment_t, we don't care about the other flags.
     * We have an ASSERT in fcache_init() to ensure the byte ordering is right.
     *
     * Since we compare a fragment_t* to this inlined struct, we're really
     * comparing a fragment_t* to the first field, the next pointer.  So when we
     * de-reference the flags we're looking at the flags of the next entry in
     * the free list.  Thus to identify a free list entry we must check for
     * either NULL or for the FRAG_FCACHE_FREE_LIST flag.
     */
    uint flags;
    /* Although fragments are limited to ushort sizes, free entries are coalesced
     * and can get larger.  We thus make space for a larger size (i#4434), as the
     * only downside is a smaller MIN_FCACHE_SLOT_SIZE, which is still small enough.
     */
    uint size;
    struct _free_list_header_t *prev;
} free_list_header_t;

/* We also place a size field at the end of the free list slot.
 * Combined w/ the FRAG_FOLLOWS_FREE_ENTRY flag this allows us to coalesce
 * new free list entries with existing previous entries.
 */
typedef struct _free_list_footer_t {
    uint size;
} free_list_footer_t;

#define MAX_FREE_ENTRY_SIZE UINT_MAX

/* See notes above: since f is either fragment_t* or free_list_header_t.next,
 * we're checking the next free list entry's flags by dereferencing, forcing a
 * check for NULL as well (== end of list).
 */
#define FRAG_IS_FREE_LIST(f) ((f) == NULL || TEST(FRAG_FCACHE_FREE_LIST, (f)->flags))

/* De-references the fragment header stored at the start of the next
 * fcache slot, given a pc and a size for the current slot.
 */
#define FRAG_NEXT_SLOT(pc, size) (*((fragment_t **)((pc) + (size))))

/* Caller must know that the next slot is a free slot! */
#define FRAG_NEXT_FREE(pc, size) ((free_list_header_t *)((pc) + (size)))

/* XXX: For non-free-list-using caches we could shrink this.
 * Current smallest bb is 5 bytes (single jmp) align-4 + header is 12,
 * and we're at 20 here, so we are wasting some space, but few
 * fragments are under 20: some are at 16 for 32-bit but almost none
 * are smaller (see request_size_histogram[]).
 */
#define MIN_FCACHE_SLOT_SIZE(cache) \
    ((cache)->is_coarse ? 0 : (sizeof(free_list_header_t) + sizeof(free_list_footer_t)))

/**************************************************
 * single mmapped piece of cache
 */
struct _fcache;

#define UNIT_RESERVED_SIZE(u) ((size_t)((u)->reserved_end_pc - (u)->start_pc))

typedef struct _fcache_unit_t {
    cache_pc start_pc;        /* start address of fcache storage */
    cache_pc end_pc;          /* end address of committed storage, open-ended */
    cache_pc cur_pc;          /* if not filled up yet, bottom of cache */
    cache_pc reserved_end_pc; /* reservation end address, open-ended */
    size_t size;              /* committed size: equals (end_pc - start_pc) */
    bool full;                /* to tell whether cache is filled to end */
    struct _fcache *cache;    /* up-pointer to parent cache */
#if defined(SIDELINE) || defined(WINDOWS_PC_SAMPLE)
    dcontext_t *dcontext;
#endif
    bool writable; /* remember state of cache memory protection */
#ifdef WINDOWS_PC_SAMPLE
    /* We cache these values for units_to_{flush,free} units whose cache
     * field has been invalidated
     */
    bool was_trace;
    bool was_shared;
    profile_t *profile;
#endif
    bool per_thread;   /* Used for -per_thread_guard_pages. */
    bool pending_free; /* was entire unit flushed and slated for free? */
#ifdef DEBUG
    bool pending_flush; /* indicates in-limbo unit pre-flush is still live */
#endif
    uint flushtime;                     /* free this unit when this flushtime is freed --
                                         * used only for units_to_free list, else 0 */
    struct _fcache_unit_t *next_global; /* used to link all units */
    struct _fcache_unit_t *prev_global; /* used to link all units */
    struct _fcache_unit_t *next_local;  /* used to link an fcache_t's units */
} fcache_unit_t;

#ifdef DEBUG
/* study request allocation sizes */
enum {
    HISTOGRAM_GRANULARITY = 4,
    HISTOGRAM_MAX_SIZE = 256,
};
#endif /* DEBUG */

/**************************************************
 * one "code cache" of a single type of fragment, made up of potentially
 * multiple FcacheUnits
 */
typedef struct _fcache {
    /* FIXME: do we want space or perf here (bitfield vs full field)? */
    bool is_trace : 1; /* for varying alignment, etc. */
    bool is_shared : 1;
#ifdef DEBUG
    /* A local cache's pointer has not escaped to any other thread.
     * We only use this flag to get around lock ordering issues w/
     * persistent caches and we don't bother to set it for all
     * private caches.
     */
    bool is_local : 1; /* no lock needed since only known to this thread */
#endif
    /* Is this a dedicated coarse-grain cache unit */
    bool is_coarse : 1;
    fragment_t *fifo;     /* the FIFO list of fragments to delete.
                           * also includes empty slots as EmptySlots
                           * (all empty slots are at front of FIFO) */
    fcache_unit_t *units; /* list of all units, also FIFO -- the front
                           * of the list is the only potentially
                           * non-full unit */
    size_t size;          /* sum of sizes of all units */

    /* can't rely on bb_building_lock b/c shared deletion doesn't hold it,
     * and cleaner to have dedicated lock
     */
    mutex_t lock;

#ifdef DEBUG
    const char *name;
    bool consistent; /* used to avoid fcache_fragment_pclookup problems */
#endif

    /* backpointer for mapping cache pc to coarse info for inter-unit unlink */
    coarse_info_t *coarse_info;

    /* we cache parameter here so we don't have to dispatch on bb/trace
     * type every time -- this also allows flexibility if we ever want
     * to have different parameters per thread or something.
     * not much of a space hit at all since there are 2 caches per thread
     * and then 2 global caches.
     */
    size_t max_size; /* maximum sum of sizes */
    size_t max_unit_size;
    size_t max_quadrupled_unit_size;
    size_t free_upgrade_size;
    size_t init_unit_size;
    bool finite_cache;
    uint regen_param;
    uint replace_param;

    /* for adaptive working set: */
    uint num_regenerated;
    uint num_replaced; /* for shared cache, simply number created */
    /* for fifo caches, wset_check is simply an optimization to avoid
     * too many checks when parameters are such that regen<<replace
     */
    int wset_check;
    /* for non-fifo caches, this flag indicates we should start
     * recording num_regenerated and num_replaced
     */
    bool record_wset;

    free_list_header_t *free_list[FREE_LIST_SIZES_NUM];
#ifdef DEBUG
    uint free_stats_freed[FREE_LIST_SIZES_NUM];     /* occurrences */
    uint free_stats_reused[FREE_LIST_SIZES_NUM];    /* occurrences */
    uint free_stats_coalesced[FREE_LIST_SIZES_NUM]; /* occurrences */
    uint free_stats_split[FREE_LIST_SIZES_NUM];     /* entry split, occurrences */
    uint free_stats_charge[FREE_LIST_SIZES_NUM];    /* bytes on free list */
    /* sizes of real requests and frees */
    uint request_size_histogram[HISTOGRAM_MAX_SIZE / HISTOGRAM_GRANULARITY];
    uint free_size_histogram[HISTOGRAM_MAX_SIZE / HISTOGRAM_GRANULARITY];
#endif
} fcache_t;

/**************************************************
 * per-thread structure
 */
typedef struct _fcache_thread_units_t {
    fcache_t *bb;    /* basic block fcache */
    fcache_t *trace; /* trace fcache */
    /* we delay unmapping units, but only one at a time: */
    cache_pc pending_unmap_pc;
    size_t pending_unmap_size;
    /* are there units waiting to be flushed at a safe spot? */
    bool pending_flush;
} fcache_thread_units_t;

#define ALLOC_DC(dc, cache) ((cache)->is_shared ? GLOBAL_DCONTEXT : (dc))

/* We cannot acquire shared_cache_lock while allsynch-flushing as we then hold
 * the lower-ranked shared_vm_areas lock, but the allsynch makes it safe to not
 * acquire it.
 */
#define PROTECT_CACHE(cache, op)                                  \
    do {                                                          \
        if ((cache)->is_shared && !is_self_allsynch_flushing()) { \
            d_r_mutex_##op(&(cache)->lock);                       \
        }                                                         \
    } while (0);

#define CACHE_PROTECTED(cache)                                                \
    (!(cache)->is_shared || (cache)->is_local || OWN_MUTEX(&(cache)->lock) || \
     dynamo_all_threads_synched)

/**************************************************
 * global, unique thread-shared structure:
 */
typedef struct _fcache_list_t {
    /* These lists are protected by allunits_lock. */
    fcache_unit_t *units; /* list of all allocated fcache units */
    fcache_unit_t *dead;  /* list of deleted units ready for re-allocation */
    /* FIXME: num_dead duplicates d_r_stats->fcache_num_free, but we want num_dead
     * for release build too, so it's separate...can we do better?
     */
    uint num_dead;

    /* Global lists of cache units to flush and to free, chained by next_local
     * and kept on the live units list.  Protected by unit_flush_lock, NOT by
     * allunits_lock.
     * We keep these list pointers on the heap for selfprot (case 8074).
     */
    /* units to be flushed once at a safe spot */
    fcache_unit_t *units_to_flush;
    /* units to be freed once their contents are, kept sorted in
     * increasing flushtime, with a tail pointer to make appends easy
     */
    fcache_unit_t *units_to_free;
    fcache_unit_t *units_to_free_tail;
} fcache_list_t;

/* Kept on the heap for selfprot (case 7957). */
static fcache_list_t *allunits;
/* FIXME: rename to fcache_unit_lock? */
DECLARE_CXTSWPROT_VAR(static mutex_t allunits_lock, INIT_LOCK_FREE(allunits_lock));

DECLARE_CXTSWPROT_VAR(static mutex_t unit_flush_lock, INIT_LOCK_FREE(unit_flush_lock));

static fcache_t *shared_cache_bb;
static fcache_t *shared_cache_trace;

/* To locate the fcache_unit_t corresponding to a fragment or empty slot
 * we use an interval data structure rather than waste space with a
 * backpointer in each fragment.
 * Non-static so that synch routines in os.c can check its lock before calling
 * is_pc_recreatable which calls in_fcache.
 * Kept on the heap for selfprot (case 7957).
 */
vm_area_vector_t *fcache_unit_areas;

/* indicates a reset is in progress (whereas dynamo_resetting indicates that
 * all threads are suspended and so no synch is needed)
 */
DECLARE_FREQPROT_VAR(bool reset_in_progress, false);
/* protects reset triggers: reset_pending, reset_in_progress, reset_at_nth_thread
 * FIXME: use separate locks for separate triggers?
 * reset_at_nth_thread is wholly inside dynamo.c, e.g.
 */
DECLARE_CXTSWPROT_VAR(mutex_t reset_pending_lock, INIT_LOCK_FREE(reset_pending_lock));
/* indicates a call to fcache_reset_all_caches_proactively() is pending in d_r_dispatch */
DECLARE_FREQPROT_VAR(uint reset_pending, 0);

/* these cannot be per-cache since caches are reset so we have them act globally.
 * protected by allunits_lock since only read during unit creation.
 */
enum {
    CACHE_BB = 0,
    CACHE_TRACE,
    CACHE_NUM_TYPES,
};
DECLARE_FREQPROT_VAR(uint reset_at_nth_unit[CACHE_NUM_TYPES],
                     {
                         0,
                     });
DECLARE_FREQPROT_VAR(uint reset_every_nth_unit[CACHE_NUM_TYPES],
                     {
                         0,
                     });

#define STATS_FCACHE_ADD(cache, stat, val)                  \
    DOSTATS({                                               \
        if (cache->is_shared) {                             \
            if (cache->is_trace)                            \
                STATS_ADD(fcache_shared_trace_##stat, val); \
            else                                            \
                STATS_ADD(fcache_shared_bb_##stat, val);    \
        } else if (cache->is_trace)                         \
            STATS_ADD(fcache_trace_##stat, val);            \
        else                                                \
            STATS_ADD(fcache_bb_##stat, val);               \
    })

/* convenience routine to avoid casts to signed types everywhere */
#define STATS_FCACHE_SUB(cache, stat, val) \
    STATS_FCACHE_ADD(cache, stat, -(stats_int_t)(val))

#define STATS_FCACHE_MAX(cache, stat1, stat2)                                        \
    DOSTATS({                                                                        \
        if (cache->is_shared) {                                                      \
            if (cache->is_trace)                                                     \
                STATS_MAX(fcache_shared_trace_##stat1, fcache_shared_trace_##stat2); \
            else                                                                     \
                STATS_MAX(fcache_shared_bb_##stat1, fcache_shared_bb_##stat2);       \
        } else if (cache->is_trace)                                                  \
            STATS_MAX(fcache_trace_##stat1, fcache_trace_##stat2);                   \
        else                                                                         \
            STATS_MAX(fcache_bb_##stat1, fcache_bb_##stat2);                         \
    })

#ifdef DEBUG
/* forward decl */
#    ifdef INTERNAL
static void
verify_fifo(dcontext_t *dcontext, fcache_t *cache);

static void
print_fifo(dcontext_t *dcontext, fcache_t *cache);
#    endif

static void
fcache_cache_stats(dcontext_t *dcontext, fcache_t *cache);
#endif

static fcache_t *
fcache_cache_init(dcontext_t *dcontext, uint flags, bool initial_unit);

static void
fcache_cache_free(dcontext_t *dcontext, fcache_t *cache, bool free_units);

static void
add_to_free_list(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                 fragment_t *f, cache_pc start_pc, uint size);

static void
fcache_free_unit(dcontext_t *dcontext, fcache_unit_t *unit, bool dealloc_or_reuse);

#define CHECK_PARAMS(who, name, ret)                                                    \
    do {                                                                                \
        /* make it easier to set max */                                                 \
        if (FCACHE_OPTION(cache_##who##_max) > 0 &&                                     \
            FCACHE_OPTION(cache_##who##_max) < FCACHE_OPTION(cache_##who##_unit_max)) { \
            FCACHE_OPTION(cache_##who##_unit_max) = FCACHE_OPTION(cache_##who##_max);   \
            FCACHE_OPTION(cache_##who##_unit_init) = FCACHE_OPTION(cache_##who##_max);  \
            FCACHE_OPTION(cache_##who##_unit_quadruple) =                               \
                FCACHE_OPTION(cache_##who##_max);                                       \
            FCACHE_OPTION(cache_##who##_unit_upgrade) =                                 \
                FCACHE_OPTION(cache_##who##_max);                                       \
        }                                                                               \
        /* case 7626: don't short-circuit checks, as later ones may be needed */        \
        ret = check_param_bounds(&FCACHE_OPTION(cache_##who##_max), (uint)PAGE_SIZE, 0, \
                                 name " cache max size") ||                             \
            ret;                                                                        \
        /* N.B.: we assume cache unit max fits in uint */                               \
        ret = check_param_bounds(&FCACHE_OPTION(cache_##who##_unit_max),                \
                                 FCACHE_OPTION(cache_##who##_unit_init),                \
                                 FCACHE_OPTION(cache_##who##_max),                      \
                                 name " cache unit max size") ||                        \
            ret;                                                                        \
        ret = check_param_bounds(&FCACHE_OPTION(cache_##who##_unit_quadruple),          \
                                 FCACHE_OPTION(cache_##who##_unit_init),                \
                                 FCACHE_OPTION(cache_##who##_max),                      \
                                 name " cache unit quadruple-to size") ||               \
            ret;                                                                        \
        ret = check_param_bounds(&FCACHE_OPTION(cache_##who##_unit_upgrade),            \
                                 FCACHE_OPTION(cache_##who##_unit_init),                \
                                 FCACHE_OPTION(cache_##who##_max),                      \
                                 name " cache unit free upgrade size") ||               \
            ret;                                                                        \
        ret =                                                                           \
            check_param_bounds(                                                         \
                &FCACHE_OPTION(cache_##who##_unit_init), /* x64 does not support        \
                                                            resizing fcache units */    \
                IF_X64_ELSE(FCACHE_OPTION(cache_##who##_unit_max), (uint)PAGE_SIZE),    \
                FCACHE_OPTION(cache_##who##_unit_max), name " cache unit init size") || \
            ret;                                                                        \
        /* We let cache_commit_increment be any size to support raising it w/o */       \
        /* setting a dozen unit sizes. */                                               \
    } while (0);

#define CHECK_WSET_PARAM(param, ret)                                                   \
    do {                                                                               \
        if (dynamo_options.cache_##param##_regen < 0) {                                \
            USAGE_ERROR("-cache_" #param "_regen must be >= 0, is %d, setting to 0",   \
                        dynamo_options.cache_##param##_regen);                         \
            dynamo_options.cache_##param##_regen = 0;                                  \
            ret = true;                                                                \
        }                                                                              \
        if (dynamo_options.cache_##param##_replace < 0) {                              \
            USAGE_ERROR("-cache_" #param "_replace must be >= 0, id %d, setting to 0", \
                        dynamo_options.cache_##param##_replace);                       \
            dynamo_options.cache_##param##_replace = 0;                                \
            ret = true;                                                                \
        }                                                                              \
        if (dynamo_options.cache_##param##_replace != 0 &&                             \
            dynamo_options.cache_##param##_regen >                                     \
                dynamo_options.cache_##param##_replace) {                              \
            USAGE_ERROR("-cache_" #param "_regen (currently %d) must be <= "           \
                        "-cache_" #param "_replace (currently %d) (if -cache_" #param  \
                        "_replace > 0), setting regen to equal replace",               \
                        dynamo_options.cache_##param##_regen,                          \
                        dynamo_options.cache_##param##_replace);                       \
            dynamo_options.cache_##param##_regen =                                     \
                dynamo_options.cache_##param##_replace;                                \
            ret = true;                                                                \
        }                                                                              \
    } while (0);

/* pulled out from fcache_init, checks for compatibility among the fcache
 * options, returns true if modified the value of any options to make them
 * compatible.  This is called while the options are writable. */
bool
fcache_check_option_compatibility()
{
    bool ret = false;
    uint i;
    CHECK_PARAMS(bb, "Basic block", ret);
    CHECK_PARAMS(trace, "Trace", ret);
    CHECK_WSET_PARAM(bb, ret);
    CHECK_WSET_PARAM(trace, ret);
    if (DYNAMO_OPTION(shared_bbs)) {
        if (DYNAMO_OPTION(cache_shared_bb_max) > 0) {
            /* case 8203: NYI */
            USAGE_ERROR("-cache_shared_bb_max != 0 not supported");
            dynamo_options.cache_shared_bb_max = 0;
            ret = true;
        }
        CHECK_PARAMS(shared_bb, "Shared bb", ret);
        CHECK_WSET_PARAM(shared_bb, ret);
        /* FIXME: cannot handle resizing of cache, separate units only */
        /* case 7626: don't short-circuit checks, as later ones may be needed */
        ret = check_param_bounds(&FCACHE_OPTION(cache_shared_bb_unit_init),
                                 FCACHE_OPTION(cache_shared_bb_unit_max),
                                 FCACHE_OPTION(cache_shared_bb_unit_max),
                                 "cache_shared_bb_unit_init should equal "
                                 "cache_shared_bb_unit_max") ||
            ret;
    }
    if (DYNAMO_OPTION(shared_traces)) {
        if (DYNAMO_OPTION(cache_shared_trace_max) > 0) {
            /* case 8203: NYI */
            USAGE_ERROR("-cache_shared_trace_max != 0 not supported");
            dynamo_options.cache_shared_trace_max = 0;
            ret = true;
        }
        CHECK_PARAMS(shared_trace, "Shared trace", ret);
        CHECK_WSET_PARAM(shared_trace, ret);
        /* FIXME: cannot handle resizing of cache, separate units only */
        ret = check_param_bounds(&FCACHE_OPTION(cache_shared_trace_unit_init),
                                 FCACHE_OPTION(cache_shared_trace_unit_max),
                                 FCACHE_OPTION(cache_shared_trace_unit_max),
                                 "cache_shared_trace_unit_init should equal "
                                 "cache_shared_trace_unit_max") ||
            ret;
    }
    if (INTERNAL_OPTION(pad_jmps_shift_bb) &&
        DYNAMO_OPTION(cache_bb_align) < START_PC_ALIGNMENT) {
        USAGE_ERROR("if -pad_jmps_shift_bb, -cache_bb_align must be >= %d",
                    START_PC_ALIGNMENT);
        dynamo_options.cache_bb_align = START_PC_ALIGNMENT;
        ret = true;
    }
    /* (case 8647: cache_coarse_align can be anything as we don't pad jmps) */
    if (INTERNAL_OPTION(pad_jmps_shift_trace) &&
        DYNAMO_OPTION(cache_trace_align) < START_PC_ALIGNMENT) {
        USAGE_ERROR("if -pad_jmps_shift_trace, -cache_trace_align must be >= %d",
                    START_PC_ALIGNMENT);
        dynamo_options.cache_trace_align = START_PC_ALIGNMENT;
        ret = true;
    }
    reset_at_nth_unit[CACHE_BB] = DYNAMO_OPTION(reset_at_nth_bb_unit);
    reset_every_nth_unit[CACHE_BB] = DYNAMO_OPTION(reset_every_nth_bb_unit);
    reset_at_nth_unit[CACHE_TRACE] = DYNAMO_OPTION(reset_at_nth_trace_unit);
    reset_every_nth_unit[CACHE_TRACE] = DYNAMO_OPTION(reset_every_nth_trace_unit);
    /* yes can set both to different values -- but every won't kick in until
     * after first at
     */
    for (i = 0; i < CACHE_NUM_TYPES; i++) {
        if (reset_every_nth_unit[i] > 0 && reset_at_nth_unit[i] == 0)
            reset_at_nth_unit[i] = reset_every_nth_unit[i];
    }
    return ret;
}

/* thread-shared initialization that should be repeated after a reset */
static void
fcache_reset_init(void)
{
    /* case 7966: don't initialize at all for hotp_only & thin_client
     * FIXME: could set initial sizes to 0 for all configurations, instead
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    if (DYNAMO_OPTION(shared_bbs)) {
        shared_cache_bb = fcache_cache_init(GLOBAL_DCONTEXT, FRAG_SHARED, true);
        ASSERT(shared_cache_bb != NULL);
        LOG(GLOBAL, LOG_CACHE, 1, "Initial shared bb cache is %d KB\n",
            shared_cache_bb->init_unit_size / 1024);
    }
    if (DYNAMO_OPTION(shared_traces)) {
        shared_cache_trace =
            fcache_cache_init(GLOBAL_DCONTEXT, FRAG_SHARED | FRAG_IS_TRACE, true);
        ASSERT(shared_cache_trace != NULL);
        LOG(GLOBAL, LOG_CACHE, 1, "Initial shared trace cache is %d KB\n",
            shared_cache_trace->init_unit_size / 1024);
    }
}

/* initialization -- needs no locks */
void
fcache_init()
{
    ASSERT(offsetof(fragment_t, flags) == offsetof(empty_slot_t, flags));
    DOCHECK(1, {
        /* ensure flag in free list is at same spot as in fragment_t */
        static free_list_header_t free;
        free.flags = FRAG_FAKE | FRAG_FCACHE_FREE_LIST;
        ASSERT(TEST(FRAG_FCACHE_FREE_LIST, ((fragment_t *)(&free))->flags));
        /* ensure treating fragment_t* as next will work */
        ASSERT(offsetof(free_list_header_t, next) == offsetof(live_header_t, f));
    });

    /* we rely on this */
    ASSERT(FREE_LIST_SIZES[0] == 0);

    VMVECTOR_ALLOC_VECTOR(fcache_unit_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, fcache_unit_areas);

    allunits = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fcache_list_t, ACCT_OTHER, PROTECTED);
    allunits->units = NULL;
    allunits->dead = NULL;
    allunits->num_dead = 0;
    allunits->units_to_flush = NULL;
    allunits->units_to_free = NULL;
    allunits->units_to_free_tail = NULL;

    fcache_reset_init();
}

#ifdef WINDOWS_PC_SAMPLE
static void
fcache_unit_profile_stop(fcache_unit_t *u)
{
    int sum;

    stop_profile(u->profile);
    sum = sum_profile(u->profile);
    if (sum > 0) {
        bool shared, trace;
        if (u->cache == NULL) {
            shared = u->was_shared;
            trace = u->was_trace;
        } else {
            shared = u->cache->is_shared;
            trace = u->cache->is_trace;
        }
        d_r_mutex_lock(&profile_dump_lock);
        if (shared) {
            print_file(profile_file,
                       "\nDumping fcache %s unit profile (Shared)\n%d hits\n",
                       trace ? "trace" : "bb", sum);
        } else {
            print_file(profile_file,
                       "\nDumping fcache %s unit profile (Thread %d)\n%d hits\n",
                       trace ? "trace" : "bb", u->dcontext->owning_thread, sum);
        }
        dump_profile(profile_file, u->profile);
        d_r_mutex_unlock(&profile_dump_lock);
    }
}
#endif

static inline void
remove_unit_from_cache(fcache_unit_t *u)
{
    ASSERT(u->cache != NULL);
    u->cache->size -= u->size;
    RSTATS_DEC(fcache_num_live);
    STATS_FCACHE_SUB(u->cache, capacity, u->size);
#ifdef WINDOWS_PC_SAMPLE
    /* Units moved to units_to_{flush,free} can't have their profile stopped
     * until they are really freed, so we must cache their type here.
     * We do need to clear their cache field to support private or other
     * deletable flushable unit types (though w/ default ops today no flushable
     * unit will have its cache deleted).
     */
    u->was_trace = u->cache->is_trace;
    u->was_shared = u->cache->is_shared;
#endif
    u->cache = NULL;
}

static void
fcache_really_free_unit(fcache_unit_t *u, bool on_dead_list, bool dealloc_unit)
{
    if (TEST(SELFPROT_CACHE, dynamo_options.protect_mask) && !u->writable) {
        change_protection((void *)u->start_pc, u->size, WRITABLE);
    }
#ifdef WINDOWS_PC_SAMPLE
    if (u->profile != NULL) {
        if (!on_dead_list)
            fcache_unit_profile_stop(u);
        free_profile(u->profile);
        u->profile = NULL;
    }
#endif
    if (u->cache != NULL)
        remove_unit_from_cache(u);
    if (on_dead_list) {
        ASSERT(u->cache == NULL);
        allunits->num_dead--;
        RSTATS_DEC(fcache_num_free);
        STATS_SUB(fcache_free_capacity, u->size);
    }
    RSTATS_SUB(fcache_combined_capacity, u->size);
    /* remove from interval data struct first to avoid races w/ it
     * being re-used and not showing up in in_fcache
     */
    vmvector_remove(fcache_unit_areas, u->start_pc, u->reserved_end_pc);
    if (dealloc_unit) {
        heap_munmap((void *)u->start_pc, UNIT_RESERVED_SIZE(u),
                    VMM_CACHE | VMM_REACHABLE | (u->per_thread ? VMM_PER_THREAD : 0));
    }
    /* always dealloc the metadata */
    nonpersistent_heap_free(GLOBAL_DCONTEXT, u,
                            sizeof(fcache_unit_t) HEAPACCT(ACCT_MEM_MGT));
}

#ifdef DEBUG
/* needs to be called before fragment_exit */
void
fcache_stats_exit()
{
    if (DYNAMO_OPTION(shared_bbs)) {
        fcache_t *cache = shared_cache_bb;
        /* cache may be NULL, for stats called after fcache_exit() */
        if (cache != NULL) {
            /* FIXME: report_dynamorio_problem() calls
             * dump_global_stats() which currently regularly calls
             * this, so any ASSERTs on this path will deadlock
             * (workaround is to be vigilant and use msgbox_mask).
             */
            ASSERT_DO_NOT_OWN_MUTEX(cache->is_shared, &cache->lock);
            PROTECT_CACHE(cache, lock);
            fcache_cache_stats(GLOBAL_DCONTEXT, cache);
            PROTECT_CACHE(cache, unlock);
        }
    }
    if (DYNAMO_OPTION(shared_traces)) {
        fcache_t *cache = shared_cache_trace;
        if (cache != NULL) {
            ASSERT_DO_NOT_OWN_MUTEX(cache->is_shared, &cache->lock);
            PROTECT_CACHE(cache, lock);
            fcache_cache_stats(GLOBAL_DCONTEXT, cache);
            PROTECT_CACHE(cache, unlock);
        }
    }
}
#endif

/* Free all thread-shared state not critical to forward progress;
 * fcache_reset_init() will be called before continuing.
 */
static void
fcache_reset_free(void)
{
    fcache_unit_t *u, *next_u;

    /* case 7966: don't initialize at all for hotp_only & thin_client
     * FIXME: could set initial sizes to 0 for all configurations, instead
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

    /* FIXME: for reset (not exit), optimize to avoid calling
     * fcache_really_free_unit() to move units onto dead list only to delete
     * here: should directly delete, but maintain fcache stats.
     */

    /* we do not acquire each shared cache's lock for reset, assuming no
     * synch issues (plus the lock will be deleted)
     */
    if (DYNAMO_OPTION(shared_bbs)) {
        fcache_cache_free(GLOBAL_DCONTEXT, shared_cache_bb, true);
        shared_cache_bb = NULL;
    }
    if (DYNAMO_OPTION(shared_traces)) {
        fcache_cache_free(GLOBAL_DCONTEXT, shared_cache_trace, true);
        shared_cache_trace = NULL;
    }

    /* there may be units stranded on the to-flush list.
     * we must free the units here as they are unreachable elsewhere.
     * their fragments will be freed by the fragment htable walk.
     */
    d_r_mutex_lock(&unit_flush_lock);
    u = allunits->units_to_flush;
    while (u != NULL) {
        next_u = u->next_local;
        LOG(GLOBAL, LOG_CACHE, 2,
            "@ reset-free freeing to-be-flushed unit " PFX "-" PFX "\n", u->start_pc,
            u->end_pc);
        fcache_free_unit(GLOBAL_DCONTEXT, u, true);
        u = next_u;
    }
    allunits->units_to_flush = NULL;
    d_r_mutex_unlock(&unit_flush_lock);

    /* should be freed via vm_area_check_shared_pending() */
    ASSERT(allunits->units_to_free == NULL);

    d_r_mutex_lock(&allunits_lock);
    u = allunits->dead;
    while (u != NULL) {
        next_u = u->next_global;
        fcache_really_free_unit(u, true /*on dead list*/, true /*dealloc*/);
        u = next_u;
    }
    /* clear fields for reset_init() */
    allunits->dead = NULL;
    allunits->num_dead = 0;
    d_r_mutex_unlock(&allunits_lock);
}

/* atexit cleanup -- needs no locks */
void
fcache_exit()
{
    fcache_unit_t *u, *next_u;

    DOSTATS({
        LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1, "fcache_exit: before fcache cleanup\n");
        DOLOG(1, LOG_CACHE, fcache_stats_exit(););
    });

    fcache_reset_free();

    /* free heap for all live units (reset did dead ones) */
    d_r_mutex_lock(&allunits_lock);
    u = allunits->units;
    while (u != NULL) {
        next_u = u->next_global;
        fcache_really_free_unit(u, false /*live*/, true /*dealloc*/);
        u = next_u;
    }
    d_r_mutex_unlock(&allunits_lock);

    ASSERT(vmvector_empty(fcache_unit_areas));
    vmvector_delete_vector(GLOBAL_DCONTEXT, fcache_unit_areas);

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, allunits, fcache_list_t, ACCT_OTHER, PROTECTED);

    reset_pending = 0; /* For reattach. */

    DELETE_LOCK(allunits_lock);
    DELETE_LOCK(reset_pending_lock);
    DELETE_LOCK(unit_flush_lock);
}

#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
/* for fast exit path only, normal path taken care of in free unit*/
void
fcache_profile_exit()
{
    fcache_unit_t *u;
    d_r_mutex_lock(&allunits_lock);
    for (u = allunits->units; u != NULL; u = u->next_global) {
        if (u->profile) {
            fcache_unit_profile_stop(u);
            free_profile(u->profile);
            u->profile = NULL;
        }
    }
    d_r_mutex_unlock(&allunits_lock);
}
#endif

static fcache_unit_t *
fcache_lookup_unit(cache_pc pc)
{
    /* let's see if this becomes frequent enough to be a perf hit */
    STATS_INC(fcache_unit_lookups);
    return (fcache_unit_t *)vmvector_lookup(fcache_unit_areas, pc);
}

/* Returns the fragment_t whose body (not cache slot) contains lookup_pc */
fragment_t *
fcache_fragment_pclookup(dcontext_t *dcontext, cache_pc lookup_pc, fragment_t *wrapper)
{
    fragment_t *f, *found = NULL;
    cache_pc pc;
    fcache_unit_t *unit = fcache_lookup_unit(lookup_pc);
    if (unit == NULL)
        return NULL;
    LOG(THREAD, LOG_CACHE, 5, "fcache_fragment_pclookup " PFX " -> " PFX "-" PFX "\n",
        lookup_pc, unit->start_pc, unit->end_pc);
    if (unit->cache->is_coarse) {
        /* No metadata in cache so we must walk the htable.  We shouldn't need
         * to lock the cache itself.
         */
        coarse_info_t *info = unit->cache->coarse_info;
        cache_pc body;
        app_pc tag;
        ASSERT(info != NULL);
        tag = fragment_coarse_pclookup(dcontext, info, lookup_pc, &body);
        ASSERT(wrapper != NULL);
        fragment_coarse_wrapper(wrapper, tag, body);
        return wrapper;
    }
    PROTECT_CACHE(unit->cache, lock);
    DODEBUG({
        if (!unit->cache->consistent) {
            /* We're in the middle of an fcache operation during which we cannot
             * physically walk the cache.  ASSUMPTION: this only happens for
             * debug builds when we pclookup on disassembly.
             */
            PROTECT_CACHE(unit->cache, unlock);
            return fragment_pclookup_by_htable(dcontext, lookup_pc, wrapper);
        }
    });
    pc = unit->start_pc;
    while (pc < unit->cur_pc && pc < lookup_pc) {
        f = *((fragment_t **)pc);
        LOG(THREAD, LOG_CACHE, 6, "\treading " PFX " -> " PFX "\n", pc, f);
        if (!USE_FIFO_FOR_CACHE(unit->cache)) {
            if (FRAG_IS_FREE_LIST(f)) {
                pc += ((free_list_header_t *)pc)->size;
                continue;
            }
        }
        ASSERT(f != NULL);
        ASSERT(FIFO_UNIT(f) == unit);
        ASSERT(FRAG_HDR_START(f) == pc);
        if (!FRAG_EMPTY(f) && lookup_pc < (f->start_pc + f->size) &&
            lookup_pc >= f->start_pc) {
            found = f;
            LOG(THREAD, LOG_CACHE, 5, "\tfound F%d (" PFX ")." PFX "\n", f->id, f->tag,
                f->start_pc);
            break;
        }
        /* advance to contiguously-next fragment_t in cache */
        pc += FRAG_SIZE(f);
    }
    PROTECT_CACHE(unit->cache, unlock);
    return found;
}

/* This is safe to call from a signal handler. */
dr_where_am_i_t
fcache_refine_whereami(dcontext_t *dcontext, dr_where_am_i_t whereami, app_pc pc,
                       OUT fragment_t **containing_fragment)
{
    if (whereami != DR_WHERE_FCACHE) {
        if (containing_fragment != NULL)
            *containing_fragment = NULL;
        return whereami;
    }
    fragment_t wrapper;
    fragment_t *fragment = fragment_pclookup(dcontext, pc, &wrapper);
    if (fragment == NULL) {
        /* Since we're DR_WHERE_FCACHE, our locks shouldn't be held.
         * XXX: we could double-check fcache_unit_areas.lock before
         * calling (case 1317) and assert on it.
         */
        if (in_fcache(pc)) {
            whereami = DR_WHERE_UNKNOWN;
        } else {
            /* Identify parts of our assembly code now.
             * It's all generated and post-process can't identify.
             * Assume code order is as follows:
             */
            if (in_indirect_branch_lookup_code(dcontext, (cache_pc)pc)) {
                whereami = DR_WHERE_IBL;
            } else if (in_generated_routine(dcontext, (cache_pc)pc)) {
                /* We consider any non-ibl generated code as "context switch":
                 * not just private or shared fcache_{enter,return} but also
                 * do_syscall and other common transition code.
                 */
                whereami = DR_WHERE_CONTEXT_SWITCH;
            } else {
                whereami = DR_WHERE_UNKNOWN;
            }
        }
    }
    if (containing_fragment != NULL)
        *containing_fragment = fragment;
    return whereami;
}

#ifdef DEBUG
static bool
fcache_pc_in_live_unit(fcache_t *cache, cache_pc pc)
{
    fcache_unit_t *unit;
    for (unit = cache->units; unit != NULL; unit = unit->next_local) {
        if (pc >= unit->start_pc && pc < unit->end_pc)
            return true;
    }
    unit = fcache_lookup_unit(pc);
    /* pending flush is still considered live: removed from all lists
     * just prior to flush synch
     */
    if (unit != NULL && unit->pending_flush)
        return true;
    return false;
}
#endif

bool
fcache_is_writable(fragment_t *f)
{
    fcache_unit_t *unit = fcache_lookup_unit(f->start_pc);
    ASSERT(unit != NULL);
    return unit->writable;
}

/* If f is null, changes protection of entire fcache
 * Else, does the unit f is part of
 */
void
fcache_change_fragment_protection(dcontext_t *dcontext, fragment_t *f, bool writable)
{
    fcache_unit_t *u;
    ASSERT(TEST(SELFPROT_CACHE, dynamo_options.protect_mask));
    if (f != NULL) {
        u = fcache_lookup_unit(f->start_pc);
        ASSERT(u != NULL);
        if (u->writable == writable)
            return;
        change_protection((void *)u->start_pc, u->size, writable);
        u->writable = writable;
    } else {
        /* else, do entire fcache
         * win32 does not allow single protection change call on units that
         * were allocated with separate calls so we don't try to combine
         * adjacent units here
         */
        /* FIXME: right now no synch here, so one thread could unprot, another prots,
         * and the first segfaults
         */
        d_r_mutex_lock(&allunits_lock);
        u = allunits->units;
        while (u != NULL) {
            if (u->writable != writable) {
                change_protection((void *)u->start_pc, u->size, writable);
                u->writable = writable;
            }
            u = u->next_global;
        }
        d_r_mutex_unlock(&allunits_lock);
    }
}

/* return true if pc is in the fcache address space */
/* This routine can be called with a thread suspended in an unknown state.
 * Currently only the fcache_unit_areas write lock is checked, so if
 * this routine is changed
 * to grab any other locks, or call a routine that does, then the
 * at_safe_spot() routine in os.c must be updated */
bool
in_fcache(void *pc)
{
    return fcache_lookup_unit((cache_pc)pc) != NULL;
}

/* Pass NULL for pc if this routine should allocate the cache space.
 * If pc is non-NULL, this routine assumes that size is fully
 * committed and initializes accordingly.
 */
static fcache_unit_t *
fcache_create_unit(dcontext_t *dcontext, fcache_t *cache, cache_pc pc, size_t size)
{
    fcache_unit_t *u = NULL;
    uint cache_type;
    ASSERT(CACHE_PROTECTED(cache));

    /* currently we assume for FIFO empties that we can't have a single
     * contiguous empty slot that overflows a uint.
     * minor: all our stats only take ints, as well
     */
    ASSERT(CHECK_TRUNCATE_TYPE_uint(size));
    ASSERT(ALIGNED(size, PAGE_SIZE));

    if (pc == NULL) {
        /* take from dead list if possible */
        d_r_mutex_lock(&allunits_lock);
        if (allunits->dead != NULL) {
            fcache_unit_t *prev_u = NULL;
            u = allunits->dead;
            while (u != NULL) {
                /* We are ok re-using a per-thread-guarded unit in a shared cache. */
                if (u->size >= size &&
                    (cache->max_size == 0 || cache->size + u->size <= cache->max_size)) {
                    /* remove from dead list */
                    if (prev_u == NULL)
                        allunits->dead = u->next_global;
                    else
                        prev_u->next_global = u->next_global;
                    LOG(THREAD, LOG_CACHE, 1,
                        "\tFound unit " PFX " of size %d (need %d) on dead list\n",
                        u->start_pc, u->size / 1024, size / 1024);
                    allunits->num_dead--;
                    RSTATS_DEC(fcache_num_free);
                    STATS_SUB(fcache_free_capacity, u->size);
#ifdef WINDOWS_PC_SAMPLE
                    if (u->profile) {
                        reset_profile(u->profile);
                        start_profile(u->profile);
                    }
#endif
                    break;
                }
                prev_u = u;
                u = u->next_global;
            }
        }
        d_r_mutex_unlock(&allunits_lock);
    }

    if (u == NULL) {
        size_t commit_size;
        /* use global heap b/c this can be re-used by later threads */
        u = (fcache_unit_t *)nonpersistent_heap_alloc(
            GLOBAL_DCONTEXT, sizeof(fcache_unit_t) HEAPACCT(ACCT_MEM_MGT));
        u->per_thread = false;
        if (pc != NULL) {
            u->start_pc = pc;
            commit_size = size;
            STATS_FCACHE_ADD(cache, claimed, size);
            STATS_ADD(fcache_combined_claimed, size);
        } else {
            /* Allocate new unit. */
            commit_size = DYNAMO_OPTION(cache_commit_increment);
            /* Cap the commit size at this unit's size.  Since we
             * have a single param for the commit size, this
             * makes it much easier to set it without having to
             * set a dozen unrelated unit sizes too.
             */
            if (commit_size > size)
                commit_size = size;
            which_vmm_t which = VMM_CACHE | VMM_REACHABLE;
            if (!cache->is_shared && cache->units == NULL) {
                /* Tradeoff (i#4424): no guard pages on per-thread initial units, to
                 * save space for many-threaded apps.  These units are rarely used.
                 * We do not bother to mark subsequent units this way: the goal is to
                 * reduce up-front per-thread costs in common usage, while additional
                 * units indicate -thread_private or other settings.
                 */
                which |= VMM_PER_THREAD;
                u->per_thread = true;
            }
            u->start_pc = (cache_pc)heap_mmap_reserve(
                size, commit_size, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE, which);
        }
        ASSERT(u->start_pc != NULL);
        ASSERT(proc_is_cache_aligned((void *)u->start_pc));
        LOG(THREAD, LOG_HEAP, 3, "fcache_create_unit -> " PFX "\n", u->start_pc);
        u->size = commit_size;
        u->end_pc = u->start_pc + commit_size;
        u->reserved_end_pc = u->start_pc + size;
        vmvector_add(fcache_unit_areas, u->start_pc, u->reserved_end_pc, (void *)u);
        RSTATS_ADD_PEAK(fcache_combined_capacity, u->size);

#ifdef WINDOWS_PC_SAMPLE
        if (dynamo_options.profile_pcs && dynamo_options.prof_pcs_fcache >= 2 &&
            dynamo_options.prof_pcs_fcache <= 32) {
            u->profile = create_profile(u->start_pc, u->reserved_end_pc,
                                        dynamo_options.prof_pcs_fcache, NULL);
            start_profile(u->profile);
        } else
            u->profile = NULL;
#endif
    }

    cache->size += u->size;

    u->cur_pc = u->start_pc;
    u->full = false;
    u->cache = cache;
#if defined(SIDELINE) || defined(WINDOWS_PC_SAMPLE)
    u->dcontext = dcontext;
#endif
    u->writable = true;
    u->pending_free = false;
    DODEBUG({ u->pending_flush = false; });
    u->flushtime = 0;

    RSTATS_ADD_PEAK(fcache_num_live, 1);
    STATS_FCACHE_ADD(u->cache, capacity, u->size);
    STATS_FCACHE_MAX(u->cache, capacity_peak, capacity);

    u->next_local = NULL; /* must be set by caller */
    d_r_mutex_lock(&allunits_lock);

    if (allunits->units != NULL)
        allunits->units->prev_global = u;
    u->next_global = allunits->units;
    u->prev_global = NULL;
    allunits->units = u;

    cache_type = cache->is_trace ? CACHE_TRACE : CACHE_BB;
    if (reset_at_nth_unit[cache_type] > 0) {
        /* reset on nth NEW unit (ignoring total unit count, whether
         * some were flushed, etc.)
         */
        reset_at_nth_unit[cache_type]--;
        if (reset_at_nth_unit[cache_type] == 0) {
            schedule_reset(RESET_ALL);
            if (reset_every_nth_unit[cache_type] > 0)
                reset_at_nth_unit[cache_type] = reset_every_nth_unit[cache_type];
        }
    }

    d_r_mutex_unlock(&allunits_lock);

    return u;
}

/* this routine does NOT remove the unit from its local cache list */
static void
fcache_free_unit(dcontext_t *dcontext, fcache_unit_t *unit, bool dealloc_or_reuse)
{
    DODEBUG({
        if (unit->flushtime > 0) {
            ASSERT_OWN_MUTEX(true, &unit_flush_lock);
            /* we set to 0 to avoid this assert on fcache_reset_exit()
             * when freeing dead units -- not needed for release build
             */
            unit->flushtime = 0;
        } else {
            ASSERT(dynamo_exited || dynamo_resetting || CACHE_PROTECTED(unit->cache));
        }
    });
    d_r_mutex_lock(&allunits_lock);
    /* remove from live list */
    if (unit->prev_global != NULL)
        unit->prev_global->next_global = unit->next_global;
    else
        allunits->units = unit->next_global;
    if (unit->next_global != NULL)
        unit->next_global->prev_global = unit->prev_global;
    STATS_FCACHE_SUB(unit->cache, claimed, (unit->cur_pc - unit->start_pc));
    STATS_FCACHE_SUB(unit->cache, empty, (unit->cur_pc - unit->start_pc));
    STATS_SUB(fcache_combined_claimed, (unit->cur_pc - unit->start_pc));

    if (!dealloc_or_reuse) {
        /* up to caller to dealloc */
        d_r_mutex_unlock(&allunits_lock);
        /* we do want to update cache->size and fcache_unit_areas: */
        fcache_really_free_unit(unit, false /*live*/, false /*do not dealloc unit*/);
    }
    /* heuristic: don't keep around more dead units than max(5, 1/4 num threads) */
    else if (allunits->num_dead < 5 ||
             allunits->num_dead * 4U <= (uint)d_r_get_num_threads()) {
        /* Keep dead list sorted small-to-large to avoid grabbing large
         * when can take small and then needing to allocate when only
         * have small left.  Helps out with lots of small threads.
         */
        fcache_unit_t *u, *prev_u;
        for (u = allunits->dead, prev_u = NULL; u != NULL && u->size < unit->size;
             prev_u = u, u = u->next_global)
            ;
        /* prev_global and next_local are not used in the dead list */
        unit->prev_global = NULL;
        unit->next_local = NULL;
        if (prev_u == NULL) {
            unit->next_global = allunits->dead;
            allunits->dead = unit;
        } else {
            unit->next_global = u;
            prev_u->next_global = unit;
        }
        allunits->num_dead++;
        RSTATS_ADD_PEAK(fcache_num_free, 1);
        STATS_ADD(fcache_free_capacity, unit->size);
#ifdef WINDOWS_PC_SAMPLE
        if (unit->profile)
            fcache_unit_profile_stop(unit);
#endif
        /* this is done by fcache_really_free_unit for else path */
        remove_unit_from_cache(unit);
        d_r_mutex_unlock(&allunits_lock);
    } else {
        d_r_mutex_unlock(&allunits_lock);
        fcache_really_free_unit(unit, false /*live*/, true /*dealloc*/);
    }
}

/* We do not consider guard pages in our sizing, since the VMM no longer uses
 * larger-than-page block sizing (i#2607, i#4424).  Guards will be added on top.
 */
#define SET_CACHE_PARAMS(cache, which)                                                   \
    do {                                                                                 \
        cache->max_size = FCACHE_OPTION(cache_##which##_max);                            \
        cache->max_unit_size = FCACHE_OPTION(cache_##which##_unit_max);                  \
        cache->max_quadrupled_unit_size = FCACHE_OPTION(cache_##which##_unit_quadruple); \
        cache->free_upgrade_size = FCACHE_OPTION(cache_##which##_unit_upgrade);          \
        cache->init_unit_size = FCACHE_OPTION(cache_##which##_unit_init);                \
        cache->finite_cache = dynamo_options.finite_##which##_cache;                     \
        cache->regen_param = dynamo_options.cache_##which##_regen;                       \
        cache->replace_param = dynamo_options.cache_##which##_replace;                   \
    } while (0);

static fcache_t *
fcache_cache_init(dcontext_t *dcontext, uint flags, bool initial_unit)
{
    fcache_t *cache = (fcache_t *)nonpersistent_heap_alloc(
        dcontext, sizeof(fcache_t) HEAPACCT(ACCT_MEM_MGT));
    cache->fifo = NULL;
    cache->size = 0;
    cache->is_trace = TEST(FRAG_IS_TRACE, flags);
    cache->is_shared = TEST(FRAG_SHARED, flags);
    cache->is_coarse = TEST(FRAG_COARSE_GRAIN, flags);
    DODEBUG({ cache->is_local = false; });
    cache->coarse_info = NULL;
    DODEBUG({ cache->consistent = true; });
    if (cache->is_shared) {
        ASSERT(dcontext == GLOBAL_DCONTEXT);
        if (TEST(FRAG_IS_TRACE, flags)) {
            DODEBUG({ cache->name = "Trace (shared)"; });
            SET_CACHE_PARAMS(cache, shared_trace);
        } else if (cache->is_coarse) {
            DODEBUG({ cache->name = "Coarse basic block (shared)"; });
            SET_CACHE_PARAMS(cache, coarse_bb);
        } else {
            DODEBUG({ cache->name = "Basic block (shared)"; });
            SET_CACHE_PARAMS(cache, shared_bb);
        }
    } else {
        ASSERT(dcontext != GLOBAL_DCONTEXT);
        if (TEST(FRAG_IS_TRACE, flags)) {
            DODEBUG({ cache->name = "Trace (private)"; });
            SET_CACHE_PARAMS(cache, trace);
        } else {
            DODEBUG({ cache->name = "Basic block (private)"; });
            SET_CACHE_PARAMS(cache, bb);
        }
    }
#ifdef DISALLOW_CACHE_RESIZING
    /* cannot handle resizing of cache, separate units only */
    cache->init_unit_size = cache->max_unit_size;
#endif
    if (cache->is_shared)
        ASSIGN_INIT_LOCK_FREE(cache->lock, shared_cache_lock);
    if (initial_unit) {
        PROTECT_CACHE(cache, lock);
        cache->units = fcache_create_unit(dcontext, cache, NULL, cache->init_unit_size);
        PROTECT_CACHE(cache, unlock);
    } else
        cache->units = NULL;
    cache->num_regenerated = 0;
    cache->num_replaced = 0;
    cache->wset_check = 0;
    cache->record_wset = false;
    if (cache->is_shared) { /* else won't use free list */
        memset(cache->free_list, 0, sizeof(cache->free_list));
        DODEBUG({
            memset(cache->free_stats_freed, 0, sizeof(cache->free_stats_freed));
            memset(cache->free_stats_reused, 0, sizeof(cache->free_stats_reused));
            memset(cache->free_stats_coalesced, 0, sizeof(cache->free_stats_coalesced));
            memset(cache->free_stats_charge, 0, sizeof(cache->free_stats_charge));
            memset(cache->free_stats_split, 0, sizeof(cache->free_stats_split));
            memset(cache->request_size_histogram, 0,
                   sizeof(cache->request_size_histogram));
            memset(cache->free_size_histogram, 0, sizeof(cache->free_size_histogram));
        });
    }
    return cache;
}

/* assumption: only called on thread exit path.
 * If !free_units, we do not de-allocate or move the units to the dead list,
 * but we still remove from the live list.
 */
static void
fcache_cache_free(dcontext_t *dcontext, fcache_t *cache, bool free_units)
{
    fcache_unit_t *u, *next_u;
    dcontext_t *alloc_dc = ALLOC_DC(dcontext, cache);
    DEBUG_DECLARE(size_t cache_size = cache->size;)
    DEBUG_DECLARE(size_t size_check = 0;)

    if (USE_FIFO_FOR_CACHE(cache)) {
        fragment_t *f, *nextf;
        ASSERT(cache->consistent);
        for (f = cache->fifo; f != NULL; f = nextf) {
            /* fragment exit may have already happened, but we won't deref
             * freed memory here since fragments will have been removed from FIFO
             */
            nextf = FIFO_NEXT(f);
            if (FRAG_EMPTY(f)) {
                nonpersistent_heap_free(alloc_dc, f,
                                        sizeof(empty_slot_t) HEAPACCT(ACCT_FCACHE_EMPTY));
            }
        }
        cache->fifo = NULL;
    }
    ASSERT(cache->fifo == NULL);

    u = cache->units;
    while (u != NULL) {
        DODEBUG({ size_check += u->size; });
        next_u = u->next_local;
        fcache_free_unit(dcontext, u, free_units);
        u = next_u;
    }
    /* we must use pre-cached cache_size since fcache_free_unit decrements it */
    ASSERT(size_check == cache_size);
    ASSERT(cache->size == 0);

    if (cache->is_shared)
        DELETE_LOCK(cache->lock);

    nonpersistent_heap_free(alloc_dc, cache, sizeof(fcache_t) HEAPACCT(ACCT_MEM_MGT));
}

#ifdef DEBUG
void
fcache_free_list_consistency(dcontext_t *dcontext, fcache_t *cache, int bucket)
{
    uint live = 0;
    uint charge = 0;
    uint waste = 0;
    free_list_header_t *header = cache->free_list[bucket];
    uint prev_size = 0;
    fcache_unit_t *unit;

    LOG(GLOBAL, LOG_CACHE, 3, "fcache_free_list_consistency %s bucket[%2d], %3d bytes\n",
        cache->name, bucket, FREE_LIST_SIZES[bucket]);
    /* walk list, and verify counters */
    while (header != NULL) {
        cache_pc start_pc = (cache_pc)header;
        uint size = header->size;

        ASSERT(size >= FREE_LIST_SIZES[bucket] && size <= MAX_FREE_ENTRY_SIZE &&
               (bucket == FREE_LIST_SIZES_NUM - 1 || size < FREE_LIST_SIZES[bucket + 1]));

        /* FIXME: should ASSERT entries in a bucket are all sorted
         * properly, when we start keeping them in order */
        ASSERT(prev_size < size || true /* not sorted yet */);
        prev_size = size;
        ASSERT(header->next == NULL || header->next->prev == header);
        ASSERT(header->prev == NULL || header->prev->next == header);
        ASSERT(FRAG_IS_FREE_LIST((fragment_t *)header));

        unit = fcache_lookup_unit(start_pc);
        if (unit->cur_pc > start_pc + header->size) {
            fragment_t *subseq = FRAG_NEXT_SLOT(start_pc, header->size);
            /* should NOT be followed by a free list entry; should instead be
             * followed by a marked fragment_t.
             */
            ASSERT((!FRAG_IS_FREE_LIST(subseq) &&
                    TEST(FRAG_FOLLOWS_FREE_ENTRY, subseq->flags)) ||
                   /* ok to have subsequent free entry if unable to coalesce due
                    * to ushort size limits */
                   size + FRAG_NEXT_FREE(start_pc, header->size)->size >
                       MAX_FREE_ENTRY_SIZE);
        }
        /* Invariant: no free list entry at append point */
        ASSERT(unit->full || unit->cur_pc != start_pc + header->size);

        header = header->next;

        /* maximum waste if this entry is used.  The scheme before
         * case 7318 was really wasting memory, for comparison here */
        LOG(GLOBAL, LOG_CACHE, 4, "\t  @" PFX ": %3d bytes, %3d max waste\n", start_pc,
            size, size - FREE_LIST_SIZES[bucket]);
        live++;
        charge += size;
        waste += size - FREE_LIST_SIZES[bucket];
    }

    ASSERT(live ==
           cache->free_stats_freed[bucket] -
               (cache->free_stats_reused[bucket] + cache->free_stats_coalesced[bucket]));
    ASSERT(charge == cache->free_stats_charge[bucket]);
    ASSERT(waste >= (
                        /* waste estimate =
                           charged bytes - live entries * bucket _minimal_ size */
                        cache->free_stats_charge[bucket] -
                        (cache->free_stats_freed[bucket] -
                         (cache->free_stats_reused[bucket] +
                          cache->free_stats_coalesced[bucket])) *
                            FREE_LIST_SIZES[bucket]));
    LOG(GLOBAL, LOG_CACHE, 2, "\t#%2d %3d bytes == %d live, %8d charge, %8d waste\n",
        bucket, FREE_LIST_SIZES[bucket], live, charge, waste);
}

/* FIXME: put w/ periodic stats dumps and not only at end? */
static void
fcache_cache_stats(dcontext_t *dcontext, fcache_t *cache)
{
    fcache_unit_t *u;
    int i = 0;
    size_t capacity = 0;
    size_t used = 0;
    bool full = true;
    ASSERT_OWN_MUTEX(!dynamo_exited && cache->is_shared, &cache->lock);
    for (u = cache->units; u != NULL; u = u->next_local, i++) {
        capacity += u->size;
        used += u->cur_pc - u->start_pc;
        full &= u->full;
        LOG(THREAD, LOG_CACHE, 1,
            "\t%s unit %d @" PFX ": capacity %d KB, used %d KB, %s\n", cache->name, i,
            u->start_pc, u->size / 1024, (u->cur_pc - u->start_pc) / 1024,
            u->full ? "full" : "not full");
    }
    LOG(THREAD, LOG_CACHE, 1, "%s cache: capacity %d KB, used %d KB, %s\n", cache->name,
        capacity / 1024, used / 1024, full ? "full" : "not full");
    if (DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared) { /* using free list */
        int bucket;
        LOG(GLOBAL, LOG_CACHE, 1, "fcache %s free list stats:\n", cache->name);
        for (bucket = 0; bucket < FREE_LIST_SIZES_NUM; bucket++) {
            LOG(GLOBAL, LOG_ALL, 1,
                "\t#%2d %3d bytes : %7d free, %7d reuse, %5d coalesce, %5d split\n"
                "\t    %3d bytes : %5d live, %8d charge, %8d waste\n",
                bucket, FREE_LIST_SIZES[bucket], cache->free_stats_freed[bucket],
                cache->free_stats_reused[bucket], cache->free_stats_coalesced[bucket],
                cache->free_stats_split[bucket], FREE_LIST_SIZES[bucket],
                cache->free_stats_freed[bucket] -
                    (cache->free_stats_reused[bucket] +
                     cache->free_stats_coalesced[bucket]),
                cache->free_stats_charge[bucket],
                /* waste = charged bytes - live entries * bucket _minimal_ size */
                cache->free_stats_charge[bucket] -
                    (cache->free_stats_freed[bucket] -
                     (cache->free_stats_reused[bucket] +
                      cache->free_stats_coalesced[bucket])) *
                        FREE_LIST_SIZES[bucket]);
        }

        DOLOG(1, LOG_CACHE, {
            /* FIXME: add in all debug runs, if not too slow */
            for (bucket = 0; bucket < FREE_LIST_SIZES_NUM; bucket++) {
                fcache_free_list_consistency(dcontext, cache, bucket);
            }
        });

        LOG(GLOBAL, LOG_ALL, 1, "fcache %s requests and frees histogram:\n", cache->name);
        for (bucket = 0; bucket < sizeof(cache->request_size_histogram) /
                 sizeof(cache->request_size_histogram[0]);
             bucket++) {
            if (cache->request_size_histogram[bucket] != 0 ||
                cache->free_size_histogram[bucket] != 0) {
                LOG(GLOBAL, LOG_ALL, 1, "\t# %3d bytes == %8d requests   %8d freed\n",
                    bucket * HISTOGRAM_GRANULARITY, cache->request_size_histogram[bucket],
                    cache->free_size_histogram[bucket]);
            }
        }
    }
}

static inline int
get_histogram_bucket(int size)
{
    uint histogram_bucket = size / HISTOGRAM_GRANULARITY;
    if (histogram_bucket >= HISTOGRAM_MAX_SIZE / HISTOGRAM_GRANULARITY)
        histogram_bucket = (HISTOGRAM_MAX_SIZE / HISTOGRAM_GRANULARITY) - 1;
    return histogram_bucket;
}

#endif /* DEBUG */

static void
fcache_shift_fragments(dcontext_t *dcontext, fcache_unit_t *unit, ssize_t shift,
                       cache_pc start, cache_pc end, size_t old_size)
{
    fragment_t *f;
    cache_pc pc, new_pc;

    ASSERT_OWN_MUTEX(unit->cache->is_shared, &unit->cache->lock);

    /* free list scheme can't be walked expecting FIFO headers */
    ASSERT(!DYNAMO_OPTION(cache_shared_free_list) || !unit->cache->is_shared);

    /* would need to re-relativize fcache exit prefix for coarse */
    ASSERT(!unit->cache->is_coarse);

    DODEBUG({ unit->cache->consistent = false; });
    LOG(THREAD, LOG_CACHE, 2, "fcache_shift_fragments: first pass\n");
    /* walk the physical cache and shift each fragment
     * fine to walk the old memory, we just need the fragment_t* pointers
     */
    pc = unit->start_pc;
    LOG(THREAD, LOG_CACHE, 2, "  unit " PFX "-" PFX " [-" PFX "]\n", pc, unit->end_pc,
        unit->reserved_end_pc);
    while (pc < unit->cur_pc) {
        f = *((fragment_t **)pc);
        ASSERT(f != NULL);
        ASSERT(FIFO_UNIT(f) == unit); /* sanity check */
        if (FRAG_EMPTY(f))
            FRAG_START_ASSIGN(f, FRAG_START(f) + shift);
        else {
            LOG(THREAD, LOG_CACHE, 5, "\treading " PFX " -> " PFX " = F%d\n", pc, f,
                f->id);
            fragment_shift_fcache_pointers(dcontext, f, shift, start, end, old_size);
        }
        /* now that f->start_pc is updated, update the backpointer */
        new_pc = FRAG_HDR_START(f);
        LOG(THREAD, LOG_CACHE, 4, "resize: writing " PFX " to " PFX "\n", f, new_pc);
        *((fragment_t **)vmcode_get_writable_addr(new_pc)) = f;
        /* move to contiguously-next fragment_t in cache */
        pc += FRAG_SIZE(f);
    }

    DOLOG(2, LOG_FRAGMENT, {
        /* need to check for consistency all tables at this point */
        study_all_hashtables(dcontext);
    });

    LOG(THREAD, LOG_CACHE, 2, "fcache_shift_fragments: second pass\n");
    /* have to do a second pass to link them to each other */
    pc = unit->start_pc;
    LOG(THREAD, LOG_CACHE, 2, "  unit " PFX "-" PFX " [-" PFX "]\n", pc, unit->end_pc,
        unit->reserved_end_pc);
    while (pc < unit->cur_pc) {
        f = *((fragment_t **)pc);
        ASSERT(f != NULL);
        /* can't repeat the FIFO_UNIT(f)==unit check b/c we've already adjusted
         * f->start_pc, which is used to find the unit
         */
        if (!FRAG_EMPTY(f)) {
            LOG(THREAD, LOG_CACHE, 5, "\treading " PFX " -> " PFX " = F%d\n", pc, f,
                f->id);

            /* inter-cache links must be redone:
             * we have links from bb cache to trace cache, and sometimes links
             * the other direction, for example from a trace to a bb that
             * cannot be a trace head (e.g., is marked CANNOT_BE_TRACE).
             * simplest to re-link every fragment in the shifted cache.
             * N.B.: we do NOT need to re-link the outgoing, since
             * fragment_shift_fcache_pointers re-relativized all outgoing
             * ctis by the shifted amount.
             */
            if (TEST(FRAG_LINKED_INCOMING, f->flags)) {
                unlink_fragment_incoming(dcontext, f);
                link_fragment_incoming(dcontext, f, false /*not new*/);
            }
        }

        /* move to contiguously-next fragment_t in cache */
        pc += FRAG_SIZE(f);
    }
    DODEBUG({ unit->cache->consistent = true; });
}

static void
cache_extend_commitment(fcache_unit_t *unit, size_t commit_size)
{
    ASSERT(unit != NULL);
    ASSERT(ALIGNED(commit_size, DYNAMO_OPTION(cache_commit_increment)));
    heap_mmap_extend_commitment(unit->end_pc, commit_size, VMM_CACHE | VMM_REACHABLE);
    unit->end_pc += commit_size;
    unit->size += commit_size;
    unit->cache->size += commit_size;
    unit->full = false;
    STATS_FCACHE_ADD(unit->cache, capacity, commit_size);
    STATS_FCACHE_MAX(unit->cache, capacity_peak, capacity);
    RSTATS_ADD_PEAK(fcache_combined_capacity, commit_size);
    ASSERT(unit->end_pc <= unit->reserved_end_pc);
    ASSERT(unit->size <= UNIT_RESERVED_SIZE(unit));
}

/* FIXME case 8617: now that we have cache commit-on-demand we should
 * make the private-configuration caches larger.  We could even get
 * rid of the fcache shifting.
 */
/* i#696: We're not getting rid of fcache shifting yet, but it is incompatible
 * with labels-as-values since we can't patch those absolute addresses.
 */
static void
fcache_increase_size(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                     uint slot_size)
{
    bool reallocated = false;
    cache_pc new_memory = NULL;
    ssize_t shift;
    size_t new_size = unit->size;
    size_t commit_size;
    /* i#696: Incompatible with clients that use labels-as-values. */
    ASSERT(!dr_bb_hook_exists() && !dr_trace_hook_exists());
    /* we shouldn't come here if we have reservation room */
    ASSERT(unit->reserved_end_pc == unit->end_pc);
    if (new_size * 4 <= cache->max_quadrupled_unit_size)
        new_size *= 4;
    else
        new_size *= 2;
    if (new_size < slot_size * MAX_SINGLE_MULTIPLE)
        new_size = ALIGN_FORWARD(slot_size * MAX_SINGLE_MULTIPLE, PAGE_SIZE);
    /* unit limit */
    if (new_size > cache->max_unit_size)
        new_size = cache->max_unit_size;
    /* total cache limit */
    if (cache->max_size != 0 && cache->size - unit->size + new_size > cache->max_size) {
        new_size = cache->max_size - cache->size + unit->size;
    }
    commit_size = new_size; /* should be re-set below, this makes compiler happy */
    /* FIXME: shouldn't this routine return whether it
     * allocated enough space for slot_size?
     */
    ASSERT(unit->size + slot_size <= new_size);
    LOG(THREAD, LOG_CACHE, 2, "Increasing %s unit size from %d KB to %d KB\n",
        cache->name, unit->size / 1024, new_size / 1024);
#ifdef DISALLOW_CACHE_RESIZING
    SYSLOG_INTERNAL_ERROR("This build cannot handle cache resizing");
    ASSERT_NOT_REACHED();
#endif
    ASSERT(CACHE_PROTECTED(cache));

    /* take from dead list if possible */
    if (allunits->dead != NULL) {
        fcache_unit_t *u, *prev_u;
        d_r_mutex_lock(&allunits_lock);
        u = allunits->dead;
        prev_u = NULL;
        while (u != NULL) {
            if (UNIT_RESERVED_SIZE(u) >= new_size) {
                fcache_thread_units_t *tu =
                    (fcache_thread_units_t *)dcontext->fcache_field;
                /* remove from dead list */
                if (prev_u == NULL)
                    allunits->dead = u->next_global;
                else
                    prev_u->next_global = u->next_global;
                allunits->num_dead--;
                RSTATS_DEC(fcache_num_free);
                STATS_SUB(fcache_free_capacity, u->size);
                LOG(THREAD, LOG_CACHE, 1, "\tFound unit of size %d on dead list\n",
                    u->size / 1024);
                new_memory = u->start_pc;
                ASSERT_TRUNCATE(new_size, uint, UNIT_RESERVED_SIZE(u));
                new_size = UNIT_RESERVED_SIZE(u);
                u->cache = cache;
                /* Add to stats prior to extending commit as that will add the
                 * extension amount itself.
                 * Don't need to add to combined capacity: it includes free.
                 */
                STATS_FCACHE_ADD(cache, capacity, u->size);
                STATS_FCACHE_MAX(cache, capacity_peak, capacity);
                if (u->size < new_size) { /* case 8688: fill out to promised size */
                    size_t new_commit = ALIGN_FORWARD(
                        new_size - u->size, DYNAMO_OPTION(cache_commit_increment));
                    ASSERT(u->size + new_commit <= UNIT_RESERVED_SIZE(u));
                    cache_extend_commitment(u, new_commit);
                    /* we increase cache's size below so undo what
                     * cache_extend_commitment did */
                    u->cache->size -= new_commit;
                    ASSERT(u->size >= new_size);
                }
                commit_size = u->size;
                /* use unit's fcache_unit_t struct but u's mmap space */
                ASSERT(tu->pending_unmap_pc == NULL);
                tu->pending_unmap_pc = unit->start_pc;
                tu->pending_unmap_size = UNIT_RESERVED_SIZE(unit);
                STATS_FCACHE_SUB(cache, capacity, unit->size);
                RSTATS_SUB(fcache_combined_capacity, unit->size);
#ifdef WINDOWS_PC_SAMPLE
                if (u->profile != NULL) {
                    free_profile(u->profile);
                    u->profile = NULL;
                }
#endif
                /* need to replace u with unit: we remove from fcache_unit_areas
                 * here and re-add down below
                 */
                vmvector_remove(fcache_unit_areas, u->start_pc, u->reserved_end_pc);
                nonpersistent_heap_free(GLOBAL_DCONTEXT, u,
                                        sizeof(fcache_unit_t) HEAPACCT(ACCT_MEM_MGT));
                break;
            }
            prev_u = u;
            u = u->next_global;
        }
        d_r_mutex_unlock(&allunits_lock);
    }
    if (new_memory == NULL) {
        /* allocate new memory for unit */
        reallocated = true;
        commit_size = 0;
        while (commit_size < slot_size && unit->size + commit_size < new_size) {
            commit_size += DYNAMO_OPTION(cache_commit_increment);
        }
        /* FIXME: If not we have a problem -- this routine should return failure */
        ASSERT(commit_size >= slot_size);
        commit_size += unit->size;
        ASSERT(commit_size <= new_size);
        new_memory = (cache_pc)heap_mmap_reserve(
            new_size, commit_size, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
            VMM_CACHE | VMM_REACHABLE);
        STATS_FCACHE_SUB(cache, capacity, unit->size);
        STATS_FCACHE_ADD(cache, capacity, commit_size);
        STATS_FCACHE_MAX(cache, capacity_peak, capacity);
        RSTATS_SUB(fcache_combined_capacity, unit->size);
        RSTATS_ADD_PEAK(fcache_combined_capacity, commit_size);
        LOG(THREAD, LOG_HEAP, 3, "fcache_increase_size -> " PFX "\n", new_memory);
        ASSERT(new_memory != NULL);
        ASSERT(proc_is_cache_aligned((void *)new_memory));
    }

    /* while we can handle resizing any unit, we only expect to resize
     * the initial unit in a cache until it reaches the max unit size.
     */
    ASSERT(unit == cache->units && unit->next_local == NULL);

    /* copy old data over to new memory */
    memcpy(new_memory, unit->start_pc, unit->size);

    /* update pc-relative into-cache or out-of-cache pointers
     * also update stored addresses like start pc
     * assumption: all intra-cache links will still work!
     * they're all relative, we copied entire cache!
     */
    shift = (new_memory - unit->start_pc);
    /* make sure we don't screw up any alignment */
    ASSERT(ALIGNED(shift, proc_get_cache_line_size()));
    ASSERT(ALIGNED(shift, PAD_JMPS_ALIGNMENT));
    fcache_shift_fragments(dcontext, unit, shift, new_memory, new_memory + new_size,
                           unit->size);

    /* now change unit fields */
    if (reallocated) {
        /* de-allocate old memory -- not now, but next time we're in
         * fcache_add_fragment, b/c the current ilist for being-added fragment
         * may reference memory in old cache
         */
        fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
        ASSERT(tu->pending_unmap_pc == NULL);
        tu->pending_unmap_pc = unit->start_pc;
        tu->pending_unmap_size = UNIT_RESERVED_SIZE(unit);
    }

    /* whether newly allocated or taken from dead list, increase cache->size
     * by the difference between new size and old size
     */
    cache->size -= unit->size;
    cache->size += commit_size;
    unit->cur_pc += shift;
    unit->start_pc = new_memory;
    unit->size = commit_size;
    unit->end_pc = unit->start_pc + commit_size;
    unit->reserved_end_pc = unit->start_pc + new_size;
    vmvector_add(fcache_unit_areas, unit->start_pc, unit->reserved_end_pc, (void *)unit);
    unit->full = false; /* reset */

#ifdef WINDOWS_PC_SAMPLE
    {
        /* old unit was copied to start of enlarged unit, can copy old prof
         * buffer to start of new buffer and maintain correspondence
         */
        profile_t *old_prof = unit->profile;
        if (old_prof != NULL) {
            unit->profile = create_profile(unit->start_pc, unit->reserved_end_pc,
                                           dynamo_options.prof_pcs_fcache, NULL);
            stop_profile(old_prof);
            ASSERT(unit->profile->buffer_size >= old_prof->buffer_size);
            memcpy(unit->profile->buffer, old_prof->buffer, old_prof->buffer_size);
            free_profile(old_prof);
            start_profile(unit->profile);
        }
    }
#endif

    DOLOG(2, LOG_CACHE, { verify_fifo(dcontext, cache); });
    LOG(THREAD, LOG_CACHE, 1, "\tDone increasing unit size\n");
}

static void
fcache_thread_reset_init(dcontext_t *dcontext)
{
    /* nothing */
}

void
fcache_thread_init(dcontext_t *dcontext)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)heap_alloc(
        dcontext, sizeof(fcache_thread_units_t) HEAPACCT(ACCT_OTHER));
    dcontext->fcache_field = (void *)tu;
    /* don't build trace cache until we actually build a trace
     * this saves memory for both DYNAMO_OPTION(disable_traces) and for
     * idle threads that never do much
     */
    tu->trace = NULL;
    /* in fact, let's delay both, cost is single conditional in fcache_add_fragment,
     * once we have that conditional for traces it's no extra cost for bbs
     */
    tu->bb = NULL;
    tu->pending_unmap_pc = NULL;
    tu->pending_flush = false;

    fcache_thread_reset_init(dcontext);
}

/* see if a fragment with that tag has existed, ever, in any cache */
bool
fragment_lookup_deleted(dcontext_t *dcontext, app_pc tag)
{
    future_fragment_t *fut;
    if (SHARED_FRAGMENTS_ENABLED() && dcontext != GLOBAL_DCONTEXT) {
        fut = fragment_lookup_private_future(dcontext, tag);
        if (fut != NULL)
            return TEST(FRAG_WAS_DELETED, fut->flags);
        /* if no private, lookup shared */
    }
    fut = fragment_lookup_future(dcontext, tag);
    return (fut != NULL && TEST(FRAG_WAS_DELETED, fut->flags));
}

/* find a fragment that existed in the same type of cache */
static future_fragment_t *
fragment_lookup_cache_deleted(dcontext_t *dcontext, fcache_t *cache, app_pc tag)
{
    future_fragment_t *fut;
    if (!cache->is_shared) {
        /* only look for private futures, since we only care about whether this
         * cache needs to be resized, and thus only if we kicked tag out of
         * this cache, not whether we kicked it out of the shared cache
         */
        fut = fragment_lookup_private_future(dcontext, tag);
    } else
        fut = fragment_lookup_future(dcontext, tag);
    if (fut != NULL && TEST(FRAG_WAS_DELETED, fut->flags))
        return fut;
    else
        return NULL;
}

#ifdef DEBUG
/* this routine is separate from fcache_thread_exit because it needs to
 * be run before fragment_thread_exit, whereas the real fcache cleanup
 * needs to be done after fragment's cleanup
 */
void
fcache_thread_exit_stats(dcontext_t *dcontext)
{
    DOELOG(1, LOG_CACHE, {
        fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
        if (tu->bb != NULL)
            fcache_cache_stats(dcontext, tu->bb);
        if (tu->trace != NULL)
            fcache_cache_stats(dcontext, tu->trace);
    });
}
#endif

static void
fcache_thread_reset_free(dcontext_t *dcontext)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
    if (tu->pending_unmap_pc != NULL) {
        /* de-allocate old memory -- stats have already been taken care of */
        /* remove from interval data struct first to avoid races w/ it
         * being re-used and not showing up in in_fcache
         */
        vmvector_remove(fcache_unit_areas, tu->pending_unmap_pc,
                        tu->pending_unmap_pc + tu->pending_unmap_size);
        heap_munmap(tu->pending_unmap_pc, tu->pending_unmap_size,
                    VMM_CACHE | VMM_REACHABLE);
        tu->pending_unmap_pc = NULL;
    }
    if (tu->bb != NULL) {
        fcache_cache_free(dcontext, tu->bb, true);
        tu->bb = NULL;
    }
    if (tu->trace != NULL) {
        fcache_cache_free(dcontext, tu->trace, true);
        tu->trace = NULL;
    }
}

void
fcache_thread_exit(dcontext_t *dcontext)
{
    DEBUG_DECLARE(fcache_thread_units_t *tu =
                      (fcache_thread_units_t *)dcontext->fcache_field;)
    fcache_thread_reset_free(dcontext);
    DODEBUG({
        /* for non-debug we do fast exit path and don't free local heap */
        heap_free(dcontext, tu, sizeof(fcache_thread_units_t) HEAPACCT(ACCT_OTHER));
    });
}

#if defined(DEBUG) && defined(INTERNAL)
static void
print_fifo(dcontext_t *dcontext, fcache_t *cache)
{
    fragment_t *f = cache->fifo;
    ASSERT(USE_FIFO_FOR_CACHE(cache));
    ASSERT(CACHE_PROTECTED(cache));
    while (f != NULL) {
        /* caller sets loglevel */
        LOG(THREAD, LOG_CACHE, 1, "\tF%d " PFX " = @" PFX " size %d\n", FRAG_ID(f),
            FRAG_TAG(f), FRAG_HDR_START(f), FRAG_SIZE(f));
        f = FIFO_NEXT(f);
    }
}

static void
verify_fifo(dcontext_t *dcontext, fcache_t *cache)
{
    fragment_t *f = cache->fifo;
    fcache_unit_t *u;
    cache_pc pc;
    ASSERT(USE_FIFO_FOR_CACHE(cache));
    ASSERT(CACHE_PROTECTED(cache));
    while (f != NULL) {
        LOG(THREAD, LOG_CACHE, 6, "\t*" PFX " F%d " PFX " = @" PFX " size %d\n", f,
            FRAG_ID(f), FRAG_TAG(f), FRAG_HDR_START(f), FRAG_SIZE(f));
        /* check that header is intact */
        pc = FRAG_HDR_START(f);
        ASSERT(*((fragment_t **)pc) == f);
        /* check that no missing space */
        pc += FRAG_SIZE(f);
        u = FIFO_UNIT(f);
        /* free list scheme can't be walked expecting FIFO headers */
        if ((!DYNAMO_OPTION(cache_shared_free_list) || !cache->is_shared)) {
            if (pc < u->cur_pc)
                ASSERT(*((fragment_t **)pc) != NULL);
        }
        f = FIFO_NEXT(f);
    }
}
#endif

static void
fifo_append(fcache_t *cache, fragment_t *f)
{
    ASSERT(USE_FIFO(f));
    ASSERT(CACHE_PROTECTED(cache));
    /* start has prev to end, but end does NOT have next to start */
    FIFO_NEXT_ASSIGN(f, NULL);
    if (cache->fifo == NULL) {
        cache->fifo = f;
        FIFO_PREV_ASSIGN(f, f);
    } else {
        FIFO_PREV_ASSIGN(f, FIFO_PREV(cache->fifo));
        FIFO_NEXT_ASSIGN(FIFO_PREV(cache->fifo), f);
        FIFO_PREV_ASSIGN(cache->fifo, f);
    }
    FIFO_NEXT_ASSIGN(f, NULL);
    LOG(THREAD_GET, LOG_CACHE, 5, "fifo_append F%d @" PFX "\n", FRAG_ID(f),
        FRAG_HDR_START(f));
    DOLOG(6, LOG_CACHE, { print_fifo(get_thread_private_dcontext(), cache); });
}

static void
fifo_remove(dcontext_t *dcontext, fcache_t *cache, fragment_t *f)
{
    ASSERT(USE_FIFO(f));
    ASSERT(CACHE_PROTECTED(cache));
    ASSERT(cache->fifo != NULL);
    /* start has prev to end, but end does NOT have next to start */
    if (f == cache->fifo) {
        cache->fifo = FIFO_NEXT(f);
    } else {
        FIFO_NEXT_ASSIGN(FIFO_PREV(f), FIFO_NEXT(f));
    }
    if (FIFO_NEXT(f) == NULL) {
        if (cache->fifo != NULL)
            FIFO_PREV_ASSIGN(cache->fifo, FIFO_PREV(f));
    } else {
        FIFO_PREV_ASSIGN(FIFO_NEXT(f), FIFO_PREV(f));
    }
    LOG(THREAD, LOG_CACHE, 5, "fifo_remove F%d @" PFX "\n", FRAG_ID(f),
        FRAG_HDR_START(f));
    DOLOG(6, LOG_CACHE, { print_fifo(dcontext, cache); });
    if (FRAG_EMPTY(f)) {
        STATS_FCACHE_SUB(cache, empty, FRAG_SIZE(f));
        STATS_FCACHE_ADD(cache, used, FRAG_SIZE(f));
        nonpersistent_heap_free(ALLOC_DC(dcontext, cache), f,
                                sizeof(empty_slot_t) HEAPACCT(ACCT_FCACHE_EMPTY));
    }
}

static void
fifo_prepend_empty(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                   fragment_t *f, cache_pc start_pc, uint size)
{
    empty_slot_t *slot;
    ASSERT(CACHE_PROTECTED(cache));

    STATS_FCACHE_ADD(cache, empty, size);

    if (DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared && !cache->is_coarse) {
        ASSERT(USE_FREE_LIST_FOR_CACHE(cache));
        add_to_free_list(dcontext, cache, unit, f, start_pc, size);
        return;
    }
    /* FIXME: make cache_shared_free_list always on and remove the option
     * as there really is no alternative implemented -- we just waste the space.
     * FIXME case 8714: anything we can do for coarse-grain?
     */
    if (!USE_FIFO_FOR_CACHE(cache))
        return;

    /* don't make two entries for adjacent empties
     * for efficiency only check front of FIFO -- most common case anyway
     */
    if (cache->fifo != NULL && FRAG_EMPTY(cache->fifo)) {
        if (FRAG_HDR_START(cache->fifo) == start_pc + size) {
            LOG(THREAD, LOG_CACHE, 5, "prepend: just enlarging next empty\n");
            FRAG_START_ASSIGN(cache->fifo, start_pc + HEADER_SIZE_FROM_CACHE(cache));
            *((fragment_t **)vmcode_get_writable_addr(start_pc)) = cache->fifo;
            FRAG_SIZE_ASSIGN(cache->fifo, FRAG_SIZE(cache->fifo) + size);
            return;
        } else if (FRAG_HDR_START(cache->fifo) + FRAG_SIZE(cache->fifo) == start_pc) {
            LOG(THREAD, LOG_CACHE, 5, "prepend: just enlarging prev empty\n");
            FRAG_SIZE_ASSIGN(cache->fifo, FRAG_SIZE(cache->fifo) + size);
            return;
        }
    }

    slot = (empty_slot_t *)nonpersistent_heap_alloc(
        ALLOC_DC(dcontext, cache), sizeof(empty_slot_t) HEAPACCT(ACCT_FCACHE_EMPTY));
    slot->flags = FRAG_FAKE | FRAG_IS_EMPTY_SLOT;
    LOG(THREAD, LOG_CACHE, 5, "prepend: writing " PFX " to " PFX "\n", slot, start_pc);
    *((empty_slot_t **)vmcode_get_writable_addr(start_pc)) = slot;
    slot->start_pc = start_pc + HEADER_SIZE_FROM_CACHE(cache);
    slot->fcache_size = size;
    /* stick on front */
    slot->next_fcache = cache->fifo;
    if (cache->fifo == NULL)
        slot->prev_fcache = (fragment_t *)slot;
    else {
        slot->prev_fcache = FIFO_PREV(cache->fifo);
        FIFO_PREV_ASSIGN(cache->fifo, (fragment_t *)slot);
    }
    /* start has prev to end, but end does NOT have next to start */
    cache->fifo = (fragment_t *)slot;
    LOG(THREAD, LOG_CACHE, 5, "fifo_prepend_empty F-1 @" PFX "\n", start_pc);
    DOLOG(6, LOG_CACHE, { print_fifo(dcontext, cache); });
}

/* returns whether the cache should be allowed to grow */
static bool
check_regen_replace_ratio(dcontext_t *dcontext, fcache_t *cache, uint add_size)
{
    if (cache->max_size != 0 && cache->size + add_size > cache->max_size) {
        /* if at max size, avoid regen/replace checks */
        LOG(THREAD, LOG_CACHE, 4, "at max size 0x%x\n", cache->max_size);
        return false;
    } else if (!cache->finite_cache || cache->replace_param == 0) {
        /* always upgrade -- adaptive working set is disabled */
        LOG(THREAD, LOG_CACHE, 4, "upgrading since fcache_replace==0\n");
        return true;
    } else if (cache->regen_param == 0) {
        /* never upgrade due to regen ratio */
        LOG(THREAD, LOG_CACHE, 4, "will never upgrade since fcache_regen==0\n");
        return false;
    } else if (cache->wset_check > 0) {
        /* wset_check is only used for fifo caches */
        ASSERT(USE_FIFO_FOR_CACHE(cache));
        cache->wset_check--;
        LOG(THREAD, LOG_CACHE, 4, "dec wset_check -> %d\n", cache->wset_check);
        return false;
    } else if (cache->size < cache->free_upgrade_size) {
        /* free upgrade, but set check for next time */
        if (USE_FIFO_FOR_CACHE(cache)) {
            ASSERT(cache->wset_check == 0);
            cache->wset_check = cache->replace_param;
        } else {
            ASSERT(!cache->is_coarse); /* no individual fragment support */
            /* if a new unit would put us over the free upgrade point,
             * start keeping track of regen stats
             */
            if (cache->size + cache->max_unit_size >= cache->free_upgrade_size
                /* could come here after having already set this if flush
                 * a larger unit than last new unit and drop back below threshold
                 */
                && !cache->record_wset) {
                cache->record_wset = true;
            }
        }
        LOG(THREAD, LOG_CACHE, 3, "Free upgrade, no resize check\n");
        return true;
    } else {
        if (USE_FIFO_FOR_CACHE(cache)) {
            /* wait cache->replace_param frags before checking again,
             * to avoid too many checks when regen<<replace
             */
            cache->wset_check = cache->replace_param;
        } else {
            ASSERT(!cache->is_coarse); /* no individual fragment support */
            if (!cache->record_wset) {
                /* now we are big enough that we need to keep track, though
                 * ideally we should hit this prior to the free upgrade point,
                 * as otherwise this is a 2nd free resize, but might not if
                 * create a new unit larger than max unit size.
                 */
                cache->record_wset = true;
                return true;
            }
        }
        /* FIXME: for shared w/ replace==100 perhaps remove this if */
        if (cache->num_replaced >= cache->replace_param &&
            cache->num_regenerated >= cache->regen_param) {
            /* minimum regen/replaced ratio, compute w/o using floating point ops
             * and avoiding overflow (unless replace overflows before regen
             * hits regen_param, which is very unlikely and who cares if it does)
             */
            /* this loop guaranteed to terminate b/c we check for 0 above */
            ASSERT(cache->replace_param > 0 && cache->regen_param > 0);
            while (cache->num_replaced >= cache->replace_param &&
                   cache->num_regenerated >= cache->regen_param) {
                cache->num_replaced -= cache->replace_param;
                cache->num_regenerated -= cache->regen_param;
            }
            LOG(THREAD, LOG_CACHE, 3,
                "Resize check: for %s unit: %d regenerated / %d replaced\n", cache->name,
                cache->num_regenerated, cache->num_replaced);
            if (cache->num_regenerated >= cache->regen_param) {
                LOG(THREAD, LOG_CACHE, 1,
                    "%s unit reached ratio with %d regenerated / %d replaced\n",
                    cache->name, cache->num_regenerated, cache->num_replaced);
                return true;
            }
        }
        LOG(THREAD, LOG_CACHE, 4, "No resize allowed yet\n");
        return false;
    }
    return true;
}

/* Adds size to the end of the non-empty unit unit
 * If a small area is eaten and added to size, returns that amount
 */
static size_t
extend_unit_end(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit, size_t size,
                bool rest_empty)
{
    size_t left, extra = 0;
    ASSERT(CACHE_PROTECTED(cache));
    unit->cur_pc += size;
    STATS_FCACHE_ADD(cache, claimed, size);
    STATS_ADD(fcache_combined_claimed, size);
    left = unit->end_pc - unit->cur_pc;
    ASSERT(unit->end_pc >= unit->cur_pc);
    if (left < MIN_UNIT_END_HOLE(cache)) {
        LOG(THREAD_GET, LOG_CACHE, 3, "\tunit is now full\n");
        unit->full = true;
        if (left > 0) {
            /* eat up too-small area at end */
            extra = left;
            unit->cur_pc += extra;
            STATS_FCACHE_ADD(cache, claimed, extra);
            STATS_ADD(fcache_combined_claimed, extra);
        }
    } else if (rest_empty) {
        /* make entire rest of unit into an empty slot */
        ASSERT(CHECK_TRUNCATE_TYPE_uint(left));
        fifo_prepend_empty(dcontext, cache, unit, NULL, unit->cur_pc, (uint)left);
        unit->cur_pc += left;
        STATS_FCACHE_ADD(cache, claimed, left);
        STATS_ADD(fcache_combined_claimed, left);
        unit->full = true;
    }
    LOG(THREAD, LOG_CACHE, 5, "\t\textend_unit_end: %d + %d / %d => cur_pc = " PFX "\n",
        size, extra, left, unit->cur_pc);
    /* FIXME: if extended b/c need new unit (size==0), extra is empty space, but
     * we cannot add it to stats b/c will never be removed!
     */
    STATS_FCACHE_ADD(cache, used, (size + extra));
    STATS_FCACHE_MAX(cache, peak, used);
    return extra;
}

/* Returns whether was able to either resize unit or create a new unit.
 * For non-FIFO caches this routine cannot fail and must suspend the world
 * and reset if necessary.
 */
static bool
try_for_more_space(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                   uint slot_size)
{
    size_t commit_size = DYNAMO_OPTION(cache_commit_increment);
    ASSERT(CACHE_PROTECTED(cache));

    if (unit->end_pc < unit->reserved_end_pc &&
        !POINTER_OVERFLOW_ON_ADD(unit->cur_pc, slot_size) &&
        /* simpler to just not support taking very last page in address space */
        !POINTER_OVERFLOW_ON_ADD(unit->end_pc, commit_size)) {
        /* extend commitment if have more reserved */
        while (unit->cur_pc + slot_size > unit->end_pc + commit_size)
            commit_size *= 2;
        if (unit->end_pc + commit_size > unit->reserved_end_pc) {
            commit_size = unit->reserved_end_pc - unit->end_pc;
        }
        cache_extend_commitment(unit, commit_size);
        if (unit->cur_pc + slot_size > unit->end_pc) {
            /* must be a huge trace or something
             * still worth committing, we'll make an empty here
             */
            extend_unit_end(dcontext, cache, unit, 0, true);
            /* continue below and try to make more space */
        } else
            return true;
    }

    /* see if we have room to expand according to user-set maximum */
    if (cache->max_size == 0 || cache->size + slot_size <= cache->max_size) {
        LOG(THREAD, LOG_CACHE, 1, "max size = %d, cur size = %d\n",
            cache->max_size / 1024, cache->size / 1024);
        /* At larger sizes better to create separate units to avoid expensive
         * re-linking when resize.
         * i#696: Don't try to resize fcache units when clients are present.
         * They may use labels to insert absolute fragment PCs.
         */
        if (unit->size >= cache->max_unit_size || dr_bb_hook_exists() ||
            dr_trace_hook_exists()) {
            fcache_unit_t *newunit;
            size_t newsize;
            ASSERT(!USE_FIFO_FOR_CACHE(cache) || cache->fifo != NULL ||
                   /* i#1129: we can get here for initial 4KB unit whose initial
                    * fragment is >4KB!  We'll have set wset_check though.
                    */
                   cache->wset_check > 0); /* shouldn't be empty! */
            /* fill out to end first -- turn remaining room into empty slot */
            extend_unit_end(dcontext, cache, unit, 0, true);
            ASSERT(unit->full);

            /* before create a new unit, see if we should flush an old one */
            if (!USE_FIFO_FOR_CACHE(cache) && cache->finite_cache && !cache->is_coarse) {
                /* wset algorithm for shared caches: when request a new unit,
                 * must grant the request, but if regen/replace ratio does not
                 * exceed target, must flush an old unit.
                 */
                if (!check_regen_replace_ratio(dcontext, cache,
                                               0 /*not adding a fragment*/)) {
                    /* flush the oldest unit, at the end of the list */
                    fcache_thread_units_t *tu =
                        (fcache_thread_units_t *)dcontext->fcache_field;
                    fcache_unit_t *oldest = cache->units, *prev = NULL;
                    ASSERT(oldest != NULL);

                    /* another place where a prev_local would be nice */
                    for (; oldest->next_local != NULL;
                         prev = oldest, oldest = oldest->next_local)
                        ; /* nothing */

                    /* Indicate unit is still live even though off live list.
                     * Flag will be cleared once really flushed in
                     * fcache_flush_pending_units()
                     */
                    DODEBUG({ oldest->pending_flush = true; });

                    LOG(THREAD, LOG_CACHE, 2,
                        "marking unit " PFX "-" PFX " for flushing\n", oldest->start_pc,
                        oldest->end_pc);

                    /* move to pending-flush list and set trigger */
                    if (prev == NULL)
                        cache->units = oldest->next_local;
                    else
                        prev->next_local = oldest->next_local;
                    /* clear local just in case, should be no downstream use */
                    if (unit == oldest)
                        unit = NULL;

                    d_r_mutex_lock(&unit_flush_lock);
                    oldest->next_local = allunits->units_to_flush;
                    allunits->units_to_flush = oldest;
                    STATS_ADD_PEAK(cache_units_toflush, 1);
                    /* FIXME case 8743: we should call remove_unit_from_cache() here,
                     * but we need the cache field for chain_fragments_for_flush() --
                     * so we assume for now that there are no deletable caches that
                     * don't use fifos yet are finite, and let
                     * append_units_to_free_list() remove from the cache later on.
                     * This does mean that cache->size is too big from now until
                     * then, so we don't really support hardcoded cache sizes.
                     */
                    d_r_mutex_unlock(&unit_flush_lock);

                    tu->pending_flush = true;
                    STATS_INC(cache_units_wset_flushed);
                } else
                    STATS_INC(cache_units_wset_allowed);
            }

            /* now make a new unit
             * if new frag is large, make unit large as well
             */
            newsize = cache->max_unit_size;
            if (newsize < slot_size * MAX_SINGLE_MULTIPLE)
                newsize = ALIGN_FORWARD(slot_size * MAX_SINGLE_MULTIPLE, PAGE_SIZE);
            /* final adjustment: make sure don't go over max */
            if (cache->max_size > 0 && cache->size + newsize > cache->max_size)
                newsize = cache->max_size - cache->size;
            newunit = fcache_create_unit(dcontext, cache, NULL, newsize);
            LOG(THREAD, LOG_CACHE, 1, "Creating a new %s unit of %d KB @" PFX "\n",
                cache->name, newunit->size / 1024, newunit->start_pc);
            newunit->next_local = cache->units;
            cache->units = newunit;
        } else {
            ASSERT(!cache->is_coarse); /* no individual support so harder to resize */
            LOG(THREAD, LOG_CACHE, 1, "Increasing size of %s unit of %d KB @" PFX "\n",
                cache->name, unit->size / 1024, unit->start_pc);
            fcache_increase_size(dcontext, cache, unit, slot_size);
            LOG(THREAD, LOG_CACHE, 1, "\tnow %d KB @" PFX "\n", unit->size / 1024,
                unit->start_pc);
        }
        /* reset counters, but not deleted table */
        cache->num_replaced = 0;
        cache->num_regenerated = 0;
        DOLOG(2, LOG_CACHE, { fcache_cache_stats(dcontext, cache); });
        return true;
    } else {
        if (!USE_FIFO_FOR_CACHE(cache)) {
            /* options check up front shouldn't allow us to get here */
            ASSERT_NOT_REACHED();
            /* Case 8203: we need a new reset type that doesn't free anything,
             * and aborts traces only of other threads (not this one, as this
             * could be a trace we're emitting now).  Then we could free all
             * fragments in a unit here.
             * In order to do the reset we'd first need to release cache->lock,
             * if !cache->is_trace release the bb_building_lock, and enter_nolinking().
             * Note that for bb cache we could instead do a full reset and then
             * transfer_to_dispatch() but in debug builds we won't
             * free locals in prior stack frames: the fragment_t, the
             * instrlist_t, etc.  For trace cache doing that would be a bigger
             * step backward and take longer to get back here.
             */
        }
        /* tell user if fragment bigger than max size
         * FIXME: but if trace cache has small max size, should just
         * not build traces that big!
         */
        if (cache->max_size > 0 && slot_size > (int)cache->max_size) {
#ifdef INTERNAL
            const char *name = "";
            DODEBUG(name = cache->name;);
#endif
            USAGE_ERROR("single %s fragment (%d bytes) > max cache size (%d bytes)", name,
                        slot_size, cache->max_size);
        }
        return false;
    }
}

static void
place_fragment(dcontext_t *dcontext, fragment_t *f, fcache_unit_t *unit,
               cache_pc header_pc)
{
    fcache_t *cache = unit->cache;
    ASSERT_OWN_MUTEX(unit->cache->is_shared, &unit->cache->lock);
    DOLOG(3, LOG_CACHE,
          { /* only to reduce perf hit */
            fragment_t wrapper;
            /* cannot call fragment_pclookup as it will grab the fcache lock */
            ASSERT(fragment_pclookup_by_htable(dcontext, header_pc + HEADER_SIZE(f),
                                               &wrapper) == NULL);
          });
    if (HEADER_SIZE(f) > 0) {
        /* add header */
        LOG(THREAD, LOG_CACHE, 5, "place: writing " PFX " to " PFX "\n", f, header_pc);
        *((fragment_t **)vmcode_get_writable_addr(header_pc)) = f;
    }
    /* we assume alignment padding was added at end of prev fragment, so this
     * guy needs no padding at start
     */
    FRAG_START_ASSIGN(f, header_pc + HEADER_SIZE(f));
    ASSERT(ALIGNED(FRAG_HDR_START(f), SLOT_ALIGNMENT(cache)));
    STATS_FCACHE_ADD(cache, headers, HEADER_SIZE(f));
    STATS_FCACHE_ADD(cache, align, f->fcache_extra - (stats_int_t)HEADER_SIZE(f));

    /* for shared caches we must track regen/replace on every placement */
    if (cache->record_wset) {
        /* FIXME: how is this supposed to work for traces where a bb
         * may have replaced the future?  xref case 7151, though that
         * should be a problem for private as well...
         */
        future_fragment_t *fut = fragment_lookup_cache_deleted(dcontext, cache, f->tag);
        ASSERT(!USE_FIFO_FOR_CACHE(cache));
        ASSERT(!cache->is_coarse);
        cache->num_replaced++; /* simply number created past record_wset point */
        if (fut != NULL) {
            cache->num_regenerated++;
            STATS_INC(num_fragments_regenerated);
            SHARED_FLAGS_RECURSIVE_LOCK(fut->flags, acquire, change_linking_lock);
            fut->flags &= ~FRAG_WAS_DELETED;
            SHARED_FLAGS_RECURSIVE_LOCK(fut->flags, release, change_linking_lock);
        }
        LOG(THREAD, LOG_CACHE, 4, "For %s unit: %d regenerated / %d replaced\n",
            cache->name, cache->num_regenerated, cache->num_replaced);
    }
}

#ifdef DEBUG
static void
removed_fragment_stats(dcontext_t *dcontext, fcache_t *cache, fragment_t *f)
{
    int prefixes = fragment_prefix_size(f->flags);
    linkstub_t *l = FRAGMENT_EXIT_STUBS(f);
    int stubs = 0;
    uint selfmod = 0;
    /* N.B.: we cannot call EXIT_STUB_PC() here as with the -detect_dangling_fcache
     * option the fragment will be obliterated at this point and we will not
     * be able to locate the stub pc for an indirect exit.
     * Instead we simply calculate what the stub sizes should be.
     */
    for (; l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        int sz;
        if (!EXIT_HAS_LOCAL_STUB(l->flags, f->flags))
            continue; /* it's kept elsewhere */
        sz = linkstub_size(dcontext, f, l);
        if (LINKSTUB_INDIRECT(l->flags))
            STATS_FCACHE_ADD(cache, indirect_stubs, -sz);
        else {
            ASSERT(LINKSTUB_DIRECT(l->flags));
            STATS_FCACHE_ADD(cache, direct_stubs, -sz);
        }
        stubs += sz;
    }
    if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
        /* we cannot go and re-decode the app bb now, since it may have been
         * changed (selfmod doesn't make page RO!), so we use a stored size
         * that's there just for stats
         */
        selfmod = FRAGMENT_SELFMOD_COPY_SIZE(f);
        STATS_FCACHE_SUB(cache, selfmod_copy, selfmod);
    }
    STATS_FCACHE_SUB(cache, bodies, (f->size - (prefixes + stubs + selfmod)));
    STATS_FCACHE_SUB(cache, prefixes, prefixes);
    STATS_FCACHE_SUB(cache, headers, HEADER_SIZE(f));
    STATS_FCACHE_SUB(cache, align, (f->fcache_extra - (stats_int_t)HEADER_SIZE(f)));
}
#endif /* DEBUG */

static void
force_fragment_from_cache(dcontext_t *dcontext, fcache_t *cache, fragment_t *victim)
{
    bool empty = FRAG_EMPTY(victim); /* fifo_remove will free empty slot */
    ASSERT(CACHE_PROTECTED(cache));
    if (USE_FIFO(victim))
        fifo_remove(dcontext, cache, victim);
    if (!empty) {
        /* don't need to add deleted -- that's done by link.c for us,
         * when it makes a future fragment it uses the FRAG_WAS_DELETED flag
         */
        if (cache->finite_cache)
            cache->num_replaced++;
        DOSTATS({ removed_fragment_stats(dcontext, cache, victim); });
        STATS_INC(num_fragments_replaced);
        fragment_delete(dcontext, victim, FRAGDEL_NO_FCACHE);
    }
}

static bool
replace_fragments(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                  fragment_t *f, fragment_t *fifo, uint slot_size)
{
    fragment_t *victim;
    uint slot_so_far;
    cache_pc pc, header_pc;

    ASSERT(CACHE_PROTECTED(cache));
    /* free list scheme can't be walked expecting FIFO headers */
    ASSERT(!DYNAMO_OPTION(cache_shared_free_list) || !cache->is_shared);
    ASSERT(USE_FIFO_FOR_CACHE(cache));

    DODEBUG({ cache->consistent = false; });
    /* first walk: make sure this is possible (look for un-deletable frags) */
    slot_so_far = 0;
    pc = FRAG_HDR_START(fifo);
    victim = fifo;
    while (true) {
        if (TEST(FRAG_CANNOT_DELETE, victim->flags)) {
            DODEBUG({ cache->consistent = true; });
            return false;
        }
        slot_so_far += FRAG_SIZE(victim);
        if (slot_so_far >= slot_size)
            break;
        /* look at contiguously-next fragment_t in cache */
        pc += FRAG_SIZE(victim);
        if (pc == unit->cur_pc) {
            /* we can just take unallocated space */
            break;
        }
        ASSERT(pc < unit->cur_pc);
        victim = *((fragment_t **)pc);
        LOG(THREAD, LOG_CACHE, 5, "\treading " PFX " -> " PFX "\n", pc, victim);
        ASSERT(victim != NULL);
        ASSERT(FIFO_UNIT(victim) == unit);
    }

    LOG(THREAD, LOG_CACHE, 4, "\treplacing fragment(s) in filled unit\n");

    /* record stats that will be destroyed */
    header_pc = FRAG_HDR_START(fifo);

    /* second walk: do the deletion */
    slot_so_far = 0;
    pc = header_pc;
    victim = fifo;
    while (true) {
        slot_so_far += FRAG_SIZE(victim);
        pc += FRAG_SIZE(victim);
        LOG(THREAD, LOG_CACHE, 4, "\t\tdeleting F%d => %d bytes\n", FRAG_ID(victim),
            slot_so_far);
        force_fragment_from_cache(dcontext, cache, victim);
        if (slot_so_far >= slot_size)
            break;
        /* look at contiguously-next fragment_t in cache
         * assumption: wouldn't be here if not enough victims below us, so
         * don't need to check for end of cache
         */
        if (pc == unit->cur_pc) {
            /* take unallocated space */
            size_t extra =
                extend_unit_end(dcontext, cache, unit, slot_size - slot_so_far, false);
            LOG(THREAD, LOG_CACHE, 4, "\t\textending unit by %d => %d bytes\n",
                slot_size - slot_so_far, slot_size);
            ASSERT(CHECK_TRUNCATE_TYPE_uint(extra));
            FRAG_SIZE_ASSIGN(f, slot_size + (uint)extra);
            /* no splitting will be needed */
            slot_so_far = slot_size;
            break;
        }
        ASSERT(pc < unit->cur_pc);
        victim = *((fragment_t **)pc);
    }

    if (slot_so_far > slot_size) {
        uint diff = slot_so_far - slot_size;
        /* if we were using free lists we would check for next slot being a
         * free entry and if so coalescing any size space with it
         */
        if (diff < MIN_EMPTY_HOLE(cache)) {
            FRAG_SIZE_ASSIGN(f, slot_so_far);
            LOG(THREAD, LOG_CACHE, 4, "\t\teating extra %d bytes\n", diff);
        } else {
            /* add entry for diff */
            fifo_prepend_empty(dcontext, cache, unit, NULL, header_pc + FRAG_SIZE(f),
                               diff);
            STATS_FCACHE_SUB(cache, used, diff);
        }
    }

    place_fragment(dcontext, f, unit, header_pc);
    fifo_append(cache, f);

    if (cache->finite_cache && cache->num_replaced > 0) {
        future_fragment_t *fut = fragment_lookup_cache_deleted(dcontext, cache, f->tag);
        ASSERT(cache->finite_cache && cache->replace_param > 0);
        if (fut != NULL) {
            cache->num_regenerated++;
            STATS_INC(num_fragments_regenerated);
            SHARED_FLAGS_RECURSIVE_LOCK(fut->flags, acquire, change_linking_lock);
            fut->flags &= ~FRAG_WAS_DELETED;
            SHARED_FLAGS_RECURSIVE_LOCK(fut->flags, release, change_linking_lock);
        }
        LOG(THREAD, LOG_CACHE, 4, "For %s unit: %d regenerated / %d replaced\n",
            cache->name, cache->num_regenerated, cache->num_replaced);
    }
    DODEBUG({ cache->consistent = true; });
    return true;
}

static inline bool
replace_fifo(dcontext_t *dcontext, fcache_t *cache, fragment_t *f, uint slot_size,
             fragment_t *fifo)
{
    fcache_unit_t *unit;
    ASSERT(USE_FIFO(f));
    ASSERT(CACHE_PROTECTED(cache));
    while (fifo != NULL) {
        unit = FIFO_UNIT(fifo);
        if ((ptr_uint_t)(unit->end_pc - FRAG_HDR_START(fifo)) >= slot_size) {
            /* try to replace fifo and possibly subsequent frags with f
             * could fail if un-deletable frags
             */
            DOLOG(4, LOG_CACHE, { verify_fifo(dcontext, cache); });
            if (replace_fragments(dcontext, cache, unit, f, fifo, slot_size))
                return true;
        }
        fifo = FIFO_NEXT(fifo);
    }
    return false;
}

static inline int
find_free_list_bucket(uint size)
{
    int bucket;
    /* Find maximum slot we are >= than, for size in [SIZE[bucket], SIZE[bucket+1]) */
    for (bucket = FREE_LIST_SIZES_NUM - 1; size < FREE_LIST_SIZES[bucket]; bucket--)
        ; /* no body */
    ASSERT(bucket >= 0);
    return bucket;
}

static inline free_list_footer_t *
free_list_footer_from_header(free_list_header_t *h)
{
    return (free_list_footer_t *)(((cache_pc)h) + h->size - sizeof(free_list_footer_t));
}

static inline free_list_header_t *
free_list_header_from_footer(free_list_footer_t *h)
{
    return (free_list_header_t *)(((cache_pc)h) + sizeof(free_list_footer_t) - h->size);
}

static inline void
remove_from_free_list(fcache_t *cache, uint bucket,
                      free_list_header_t *header _IF_DEBUG(bool coalesce))
{
    ASSERT(CACHE_PROTECTED(cache));
    ASSERT(DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared);
    LOG(GLOBAL, LOG_CACHE, 4, "remove_from_free_list: %s bucket[%d] %d bytes @" PFX "\n",
        cache->name, bucket, header->size, header);
    if (header->prev != NULL) {
        free_list_header_t *prev_writable =
            (free_list_header_t *)vmcode_get_writable_addr((byte *)header->prev);
        prev_writable->next = header->next;
    } else
        cache->free_list[bucket] = header->next;
    if (header->next != NULL) {
        free_list_header_t *next_writable =
            (free_list_header_t *)vmcode_get_writable_addr((byte *)header->next);
        next_writable->prev = header->prev;
    }
    /* it's up to the caller to adjust FRAG_FOLLOWS_FREE_ENTRY if a fragment_t
     * follows header.
     * no reason to remove FRAG_FCACHE_FREE_LIST flag here.
     */
    DOSTATS({
        if (coalesce)
            cache->free_stats_coalesced[bucket]++;
        else
            cache->free_stats_reused[bucket]++;
        cache->free_stats_charge[bucket] -= header->size;
    });
}

/* If freeing a fragment, must pass that as f; else, pass NULL as f */
static void
add_to_free_list(dcontext_t *dcontext, fcache_t *cache, fcache_unit_t *unit,
                 fragment_t *f, cache_pc start_pc, uint size)
{
    int bucket;
    free_list_header_t *header = (free_list_header_t *)start_pc;

    ASSERT(CACHE_PROTECTED(cache));
    ASSERT(DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared);
    DODEBUG({
        /* only count frees of actual fragments */
        if (f != NULL)
            cache->free_size_histogram[get_histogram_bucket(size)]++;
    });
    DOCHECK(CHKLVL_DEFAULT,
            { /* expensive, makes fragment_exit() O(n^2) */
              ASSERT(dynamo_resetting || fcache_pc_in_live_unit(cache, start_pc));
            });

    if (size > MAX_FREE_ENTRY_SIZE) {
        /* FIXME PR 203913: fifo_prepend_empty can handle larger sizes, but
         * we can't: we would need to split into two empty slots.
         * For now we bail and leak.
         */
        ASSERT_NOT_REACHED();
        return;
    }

    /* check next slot first before we potentially shift back from coalescing */
    if (unit->cur_pc > start_pc + size && !POINTER_OVERFLOW_ON_ADD(start_pc, size)) {
        fragment_t *subseq = FRAG_NEXT_SLOT(start_pc, size);
        if (FRAG_IS_FREE_LIST(subseq)) {
            /* this is a free list entry, coalesce with it */
            free_list_header_t *next_header = FRAG_NEXT_FREE(start_pc, size);
            /* only coalesce if not over size limit */
            if ((uint)next_header->size + size <= MAX_FREE_ENTRY_SIZE) {
                uint next_bucket = find_free_list_bucket(next_header->size);
                LOG(GLOBAL, LOG_CACHE, 4,
                    "add_to_free_list: coalesce w/ next %s bucket[%d] %d bytes @" PFX
                    "\n",
                    cache->name, next_bucket, next_header->size, next_header);
                size += next_header->size;
                /* OPTIMIZATION: if still in same bucket can eliminate some work */
                remove_from_free_list(cache, next_bucket,
                                      next_header _IF_DEBUG(true /*coalesce*/));
                /* fall-through and add to free list anew
                 * (potentially coalesce with prev as well)
                 */
                STATS_FCACHE_ADD(cache, free_coalesce_next, 1);
            } else {
                /* FIXME: if we have a few giant free entries we should
                 * free the whole unit.
                 */
                STATS_FCACHE_ADD(cache, free_coalesce_too_big, 1);
            }
        } else {
            /* A real fragment_t: mark it.
             * This is the only place we need to mark, as we disallow free lists at
             * the end of the current unit (so an appended fragment_t will never need
             * this flag).
             */
            LOG(GLOBAL, LOG_CACHE, 4,
                "add_to_free_list: marking next F%d(" PFX ")." PFX " as after-free\n",
                subseq->id, subseq->tag, subseq->start_pc);
            ASSERT(FIFO_UNIT(subseq) == unit);
            ASSERT(FRAG_HDR_START(subseq) == start_pc + size);
            /* can already be marked if we're called due to a split */
            if (!TEST(FRAG_FOLLOWS_FREE_ENTRY, subseq->flags)) {
                if (TEST(FRAG_SHARED, subseq->flags))
                    acquire_recursive_lock(&change_linking_lock);
                subseq->flags |= FRAG_FOLLOWS_FREE_ENTRY;
                if (TEST(FRAG_SHARED, subseq->flags))
                    release_recursive_lock(&change_linking_lock);
            }
        }
    }
    /* If we're actually freeing a real fragment_t, we can coalesce with prev.
     * Other reasons to come here should never have a free entry in the prev slot.
     */
    if (f != NULL && TEST(FRAG_FOLLOWS_FREE_ENTRY, f->flags)) {
        /* coalesce with prev */
        free_list_footer_t *prev_footer =
            (free_list_footer_t *)(start_pc - sizeof(free_list_footer_t));
        free_list_header_t *prev_header = free_list_header_from_footer(prev_footer);
        uint prev_bucket = find_free_list_bucket(prev_footer->size);
        /* only coalesce if not over size limit */
        uint new_size = (uint)prev_header->size + size;
        if (new_size <= MAX_FREE_ENTRY_SIZE) {
            LOG(GLOBAL, LOG_CACHE, 4,
                "add_to_free_list: coalesce w/ prev %s bucket[%d] %d bytes @" PFX "\n",
                cache->name, prev_bucket, prev_header->size, prev_header);
            size += prev_header->size;
            header = prev_header;
            start_pc = (cache_pc)header;
            /* OPTIMIZATION: if still in same bucket can eliminate some work */
            remove_from_free_list(cache, prev_bucket,
                                  prev_header _IF_DEBUG(true /*coalesce*/));
            /* fall-through and add to free list anew */
            STATS_FCACHE_ADD(cache, free_coalesce_prev, 1);
        } else {
            /* see FIXMEs for next-coalesce-too-large above */
            STATS_FCACHE_ADD(cache, free_coalesce_too_big, 1);
        }
    }

    /* Invariant: no free entry can end at the append point of the current unit.
     * It we want to relax this we must mark an appended fragment as
     * FRAG_FOLLOWS_FREE_ENTRY.  We do allow a free entry at the very end of the
     * now-full cur unit.  FIXME: this code is fragile wrt extend_unit_end's
     * fifo_prepend_empty(), which wants a free list at the end of the unit, and
     * only avoids disaster here by not incrementing cur_pc until afterward, so
     * our condition here is not triggered.  We could add another param to
     * distinguish (f==NULL is not good enough as splits also call here).
     */
    if (unit == cache->units && unit->cur_pc == start_pc + size) {
        /* free space at end of current unit: just adjust cur_pc */
        ASSERT((unit->full && unit->cur_pc == unit->end_pc) ||
               (!unit->full && unit->cur_pc < unit->end_pc));
        unit->cur_pc = start_pc;
        unit->full = false;
        STATS_FCACHE_ADD(cache, return_last, 1);
        STATS_FCACHE_SUB(cache, claimed, size);
        STATS_SUB(fcache_combined_claimed, size);
        return;
    }

    /* ok to call w/ small size if you know it will be coalesced or returned,
     * but not if it's to be its own entry
     */
    ASSERT(size >= MIN_FCACHE_SLOT_SIZE(cache));

    bucket = find_free_list_bucket(size);

    free_list_header_t *header_writable =
        (free_list_header_t *)vmcode_get_writable_addr((byte *)header);
    header_writable->next = cache->free_list[bucket];
    header_writable->prev = NULL;
    header_writable->size = size;
    header_writable->flags = FRAG_FAKE | FRAG_FCACHE_FREE_LIST;
    free_list_footer_t *footer_writable = free_list_footer_from_header(header_writable);
    footer_writable->size = size;
    if (cache->free_list[bucket] != NULL) {
        ASSERT(cache->free_list[bucket]->prev == NULL);
        free_list_header_t *list_writable =
            (free_list_header_t *)vmcode_get_writable_addr(
                (byte *)cache->free_list[bucket]);
        list_writable->prev = header;
    }
    cache->free_list[bucket] = header;
    /* FIXME: case 7318 we should keep sorted */

    DOSTATS({
        /* FIXME: we could split freed into pure-freed, split-freed, and coalesce-freed */
        cache->free_stats_freed[bucket]++;
        cache->free_stats_charge[bucket] += size;
        LOG(GLOBAL, LOG_CACHE, 4, "add_to_free_list: %s bucket[%d] %d bytes @" PFX "\n",
            cache->name, bucket, size, start_pc);
        /* assumption: caller has already adjusted cache's empty space stats */
    });
}

static bool
find_free_list_slot(dcontext_t *dcontext, fcache_t *cache, fragment_t *f, uint size)
{
    int bucket;
    cache_pc start_pc;
    uint free_size;
    free_list_header_t *header = NULL;
    fcache_unit_t *unit = NULL;
    DEBUG_DECLARE(bool split_empty = false;)

    ASSERT(!USE_FIFO(f) && USE_FREE_LIST(f));
    ASSERT(CACHE_PROTECTED(cache));
    ASSERT(DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared);
    LOG(THREAD, LOG_CACHE, 4, "find_free_list_slot: %d bytes\n", size);
    DODEBUG({ cache->request_size_histogram[get_histogram_bucket(size)]++; });

    if (size > MAX_FREE_ENTRY_SIZE) {
        /* FIXME: we may have adjacent un-coalesced free slots we could use */
        return false;
    }

    /* Strategy: search first in current bucket and if nothing is
     * found, then upgrade.  Coalescing and splitting allows a blind
     * always-upgrade strategy.
     */
    /* case 7318 for discussion on additional search strategies
     * FIXME: for any bucket if we are
     * too close to the top we should look in the next bucket?  (We
     * used to have a scheme for FREE_LIST_BOTTOM_BUCKET_MARGIN)
     */
    for (bucket = find_free_list_bucket(size); bucket < FREE_LIST_SIZES_NUM; bucket++) {
        if (cache->free_list[bucket] == NULL)
            continue;

        /* search for first entry larger than size in current bucket */
        /* note that for a bucket of only one size, this search should
         * finish immediately */
        header = cache->free_list[bucket];

        while (header != NULL && header->size < size) {
            /* FIXME: if we keep the list sorted, we'd not waste too
             * much space by picking the first large enough slot */
            /* FIXME: if we want to coalesce here we can act on any
             * fragment while walking list and make sure that it is
             * coalesced */
            header = header->next;
        }
        if (header != NULL)
            break;
    }
    if (bucket >= FREE_LIST_SIZES_NUM)
        return false;

    DOSTATS({
        if (bucket > find_free_list_bucket(size))
            STATS_FCACHE_ADD(cache, free_use_larger, 1);
    });
    ASSERT(header != NULL);
    /* found big enough free slot, extract from free list */
    remove_from_free_list(cache, bucket, header _IF_DEBUG(false /*!coalesce*/));

    start_pc = (cache_pc)header;
    free_size = header->size;
    ASSERT(free_size >= size);
    ASSERT(free_size <= MAX_FREE_ENTRY_SIZE);

    /* FIXME: if this vmarea lookup is expensive, we can also keep the
     * unit ptr/tag in the free header.*/
    unit = fcache_lookup_unit(start_pc);
    ASSERT(unit != NULL);
    DOCHECK(CHKLVL_DEFAULT, { /* expensive */
                              ASSERT(fcache_pc_in_live_unit(cache, start_pc));
    });

    /* FIXME: if bucket sizes are spread apart further than
     * MIN_EMPTY_HOLE() this will also kick in.  Currently an
     * issue only for traces and the bucket [112, 172).
     */
    /* if enough room left over, split it off as its own free slot */
    if (free_size - size > MIN_EMPTY_HOLE(cache)) {
        DODEBUG({
            cache->free_stats_split[bucket]++;
            split_empty = true;
        });
        STATS_FCACHE_ADD(cache, free_split, 1);

        add_to_free_list(dcontext, cache, unit, NULL, start_pc + size, free_size - size);
        free_size = size;
        /* next fragment_t remains marked as FRAG_FOLLOWS_FREE_ENTRY */
    } else {
        /* taking whole entry */
        if (unit->cur_pc > start_pc + free_size) {
            fragment_t *subseq = FRAG_NEXT_SLOT(start_pc, free_size);
            /* remove FRAG_FOLLOWS_FREE_ENTRY flag from subsequent fragment_t,
             * if it exists
             */
            if (!FRAG_IS_FREE_LIST(subseq)) {
                LOG(GLOBAL, LOG_CACHE, 4,
                    "find_free_list_slot: un-marking next F%d(" PFX ")." PFX
                    " as after-free\n",
                    subseq->id, subseq->tag, subseq->start_pc);
                ASSERT(FIFO_UNIT(subseq) == unit);
                ASSERT(FRAG_HDR_START(subseq) == start_pc + free_size);
                if (TEST(FRAG_SHARED, subseq->flags))
                    acquire_recursive_lock(&change_linking_lock);
                ASSERT(TEST(FRAG_FOLLOWS_FREE_ENTRY, subseq->flags));
                subseq->flags &= ~FRAG_FOLLOWS_FREE_ENTRY;
                if (TEST(FRAG_SHARED, subseq->flags))
                    release_recursive_lock(&change_linking_lock);
            } else {
                /* shouldn't be free list entry following this one, unless
                 * unable to coalesce due to ushort size limits
                 */
                ASSERT(free_size + FRAG_NEXT_FREE(start_pc, free_size)->size >
                       MAX_FREE_ENTRY_SIZE);
            }
        }
    }

    place_fragment(dcontext, f, unit, start_pc);
    FRAG_SIZE_ASSIGN(f, free_size);

    DOSTATS({
        LOG(GLOBAL, LOG_CACHE, 4,
            "find_free_list_slot: %s bucket[%d]%s"
            " %d bytes @" PFX " requested %d, waste=%d\n",
            cache->name, bucket, split_empty ? "split" : "", free_size, start_pc, size,
            (free_size - size));
    });
    STATS_FCACHE_ADD(cache, align, free_size - size);
    STATS_FCACHE_SUB(cache, empty, free_size);
    return true;
}

/* separate routine b/c it may recurse */
static void
add_fragment_common(dcontext_t *dcontext, fcache_t *cache, fragment_t *f, uint slot_size)
{
    fragment_t *fifo = NULL;
    fcache_unit_t *unit;
    ASSERT(CACHE_PROTECTED(cache));

    /* first, check fifo for empty slot
     * don't look for empty of appropriate size -- kick out the neighbors!
     * we've found that works better, else empty list too long, and splitting
     * it by size ruins the fifo
     */
    if (USE_FREE_LIST_FOR_CACHE(cache)) {
        if (DYNAMO_OPTION(cache_shared_free_list) &&
            find_free_list_slot(dcontext, cache, f, slot_size))
            return;
        /* If no free list, no way to insert fragment into middle of cache */
    } else if (USE_FIFO_FOR_CACHE(cache)) {
        fifo = cache->fifo;
        while (fifo != NULL && FRAG_EMPTY(fifo)) {
            unit = FIFO_UNIT(fifo);
            if ((ptr_uint_t)(unit->end_pc - FRAG_HDR_START(fifo)) >= slot_size) {
                /* try to replace fifo and possibly subsequent frags with f
                 * could fail if un-deletable frags
                 */
                LOG(THREAD, LOG_CACHE, 4, "\ttrying to fit in empty slot\n");
                DOLOG(4, LOG_CACHE, { verify_fifo(dcontext, cache); });
                if (replace_fragments(dcontext, cache, unit, f, fifo, slot_size))
                    return;
            }
            fifo = FIFO_NEXT(fifo);
        }
    }

    /* second, look for room at end, if cache never filled up before */
    unit = cache->units; /* most recent is only potentially non-full unit */
    if (!unit->full && (ptr_uint_t)(unit->end_pc - unit->cur_pc) >= slot_size) {
        /* just add to end */
        size_t extra;
        place_fragment(dcontext, f, unit, unit->cur_pc);
        extra = extend_unit_end(dcontext, cache, unit, slot_size, false);
        if (extra > 0) {
            ASSERT(CHECK_TRUNCATE_TYPE_uint(extra));
            FRAG_SIZE_ASSIGN(f, slot_size + (uint)extra);
            STATS_FCACHE_ADD(cache, align, extra);
        }
        if (USE_FIFO_FOR_CACHE(cache))
            fifo_append(cache, f);
        LOG(THREAD, LOG_CACHE, 4,
            "\tadded F%d to unfilled unit @" PFX " (%d [/%d] bytes left now)\n", f->id,
            f->start_pc, unit->end_pc - unit->cur_pc, UNIT_RESERVED_SIZE(unit));
        return;
    }

    /* third, resize and try again.
     * for fifo caches, don't resize unless regen/replace ratio warrants it.
     */
    if (!USE_FIFO_FOR_CACHE(cache) || cache->is_coarse ||
        check_regen_replace_ratio(dcontext, cache, slot_size)) {
        LOG(THREAD, LOG_CACHE, 3, "\tcache is full, trying to acquire more space\n");
        if (try_for_more_space(dcontext, cache, unit, slot_size)) {
            add_fragment_common(dcontext, cache, f, slot_size);
            return;
        }
    }

    /* all our subsequent schemes require a FIFO (non-FIFO caches will have
     * been resized in step 3)
     */
    ASSERT(USE_FIFO_FOR_CACHE(cache));

    /* finally, boot somebody out -- in FIFO order, only go to next if
     * not enough room from victim to end of cache.
     * fifo should be pointing to first non-empty slot!
     */
    if (replace_fifo(dcontext, cache, f, slot_size, fifo))
        return;

    /* if get here, no room, so must resize */
    LOG(THREAD, LOG_CACHE, 3,
        "\ttried to avoid resizing, but no way around it for fragment size %d\n",
        slot_size);
    if (try_for_more_space(dcontext, cache, unit, slot_size)) {
        add_fragment_common(dcontext, cache, f, slot_size);
        return;
    }

    /* if get here, must have a very large fragment relative to constrained cache size,
     * plus some undeletable fragments right in the middle.
     * abort the trace, hopefully making fragments re-deletable, and try again.
     */
    trace_abort(dcontext);
    if (replace_fifo(dcontext, cache, f, slot_size, cache->fifo))
        return;

    /* could still get here if undeletable fragments...but what can we do?
     * current impl only makes trace-in-progress undeletable, right?
     * so should never get here
     */
    ASSERT_NOT_REACHED();
}

void
fcache_shift_start_pc(dcontext_t *dcontext, fragment_t *f, uint space)
{
    fcache_unit_t *unit;
    fcache_t *cache = NULL;

    if (space == 0)
        return;

    /* must hold cache lock across any set of a fragment's start_pc or size
     * once that fragment is in a cache, as contig-cache-walkers need a consistent view!
     */
    if (TEST(FRAG_SHARED, f->flags)) {
        /* optimization: avoid unit lookup for private fragments.
         * this assumes that no cache lock is used for private fragments!
         */
        unit = FIFO_UNIT(f);
        ASSERT(unit != NULL);
        cache = unit->cache;
        PROTECT_CACHE(cache, lock);
    }

    /* we back align to remove the padding */
    ASSERT(ALIGNED(f->start_pc, START_PC_ALIGNMENT));

    /* adjusting start_pc */
    ASSERT(space <= START_PC_ALIGNMENT - 1); /* most we can shift */
    ASSERT(PAD_JMPS_SHIFT_START(f->flags));

    /* FIXME : no need to set this memory to anything, but is easier to debug
     * if it's valid instructions */
    SET_TO_DEBUG(vmcode_get_writable_addr(f->start_pc), space);

    f->start_pc += space;
    ASSERT_TRUNCATE(f->size, ushort, (f->size - space));
    f->size = (ushort)(f->size - space);
    DODEBUG({
        if (space > 0) {
            STATS_PAD_JMPS_ADD(f->flags, num_start_pc_shifted, 1);
            STATS_PAD_JMPS_ADD(f->flags, start_pc_shifted_bytes, space);
        }
    });

    if (TEST(FRAG_SHARED, f->flags))
        PROTECT_CACHE(cache, unlock);
}

void
fcache_return_extra_space(dcontext_t *dcontext, fragment_t *f, size_t space_in)
{
    fcache_unit_t *unit = FIFO_UNIT(f);
    fcache_t *cache = unit->cache;
    uint returnable_space, slot_padding, space;
    cache_pc end_addr;
    uint min_padding = 0;
    byte *returnable_start;
    bool released = false;
    PROTECT_CACHE(cache, lock);

    /* truncate up front */
    ASSERT_TRUNCATE(space, uint, space_in);
    space = (uint)space_in;

    DOSTATS({
        if (ALIGN_FORWARD(f->size + HEADER_SIZE(f) - space, SLOT_ALIGNMENT(cache)) <
            MIN_FCACHE_SLOT_SIZE(cache))
            STATS_INC(num_final_fragment_too_small);
    });

    /* get the total amount of free space at the end of the fragment including
     * any end padding (for slot alignment etc., stored in fcache->extra) */
    returnable_space = space + f->fcache_extra - HEADER_SIZE(f);
    if (FRAG_SIZE(f) - returnable_space < MIN_FCACHE_SLOT_SIZE(cache)) {
        min_padding = returnable_space - (FRAG_SIZE(f) - MIN_FCACHE_SLOT_SIZE(cache));
        returnable_space = FRAG_SIZE(f) - MIN_FCACHE_SLOT_SIZE(cache);
    }
    /* now adjust for slot alignment padding */
    end_addr = FRAG_HDR_START(f) + FRAG_SIZE(f) - returnable_space;
    ASSERT_TRUNCATE(slot_padding, uint,
                    ALIGN_FORWARD(end_addr, SLOT_ALIGNMENT(cache)) -
                        (ptr_uint_t)end_addr);
    slot_padding =
        (uint)(ALIGN_FORWARD(end_addr, SLOT_ALIGNMENT(cache)) - (ptr_uint_t)end_addr);
    returnable_space -= slot_padding;
    returnable_start = FRAG_HDR_START(f) + FRAG_SIZE(f) - returnable_space;
    ASSERT(FRAG_SIZE(f) - returnable_space >= MIN_FCACHE_SLOT_SIZE(cache));
    ASSERT(ALIGNED(returnable_space, SLOT_ALIGNMENT(cache)));
    ASSERT(ALIGNED(returnable_start, SLOT_ALIGNMENT(cache)));

    if (returnable_space > 0) {
        STATS_INC(pad_jmps_fragments_overestimated);
        /* first check if f is the last fragment in the unit */
        if (FRAG_HDR_START(f) + FRAG_SIZE(f) == unit->end_pc) {
            /* f is the last fragment in a full unit */
            ASSERT(unit->full);
            if (returnable_space >= MIN_UNIT_END_HOLE(cache)) {
                if (unit == cache->units) {
                    /* we just filled this unit, mark un-full and adjust cur_pc */
                    ASSERT(unit->cur_pc == unit->end_pc);
                    unit->full = false;
                    unit->cur_pc -= returnable_space;
                    STATS_FCACHE_SUB(cache, claimed, returnable_space);
                    STATS_SUB(fcache_combined_claimed, returnable_space);
                    released = true;
                } else {
                    /* create a new empty slot */
                    fifo_prepend_empty(dcontext, cache, unit, NULL, returnable_start,
                                       returnable_space);
                    released = true;
                }
            }
        } else if (FRAG_HDR_START(f) + FRAG_SIZE(f) == unit->cur_pc) {
            /* f is the last fragment in a non-full unit */
            ASSERT(!unit->full);
            unit->cur_pc -= returnable_space;
            STATS_FCACHE_SUB(cache, claimed, returnable_space);
            STATS_SUB(fcache_combined_claimed, returnable_space);
            released = true;
        } else {
            if (!(DYNAMO_OPTION(cache_shared_free_list) && cache->is_shared)) {
                fragment_t *next_f;
                /* since we aren't at the end of the used space in the unit there must
                 * be a fragment or empty slot after us */
                next_f = FRAG_NEXT_SLOT(FRAG_HDR_START(f), FRAG_SIZE(f));
                ASSERT(next_f != NULL); /* sanity check, though not very good one */
                if (FRAG_EMPTY(next_f)) {
                    STATS_FCACHE_ADD(cache, empty, returnable_space);
                    FRAG_START_ASSIGN(next_f, returnable_start + HEADER_SIZE(f));
                    *((fragment_t **)vmcode_get_writable_addr(returnable_start)) = next_f;
                    FRAG_SIZE_ASSIGN(next_f, FRAG_SIZE(next_f) + returnable_space);
                    released = true;
                }
            }
            if (!released) {
                /* return excess if next slot is a free list (will be coalesced) or
                 * if excess is large enough by itself
                 */
                fragment_t *subseq = FRAG_NEXT_SLOT(FRAG_HDR_START(f), FRAG_SIZE(f));
                ASSERT(FRAG_HDR_START(f) + FRAG_SIZE(f) < unit->cur_pc);
                if ((FRAG_IS_FREE_LIST(subseq) &&
                     /* make sure will coalesce
                      * FIXME: fragile if coalesce rules change -- perhaps have
                      * free list routine return failure?
                      */
                     returnable_space +
                             FRAG_NEXT_FREE(FRAG_HDR_START(f), FRAG_SIZE(f))->size <=
                         MAX_FREE_ENTRY_SIZE) ||
                    returnable_space >= MIN_EMPTY_HOLE(cache)) {
                    fifo_prepend_empty(dcontext, cache, unit, NULL, returnable_start,
                                       returnable_space);
                    released = true;
                    DOSTATS({
                        if (FRAG_IS_FREE_LIST(subseq))
                            STATS_INC(pad_jmps_excess_next_free);
                    });
                }
            }
        }
    }

    /* even if returnable_space is 0, space is not, and we need to shift it
     * from f->size to f->fcache_extra
     */
    /* update fragment values + sanity check */
    ASSERT(f->fcache_extra + space ==
           slot_padding + HEADER_SIZE(f) + returnable_space + min_padding);
    ASSERT_TRUNCATE(f->size, ushort, (f->size - space));
    f->size = (ushort)(f->size - space);
    FRAG_SIZE_ASSIGN(f,
                     f->size + HEADER_SIZE(f) + FRAG_START_PADDING(f) + slot_padding +
                         (released ? 0 : returnable_space) + min_padding);
    ASSERT(FRAG_SIZE(f) >= MIN_FCACHE_SLOT_SIZE(cache));
    DOSTATS({
        if (released)
            STATS_FCACHE_SUB(cache, used, returnable_space);
        else if (returnable_space > 0) {
            STATS_INC(pad_jmps_excess_wasted);
            STATS_ADD(pad_jmps_excess_wasted_bytes, returnable_space);
        }
    });

    DOLOG(3, LOG_CACHE, {
        if (USE_FIFO_FOR_CACHE(cache))
            verify_fifo(dcontext, cache);
    });

    PROTECT_CACHE(cache, unlock);

    STATS_PAD_JMPS_ADD(f->flags, extra_space_released, released ? returnable_space : 0);
}

static fcache_t *
get_cache_for_new_fragment(dcontext_t *dcontext, fragment_t *f)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
    fcache_t *cache = NULL;
    if (TEST(FRAG_SHARED, f->flags)) {
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* new fragment must be targeting the one non-frozen unit */
            coarse_info_t *info = get_executable_area_coarse_info(f->tag);
            ASSERT(info != NULL);
            if (info != NULL && info->frozen)
                info = info->non_frozen;
            ASSERT(info != NULL);
            if (info->cache == NULL) {
                /* due to lock ordering problems we must create the cache
                 * before acquiring the info->lock
                 */
                cache = fcache_cache_init(GLOBAL_DCONTEXT, f->flags, true);
                d_r_mutex_lock(&info->lock);
                if (info->cache == NULL) {
                    cache->coarse_info = info;
                    coarse_unit_init(info, cache);
                    ASSERT(cache == info->cache);
                    d_r_mutex_unlock(&info->lock);
                } else {
                    /* w/ bb_building_lock we shouldn't have a race here */
                    ASSERT_CURIOSITY(false && "race in creating coarse cache");
                    d_r_mutex_unlock(&info->lock);
                    fcache_cache_free(GLOBAL_DCONTEXT, cache, true);
                }
            }
            ASSERT(info->cache != NULL);
            ASSERT(((fcache_t *)info->cache)->coarse_info == info);
            return (fcache_t *)info->cache;
        } else {
            if (IN_TRACE_CACHE(f->flags))
                return shared_cache_trace;
            else
                return shared_cache_bb;
        }
    } else {
        /* thread-private caches are delayed */
        if (IN_TRACE_CACHE(f->flags)) {
            if (tu->trace == NULL) {
                tu->trace = fcache_cache_init(dcontext, FRAG_IS_TRACE, true);
                ASSERT(tu->trace != NULL);
                LOG(THREAD, LOG_CACHE, 1, "Initial trace cache is %d KB\n",
                    tu->trace->init_unit_size / 1024);
            }
            return tu->trace;
        } else {
            if (tu->bb == NULL) {
                tu->bb = fcache_cache_init(dcontext, 0 /*private bb*/, true);
                ASSERT(tu->bb != NULL);
                LOG(THREAD, LOG_CACHE, 1, "Initial basic block cache is %d KB\n",
                    tu->bb->init_unit_size / 1024);
            }
            return tu->bb;
        }
    }
    ASSERT_NOT_REACHED();
    return NULL;
}

void
fcache_add_fragment(dcontext_t *dcontext, fragment_t *f)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
    uint slot_size;
    fcache_t *cache = get_cache_for_new_fragment(dcontext, f);
    ASSERT(cache != NULL);
    PROTECT_CACHE(cache, lock);
    /* set start_pc to START_PC_ALIGNMENT so will work with FRAG_START_PADDING */
    f->start_pc = (cache_pc)(ptr_uint_t)START_PC_ALIGNMENT;

    /* delayed unmap...presumably ok to do now! */
    if (tu->pending_unmap_pc != NULL) {
        /* de-allocate old memory */
        /* remove from interval data struct first to avoid races w/ it
         * being re-used and not showing up in in_fcache
         */
        vmvector_remove(fcache_unit_areas, tu->pending_unmap_pc,
                        tu->pending_unmap_pc + tu->pending_unmap_size);
        /* caller must dec stats since here we don't know type of cache */
        heap_munmap(tu->pending_unmap_pc, tu->pending_unmap_size,
                    VMM_CACHE | VMM_REACHABLE);
        tu->pending_unmap_pc = NULL;
    }

    /* starting address of a fragment and its size should always
     * be cache-line-aligned.  we use a 4-byte header as a backpointer
     * to the fragment_t.
     */
    slot_size = f->size + HEADER_SIZE(f);
    if (slot_size < MIN_FCACHE_SLOT_SIZE(cache)) {
        STATS_INC(num_fragment_too_small);
        slot_size = MIN_FCACHE_SLOT_SIZE(cache);
    }
    slot_size = (uint)ALIGN_FORWARD(slot_size, SLOT_ALIGNMENT(cache));
    ASSERT(slot_size >= f->size + HEADER_SIZE(f));
    ASSERT_TRUNCATE(f->fcache_extra, byte, (slot_size - f->size));
    f->fcache_extra = (byte)(slot_size - f->size);
    LOG(THREAD, LOG_CACHE, 4,
        "fcache_add_fragment to %s cache (size %dKB): F%d w/ size %d (=> %d)\n",
        cache->name, cache->units->size / 1024, f->id, f->size, slot_size);

    add_fragment_common(dcontext, cache, f, slot_size);
    ASSERT(!PAD_JMPS_SHIFT_START(f->flags) ||
           ALIGNED(f->start_pc, START_PC_ALIGNMENT)); /* for start_pc padding to work */
    DOLOG(3, LOG_CACHE, {
        if (USE_FIFO_FOR_CACHE(cache))
            verify_fifo(dcontext, cache);
    });
    PROTECT_CACHE(cache, unlock);
}

void
fcache_remove_fragment(dcontext_t *dcontext, fragment_t *f)
{
    fcache_unit_t *unit = FIFO_UNIT(f);
    fcache_t *cache = unit->cache;

    /* should only be deleted through proper synched channels */
    ASSERT(dcontext != GLOBAL_DCONTEXT || dynamo_exited || dynamo_resetting ||
           TEST(FRAG_WAS_DELETED, f->flags) || is_self_allsynch_flushing());
    PROTECT_CACHE(cache, lock);

    LOG(THREAD, LOG_CACHE, 4, "fcache_remove_fragment: F%d from %s unit\n", f->id,
        cache->name);

    DOSTATS({ removed_fragment_stats(dcontext, cache, f); });
    STATS_FCACHE_SUB(cache, used, FRAG_SIZE(f));

#ifdef DEBUG_MEMORY
    /* Catch stale execution by filling w/ int3.
     * We do this before fifo_prepend_empty to avoid figuring whether
     * to leave alone th1 st 4 bytes of fragment space or not (it's
     * used to store the size for cache_shared_free_list).
     * FIXME: put in the rest of the patterns and checks to make this
     * parallel to heap DEBUG_MEMORY (==case 5657)
     */
    memset(vmcode_get_writable_addr(f->start_pc), DEBUGGER_INTERRUPT_BYTE, f->size);
#endif

    /* if the entire unit is being freed, do not place individual fragments in
     * the unit on free lists or the FIFO
     */
    if (!unit->pending_free) {
        /* empty slots always go on front */
        fifo_prepend_empty(dcontext, cache, unit, f, FRAG_HDR_START(f), FRAG_SIZE(f));
        if (USE_FIFO(f)) {
            fifo_remove(dcontext, cache, f);
            DOLOG(3, LOG_CACHE, { verify_fifo(dcontext, cache); });
        }
    }
    PROTECT_CACHE(cache, unlock);
}

#ifdef SIDELINE
dcontext_t *
get_dcontext_for_fragment(fragment_t *f)
{
    fcache_unit_t *unit = fcache_lookup_unit(f->start_pc);
    ASSERT(unit != NULL);
    return unit->dcontext;
}
#endif

/***************************************************************************
 * FLUSHING OF UNITS
 *
 * General strategy: first, put units to flush on the global units_to_flush
 * list.  At a nolinking point, use flush synch to get other threads out of
 * the target units, and then walk each unit using contiguous header walk.
 * Remove free list and lazy entries, and chain up all fragments to pass to
 * flush unlink_shared().  Move units to a global units_to_free list, recording
 * their flushtimes.  We'll be notified when a pending delete entry is
 * freed, and we can then check which units are safe to free.
 */

bool
fcache_is_flush_pending(dcontext_t *dcontext)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
    return tu->pending_flush;
}

/* accepts a chain of units linked by next_local.
 * caller must set pending_free and flushtime fields.
 */
static void
append_units_to_free_list(fcache_unit_t *u)
{
    d_r_mutex_lock(&unit_flush_lock);

    /* must append to keep in increasing flushtime order */
    if (allunits->units_to_free_tail == NULL) {
        ASSERT(allunits->units_to_free == NULL);
        allunits->units_to_free = u;
    } else {
        ASSERT(allunits->units_to_free->flushtime <= u->flushtime);
        ASSERT(allunits->units_to_free_tail->next_local == NULL);
        ASSERT(allunits->units_to_free_tail != u);
        allunits->units_to_free_tail->next_local = u;
    }
    STATS_ADD_PEAK(cache_units_tofree, 1);
    /* support adding a chain */
    ASSERT(u->flushtime > 0);
    ASSERT(u->pending_free);
    while (u->next_local != NULL) {
        if (u->cache != NULL)
            remove_unit_from_cache(u);
        ASSERT(u->cache == NULL);
        STATS_ADD_PEAK(cache_units_tofree, 1);
        ASSERT(u->flushtime <= u->next_local->flushtime);
        u = u->next_local;
        ASSERT(u->flushtime > 0);
        ASSERT(u->pending_free);
    }
    allunits->units_to_free_tail = u;
    ASSERT(allunits->units_to_free_tail->next_local == NULL);

    d_r_mutex_unlock(&unit_flush_lock);
}

/* It is up to the caller to ensure it's safe to string the fragments in
 * unit into a list, by only calling us between stage1 and stage2 of
 * flushing (there's no other synch that is safe).
 */
static fragment_t *
chain_fragments_for_flush(dcontext_t *dcontext, fcache_unit_t *unit, fragment_t **tail)
{
    fragment_t *list = NULL, *f = NULL, *prev_f = NULL;
    cache_pc pc;
    ASSERT(is_self_flushing());
    LOG(THREAD, LOG_CACHE, 4, "\tchaining fragments in unit " PFX "-" PFX "\n",
        unit->start_pc, unit->end_pc);
    /* FIXME: we walk all fragments here just to call
     * vm_area_remove_fragment(), and do another complete walk in
     * unlink_fragments_for_deletion() -- can we reduce to one walk?
     */
    pc = unit->start_pc;
    while (pc < unit->cur_pc) {
        bool add_to_list = false;
        f = *((fragment_t **)pc);
        LOG(THREAD, LOG_CACHE, 5, "\treading " PFX " -> " PFX "\n", pc, f);
        if (USE_FREE_LIST_FOR_CACHE(unit->cache)) {
            if (FRAG_IS_FREE_LIST(f)) {
                /* we're going to free the whole unit so we have to
                 * remove this entry from the free list
                 */
                /* while flush synch is enough, cleaner to hold official lock */
                free_list_header_t *cur_free = ((free_list_header_t *)pc);
                int bucket = find_free_list_bucket(cur_free->size);
                LOG(THREAD, LOG_CACHE, 5,
                    "\tremoving free list entry " PFX " size %d bucket [%d]\n", cur_free,
                    cur_free->size, bucket);
                /* we officially grab the lock for free list manip, though
                 * flush synch is enough.  we can't hold cache lock for this
                 * entire routine b/c it has lower rank than the lazy_delete lock.
                 */
                PROTECT_CACHE(unit->cache, lock);
                remove_from_free_list(unit->cache, bucket,
                                      cur_free _IF_DEBUG(false /*!coalesce*/));
                PROTECT_CACHE(unit->cache, unlock);
                pc += cur_free->size;
                continue;
            }
        }
        ASSERT(f != NULL);
        ASSERT(FIFO_UNIT(f) == unit);
        ASSERT(FRAG_HDR_START(f) == pc);
        ASSERT(!TEST(FRAG_CANNOT_DELETE, f->flags));
        if (TEST(FRAG_WAS_DELETED, f->flags)) {
            /* must be a consistency-flushed or lazily deleted fragment.
             * while typically it will be deleted before the other fragments
             * in this unit (since flushtime now is < ours, and lazy are deleted
             * before pending-delete), there is a case where it will not be:
             * if the lazy list hits its threshold and a new pending-delete entry
             * is created before we free our unit-flush pending entry, the
             * lazy pending entry will be freed AFTER flushed unit's entries.
             * we must remove from the lazy list, and go ahead and put into the
             * pending delete list, to keep all our dependences together.
             * fragment_unlink_for_deletion() will not re-do the unlink for
             * the lazy fragment.
             * FIXME: this is inefficient b/c no prev ptr in lazy list
             */
            add_to_list = remove_from_lazy_deletion_list(dcontext, f);
            /* if not found, we assume the fragment has already been moved to
             * a pending delete entry, which must have a lower timestamp
             * than ours, so we're all set since vm_area_check_shared_pending()
             * now walks all to-be-freed entries in increasing flushtime order.
             */
            LOG(THREAD, LOG_CACHE, 5,
                "\tlazily-deleted fragment F%d." PFX " removed from lazy list\n", f->id,
                f->start_pc);
        } else {
            LOG(THREAD, LOG_CACHE, 5, "\tadding F%d." PFX " to to-flush list\n", f->id,
                f->start_pc);
            vm_area_remove_fragment(dcontext, f);
            add_to_list = true;
        }
        if (add_to_list) {
            if (prev_f == NULL)
                list = f;
            else
                prev_f->next_vmarea = f;
            prev_f = f;
        }
        /* advance to contiguously-next fragment_t in cache */
        pc += FRAG_SIZE(f);
    }
    ASSERT(pc == unit->cur_pc);
    /* If entire unit is free list entries or lazy-dels that
     * were moved to pending-delete list, then we'll have no list and
     * prev_f==NULL.
     */
    if (list == NULL) {
        ASSERT(prev_f == NULL);
        /* we have to finish off the flush synch so we just pass no list
         * or region to flush_fragments_unlink_shared()
         */
        STATS_INC(cache_units_flushed_nolive);
    } else {
        ASSERT(list != NULL);
        ASSERT(prev_f != NULL);
        prev_f->next_vmarea = NULL;
    }

    if (tail != NULL)
        *tail = prev_f;
    return list;
}

/* Flushes all fragments in the units in the units_to_flush list and
 * moves those units to the units_to_free list.
 * This routine can only be called when !is_self_couldbelinking() and when
 * no locks are held.
 */
bool
fcache_flush_pending_units(dcontext_t *dcontext, fragment_t *was_I_flushed)
{
    fcache_thread_units_t *tu = (fcache_thread_units_t *)dcontext->fcache_field;
    fcache_unit_t *unit_flushed = NULL;
    fcache_unit_t *u, *local_to_flush;
    bool not_flushed = true;
    fragment_t *list_head = NULL, *list_tail = NULL;
    fragment_t *list_add_head, *list_add_tail;
    uint flushtime;
    DEBUG_DECLARE(bool flushed;)

    ASSERT(!is_self_couldbelinking());
    ASSERT_OWN_NO_LOCKS();

    if (!tu->pending_flush)
        return not_flushed;
    tu->pending_flush = false;

    /* we grab a local copy to deal w/ races to flush these units up front
     * rather than getting into the flush synch and finding someone beat us
     */
    d_r_mutex_lock(&unit_flush_lock);
    local_to_flush = allunits->units_to_flush;
    allunits->units_to_flush = NULL;
    d_r_mutex_unlock(&unit_flush_lock);
    if (local_to_flush == NULL)
        return not_flushed;

    LOG(THREAD, LOG_CACHE, 2, "flushing fragments in all pending units\n");

    /* flush flag is private, and shared caches are synch-ed within
     * pending_flush_units_in_cache, so no lock needed here
     */
    if (was_I_flushed != NULL)
        unit_flushed = FIFO_UNIT(was_I_flushed);

    /* First we have to synch w/ all threads to avoid races w/ other threads
     * manipulating fragments in these units at the same time that we are (e.g.,
     * lazily deleting a trace head).  Sure, the unit is not on the live
     * list anymore, but the fragments are reachable.
     */
    DEBUG_DECLARE(flushed =)
    flush_fragments_synch_unlink_priv(dcontext, EMPTY_REGION_BASE, EMPTY_REGION_SIZE,
                                      false /*don't have thread_initexit_lock*/,
                                      false /*not invalidating exec areas*/,
                                      false /*don't force synchall*/
                                      _IF_DGCDIAG(NULL));
    ASSERT(flushed);

    KSTART(cache_flush_unit_walk);
    for (u = local_to_flush; u != NULL; u = u->next_local) {
        /* not implemented for private units, since no reason to use flush there */
        ASSERT(u->cache->is_shared);

        /* unit is no longer to be considered live -- no other thread should
         * use it as a live unit from this point on
         */
        DODEBUG({ u->pending_flush = false; });

        /* Indicate that individual fragments in this unit must not be put
         * on free lists/FIFO empty slots.
         * FIXME: would be nice to set this earlier when move to
         * units_to_flush list, but then end up w/ freed fragments in the
         * middle of the cache that we can't identify in our walk -- so we
         * inefficiently put on free list and then take off in the walk.
         */
        u->pending_free = true;

        if (u == unit_flushed)
            not_flushed = false;

        list_add_head = chain_fragments_for_flush(dcontext, u, &list_add_tail);
        if (list_head == NULL) {
            list_head = list_add_head;
            list_tail = list_add_tail;
        } else if (list_add_head != NULL) {
            list_tail->next_vmarea = list_add_head;
            list_tail = list_add_tail;
        }
        ASSERT(list_tail == NULL /*list empty so far*/ || list_tail->next_vmarea == NULL);

        STATS_INC(cache_units_flushed);
        STATS_DEC(cache_units_toflush);
    }
    KSTOP(cache_flush_unit_walk);
    /* it's ok if list_head is NULL */

    /* do the rest of the unlink and move the fragments to pending del list */
    flush_fragments_unlink_shared(dcontext, EMPTY_REGION_BASE, EMPTY_REGION_SIZE,
                                  list_head _IF_DGCDIAG(NULL));
    flushtime = flushtime_global;
    flush_fragments_end_synch(dcontext, false /*don't keep initexit_lock*/);

    for (u = local_to_flush; u != NULL; u = u->next_local) {
        u->flushtime = flushtime;
        LOG(THREAD, LOG_CACHE, 2,
            "flushed fragments in unit " PFX "-" PFX " @flushtime %u\n", u->start_pc,
            u->end_pc, u->flushtime);
    }

    append_units_to_free_list(local_to_flush);

    return not_flushed;
}

void
fcache_free_pending_units(dcontext_t *dcontext, uint flushtime)
{
    fcache_unit_t *u, *nxt;
    d_r_mutex_lock(&unit_flush_lock);
    for (u = allunits->units_to_free; u != NULL; u = nxt) {
        nxt = u->next_local;
        /* free list must be sorted in increasing flushtime */
        ASSERT(nxt == NULL || u->flushtime <= nxt->flushtime);
        if (u->flushtime <= flushtime) {
            if (u == allunits->units_to_free_tail) {
                ASSERT(u == allunits->units_to_free);
                ASSERT(nxt == NULL);
                allunits->units_to_free_tail = NULL;
            }
            allunits->units_to_free = nxt;
            LOG(THREAD, LOG_CACHE, 2, "freeing flushed unit " PFX "-" PFX "\n",
                u->start_pc, u->end_pc);
            ASSERT(u->pending_free);
            u->pending_free = false;
            /* free_unit will set flushtime to 0 (needs it to assert locks) */
            fcache_free_unit(dcontext, u, true);
            STATS_INC(cache_units_flushed_freed);
            STATS_DEC(cache_units_tofree);
        } else
            break; /* sorted! */
    }
    d_r_mutex_unlock(&unit_flush_lock);
}

/* Used to prevent shared units earmarked for freeing from being re-used.
 * Caller must be at full synch for a flush.
 */
static void
fcache_mark_units_for_free(dcontext_t *dcontext, fcache_t *cache)
{
    fcache_unit_t *u, *head;
    ASSERT(is_self_flushing()); /* FIXME: want to assert in full synch */
    PROTECT_CACHE(cache, lock);
    /* Mark all units as pending_free to avoid fragment deletion from adding
     * them to free lists.
     * Also set flushtime and move them to the pending_free list.
     */
    u = cache->units;
    ASSERT(u != NULL);
    /* leave one unit */
    head = u->next_local;
    cache->units->next_local = NULL;
    for (u = head; u != NULL; u = u->next_local) {
        u->pending_free = true;
        u->flushtime = flushtime_global;
        remove_unit_from_cache(u);
    }
    PROTECT_CACHE(cache, unlock);
    append_units_to_free_list(head);
}

/* Flush all fragments and mark as many cache units as possible for freeing
 * (while invalidate_code_cache() only flushes all fragments and does not try
 * to free any units -- it is meant for consistency purposes, while this is
 * meant for capacity purposes).
 * FIXME: currently only marks shared cache units for freeing.
 * FIXME: should add -stress_flush_units N parameter
 */
void
fcache_flush_all_caches()
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    ASSERT_NOT_TESTED();
    /* FIXME: share parameters w/ invalidate_code_cache()?
     * FIXME: efficiency of region-based vs unit-based flushing
     */
    flush_fragments_in_region_start(
        dcontext, UNIVERSAL_REGION_BASE, UNIVERSAL_REGION_SIZE,
        false /* don't own initexit_lock */, true /* remove futures */,
        false /*not invalidating exec areas*/,
        false /*don't force synchall*/
        _IF_DGCDIAG(NULL));
    /* In presence of any shared fragments, all threads are stuck here at synch point,
     * so we can mess w/ global cache units in an atomic manner wrt the flush.
     * We can't do private here since threads are let go if no shared
     * fragments are enabled, but better to have each thread mark its
     * own anyway.  FIXME -- put flag in delete-list entry
     */
    if (DYNAMO_OPTION(shared_bbs)) {
        fcache_mark_units_for_free(dcontext, shared_cache_bb);
    }
    if (DYNAMO_OPTION(shared_traces)) {
        fcache_mark_units_for_free(dcontext, shared_cache_trace);
    }
    /* FIXME: for thread-private units, should use a trigger in
     * vm_area_flush_fragments() to call a routine here that frees all but
     * one unit
     */
    flush_fragments_in_region_finish(dcontext, false /*don't keep initexit_lock*/);
    STATS_INC(fcache_flush_all);
}

/***************************************************************************/

/* Flush all fragments from all caches and free all of those caches, starting
 * over completely, by suspending all other threads and freeing all fragments
 * and cache units immediately.
 * Can only be called while !couldbelinking.
 * Assumes caller holds reset_pending_lock.
 * Simultaneous resets are not queued up -- one wins and the rest are canceled.
 * Use the schedule_reset() routine to queue up resets of different types, which
 * will all be combined.
 * FIXME: currently target is ignored and assumed to be RESET_ALL
 */
void
fcache_reset_all_caches_proactively(uint target)
{
    thread_record_t **threads;
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    int i, num_threads;
    const thread_synch_state_t desired_state =
        THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER;

    /* Reset is meaningless for hotp_only and thin_client modes (case 8389),
     * though it can be used to release old tables; but old tables aren't
     * needed for hotp_only so they should be stored anyway, which is NYI
     * today.
     */
    ASSERT(DYNAMO_OPTION(enable_reset) && !RUNNING_WITHOUT_CODE_CACHE());
    ASSERT(!is_self_couldbelinking());

    /* synch with other threads also trying to call this routine */
    /* FIXME: use a cleaner model than having callers grab this lock? */
    ASSERT_OWN_MUTEX(true, &reset_pending_lock);
    if (reset_in_progress) {
        d_r_mutex_unlock(&reset_pending_lock);
        return;
    }
    /* Extra layer of checking to avoid a reset when the user does not want it
     * (xref i#3645).
     */
    if (!DYNAMO_OPTION(enable_reset)) {
        d_r_mutex_unlock(&reset_pending_lock);
        return;
    }
    /* N.B.: we relax various synch checks if dynamo_resetting is true, since
     * we will not be holding some locks we normally would need when
     * deleting shared fragments, etc., assuming that we suspend all
     * threads in DR while resetting -- if that ever changes we need to
     * tighten up all those checks again!
     */
    reset_in_progress = true;
    /* this lock is only for synchronizing resets and we do not give it the
     * rank it would need to be held across the whole routine
     */
    d_r_mutex_unlock(&reset_pending_lock);

    LOG(GLOBAL, LOG_CACHE, 2,
        "\nfcache_reset_all_caches_proactively: thread " TIDFMT
        " suspending all threads\n",
        d_r_get_thread_id());

    /* Suspend all DR-controlled threads at safe locations.
     * Case 6821: other synch-all-thread uses can be ignored, as none of them carry
     * any non-persistent state.
     */
    if (!synch_with_all_threads(desired_state, &threads, &num_threads,
                                /* when called prior to entering the cache we could set
                                 * mcontext->pc to next_tag and use
                                 * THREAD_SYNCH_VALID_MCONTEXT, but some callers (like
                                 * nudge) do not satisfy that */
                                THREAD_SYNCH_NO_LOCKS_NO_XFER, /* Case 6821 */
                                /* if we fail to suspend a thread (e.g., for privilege
                                 * reasons) just abort */
                                THREAD_SYNCH_SUSPEND_FAILURE_ABORT
                                    /* if we get in a race with detach, or are having
                                     * synch issues for whatever reason, bail out sooner
                                     * rather than later */
                                    | THREAD_SYNCH_SMALL_LOOP_MAX)) {
        /* just give up */
        reset_in_progress = false;
        ASSERT(!OWN_MUTEX(&all_threads_synch_lock) && !OWN_MUTEX(&thread_initexit_lock));
        ASSERT(threads == NULL);
        ASSERT(!dynamo_all_threads_synched);
        STATS_INC(fcache_reset_abort);
        LOG(GLOBAL, LOG_CACHE, 2,
            "fcache_reset_all_caches_proactively: aborting due to thread synch "
            "failure\n");
        /* FIXME: may need DO_ONCE but only if we do a LOT of resets combined with
         * other nudges or sources of thread permission problems */
        SYSLOG_INTERNAL_WARNING("proactive reset aborted due to thread synch failure");
        return;
    }

    /* now we own the thread_initexit_lock */
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));
    STATS_INC(fcache_reset_proactively);
    DOSTATS({
        if (TEST(RESET_PENDING_DELETION, target))
            STATS_INC(fcache_reset_pending_del);
    });

    DOLOG(1, LOG_STATS, {
        LOG(GLOBAL, LOG_STATS, 1, "\n**************************Stats BEFORE reset:\n");
        dump_global_stats(false);
    });

    LOG(GLOBAL, LOG_CACHE, 2,
        "fcache_reset_all_caches_proactively: walking the threads\n");
    char buf[16];
    snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%d",
             GLOBAL_STAT(num_bbs) + GLOBAL_STAT(num_traces));
    NULL_TERMINATE_BUFFER(buf);
    SYSLOG(SYSLOG_INFORMATION, INFO_RESET_IN_PROGRESS, 3, buf, get_application_name(),
           get_application_pid());

    /* reset_free and reset_init may write to .data.
     * All threads are suspended so no security risk.
     */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);

    /* no lock needed */
    dynamo_resetting = true;

    IF_AARCHXX({
        if (INTERNAL_OPTION(steal_reg_at_reset) != 0)
            arch_reset_stolen_reg();
    });

    /* We free everything before re-init so we can free all heap units.
     * For everything to be freed, it must either be individually freed,
     * or it must reside in non-persistent heap units,
     * which will be thrown out wholesale in heap_reset_free().  The latter
     * is preferable to not waste time on individual deletion.
     * XXX: add consistency check walks before and after for all modules
     */
    for (i = 0; i < num_threads; i++) {
        dcontext_t *dcontext = threads[i]->dcontext;
        if (dcontext != NULL) { /* include my_dcontext here */
            LOG(GLOBAL, LOG_CACHE, 2, "\tconsidering thread #%d " TIDFMT "\n", i,
                threads[i]->id);
            if (dcontext != my_dcontext) {
                /* must translate BEFORE freeing any memory! */
                if (is_thread_currently_native(threads[i])) {
                    /* native_exec's regain-control point is in our DLL,
                     * and lost-control threads are truly native, so no
                     * state to worry about except for hooks -- and we're
                     * not freeing the interception buffer.
                     */
                    LOG(GLOBAL, LOG_CACHE, 2,
                        "\tcurrently native so no translation needed\n");
                } else if (thread_synch_state_no_xfer(dcontext)) {
                    /* Case 6821: do not translate other synch-all-thread users.
                     * They have no persistent state, so leave alone.
                     * Also xref case 7760.
                     */
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "\tat THREAD_SYNCH_NO_LOCKS_NO_XFER so no translation needed\n");
                } else {
                    translate_from_synchall_to_dispatch(threads[i], desired_state);
                }
            }
            last_exit_deleted(dcontext);
            if (target == RESET_PENDING_DELETION) {
                /* case 7394: need to abort other threads' trace building
                 * since the reset xfer to d_r_dispatch will disrupt it
                 */
                if (is_building_trace(dcontext)) {
                    LOG(THREAD, LOG_FRAGMENT, 2,
                        "\tsquashing trace of thread " TIDFMT "\n", i);
                    trace_abort(dcontext);
                }
            } else {
                LOG(GLOBAL, LOG_CACHE, 2, "\tfreeing memory in thread " TIDFMT "\n", i);
                LOG(THREAD, LOG_CACHE, 2, "------- reset for thread " TIDFMT " -------\n",
                    threads[i]->id);
                /* N.B.: none of these can assume the executing thread is the
                 * dcontext owner, esp. wrt tls!
                 */
                /* XXX: now we have {thread_,}init(), {thread_,}exit(), and
                 * *_reset() -- can we systematically construct these lists of
                 * module calls?  The list here, though, is a subset of the others.
                 */
                /* monitor must go first to remove any undeletable private fragments */
                monitor_thread_reset_free(dcontext);
                fragment_thread_reset_free(dcontext);
                fcache_thread_reset_free(dcontext);
                /* arch and os data is all persistent */
                vm_areas_thread_reset_free(dcontext);
                /* now we throw out all non-persistent private heap units */
                heap_thread_reset_free(dcontext);
            }
        }
    }
    if (target == RESET_PENDING_DELETION) {
        /* XXX: optimization: suspend only those threads with low flushtimes */
        LOG(GLOBAL, LOG_CACHE, 2,
            "fcache_reset_all_caches_proactively: clearing shared deletion list\n");
        /* free entire shared deletion list */
        vm_area_check_shared_pending(GLOBAL_DCONTEXT, NULL);
    } else {
        fragment_reset_free();
        link_reset_free();
        fcache_reset_free();
        /* monitor only has thread-private data */
        /* arch and os data is all persistent */
        vm_areas_reset_free();
#ifdef HOT_PATCHING_INTERFACE
        hotp_reset_free();
#endif
        /* now we throw out all non-persistent global heap units */
        heap_reset_free();

        LOG(GLOBAL, LOG_CACHE, 2,
            "fcache_reset_all_caches_proactively: re-initializing\n");

        /* now set up state all over again */
        heap_reset_init();
#ifdef HOT_PATCHING_INTERFACE
        hotp_reset_init();
#endif
        vm_areas_reset_init();
        fcache_reset_init();
        link_reset_init();
        fragment_reset_init();

        for (i = 0; i < num_threads; i++) {
            dcontext_t *dcontext = threads[i]->dcontext;
            if (dcontext != NULL) { /* include my_dcontext here */
                LOG(GLOBAL, LOG_CACHE, 2,
                    "fcache_reset_all_caches_proactively: re-initializing thread " TIDFMT
                    "\n",
                    i);
                /* now set up private state all over again -- generally, we can do
                 * this before the global free/init since our private/global
                 * free/init are completely separate (due to the presence of
                 * persistent state we cannot do a global quick-free anyway).
                 * But when using shared IBL tables, fragment_reset_init() must
                 * be called before fragment_thread_reset_init(), since the
                 * latter copies global state initialized by the former. To
                 * simplify the code, we simply init all global state prior to
                 * initing private state (xref case 8092).
                 */
                heap_thread_reset_init(dcontext);
                vm_areas_thread_reset_init(dcontext);
                monitor_thread_reset_init(dcontext);
                fcache_thread_reset_init(dcontext);
                fragment_thread_reset_init(dcontext);
            }
        }
    }

    /* we assume these are atomic and need no locks */
    dynamo_resetting = false;
    /* new resets will now queue up on all_threads_synch_lock */
    reset_in_progress = false;

    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    DOLOG(1, LOG_STATS, {
        LOG(GLOBAL, LOG_STATS, 1, "\n**************************Stats AFTER reset:\n");
        dump_global_stats(false);
    });

    LOG(GLOBAL, LOG_CACHE, 2,
        "fcache_reset_all_caches_proactively: resuming all threads\n");
    end_synch_with_all_threads(threads, num_threads, true /*resume*/);
}

/* returns true if specified target wasn't already scheduled for reset */
bool
schedule_reset(uint target)
{
    bool added_target;
    ASSERT(target != 0);
    if (!DYNAMO_OPTION(enable_reset))
        return false;
    d_r_mutex_lock(&reset_pending_lock);
    added_target = !TESTALL(target, reset_pending);
    reset_pending |= target;
    d_r_mutex_unlock(&reset_pending_lock);
    return added_target;
}

#if 0  /* currently not used see note in fcache_low_on_memory */
static void
fcache_reset_cache(dcontext_t *dcontext, fcache_t *cache)
{
    fcache_unit_t *u, *prevu;
    fragment_t *f;
    cache_pc pc, last_pc;
    bool unit_empty;
    uint num_units = 0;
    uint sz;

    /* FIXME: this is called when low on memory: safe to grab lock? */
    PROTECT_CACHE(cache, lock);

    LOG(THREAD, LOG_CACHE, 2, "fcache_reset_cache %s\n", cache->name);
    /* we need to free entire units, so don't walk FIFO */
    for (u = cache->units; u != NULL; u = u->next_local) {
        LOG(THREAD, LOG_CACHE, 3, "  unit %d: "PFX" -> cur "PFX"\n",
            num_units, u->start_pc, u->cur_pc);
        num_units++;
        /* try to delete everybody we can */
        unit_empty = true;
        pc = u->start_pc;
        last_pc = pc;
        while (pc < u->cur_pc) {
            LOG(THREAD, LOG_CACHE, 4, "  f @ "PFX"\n", pc);
            f = *((fragment_t **)pc);
            ASSERT(f != NULL);
            ASSERT(FIFO_UNIT(f) == u);
            /* go to contiguously-next fragment_t in cache */
            sz = FRAG_SIZE(f);
            /* FIXME: do we still need the notion of undeletable fragments?
             * Should do an analysis and see if we ever use it anymore.
             * It is a powerful feature to support, but also a limiting one...
             */
            if (TEST(FRAG_CANNOT_DELETE, f->flags)) {
                unit_empty = false;
                /* FIXME: this allocates memory for the empty_slot_t data struct! */
                fifo_prepend_empty(dcontext, cache, u, NULL, last_pc, pc - last_pc);
                STATS_FCACHE_SUB(cache, used, (pc - last_pc));
                last_pc = pc + sz;
            } else {
                /* FIXME: in low-memory situation, will we have problem
                 * with the future fragment that will be created?
                 * Even worse, what if it triggers a resize of its hashtable?
                 * Since we're deleting everyone, we should set some flag saying
                 * "don't create any future fragment for this deleted fragment"
                 */
                force_fragment_from_cache(dcontext, cache, f);
            }
            pc += sz;
        }
        if (last_pc < u->cur_pc) {
            fifo_prepend_empty(dcontext, cache, u, NULL, last_pc, pc - last_pc);
            STATS_FCACHE_SUB(cache, used, (pc - last_pc));
        }
        if (unit_empty) {
            /* hack to indicate empty -- we restore end_pc in next loop */
            u->end_pc = NULL;
        }
    }

    for (prevu = NULL, u = cache->units; u != NULL; prevu = u, u = u->next_local) {
        if (u->end_pc == NULL) {
            u->end_pc = u->start_pc + u->size;
            /* have to leave one unit */
            if (num_units > 1) {
                num_units--;
                fcache_free_unit(dcontext, u, true);
                if (prevu == NULL)
                    u = cache->units;
                else
                    u = prevu->next_local;
            }
        }
    }

    /* FIXME: try to shrink remaining unit(s)?  Would we do that by freeing
     * just the tail of the unit?
     */
    PROTECT_CACHE(cache, unlock);
}
#endif /* #if 0 */

/* This routine has to assume it cannot allocate memory.
 * Always safe to free free list (using lock).
 * Other allocations must be freed only in the current thread:
 * cannot get list of all threads for a global approach b/c that requires memory.
 * We let the other threads trip over the low memory trigger to flush their own
 * caches.
 */
void
fcache_low_on_memory()
{
    fcache_unit_t *u, *next_u;
    DEBUG_DECLARE(size_t freed = 0;)

#if 0
    dcontext_t *dcontext = get_thread_private_dcontext();
    fcache_thread_units_t *tu = (fcache_thread_units_t *) dcontext->fcache_field;
    /* FIXME: we cannot reset the cache at arbitrary points -- and we can
     * be called at any alloc point!  If in middle of fragment creation,
     * we can't just go delete the fragment!
     * STRATEGY:
     * keep a reserved piece of heap per thread that's big enough to get to
     * a safe point from any DR allocation site (perhaps it should use
     * a stack allocator).   We keep going, using that for memory
     * (have to work out shared vs private memory issues if building a
     * shared bb), and then at a safe point we reset the cache.
     */
    /* free the current thread's entire cache, leaving one empty unit */
    if (tu->bb != NULL)
        fcache_reset_cache(dcontext, tu->bb);
    if (tu->trace != NULL)
        fcache_reset_cache(dcontext, tu->trace);
#endif

    /* now free the entire dead list (including thread units just moved here) */
    LOG(GLOBAL, LOG_CACHE | LOG_STATS, 1,
        "fcache_low_on_memory: about to free dead list units\n");
    /* WARNING: this routine is called at arbitrary allocation failure points,
     * so we have to be careful what locks we grab.
     * No allocation site can hold a lock weaker in rank than heap_unit_lock,
     * b/c it could deadlock on the allocation itself! So we only have to worry
     * about the locks of rank between heap_alloc_lock and allunits_lock --
     * currently dynamo_areas, fcache_unit_areas and global_alloc_lock. We
     * check for those locks here.  FIXME we have no way to check if holding
     * a readlock on the dynamo/fcache_unit_areas lock.  FIXME owning the
     * dynamo_areas lock here is prob. not that uncommon, we may be able to
     * release and re-grab it but would have to be sure that works in all the
     * corner cases (if the failing alloc is for a dynamo_areas vector resize
     * etc.).
     */
    if (lockwise_safe_to_allocate_memory() && !self_owns_dynamo_vm_area_lock() &&
        !self_owns_write_lock(&fcache_unit_areas->lock)) {
        d_r_mutex_lock(&allunits_lock);
        u = allunits->dead;
        while (u != NULL) {
            next_u = u->next_global;
            IF_DEBUG(freed += u->size;)
            fcache_really_free_unit(u, true /*on dead list*/, true /*dealloc*/);
            u = next_u;
        }
        allunits->dead = NULL;
        d_r_mutex_unlock(&allunits_lock);
        LOG(GLOBAL, LOG_CACHE | LOG_STATS, 1, "fcache_low_on_memory: freed %d KB\n",
            freed / 1024);
    } else {
        LOG(GLOBAL, LOG_CACHE | LOG_STATS, 1,
            "fcache_low_on_memory: cannot walk units b/c of deadlock potential\n");
    }

    options_make_writable();
    /* be more aggressive about not resizing cache
     * FIXME: I just made this up -- have param to control?
     * FIXME: restore params back to original values at some point?
     */
    if (dynamo_options.finite_bb_cache && dynamo_options.cache_bb_replace > 0) {
        dynamo_options.cache_bb_regen *= 2;
        if (dynamo_options.cache_bb_regen > dynamo_options.cache_bb_replace)
            dynamo_options.cache_bb_regen = 4 * dynamo_options.cache_bb_replace / 5;
    }
    if (dynamo_options.finite_shared_bb_cache &&
        dynamo_options.cache_shared_bb_replace > 0) {
        dynamo_options.cache_shared_bb_regen *= 2;
        if (dynamo_options.cache_shared_bb_regen >
            dynamo_options.cache_shared_bb_replace) {
            dynamo_options.cache_shared_bb_regen =
                4 * dynamo_options.cache_shared_bb_replace / 5;
        }
    }
    if (dynamo_options.finite_shared_trace_cache &&
        dynamo_options.cache_shared_trace_replace > 0) {
        dynamo_options.cache_shared_trace_regen *= 2;
        if (dynamo_options.cache_shared_trace_regen >
            dynamo_options.cache_shared_trace_replace) {
            dynamo_options.cache_shared_trace_regen =
                4 * dynamo_options.cache_shared_trace_replace / 5;
        }
    }
    /* FIXME: be more or less aggressive about traces than bbs?
     * could get rid of trace cache altogether...
     */
    if (dynamo_options.finite_trace_cache && dynamo_options.cache_trace_replace > 0) {
        dynamo_options.cache_trace_regen *= 2;
        if (dynamo_options.cache_trace_regen > dynamo_options.cache_trace_replace)
            dynamo_options.cache_trace_regen = 4 * dynamo_options.cache_trace_replace / 5;
    }
    options_restore_readonly();
}

/***************************************************************************
 * COARSE-GRAIN UNITS
 */

/* Returns NULL if pc is not an address contained in a coarse fcache unit */
coarse_info_t *
get_fcache_coarse_info(cache_pc pc)
{
    fcache_unit_t *unit = fcache_lookup_unit(pc);
    if (unit == NULL || unit->cache == NULL)
        return NULL;
    ASSERT((unit->cache->is_coarse && unit->cache->coarse_info != NULL) ||
           (!unit->cache->is_coarse && unit->cache->coarse_info == NULL));
    return unit->cache->coarse_info;
}

void
fcache_coarse_cache_delete(dcontext_t *dcontext, coarse_info_t *info)
{
    fcache_t *cache;
    ASSERT(info != NULL);
    ASSERT_OWN_MUTEX(!info->is_local, &info->lock);
    cache = (fcache_t *)info->cache;
    if (cache == NULL) /* lazily initialized, so common to have empty units */
        return;
    /* We don't PROTECT_CACHE(cache, lock) to avoid rank order w/ coarse info lock.
     * We assume that deletion can only happen for local cache or at reset/exit.
     */
    DODEBUG({ cache->is_local = true; });
    fcache_cache_free(dcontext, cache, !info->frozen /*do not free frozen unit*/);
    info->cache = NULL;
    /* We unlink any outgoing links by walking the stubs, not walking the units,
     * so nothing else to do here
     */
}

/* Returns an upper bound on the size needed for the cache if info is frozen. */
size_t
coarse_frozen_cache_size(dcontext_t *dcontext, coarse_info_t *info)
{
    fcache_t *cache;
    ASSERT(info != NULL);
    ASSERT_OWN_MUTEX(true, &info->lock);
    cache = (fcache_t *)info->cache;
    if (cache == NULL)
        return 0;
    /* we ignore any shrinking from eliding fall-through ubrs
     * or conversion to 8-bit-jmps
     */
    /* cache->size is simply committed size, so subtract unused committed
     * at end of last unit; we ignore small unused space at end of each unit.
     */
    return cache->size - (cache->units->end_pc - cache->units->cur_pc);
}

/* Assumes that no cache lock is needed because info is newly created
 * and unknown to all but this thread.
 */
void
fcache_coarse_init_frozen(dcontext_t *dcontext, coarse_info_t *info, cache_pc start_pc,
                          size_t size)
{
    fcache_t *cache = fcache_cache_init(GLOBAL_DCONTEXT, FRAG_SHARED | FRAG_COARSE_GRAIN,
                                        false /*no initial unit*/);
    /* We don't PROTECT_CACHE(cache, lock) to avoid rank order w/ coarse info lock,
     * assuming that info is newly created and unknown to all but this thread.
     * (For freezing we also have dynamo_all_threads_synched, but we don't for
     * loading in persisted caches.)
     */
    DODEBUG({ cache->is_local = true; });
    cache->units = fcache_create_unit(dcontext, cache, start_pc, size);
    DODEBUG({ cache->is_local = false; });
    cache->units->cur_pc = cache->units->end_pc;
    cache->units->full = true;
    cache->coarse_info = info;
    info->cache = cache;
}

/* Used when swapping info structs for in-place freezing */
void
fcache_coarse_set_info(dcontext_t *dcontext, coarse_info_t *info)
{
    fcache_t *cache;
    ASSERT(info != NULL);
    ASSERT_OWN_MUTEX(true, &info->lock);
    cache = (fcache_t *)info->cache;
    cache->coarse_info = info;
}
