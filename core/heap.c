/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * heap.c - heap manager
 */

#include "globals.h"
#include <limits.h>

#include "fragment.h" /* for struct sizes */
#include "link.h"     /* for struct sizes */
#include "instr.h"    /* for struct sizes */
#include "fcache.h"   /* fcache_low_on_memory */
#ifdef UNIX
#    include "memquery.h"
#endif
#ifdef DEBUG
#    include "hotpatch.h" /* To handle leak for case 9593. */
#endif
#include "instrument.h"

#ifdef HEAP_ACCOUNTING
#    ifndef DEBUG
#        error HEAP_ACCOUNTING requires DEBUG
#    endif
#endif

#ifdef DEBUG_MEMORY
/* on by default but higher than general asserts */
#    define CHKLVL_MEMFILL CHKLVL_DEFAULT
#endif

extern bool vm_areas_exited;

/***************************************************************************
 * we cannot use malloc in the middle of interpreting the client program
 * because we could be in the middle of interpreting malloc, which is not
 * always reentrant
 *
 * We have a virtual memory manager which makes sure memory is
 * reserved within the application address space so that we don't have
 * to fight with the application.  We call os_heap_reserve to allocate
 * virtual space in a single consecutive region.  We later use
 * os_heap_commit to get committed memory in large chunks and manage
 * the chunks using a simple scheme of free lists of different sizes.
 * The virtual memory manager has to store out of band information about
 * used and free blocks, since of course there is no real memory to use.
 * The chunks (heap units) store in band extra information both for
 * used and free.  However, in the allocated blocks within a unit we
 * don't need to store any information since heap_free passes in the
 * size; we store the next pointers for the free lists at the start of
 * the free blocks themselves.  We have one large reservation for most of
 * our allocations, and yet another for allocations that we do not
 * plan on ever freeing up on detach - the only unavoidable tombstones
 * are those for thread private code system calls that may be stuck on
 * callbacks.  In case we run out of reserved memory we do fall back
 * on requests from the OS, but any of these may fail if we are
 * competing with the application.
 *
 * looking at dynamo behavior as of Jan 2001, most heap_alloc requests are
 * for < 128 bytes, very few for larger, so we have a bunch of fixed-size
 * blocks of small sizes
 *
 * the UINT_MAX size is a variable-length block, we keep one byte to store
 * the size (again storing the next pointer when free at the start of
 * what we pass to the user)
 */

static const uint BLOCK_SIZES[] = {
    8, /* for instr bits */
#ifndef X64
    /* for x64 future_fragment_t is 24 bytes (could be 20 if we could put flags last) */
    sizeof(future_fragment_t), /* 12 (24 x64) */
#endif
    /* we have a lot of size 16 requests for IR but they are transient */
    24, /* fcache empties and vm_area_t are now 20, vm area extras still 24 */
    /* 40 dbg / 36 rel: */
    ALIGN_FORWARD(sizeof(fragment_t) + sizeof(indirect_linkstub_t), HEAP_ALIGNMENT),
#if defined(X64)
#    ifdef DEBUG
    sizeof(fragment_t) + sizeof(direct_linkstub_t) +
        sizeof(cbr_fallthrough_linkstub_t), /* 112 dbg x64 / 104 rel x64 */
#    else
    sizeof(instr_t), /* 112 x64 */
#    endif
#else
    sizeof(fragment_t) + sizeof(direct_linkstub_t) +
        sizeof(cbr_fallthrough_linkstub_t), /* 60 dbg / 56 rel */
#    ifndef DEBUG
    sizeof(instr_t),                        /* 72 */
#    endif
#endif
    /* we keep this bucket even though only 10% or so of normal bbs
     * hit this.
     */
    ALIGN_FORWARD(sizeof(fragment_t) + 2 * sizeof(direct_linkstub_t),
                  HEAP_ALIGNMENT), /* 68 dbg / 64 rel (128 x64) */
    ALIGN_FORWARD(sizeof(trace_t) + 2 * sizeof(direct_linkstub_t) + sizeof(uint),
                  HEAP_ALIGNMENT), /* 80 dbg / 76 rel (148 x64 => 152) */
    /* FIXME: measure whether should put in indirect mixes as well */
    ALIGN_FORWARD(sizeof(trace_t) + 3 * sizeof(direct_linkstub_t) + sizeof(uint),
                  HEAP_ALIGNMENT), /* 96 dbg / 92 rel (180 x64 => 184) */
    ALIGN_FORWARD(sizeof(trace_t) + 5 * sizeof(direct_linkstub_t) + sizeof(uint),
                  HEAP_ALIGNMENT), /* 128 dbg / 124 rel (244 x64 => 248) */
    256, 512, UINT_MAX             /* variable-length */
};
#define BLOCK_TYPES (sizeof(BLOCK_SIZES) / sizeof(uint))

#ifdef DEBUG
/* FIXME: would be nice to have these stats per HEAPACCT category */
/* These are ints only b/c we used to do non-atomic adds and wanted to
 * gracefully handle underflow to negative values
 */
DECLARE_NEVERPROT_VAR(static int block_total_count[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_count[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_peak_count[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_wasted[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_peak_wasted[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_align_pad[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static int block_peak_align_pad[BLOCK_TYPES], { 0 });
DECLARE_NEVERPROT_VAR(static bool out_of_vmheap_once, false);
#endif

/* variable-length: we steal one int for the size */
#define HEADER_SIZE (sizeof(size_t))
/* VARIABLE_SIZE is assignable */
#define VARIABLE_SIZE(p) (*(size_t *)((p)-HEADER_SIZE))
#define MEMSET_HEADER(p, value) VARIABLE_SIZE(p) = HEAP_TO_PTR_UINT(value)
#define GET_VARIABLE_ALLOCATION_SIZE(p) (VARIABLE_SIZE(p) + HEADER_SIZE)

/* The heap is allocated in units.
 * We start out with a small unit. Then each additional unit we
 * need doubles in size, up to a maximum.
 * We keep the initial units small for thread-private heaps, since with
 * thousands of threads the space can add up.
 */
#define HEAP_UNIT_MIN_SIZE DYNAMO_OPTION(initial_heap_unit_size)
#define HEAP_UNIT_MAX_SIZE INTERNAL_OPTION(max_heap_unit_size)
#define GLOBAL_UNIT_MIN_SIZE DYNAMO_OPTION(initial_global_heap_unit_size)

#define GUARD_PAGE_ADJUSTMENT (dynamo_options.guard_pages ? 2 * PAGE_SIZE : 0)

/* gets usable space in the unit */
#define UNITROOM(u) ((size_t)(u->end_pc - u->start_pc))
#define UNIT_RESERVED_ROOM(u) (u->reserved_end_pc - u->start_pc)
/* we keep the heap_unit_t header at top of the unit, this macro calculates
 * the committed size of the unit by adding header size to available size
 */
#define UNIT_COMMIT_SIZE(u) (UNITROOM(u) + sizeof(heap_unit_t))
#define UNIT_RESERVED_SIZE(u) (UNIT_RESERVED_ROOM(u) + sizeof(heap_unit_t))
#define UNIT_ALLOC_START(u) (u->start_pc - sizeof(heap_unit_t))
#define UNIT_GET_START_PC(u) (byte *)(((ptr_uint_t)u) + sizeof(heap_unit_t))
#define UNIT_COMMIT_END(u) (u->end_pc)
#define UNIT_RESERVED_END(u) (u->reserved_end_pc)

/* Gets the allocated size of the unit (reserved size; doesn't include guard pages
 * as those are not considered part of the usable space).
 */
#define UNITALLOC(u) (UNIT_RESERVED_SIZE(u))
/* Gets unit overhead: includes reserved and committed (sizeof(heap_unit_t)) portions. */
#define UNITOVERHEAD sizeof(heap_unit_t)

/* any alloc request larger than this needs a special unit */
#define MAXROOM (HEAP_UNIT_MAX_SIZE - UNITOVERHEAD)

/* maximum valid allocation (to guard against internal integer overflows) */
#define MAX_VALID_HEAP_ALLOCATION INT_MAX

/* thread-local heap structure
 * this struct is kept at top of unit itself, not in separate allocation
 */
typedef struct _heap_unit_t {
    heap_pc start_pc;        /* start address of heap storage */
    heap_pc end_pc;          /* open-ended end address of heap storage */
    heap_pc cur_pc;          /* open-ended current end of allocated storage */
    heap_pc reserved_end_pc; /* open-ended end of reserved (not nec committed) memory */
    bool in_vmarea_list;     /* perf opt for delayed batch vmarea updating */
    which_vmm_t which;
#ifdef DEBUG
    int id; /* # of this unit */
#endif
    struct _heap_unit_t *next_local;  /* used to link thread's units */
    struct _heap_unit_t *next_global; /* used to link all units */
    struct _heap_unit_t *prev_global; /* used to link all units */
} heap_unit_t;

#ifdef HEAP_ACCOUNTING
typedef struct _heap_acct_t {
    size_t alloc_reuse[ACCT_LAST];
    size_t alloc_new[ACCT_LAST];
    size_t cur_usage[ACCT_LAST];
    size_t max_usage[ACCT_LAST];
    size_t max_single[ACCT_LAST];
    uint num_alloc[ACCT_LAST];
} heap_acct_t;
#endif

/* FIXME (case 6336): rename to heap_t:
 *   a heap_t is a collection of units with the same properties
 * to reflect that this is used for more than just thread-private memory.
 * Also rename the "tu" vars to "h"
 */
typedef struct _thread_units_t {
    heap_unit_t *top_unit; /* start of linked list of heap units */
    heap_unit_t *cur_unit; /* current unit in heap list */
    heap_pc free_list[BLOCK_TYPES];
#ifdef DEBUG
    int num_units; /* total # of heap units */
#endif
    dcontext_t *dcontext; /* back pointer to owner */
    which_vmm_t which;
    bool writable; /* remember state of heap protection */
#ifdef HEAP_ACCOUNTING
    heap_acct_t acct;
#endif
} thread_units_t;

#define REACHABLE_HEAP() (IF_X64_ELSE(DYNAMO_OPTION(reachable_heap), true))

/* per-thread structure: */
typedef struct _thread_heap_t {
    thread_units_t *local_heap;
    /* We separate out heap memory used for fragments, linking, and vmarea multi-entries
     * both to enable resetting memory and for safety for unlink flushing in the presence
     * of clean calls out of the cache that might allocate IR memory (which does not
     * use nonpersistent heap).  Any client actions that involve fragments or linking
     * should require couldbelinking status, which makes them safe wrt unlink flushing.
     * Xref DrMi#1791.
     */
    thread_units_t *nonpersistent_heap;
    thread_units_t *reachable_heap; /* Only used if !REACHABLE_HEAP() */
#ifdef UNIX
    /* Used for -satisfy_w_xor_x. */
    heap_pc fork_copy_start;
    size_t fork_copy_size;
    vm_area_vector_t *fork_copy_areas;
#endif
} thread_heap_t;

/* global, unique thread-shared structure:
 * FIXME: give this name to thread_units_t, and name this AllHeapUnits
 */
typedef struct _heap_t {
    heap_unit_t *units; /* list of all allocated units */
    heap_unit_t *dead;  /* list of deleted units ready for re-allocation */
    /* FIXME: num_dead duplicates d_r_stats->heap_num_free, but we want num_dead
     * for release build too, so it's separate...can we do better?
     */
    uint num_dead;
} heap_t;

/* no synch needed since only written once */
static bool heap_exiting = false;

#ifdef DEBUG
DECLARE_NEVERPROT_VAR(static bool ever_beyond_vmm, false);
#endif

/* Lock used only for managing heap units, not for normal thread-local alloc.
 * Must be recursive due to circular dependencies between vmareas and global heap.
 * Furthermore, always grab dynamo_vm_areas_lock() before grabbing this lock,
 * to make DR areas update and heap alloc/free atomic!
 */
DECLARE_CXTSWPROT_VAR(static recursive_lock_t heap_unit_lock,
                      INIT_RECURSIVE_LOCK(heap_unit_lock));
/* N.B.: if these two locks are ever owned at the same time, the convention is
 * that global_alloc_lock MUST be grabbed first, to avoid deadlocks
 */
/* separate lock for global heap access to avoid contention between local unit
 * creation and global heap alloc
 * must be recursive so that heap_vmareas_synch_units can hold it and heap_unit_lock
 * up front to avoid deadlocks, and still allow vmareas to global_alloc --
 * BUT we do NOT want global_heap_alloc() to be able to recurse!
 * FIXME: either find a better solution to the heap_vmareas_synch_units deadlock
 * that is as efficient, or find a way to assert that the only recursion is
 * from heap_vmareas_synch_units to global_alloc
 */
DECLARE_CXTSWPROT_VAR(static recursive_lock_t global_alloc_lock,
                      INIT_RECURSIVE_LOCK(global_alloc_lock));

/* Used to sync low on memory event */
DECLARE_CXTSWPROT_VAR(static recursive_lock_t low_on_memory_pending_lock,
                      INIT_RECURSIVE_LOCK(low_on_memory_pending_lock));

/* Denotes whether or not low on memory event requires triggering. */
DECLARE_FREQPROT_VAR(bool low_on_memory_pending, false);

#if defined(DEBUG) && defined(HEAP_ACCOUNTING) && defined(HOT_PATCHING_INTERFACE)
static int
get_special_heap_header_size(void);
#endif

vm_area_vector_t *landing_pad_areas; /* PR 250294 */
#ifdef WINDOWS
/* i#939: we steal space from ntdll's +rx segment */
static app_pc lpad_temp_writable_start;
static size_t lpad_temp_writable_size;
static void
release_landing_pad_mem(void);
#endif

/* Indicates whether should back out of a global alloc/free and grab the
 * DR areas lock first, to retry
 */
static bool
safe_to_allocate_or_free_heap_units()
{
    return ((!self_owns_recursive_lock(&global_alloc_lock) &&
             !self_owns_recursive_lock(&heap_unit_lock)) ||
            self_owns_dynamo_vm_area_lock());
}

/* indicates a dynamo vm area remove was delayed
 * protected by the heap_unit_lock
 */
DECLARE_FREQPROT_VAR(static bool dynamo_areas_pending_remove, false);

#ifdef HEAP_ACCOUNTING
const char *whichheap_name[] = {
    /* max length for aligned output is length of "BB Fragments" */
    "BB Fragments",
    "Coarse Links",
    "Future Frag",
    "Frag Tables",
    "IBL Tables",
    "Traces",
    "FC Empties",
    "Vm Multis",
    "IR",
    "RCT Tables",
    "VM Areas",
    "Symbols",
#    ifdef SIDELINE
    "Sideline",
#    endif
    "TH Counter",
    "Tombstone",
    "Hot Patching",
    "Thread Mgt",
    "Memory Mgt",
    "Stats",
    "SpecialHeap",
    "Client",
    "Lib Dup",
    "Clean Call",
    /* NOTE: Add your heap name here */
    "Other",
};

/* Since using a lock for these stats adds a lot of contention, we
 * follow a two-pronged strategy:
 * 1) For accurate stats we add a thread's final stats to the global only
 * when it is cleaned up.  But, this prevents global stats from being
 * available in the middle of a run or if a run is not cleaned up nicely.
 * 2) We have a set of heap_accounting stats for incremental global stats
 * that are available at any time, yet racy and so may be off a little.
 */
/* all set to 0 is only initialization we need */
DECLARE_NEVERPROT_VAR(static thread_units_t global_racy_units, { 0 });

/* macro to get the type abstracted */
#    define ACCOUNT_FOR_ALLOC_HELPER(type, tu, which, alloc_sz, ask_sz)    \
        do {                                                               \
            (tu)->acct.type[which] += alloc_sz;                            \
            (tu)->acct.num_alloc[which]++;                                 \
            (tu)->acct.cur_usage[which] += alloc_sz;                       \
            if ((tu)->acct.cur_usage[which] > (tu)->acct.max_usage[which]) \
                (tu)->acct.max_usage[which] = (tu)->acct.cur_usage[which]; \
            if (ask_sz > (tu)->acct.max_single[which])                     \
                (tu)->acct.max_single[which] = ask_sz;                     \
        } while (0)

#    define ACCOUNT_FOR_ALLOC(type, tu, which, alloc_sz, ask_sz)                         \
        do {                                                                             \
            STATS_ADD_PEAK(heap_claimed, alloc_sz);                                      \
            ACCOUNT_FOR_ALLOC_HELPER(type, tu, which, alloc_sz, ask_sz);                 \
            ACCOUNT_FOR_ALLOC_HELPER(type, &global_racy_units, which, alloc_sz, ask_sz); \
        } while (0)

#    define ACCOUNT_FOR_FREE(tu, which, size)                \
        do {                                                 \
            STATS_SUB(heap_claimed, (size));                 \
            (tu)->acct.cur_usage[which] -= size;             \
            global_racy_units.acct.cur_usage[which] -= size; \
        } while (0)

#else
#    define ACCOUNT_FOR_ALLOC(type, tu, which, alloc_sz, ask_sz)
#    define ACCOUNT_FOR_FREE(tu, which, size)
#endif

typedef byte *vm_addr_t;

#ifdef X64
/* designates the closed interval within which we must allocate DR heap space */
static byte *heap_allowable_region_start = (byte *)PTR_UINT_0;
static byte *heap_allowable_region_end = (byte *)POINTER_MAX;

/* In standalone mode we do not guarantee 32-bit reachability for anything.
 * This lets apps grow beyond 4G of heap.
 */
#    define HEAP_REACHABILITY_ENABLED() (!standalone_library)

/* Used only to protect read/write access to the must_reach_* static variables
 * used in request_region_be_heap_reachable().
 */
DECLARE_CXTSWPROT_VAR(static mutex_t request_region_be_heap_reachable_lock,
                      INIT_LOCK_FREE(request_region_be_heap_reachable_lock));

/* Initialize so will be overridden on first call; protected by the
 * request_region_be_heap_reachable_lock.
 */
static byte *must_reach_region_start = (byte *)POINTER_MAX;
static byte *must_reach_region_end = (byte *)PTR_UINT_0; /* closed */

static void
reset_heap_reachable_bounds(void)
{
    heap_allowable_region_start = (byte *)PTR_UINT_0;
    heap_allowable_region_end = (byte *)POINTER_MAX;
    must_reach_region_start = (byte *)POINTER_MAX;
    must_reach_region_end = (byte *)PTR_UINT_0; /* closed */
}

/* Request that the supplied region be 32bit offset reachable from the DR heap.  Should
 * be called before vmm_heap_init() so we can place the DR heap to meet these constraints.
 * Can also be called post vmm_heap_init() but at that point acts as an assert that the
 * supplied region is reachable since the heap is already reserved.
 *
 * Must be called at least once up front, for the -heap_in_lower_4GB code here
 * to kick in!
 */
void
request_region_be_heap_reachable(byte *start, size_t size)
{
    if (!HEAP_REACHABILITY_ENABLED())
        return;

    LOG(GLOBAL, LOG_HEAP, 2,
        "Adding must-be-reachable-from-heap region " PFX "-" PFX "\n"
        "Existing must-be-reachable region " PFX "-" PFX "\n"
        "Existing allowed range " PFX "-" PFX "\n",
        start, start + size, must_reach_region_start, must_reach_region_end,
        heap_allowable_region_start, heap_allowable_region_end);
    ASSERT(!POINTER_OVERFLOW_ON_ADD(start, size));
    ASSERT(size > 0);

    d_r_mutex_lock(&request_region_be_heap_reachable_lock);
    if (start < must_reach_region_start) {
        byte *allowable_end_tmp;
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        must_reach_region_start = start;
        allowable_end_tmp =
            REACHABLE_32BIT_END(must_reach_region_start, must_reach_region_end);
        /* PR 215395 - add in absolute address reachability */
        if (DYNAMO_OPTION(heap_in_lower_4GB) &&
            allowable_end_tmp > (byte *)POINTER_MAX_32BIT) {
            allowable_end_tmp = (byte *)POINTER_MAX_32BIT;
        }
        /* Write assumed to be atomic so we don't have to hold a lock to use
         * heap_allowable_region_end. */
        heap_allowable_region_end = allowable_end_tmp;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
    if (start + size - 1 > must_reach_region_end) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        must_reach_region_end = start + size - 1; /* closed */
        /* Write assumed to be atomic so we don't have to hold a lock to use
         * heap_allowable_region_start. */
        heap_allowable_region_start =
            REACHABLE_32BIT_START(must_reach_region_start, must_reach_region_end);
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
    ASSERT(must_reach_region_start <= must_reach_region_end); /* correctness check */
    /* verify can be addressed absolutely (if required), correctness check */
    ASSERT(!DYNAMO_OPTION(heap_in_lower_4GB) ||
           heap_allowable_region_end <= (byte *)POINTER_MAX_32BIT);
    d_r_mutex_unlock(&request_region_be_heap_reachable_lock);

    LOG(GLOBAL, LOG_HEAP, 1,
        "Added must-be-reachable-from-heap region " PFX "-" PFX "\n"
        "New must-be-reachable region " PFX "-" PFX "\n"
        "New allowed range " PFX "-" PFX "\n",
        start, start + size, must_reach_region_start, must_reach_region_end,
        heap_allowable_region_start, heap_allowable_region_end);

    /* Reachability checks (xref PR 215395, note since we currently can't directly
     * control where DR/client dlls are loaded these could fire if rebased). */
    ASSERT(heap_allowable_region_start <= must_reach_region_start &&
           "x64 reachability contraints not satisfiable");
    ASSERT(must_reach_region_end <= heap_allowable_region_end &&
           "x64 reachability contraints not satisfiable");

    /* Handle release build failure. */
    if (heap_allowable_region_start > must_reach_region_start ||
        must_reach_region_end > heap_allowable_region_end) {
        /* FIXME - in a released product we may want to detach or something else less
         * drastic than triggering a FATAL_USAGE_ERROR. */
        FATAL_USAGE_ERROR(HEAP_CONTRAINTS_UNSATISFIABLE, 2, get_application_name(),
                          get_application_pid());
    }
}

void
vmcode_get_reachable_region(byte **region_start OUT, byte **region_end OUT)
{
    /* We track sub-page for more accuracy on additional constraints, and
     * align when asked about it.
     */
    if (region_start != NULL)
        *region_start = (byte *)ALIGN_FORWARD(heap_allowable_region_start, PAGE_SIZE);
    if (region_end != NULL)
        *region_end = (byte *)ALIGN_BACKWARD(heap_allowable_region_end, PAGE_SIZE);
}
#endif

/* forward declarations of static functions */
static void
threadunits_init(dcontext_t *dcontext, thread_units_t *tu, size_t size, bool reachable);
/* dcontext only used for debugging */
static void
threadunits_exit(thread_units_t *tu, dcontext_t *dcontext);
static void *
common_heap_alloc(thread_units_t *tu, size_t size HEAPACCT(which_heap_t which));
static bool
common_heap_free(thread_units_t *tu, void *p, size_t size HEAPACCT(which_heap_t which));
static void
release_real_memory(void *p, size_t size, bool remove_vm, which_vmm_t which);
static void
release_guarded_real_memory(vm_addr_t p, size_t size, bool remove_vm, bool guarded,
                            which_vmm_t which);

typedef enum {
    /* I - Init, Interop - first allocation failed
     *    check for incompatible kernel drivers
     */
    OOM_INIT = 0x1,
    /* R - Reserve - out of virtual reservation *
     *    increase -vm_size to reserve more memory
     */
    OOM_RESERVE = 0x2,
    /* C - Commit - systemwide page file limit, or current process job limit hit
     * Increase pagefile size, check for memory leak in any application.
     *
     * FIXME: possible automatic actions
     *    if systemwide failure we may want to wait if transient
     *    FIXME: if in a job latter we want to detect and just die
     *    (though after freeing as much memory as we can)
     */
    OOM_COMMIT = 0x4,
    /* E - Extending Commit - same reasons as Commit
     *    as a possible workaround increasing -heap_commit_increment
     *    may make expose us to commit-ing less frequently,
     *    On the other hand committing smaller chunks has a higher
     *    chance of getting through when there is very little memory.
     *
     *    FIXME: not much more informative than OOM_COMMIT
     */
    OOM_EXTEND = 0x8,
} oom_source_t;

static void
report_low_on_memory(which_vmm_t which, oom_source_t source,
                     heap_error_code_t os_error_code);

#define MAX_VMCODE_SIZE (2ULL * 1024 * 1024 * 1024)
#define MAX_VMHEAP_SIZE (IF_X64_ELSE(128ULL, (4ULL - 1)) * 1024 * 1024 * 1024)

/* We should normally have only one large unit, so this is in fact
 * the maximum we should count on in one process
 */

/* minimum will be used only if an invalid option is set */
#define MIN_VMM_HEAP_UNIT_SIZE DYNAMO_OPTION(vmm_block_size)

typedef struct {
    vm_addr_t start_addr;  /* base virtual address */
    vm_addr_t end_addr;    /* noninclusive virtual memory range [start,end) */
    vm_addr_t alloc_start; /* base allocation virtual address */
    size_t alloc_size;     /* allocation size */
    /* for 64-bit do we want to shift to size_t to allow a larger region?
     * if so must update the bitmap_t routines
     */
    uint num_blocks; /* total number of blocks in virtual allocation */

    mutex_t lock; /* write access to the rest of the fields is protected */
    /* We make an assumption about the bitmap_t implementation being
       static therefore we don't grab locks on read accesses.  Anyways,
       currently the bitmap_t is used with no write intent only for ASSERTs. */
    uint num_free_blocks; /* currently free blocks */
    const char *name;
    /* We dynamically allocate the bitmap to allow for different sizes for
     * vmcode and vmheap and to allow for large vmheap sizes.
     * We place it at start_addr, or the writable equivalent for vmcode.
     */
    bitmap_element_t *blocks;
} vm_heap_t;

/* We keep our heap management structs on the heap for selfprot (case 8074).
 * Note that we do have static structs for bootstrapping and we later move
 * the data here.
 */
typedef struct _heap_management_t {
    /* We split our 32-bit-displacement-reachable memory, which is mostly our
     * code cache and thus is called "vmcode", from our heap which can go anywhere,
     * "vmheap".
     * For each, we reserve a single vm_heap_t for guaranteed allocation.
     * We fall back to the OS when run out of reservation space.
     * If REACHABLE_HEAP() we do not use vmheap and put everything in
     * vmcode.
     */
    vm_heap_t vmheap;
    /* We only need a single 32-bit-displacement-reachable region since it cannot
     * be larger than 2G anyway.
     * XXX i#1132: for 64-bit, we make it 2G by default so we never have to
     * fall back to the OS!  We'll have to make room to load client libs inside
     * the VMM-managed space is all.
     * For 32-bit it will have to remain smaller and handle falling back to the OS.
     */
    vm_heap_t vmcode;
    /* A writable mirror of read-only vmcode for -satisfy_w_xor_x. */
    file_t dual_map_file;
    vm_addr_t vmcode_writable_base;
    vm_addr_t vmcode_writable_alloc;
    heap_t heap;
    /* thread-shared heaps: */
    thread_units_t global_units;
    /* Separate non-persistent heap.  See thread_heap_t.nonpersisent_heap comment. */
    thread_units_t global_nonpersistent_units;
    bool global_heap_writable;
    thread_units_t global_unprotected_units;
    thread_units_t global_reachable_units; /* Used if !REACHABLE_HEAP() */
} heap_management_t;

/* For bootstrapping until we can allocate our real heapmgt (case 8074).
 * temp_heapmgt.lock is initialized in vmm_heap_unit_init().
 */
static heap_management_t temp_heapmgt;
static heap_management_t *heapmgt = &temp_heapmgt; /* initial value until alloced */

static bool vmm_heap_exited = false; /* FIXME: used only to thwart stack_free from trying,
                                        should change the interface for the last stack
                                     */

#define MEMORY_FILE_NAME "dynamorio_dual_map"

static vm_addr_t
vmm_heap_reserve_blocks(vm_heap_t *vmh, size_t size_in, byte *base, which_vmm_t which);

static bool
vmm_heap_commit(vm_addr_t p, size_t size, uint prot, heap_error_code_t *error_code,
                which_vmm_t which);

static inline uint
vmm_addr_to_block(vm_heap_t *vmh, vm_addr_t p)
{
    ASSERT(
        CHECK_TRUNCATE_TYPE_uint((p - vmh->start_addr) / DYNAMO_OPTION(vmm_block_size)));
    return (uint)((p - vmh->start_addr) / DYNAMO_OPTION(vmm_block_size));
}

static inline vm_addr_t
vmm_block_to_addr(vm_heap_t *vmh, uint block)
{
    ASSERT(block >= 0 && block < vmh->num_blocks);
    return (vm_addr_t)(vmh->start_addr + block * DYNAMO_OPTION(vmm_block_size));
}

static bool
vmm_in_same_block(vm_heap_t *vmh, vm_addr_t p1, vm_addr_t p2)
{
    return vmm_addr_to_block(vmh, p1) == vmm_addr_to_block(vmh, p2);
}

#if defined(DEBUG) && defined(INTERNAL)
static void
vmm_dump_map(vm_heap_t *vmh)
{
    uint i;
    bitmap_element_t *b = vmh->blocks;
    uint bitmap_size = vmh->num_blocks;
    uint last_i = 0;
    bool is_used = bitmap_test(b, 0) == 0;

    LOG(GLOBAL, LOG_HEAP, 3, "vmm_dump_map(" PFX ")\n", vmh);
    /* We used to do raw dumps but with the shift to 4K blocks, this is just way
     * too big.  We disable but leave the capability to enable one-off use.
     */
    DOLOG(20, LOG_HEAP, {
        dump_buffer_as_bytes(GLOBAL, b,
                             BITMAP_INDEX(bitmap_size) * sizeof(bitmap_element_t),
                             DUMP_RAW | DUMP_ADDRESS);
    });

    LOG(GLOBAL, LOG_HEAP, 1, "\nvmm_dump_map(" PFX ") virtual regions\n", vmh);
#    define VMM_DUMP_MAP_LOG(i, last_i)                                        \
        LOG(GLOBAL, LOG_HEAP, 1, PFX "-" PFX " size=%d %s\n",                  \
            vmm_block_to_addr(vmh, last_i),                                    \
            vmm_block_to_addr(vmh, i - 1) + DYNAMO_OPTION(vmm_block_size) - 1, \
            (i - last_i) * DYNAMO_OPTION(vmm_block_size),                      \
            is_used ? "reserved" : "free");

    for (i = 0; i < bitmap_size; i++) {
        /* start counting at free/used boundaries */
        if (is_used != (bitmap_test(b, i) == 0)) {
            VMM_DUMP_MAP_LOG(i, last_i);
            is_used = (bitmap_test(b, i) == 0);
            last_i = i;
        }
    }
    VMM_DUMP_MAP_LOG(bitmap_size, last_i);
}
#endif /* DEBUG */

static inline void
print_vmh_data(vm_heap_t *vmh, file_t outf)
{
    d_r_mutex_lock(&vmh->lock);
    print_file(outf, "VM heap: addr range " PFX "--" PFX ", # free blocks %d\n",
               vmh->start_addr, vmh->end_addr, vmh->num_free_blocks);
    d_r_mutex_unlock(&vmh->lock);
}

void
print_vmm_heap_data(file_t outf)
{
    if (heapmgt->vmheap.start_addr != NULL)
        print_vmh_data(&heapmgt->vmheap, outf);
    if (heapmgt->vmcode.start_addr != NULL)
        print_vmh_data(&heapmgt->vmcode, outf);
}

static inline void
vmm_heap_initialize_unusable(vm_heap_t *vmh)
{
    vmh->start_addr = vmh->end_addr = NULL;
    vmh->num_free_blocks = vmh->num_blocks = 0;
}

static void
report_w_xor_x_fatal_error_and_exit(void)
{
    REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_SATISFY_W_XOR_X, 2, get_application_name(),
                                get_application_pid());
    ASSERT_NOT_REACHED();
}

static void
vmm_place_vmcode(vm_heap_t *vmh, size_t size, heap_error_code_t *error_code)
{
    ptr_uint_t preferred = 0;
#ifdef X64
    /* -heap_in_lower_4GB takes top priority and has already set heap_allowable_region_*.
     * Next comes -vm_base_near_app.  It will fail for -vm_size=2G, which we document.
     */
    if (DYNAMO_OPTION(vm_base_near_app)) {
        /* Required for STATIC_LIBRARY: must be near app b/c clients are there.
         * Non-static: still a good idea for fewer rip-rel manglings.
         * Asking for app base means we'll prefer before the app, which
         * has less of an impact on its heap.
         */
        app_pc app_base = get_application_base();
        app_pc app_end = get_application_end();
        /* To avoid ignoring -vm_base and -vm_max_offset we fall through to that
         * code if the app base is near -vm_base.
         */
        if (!REL32_REACHABLE(app_base, (app_pc)DYNAMO_OPTION(vm_base)) ||
            !REL32_REACHABLE(app_base,
                             (app_pc)DYNAMO_OPTION(vm_base) +
                                 DYNAMO_OPTION(vm_max_offset)) ||
            ((app_pc)DYNAMO_OPTION(vm_base) < app_end &&
             (app_pc)DYNAMO_OPTION(vm_base) + DYNAMO_OPTION(vm_max_offset) > app_base)) {
            byte *reach_base = MAX(REACHABLE_32BIT_START(app_base, app_end),
                                   heap_allowable_region_start);
            byte *reach_end =
                MIN(REACHABLE_32BIT_END(app_base, app_end), heap_allowable_region_end);
            if (reach_base < reach_end) {
                size_t add_for_align = DYNAMO_OPTION(vmm_block_size);
                if (DYNAMO_OPTION(vmm_block_size) == PAGE_SIZE) {
                    /* No need for extra space for alignment. */
                    add_for_align = 0;
                }
                vmh->alloc_start = os_heap_reserve_in_region(
                    (void *)ALIGN_FORWARD(reach_base, PAGE_SIZE),
                    (void *)ALIGN_BACKWARD(reach_end, PAGE_SIZE), size + add_for_align,
                    error_code, true /*+x*/);
                if (vmh->alloc_start != NULL) {
                    vmh->start_addr = (heap_pc)ALIGN_FORWARD(
                        vmh->alloc_start, DYNAMO_OPTION(vmm_block_size));
                    if (add_for_align == 0) {
                        ASSERT(ALIGNED(vmh->alloc_start, DYNAMO_OPTION(vmm_block_size)));
                        ASSERT(vmh->start_addr == vmh->alloc_start);
                    }
                    request_region_be_heap_reachable(app_base, app_end - app_base);
                    LOG(GLOBAL, LOG_HEAP, 1, "vmm_heap_unit_init: placed %s near app\n",
                        vmh->name);
                }
            }
        }
    }
#endif /* X64 */

    /* Next we try the -vm_base value plus a random offset. */
    if (vmh->start_addr == NULL) {
        /* Out of 32 bits = 12 bits are page offset, windows wastes 4 more
         * since its allocation base is 64KB, and if we want to stay
         * safely in say 0x20000000-0x2fffffff we're left with only 12
         * bits of randomness - which may be too little.  On the other
         * hand changing any of the lower 16 bits will make our bugs
         * non-deterministic. */
        /* Make sure we don't waste the lower bits from our random number */
        preferred = (DYNAMO_OPTION(vm_base) +
                     get_random_offset(DYNAMO_OPTION(vm_max_offset) /
                                       DYNAMO_OPTION(vmm_block_size)) *
                         DYNAMO_OPTION(vmm_block_size));
        preferred = ALIGN_FORWARD(preferred, OS_ALLOC_GRANULARITY);
        /* overflow check: w/ vm_base shouldn't happen so debug-only check */
        ASSERT(!POINTER_OVERFLOW_ON_ADD(preferred, size));
        /* let's assume a single chunk is sufficient to reserve */
#ifdef X64
        if ((byte *)preferred < heap_allowable_region_start ||
            (byte *)preferred + size > heap_allowable_region_end) {
            *error_code = HEAP_ERROR_NOT_AT_PREFERRED;
            LOG(GLOBAL, LOG_HEAP, 1,
                "vmm_heap_unit_init preferred=" PFX " too far from " PFX "-" PFX "\n",
                preferred, heap_allowable_region_start, heap_allowable_region_end);
        } else {
#endif
            vmh->alloc_start =
                os_heap_reserve((void *)preferred, size, error_code, true /*+x*/);
            vmh->start_addr = vmh->alloc_start;
            LOG(GLOBAL, LOG_HEAP, 1,
                "vmm_heap_unit_init preferred=" PFX " got start_addr=" PFX "\n",
                preferred, vmh->start_addr);
#ifdef X64
        }
#endif
    }
    while (vmh->start_addr == NULL && DYNAMO_OPTION(vm_allow_not_at_base)) {
        /* Since we prioritize low-4GB or near-app over -vm_base, we do not
         * syslog or assert here
         */
        /* need extra size to ensure alignment */
        vmh->alloc_size = size + DYNAMO_OPTION(vmm_block_size);
#ifdef X64
        /* PR 215395, make sure allocation satisfies heap reachability contraints */
        vmh->alloc_start = os_heap_reserve_in_region(
            (void *)ALIGN_FORWARD(heap_allowable_region_start, PAGE_SIZE),
            (void *)ALIGN_BACKWARD(heap_allowable_region_end, PAGE_SIZE),
            size + DYNAMO_OPTION(vmm_block_size), error_code, true /*+x*/);
#else
        vmh->alloc_start = (heap_pc)os_heap_reserve(
            NULL, size + DYNAMO_OPTION(vmm_block_size), error_code, true /*+x*/);
#endif
        vmh->start_addr =
            (heap_pc)ALIGN_FORWARD(vmh->alloc_start, DYNAMO_OPTION(vmm_block_size));
        LOG(GLOBAL, LOG_HEAP, 1,
            "vmm_heap_unit_init unable to allocate at preferred=" PFX
            " letting OS place sz=%dM addr=" PFX "\n",
            preferred, size / (1024 * 1024), vmh->start_addr);
        if (vmh->alloc_start == NULL && DYNAMO_OPTION(vm_allow_smaller)) {
            /* Just a little smaller might fit */
            size_t sub = (size_t)ALIGN_FORWARD(size / 16, 1024 * 1024);
            SYSLOG_INTERNAL_WARNING_ONCE("Full size vmm heap allocation failed");
            if (size > sub)
                size -= sub;
            else
                break;
        } else
            break;
    }
#ifdef X64
    if (DYNAMO_OPTION(satisfy_w_xor_x)) {
        /* Rather than replacing the 3 os_heap_reserve* calls above with os_map_file
         * whose MAP_FILE_REACHABLE relies on VMM (us!) being initialized, which is
         * tricky, we simply do the standard reserve above and then map our file
         * on top.  TODO i#3566: We need a different strategy on Windows.
         */
        /* Ensure os_map_file ignores vmcode: */
        ASSERT(!is_vmm_reserved_address(vmh->start_addr, size, NULL, NULL));
        size_t map_size = vmh->alloc_size;
        byte *map_base =
            os_map_file(heapmgt->dual_map_file, &map_size, 0, vmh->alloc_start,
                        MEMPROT_NONE, MAP_FILE_VMM_COMMIT | MAP_FILE_FIXED);
        if (map_base != vmh->alloc_start || map_size != vmh->alloc_size) {
            report_w_xor_x_fatal_error_and_exit();
            ASSERT_NOT_REACHED();
        }
    }
    /* ensure future out-of-block heap allocations are reachable from this allocation */
    if (vmh->start_addr != NULL) {
        ASSERT(vmh->start_addr >= heap_allowable_region_start &&
               !POINTER_OVERFLOW_ON_ADD(vmh->start_addr, size) &&
               vmh->start_addr + size <= heap_allowable_region_end);
        request_region_be_heap_reachable(vmh->start_addr, size);
    }
#endif
    ASSERT(ALIGNED(vmh->start_addr, DYNAMO_OPTION(vmm_block_size)));
}

/* Does not return. */
static void
vmm_heap_unit_init_failed(vm_heap_t *vmh, heap_error_code_t error_code, const char *name)
{
    LOG(GLOBAL, LOG_HEAP, 1, "vmm_heap_unit_init %s: failed to allocate memory!\n", name);
    vmm_heap_initialize_unusable(vmh);
    /* We couldn't even reserve initial virtual memory - we're out of luck. */
    report_low_on_memory(VMM_HEAP, OOM_INIT, error_code);
    ASSERT_NOT_REACHED();
}

static void
vmm_heap_unit_init(vm_heap_t *vmh, size_t size, bool is_vmcode, const char *name)
{
    heap_error_code_t error_code = 0;
    ASSIGN_INIT_LOCK_FREE(vmh->lock, vmh_lock);
    /* We need to get the lock into the process list before we copy out of
     * temp_heapmgt, else it will point to freed memory when we go back to temp_heapmgt
     * for lock cleanup code.
     */
    d_r_mutex_lock(&vmh->lock);
    d_r_mutex_unlock(&vmh->lock);
    size = ALIGN_FORWARD(size, DYNAMO_OPTION(vmm_block_size));
    vmh->alloc_size = size;
    vmh->start_addr = NULL;
    vmh->name = name;

    if (size == 0) {
        vmm_heap_initialize_unusable(vmh);
        return;
    }

    if (is_vmcode) {
        /* This is our must-be-reachable alloc whose placement matters and is
         * controlled by runtime options.
         */
        if (DYNAMO_OPTION(satisfy_w_xor_x)) {
            heapmgt->dual_map_file = os_create_memory_file(MEMORY_FILE_NAME, size);
            if (heapmgt->dual_map_file == INVALID_FILE) {
                report_w_xor_x_fatal_error_and_exit();
                ASSERT_NOT_REACHED();
            }
        }
        vmm_place_vmcode(vmh, size, &error_code);
        if (DYNAMO_OPTION(satisfy_w_xor_x)) {
            size_t map_size = vmh->alloc_size;
            heapmgt->vmcode_writable_alloc =
                os_map_file(heapmgt->dual_map_file, &map_size, 0, NULL, MEMPROT_NONE, 0);
            ASSERT(map_size == vmh->alloc_size);
            if (heapmgt->vmcode_writable_alloc == 0) {
                LOG(GLOBAL, LOG_HEAP, 1,
                    "vmm_heap_unit_init %s: failed to allocate writable vmcode!\n");
                vmm_heap_initialize_unusable(vmh);
                report_low_on_memory(VMM_CACHE | VMM_REACHABLE, OOM_INIT, error_code);
                ASSERT_NOT_REACHED();
            }
            heapmgt->vmcode_writable_base = (heap_pc)ALIGN_FORWARD(
                heapmgt->vmcode_writable_alloc, DYNAMO_OPTION(vmm_block_size));
            LOG(GLOBAL, LOG_HEAP, 1,
                "vmm_heap_unit_init vmcode+w reservation: [" PFX "," PFX ")\n",
                heapmgt->vmcode_writable_base, heapmgt->vmcode_writable_base + size);
        }
    } else {
        /* These days every OS provides ASLR, so we do not bother to do our own
         * for this second reservation and rely on the OS.
         */
        vmh->alloc_size = size + DYNAMO_OPTION(vmm_block_size);
        vmh->alloc_start = (heap_pc)os_heap_reserve(
            NULL, size + DYNAMO_OPTION(vmm_block_size), &error_code, false /*-x*/);
        vmh->start_addr =
            (heap_pc)ALIGN_FORWARD(vmh->alloc_start, DYNAMO_OPTION(vmm_block_size));
    }

    if (vmh->start_addr == 0) {
        vmm_heap_unit_init_failed(vmh, error_code, name);
        ASSERT_NOT_REACHED();
    }
    vmh->end_addr = vmh->start_addr + size;
    ASSERT_TRUNCATE(vmh->num_blocks, uint, size / DYNAMO_OPTION(vmm_block_size));
    vmh->num_blocks = (uint)(size / DYNAMO_OPTION(vmm_block_size));
    size_t blocks_sz_bytes = BITMAP_INDEX(vmh->num_blocks) * sizeof(bitmap_element_t);
    blocks_sz_bytes = ALIGN_FORWARD(blocks_sz_bytes, DYNAMO_OPTION(vmm_block_size));
    /* We place the bitmap at the start of the (writable) vmm region. */
    vmh->blocks = (bitmap_element_t *)vmh->start_addr;
    if (is_vmcode)
        vmh->blocks = (bitmap_element_t *)vmcode_get_writable_addr((byte *)vmh->blocks);
    vmh->num_free_blocks = vmh->num_blocks;
    LOG(GLOBAL, LOG_HEAP, 1,
        "vmm_heap_unit_init %s reservation: [" PFX "," PFX ") total=%d free=%d\n", name,
        vmh->start_addr, vmh->end_addr, vmh->num_blocks, vmh->num_free_blocks);

    /* Make sure the vmm area is properly aligned on block boundaries.
     * The size was aligned above.
     */
    ASSERT(ALIGNED(vmh->blocks, DYNAMO_OPTION(vmm_block_size)));

    which_vmm_t which = VMM_HEAP | (is_vmcode ? VMM_REACHABLE : 0);
    /* We have to commit first which our code does support. */
    vmm_heap_commit((vm_addr_t)vmh->blocks, blocks_sz_bytes, MEMPROT_READ | MEMPROT_WRITE,
                    &error_code, which);
    if (error_code != 0) {
        vmm_heap_unit_init_failed(vmh, error_code, name);
        ASSERT_NOT_REACHED();
    }
    bitmap_initialize_free(vmh->blocks, vmh->num_blocks);
    vmm_heap_reserve_blocks(vmh, blocks_sz_bytes, vmh->start_addr, which);
    DOLOG(1, LOG_HEAP, { vmm_dump_map(vmh); });
    ASSERT(bitmap_check_consistency(vmh->blocks, vmh->num_blocks, vmh->num_free_blocks));
}

static void
vmm_heap_unit_exit(vm_heap_t *vmh)
{
    LOG(GLOBAL, LOG_HEAP, 1, "vmm_heap_unit_exit %s [" PFX "," PFX ") total=%d free=%d\n",
        vmh->name, vmh->start_addr, vmh->end_addr, vmh->num_blocks, vmh->num_free_blocks);
    /* we assume single thread in DR at this point */
    DELETE_LOCK(vmh->lock);

    if (vmh->start_addr == NULL)
        return;

    DOLOG(1, LOG_HEAP, { vmm_dump_map(vmh); });
    ASSERT(bitmap_check_consistency(vmh->blocks, vmh->num_blocks, vmh->num_free_blocks));
    ASSERT(vmh->num_blocks * DYNAMO_OPTION(vmm_block_size) ==
           (ptr_uint_t)(vmh->end_addr - vmh->start_addr));

    /* In case there are no tombstones we can just free the unit and
     * that is what we'll do, otherwise it will stay up forever.
     */
    bool free_heap = vmh->num_free_blocks == vmh->num_blocks;
#ifdef UNIX
    /* On unix there's no fear of leftover tombstones, and as long as we're
     * doing a detach we can be sure our stack is not actually in the heap.
     */
    if (doing_detach) {
        DODEBUG({
            byte *sp;
            GET_STACK_PTR(sp);
            ASSERT(!(sp >= vmh->start_addr && sp < vmh->end_addr));
        });
        free_heap = true;
    }
#endif
    if (free_heap) {
        heap_error_code_t error_code;
        os_heap_free(vmh->alloc_start, vmh->alloc_size, &error_code);
        ASSERT(error_code == HEAP_ERROR_SUCCESS);
        if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode) {
            os_heap_free(heapmgt->vmcode_writable_alloc, vmh->alloc_size, &error_code);
            ASSERT(error_code == HEAP_ERROR_SUCCESS);
            os_delete_memory_file(MEMORY_FILE_NAME, heapmgt->dual_map_file);
            heapmgt->dual_map_file = INVALID_FILE;
        }
    } else {
        /* FIXME: doing nothing for now - we only care about this in
         * detach scenarios where we should try to clean up from the
         * virtual address space
         */
    }
    vmm_heap_initialize_unusable(vmh);
}

/* Returns whether within the region we reserved from the OS for doling
 * out internally via our vm_heap_t; asserts that the address was also
 * logically reserved within the vm_heap_t.
 */
static bool
vmm_is_reserved_unit(vm_heap_t *vmh, vm_addr_t p, size_t size)
{
    size = ALIGN_FORWARD(size, DYNAMO_OPTION(vmm_block_size));
    if (p < vmh->start_addr || vmh->end_addr < p /*overflow*/ ||
        vmh->end_addr < (p + size))
        return false;
    ASSERT(CHECK_TRUNCATE_TYPE_uint(size / DYNAMO_OPTION(vmm_block_size)));
    ASSERT(bitmap_are_reserved_blocks(vmh->blocks, vmh->num_blocks,
                                      vmm_addr_to_block(vmh, p),
                                      (uint)(size / DYNAMO_OPTION(vmm_block_size))));
    return true;
}

static inline bool
is_vmh_reserved_address(vm_heap_t *vmh, byte *pc, size_t size, OUT byte **region_start,
                        OUT byte **region_end)
{
    /* Case 10293: we don't call vmm_is_reserved_unit to avoid its
     * assert, which we want to maintain for callers only dealing with
     * DR-allocated addresses, while this routine is called w/ random
     * addresses
     */
    if (pc >= vmh->start_addr && !POINTER_OVERFLOW_ON_ADD(pc, size) &&
        (pc + size) <= vmh->end_addr) {
        if (region_start != NULL)
            *region_start = vmh->start_addr;
        if (region_end != NULL)
            *region_end = vmh->end_addr;
        return true;
    }
    return false;
}

/* Returns whether entirely within a region we reserve from the OS for doling
 * out internally via our vm_heap_t.  Optionally returns the bounds of the region.
 * Does not consider memory we allocate once we run out of our original reservations.
 */
bool
is_vmm_reserved_address(byte *pc, size_t size, OUT byte **region_start,
                        OUT byte **region_end)
{
    ASSERT(heapmgt != NULL);
    if (heapmgt->vmheap.start_addr != NULL &&
        is_vmh_reserved_address(&heapmgt->vmheap, pc, size, region_start, region_end))
        return true;
    if (heapmgt->vmcode.start_addr != NULL &&
        is_vmh_reserved_address(&heapmgt->vmcode, pc, size, region_start, region_end))
        return true;
    if (heapmgt->vmcode_writable_base != NULL &&
        is_vmh_reserved_address(&heapmgt->vmcode, vmcode_get_executable_addr(pc), size,
                                region_start, region_end)) {
        if (region_start != NULL)
            *region_start = vmcode_get_writable_addr(*region_start);
        if (region_end != NULL)
            *region_end = vmcode_get_writable_addr(*region_end);
        return true;
    }
    return false;
}

byte *
vmcode_get_start(void)
{
    if (heapmgt->vmcode.start_addr != NULL)
        return heapmgt->vmcode.start_addr;
    if (heapmgt->vmheap.start_addr != NULL)
        return heapmgt->vmheap.start_addr;
    return NULL;
}

byte *
vmcode_get_end(void)
{
    if (heapmgt->vmcode.start_addr != NULL)
        return heapmgt->vmcode.end_addr;
    if (heapmgt->vmheap.start_addr != NULL)
        return heapmgt->vmheap.end_addr;
    return NULL;
}

static vm_heap_t *
vmheap_for_which(which_vmm_t which)
{
    if (TEST(VMM_REACHABLE, which) || REACHABLE_HEAP())
        return &heapmgt->vmcode;
    else
        return &heapmgt->vmheap;
}

byte *
vmcode_get_writable_addr(byte *exec_addr)
{
    /* XXX i#5383: Audit these calls and ensure they cover all scenarios, are placed
     * at the most efficient level, and are always properly paired.
     */
    PTHREAD_JIT_WRITE();
    if (!DYNAMO_OPTION(satisfy_w_xor_x))
        return exec_addr;
    /* If we want this to be an assert instead to catch superfluous calls, we'll need
     * to change things like set_selfmod_sandbox_offsets()'s call to
     * encode_with_patch_list() into a stack buffer.
     */
    if (exec_addr < heapmgt->vmcode.start_addr || exec_addr >= heapmgt->vmcode.end_addr)
        return exec_addr;
    return (exec_addr - heapmgt->vmcode.start_addr) + heapmgt->vmcode_writable_base;
}

byte *
vmcode_get_executable_addr(byte *write_addr)
{
    if (!DYNAMO_OPTION(satisfy_w_xor_x))
        return write_addr;
    if (write_addr < heapmgt->vmcode_writable_base ||
        write_addr >= heapmgt->vmcode_writable_base +
                (heapmgt->vmcode.end_addr - heapmgt->vmcode.start_addr))
        return write_addr;
    return (write_addr - heapmgt->vmcode_writable_base) + heapmgt->vmcode.start_addr;
}

#ifdef DEBUG_MEMORY
static inline byte *
vmm_get_writable_addr(byte *exec_addr, which_vmm_t which)
{
    vm_heap_t *vmh = vmheap_for_which(which);
    if (vmh == &heapmgt->vmcode)
        return vmcode_get_writable_addr(exec_addr);
    return exec_addr;
}
#endif

/* The caller must first ensure this is a vmcode address.  Returns p_writable. */
static inline vm_addr_t
vmm_normalize_addr(vm_heap_t *vmh, INOUT vm_addr_t *p_exec)
{
    vm_addr_t p = *p_exec;
    if (p < vmh->start_addr || p >= vmh->end_addr) {
        /* This is a writable addr. */
        p = (p - heapmgt->vmcode_writable_base) + vmh->start_addr;
        *p_exec = p;
    }
    return (p - vmh->start_addr) + heapmgt->vmcode_writable_base;
}

#ifdef WINDOWS
static byte *
vmheap_get_start(void)
{
    if (heapmgt->vmheap.start_addr != NULL)
        return heapmgt->vmheap.start_addr;
    if (heapmgt->vmcode.start_addr != NULL)
        return heapmgt->vmcode.start_addr;
    return NULL;
}
#endif

static inline bool
has_guard_pages(which_vmm_t which)
{
    if (!DYNAMO_OPTION(guard_pages))
        return false;
    if (TEST(VMM_PER_THREAD, which) && !DYNAMO_OPTION(per_thread_guard_pages))
        return false;
    return true;
}

void
iterate_vmm_regions(void (*cb)(byte *region_start, byte *region_end, void *user_data),
                    void *user_data)
{
    if (heapmgt->vmcode.start_addr != NULL)
        (*cb)(heapmgt->vmcode.start_addr, heapmgt->vmcode.end_addr, user_data);
    if (heapmgt->vmheap.start_addr != NULL)
        (*cb)(heapmgt->vmheap.start_addr, heapmgt->vmheap.end_addr, user_data);
    if (heapmgt->vmcode_writable_base != NULL) {
        (*cb)(heapmgt->vmcode_writable_base,
              heapmgt->vmcode_writable_base +
                  (heapmgt->vmcode.end_addr - heapmgt->vmcode.start_addr),
              user_data);
    }
}

byte *
vmcode_unreachable_pc(void)
{
#ifdef X86_64
    /* This is used to indicate something that is unreachable from *everything*
     * for DR_CLEANCALL_INDIRECT, so ideally we want to not just provide an
     * address that vmcode can't reach.
     * We use a non-canonical address for x86_64.
     */
    return (byte *)0x8000000100000000ULL;
#else
    /* This is not really used for aarch* so we just go with vmcode reachability. */
    ptr_uint_t start = (ptr_uint_t)vmcode_get_start();
    ptr_uint_t end = (ptr_uint_t)vmcode_get_end();
    if (start > INT_MAX)
        return NULL;
    else {
        /* We do not use -1 to avoid wraparound from thinking it's reachable. */
        return (byte *)end + INT_MAX + PAGE_SIZE;
    }
#endif
}

bool
rel32_reachable_from_vmcode(byte *tgt)
{
#ifdef X64
    /* To handle beyond-vmm-reservation allocs, we must compare to the allowable
     * heap range and not just the vmcode range (i#1479).
     */
    ptr_int_t new_offs = (tgt > heap_allowable_region_start)
        ? (tgt - heap_allowable_region_start)
        : (heap_allowable_region_end - tgt);
    ASSERT(vmcode_get_start() >= heap_allowable_region_start ||
           !DYNAMO_OPTION(vm_reserve));
    ASSERT(vmcode_get_end() <= heap_allowable_region_end + 1 /*closed*/ ||
           !DYNAMO_OPTION(vm_reserve));
    return REL32_REACHABLE_OFFS(new_offs);
#else
    return true;
#endif
}

bool
rel32_reachable_from_current_vmcode(byte *tgt)
{
#ifdef X64
    ptr_int_t new_offs = (tgt > must_reach_region_start) ? (tgt - must_reach_region_start)
                                                         : (must_reach_region_end - tgt);
    return REL32_REACHABLE_OFFS(new_offs);
#else
    return true;
#endif
}

static inline void
vmm_update_block_stats(which_vmm_t which, uint num_blocks, bool add)
{
    /* We do not split the stats for cache (always reachable) nor stack (never reachable).
     * We confirm our assumptions here.
     */
    ASSERT(!TESTALL(VMM_REACHABLE | VMM_STACK, which) &&
           (TEST(VMM_REACHABLE, which) || !TEST(VMM_CACHE, which)));
    /* XXX: find some way to make a stats array */
    if (add) {
        if (TEST(VMM_HEAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_ADD_PEAK(vmm_blocks_reach_heap, num_blocks);
            else
                RSTATS_ADD_PEAK(vmm_blocks_unreach_heap, num_blocks);
        } else if (TEST(VMM_CACHE, which))
            RSTATS_ADD_PEAK(vmm_blocks_reach_cache, num_blocks);
        else if (TEST(VMM_STACK, which))
            RSTATS_ADD_PEAK(vmm_blocks_unreach_stack, num_blocks);
        else if (TEST(VMM_SPECIAL_HEAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_ADD_PEAK(vmm_blocks_reach_special_heap, num_blocks);
            else
                RSTATS_ADD_PEAK(vmm_blocks_unreach_special_heap, num_blocks);
        } else if (TEST(VMM_SPECIAL_MMAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_ADD_PEAK(vmm_blocks_reach_special_mmap, num_blocks);
            else
                RSTATS_ADD_PEAK(vmm_blocks_unreach_special_mmap, num_blocks);
        }
    } else {
        if (TEST(VMM_HEAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_SUB(vmm_blocks_reach_heap, num_blocks);
            else
                RSTATS_SUB(vmm_blocks_unreach_heap, num_blocks);
        } else if (TEST(VMM_CACHE, which))
            RSTATS_SUB(vmm_blocks_reach_cache, num_blocks);
        else if (TEST(VMM_STACK, which))
            RSTATS_SUB(vmm_blocks_unreach_stack, num_blocks);
        else if (TEST(VMM_SPECIAL_HEAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_SUB(vmm_blocks_reach_special_heap, num_blocks);
            else
                RSTATS_SUB(vmm_blocks_unreach_special_heap, num_blocks);
        } else if (TEST(VMM_SPECIAL_MMAP, which)) {
            if (TEST(VMM_REACHABLE, which))
                RSTATS_SUB(vmm_blocks_reach_special_mmap, num_blocks);
            else
                RSTATS_SUB(vmm_blocks_unreach_special_mmap, num_blocks);
        }
    }
}

/* Reservations here are done with DYNAMO_OPTION(vmm_block_size) alignment
 * (e.g. 64KB) but the caller is not forced to request at that
 * alignment.  We explicitly synchronize reservations and decommits
 * within the vm_heap_t.

 * Returns NULL if the VMMHeap is full or too fragmented to satisfy
 * the request.
 */
static vm_addr_t
vmm_heap_reserve_blocks(vm_heap_t *vmh, size_t size_in, byte *base, which_vmm_t which)
{
    vm_addr_t p;
    uint request;
    uint first_block;
    size_t size;
    uint must_start;

    size = ALIGN_FORWARD(size_in, DYNAMO_OPTION(vmm_block_size));
    ASSERT_TRUNCATE(request, uint, size / DYNAMO_OPTION(vmm_block_size));
    request = (uint)(size / DYNAMO_OPTION(vmm_block_size));

    if (base != NULL)
        must_start = vmm_addr_to_block(vmh, base);
    else
        must_start = UINT_MAX;

    LOG(GLOBAL, LOG_HEAP, 2,
        "vmm_heap_reserve_blocks %s: size=%d => %d in blocks=%d free_blocks=%d\n",
        vmh->name, size_in, size, request, vmh->num_free_blocks);

    d_r_mutex_lock(&vmh->lock);
    if (vmh->num_free_blocks < request) {
        d_r_mutex_unlock(&vmh->lock);
        return NULL;
    }
    first_block =
        bitmap_allocate_blocks(vmh->blocks, vmh->num_blocks, request, must_start);
    if (first_block != BITMAP_NOT_FOUND) {
        vmh->num_free_blocks -= request;
    }
    d_r_mutex_unlock(&vmh->lock);

    if (first_block != BITMAP_NOT_FOUND) {
        p = vmm_block_to_addr(vmh, first_block);
        RSTATS_ADD_PEAK(vmm_vsize_used, size);
        STATS_ADD_PEAK(vmm_vsize_blocks_used, request);
        STATS_ADD_PEAK(vmm_vsize_wasted, size - size_in);
        vmm_update_block_stats(which, request, true /*add*/);
        DOSTATS({
            if (request > 1) {
                STATS_INC(vmm_multi_block_allocs);
                STATS_ADD(vmm_multi_blocks, request);
            }
        });
    } else {
        p = NULL;
    }
    LOG(GLOBAL, LOG_HEAP, 2,
        "vmm_heap_reserve_blocks %s: size=%d blocks=%d p=" PFX " index=%u\n", vmh->name,
        size, request, p, first_block);
    DOLOG(5, LOG_HEAP, { vmm_dump_map(vmh); });
    return p;
}

/* We explicitly synchronize reservations and decommits within the vm_heap_t.
 * Update bookkeeping information about the freed region.
 */
static void
vmm_heap_free_blocks(vm_heap_t *vmh, vm_addr_t p, size_t size_in, which_vmm_t which)
{
    uint first_block = vmm_addr_to_block(vmh, p);
    uint request;
    size_t size;

    size = ALIGN_FORWARD(size_in, DYNAMO_OPTION(vmm_block_size));
    ASSERT_TRUNCATE(request, uint, size / DYNAMO_OPTION(vmm_block_size));
    request = (uint)(size / DYNAMO_OPTION(vmm_block_size));

    LOG(GLOBAL, LOG_HEAP, 2, "vmm_heap_free_blocks %s: size=%d blocks=%d p=" PFX "\n",
        vmh->name, size, request, p);

    d_r_mutex_lock(&vmh->lock);
    bitmap_free_blocks(vmh->blocks, vmh->num_blocks, first_block, request);
    vmh->num_free_blocks += request;
    d_r_mutex_unlock(&vmh->lock);

    ASSERT(vmh->num_free_blocks <= vmh->num_blocks);
    RSTATS_SUB(vmm_vsize_used, size);
    STATS_SUB(vmm_vsize_blocks_used, request);
    vmm_update_block_stats(which, request, false /*sub*/);
    STATS_SUB(vmm_vsize_wasted, size - size_in);
}

/* This is the proper interface for the rest of heap.c to the os_heap_* functions */

/* place all the local-scope static vars (from DO_THRESHOLD) into .fspdata to avoid
 * protection changes */
START_DATA_SECTION(FREQ_PROTECTED_SECTION, "w");

static bool
at_reset_at_vmm_limit(vm_heap_t *vmh)
{
    return (DYNAMO_OPTION(reset_at_vmm_percent_free_limit) != 0 &&
            100 * vmh->num_free_blocks <
                DYNAMO_OPTION(reset_at_vmm_percent_free_limit) * vmh->num_blocks) ||
        (DYNAMO_OPTION(reset_at_vmm_free_limit) != 0 &&
         vmh->num_free_blocks * DYNAMO_OPTION(vmm_block_size) <
             DYNAMO_OPTION(reset_at_vmm_free_limit));
}

static void
reached_beyond_vmm(which_vmm_t which)
{
    DODEBUG(ever_beyond_vmm = true;);
    /* Stats can be very useful to diagnose why we hit OOM. */
    if (INTERNAL_OPTION(rstats_to_stderr))
        dump_global_rstats_to_stderr();
    char message[256];
    if (DYNAMO_OPTION(satisfy_w_xor_x) &&
        (TEST(VMM_REACHABLE, which) || REACHABLE_HEAP())) {
        /* We do not bother to try to mirror separate from-OS allocs: the user
         * should set -vm_size 2G instead and take the rip-rel mangling hit
         * (see i#3570).
         */
        snprintf(
            message, BUFFER_SIZE_ELEMENTS(message),
            "Alloc type: 0x%x.  -satisfy_w_xor_x requires VMM memory: try '-vm_size 2G'",
            which);
        NULL_TERMINATE_BUFFER(message);
        REPORT_FATAL_ERROR_AND_EXIT(OUT_OF_VMM_CANNOT_USE_OS, 3, get_application_name(),
                                    get_application_pid(), message);
        ASSERT_NOT_REACHED();
    } else {
        snprintf(message, BUFFER_SIZE_ELEMENTS(message), "Alloc type: 0x%x.", which);
        NULL_TERMINATE_BUFFER(message);
        SYSLOG(SYSLOG_WARNING, OUT_OF_VMM_CANNOT_USE_OS, 3, get_application_name(),
               get_application_pid(), message);
    }
}

void
vmm_heap_handle_pending_low_on_memory_event_trigger()
{
    bool trigger = false;

    acquire_recursive_lock(&low_on_memory_pending_lock);
    if (low_on_memory_pending) {
        bool value = false;
        ATOMIC_1BYTE_WRITE(&low_on_memory_pending, value, false);
        trigger = true;
    }
    release_recursive_lock(&low_on_memory_pending_lock);

    if (trigger)
        instrument_low_on_memory();
}

static void
schedule_low_on_memory_event_trigger()
{
    bool value = true;
    ATOMIC_1BYTE_WRITE(&low_on_memory_pending, value, false);
}

/* Reserve virtual address space without committing swap space for it */
static vm_addr_t
vmm_heap_reserve(size_t size, heap_error_code_t *error_code, bool executable,
                 which_vmm_t which)
{
    vm_addr_t p;
    vm_heap_t *vmh = vmheap_for_which(which);
    /* should only be used on sizable aligned pieces */
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));
    ASSERT(!OWN_MUTEX(&reset_pending_lock));

    if (DYNAMO_OPTION(vm_reserve)) {
        /* FIXME: should we make this an external option? */
        if (INTERNAL_OPTION(vm_use_last) ||
            (DYNAMO_OPTION(switch_to_os_at_vmm_reset_limit) &&
             at_reset_at_vmm_limit(vmh))) {
            DO_ONCE({
                if (DYNAMO_OPTION(reset_at_switch_to_os_at_vmm_limit)) {
                    schedule_reset(RESET_ALL);
                }
                schedule_low_on_memory_event_trigger();
                DOCHECK(1, {
                    if (!INTERNAL_OPTION(vm_use_last)) {
                        ASSERT_CURIOSITY(false && "running low on vm reserve");
                    }
                });
                /* FIXME - for our testing would be nice to have some release build
                 * notification of this ... */
            });
            reached_beyond_vmm(which);
#ifdef X64
            if (TEST(VMM_REACHABLE, which) || REACHABLE_HEAP()) {
                /* PR 215395, make sure allocation satisfies heap reachability
                 * contraints */
                p = os_heap_reserve_in_region(
                    (void *)ALIGN_FORWARD(heap_allowable_region_start, PAGE_SIZE),
                    (void *)ALIGN_BACKWARD(heap_allowable_region_end, PAGE_SIZE), size,
                    error_code, executable);
                /* ensure future heap allocations are reachable from this allocation */
                if (p != NULL)
                    request_region_be_heap_reachable(p, size);
            } else
                p = os_heap_reserve(NULL, size, error_code, executable);
#else
            p = os_heap_reserve(NULL, size, error_code, executable);
#endif
            if (p != NULL)
                return p;
            LOG(GLOBAL, LOG_HEAP, 1, "vmm_heap_reserve %s: failed " PFX "\n", vmh->name,
                *error_code);
        }

        if (at_reset_at_vmm_limit(vmh)) {
            /* We're running low on our reservation, trigger a reset */
            schedule_low_on_memory_event_trigger();
            if (schedule_reset(RESET_ALL)) {
                STATS_INC(reset_low_vmm_count);
                DO_THRESHOLD_SAFE(
                    DYNAMO_OPTION(report_reset_vmm_threshold), FREQ_PROTECTED_SECTION,
                    { /* < max - nothing */ },
                    { /* >= max */
                      /* FIXME - do we want to report more then once to give some idea of
                       * how much thrashing there is? */
                      DO_ONCE({
                          SYSLOG_CUSTOM_NOTIFY(SYSLOG_WARNING, MSG_LOW_ON_VMM_MEMORY, 2,
                                               "Potentially thrashing on low virtual "
                                               "memory resetting.",
                                               get_application_name(),
                                               get_application_pid());
                          /* want QA to notice */
                          ASSERT_CURIOSITY(false && "vmm heap limit reset thrashing");
                      });
                    });
            }
        }

        p = vmm_heap_reserve_blocks(vmh, size, NULL, which);
        LOG(GLOBAL, LOG_HEAP, 2, "vmm_heap_reserve %s: size=%d p=" PFX "\n", vmh->name,
            size, p);

        if (p != NULL) {
            if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode &&
                !executable) {
                /* Pass back the writable address, not the executable.
                 * Then things like reachable heap do not need to convert to
                 * writable all over the place.
                 */
                p = (p - vmh->start_addr) + heapmgt->vmcode_writable_base;
            }
            return p;
        }
        DO_ONCE({
            DODEBUG({ out_of_vmheap_once = true; });
            if (!INTERNAL_OPTION(skip_out_of_vm_reserve_curiosity)) {
                /* this maybe unsafe for early services w.r.t. case 666 */
                SYSLOG_INTERNAL_WARNING("Out of %s reservation - reserving %dKB. "
                                        "Falling back onto OS allocation",
                                        (TEST(VMM_REACHABLE, which) || REACHABLE_HEAP())
                                            ? "vmcode"
                                            : "vmheap",
                                        size / 1024);
                ASSERT_CURIOSITY(false && "Out of vmheap reservation");
            }
            /* This actually-out trigger is only trying to help issues like a
             * thread-private configuration being a memory hog (and thus we use up
             * our reserve). Reset needs memory, and this is asynchronous, so no
             * guarantees here anyway (the app may have already reserved all memory
             * beyond our reservation, see sqlsrvr.exe and cisvc.exe for ex.) which is
             * why we have -reset_at_vmm_threshold to make reset success more likely. */
            if (DYNAMO_OPTION(reset_at_vmm_full)) {
                schedule_reset(RESET_ALL);
            }
        });
    }
    /* if we fail to allocate from our reservation we fall back to the OS */
    reached_beyond_vmm(which);
#ifdef X64
    if (TEST(VMM_REACHABLE, which) || REACHABLE_HEAP()) {
        /* PR 215395, make sure allocation satisfies heap reachability contraints */
        p = os_heap_reserve_in_region(
            (void *)ALIGN_FORWARD(heap_allowable_region_start, PAGE_SIZE),
            (void *)ALIGN_BACKWARD(heap_allowable_region_end, PAGE_SIZE), size,
            error_code, executable);
        /* ensure future heap allocations are reachable from this allocation */
        if (p != NULL)
            request_region_be_heap_reachable(p, size);
    } else
        p = os_heap_reserve(NULL, size, error_code, executable);
#else
    p = os_heap_reserve(NULL, size, error_code, executable);
#endif
    return p;
}

/* Commit previously reserved pages, returns false when out of memory
 * This is here just to complement the vmm interface, in fact it is
 * almost an alias for os_heap_commit.  (If we had strict types then
 * here we'd convert a vm_addr_t into a heap_pc.)
 */
static inline bool
vmm_heap_commit(vm_addr_t p, size_t size, uint prot, heap_error_code_t *error_code,
                which_vmm_t which)
{
    bool res = true;
    vm_heap_t *vmh = vmheap_for_which(which);
    LOG(GLOBAL, LOG_HEAP, 3, "vmm_heap_commit %s: size=%d p=" PFX " prot=%x\n", vmh->name,
        size, p, prot);
    if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode) {
        vm_addr_t p_writable = vmm_normalize_addr(vmh, &p);
        /* We blindly shadow even if prot is -w to simplify de-alloc.  -w is rare. */
        uint shadow_prot = prot & ~(MEMPROT_EXEC);
        res = os_heap_commit(p_writable, size, shadow_prot, error_code);
        prot &= ~(MEMPROT_WRITE);
        if (res) {
            /* We use mmap instead of mprotect since W^X policies often only allow
             * execution from regions allocated executable, not changed to executable.
             * There is a downside: IMA policies can cause a significant (~5s) delay
             * while a hash is computed of our vmcode region on the first +x mmap.
             * Today os_create_memory_file() does a temporary +x mmap for us, avoiding
             * any cost here.
             */
            size_t map_size = size;
            size_t map_offs = p - vmh->start_addr;
            vm_addr_t map_addr =
                os_map_file(heapmgt->dual_map_file, &map_size, map_offs, p, prot,
                            MAP_FILE_VMM_COMMIT | MAP_FILE_FIXED);
            ASSERT(map_size == size);
            res = (map_addr != NULL);
            ASSERT(map_addr == NULL || map_addr == p);
        }
    } else
        res = os_heap_commit(p, size, prot, error_code);
    size_t commit_used, commit_limit;
    ASSERT(!OWN_MUTEX(&reset_pending_lock));
    if ((DYNAMO_OPTION(reset_at_commit_percent_free_limit) != 0 ||
         DYNAMO_OPTION(reset_at_commit_free_limit) != 0) &&
        os_heap_get_commit_limit(&commit_used, &commit_limit)) {
        size_t commit_left = commit_limit - commit_used;
        ASSERT(commit_used <= commit_limit);
        /* FIXME - worry about overflow in the multiplies below? With 4kb pages isn't
         * an issue till 160GB of committable memory. */
        if ((DYNAMO_OPTION(reset_at_commit_free_limit) != 0 &&
             commit_left < DYNAMO_OPTION(reset_at_commit_free_limit) / PAGE_SIZE) ||
            (DYNAMO_OPTION(reset_at_commit_percent_free_limit) != 0 &&
             100 * commit_left <
                 DYNAMO_OPTION(reset_at_commit_percent_free_limit) * commit_limit)) {
            /* Machine is getting low on memory, trigger a reset */
            /* FIXME - if we aren't the ones hogging committed memory (rougue app) then
             * do we want a version of reset that doesn't de-commit our already grabbed
             * memory to avoid someone else stealing it (or perhaps keep just a minimal
             * level to ensure we make some progress)? */
            /* FIXME - the commit limit is for the whole system; we have no good way of
             * telling if we're running in a job and if so what the commit limit for the
             * job is. */
            /* FIXME - if a new process is started under dr while the machine is already
             * past the threshold we will just spin resetting here and not make any
             * progress, may be better to only reset when we have a reasonable amount of
             * non-persistent memory to free (so that we can at least make some progress
             * before resetting again). */
            /* FIXME - the threshold is calculated at the current page file size, but
             * it's possible that the pagefile is expandable (dependent on disk space of
             * course) and thus we're preventing a potentially beneficial (to us)
             * upsizing of the pagefile here.  See "HKLM\SYSTEM\CCS\ControlSession /
             * Manager\Memory Management" for the initial/max size of the various page
             * files (query SystemPafefileInformation only gets you the current size). */
            /* xref case 345 on fixmes (and link to wiki discussion) */
            if (schedule_reset(RESET_ALL)) {
                STATS_INC(reset_low_commit_count);
                DO_THRESHOLD_SAFE(
                    DYNAMO_OPTION(report_reset_commit_threshold), FREQ_PROTECTED_SECTION,
                    { /* < max - nothing */ },
                    { /* >= max */
                      /* FIXME - do we want to report more then once to give some idea of
                       * how much thrashing there is? */
                      DO_ONCE({
                          SYSLOG_CUSTOM_NOTIFY(
                              SYSLOG_WARNING, MSG_LOW_ON_COMMITTABLE_MEMORY, 2,
                              "Potentially thrashing on low committable "
                              "memory resetting.",
                              get_application_name(), get_application_pid());
                          /* want QA to notice */
                          ASSERT_CURIOSITY(false && "commit limit reset thrashing");
                      });
                    });
            }
        }
    }
    if (!res && DYNAMO_OPTION(oom_timeout) != 0 &&
        !(DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode)) {
        DEBUG_DECLARE(heap_error_code_t old_error_code = *error_code;)
        ASSERT(old_error_code != HEAP_ERROR_SUCCESS);

        /* check whether worth retrying */
        if (!os_heap_systemwide_overcommit(*error_code)) {
            /* FIXME: we should check whether current process is the hog */
            /* unless we have used the memory, there is still a
             * miniscule chance another thread will free up some or
             * will attempt suicide, so could retry even if current
             * process has a leak */
            ASSERT_NOT_IMPLEMENTED(false);
            /* retry */
        }

        SYSLOG_INTERNAL_WARNING("vmm_heap_commit oom: timeout and retry");
        /* let's hope a memory hog dies in the mean time */
        os_timeout(DYNAMO_OPTION(oom_timeout));

        res = os_heap_commit(p, size, prot, error_code);
        DODEBUG({
            if (res) {
                SYSLOG_INTERNAL_WARNING("vmm_heap_commit retried, got away!  old=" PFX
                                        " new=" PFX "\n",
                                        old_error_code, *error_code);
            } else {
                SYSLOG_INTERNAL_WARNING("vmm_heap_commit retrying, no luck.  old=" PFX
                                        " new=" PFX "\n",
                                        old_error_code, *error_code);
            }
        });
    }

    return res;
}

/* back to normal section */
END_DATA_SECTION()

/* Free previously reserved and possibly committed memory.  Check if
 * it is within the memory managed by the virtual memory manager we
 * only decommit back to the OS, and we remove the vmm reservation.
 * Keep in mind that this can be called on units that are not fully
 * committed, e.g. guard pages are added to this - as long as the
 * os_heap_decommit interface can handle this we're OK
 */
static void
vmm_heap_free(vm_addr_t p, size_t size, heap_error_code_t *error_code, which_vmm_t which)
{
    vm_heap_t *vmh = vmheap_for_which(which);
    LOG(GLOBAL, LOG_HEAP, 2, "vmm_heap_free %s: size=%d p=" PFX " is_reserved=%d\n",
        vmh->name, size, p, vmm_is_reserved_unit(vmh, p, size));
    vm_addr_t p_writable = p;
    if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode)
        p_writable = vmm_normalize_addr(vmh, &p);

    /* The memory doesn't have to be within our VM reserve if it
     * was allocated as an extra OS call when if we ran out.
     */
    if (DYNAMO_OPTION(vm_reserve)) {
        if (vmm_is_reserved_unit(vmh, p, size)) {
            if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode)
                os_heap_decommit(p_writable, size, error_code);
            os_heap_decommit(p, size, error_code);
            vmm_heap_free_blocks(vmh, p, size, which);
            LOG(GLOBAL, LOG_HEAP, 2, "vmm_heap_free %s: freed size=%d p=" PFX "\n",
                vmh->name, size, p);
            return;
        } else {
            /* FIXME: check if this is stack_free getting in the way, then ignore it */
            /* FIXME: could do this by overriding the meaning of the vmheap fields
               after cleanup to a different combination that start_pc = end_pc = NULL
             */
            /* FIXME: see vmm_heap_unit_exit for the current stack_free problem */
            if (vmm_heap_exited) {
                *error_code = HEAP_ERROR_SUCCESS;
                return;
            }
        }
    }
    if (DYNAMO_OPTION(satisfy_w_xor_x) && vmh == &heapmgt->vmcode)
        os_heap_free(p_writable, size, error_code);
    os_heap_free(p, size, error_code);
}

static void
vmm_heap_decommit(vm_addr_t p, size_t size, heap_error_code_t *error_code,
                  which_vmm_t which)
{
    LOG(GLOBAL, LOG_HEAP, 2, "vmm_heap_decommit: size=%d p=" PFX " is_reserved=%d\n",
        size, p, is_vmm_reserved_address(p, size, NULL, NULL));
    if (DYNAMO_OPTION(satisfy_w_xor_x)) {
        vm_heap_t *vmh = vmheap_for_which(which);
        if (vmh == &heapmgt->vmcode) {
            vm_addr_t p_writable = vmm_normalize_addr(vmh, &p);
            os_heap_decommit(p_writable, size, error_code);
        }
    }
    os_heap_decommit(p, size, error_code);
    /* nothing to be done to vmm blocks */
}

/* Caller is required to handle thread synchronization and to update dynamo vm areas.
 * size must be PAGE_SIZE-aligned.
 * Returns NULL if fails to allocate memory!
 */
static void *
vmm_heap_alloc(size_t size, uint prot, heap_error_code_t *error_code, which_vmm_t which)
{
    vm_addr_t p = vmm_heap_reserve(size, error_code, TEST(MEMPROT_EXEC, prot), which);
    if (!p)
        return NULL; /* out of reserved memory */

    if (!vmm_heap_commit(p, size, prot, error_code, which))
        return NULL; /* out of committed memory */
    return p;
}

/* virtual memory manager initialization */
void
vmm_heap_init()
{
    IF_WINDOWS(ASSERT(ALIGNED(OS_ALLOC_GRANULARITY, DYNAMO_OPTION(vmm_block_size))));
#ifdef X64
    /* add reachable regions before we allocate the heap, xref PR 215395 */
    /* i#774, i#901: we no longer need the DR library nor ntdll.dll to be
     * reachable by the vmheap reservation.  But, for -heap_in_lower_4GB,
     * we must call request_region_be_heap_reachable() up front.
     * This is a hard requirement so we set it prior to locating the vmm region.
     */
    if (DYNAMO_OPTION(heap_in_lower_4GB))
        request_region_be_heap_reachable(0, 0x80000000);
#endif
    if (DYNAMO_OPTION(vm_reserve)) {
        vmm_heap_unit_init(&heapmgt->vmcode, DYNAMO_OPTION(vm_size), true, "vmcode");
        if (!REACHABLE_HEAP()) {
            vmm_heap_unit_init(
                &heapmgt->vmheap,
                /* Use vmheap_size_wow64 if target is WoW64 windows process. */
                IF_WINDOWS_ELSE(IF_X64_ELSE(is_wow64_process(NT_CURRENT_PROCESS)
                                                ? DYNAMO_OPTION(vmheap_size_wow64)
                                                : DYNAMO_OPTION(vmheap_size),
                                            DYNAMO_OPTION(vmheap_size)),
                                DYNAMO_OPTION(vmheap_size)),
                false, "vmheap");
        }
    }
}

static void
vmh_exit(vm_heap_t *vmh, bool contains_stacks)
{
    /* We have three regions that are not explicitly deallocated: current stack, init
     * stack, global_do_syscall.
     */
    DOCHECK(1, {
        uint perstack =
            (uint)(ALIGN_FORWARD_UINT(
                       DYNAMO_OPTION(stack_size) +
                           (has_guard_pages(VMM_STACK | VMM_PER_THREAD)
                                ? (2 * PAGE_SIZE)
                                : (DYNAMO_OPTION(stack_guard_pages) ? PAGE_SIZE : 0)),
                       DYNAMO_OPTION(vmm_block_size)) /
                   DYNAMO_OPTION(vmm_block_size));
        uint unfreed_blocks;
        if (!contains_stacks || standalone_library)
            unfreed_blocks = 0;
        else {
            unfreed_blocks = perstack * 1 /* d_r_initstack */ +
                /* current stack */
                perstack * ((doing_detach IF_APP_EXPORTS(|| dr_api_exit)) ? 0 : 1);
        }
        /* Our bitmap does not get freed. */
        size_t blocks_sz_bytes =
            ALIGN_FORWARD_UINT(BITMAP_INDEX(vmh->num_blocks) * sizeof(bitmap_element_t),
                               DYNAMO_OPTION(vmm_block_size));
        unfreed_blocks += (uint)(blocks_sz_bytes / DYNAMO_OPTION(vmm_block_size));
        /* XXX: On detach, arch_thread_exit should explicitly mark as
         * left behind all TPCs needed so then we can assert even for
         * detach.
         */
        ASSERT(IF_WINDOWS(doing_detach ||) /* not deterministic when detaching */
                   vmh->num_free_blocks == vmh->num_blocks - unfreed_blocks ||
               /* >=, not ==, b/c if we hit the vmm limit the cur dstack
                * could be outside of vmm (i#1164).
                */
               ((ever_beyond_vmm
                     /* This also happens for dstacks up high for DrMi#1723. */
                     IF_WINDOWS(|| get_os_version() >= WINDOWS_VERSION_8_1)) &&
                vmh->num_free_blocks >= vmh->num_blocks - unfreed_blocks));
    });
    /* On process exit we are currently executing off a
     * stack in this region so we cannot free the whole allocation.

     * XXX: Any tombstone allocations will have to use a
     * different interface than the generic heap_mmap() which is
     * sometimes used to leave things behind.  FIXME: Currently
     * we'll leave behind the whole vm unit if any tombstones are
     * left - which in fact is always the case, no matter whether
     * thread private code needs to be left or not.

     * global_do_syscall 32 byte allocation should be part of our
     * dll and won't have to be left.

     * The current stack is the main problem because it is later
     * cleaned up in cleanup_and_terminate by calling stack_free which
     * in turn gets all the way to vmm_heap_free.  Therefore we add an
     * explicit test for vmm_heap_exited, so that we can otherwise free
     * bookkeeping information and delete the lock now.

     * Potential solution to most of these problems is to have
     * cleanup_and_terminate call vmm_heap_exit when cleaning up
     * the process, or to just leave the vm mapping behind and
     * simply pass a different argument to stack_free.
     */
    vmm_heap_unit_exit(vmh);
}

void
vmm_heap_exit()
{
    /* virtual memory manager exit */
    if (DYNAMO_OPTION(vm_reserve)) {
        if (heapmgt->vmcode.start_addr != NULL)
            vmh_exit(&heapmgt->vmcode, heapmgt->vmheap.start_addr == NULL);
        if (heapmgt->vmheap.start_addr != NULL)
            vmh_exit(&heapmgt->vmheap, true);
        vmm_heap_exited = true;
    }
}

#ifdef UNIX
void
vmm_heap_fork_pre(dcontext_t *dcontext)
{
    if (!DYNAMO_OPTION(satisfy_w_xor_x))
        return;
    /* The child wants a private copy of our dual mapping setup, rather than
     * sharing the parent's.  Unfortunately that requires copying the entire
     * vmcode contents into new mappings.  To avoid a race while the child makes
     * this copy from our live mappings, we create a temp copy now.  The
     * disadvantage is that we need a bunch of free memory (and address space:
     * but this is 64-bit-only).  The alternative is to have the parent wait for
     * the child but that seems too disruptive to scheduling.
     */
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    heap_error_code_t error_code;
    /* We store in a dcontext field to avoid races with other threads doing forks. */
    th->fork_copy_size = heapmgt->vmcode.alloc_size;
    th->fork_copy_start =
        os_heap_reserve(NULL, th->fork_copy_size, &error_code, true /*+x*/);
    if (th->fork_copy_start == NULL) {
        report_w_xor_x_fatal_error_and_exit();
        ASSERT_NOT_REACHED();
    }

    /* Copy each mapping.  We also need to record the +*x protections (because some
     * are +rw (ELF data segments), some are +rx, and some are +r (reachable
     * (non-exec) heap)).  We can't use the actual page prot of the copy to store
     * what the vmcode prot should be, because some W^X implementations remove +x
     * from a +wx region, and we require +w to make our copy.  Thus we store the
     * mapping prots in a vmvector.
     */
    VMVECTOR_ALLOC_VECTOR(th->fork_copy_areas, dcontext,
                          VECTOR_NEVER_MERGE | VECTOR_NO_LOCK, innermost_lock);
    memquery_iter_t iter;
    if (!memquery_iterator_start(&iter, heapmgt->vmcode.alloc_start,
                                 true /*using heap*/)) {
        report_w_xor_x_fatal_error_and_exit();
        ASSERT_NOT_REACHED();
    }
    while (memquery_iterator_next(&iter) && iter.vm_start < heapmgt->vmcode.end_addr) {
        if (iter.vm_start < heapmgt->vmcode.alloc_start || iter.prot == MEMPROT_NONE)
            continue;
        byte *new_start =
            iter.vm_start - heapmgt->vmcode.alloc_start + th->fork_copy_start;
        vmvector_add(th->fork_copy_areas, new_start,
                     new_start + (iter.vm_end - iter.vm_start),
                     (void *)(ptr_uint_t)iter.prot);
        if (!os_heap_commit(new_start, iter.vm_end - iter.vm_start,
                            MEMPROT_READ | MEMPROT_WRITE, &error_code)) {
            report_w_xor_x_fatal_error_and_exit();
            ASSERT_NOT_REACHED();
        }
        memcpy(new_start, iter.vm_start, iter.vm_end - iter.vm_start);
        LOG(GLOBAL, LOG_HEAP, 2, "%s: copied %p-%p %x to %p-%p\n", __FUNCTION__,
            iter.vm_start, iter.vm_end, iter.prot, new_start,
            new_start + (iter.vm_end - iter.vm_start));
    }
    memquery_iterator_stop(&iter);
}

void
vmm_heap_fork_post(dcontext_t *dcontext, bool parent)
{
    if (!DYNAMO_OPTION(satisfy_w_xor_x) || !parent)
        return;
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    heap_error_code_t error_code;
    os_heap_free(th->fork_copy_start, th->fork_copy_size, &error_code);
    if (error_code != HEAP_ERROR_SUCCESS) {
        report_w_xor_x_fatal_error_and_exit();
        ASSERT_NOT_REACHED();
    }
    th->fork_copy_start = NULL;
    th->fork_copy_size = 0;
    vmvector_reset_vector(dcontext, th->fork_copy_areas);
    vmvector_delete_vector(dcontext, th->fork_copy_areas);
    th->fork_copy_areas = NULL;
}

void
vmm_heap_fork_init(dcontext_t *dcontext)
{
    if (!DYNAMO_OPTION(satisfy_w_xor_x))
        return;
    /* We want a private copy of our dual mapping setup, rather than sharing the
     * parent's.  Unfortunately that requires copying the entire vmcode contents
     * into new mappings.  The parent has made a temp copy for us to avoid races
     * if we tried to copy its live memory.
     */

    /* First, make a new file. */
    int old_fd = heapmgt->dual_map_file;
    heapmgt->dual_map_file =
        os_create_memory_file(MEMORY_FILE_NAME, heapmgt->vmcode.alloc_size);
    if (heapmgt->dual_map_file == INVALID_FILE)
        goto vmm_heap_fork_init_failed;
    LOG(GLOBAL, LOG_HEAP, 2, "%s: new dual_map_file is %d\n", __FUNCTION__,
        heapmgt->dual_map_file);

    /* Second, make a new +w region and copy the old protections and contents. */
    size_t map_size = heapmgt->vmcode.alloc_size;
    byte *map_base =
        os_map_file(heapmgt->dual_map_file, &map_size, 0, heapmgt->vmcode_writable_alloc,
                    MEMPROT_NONE, MAP_FILE_VMM_COMMIT | MAP_FILE_FIXED);
    if (map_base != heapmgt->vmcode_writable_alloc ||
        map_size != heapmgt->vmcode.alloc_size)
        goto vmm_heap_fork_init_failed;
    heap_error_code_t error_code;
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    vmvector_iterator_t vmvi;
    vmvector_iterator_start(th->fork_copy_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        byte *start, *end;
        uint prot = (uint)(ptr_uint_t)vmvector_iterator_next(&vmvi, &start, &end);
        byte *new_start = start - th->fork_copy_start + heapmgt->vmcode_writable_alloc;
        uint new_prot = (prot & ~(MEMPROT_EXEC)) | MEMPROT_WRITE;
        if (!os_heap_commit(new_start, end - start, new_prot, &error_code))
            goto vmm_heap_fork_init_failed;
        memcpy(new_start, start, end - start);
        LOG(GLOBAL, LOG_HEAP, 2, "%s: re-mapped %p-%p %x; copied from %p-%p %x\n",
            __FUNCTION__, new_start, new_start + (end - start), new_prot, start, end,
            prot);
    }
    vmvector_iterator_stop(&vmvi);

    /* Third, make a new +x region and set up the right protections and mappings. */
    map_size = heapmgt->vmcode.alloc_size;
    map_base =
        os_map_file(heapmgt->dual_map_file, &map_size, 0, heapmgt->vmcode.alloc_start,
                    MEMPROT_NONE, MAP_FILE_VMM_COMMIT | MAP_FILE_FIXED);
    if (map_base != heapmgt->vmcode.alloc_start || map_size != heapmgt->vmcode.alloc_size)
        goto vmm_heap_fork_init_failed;
    vmvector_iterator_start(th->fork_copy_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        byte *start, *end;
        uint prot = (uint)(ptr_uint_t)vmvector_iterator_next(&vmvi, &start, &end);
        byte *new_start = start - th->fork_copy_start + heapmgt->vmcode.alloc_start;
        map_size = end - start;
        map_base =
            os_map_file(heapmgt->dual_map_file, &map_size, start - th->fork_copy_start,
                        new_start, prot, MAP_FILE_VMM_COMMIT | MAP_FILE_FIXED);
        if (map_base != new_start || map_size != end - start)
            goto vmm_heap_fork_init_failed;
        LOG(GLOBAL, LOG_HEAP, 2, "%s: re-mapped %p-%p %x\n", __FUNCTION__, new_start,
            new_start + map_size, prot);
    }
    vmvector_iterator_stop(&vmvi);

    os_heap_free(th->fork_copy_start, th->fork_copy_size, &error_code);
    if (error_code != HEAP_ERROR_SUCCESS)
        goto vmm_heap_fork_init_failed;
    th->fork_copy_start = NULL;
    th->fork_copy_size = 0;
    vmvector_reset_vector(dcontext, th->fork_copy_areas);
    vmvector_delete_vector(dcontext, th->fork_copy_areas);
    th->fork_copy_areas = NULL;

    /* XXX: We don't want to unlink any tmpfs file so we don't use
     * os_delete_memory_file(). This may not work on Windows if that function needs to do
     * more.
     */
    os_close_protected(old_fd);
    return;

vmm_heap_fork_init_failed:
    report_w_xor_x_fatal_error_and_exit();
    ASSERT_NOT_REACHED();
}
#endif

/* checks for compatibility among heap options, returns true if
 * modified the value of any options to make them compatible
 */
bool
heap_check_option_compatibility()
{
    bool ret = false;

    ret = check_param_bounds(&dynamo_options.vm_size, MIN_VMM_HEAP_UNIT_SIZE,
                             MAX_VMCODE_SIZE, "vm_size") ||
        ret;
    ret = check_param_bounds(&dynamo_options.vmheap_size, MIN_VMM_HEAP_UNIT_SIZE,
                             MAX_VMHEAP_SIZE, "vmheap_size") ||
        ret;
#ifdef INTERNAL
    /* if max_heap_unit_size is too small you may get a funny message
     * "initial_heap_unit_size must be >= 8229 and <= 4096" but in
     * release build we will take the min and then complain about
     * max_heap_unit_size and set it to the min also, so it all works
     * out w/o needing an extra check() call.
     */
    /* case 7626: don't short-circuit checks, as later ones may be needed */
    ret = check_param_bounds(&dynamo_options.initial_heap_unit_size,
                             /* if have units smaller than a page we end up
                              * allocating 64KB chunks for "oversized" units
                              * for just about every alloc!  so round up to
                              * at least a page.
                              */
                             ALIGN_FORWARD(UNITOVERHEAD + 1, (uint)PAGE_SIZE),
                             HEAP_UNIT_MAX_SIZE, "initial_heap_unit_size") ||
        ret;
    ret = check_param_bounds(&dynamo_options.initial_global_heap_unit_size,
                             ALIGN_FORWARD(UNITOVERHEAD + 1, (uint)PAGE_SIZE),
                             HEAP_UNIT_MAX_SIZE, "initial_global_heap_unit_size") ||
        ret;
    ret = check_param_bounds(&dynamo_options.max_heap_unit_size,
                             MAX(HEAP_UNIT_MIN_SIZE, GLOBAL_UNIT_MIN_SIZE), INT_MAX,
                             "max_heap_unit_size") ||
        ret;
#endif
    return ret;
}

/* thread-shared initialization that should be repeated after a reset */
void
heap_reset_init()
{
    threadunits_init(GLOBAL_DCONTEXT, &heapmgt->global_nonpersistent_units,
                     GLOBAL_UNIT_MIN_SIZE, false);
}

/* initialization */
void
d_r_heap_init()
{
    int i;
    DEBUG_DECLARE(uint prev_sz = 0;)

    LOG(GLOBAL, LOG_TOP | LOG_HEAP, 2, "Heap bucket sizes are:\n");
    /* make sure we'll preserve alignment */
    ASSERT(ALIGNED(HEADER_SIZE, HEAP_ALIGNMENT));
    /* make sure free list pointers will fit */
    ASSERT(BLOCK_SIZES[0] >= sizeof(heap_pc *));
    /* since sizes depend on size of structs, make sure they're in order */
    for (i = 0; i < BLOCK_TYPES; i++) {
        ASSERT(BLOCK_SIZES[i] > prev_sz);
        /* we assume all of our heap allocs are aligned */
        ASSERT(i == BLOCK_TYPES - 1 || ALIGNED(BLOCK_SIZES[i], HEAP_ALIGNMENT));
        DODEBUG(prev_sz = BLOCK_SIZES[i];);
        LOG(GLOBAL, LOG_TOP | LOG_HEAP, 2, "\t%d bytes\n", BLOCK_SIZES[i]);
    }

    /* we assume writes to some static vars are atomic,
     * i.e., the vars don't cross cache lines.  they shouldn't since
     * they should all be 4-byte-aligned in the data segment.
     * FIXME: ensure that release build aligns ok?
     * I would be quite surprised if static vars were not 4-byte-aligned!
     */
    ASSERT(ALIGN_BACKWARD(&heap_exiting, CACHE_LINE_SIZE()) ==
           ALIGN_BACKWARD(&heap_exiting + 1, CACHE_LINE_SIZE()));
    ASSERT(ALIGN_BACKWARD(&heap_unit_lock.owner, CACHE_LINE_SIZE()) ==
           ALIGN_BACKWARD(&heap_unit_lock.owner + 1, CACHE_LINE_SIZE()));

    /* For simplicity we go through our normal heap mechanism to allocate
     * our post-init heapmgt struct
     */
    ASSERT(heapmgt == &temp_heapmgt);
    heapmgt->global_heap_writable = true; /* this is relied on in global_heap_alloc */
    threadunits_init(GLOBAL_DCONTEXT, &heapmgt->global_units, GLOBAL_UNIT_MIN_SIZE,
                     false);

    heapmgt =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, heap_management_t, ACCT_MEM_MGT, PROTECTED);
    ASSERT(sizeof(temp_heapmgt) == sizeof(*heapmgt));
    memcpy(heapmgt, &temp_heapmgt, sizeof(temp_heapmgt));

    threadunits_init(GLOBAL_DCONTEXT, &heapmgt->global_unprotected_units,
                     GLOBAL_UNIT_MIN_SIZE, false);
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        threadunits_init(GLOBAL_DCONTEXT, &heapmgt->global_reachable_units,
                         GLOBAL_UNIT_MIN_SIZE, true);
    }
    heap_reset_init();

#ifdef WINDOWS
    /* PR 250294: As part of 64-bit hook work, hook reachability was addressed
     * using landing pads (see win32/callback.c for more explanation).  Landing
     * pad areas are a type of special heap, so they should be initialized
     * during heap init.
     * Each landing pad area has its own allocation pointer, so they shouldn't
     * be merged automatically.
     */
    VMVECTOR_ALLOC_VECTOR(landing_pad_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, landing_pad_areas_lock);
#endif
}

/* need to not remove from vmareas on process exit -- vmareas has already exited! */
static void
really_free_unit(heap_unit_t *u)
{
    RSTATS_SUB(heap_capacity, UNIT_COMMIT_SIZE(u));
    STATS_ADD(heap_reserved_only,
              (stats_int_t)(UNIT_COMMIT_SIZE(u) - UNIT_RESERVED_SIZE(u)));
    /* remember that u itself is inside unit, not separately allocated */
    release_guarded_real_memory((vm_addr_t)u, UNIT_RESERVED_SIZE(u),
                                false /*do not update DR areas now*/, true, u->which);
}

/* Free all thread-shared state not critical to forward progress;
 * heap_reset_init() will be called before continuing.
 */
void
heap_reset_free()
{
    heap_unit_t *u, *next_u;
    /* FIXME: share some code w/ heap_exit -- currently only called by reset */
    ASSERT(DYNAMO_OPTION(enable_reset));

    /* we must grab this lock before heap_unit_lock to avoid rank
     * order violations when freeing
     */
    dynamo_vm_areas_lock();

    /* for combining stats into global_units we need this lock
     * FIXME: remove if we go to separate stats sum location
     */
    DODEBUG({ acquire_recursive_lock(&global_alloc_lock); });

    acquire_recursive_lock(&heap_unit_lock);

    LOG(GLOBAL, LOG_HEAP, 1, "Pre-reset, global heap unit stats:\n");
    /* FIXME: free directly rather than putting on dead list first */
    threadunits_exit(&heapmgt->global_nonpersistent_units, GLOBAL_DCONTEXT);

    /* free all dead units */
    u = heapmgt->heap.dead;
    while (u != NULL) {
        next_u = u->next_global;
        LOG(GLOBAL, LOG_HEAP, 1, "\tfreeing dead unit " PFX "-" PFX " [-" PFX "]\n", u,
            UNIT_COMMIT_END(u), UNIT_RESERVED_END(u));
        RSTATS_DEC(heap_num_free);
        really_free_unit(u);
        u = next_u;
    }
    heapmgt->heap.dead = NULL;
    heapmgt->heap.num_dead = 0;
    release_recursive_lock(&heap_unit_lock);
    DODEBUG({ release_recursive_lock(&global_alloc_lock); });
    dynamo_vm_areas_unlock();
}

/* atexit cleanup */
void
d_r_heap_exit()
{
    heap_unit_t *u, *next_u;
    heap_management_t *temp;

    heap_exiting = true;
    /* FIXME: we shouldn't need either lock if executed last */
    dynamo_vm_areas_lock();
    acquire_recursive_lock(&heap_unit_lock);

#ifdef WINDOWS
    release_landing_pad_mem(); /* PR 250294 */
#endif

    LOG(GLOBAL, LOG_HEAP, 1, "Global unprotected heap unit stats:\n");
    threadunits_exit(&heapmgt->global_unprotected_units, GLOBAL_DCONTEXT);
    LOG(GLOBAL, LOG_HEAP, 1, "Global nonpersistent heap unit stats:\n");
    threadunits_exit(&heapmgt->global_nonpersistent_units, GLOBAL_DCONTEXT);
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        LOG(GLOBAL, LOG_HEAP, 1, "Global reachable heap unit stats:\n");
        threadunits_exit(&heapmgt->global_reachable_units, GLOBAL_DCONTEXT);
    }

    /* Now we need to go back to the static struct to clean up */
    ASSERT(heapmgt != &temp_heapmgt);
    /* We need to maintain the lock process list which was using the temp_heapmgt
     * lock structure.
     */
    mutex_t temp_vmcode = temp_heapmgt.vmcode.lock;
    mutex_t temp_vmheap = temp_heapmgt.vmheap.lock;
    memcpy(&temp_heapmgt, heapmgt, sizeof(temp_heapmgt));
    temp_heapmgt.vmcode.lock = temp_vmcode;
    temp_heapmgt.vmheap.lock = temp_vmheap;
    temp = heapmgt;
    heapmgt = &temp_heapmgt;
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, temp, heap_management_t, ACCT_MEM_MGT, PROTECTED);

    LOG(GLOBAL, LOG_HEAP, 1, "Global heap unit stats:\n");
    threadunits_exit(&heapmgt->global_units, GLOBAL_DCONTEXT);

    /* free heap for all unfreed units */
    LOG(GLOBAL, LOG_HEAP, 1, "Unfreed units:\n");
    u = heapmgt->heap.units;
    while (u != NULL) {
        next_u = u->next_global;
        LOG(GLOBAL, LOG_HEAP, 1, "\tfreeing live unit " PFX "-" PFX " [-" PFX "]\n", u,
            UNIT_COMMIT_END(u), UNIT_RESERVED_END(u));
        RSTATS_DEC(heap_num_live);
        really_free_unit(u);
        u = next_u;
    }
    heapmgt->heap.units = NULL;
    u = heapmgt->heap.dead;
    while (u != NULL) {
        next_u = u->next_global;
        LOG(GLOBAL, LOG_HEAP, 1, "\tfreeing dead unit " PFX "-" PFX " [-" PFX "]\n", u,
            UNIT_COMMIT_END(u), UNIT_RESERVED_END(u));
        RSTATS_DEC(heap_num_free);
        really_free_unit(u);
        u = next_u;
    }
    heapmgt->heap.dead = NULL;
    heapmgt->global_heap_writable = false; /* This is relied on in global_heap_alloc. */
    release_recursive_lock(&heap_unit_lock);
    dynamo_vm_areas_unlock();

    DELETE_RECURSIVE_LOCK(heap_unit_lock);
    DELETE_RECURSIVE_LOCK(global_alloc_lock);
    DELETE_RECURSIVE_LOCK(low_on_memory_pending_lock);

#ifdef X64
    DELETE_LOCK(request_region_be_heap_reachable_lock);
#endif

    if (doing_detach) {
        heapmgt = &temp_heapmgt;
        IF_X64(reset_heap_reachable_bounds());
    }
}

void
heap_post_exit()
{
    heap_exiting = false;
}

/* FIXME:
 * detect if the app is who we're fighting for memory, if so, don't
 * free memory, else the app will just keep grabbing more.
 * need a test for hitting 2GB (or 3GB!) user mode limit.
 */
static void
heap_low_on_memory()
{
    /* free some memory! */
    heap_unit_t *u, *next_u;
    DEBUG_DECLARE(size_t freed = 0;)
    LOG(GLOBAL, LOG_CACHE | LOG_STATS, 1,
        "heap_low_on_memory: about to free dead list units\n");
    /* WARNING: this routine is called at arbitrary allocation failure points,
     * so we have to be careful what locks we grab
     * However, no allocation site can hold a lock weaker in rank than
     * heap_unit_lock, b/c it could deadlock on the allocation itself!
     * So we're safe.
     */
    /* must grab this lock prior to heap_unit_lock if affecting DR vm areas
     * this is recursive so ok if we ran out of memory while holding DR vm area lock
     */
    ASSERT(safe_to_allocate_or_free_heap_units());
    dynamo_vm_areas_lock();
    acquire_recursive_lock(&heap_unit_lock);
    u = heapmgt->heap.dead;
    while (u != NULL) {
        next_u = u->next_global;
        DODEBUG(freed += UNIT_COMMIT_SIZE(u););
        /* FIXME: if out of committed pages only, could keep our reservations */
        LOG(GLOBAL, LOG_HEAP, 1, "\tfreeing dead unit " PFX "-" PFX " [-" PFX "]\n", u,
            UNIT_COMMIT_END(u), UNIT_RESERVED_END(u));
        RSTATS_DEC(heap_num_free);
        really_free_unit(u);
        u = next_u;
        heapmgt->heap.num_dead--;
    }
    heapmgt->heap.dead = NULL;
    release_recursive_lock(&heap_unit_lock);
    dynamo_vm_areas_unlock();
    LOG(GLOBAL, LOG_CACHE | LOG_STATS, 1, "heap_low_on_memory: freed %d KB\n",
        freed / 1024);
    /* FIXME: we don't keep a list of guard pages, which we may decide to throw
     * out or compact at this time.
     */
    /* FIXME: should also fix up the allocator to look in other free lists
     * of sizes larger than asked for, we may have plenty of memory available
     * in other lists!  see comments in common_heap_alloc
     */
}

static const char *
get_oom_source_name(oom_source_t source)
{
    /* currently only single character codenames,
     * (still as a string though)
     */
    const char *code_name = "?";

    switch (source) {
    case OOM_INIT: code_name = "I"; break;
    case OOM_RESERVE: code_name = "R"; break;
    case OOM_COMMIT: code_name = "C"; break;
    case OOM_EXTEND: code_name = "E"; break;
    default: ASSERT_NOT_REACHED();
    }
    return code_name;
}

static bool
silent_oom_for_process(oom_source_t source)
{
    if (TESTANY(OOM_COMMIT | OOM_EXTEND, source) &&
        !IS_STRING_OPTION_EMPTY(silent_commit_oom_list)) {
        bool onlist;
        const char *process_name = get_short_name(get_application_name());
        string_option_read_lock();
        onlist = check_filter_with_wildcards(DYNAMO_OPTION(silent_commit_oom_list),
                                             process_name);
        string_option_read_unlock();

        if (onlist) {
            SYSLOG_INTERNAL_WARNING("not reporting last words of executable %s",
                                    process_name);
            return true;
        }
    }
    return false;
}

/* oom_source_t identifies the action we were taking, os_error_code is
 * the returned value from the last system call - opaque at this OS
 * independent layer.
 */
static void
report_low_on_memory(which_vmm_t which, oom_source_t source,
                     heap_error_code_t os_error_code)
{
    if (TESTANY(DYNAMO_OPTION(silent_oom_mask), source) ||
        silent_oom_for_process(source)) {
        SYSLOG_INTERNAL_WARNING("Mostly silent OOM: %s " PFX ".\n",
                                get_oom_source_name(source), os_error_code);
        /* still produce an ldmp for internal use */
        if (TEST(DUMPCORE_OUT_OF_MEM_SILENT, DYNAMO_OPTION(dumpcore_mask)))
            os_dump_core("Out of memory, silently aborting program.");
    } else {
        const char *oom_source_code = get_oom_source_name(source);
        char type_hex[19];
        snprintf(type_hex, BUFFER_SIZE_ELEMENTS(type_hex), PFX, which);
        NULL_TERMINATE_BUFFER(type_hex);
        char status_hex[19];
        snprintf(status_hex, BUFFER_SIZE_ELEMENTS(status_hex), PFX, os_error_code);
        NULL_TERMINATE_BUFFER(status_hex);
        /* SYSLOG first */
        SYSLOG(SYSLOG_CRITICAL, OUT_OF_MEMORY, 4, get_application_name(),
               get_application_pid(), oom_source_code, type_hex, status_hex);
        /* Stats can be very useful to diagnose why we hit OOM. */
        if (INTERNAL_OPTION(rstats_to_stderr))
            dump_global_rstats_to_stderr();

        /* XXX: case 7296 - ldmp even if we have decided not to produce an event above */
        if (TEST(DUMPCORE_OUT_OF_MEM, DYNAMO_OPTION(dumpcore_mask)))
            os_dump_core("Out of memory, aborting program.");

        /* pass only status code to XML where we should have a stack dump and callstack */
        report_diagnostics("Out of memory", status_hex, NO_VIOLATION_BAD_INTERNAL_STATE);
    }
    os_terminate(NULL, TERMINATE_PROCESS);
    ASSERT_NOT_REACHED();
}

/* update statistics for committed memory, and add to vm_areas */
static inline void
account_for_memory(void *p, size_t size, uint prot, bool add_vm,
                   bool image _IF_DEBUG(const char *comment))
{
    RSTATS_ADD_PEAK(memory_capacity, size);

    /* case 3045: areas inside the vmheap reservation are not added to the list
     * for clients that use DR-allocated memory, we have get_memory_info()
     * query from the OS to see inside
     */
    if (is_vmm_reserved_address(p, size, NULL, NULL)) {
        return;
    }

    if (add_vm) {
        add_dynamo_vm_area(p, ((app_pc)p) + size, prot, image _IF_DEBUG(comment));
    } else {
        /* due to circular dependencies bet vmareas and global heap we do not call
         * add_dynamo_vm_area here, instead we indicate that something has changed
         */
        mark_dynamo_vm_areas_stale();
        /* NOTE: 'prot' info is lost about this region, but is needed in
         * heap_vmareas_synch_units to update all_memory_areas.  Currently
         * heap_create_unit is the only place that passes 'false' with prot rw-.
         */
        ASSERT(TESTALL(MEMPROT_READ | MEMPROT_WRITE, prot));
    }
}

/* remove_vm MUST be false iff this is heap memory, which is updated separately */
static void
update_dynamo_areas_on_release(app_pc start, app_pc end, bool remove_vm)
{
    if (!vm_areas_exited && !heap_exiting) { /* avoid problems when exiting */
        /* case 3045: areas inside the vmheap reservation are not added to the list
         * for clients that use DR-allocated memory, we have get_memory_info()
         * query from the OS to see inside
         */
        if (is_vmm_reserved_address(start, end - start, NULL, NULL)) {
            return;
        }
        if (remove_vm) {
            remove_dynamo_vm_area(start, end);
        } else {
            /* Due to cyclic dependencies bet heap and vmareas we cannot remove
             * incrementally.  The pending set is protected by the same lock
             * needed to synch the vm areas, so we will never mis-identify free
             * memory as DR memory.
             */
            mark_dynamo_vm_areas_stale();
            dynamo_areas_pending_remove = true;
        }
    }
}

bool
lockwise_safe_to_allocate_memory()
{
    /* check whether it's safe to hold a lock that normally can be held
     * for memory allocation -- i.e., check whether we hold the
     * global_alloc_lock
     */
    return !self_owns_recursive_lock(&global_alloc_lock);
}

/* Reserves space inside the VMM region which can be used by the caller for mapping
 * mapping a file.  First attempts to reserve at "preferred" but if that fails it
 * attempts at any available location.
 */
byte *
heap_reserve_for_external_mapping(byte *preferred, size_t size, which_vmm_t which)
{
#ifdef WINDOWS
    /* TODO i#3570: Add Windows support, which is complex as we cannot map a file
     * on top of an existing reservation; nor can we un-reserve a piece of a
     * reservation.  See the issue for solution ideas.
     */
    ASSERT_NOT_IMPLEMENTED(false && "i#3570");
    return NULL;
#endif
    vm_addr_t p = NULL;
    vm_heap_t *vmh = vmheap_for_which(which);
    ASSERT(size > 0);
    size = ALIGN_FORWARD(size, PAGE_SIZE);
    if (!DYNAMO_OPTION(vm_reserve))
        return NULL;
    if (preferred >= vmh->start_addr && preferred + size <= vmh->end_addr)
        p = vmm_heap_reserve_blocks(vmh, size, preferred, which);
    if (p == NULL)
        p = vmm_heap_reserve_blocks(vmh, size, NULL, which);
    LOG(GLOBAL, LOG_HEAP, 2, "%s %s: size=%d p=" PFX "\n", __FUNCTION__, vmh->name, size,
        p);
    return p;
}

/* Before calling this function, the caller must restore [p,p+size) to
 * its state from before heap_reserve_for_external_mapping() was
 * called: reserved but not committed.
 */
bool
heap_unreserve_for_external_mapping(byte *p, size_t size, which_vmm_t which)
{
#ifdef WINDOWS
    /* TODO i#3570: Add Windows support, which is complex as we cannot map a file
     * on top of an existing reservation; nor can we un-reserve a piece of a
     * reservation.  See the issue for solution ideas.
     */
    ASSERT_NOT_IMPLEMENTED(false && "i#3570");
    return false;
#endif
    vm_heap_t *vmh = vmheap_for_which(which);
    ASSERT(size > 0);
    size = ALIGN_FORWARD(size, PAGE_SIZE);
    if (!DYNAMO_OPTION(vm_reserve) || !is_vmm_reserved_address(p, size, NULL, NULL))
        return false;
    vmm_heap_free_blocks(vmh, p, size, which);
    LOG(GLOBAL, LOG_HEAP, 2, "%s %s: size=%d p=" PFX "\n", __FUNCTION__, vmh->name, size,
        p);
    return true;
}

/* we indirect all os memory requests through here so we have a central place
 * to handle the out-of-memory condition.
 * add_vm MUST be false iff this is heap memory, which is updated separately.
 */
static void *
get_real_memory(size_t size, uint prot, bool add_vm,
                which_vmm_t which _IF_DEBUG(const char *comment))
{
    void *p;
    heap_error_code_t error_code;
    /* must round up to page sizes, else vmm_heap_alloc assert triggers */
    size = ALIGN_FORWARD(size, PAGE_SIZE);

    /* memory alloc/dealloc and updating DR list must be atomic */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */

    p = vmm_heap_alloc(size, prot, &error_code, which);
    if (p == NULL) {
        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory -- cannot reserve or "
                                     "commit %dKB.  Trying to recover.",
                                     size / 1024);
        /* we should be ok here, shouldn't come in here holding global_alloc_lock
         * or heap_unit_lock w/o first having grabbed DR areas lock
         */
        ASSERT(safe_to_allocate_or_free_heap_units());
        heap_low_on_memory();
        fcache_low_on_memory();
        /* try again
         * FIXME: have more sophisticated strategy of freeing a little, then getting
         * more drastic with each subsequent failure
         * FIXME: can only free live fcache units for current thread w/ current
         * impl...should we wait a while and try again if out of memory, hoping
         * other threads have freed some?!?!
         */
        p = vmm_heap_alloc(size, prot, &error_code, which);
        if (p == NULL) {
            report_low_on_memory(which, OOM_RESERVE, error_code);
        }
        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory -- but still alive after "
                                     "emergency free.");
    }

    account_for_memory(p, size, prot, add_vm, false _IF_DEBUG(comment));
    dynamo_vm_areas_unlock();

    return p;
}

static void
release_memory_and_update_areas(app_pc p, size_t size, bool decommit, bool remove_vm,
                                which_vmm_t which)
{
    heap_error_code_t error_code;
    /* these two operations need to be atomic wrt DR area updates */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    /* ref case 3035, we must remove from dynamo_areas before we free in case
     * we end up allocating memory in the process of removing the area
     * (we don't want to end up getting the memory we just freed since that
     *  would lead to errors in the list when we finally did remove it)
     */
    update_dynamo_areas_on_release(p, p + size, remove_vm);
    if (decommit)
        vmm_heap_decommit(p, size, &error_code, which);
    else
        vmm_heap_free(p, size, &error_code, which);
    ASSERT(error_code == HEAP_ERROR_SUCCESS);
    dynamo_vm_areas_unlock();
}

/* remove_vm MUST be false iff this is heap memory, which is updated separately */
static void
release_real_memory(void *p, size_t size, bool remove_vm, which_vmm_t which)
{
    /* must round up to page sizes for vmm_heap_free */
    size = ALIGN_FORWARD(size, PAGE_SIZE);

    release_memory_and_update_areas((app_pc)p, size, false /*free*/, remove_vm, which);

    /* avoid problem w/ being called by cleanup_and_terminate after dynamo_process_exit */
    if (IF_DEBUG_ELSE(!dynamo_exited_log_and_stats, true))
        RSTATS_SUB(memory_capacity, size);
}

static void
extend_commitment(vm_addr_t p, size_t size, uint prot, bool initial_commit,
                  which_vmm_t which)
{
    heap_error_code_t error_code;
    ASSERT(ALIGNED(p, PAGE_SIZE));
    size = ALIGN_FORWARD(size, PAGE_SIZE);
    if (!vmm_heap_commit(p, size, prot, &error_code, which)) {
        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory - cannot extend commit "
                                     "%dKB. Trying to recover.",
                                     size / 1024);
        heap_low_on_memory();
        fcache_low_on_memory();
        /* see low-memory ideas in get_real_memory */
        if (!vmm_heap_commit(p, size, prot, &error_code, which)) {
            report_low_on_memory(which, initial_commit ? OOM_COMMIT : OOM_EXTEND,
                                 error_code);
        }

        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory in extend - still alive "
                                     "after emergency free.");
    }
}

/* A wrapper around get_real_memory that adds a guard page on each side of the
 * requested unit.  These should consume only uncommitted virtual address and
 * should not use any physical memory.
 * add_vm MUST be false iff this is heap memory, which is updated separately.
 * Non-NULL min_addr is only supported for stack allocations (DrMi#1723).
 */
static vm_addr_t
get_guarded_real_memory(size_t reserve_size, size_t commit_size, uint prot, bool add_vm,
                        bool guarded, byte *min_addr,
                        which_vmm_t which _IF_DEBUG(const char *comment))
{
    vm_addr_t p = NULL;
    uint guard_size = (uint)PAGE_SIZE;
    heap_error_code_t error_code;
    bool try_vmm = true;
    ASSERT(reserve_size >= commit_size);
    if (!guarded || !has_guard_pages(which)) {
        if (reserve_size == commit_size)
            return get_real_memory(reserve_size, prot, add_vm, which _IF_DEBUG(comment));
        guard_size = 0;
    }

    reserve_size = ALIGN_FORWARD(reserve_size, PAGE_SIZE);
    commit_size = ALIGN_FORWARD(commit_size, PAGE_SIZE);

    reserve_size += 2 * guard_size; /* add top and bottom guards */

    /* memory alloc/dealloc and updating DR list must be atomic */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */

#ifdef WINDOWS
    /* DrMi#1723: if we swap TEB stack fields, a client (or a DR app mem touch)
     * can trigger an app guard
     * page.  We have to ensure that the kernel will update TEB.StackLimit in that
     * case, which requires our dstack to be higher than the app stack.
     * This results in more fragmentation and larger dynamo_areas so we avoid
     * if we can.  We could consider a 2nd vm_reserve region just for stacks.
     */
    if (SWAP_TEB_STACKBASE() && (!DYNAMO_OPTION(vm_reserve) && min_addr > NULL) ||
        (DYNAMO_OPTION(vm_reserve) && min_addr > vmheap_get_start())) {
        try_vmm = false;
    }
#endif

    if (try_vmm)
        p = vmm_heap_reserve(reserve_size, &error_code, TEST(MEMPROT_EXEC, prot), which);

#ifdef WINDOWS
    if (!try_vmm || p < (vm_addr_t)min_addr) {
        if (p != NULL)
            vmm_heap_free(p, reserve_size, &error_code, which);
        p = os_heap_reserve_in_region((void *)ALIGN_FORWARD(min_addr, PAGE_SIZE),
                                      (void *)PAGE_START(POINTER_MAX), reserve_size,
                                      &error_code, TEST(MEMPROT_EXEC, prot));
        /* No reason to update heap-reachable b/c stack doesn't need to reach
         * (min_addr != NULL assumed to be stack).
         */
        ASSERT(!DYNAMO_OPTION(stack_shares_gencode)); /* would break reachability */
        /* If it fails we can't do much: we fall back to within-vmm, if possible,
         * and rely on our other best-effort TEB.StackLimit updating checks
         * (check_app_stack_limit()).
         */
        if (p == NULL) {
            SYSLOG_INTERNAL_WARNING_ONCE("Unable to allocate dstack above app stack");
            if (!try_vmm) {
                p = vmm_heap_reserve(reserve_size, &error_code, TEST(MEMPROT_EXEC, prot),
                                     which);
            }
        }
    }
#endif

    if (p == NULL) {
        /* Very unlikely to happen: we have to reach at least 2GB reserved memory. */
        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory - cannot reserve %dKB. "
                                     "Trying to recover.",
                                     reserve_size / 1024);
        heap_low_on_memory();
        fcache_low_on_memory();

        p = vmm_heap_reserve(reserve_size, &error_code, TEST(MEMPROT_EXEC, prot), which);
        if (p == NULL) {
            report_low_on_memory(which, OOM_RESERVE, error_code);
        }

        SYSLOG_INTERNAL_WARNING_ONCE("Out of memory on reserve - but still "
                                     "alive after emergency free.");
    }
    /* includes guard pages if add_vm -- else, heap_vmareas_synch_units() will
     * add guard pages in by assuming one page on each side of every heap unit
     * if dynamo_options.guard_pages
     */
    account_for_memory((void *)p, reserve_size, prot, add_vm, false _IF_DEBUG(comment));
    dynamo_vm_areas_unlock();

    STATS_ADD_PEAK(reserved_memory_capacity, reserve_size);
    STATS_ADD_PEAK(guard_pages, 2);

    p += guard_size;
    extend_commitment(p, commit_size, prot, true /* initial commit */, which);

    return p;
}

/* A wrapper around get_release_memory that also frees the guard pages on each
 * side of the requested unit.  remove_vm MUST be false iff this is heap memory,
 * which is updated separately.
 */
static void
release_guarded_real_memory(vm_addr_t p, size_t size, bool remove_vm, bool guarded,
                            which_vmm_t which)
{
    if (!guarded || !has_guard_pages(which)) {
        release_real_memory(p, size, remove_vm, which);
        return;
    }

    size = ALIGN_FORWARD(size, PAGE_SIZE);
    size += PAGE_SIZE * 2; /* add top and bottom guards */
    p -= PAGE_SIZE;

    release_memory_and_update_areas((app_pc)p, size, false /*free*/, remove_vm, which);

    /* avoid problem w/ being called by cleanup_and_terminate after dynamo_process_exit */
    if (IF_DEBUG_ELSE(!dynamo_exited_log_and_stats, true)) {
        RSTATS_SUB(memory_capacity, size);
        STATS_SUB(reserved_memory_capacity, size);
        STATS_ADD(guard_pages, -2);
    }
}

/* use heap_mmap to allocate large chunks of executable memory
 * it's mainly used to allocate our fcache units
 */
void *
heap_mmap_ex(size_t reserve_size, size_t commit_size, uint prot, bool guarded,
             which_vmm_t which)
{
    void *p = get_guarded_real_memory(reserve_size, commit_size, prot, true, guarded,
                                      NULL, which _IF_DEBUG("heap_mmap"));
#ifdef DEBUG_MEMORY
    if (TEST(MEMPROT_WRITE, prot))
        memset(vmm_get_writable_addr(p, which), HEAP_ALLOCATED_BYTE, commit_size);
#endif
    /* We rely on this for freeing _post_stack in absence of dcontext */
    ASSERT(!DYNAMO_OPTION(vm_reserve) || !DYNAMO_OPTION(stack_shares_gencode) ||
           (ptr_uint_t)p - (guarded ? (GUARD_PAGE_ADJUSTMENT / 2) : 0) ==
               ALIGN_BACKWARD(p, DYNAMO_OPTION(vmm_block_size)) ||
           at_reset_at_vmm_limit(vmheap_for_which(which)));
    LOG(GLOBAL, LOG_HEAP, 2, "heap_mmap: %d bytes [/ %d] @ " PFX "\n", commit_size,
        reserve_size, p);
    STATS_ADD_PEAK(mmap_capacity, commit_size);
    STATS_ADD_PEAK(mmap_reserved_only, (reserve_size - commit_size));
    return p;
}

/* Use heap_mmap to allocate large chunks of memory. */
void *
heap_mmap_reserve(size_t reserve_size, size_t commit_size, uint prot, which_vmm_t which)
{
    return heap_mmap_ex(reserve_size, commit_size, prot, true, which);
}

/* It is up to the caller to ensure commit_size is a page size multiple,
 * and that it does not extend beyond the initial reservation.
 */
void
heap_mmap_extend_commitment(void *p, size_t commit_size, which_vmm_t which)
{
    extend_commitment(p, commit_size, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
                      false /*not initial commit*/, which);
    STATS_SUB(mmap_reserved_only, commit_size);
    STATS_ADD_PEAK(mmap_capacity, commit_size);
#ifdef DEBUG_MEMORY
    memset(vmm_get_writable_addr(p, which), HEAP_ALLOCATED_BYTE, commit_size);
#endif
}

/* De-commits from a committed region. */
void
heap_mmap_retract_commitment(void *retract_start, size_t decommit_size, which_vmm_t which)
{
    heap_error_code_t error_code;
    ASSERT(ALIGNED(decommit_size, PAGE_SIZE));
    vmm_heap_decommit(retract_start, decommit_size, &error_code, which);
    STATS_ADD(mmap_reserved_only, decommit_size);
    STATS_ADD_PEAK(mmap_capacity, -(stats_int_t)decommit_size);
}

/* Allocates executable memory in the same allocation region as this thread's
 * stack, to save address space (case 9474).
 * Doing so is only supported for -reachable_heap (but for x64 we don't have
 * private gencode in any case).
 */
void *
heap_mmap_reserve_post_stack(dcontext_t *dcontext, size_t reserve_size,
                             size_t commit_size, uint prot, which_vmm_t which)
{
    void *p;
    byte *stack_reserve_end = NULL;
    heap_error_code_t error_code;
    size_t available = 0;
    uint cur_prot = 0; /* avoid compiler warning */
    bool known_stack = false;
    vm_heap_t *vmh = vmheap_for_which(which);
    ASSERT(reserve_size > 0 && commit_size < reserve_size);
    /* 1.5 * guard page adjustment since we'll share the middle one */
    if (DYNAMO_OPTION(stack_size) + reserve_size + GUARD_PAGE_ADJUSTMENT +
            GUARD_PAGE_ADJUSTMENT / 2 >
        DYNAMO_OPTION(vmm_block_size)) {
        /* there's not enough room to share the allocation block, stack is too big */
        LOG(GLOBAL, LOG_HEAP, 1,
            "Not enough room to allocate 0x%08x bytes post stack "
            "of size 0x%08x\n",
            reserve_size, DYNAMO_OPTION(stack_size));
        return heap_mmap_reserve(reserve_size, commit_size, prot, which);
    }
    if (DYNAMO_OPTION(stack_shares_gencode) &&
        /* FIXME: we could support this w/o vm_reserve, or when beyond
         * the reservation, but we don't bother */
        DYNAMO_OPTION(vm_reserve) && dcontext != GLOBAL_DCONTEXT && dcontext != NULL) {
        stack_reserve_end = dcontext->dstack + GUARD_PAGE_ADJUSTMENT / 2;
#if defined(UNIX) && !defined(HAVE_MEMINFO)
        if (!dynamo_initialized) {
            /* memory info is not yet set up.  since so early we only support
             * post-stack if inside vmm (won't be true only for pathological
             * tiny vmm sizes)
             */
            if (vmm_is_reserved_unit(vmh, stack_reserve_end, reserve_size)) {
                known_stack = true;
                available = reserve_size;
            } else
                known_stack = false;
        } else
#elif defined(UNIX)
        /* the all_memory_areas list doesn't keep details inside vmheap */
        known_stack =
            get_memory_info_from_os(stack_reserve_end, NULL, &available, &cur_prot);
#else
        known_stack = get_memory_info(stack_reserve_end, NULL, &available, &cur_prot);
#endif
            /* If ever out of vmheap, then may have free space beyond stack,
             * which we could support but don't (see FIXME above) */
            ASSERT(out_of_vmheap_once ||
                   (known_stack && available >= reserve_size && cur_prot == 0));
    }
    if (!known_stack ||
        /* if -no_vm_reserve will short-circuit so no vmh deref danger */
        !vmm_in_same_block(vmh, dcontext->dstack,
                           /* we do want a guard page at the end */
                           stack_reserve_end + reserve_size) ||
        available < reserve_size) {
        ASSERT(!DYNAMO_OPTION(stack_shares_gencode) || !DYNAMO_OPTION(vm_reserve) ||
               out_of_vmheap_once);
        DOLOG(1, LOG_HEAP, {
            if (known_stack && available < reserve_size) {
                LOG(GLOBAL, LOG_HEAP, 1,
                    "heap_mmap_reserve_post_stack: avail %d < needed %d\n", available,
                    reserve_size);
            }
        });
        STATS_INC(mmap_no_share_stack_region);
        return heap_mmap_reserve(reserve_size, commit_size, prot, which);
    }
    ASSERT(DYNAMO_OPTION(vm_reserve));
    ASSERT(REACHABLE_HEAP());
    ASSERT(stack_reserve_end != NULL);
    /* memory alloc/dealloc and updating DR list must be atomic */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    /* We share the stack's end guard page as our start guard page */
    if (vmm_is_reserved_unit(vmh, stack_reserve_end, reserve_size)) {
        /* Memory is already reserved with OS */
        p = stack_reserve_end;
    } else {
        p = os_heap_reserve(stack_reserve_end, reserve_size, &error_code, true /*+x*/);
#ifdef X64
        /* ensure future heap allocations are reachable from this allocation
         * (this will also verify that this region meets reachability requirements) */
        if (p != NULL)
            request_region_be_heap_reachable(p, reserve_size);
#endif
        if (p == NULL) {
            ASSERT_NOT_REACHED();
            LOG(GLOBAL, LOG_HEAP, 1,
                "heap_mmap_reserve_post_stack: reserve failed " PFX "\n", error_code);
            dynamo_vm_areas_unlock();
            STATS_INC(mmap_no_share_stack_region);
            return heap_mmap_reserve(reserve_size, commit_size, prot, which);
        }
        ASSERT(error_code == HEAP_ERROR_SUCCESS);
    }
    if (!vmm_heap_commit(p, commit_size, prot, &error_code, which)) {
        ASSERT_NOT_REACHED();
        LOG(GLOBAL, LOG_HEAP, 1, "heap_mmap_reserve_post_stack: commit failed " PFX "\n",
            error_code);
        if (!vmm_is_reserved_unit(vmh, stack_reserve_end, reserve_size)) {
            os_heap_free(p, reserve_size, &error_code);
            ASSERT(error_code == HEAP_ERROR_SUCCESS);
        }
        dynamo_vm_areas_unlock();
        STATS_INC(mmap_no_share_stack_region);
        return heap_mmap_reserve(reserve_size, commit_size, prot, which);
    }
    account_for_memory(p, reserve_size, prot, true /*add now*/,
                       false _IF_DEBUG("heap_mmap_reserve_post_stack"));
    dynamo_vm_areas_unlock();
    /* We rely on this for freeing in absence of dcontext */
    ASSERT((ptr_uint_t)p - GUARD_PAGE_ADJUSTMENT / 2 !=
           ALIGN_BACKWARD(p, DYNAMO_OPTION(vmm_block_size)));
#ifdef DEBUG_MEMORY
    memset(vmm_get_writable_addr(p, which), HEAP_ALLOCATED_BYTE, commit_size);
#endif
    LOG(GLOBAL, LOG_HEAP, 2, "heap_mmap w/ stack: %d bytes [/ %d] @ " PFX "\n",
        commit_size, reserve_size, p);
    STATS_ADD_PEAK(mmap_capacity, commit_size);
    STATS_ADD_PEAK(mmap_reserved_only, (reserve_size - commit_size));
    STATS_INC(mmap_share_stack_region);
    return p;
}

/* De-commits memory that was allocated in the same allocation region as this
 * thread's stack (case 9474).
 */
void
heap_munmap_post_stack(dcontext_t *dcontext, void *p, size_t reserve_size,
                       which_vmm_t which)
{
    /* We would require a valid dcontext and compare to the stack reserve end,
     * but on detach we have no dcontext, so we instead use block alignment.
     */
    DOCHECK(1, {
        if (dcontext != NULL && dcontext != GLOBAL_DCONTEXT &&
            DYNAMO_OPTION(vm_reserve) && DYNAMO_OPTION(stack_shares_gencode)) {
            bool at_stack_end = (p == dcontext->dstack + GUARD_PAGE_ADJUSTMENT / 2);
            bool at_block_start = ((ptr_uint_t)p - GUARD_PAGE_ADJUSTMENT / 2 ==
                                   ALIGN_BACKWARD(p, DYNAMO_OPTION(vmm_block_size)));
            ASSERT((at_stack_end && !at_block_start) ||
                   (!at_stack_end && at_block_start));
        }
    });
    if (!DYNAMO_OPTION(vm_reserve) || !DYNAMO_OPTION(stack_shares_gencode) ||
        (ptr_uint_t)p - GUARD_PAGE_ADJUSTMENT / 2 ==
            ALIGN_BACKWARD(p, DYNAMO_OPTION(vmm_block_size))) {
        heap_munmap(p, reserve_size, which);
    } else {
        /* Detach makes it a pain to pass in the commit size so
         * we use the reserve size, which works fine.
         */
        release_memory_and_update_areas((app_pc)p, reserve_size, true /*decommit*/,
                                        true /*update now*/, which);
        LOG(GLOBAL, LOG_HEAP, 2, "heap_munmap_post_stack: %d bytes @ " PFX "\n",
            reserve_size, p);
        STATS_SUB(mmap_capacity, reserve_size);
        STATS_SUB(mmap_reserved_only, reserve_size);
    }
}

/* Use heap_mmap to allocate large chunks of memory. */
void *
heap_mmap(size_t size, uint prot, which_vmm_t which)
{
    return heap_mmap_ex(size, size, prot, true, which);
}

/* free memory-mapped storage */
void
heap_munmap_ex(void *p, size_t size, bool guarded, which_vmm_t which)
{
#ifdef DEBUG_MEMORY
    /* can't set to HEAP_UNALLOCATED_BYTE since really not in our address
     * space anymore */
#endif
    release_guarded_real_memory((vm_addr_t)p, size, true /*update DR areas immediately*/,
                                guarded, which);

    DOSTATS({
        /* avoid problem w/ being called by cleanup_and_terminate after
         * dynamo_process_exit
         */
        if (!dynamo_exited_log_and_stats) {
            LOG(GLOBAL, LOG_HEAP, 2, "heap_munmap: %d bytes @ " PFX "\n", size, p);
            STATS_SUB(mmap_capacity, size);
            STATS_SUB(mmap_reserved_only, size);
        }
    });
}

/* free memory-mapped storage */
void
heap_munmap(void *p, size_t size, which_vmm_t which)
{
    heap_munmap_ex(p, size, true /*guarded*/, which);
}

/* use stack_alloc to build a stack -- it returns TOS
 * For -stack_guard_pages, also allocates an extra page
 * on the bottom and uses it to detect overflows when accessed.
 */
void *
stack_alloc(size_t size, byte *min_addr)
{
    void *p;
    /* we reserve and commit at once for now
     * FIXME case 2330: commit-on-demand could allow larger max sizes w/o
     * hurting us in the common case
     */
    size_t alloc_size = size;
    if (!has_guard_pages(VMM_STACK | VMM_PER_THREAD) && DYNAMO_OPTION(stack_guard_pages))
        alloc_size += PAGE_SIZE;
    p = get_guarded_real_memory(alloc_size, alloc_size, MEMPROT_READ | MEMPROT_WRITE,
                                true, true, min_addr,
                                VMM_STACK | VMM_PER_THREAD _IF_DEBUG("stack_alloc"));
    if (!has_guard_pages(VMM_STACK | VMM_PER_THREAD) && DYNAMO_OPTION(stack_guard_pages))
        p = (byte *)p + PAGE_SIZE;
#ifdef DEBUG_MEMORY
    memset(p, HEAP_ALLOCATED_BYTE, size);
#endif

    if (DYNAMO_OPTION(stack_guard_pages)) {
        /* XXX: maybe we should this option a count of how many pages, to catch
         * overflow that uses a large stride and skips over one page (UNIX-only
         * since Windows code always uses chkstk to trigger guard pages).
         */
        /* We place a guard on UNIX signal stacks too: although we can't report
         * such overflows, we'd rather have a clear crash than memory corruption
         * from clobbering whatever memory is below the stack.
         */
        /* mark the bottom page non-accessible to trap stack overflow */
        byte *guard = (byte *)p - PAGE_SIZE;
#ifdef WINDOWS
        /* Only a committed page can be a guard page. */
        /* XXX: this doesn't work well with -vm_reserve where the kernel will
         * auto-expand the stack into adjacent allocations below the stack.
         */
        heap_error_code_t error_code;
        if (vmm_heap_commit(guard, PAGE_SIZE, MEMPROT_READ | MEMPROT_WRITE, &error_code,
                            VMM_STACK | VMM_PER_THREAD))
            mark_page_as_guard(guard);
#else
        /* For UNIX we just mark it as inaccessible. */
        if (!has_guard_pages(VMM_STACK | VMM_PER_THREAD))
            set_protection(guard, PAGE_SIZE, MEMPROT_READ);
#endif
    }

    RSTATS_ADD_PEAK(stack_capacity, size);
    /* stack grows from high to low */
    return (void *)((ptr_uint_t)p + size);
}

/* free stack storage */
void
stack_free(void *p, size_t size)
{
    size_t alloc_size;
    if (size == 0)
        size = DYNAMORIO_STACK_SIZE;
    alloc_size = size;
    p = (void *)((vm_addr_t)p - size);
    if (!has_guard_pages(VMM_STACK | VMM_PER_THREAD) &&
        DYNAMO_OPTION(stack_guard_pages)) {
        alloc_size += PAGE_SIZE;
        p = (byte *)p - PAGE_SIZE;
    }
    release_guarded_real_memory((vm_addr_t)p, alloc_size,
                                true /*update DR areas immediately*/, true,
                                VMM_STACK | VMM_PER_THREAD);
    if (IF_DEBUG_ELSE(!dynamo_exited_log_and_stats, true))
        RSTATS_SUB(stack_capacity, size);
}

/* only checks d_r_initstack and current dcontext
 * does not check any dstacks on the callback stack (win32) */
bool
is_stack_overflow(dcontext_t *dcontext, byte *sp)
{
    /* ASSUMPTION: size of stack is DYNAMORIO_STACK_SIZE = dynamo_options.stack_size
     * Currently sideline violates that for a thread stack, and we have separated
     * -signal_stack_size, but all dstacks and d_r_initstack should be this size.
     */
    byte *bottom = dcontext->dstack - DYNAMORIO_STACK_SIZE;
    if (!DYNAMO_OPTION(stack_guard_pages) && !DYNAMO_OPTION(per_thread_guard_pages))
        return false;
    /* see if in bottom guard page of dstack */
    if (sp >= bottom - PAGE_SIZE && sp < bottom)
        return true;
    /* now check the d_r_initstack */
    bottom = d_r_initstack - DYNAMORIO_STACK_SIZE;
    if (sp >= bottom - PAGE_SIZE && sp < bottom)
        return true;
    return false;
}

byte *
d_r_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
             map_flags_t map_flags)
{
    byte *view;
    /* memory alloc/dealloc and updating DR list must be atomic */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    view = os_map_file(f, size, offs, addr, prot, map_flags);
    if (view != NULL) {
        STATS_ADD_PEAK(file_map_capacity, *size);
        account_for_memory((void *)view, *size, prot, true /*add now*/,
                           true /*image*/
                           _IF_DEBUG("map_file"));
    }
    dynamo_vm_areas_unlock();
    return view;
}

bool
d_r_unmap_file(byte *map, size_t size)
{
    bool success;
    ASSERT(map != NULL && ALIGNED(map, PAGE_SIZE));
    size = ALIGN_FORWARD(size, PAGE_SIZE);
    /* memory alloc/dealloc and updating DR list must be atomic */
    dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    success = os_unmap_file(map, size);
    if (success) {
        /* Only update the all_memory_areas on success.
         * It should still be atomic to the outside observers.
         */
        update_dynamo_areas_on_release(map, map + size, true /*remove now*/);
        STATS_SUB(file_map_capacity, size);
    }
    dynamo_vm_areas_unlock();
    return success;
}

/* We cannot incrementally keep dynamo vm area list up to date due to
 * circular dependencies bet vmareas and global heap (trust me, I've tried
 * to support it with reentrant routines and recursive locks, the hard part
 * is getting add_vm_area to be reentrant or to queue up adding areas,
 * I think this solution is much more elegant, plus it avoids race conditions
 * between DR memory allocation and the vmareas list by ensuring the list
 * is up to date at the exact time of each query).
 * Instead we on-demand walk the units.
 * Freed units can usually be removed incrementally, except when we
 * hold the heap_unit_lock when we run out of memory -- when we set
 * a flag telling the caller of this routine to remove all heap areas
 * from the vm list prior to calling us to add the real ones back in.
 * Re-adding everyone is the simplest policy, so we don't have to keep
 * track of who's been added.
 * The caller is assumed to hold the dynamo vm areas write lock.
 */
void
heap_vmareas_synch_units()
{
    heap_unit_t *u, *next;
    /* we again have circular dependence w/ vmareas if it happens to need a
     * new unit in the course of adding these areas, so we use a recursive lock!
     * furthermore, we need to own the global lock now, to avoid deadlock with
     * another thread who does global_alloc and then needs a new unit!
     * which means that the global_alloc lock must be recursive since vmareas
     * may need to global_alloc...
     */
    /* if chance could own both locks, must grab both now
     * always grab global_alloc first, then we won't have deadlocks
     */
    acquire_recursive_lock(&global_alloc_lock);
    acquire_recursive_lock(&heap_unit_lock);
    if (dynamo_areas_pending_remove) {
        dynamo_areas_pending_remove = false;
        remove_dynamo_heap_areas();

        /* When heap units are removed from the dynamo_area, they should be
         * marked so.  See case 4196.
         */
        for (u = heapmgt->heap.units; u != NULL; u = u->next_global)
            u->in_vmarea_list = false;
        for (u = heapmgt->heap.dead; u != NULL; u = u->next_global)
            u->in_vmarea_list = false;
    }
    for (u = heapmgt->heap.units; u != NULL; u = next) {
        /* Make sure to add any guard page on each side, as well. */
        uint offs = has_guard_pages(u->which) ? (uint)PAGE_SIZE : 0;
        app_pc start = (app_pc)u - offs;
        /* support un-aligned heap reservation end: PR 415269 (though as
         * part of that PR we shouldn't have un-aligned anymore)
         */
        app_pc end_align = (app_pc)ALIGN_FORWARD(UNIT_RESERVED_END(u), PAGE_SIZE);
        app_pc end = end_align + offs;
        /* u can be moved to dead list, so cache the next link; case 4196. */
        next = u->next_global;
        /* case 3045: areas inside the vmheap reservation are not added to the list */
        if (!u->in_vmarea_list &&
            !is_vmm_reserved_address(start, end - start, NULL, NULL)) {
            /* case 4196 if next is used by dynamo_vmareas then next
             * may become dead if vector is resized, then u should be
             * alive and u->next_global should be reset AFTER add  */
            bool next_may_die =
                /* keep breaking abstractions */
                is_dynamo_area_buffer(UNIT_GET_START_PC(next));
            /* dynamo_areas.buf vector may get resized and u can either
             * go to the dead unit list, or it can be released back to
             * the OS.  We'll mark it as being in vmarea list to avoid
             * re-adding when going through dead one's, and we'll mark
             * _before_ the potential free.  If dynamo_areas.buf is
             * freed back to the OS we'll have another iteration in
             * update_dynamo_vm_areas() until we get fully
             * synchronized, so we don't need to worry about the
             * inconsistency.
             */
            u->in_vmarea_list = true;
            add_dynamo_heap_vm_area(start, end, true, false _IF_DEBUG("heap unit"));
            /* NOTE: Since we could mark_dynamo_vm_areas_stale instead of adding to
             * it, we may lose prot info about this unit.
             * FIXME: Currently, this is done only at one place, which allocates unit
             * as MEMPROT_READ | MEMPROT_WRITE.  If other places are added, then this
             * needs to change.
             */
            update_all_memory_areas((app_pc)u, end_align, MEMPROT_READ | MEMPROT_WRITE,
                                    DR_MEMTYPE_DATA); /* unit */
            if (offs != 0) {
                /* guard pages */
                update_all_memory_areas((app_pc)u - offs, (app_pc)u, MEMPROT_NONE,
                                        DR_MEMTYPE_DATA);
                update_all_memory_areas(end_align, end, MEMPROT_NONE, DR_MEMTYPE_DATA);
            }
            if (next_may_die) {
                STATS_INC(num_vmareas_resize_synch);
                /* if next was potentially on dead row, then current
                 * should still be live and point to the next live
                 */
                next = u->next_global;
            }
        }
    }
    for (u = heapmgt->heap.dead; u != NULL; u = next) {
        uint offs = has_guard_pages(u->which) ? (uint)PAGE_SIZE : 0;
        app_pc start = (app_pc)u - offs;
        /* support un-aligned heap reservation end: PR 415269 (though as
         * part of that PR we shouldn't have un-aligned anymore)
         */
        app_pc end_align = (app_pc)ALIGN_FORWARD(UNIT_RESERVED_END(u), PAGE_SIZE);
        app_pc end = end_align + offs;
        /* u can be moved to live list, so cache the next link; case 4196. */
        next = u->next_global;
        /* case 3045: areas inside the vmheap reservation are not added to the list */
        if (!u->in_vmarea_list &&
            !is_vmm_reserved_address(start, end - start, NULL, NULL)) {
            u->in_vmarea_list = true;
            add_dynamo_heap_vm_area(start, end, true, false _IF_DEBUG("dead heap unit"));
            update_all_memory_areas((app_pc)u, end_align, MEMPROT_READ | MEMPROT_WRITE,
                                    DR_MEMTYPE_DATA); /* unit */
            if (offs != 0) {
                /* guard pages */
                update_all_memory_areas(start, (app_pc)u, MEMPROT_NONE, DR_MEMTYPE_DATA);
                update_all_memory_areas(end_align, end, MEMPROT_NONE, DR_MEMTYPE_DATA);
            }
            /* case 4196 if next was put back on live list for
             * dynamo_areas.buf vector, then next will no longer be a
             * valid iterator over dead list
             */
            /* keep breaking abstractions */
            if (is_dynamo_area_buffer(UNIT_GET_START_PC(next))) {
                STATS_INC(num_vmareas_resize_synch);
                ASSERT_NOT_TESTED();
                next = u->next_global;
            }
        }
    }
    release_recursive_lock(&heap_unit_lock);
    release_recursive_lock(&global_alloc_lock);
}

/* shared between global and global_unprotected */
static void *
common_global_heap_alloc(thread_units_t *tu, size_t size HEAPACCT(which_heap_t which))
{
#ifdef STATIC_LIBRARY
    if (standalone_library) {
        /* i#3316: Use regular malloc for better multi-thread performance and better
         * interoperability with tools like sanitizers.
         * We limit this to static DR b/c we can have a direct call to libc malloc
         * there and b/c that is the common use case for standalone mode these days.
         */
        return malloc(size);
    }
#endif
    void *p;
    acquire_recursive_lock(&global_alloc_lock);
    p = common_heap_alloc(tu, size HEAPACCT(which));
    release_recursive_lock(&global_alloc_lock);
    if (p == NULL) {
        /* circular dependence solution: we need to hold DR lock before
         * global alloc lock -- so we back out, grab it, and retry
         */
        dynamo_vm_areas_lock();
        acquire_recursive_lock(&global_alloc_lock);
        p = common_heap_alloc(tu, size HEAPACCT(which));
        release_recursive_lock(&global_alloc_lock);
        dynamo_vm_areas_unlock();
    }
    ASSERT(p != NULL);
    return p;
}

/* shared between global and global_unprotected */
static void
common_global_heap_free(thread_units_t *tu, void *p,
                        size_t size HEAPACCT(which_heap_t which))
{
#ifdef STATIC_LIBRARY
    if (standalone_library) {
        /* i#3316: Use regular malloc for better multi-thread performance and better
         * interoperability with tools like sanitizers.
         * We limit this to static DR b/c we can have a direct call to libc malloc
         * there and b/c that is the common use case for standalone mode these days.
         */
        free(p);
        return;
    }
#endif
    bool ok;
    if (p == NULL) {
        ASSERT(false && "attempt to free NULL");
        return;
    }

    acquire_recursive_lock(&global_alloc_lock);
    ok = common_heap_free(tu, p, size HEAPACCT(which));
    release_recursive_lock(&global_alloc_lock);
    if (!ok) {
        /* circular dependence solution: we need to hold DR lock before
         * global alloc lock -- so we back out, grab it, and retry
         */
        dynamo_vm_areas_lock();
        acquire_recursive_lock(&global_alloc_lock);
        ok = common_heap_free(tu, p, size HEAPACCT(which));
        release_recursive_lock(&global_alloc_lock);
        dynamo_vm_areas_unlock();
    }
    ASSERT(ok);
}

/* these functions use the global heap instead of a thread's heap: */
void *
global_heap_alloc(size_t size HEAPACCT(which_heap_t which))
{
    void *p;
    /* We pay the cost of this branch to support using DR's decode routines from the
     * regular DR library and not just drdecode, to support libraries that would use
     * drdecode but that also have to work with full DR (i#2499).
     */
    if (heapmgt == &temp_heapmgt &&
        /* We prevent recrusion by checking for a field that heap_init writes. */
        !heapmgt->global_heap_writable) {
        /* TODO i#2499: We have no control point currently to call standalone_exit().
         * We need to develop a solution with atexit() or ELF destructors or sthg.
         */
        standalone_init();
    }
    p = common_global_heap_alloc(&heapmgt->global_units, size HEAPACCT(which));
    ASSERT(p != NULL);
    LOG(GLOBAL, LOG_HEAP, 6, "\nglobal alloc: " PFX " (%d bytes)\n", p, size);
    return p;
}

void
global_heap_free(void *p, size_t size HEAPACCT(which_heap_t which))
{
    common_global_heap_free(&heapmgt->global_units, p, size HEAPACCT(which));
    LOG(GLOBAL, LOG_HEAP, 6, "\nglobal free: " PFX " (%d bytes)\n", p, size);
}

/* reallocate area
   allocates new_num elements of element_size
   if ptr is NULL acts like global_heap_alloc,
   copies an old_num elements of given size in the new area */
/* FIXME: do a heap_realloc and a special_heap_realloc too */
void *
global_heap_realloc(void *ptr, size_t old_num, size_t new_num,
                    size_t element_size HEAPACCT(which_heap_t which))
{
    void *new_area = global_heap_alloc(new_num * element_size HEAPACCT(which));
    if (ptr) {
        memcpy(new_area, ptr, (old_num < new_num ? old_num : new_num) * element_size);
        global_heap_free(ptr, old_num * element_size HEAPACCT(which));
    }
    return new_area;
}

/* size does not include guard pages (if any) and is reserved, but only
 * DYNAMO_OPTION(heap_commit_increment) is committed up front
 */
static heap_unit_t *
heap_create_unit(thread_units_t *tu, size_t size, bool must_be_new)
{
    heap_unit_t *u = NULL, *dead = NULL, *prev_dead = NULL;

    /* we do not restrict size to unit max as we have to make larger-than-max
     * units for oversized requests
     */

    /* modifying heap list and DR areas must be atomic, and must grab
     * DR area lock before heap_unit_lock
     */
    ASSERT(safe_to_allocate_or_free_heap_units());
    dynamo_vm_areas_lock();
    /* take from dead list if possible */
    acquire_recursive_lock(&heap_unit_lock);

    /* FIXME: need to unprotect units that we're going to perform
     * {next,prev}_global assignments too -- but need to know whether
     * to re-protect -- do all at once, or each we need?  add a writable
     * flag to heap_unit_t?
     */

    if (!must_be_new) {
        for (dead = heapmgt->heap.dead; dead != NULL &&
             (UNIT_RESERVED_SIZE(dead) < size || dead->which != tu->which);
             prev_dead = dead, dead = dead->next_global)
            ; /* nothing */
    }
    if (dead != NULL) {
        if (prev_dead == NULL)
            heapmgt->heap.dead = dead->next_global;
        else
            prev_dead->next_global = dead->next_global;
        u = dead;
        heapmgt->heap.num_dead--;
        RSTATS_DEC(heap_num_free);
        release_recursive_lock(&heap_unit_lock);
        LOG(GLOBAL, LOG_HEAP, 2,
            "Re-using dead heap unit: " PFX "-" PFX " %d KB (need %d KB)\n", u,
            ((byte *)u) + size, UNIT_RESERVED_SIZE(u) / 1024, size / 1024);
    } else {
        size_t commit_size = DYNAMO_OPTION(heap_commit_increment);
        release_recursive_lock(&heap_unit_lock); /* do not hold while asking for memory */
        /* create new unit */
        /* Just cap the commit size to this unit's size, to support raising the
         * commit size w/o setting a lot of different unit size parameters.
         */
        if (commit_size > size)
            commit_size = size;
        u = (heap_unit_t *)get_guarded_real_memory(size, commit_size,
                                                   MEMPROT_READ | MEMPROT_WRITE, false,
                                                   true, NULL, tu->which _IF_DEBUG(""));
        /* FIXME: handle low memory conditions by freeing units, + fcache units? */
        ASSERT(u);
        LOG(GLOBAL, LOG_HEAP, 2, "New heap unit: " PFX "-" PFX "\n", u,
            ((byte *)u) + size);
        /* u is kept at top of unit itself, so displace start pc */
        u->start_pc = (heap_pc)(((ptr_uint_t)u) + sizeof(heap_unit_t));
        u->end_pc = ((heap_pc)u) + commit_size;
        u->reserved_end_pc = ((heap_pc)u) + size;
        u->in_vmarea_list = false;
        u->which = tu->which;
        RSTATS_ADD_PEAK(heap_capacity, commit_size);
        /* FIXME: heap sizes are not always page-aligned so stats will be off */
        STATS_ADD_PEAK(heap_reserved_only, (u->reserved_end_pc - u->end_pc));
    }
    RSTATS_ADD_PEAK(heap_num_live, 1);

    ASSERT(u->which == tu->which);
    u->cur_pc = u->start_pc;
    u->next_local = NULL;
    DODEBUG({
        u->id = tu->num_units;
        tu->num_units++;
    });

    acquire_recursive_lock(&heap_unit_lock);
    u->next_global = heapmgt->heap.units;
    if (heapmgt->heap.units != NULL)
        heapmgt->heap.units->prev_global = u;
    u->prev_global = NULL;
    heapmgt->heap.units = u;
    release_recursive_lock(&heap_unit_lock);
    dynamo_vm_areas_unlock();

#ifdef DEBUG_MEMORY
    DOCHECK(CHKLVL_MEMFILL,
            memset(u->start_pc, HEAP_UNALLOCATED_BYTE, u->end_pc - u->start_pc););
#endif
    return u;
}

/* dcontext only used to determine whether a global unit or not */
static void
heap_free_unit(heap_unit_t *unit, dcontext_t *dcontext)
{
    heap_unit_t *u, *prev_u;
#ifdef DEBUG_MEMORY
    /* Unit should already be set to all HEAP_UNALLOCATED by the individual
     * frees and the free list cleanup, verify. */
    /* NOTE - this assert fires if any memory in the unit wasn't freed. This
     * would include memory allocated ACCT_TOMBSTONE (which we don't currently
     * use). Using ACCT_TOMBSTONE is dangerous since we will still free the
     * unit here (say at proc or thread exit) even if there are ACCT_TOMBSTONE
     * allocations in it. */
    /* Note, this memset check is done only on the special heap unit header,
     * not on the unit itself - FIXME: case 10434.  Maybe we should embed the
     * special heap unit header in the first special heap unit itself. */
    /* The hotp_only leak relaxation below is for case 9588 & 9593.  */
    DOCHECK(CHKLVL_MEMFILL, {
        CLIENT_ASSERT(IF_HOTP(hotp_only_contains_leaked_trampoline(
                                  unit->start_pc, unit->end_pc - unit->start_pc) ||)
                              /* i#157: private loader => system lib allocs come here =>
                               * they don't always clean up.  we have to relax here, but
                               * our threadunits_exit checks should find all leaks anyway.
                               */
                              heapmgt->global_units.acct.cur_usage[ACCT_LIBDUP] > 0 ||
                          is_region_memset_to_char(unit->start_pc,
                                                   unit->end_pc - unit->start_pc,
                                                   HEAP_UNALLOCATED_BYTE)
                          /* don't assert when client does premature exit as it's
                           * hard for Extension libs, etc. to clean up in such situations
                           */
                          || client_requested_exit,
                      "memory leak detected");
    });
#endif
    /* modifying heap list and DR areas must be atomic, and must grab
     * DR area lock before heap_unit_lock
     */
    ASSERT(safe_to_allocate_or_free_heap_units());
    dynamo_vm_areas_lock();
    acquire_recursive_lock(&heap_unit_lock);

    /* FIXME: need to unprotect units that we're going to perform
     * {next,prev}_global assignments too -- but need to know whether
     * to re-protect -- do all at once, or each we need?  add a writable
     * flag to heap_unit_t?
     */

    /* remove from live list */
    if (unit->prev_global != NULL) {
        unit->prev_global->next_global = unit->next_global;
    } else
        heapmgt->heap.units = unit->next_global;
    if (unit->next_global != NULL) {
        unit->next_global->prev_global = unit->prev_global;
    }
    /* prev_global is not used in the dead list */
    unit->prev_global = NULL;
    RSTATS_DEC(heap_num_live);

    /* heuristic: don't keep around more dead units than max(5, 1/4 num threads)
     * FIXME: share the policy with the fcache dead unit policy
     * also, don't put special larger-than-max units on free list -- though
     * we do now have support for doing so (after PR 415269)
     */
    if (UNITALLOC(unit) <= HEAP_UNIT_MAX_SIZE &&
        (heapmgt->heap.num_dead < 5 ||
         heapmgt->heap.num_dead * 4U <= (uint)d_r_get_num_threads())) {
        /* Keep dead list sorted small-to-large to avoid grabbing large
         * when can take small and then needing to allocate when only
         * have small left.  Helps out with lots of small threads.
         */
        for (u = heapmgt->heap.dead, prev_u = NULL;
             u != NULL && UNIT_RESERVED_SIZE(u) < UNIT_RESERVED_SIZE(unit);
             prev_u = u, u = u->next_global)
            ;
        if (prev_u == NULL) {
            unit->next_global = heapmgt->heap.dead;
            heapmgt->heap.dead = unit;
        } else {
            unit->next_global = u;
            prev_u->next_global = unit;
        }
        heapmgt->heap.num_dead++;
        release_recursive_lock(&heap_unit_lock);
        RSTATS_ADD_PEAK(heap_num_free, 1);
    } else {
        /* don't need to hold this while freeing since still hold DR areas lock */
        release_recursive_lock(&heap_unit_lock);
        LOG(GLOBAL, LOG_HEAP, 1,
            "\tfreeing excess dead unit " PFX "-" PFX " [-" PFX "]\n", unit,
            UNIT_COMMIT_END(unit), UNIT_RESERVED_END(unit));
        really_free_unit(unit);
    }
    /* FIXME: shrink lock-held path if we see contention */
    dynamo_vm_areas_unlock();
}

#ifdef DEBUG_MEMORY
static heap_unit_t *
find_heap_unit(thread_units_t *tu, heap_pc p, size_t size)
{
    /* FIXME (case 6198): this is a perf hit in debug builds.  But, we can't use
     * a new vmvector b/c of circular dependences.  Proposal: use custom data
     * field of vm_area_t in dynamo_areas list for heap entries to store a pointer
     * to the heap_unit_t struct, and add a backpointer to the owning thread_units_t
     * in heap_unit_t.  Then have to make sure it's ok lock-wise to query the
     * dynamo_areas in the middle of an alloc or a free.  It should be but for
     * global alloc and free we will have to grab the dynamo_areas lock up front
     * every time instead of the very rare times now when we need a new unit.
     */
    heap_unit_t *unit;
    ASSERT(!POINTER_OVERFLOW_ON_ADD(p, size)); /* should not overflow */
    for (unit = tu->top_unit;
         unit != NULL && (p < unit->start_pc || p + size > unit->end_pc);
         unit = unit->next_local)
        ;
    return unit;
}
#endif

static void
threadunits_init(dcontext_t *dcontext, thread_units_t *tu, size_t size, bool reachable)
{
    int i;
    DODEBUG({ tu->num_units = 0; });
    tu->which = VMM_HEAP | (reachable ? VMM_REACHABLE : 0);
    if (dcontext != GLOBAL_DCONTEXT) {
        /* Tradeoff (i#4424): no guard pages on per-thread units, to
         * save space for many-threaded apps.  These units are rarely used.
         * Note that this also precludes sharing dead units between thread-private
         * and shared heaps due to the different "which" value.
         */
        tu->which |= VMM_PER_THREAD;
    }
    tu->top_unit = heap_create_unit(tu, size, false /*can reuse*/);
    tu->cur_unit = tu->top_unit;
    tu->dcontext = dcontext;
    tu->writable = true;
#ifdef HEAP_ACCOUNTING
    memset(&tu->acct, 0, sizeof(tu->acct));
#endif
    for (i = 0; i < BLOCK_TYPES; i++)
        tu->free_list[i] = NULL;
}

#ifdef HEAP_ACCOUNTING
#    define MAX_5_DIGIT 99999
static void
print_tu_heap_statistics(thread_units_t *tu, file_t logfile, const char *prefix)
{
    int i;
    size_t total = 0, cur = 0;
    LOG(logfile, LOG_HEAP | LOG_STATS, 1, "%s heap breakdown:\n", prefix);
    for (i = 0; i < ACCT_LAST; i++) {
        /* print out cur since this is done periodically, not just at end */
        LOG(logfile, LOG_HEAP | LOG_STATS, 1,
            "%12s: cur=%5" SZFC "K, max=%5" SZFC "K, #=%7d, 1=", whichheap_name[i],
            tu->acct.cur_usage[i] / 1024, tu->acct.max_usage[i] / 1024,
            tu->acct.num_alloc[i]);
        if (tu->acct.max_single[i] <= MAX_5_DIGIT)
            LOG(logfile, LOG_HEAP | LOG_STATS, 1, "%5" SZFC, tu->acct.max_single[i]);
        else {
            LOG(logfile, LOG_HEAP | LOG_STATS, 1, "%4" SZFC "K",
                tu->acct.max_single[i] / 1024);
        }
        LOG(logfile, LOG_HEAP | LOG_STATS, 1, ", new=%5" SZFC "K, re=%5" SZFC "K\n",
            tu->acct.alloc_new[i] / 1024, tu->acct.alloc_reuse[i] / 1024);
        total += tu->acct.max_usage[i];
        cur += tu->acct.cur_usage[i];
    }
    LOG(logfile, LOG_HEAP | LOG_STATS, 1, "Total cur usage: %6" SZFC " KB\n", cur / 1024);
    LOG(logfile, LOG_HEAP | LOG_STATS, 1,
        "Total max (not nec. all used simult.): %6" SZFC " KB\n", total / 1024);
}

void
print_heap_statistics()
{
    /* just do cur thread, don't try to walk all threads */
    dcontext_t *dcontext = get_thread_private_dcontext();
    DOSTATS({
        uint i;
        LOG(GLOBAL, LOG_STATS, 1, "Heap bucket usage counts and wasted memory:\n");
        for (i = 0; i < BLOCK_TYPES; i++) {
            LOG(GLOBAL, LOG_STATS | LOG_HEAP, 1,
                "%2d %3d count=%9u peak_count=%9u peak_wasted=%9u peak_align=%9u\n", i,
                BLOCK_SIZES[i], block_total_count[i], block_peak_count[i],
                block_peak_wasted[i], block_peak_align_pad[i]);
        }
    });
    if (dcontext != NULL) {
        thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
        if (th != NULL) { /* may not be initialized yet */
            print_tu_heap_statistics(th->local_heap, THREAD, "Thread");
            ASSERT(th->nonpersistent_heap != NULL);
            print_tu_heap_statistics(th->nonpersistent_heap, THREAD,
                                     "Thread non-persistent");
            if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
                ASSERT(th->reachable_heap != NULL);
                print_tu_heap_statistics(th->reachable_heap, THREAD, "Thread reachable");
            }
        }
    }
    print_tu_heap_statistics(&heapmgt->global_nonpersistent_units, GLOBAL,
                             "Non-persistent global units");
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        print_tu_heap_statistics(&heapmgt->global_reachable_units, GLOBAL,
                                 "Reachable global units");
    }
    print_tu_heap_statistics(&heapmgt->global_unprotected_units, GLOBAL,
                             "Uprotected global units");
    print_tu_heap_statistics(&global_racy_units, GLOBAL, "Racy Up-to-date Process");
    print_tu_heap_statistics(&heapmgt->global_units, GLOBAL,
                             "Updated-at-end Process (max is total of maxes)");
}

static void
add_heapacct_to_global_stats(heap_acct_t *acct)
{
    /* add this thread's stats to the accurate (non-racy) global stats
     * FIXME: this gives a nice in-one-place total, but loses the
     * global-heap-only stats -- perhaps should add a total_units stats
     * to capture total and leave global alone here?
     */
    uint i;
    acquire_recursive_lock(&global_alloc_lock);
    for (i = 0; i < ACCT_LAST; i++) {
        heapmgt->global_units.acct.alloc_reuse[i] += acct->alloc_reuse[i];
        heapmgt->global_units.acct.alloc_new[i] += acct->alloc_new[i];
        heapmgt->global_units.acct.cur_usage[i] += acct->cur_usage[i];
        /* FIXME: these maxes are now not simultaneous max but sum-of-maxes */
        heapmgt->global_units.acct.max_usage[i] += acct->max_usage[i];
        heapmgt->global_units.acct.max_single[i] += acct->max_single[i];
        heapmgt->global_units.acct.num_alloc[i] += acct->num_alloc[i];
    }
    release_recursive_lock(&global_alloc_lock);
}
#endif

/* dcontext only used for debugging */
static void
threadunits_exit(thread_units_t *tu, dcontext_t *dcontext)
{
    heap_unit_t *u, *next_u;
#ifdef DEBUG
    size_t total_heap_used = 0;
#    ifdef HEAP_ACCOUNTING
    int j;
#    endif
#endif
#ifdef DEBUG_MEMORY
    /* verify and clear (for later asserts) the free list */
    uint i;
    for (i = 0; i < BLOCK_TYPES; i++) {
        heap_pc p, next_p;
        for (p = tu->free_list[i]; p != NULL; p = next_p) {
            next_p = *(heap_pc *)p;
            /* clear the pointer to the next free for later asserts */
            *(heap_pc *)p = (heap_pc)HEAP_UNALLOCATED_PTR_UINT;
            DOCHECK(CHKLVL_MEMFILL, {
                if (i < BLOCK_TYPES - 1) {
                    CLIENT_ASSERT(is_region_memset_to_char(p, BLOCK_SIZES[i],
                                                           HEAP_UNALLOCATED_BYTE),
                                  "memory corruption detected");
                } else {
                    /* variable sized blocks */
                    CLIENT_ASSERT(is_region_memset_to_char(p, VARIABLE_SIZE(p),
                                                           HEAP_UNALLOCATED_BYTE),
                                  "memory corruption detected");
                    /* clear the header for later asserts */
                    MEMSET_HEADER(p, HEAP_UNALLOCATED);
                }
            });
        }
        tu->free_list[i] = NULL;
    }
#endif
    u = tu->top_unit;
    while (u != NULL) {
        DOLOG(1, LOG_HEAP | LOG_STATS, {
            size_t num_used = u->cur_pc - u->start_pc;
            total_heap_used += num_used;
            LOG(THREAD, LOG_HEAP | LOG_STATS, 1,
                "Heap unit %d @" PFX "-" PFX " [-" PFX "] (" SZFMT " [/" SZFMT
                "] KB): used " SZFMT " bytes\n",
                u->id, u, UNIT_COMMIT_END(u), UNIT_RESERVED_END(u),
                (UNIT_COMMIT_SIZE(u)) / 1024, (UNIT_RESERVED_SIZE(u)) / 1024, num_used);
        });
        next_u = u->next_local;
        heap_free_unit(u, dcontext);
        u = next_u;
    }
    LOG(THREAD, LOG_HEAP | LOG_STATS, 1, "\tTotal heap used: " SZFMT " KB\n",
        total_heap_used / 1024);
#if defined(DEBUG) && defined(HEAP_ACCOUNTING)
    /* FIXME: separate scopes: smaller functions for DEBUG_MEMORY x HEAP_ACCOUNTING */
    for (j = 0; j < ACCT_LAST; j++) {
        size_t usage = tu->acct.cur_usage[j];
        if (usage > 0) {
            LOG(THREAD, LOG_HEAP | LOG_STATS, 1,
                "WARNING: %s " SZFMT " bytes not freed!\n", whichheap_name[j],
                tu->acct.cur_usage[j]);

#    ifdef HOT_PATCHING_INTERFACE /* known leaks for case 9593 */
            if (DYNAMO_OPTION(hotp_only) &&
                ((j == ACCT_SPECIAL && usage == (size_t)hotp_only_tramp_bytes_leaked) ||
                 /* +4 is for the allocation's header; internal to heap mgt. */
                 (j == ACCT_MEM_MGT &&
                  usage == (size_t)(get_special_heap_header_size() + 4) &&
                  hotp_only_tramp_bytes_leaked > 0)))
                continue;
#    endif
            if (j != ACCT_TOMBSTONE /* known leak */ &&
                /* i#157: private loader => system lib allocs come here =>
                 * they don't always clean up
                 */
                j != ACCT_LIBDUP && INTERNAL_OPTION(heap_accounting_assert)) {
                SYSLOG_INTERNAL_ERROR("memory leak: %s " SZFMT " bytes not freed",
                                      whichheap_name[j], tu->acct.cur_usage[j]);
                /* Don't assert when client does premature exit as it's
                 * hard for Extension libs, etc. to clean up in such situations:
                 */
                CLIENT_ASSERT(client_requested_exit || false, "memory leak detected");
            }
        }
    }
    if (tu != &heapmgt->global_units)
        add_heapacct_to_global_stats(&tu->acct);

    DOLOG(1, LOG_HEAP | LOG_STATS, {
        print_tu_heap_statistics(tu, THREAD,
                                 dcontext == GLOBAL_DCONTEXT ? "Process" : "Thread");
    });
#endif /* defined(DEBUG) && defined(HEAP_ACCOUNTING) */
}

void
heap_thread_reset_init(dcontext_t *dcontext)
{
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    ASSERT(th->nonpersistent_heap != NULL);
    threadunits_init(dcontext, th->nonpersistent_heap,
                     DYNAMO_OPTION(initial_heap_nonpers_size), false);
}

void
heap_thread_init(dcontext_t *dcontext)
{
    thread_heap_t *th =
        (thread_heap_t *)global_heap_alloc(sizeof(thread_heap_t) HEAPACCT(ACCT_MEM_MGT));
    dcontext->heap_field = (void *)th;
    th->local_heap = (thread_units_t *)global_heap_alloc(sizeof(thread_units_t)
                                                             HEAPACCT(ACCT_MEM_MGT));
    threadunits_init(dcontext, th->local_heap, HEAP_UNIT_MIN_SIZE, false);
    th->nonpersistent_heap = (thread_units_t *)global_heap_alloc(
        sizeof(thread_units_t) HEAPACCT(ACCT_MEM_MGT));
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        th->reachable_heap = (thread_units_t *)global_heap_alloc(
            sizeof(thread_units_t) HEAPACCT(ACCT_MEM_MGT));
        threadunits_init(dcontext, th->reachable_heap, HEAP_UNIT_MIN_SIZE, true);
    } else
        th->reachable_heap = NULL;
    heap_thread_reset_init(dcontext);
#ifdef UNIX
    th->fork_copy_start = NULL;
    th->fork_copy_size = 0;
#endif
}

void
heap_thread_reset_free(dcontext_t *dcontext)
{
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    ASSERT(th->nonpersistent_heap != NULL);
    /* FIXME: free directly rather than sending to dead list for
     * heap_reset_free() to free!
     * FIXME: for reset, don't free last unit so don't have to
     * recreate in reset_init()
     */
    threadunits_exit(th->nonpersistent_heap, dcontext);
}

void
heap_thread_exit(dcontext_t *dcontext)
{
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    threadunits_exit(th->local_heap, dcontext);
    heap_thread_reset_free(dcontext);
    global_heap_free(th->local_heap, sizeof(thread_units_t) HEAPACCT(ACCT_MEM_MGT));
    ASSERT(th->nonpersistent_heap != NULL);
    global_heap_free(th->nonpersistent_heap,
                     sizeof(thread_units_t) HEAPACCT(ACCT_MEM_MGT));
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        ASSERT(th->reachable_heap != NULL);
        threadunits_exit(th->reachable_heap, dcontext);
        global_heap_free(th->reachable_heap,
                         sizeof(thread_units_t) HEAPACCT(ACCT_MEM_MGT));
    }
    global_heap_free(th, sizeof(thread_heap_t) HEAPACCT(ACCT_MEM_MGT));
}

#if defined(DEBUG_MEMORY) && defined(DEBUG)
void
print_free_list(thread_units_t *tu, int i)
{
    void *p;
    int len = 0;
    dcontext_t *dcontext = tu->dcontext;
    LOG(THREAD, LOG_HEAP, 1, "Free list for size %d (== %d bytes):\n", i, BLOCK_SIZES[i]);
    p = (void *)tu->free_list[i];
    while (p != NULL) {
        LOG(THREAD, LOG_HEAP, 1, "\tp = " PFX "\n", p);
        len++;
        p = *((char **)p);
    }
    LOG(THREAD, LOG_HEAP, 1, "Total length is %d\n", len);
}
#endif

/* Used for both heap_unit_t and special_heap_unit_t.
 * Returns the amount it increased the unit by, so caller should increment
 * end_pc.
 * Both end_pc and reserved_end_pc are assumed to be open-ended!
 */
static size_t
common_heap_extend_commitment(heap_pc cur_pc, heap_pc end_pc, heap_pc reserved_end_pc,
                              size_t size_need, uint prot, which_vmm_t which)
{
    if (end_pc < reserved_end_pc && !POINTER_OVERFLOW_ON_ADD(cur_pc, size_need)) {
        /* extend commitment if have more reserved */
        size_t commit_size = DYNAMO_OPTION(heap_commit_increment);
        /* simpler to just not support taking very last page in address space */
        if (POINTER_OVERFLOW_ON_ADD(end_pc, commit_size))
            return 0;
        if (cur_pc + size_need > end_pc + commit_size) {
            commit_size =
                ALIGN_FORWARD(cur_pc + size_need - (ptr_uint_t)end_pc, PAGE_SIZE);
        }
        if (end_pc + commit_size > reserved_end_pc ||
            POINTER_OVERFLOW_ON_ADD(end_pc,
                                    commit_size) /*overflow seen in PR 518644 */) {
            /* commit anyway before caller moves on to new unit so that
             * we keep an invariant that all units but the current one
             * are fully committed, so our algorithm for looking at the end
             * of prior units holds
             */
            commit_size = reserved_end_pc - end_pc;
        }
        ASSERT(!POINTER_OVERFLOW_ON_ADD(end_pc, commit_size) &&
               end_pc + commit_size <= reserved_end_pc);
        extend_commitment(end_pc, commit_size, prot, false /* extension */, which);
#ifdef DEBUG_MEMORY
        memset(vmcode_get_writable_addr(end_pc), HEAP_UNALLOCATED_BYTE, commit_size);
#endif
        /* caller should do end_pc += commit_size */
        RSTATS_ADD_PEAK(heap_capacity, commit_size);
        /* FIXME: heap sizes are not always page-aligned so stats will be off */
        STATS_SUB(heap_reserved_only, commit_size);
        ASSERT(end_pc <= reserved_end_pc);
        return commit_size;
    } else
        return 0;
}

static void
heap_unit_extend_commitment(heap_unit_t *u, size_t size_need, uint prot)
{
    u->end_pc += common_heap_extend_commitment(u->cur_pc, u->end_pc, u->reserved_end_pc,
                                               size_need, prot, u->which);
}

/* allocate storage on the DR heap
 * returns NULL iff caller needs to grab dynamo_vm_areas_lock() and retry
 */
static void *
common_heap_alloc(thread_units_t *tu, size_t size HEAPACCT(which_heap_t which))
{
    heap_unit_t *u = tu->cur_unit;
    heap_pc p = NULL;
    int bucket = 0;
    size_t alloc_size, aligned_size;
#if defined(DEBUG_MEMORY) && defined(DEBUG)
    size_t check_alloc_size;
    dcontext_t *dcontext = tu->dcontext;
    /* DrMem i#999: private libs can be heap-intensive and our checks here
     * can have a prohibitive perf cost!
     */
    uint chklvl = CHKLVL_MEMFILL + (IF_HEAPACCT_ELSE(which == ACCT_LIBDUP ? 1 : 0, 0));
    ASSERT_CURIOSITY(which != ACCT_TOMBSTONE &&
                     "Do you really need to use ACCT_TOMBSTONE? (potentially dangerous)");
#endif
    ASSERT(size > 0); /* we don't want to pay check cost in release */
    ASSERT(size < MAX_VALID_HEAP_ALLOCATION && "potential integer overflow");
    /* we prefer to crash than having heap overflows */
    if (size > MAX_VALID_HEAP_ALLOCATION) {
        /* This routine can currently accommodate without integer
         * overflows sizes up to UINT_MAX - sizeof(heap_unit_t), but
         * INT_MAX should be more than enough.
         *
         * Caller will likely crash, but that is better than a heap
         * overflow, where a crash would be the best we can hope for.
         */
        return NULL;
    }

    /* NOTE - all of our buckets are sized to preserve alignment, so this can't change
     * which bucket is used. */
    aligned_size = ALIGN_FORWARD(size, HEAP_ALIGNMENT);
    while (aligned_size > BLOCK_SIZES[bucket])
        bucket++;
    if (bucket == BLOCK_TYPES - 1)
        alloc_size = aligned_size + HEADER_SIZE;
    else
        alloc_size = BLOCK_SIZES[bucket];
    ASSERT(size <= alloc_size);
#ifdef DEBUG_MEMORY
    /* case 10292: use original calculated size for later check */
    check_alloc_size = alloc_size;
#endif
    if (alloc_size > MAXROOM) {
        /* too big for normal unit, build a special unit just for this allocation */
        /* don't need alloc_size or even aligned_size, just need size */
        heap_unit_t *new_unit, *prev;
        /* we page-align to avoid wasting space if unit gets reused later */
        size_t unit_size = ALIGN_FORWARD(size + sizeof(heap_unit_t), PAGE_SIZE);
        ASSERT(size < unit_size && "overflow");

        if (!safe_to_allocate_or_free_heap_units()) {
            /* circular dependence solution: we need to hold DR lock before
             * global alloc lock -- so we back out, grab it, and then come back
             */
            return NULL;
        }

        /* Can reuse a dead unit if large enough: we'll just not use any
         * excess size until this is freed and put back on dead list.
         * (Currently we don't put oversized units on dead list though.)
         */
        new_unit = heap_create_unit(tu, unit_size, false /*can be reused*/);
        /* we want to commit the whole alloc right away */
        heap_unit_extend_commitment(new_unit, size, MEMPROT_READ | MEMPROT_WRITE);
        prev = tu->top_unit;
        alloc_size = size; /* should we include page-alignment? */
        /* insert prior to cur unit (new unit will be full, so keep cur unit
         * where it is)
         */
        while (prev != u && prev->next_local != u) {
            ASSERT(prev != NULL && prev->next_local != NULL);
            prev = prev->next_local;
        }
        if (prev == u) {
            ASSERT(prev == tu->top_unit);
            tu->top_unit = new_unit;
        } else
            prev->next_local = new_unit;
        new_unit->next_local = u;
#ifdef DEBUG_MEMORY
        LOG(THREAD, LOG_HEAP, 3, "\tCreating new oversized heap unit %d (%d [/%d] KB)\n",
            new_unit->id, UNIT_COMMIT_SIZE(new_unit) / 1024,
            UNIT_RESERVED_SIZE(new_unit) / 1024);
#endif
        p = new_unit->start_pc;
        new_unit->cur_pc += size;
        ACCOUNT_FOR_ALLOC(alloc_new, tu, which, size, size); /* use alloc_size? */
        goto done_allocating;
    }
    if (tu->free_list[bucket] != NULL) {
        if (bucket == BLOCK_TYPES - 1) {
            /* variable-length blocks, try to find one big enough */
            size_t sz;
            heap_pc next = tu->free_list[bucket];
            heap_pc prev;
            do {
                prev = p;
                p = next;
                /* aligned_size is written right _before_ next pointer */
                sz = VARIABLE_SIZE(next);
                next = *((heap_pc *)p);
            } while (aligned_size > sz && next != NULL);
            if (aligned_size <= sz) {
                ASSERT(ALIGNED(next, HEAP_ALIGNMENT));
                /* found one, extract from free list */
                if (p == tu->free_list[bucket])
                    tu->free_list[bucket] = next;
                else
                    *((heap_pc *)prev) = next;
#ifdef DEBUG_MEMORY
                LOG(THREAD, LOG_HEAP, 2,
                    "Variable-size block: allocating " PFX " (%d bytes [%d aligned] in "
                    "%d block)\n",
                    p, size, aligned_size, sz);
                /* ensure memory we got from the free list is in a heap unit */
                DOCHECK(CHKLVL_DEFAULT, { /* expensive check */
                                          ASSERT(find_heap_unit(tu, p, sz) != NULL);
                });
#endif
                ASSERT(ALIGNED(sz, HEAP_ALIGNMENT));
                alloc_size = sz + HEADER_SIZE;
                ACCOUNT_FOR_ALLOC(alloc_reuse, tu, which, alloc_size, aligned_size);
            } else {
                /* no free block big enough available */
                p = NULL;
            }
        } else {
            /* fixed-length free block available */
            p = tu->free_list[bucket];
            tu->free_list[bucket] = *((heap_pc *)p);
            ASSERT(ALIGNED(tu->free_list[bucket], HEAP_ALIGNMENT));
#ifdef DEBUG_MEMORY
            /* ensure memory we got from the free list is in a heap unit */
            DOCHECK(CHKLVL_DEFAULT, { /* expensive check */
                                      ASSERT(find_heap_unit(tu, p, alloc_size) != NULL);
            });
#endif
            ACCOUNT_FOR_ALLOC(alloc_reuse, tu, which, alloc_size, aligned_size);
        }
    }
    if (p == NULL) {
        /* no free blocks, grab a new one */
        /* FIXME: if no more heap but lots of larger blocks available,
         * should use the larger blocks instead of failing! */

        /* see if room for allocation size */
        ASSERT(ALIGNED(u->cur_pc, HEAP_ALIGNMENT));
        ASSERT(ALIGNED(alloc_size, HEAP_ALIGNMENT));
        if (u->cur_pc + alloc_size > u->end_pc ||
            POINTER_OVERFLOW_ON_ADD(u->cur_pc, alloc_size) /*xref PR 495961*/) {
            /* We either have to extend the current unit or, failing that,
             * allocate a new unit. */
            if (!safe_to_allocate_or_free_heap_units()) {
                /* circular dependence solution: we need to hold dynamo areas
                 * lock before global alloc lock in case we end up adding a new
                 * unit or we hit oom (which may free units) while extending
                 * the commmitment -- so we back out, grab it, and then come
                 * back. */
                return NULL;
            }
            /* try to extend if possible */
            heap_unit_extend_commitment(u, alloc_size, MEMPROT_READ | MEMPROT_WRITE);
            /* check again after extending commit */
            if (u->cur_pc + alloc_size > u->end_pc ||
                POINTER_OVERFLOW_ON_ADD(u->cur_pc, alloc_size) /*xref PR 495961*/) {
                /* no room, look for room at end of previous units
                 * FIXME: instead should put end of unit space on free list!
                 */
                heap_unit_t *prev = tu->top_unit;
                while (1) {
                    /* make sure we do NOT steal space from oversized units,
                     * who though they may have extra space from alignment
                     * may be freed wholesale when primary alloc is freed
                     */
                    if (UNITALLOC(prev) <= HEAP_UNIT_MAX_SIZE &&
                        !POINTER_OVERFLOW_ON_ADD(prev->cur_pc, alloc_size) &&
                        prev->cur_pc + alloc_size <= prev->end_pc) {
                        tu->cur_unit = prev;
                        u = prev;
                        break;
                    }
                    if (prev->next_local == NULL) {
                        /* no room anywhere, so create new unit
                         * double size of old unit (until hit max size)
                         */
                        heap_unit_t *new_unit;
                        size_t unit_size;

                        unit_size = UNITALLOC(u) * 2;
                        while (unit_size < alloc_size + UNITOVERHEAD)
                            unit_size *= 2;
                        if (unit_size > HEAP_UNIT_MAX_SIZE)
                            unit_size = HEAP_UNIT_MAX_SIZE;
                        ASSERT(unit_size > UNITOVERHEAD);
                        new_unit = heap_create_unit(tu, unit_size, false /*can reuse*/);
                        prev->next_local = new_unit;
#ifdef DEBUG_MEMORY
                        LOG(THREAD, LOG_HEAP, 2,
                            "\tCreating new heap unit %d (%d [/%d] KB)\n", new_unit->id,
                            UNIT_COMMIT_SIZE(new_unit) / 1024,
                            UNIT_RESERVED_SIZE(new_unit) / 1024);
#endif
                        /* use new unit for all future non-free-list allocations
                         * we'll try to use the free room at the end of the old unit(s)
                         * only when we next run out of room
                         */
                        tu->cur_unit = new_unit;
                        u = new_unit;
                        /* may need to extend now if alloc_size is large */
                        heap_unit_extend_commitment(u, alloc_size,
                                                    MEMPROT_READ | MEMPROT_WRITE);
                        /* otherwise would have been bigger than MAXROOM */
                        ASSERT(alloc_size <= (ptr_uint_t)(u->end_pc - u->cur_pc));
                        break;
                    }
                    prev = prev->next_local;
                }
            }
        }

        p = u->cur_pc;
        if (bucket == BLOCK_TYPES - 1) {
            /* we keep HEADER_SIZE bytes to store the size */
            p += HEADER_SIZE;
            VARIABLE_SIZE(p) = aligned_size;
        }
        u->cur_pc += alloc_size;

        ACCOUNT_FOR_ALLOC(alloc_new, tu, which, alloc_size, aligned_size);
    }
    DOSTATS({
        /* do this before done_allocating: want to ignore special-unit allocs */
        ATOMIC_ADD(int, block_count[bucket], 1);
        ATOMIC_ADD(int, block_total_count[bucket], 1);
        /* FIXME: should atomically store inc-ed val in temp to avoid races w/ max */
        ATOMIC_MAX(int, block_peak_count[bucket], block_count[bucket]);
        ASSERT(CHECK_TRUNCATE_TYPE_uint(alloc_size - aligned_size));
        ATOMIC_ADD(int, block_wasted[bucket], (int)(alloc_size - aligned_size));
        /* FIXME: should atomically store val in temp to avoid races w/ max */
        ATOMIC_MAX(int, block_peak_wasted[bucket], block_wasted[bucket]);
        if (aligned_size > size) {
            ASSERT(CHECK_TRUNCATE_TYPE_uint(aligned_size - size));
            ATOMIC_ADD(int, block_align_pad[bucket], (int)(aligned_size - size));
            /* FIXME: should atomically store val in temp to avoid races w/ max */
            ATOMIC_MAX(int, block_peak_align_pad[bucket], block_align_pad[bucket]);
            STATS_ADD_PEAK(heap_align, aligned_size - size);
            LOG(GLOBAL, LOG_STATS, 5,
                "alignment mismatch: %s ask %d, aligned is %d -> %d pad\n",
                IF_HEAPACCT_ELSE(whichheap_name[which], ""), size, aligned_size,
                aligned_size - size);
        }
        if (bucket == BLOCK_TYPES - 1) {
            STATS_ADD(heap_headers, HEADER_SIZE);
            STATS_INC(heap_allocs_variable);
        } else {
            STATS_INC(heap_allocs_buckets);
            if (alloc_size > aligned_size) {
                STATS_ADD_PEAK(heap_bucket_pad, alloc_size - aligned_size);
                LOG(GLOBAL, LOG_STATS, 5,
                    "bucket mismatch: %s ask (aligned) %d, got %d, -> %d\n",
                    IF_HEAPACCT_ELSE(whichheap_name[which], ""), aligned_size, alloc_size,
                    alloc_size - aligned_size);
            }
        }
    });
done_allocating:
#ifdef DEBUG_MEMORY
    if (bucket == BLOCK_TYPES - 1 && check_alloc_size <= MAXROOM) {
        /* verify is unallocated memory, skip possible free list next pointer */
        DOCHECK(chklvl, {
            CLIENT_ASSERT(
                is_region_memset_to_char(p + sizeof(heap_pc *),
                                         (alloc_size - HEADER_SIZE) - sizeof(heap_pc *),
                                         HEAP_UNALLOCATED_BYTE),
                "memory corruption detected");
        });
        LOG(THREAD, LOG_HEAP, 6,
            "\nalloc var " PFX "-" PFX " %d bytes, ret " PFX "-" PFX " %d bytes\n",
            p - HEADER_SIZE, p - HEADER_SIZE + alloc_size, alloc_size, p, p + size, size);
        /* there can only be extra padding if we took off of the free list */
        DOCHECK(chklvl,
                memset(p + size, HEAP_PAD_BYTE, (alloc_size - HEADER_SIZE) - size););
    } else {
        /* verify is unallocated memory, skip possible free list next pointer */
        DOCHECK(chklvl, {
            CLIENT_ASSERT(is_region_memset_to_char(p + sizeof(heap_pc *),
                                                   alloc_size - sizeof(heap_pc *),
                                                   HEAP_UNALLOCATED_BYTE),
                          "memory corruption detected");
        });
        LOG(THREAD, LOG_HEAP, 6,
            "\nalloc fix or oversize " PFX "-" PFX " %d bytes, ret " PFX "-" PFX
            " %d bytes\n",
            p, p + alloc_size, alloc_size, p, p + size, size);
        DOCHECK(chklvl, memset(p + size, HEAP_PAD_BYTE, alloc_size - size););
    }
    DOCHECK(chklvl, memset(p, HEAP_ALLOCATED_BYTE, size););
#    ifdef HEAP_ACCOUNTING
    LOG(THREAD, LOG_HEAP, 6, "\t%s\n", whichheap_name[which]);
#    endif
#endif
    return (void *)p;
}

/* allocate storage on the thread's private heap */
void *
heap_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which))
{
    thread_units_t *tu;
    void *ret_val;
    if (dcontext == GLOBAL_DCONTEXT)
        return global_heap_alloc(size HEAPACCT(which));
    tu = ((thread_heap_t *)dcontext->heap_field)->local_heap;
    ret_val = common_heap_alloc(tu, size HEAPACCT(which));
    ASSERT(ret_val != NULL);
    return ret_val;
}

/* free heap storage
 * returns false if caller needs to grab dynamo_vm_areas_lock() and retry
 */
static bool
common_heap_free(thread_units_t *tu, void *p_void,
                 size_t size HEAPACCT(which_heap_t which))
{
    int bucket = 0;
    heap_pc p = (heap_pc)p_void;
#if defined(DEBUG) && (defined(DEBUG_MEMORY) || defined(HEAP_ACCOUNTING))
    dcontext_t *dcontext = tu->dcontext;
    /* DrMem i#999: private libs can be heap-intensive and our checks here
     * can have a prohibitive perf cost!
     * XXX: b/c of re-use we have to memset on free.  Perhaps we should
     * have a separate heap pool for private libs.  But, the overhead
     * from that final memset is small compared to what we've already
     * saved, so maybe not worth it.
     */
    uint chklvl = CHKLVL_MEMFILL + (IF_HEAPACCT_ELSE(which == ACCT_LIBDUP ? 1 : 0, 0));
#endif
    size_t alloc_size, aligned_size = ALIGN_FORWARD(size, HEAP_ALIGNMENT);
    ASSERT(size > 0); /* we don't want to pay check cost in release */
    ASSERT(p != NULL);
#ifdef DEBUG_MEMORY
    /* FIXME i#417: This curiosity assertion is trying to make sure we don't
     * perform a double free, but it can fire if we ever free a data structure
     * that has the 0xcdcdcdcd bitpattern in the first or last 4 bytes.  This
     * has happened a few times:
     *
     * - case 8802: App's eax is 0xcdcdcdcd (from an app dbg memset) and we have
     *   dcontext->allocated_start==dcontext.
     * - i#417: On Linux x64 we get rax == 0xcdcdcdcd from a memset, and
     *   opnd_create_reg() only updates part of the register before returning by
     *   value in RAX:RDX.  We initialize to zero in debug mode to work around
     *   this.
     * - i#540: On Win7 x64 we see this assert when running the TSan tests in
     *   NegativeTests.WindowsRegisterWaitForSingleObjectTest.
     *
     * For now, we've downgraded this to a curiosity, but if it fires too much
     * in the future we should maintain a separate data structure in debug mode
     * to perform this check.  We accept objects that start with 0xcdcdcdcd so
     * long as the second four bytes are not also 0xcdcdcdcd.
     */
    DOCHECK(chklvl, {
        ASSERT_CURIOSITY(
            (*(uint *)p != HEAP_UNALLOCATED_UINT ||
             (size >= 2 * sizeof(uint) && *(((uint *)p) + 1) != HEAP_UNALLOCATED_UINT)) &&
            *(uint *)(p + size - sizeof(int)) != HEAP_UNALLOCATED_UINT &&
            "attempting to free memory containing HEAP_UNALLOCATED pattern, "
            "possible double free!");
    });
#endif

    while (aligned_size > BLOCK_SIZES[bucket])
        bucket++;
    if (bucket == BLOCK_TYPES - 1)
        alloc_size = aligned_size + HEADER_SIZE;
    else
        alloc_size = BLOCK_SIZES[bucket];

    if (alloc_size > MAXROOM) {
        /* we must have used a special unit just for this allocation */
        heap_unit_t *u = tu->top_unit, *prev = NULL;

#ifdef DEBUG_MEMORY
        /* ensure we are freeing memory in a proper unit */
        DOCHECK(CHKLVL_DEFAULT, { /* expensive check */
                                  ASSERT(find_heap_unit(tu, p, size) != NULL);
        });
#endif

        if (!safe_to_allocate_or_free_heap_units()) {
            /* circular dependence solution: we need to hold DR lock before
             * global alloc lock -- so we back out, grab it, and then come back
             */
            return false;
        }

        while (u != NULL && u->start_pc != p) {
            prev = u;
            u = u->next_local;
        }
        ASSERT(u != NULL);
        /* remove this unit from this thread's list, move to dead list
         * for future use -- no problems will be caused by it being
         * larger than normal
         */
        if (prev == NULL)
            tu->top_unit = u->next_local;
        else
            prev->next_local = u->next_local;
            /* just retire the unit # */
#ifdef DEBUG_MEMORY
        LOG(THREAD, LOG_HEAP, 3, "\tFreeing oversized heap unit %d (%d KB)\n", u->id,
            size / 1024);
        /* go ahead and set unallocated, even though we are just going to free
         * the unit, is needed for an assert in heap_free_unit anyways */
        DOCHECK(CHKLVL_MEMFILL, memset(p, HEAP_UNALLOCATED_BYTE, size););
#endif
        ASSERT(size <= UNITROOM(u));
        heap_free_unit(u, tu->dcontext);
        ACCOUNT_FOR_FREE(tu, which, size);
        return true;
    } else if (bucket == BLOCK_TYPES - 1) {
        ASSERT(GET_VARIABLE_ALLOCATION_SIZE(p) >= alloc_size);
        alloc_size = GET_VARIABLE_ALLOCATION_SIZE(p);
        ASSERT(alloc_size - HEADER_SIZE >= aligned_size);
    }

#if defined(DEBUG) || defined(DEBUG_MEMORY) || defined(HEAP_ACCOUNTING)
    if (bucket == BLOCK_TYPES - 1) {
#    ifdef DEBUG_MEMORY
        LOG(THREAD, LOG_HEAP, 6,
            "\nfree var " PFX "-" PFX " %d bytes, asked " PFX "-" PFX " %d bytes\n",
            p - HEADER_SIZE, p - HEADER_SIZE + alloc_size, alloc_size, p, p + size, size);
        ASSERT_MESSAGE(chklvl, "heap overflow",
                       is_region_memset_to_char(
                           p + size, (alloc_size - HEADER_SIZE) - size, HEAP_PAD_BYTE));
        /* ensure we are freeing memory in a proper unit */
        DOCHECK(CHKLVL_DEFAULT,
                { /* expensive check */
                  ASSERT(find_heap_unit(tu, p, alloc_size - HEADER_SIZE) != NULL);
                });
        /* set used and padding memory back to unallocated */
        DOCHECK(CHKLVL_MEMFILL,
                memset(p, HEAP_UNALLOCATED_BYTE, alloc_size - HEADER_SIZE););
#    endif
        STATS_SUB(heap_headers, HEADER_SIZE);
    } else {
#    ifdef DEBUG_MEMORY
        LOG(THREAD, LOG_HEAP, 6,
            "\nfree fix " PFX "-" PFX " %d bytes, asked " PFX "-" PFX " %d bytes\n", p,
            p + alloc_size, alloc_size, p, p + size, size);
        ASSERT_MESSAGE(
            chklvl, "heap overflow",
            is_region_memset_to_char(p + size, alloc_size - size, HEAP_PAD_BYTE));
        /* ensure we are freeing memory in a proper unit */
        DOCHECK(CHKLVL_DEFAULT, { /* expensive check */
                                  ASSERT(find_heap_unit(tu, p, alloc_size) != NULL);
        });
        /* set used and padding memory back to unallocated */
        DOCHECK(CHKLVL_MEMFILL, memset(p, HEAP_UNALLOCATED_BYTE, alloc_size););
#    endif
        STATS_SUB(heap_bucket_pad, (alloc_size - aligned_size));
    }
    STATS_SUB(heap_align, (aligned_size - size));
    DOSTATS({
        ATOMIC_ADD(int, block_count[bucket], -1);
        ATOMIC_ADD(int, block_wasted[bucket], -(int)(alloc_size - aligned_size));
        ATOMIC_ADD(int, block_align_pad[bucket], -(int)(aligned_size - size));
    });
#    ifdef HEAP_ACCOUNTING
    LOG(THREAD, LOG_HEAP, 6, "\t%s\n", whichheap_name[which]);
    ACCOUNT_FOR_FREE(tu, which, alloc_size);
#    endif
#endif

    /* write next pointer */
    *((heap_pc *)p) = tu->free_list[bucket];
    ASSERT(ALIGNED(tu->free_list[bucket], HEAP_ALIGNMENT));
    tu->free_list[bucket] = p;
    ASSERT(ALIGNED(tu->free_list[bucket], HEAP_ALIGNMENT));
    return true;
}

/* free heap storage */
void
heap_free(dcontext_t *dcontext, void *p, size_t size HEAPACCT(which_heap_t which))
{
    thread_units_t *tu;
    DEBUG_DECLARE(bool ok;)
    if (dcontext == GLOBAL_DCONTEXT) {
        global_heap_free(p, size HEAPACCT(which));
        return;
    }
    tu = ((thread_heap_t *)dcontext->heap_field)->local_heap;
    DEBUG_DECLARE(ok =) common_heap_free(tu, p, size HEAPACCT(which));
    ASSERT(ok);
}

bool
local_heap_protected(dcontext_t *dcontext)
{
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    return (!th->local_heap->writable ||
            (th->nonpersistent_heap != NULL && !th->nonpersistent_heap->writable) ||
            (th->reachable_heap != NULL && !th->reachable_heap->writable));
}

static inline void
protect_local_units_helper(heap_unit_t *u, bool writable)
{
    /* win32 does not allow single protection change call on units that
     * were allocated with separate calls so we don't try to combine
     * adjacent units here
     */
    while (u != NULL) {
        change_protection(UNIT_ALLOC_START(u), UNIT_COMMIT_SIZE(u), writable);
        u = u->next_local;
    }
}

static void
protect_threadunits(thread_units_t *tu, bool writable)
{
    ASSERT(TEST(SELFPROT_LOCAL, dynamo_options.protect_mask));
    if (tu->writable == writable)
        return;
    protect_local_units_helper(tu->top_unit, writable);
    tu->writable = writable;
}

void
protect_local_heap(dcontext_t *dcontext, bool writable)
{
    thread_heap_t *th = (thread_heap_t *)dcontext->heap_field;
    protect_threadunits(th->local_heap, writable);
    protect_threadunits(th->nonpersistent_heap, writable);
    if (!REACHABLE_HEAP()) /* If off, all heap is reachable. */
        protect_threadunits(th->reachable_heap, writable);
}

/* assumption: vmm_heap_alloc only gets called for HeapUnits themselves, which
 * are protected by us here, so ignore os heap
 */
void
protect_global_heap(bool writable)
{
    ASSERT(TEST(SELFPROT_GLOBAL, dynamo_options.protect_mask));

    acquire_recursive_lock(&global_alloc_lock);

    if (heapmgt->global_heap_writable == writable) {
        release_recursive_lock(&global_alloc_lock);
        return;
    }
    /* win32 does not allow single protection change call on units that
     * were allocated with separate calls so we don't try to combine
     * adjacent units here

     * FIXME: That may no longer be true for our virtual memory manager that
     * will in fact be allocated as a single unit.  It is only in case
     * we have run out of that initial allocation that we may have to
     * keep a separate list of allocations.
     */

    if (!writable) {
        ASSERT(heapmgt->global_heap_writable);
        heapmgt->global_heap_writable = writable;
    }

    protect_local_units_helper(heapmgt->global_units.top_unit, writable);
    protect_local_units_helper(heapmgt->global_nonpersistent_units.top_unit, writable);

    if (writable) {
        ASSERT(!heapmgt->global_heap_writable);
        heapmgt->global_heap_writable = writable;
    }

    release_recursive_lock(&global_alloc_lock);
}

/* FIXME: share some code...right now these are identical to protected
 * versions except the unit used
 */
void *
global_unprotected_heap_alloc(size_t size HEAPACCT(which_heap_t which))
{
    void *p = common_global_heap_alloc(&heapmgt->global_unprotected_units,
                                       size HEAPACCT(which));
    ASSERT(p != NULL);
    LOG(GLOBAL, LOG_HEAP, 6, "\nglobal unprotected alloc: " PFX " (%d bytes)\n", p, size);
    return p;
}

void
global_unprotected_heap_free(void *p, size_t size HEAPACCT(which_heap_t which))
{
    common_global_heap_free(&heapmgt->global_unprotected_units, p, size HEAPACCT(which));
    LOG(GLOBAL, LOG_HEAP, 6, "\nglobal unprotected free: " PFX " (%d bytes)\n", p, size);
}

void *
nonpersistent_heap_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which))
{
    void *p;
    if (dcontext == GLOBAL_DCONTEXT) {
        p = common_global_heap_alloc(&heapmgt->global_nonpersistent_units,
                                     size HEAPACCT(which));
        LOG(GLOBAL, LOG_HEAP, 6, "\nglobal nonpersistent alloc: " PFX " (%d bytes)\n", p,
            size);
    } else {
        thread_units_t *nph = ((thread_heap_t *)dcontext->heap_field)->nonpersistent_heap;
        p = common_heap_alloc(nph, size HEAPACCT(which));
    }
    ASSERT(p != NULL);
    return p;
}

void
nonpersistent_heap_free(dcontext_t *dcontext, void *p,
                        size_t size HEAPACCT(which_heap_t which))
{
    if (dcontext == GLOBAL_DCONTEXT) {
        common_global_heap_free(&heapmgt->global_nonpersistent_units, p,
                                size HEAPACCT(which));
        LOG(GLOBAL, LOG_HEAP, 6, "\nglobal nonpersistent free: " PFX " (%d bytes)\n", p,
            size);
    } else {
        thread_units_t *nph = ((thread_heap_t *)dcontext->heap_field)->nonpersistent_heap;
        DEBUG_DECLARE(bool ok =) common_heap_free(nph, p, size HEAPACCT(which));
        ASSERT(ok);
    }
}

void *
heap_reachable_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which))
{
    void *p;
    /* We pay the cost of this branch to support using DR's decode routines from the
     * regular DR library and not just drdecode, to support libraries that would use
     * drdecode but that also have to work with full DR (i#2499).
     */
    if (heapmgt == &temp_heapmgt &&
        /* We prevent recursion by checking for a field that d_r_heap_init() sets and
         * d_r_heap_exit() clears.
         */
        !heapmgt->global_heap_writable) {
        /* XXX: We have no control point to call standalone_exit(). */
        standalone_init();
    }
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        if (dcontext == GLOBAL_DCONTEXT) {
            p = common_global_heap_alloc(&heapmgt->global_reachable_units,
                                         size HEAPACCT(which));
            LOG(GLOBAL, LOG_HEAP, 6, "\nglobal reachable alloc: " PFX " (%d bytes)\n", p,
                size);
        } else {
            thread_units_t *units =
                ((thread_heap_t *)dcontext->heap_field)->reachable_heap;
            p = common_heap_alloc(units, size HEAPACCT(which));
        }
    } else {
        p = heap_alloc(dcontext, size HEAPACCT(which));
    }
    ASSERT(p != NULL);
    return p;
}

void
heap_reachable_free(dcontext_t *dcontext, void *p,
                    size_t size HEAPACCT(which_heap_t which))
{
    if (!REACHABLE_HEAP()) { /* If off, all heap is reachable. */
        if (dcontext == GLOBAL_DCONTEXT) {
            common_global_heap_free(&heapmgt->global_reachable_units, p,
                                    size HEAPACCT(which));
            LOG(GLOBAL, LOG_HEAP, 6, "\nglobal reachable free: " PFX " (%d bytes)\n", p,
                size);
        } else {
            thread_units_t *units =
                ((thread_heap_t *)dcontext->heap_field)->reachable_heap;
            DEBUG_DECLARE(bool ok =) common_heap_free(units, p, size HEAPACCT(which));
            ASSERT(ok);
        }
    } else {
        heap_free(dcontext, p, size HEAPACCT(which));
    }
}

/****************************************************************************
 * SPECIAL SINGLE-ALLOC-SIZE HEAP SERVICE
 */

/* Assumptions:
 *   All allocations are of a single block size
 *   If use_lock is false, no synchronization is needed or even safe
 */

/* We use our own unit struct to give us flexibility.
 * 1) We don't always allocate the header inline.
 * 2) We are sometimes executed from and so need pc prof support.
 * 3) We don't need all the fields of heap_unit_t.
 */
typedef struct _special_heap_unit_t {
    heap_pc alloc_pc;        /* start of allocation region */
    heap_pc start_pc;        /* first address we'll give out for storage */
    heap_pc end_pc;          /* open-ended address of heap storage */
    heap_pc cur_pc;          /* current end (open) of allocated storage */
    heap_pc reserved_end_pc; /* (open) end of reserved (not nec committed) memory */
#ifdef WINDOWS_PC_SAMPLE
    profile_t *profile;
#endif
#ifdef DEBUG
    int id; /* # of this unit */
#endif
    struct _special_heap_unit_t *next;
} special_heap_unit_t;

#define SPECIAL_UNIT_COMMIT_SIZE(u) ((u)->end_pc - (u)->alloc_pc)
#define SPECIAL_UNIT_RESERVED_SIZE(u) ((u)->reserved_end_pc - (u)->alloc_pc)
#define SPECIAL_UNIT_HEADER_INLINE(u) ((u)->alloc_pc != (u)->start_pc)
#define SPECIAL_UNIT_ALLOC_SIZE(u) (SPECIAL_UNIT_RESERVED_SIZE(u))

/* the cfree list stores a next ptr and a count */
typedef struct _cfree_header {
    struct _cfree_header *next_cfree;
    uint count;
} cfree_header_t;

typedef struct _special_units_t {
    special_heap_unit_t *top_unit; /* start of linked list of heap units */
    special_heap_unit_t *cur_unit; /* current unit in heap list */
    uint block_size;               /* all blocks are this size */
    uint block_alignment;
    heap_pc free_list;
    cfree_header_t *cfree_list;
#ifdef DEBUG
    int num_units; /* total # of heap units */
#endif
    bool writable : 1; /* remember state of heap protection */
    bool executable : 1;
    /* if use_lock is false, grabbing _any_ lock may be hazardous!
     * (this isn't just an optimization, it's for correctness)
     */
    bool use_lock : 1;
    bool in_iterator : 1;
    bool persistent : 1;
    bool per_thread : 1;
    mutex_t lock;

    /* Yet another feature added: pclookup, but across multiple heaps,
     * so it's via a passed-in vector and passed-in data
     */
    vm_area_vector_t *heap_areas;
    void *lookup_retval;

#ifdef WINDOWS_PC_SAMPLE
    struct _special_units_t *next;
#endif
#ifdef HEAP_ACCOUNTING
    /* we only need one bucket for SpecialHeap but to re-use code we waste space */
    heap_acct_t acct;
#endif
} special_units_t;

#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
/* For fast exit path we need a quick way to walk all the units */
DECLARE_CXTSWPROT_VAR(static mutex_t special_units_list_lock,
                      INIT_LOCK_FREE(special_units_list_lock));
/* This is only used for profiling so we don't bother to protect it */
DECLARE_CXTSWPROT_VAR(static special_units_t *special_units_list, NULL);
#endif

#if defined(DEBUG) && defined(HEAP_ACCOUNTING) && defined(HOT_PATCHING_INTERFACE)
/* To get around the problem of the special_units_t "module" being defined after
 * the heap module in the same file.  Part of fix for case 9593 that required
 * leaking memory.
 */
static int
get_special_heap_header_size(void)
{
    return sizeof(special_units_t);
}
#endif

#ifdef WINDOWS_PC_SAMPLE
static inline bool
special_heap_profile_enabled()
{
    return (dynamo_options.profile_pcs && dynamo_options.prof_pcs_stubs >= 2 &&
            dynamo_options.prof_pcs_stubs <= 32);
}
#endif

static inline uint
get_prot(special_units_t *su)
{
    return (su->executable ? MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC
                           : MEMPROT_READ | MEMPROT_WRITE);
}

static inline which_vmm_t
get_which(special_units_t *su)
{
    which_vmm_t which = VMM_SPECIAL_HEAP;
    /* We assume that +x special heap must be reachable. */
    if (su->executable)
        which |= VMM_REACHABLE;
    if (su->per_thread)
        which |= VMM_PER_THREAD;
    return which;
}

static inline byte *
special_heap_get_writable_addr(special_units_t *su, byte *addr)
{
    if (su->executable)
        return vmcode_get_writable_addr(addr);
    return addr;
}

static inline byte *
special_heap_get_executable_addr(special_units_t *su, byte *addr)
{
    if (su->executable)
        return vmcode_get_executable_addr(addr);
    return addr;
}

static void
special_unit_extend_commitment(special_units_t *su, special_heap_unit_t *u,
                               size_t size_need, uint prot)
{
    u->end_pc += common_heap_extend_commitment(u->cur_pc, u->end_pc, u->reserved_end_pc,
                                               size_need, prot, get_which(su));
}

/* If pc is NULL, allocates memory and stores the header inside it;
 * If pc is non-NULL, allocates separate memory for the header, and
 * uses pc for the heap region (assuming size is fully committed).
 * unit_full only applies to the non-NULL case, indicating whether
 * to continue to allocate from this unit.
 */
static special_heap_unit_t *
special_heap_create_unit(special_units_t *su, byte *pc, size_t size, bool unit_full)
{
    special_heap_unit_t *u;
    size_t commit_size;
    uint prot = get_prot(su);
    ASSERT_OWN_MUTEX(su->use_lock, &su->lock);

    if (pc != NULL) {
        u = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, special_heap_unit_t, ACCT_MEM_MGT,
                            PROTECTED);
        ASSERT(u != NULL);
        u->start_pc = pc;
        u->alloc_pc = pc;
        commit_size = size;
        /* caller should arrange alignment */
        ASSERT(su->block_alignment == 0 || ALIGNED(u->start_pc, su->block_alignment));
    } else {
        ASSERT(ALIGNED(size, PAGE_SIZE));
        commit_size = DYNAMO_OPTION(heap_commit_increment);
        /* Allow the general commit size to be larger than a special unit. */
        if (commit_size > size)
            commit_size = size;
        /* create new unit */
        /* Since vmm lock, dynamo_vm_areas lock, all_memory_areas lock (on
         * linux), etc. will be acquired, and presumably !su->use_lock means
         * user can't handle ANY lock being acquired, we warn here: xref PR
         * 596768.  In release build, we try to acqure the memory anyway.  I'm
         * worried about pcprofile: can only fit ~1K in one unit and so will
         * easily run out...should it allocate additional units up front?
         * => PR 596808.
         */
        DODEBUG({
            if (su->top_unit != NULL /*init*/ && !su->use_lock) {
                SYSLOG_INTERNAL_WARNING_ONCE("potentially unsafe: allocating a new "
                                             "fragile special heap unit!");
            }
        });
        u = (special_heap_unit_t *)get_guarded_real_memory(
            size, commit_size, prot, true, true, NULL,
            get_which(su) _IF_DEBUG("special_heap"));
        ASSERT(u != NULL);
        /* Unlike gencode and code cache memory, we store the writable, since there
         * is a much narrower interface for executable addresses: just returning
         * pointers on alloc, while we have many write points.
         */
        u = (special_heap_unit_t *)special_heap_get_writable_addr(su, (byte *)u);
        u->alloc_pc = (heap_pc)u;
        /* u is kept at top of unit itself, so displace start pc */
        u->start_pc = (heap_pc)(((ptr_uint_t)u) + sizeof(special_heap_unit_t));
        if (su->block_alignment != 0) {
            STATS_ADD(heap_special_align,
                      ALIGN_FORWARD(u->start_pc, su->block_alignment) -
                          (ptr_uint_t)u->start_pc);
            u->start_pc = (heap_pc)ALIGN_FORWARD(u->start_pc, su->block_alignment);
        }
    }
    u->end_pc = u->alloc_pc + commit_size;
    u->reserved_end_pc = u->alloc_pc + size;
    if (pc != NULL && unit_full) {
        ASSERT(u->reserved_end_pc == u->end_pc);
        u->cur_pc = u->end_pc;
    } else
        u->cur_pc = u->start_pc;
    u->next = NULL;
    DODEBUG({
        u->id = su->num_units;
        su->num_units++;
    });

#ifdef WINDOWS_PC_SAMPLE
    if (special_heap_profile_enabled()) {
        u->profile = create_profile((app_pc)PAGE_START(u->start_pc), u->reserved_end_pc,
                                    dynamo_options.prof_pcs_stubs, NULL);
        start_profile(u->profile);
    } else
        u->profile = NULL;
#endif

    /* N.B.: if STATS macros ever change to grab a mutex, we could deadlock
     * if !su->use_lock!
     */
    RSTATS_ADD_PEAK(heap_capacity, commit_size);
    RSTATS_ADD_PEAK(heap_special_capacity, commit_size);
    STATS_ADD_PEAK(heap_special_units, 1);
    STATS_ADD_PEAK(heap_reserved_only, (u->reserved_end_pc - u->end_pc));

    if (su->heap_areas != NULL) {
        vmvector_add(su->heap_areas, u->alloc_pc, u->reserved_end_pc, su->lookup_retval);
    }

#ifdef DEBUG_MEMORY
    /* Don't clobber already-allocated memory */
    DOCHECK(CHKLVL_MEMFILL, {
        if (pc == NULL) {
            memset(u->start_pc, HEAP_UNALLOCATED_BYTE, u->end_pc - u->start_pc);
        }
    });
#endif
    return u;
}

/* caller must store the special_units_t *, which is opaque */
static void *
special_heap_init_internal(uint block_size, uint block_alignment, bool use_lock,
                           bool executable, bool persistent, vm_area_vector_t *vector,
                           void *vector_data, byte *heap_region, size_t heap_size,
                           bool unit_full)
{
    special_units_t *su;
    size_t unit_size = heap_size;
    if (block_alignment != 0)
        block_size = ALIGN_FORWARD(block_size, block_alignment);
    if (unit_size == 0) {
        /* Our main uses (stubs, whether global or coarse, and signal
         * pending queue) don't need a lot of space, so we have a smaller min size
         * than regular heap units which use HEAP_UNIT_MIN_SIZE.
         */
        unit_size = (block_size * 16 > PAGE_SIZE) ? (block_size * 16) : PAGE_SIZE;
    }
    if (heap_region == NULL) {
        unit_size = (size_t)ALIGN_FORWARD(unit_size, PAGE_SIZE);
    }
    su = (special_units_t *)(persistent
                                 ? global_heap_alloc(sizeof(special_units_t)
                                                         HEAPACCT(ACCT_MEM_MGT))
                                 : nonpersistent_heap_alloc(GLOBAL_DCONTEXT,
                                                            sizeof(special_units_t)
                                                                HEAPACCT(ACCT_MEM_MGT)));
    memset(su, 0, sizeof(*su));
    ASSERT(block_size >= sizeof(heap_pc *) && "need room for free list ptrs");
    ASSERT(block_size >= sizeof(heap_pc *) + sizeof(uint) &&
           "need room for cfree list ptrs");
    su->block_size = block_size;
    su->block_alignment = block_alignment;
    su->executable = executable;
    su->persistent = persistent;
    /* We assume that a lockless heap is a per-thread heap. */
    su->per_thread = !use_lock;
    su->writable = true;
    su->free_list = NULL;
    su->cfree_list = NULL;
    DODEBUG({ su->num_units = 0; });
    ASSERT((vector == NULL) == (vector_data == NULL));
    su->heap_areas = vector;
    su->lookup_retval = vector_data;
    su->in_iterator = false;
    if (use_lock)
        ASSIGN_INIT_LOCK_FREE(su->lock, special_heap_lock);
    /* For persistent cache loading we hold executable_areas lock and so
     * cannot acquire special_heap_lock -- so we do not acquire
     * for the initial unit creation, which is safe since su is still
     * private to this routine.
     */
    su->use_lock = false; /* we set to real value below */
    su->top_unit = special_heap_create_unit(su, heap_region, unit_size, unit_full);
    su->use_lock = use_lock;
#ifdef HEAP_ACCOUNTING
    memset(&su->acct, 0, sizeof(su->acct));
#endif
    su->cur_unit = su->top_unit;

#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
    if (special_heap_profile_enabled()) {
        /* Add to the global main list, which requires a lock */
        d_r_mutex_lock(&special_units_list_lock);
        su->next = special_units_list;
        special_units_list = su;
        d_r_mutex_unlock(&special_units_list_lock);
    }
#endif

    return su;
}

/* Typical usage */
void *
special_heap_init(uint block_size, bool use_lock, bool executable, bool persistent)
{
    uint alignment = 0;
    /* Some users expect alignment; not much of a space loss for those who don't.
     * XXX: find those users and have them call special_heap_init_aligned()
     * and removed this.
     */
    if (IS_POWER_OF_2(block_size))
        alignment = block_size;
    return special_heap_init_internal(block_size, alignment, use_lock, executable,
                                      persistent, NULL, NULL, NULL, 0, false);
}

void *
special_heap_init_aligned(uint block_size, uint alignment, bool use_lock, bool executable,
                          bool persistent, size_t initial_unit_size)
{
    return special_heap_init_internal(block_size, alignment, use_lock, executable,
                                      persistent, NULL, NULL, NULL, initial_unit_size,
                                      false);
}

/* Special heap w/ a vector for lookups.  Also supports a pre-created heap region
 * (heap_region, heap_region+heap_size) whose fullness is unit_full. */
void *
special_heap_pclookup_init(uint block_size, bool use_lock, bool executable,
                           bool persistent, vm_area_vector_t *vector, void *vector_data,
                           byte *heap_region, size_t heap_size, bool unit_full)
{
    uint alignment = 0;
    /* XXX: see comment in special_heap_init() */
    if (IS_POWER_OF_2(block_size))
        alignment = block_size;
    return special_heap_init_internal(block_size, alignment, use_lock, executable,
                                      persistent, vector, vector_data, heap_region,
                                      heap_size, unit_full);
}

/* Sets the vector data for the lookup vector used by the special heap */
void
special_heap_set_vector_data(void *special, void *vector_data)
{
    special_units_t *su = (special_units_t *)special;
    special_heap_unit_t *u;
    ASSERT(su->heap_areas != NULL);
    /* FIXME: more efficient to walk the vector, but no interface
     * to set the data: we'd need to expose the iterator index or
     * the vmarea struct rather than than the clean copy we have now
     */
    for (u = su->top_unit; u != NULL; u = u->next) {
        vmvector_modify_data(su->heap_areas, u->alloc_pc, u->reserved_end_pc,
                             vector_data);
    }
}

/* Returns false if the special heap has more than one unit or has a
 * non-externally-allocated unit.
 * Sets the cur pc for the only unit to end_pc.
 */
bool
special_heap_set_unit_end(void *special, byte *end_pc)
{
    special_units_t *su = (special_units_t *)special;
    if (su->top_unit->next != NULL || SPECIAL_UNIT_HEADER_INLINE(su->top_unit) ||
        end_pc < su->top_unit->start_pc || end_pc > su->top_unit->end_pc)
        return false;
    su->top_unit->cur_pc = end_pc;
    return true;
}

#ifdef WINDOWS_PC_SAMPLE
static void
special_heap_profile_stop(special_heap_unit_t *u)
{
    int sum;
    ASSERT(special_heap_profile_enabled());
    stop_profile(u->profile);
    sum = sum_profile(u->profile);
    if (sum > 0) {
        d_r_mutex_lock(&profile_dump_lock);
        print_file(profile_file, "\nDumping special heap unit profile\n%d hits\n", sum);
        dump_profile(profile_file, u->profile);
        d_r_mutex_unlock(&profile_dump_lock);
    }
}
#endif

#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
/* for fast exit path only, normal path taken care of */
void
special_heap_profile_exit()
{
    special_heap_unit_t *u;
    special_units_t *su;
    ASSERT(special_heap_profile_enabled()); /* will never be compiled in I guess :) */
    d_r_mutex_lock(&special_units_list_lock);
    for (su = special_units_list; su != NULL; su = su->next) {
        if (su->use_lock)
            d_r_mutex_lock(&su->lock);
        for (u = su->top_unit; u != NULL; u = u->next) {
            if (u->profile != NULL)
                special_heap_profile_stop(u);
            /* fast exit path: do not bother to free */
        }
        if (su->use_lock)
            d_r_mutex_unlock(&su->lock);
    }
    d_r_mutex_unlock(&special_units_list_lock);
}
#endif

void
special_heap_exit(void *special)
{
    special_units_t *su = (special_units_t *)special;
    special_heap_unit_t *u, *next_u;
#ifdef DEBUG
    size_t total_heap_used = 0;
#endif
    u = su->top_unit;
    while (u != NULL) {
        /* Assumption: it's ok to use print_lock even if !su->use_lock */
        DOLOG(1, LOG_HEAP | LOG_STATS, {
            size_t num_used = u->cur_pc - u->start_pc;
            total_heap_used += num_used;
            LOG(THREAD_GET, LOG_HEAP | LOG_STATS, 1,
                "Heap unit " SZFMT " (size " SZFMT " [/" SZFMT "] KB): used " SZFMT
                " KB\n",
                u->id, (SPECIAL_UNIT_COMMIT_SIZE(u)) / 1024,
                SPECIAL_UNIT_RESERVED_SIZE(u) / 1024, num_used / 1024);
        });
        next_u = u->next;
#ifdef WINDOWS_PC_SAMPLE
        if (u->profile != NULL) {
            ASSERT(special_heap_profile_enabled());
            special_heap_profile_stop(u);
            free_profile(u->profile);
            u->profile = NULL;
        }
#endif
        STATS_ADD(heap_special_units, -1);
        RSTATS_SUB(heap_special_capacity, SPECIAL_UNIT_COMMIT_SIZE(u));
        if (su->heap_areas != NULL) {
            vmvector_remove(su->heap_areas, u->alloc_pc, u->reserved_end_pc);
        }
        if (!SPECIAL_UNIT_HEADER_INLINE(u)) {
            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, u, special_heap_unit_t, ACCT_MEM_MGT,
                           PROTECTED);
            /* up to creator to free the heap region */
        } else {
            release_guarded_real_memory((vm_addr_t)u, SPECIAL_UNIT_RESERVED_SIZE(u),
                                        true /*update DR areas immediately*/, true,
                                        get_which(su));
        }
        u = next_u;
    }
#ifdef HEAP_ACCOUNTING
    add_heapacct_to_global_stats(&su->acct);
#endif
    LOG(THREAD_GET, LOG_HEAP | LOG_STATS, 1, "\tTotal heap used: " SZFMT " KB\n",
        total_heap_used / 1024);
#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
    if (special_heap_profile_enabled()) {
        /* Removed this special_units_t from the main list */
        d_r_mutex_lock(&special_units_list_lock);
        if (special_units_list == su)
            special_units_list = su->next;
        else {
            special_units_t *prev = special_units_list;
            ASSERT(prev != NULL);
            for (; prev->next != NULL && prev->next != su; prev = prev->next)
                ; /*nothing*/
            ASSERT(prev->next == su);
            prev->next = su->next;
        }
        d_r_mutex_unlock(&special_units_list_lock);
    }
#endif
    if (su->use_lock)
        DELETE_LOCK(su->lock);
    /* up to caller to free the vector, which is typically multi-heap */
    if (su->persistent) {
        global_heap_free(su, sizeof(special_units_t) HEAPACCT(ACCT_MEM_MGT));
    } else {
        nonpersistent_heap_free(GLOBAL_DCONTEXT, su,
                                sizeof(special_units_t) HEAPACCT(ACCT_MEM_MGT));
    }
}

void *
special_heap_calloc(void *special, uint num)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif
    special_units_t *su = (special_units_t *)special;
    special_heap_unit_t *u;
    void *p = NULL;
    bool took_free = false;
    ASSERT(num > 0);
    if (su->use_lock)
        d_r_mutex_lock(&su->lock);
    u = su->cur_unit;
    if (su->free_list != NULL && num == 1) {
        p = (void *)su->free_list;
        su->free_list = *((heap_pc *)p);
        took_free = true;
    } else if (su->cfree_list != NULL && num > 1) {
        /* FIXME: take a piece of cfree if num == 1?
         * seems better to save the bigger pieces
         */
        cfree_header_t *cfree = su->cfree_list, *prev = NULL;
        while (cfree != NULL && cfree->count < num) {
            prev = cfree;
            cfree = cfree->next_cfree;
        }
        if (cfree != NULL) {
            ASSERT(cfree->count >= num);
            took_free = true;
            if (cfree->count == num) {
                /* take it out of list altogether */
                if (prev == NULL)
                    su->cfree_list = cfree->next_cfree;
                else
                    prev->next_cfree = cfree->next_cfree;
                p = (void *)cfree;
            } else if (cfree->count == num + 1) {
                /* add single-size piece to normal free list */
                heap_pc tail = ((heap_pc)cfree) + num * su->block_size;
                *((heap_pc *)tail) = su->free_list;
                su->free_list = tail;
                p = (void *)cfree;
            } else {
                /* if take tail don't have to change free list ptrs at all */
                cfree->count -= num;
                p = (void *)(((heap_pc)cfree) + (cfree->count - num) * su->block_size);
            }
        }
    }
    if (!took_free) {
        /* no free blocks, grab a new one */
        if (u->cur_pc + su->block_size * num > u->end_pc ||
            POINTER_OVERFLOW_ON_ADD(u->cur_pc, su->block_size * num)) {
            /* simply extend commitment, if possible */
            size_t pre_commit_size = SPECIAL_UNIT_COMMIT_SIZE(u);
            special_unit_extend_commitment(su, u, su->block_size * num, get_prot(su));
            RSTATS_ADD_PEAK(heap_special_capacity,
                            SPECIAL_UNIT_COMMIT_SIZE(u) - pre_commit_size);
            /* check again after extending commit */
            if (u->cur_pc + su->block_size * num > u->end_pc ||
                POINTER_OVERFLOW_ON_ADD(u->cur_pc, su->block_size * num)) {
                /* no room, need new unit */
                special_heap_unit_t *new_unit;
                special_heap_unit_t *prev = su->top_unit;
                size_t size = SPECIAL_UNIT_ALLOC_SIZE(u);
                ASSERT(ALIGNED(size, PAGE_SIZE));
                while (prev->next != NULL)
                    prev = prev->next;
                /* create new unit double size of old unit (until hit max size) */
                if (size * 2 <= HEAP_UNIT_MAX_SIZE)
                    size *= 2;
                /* we don't support arbitrarily long sequences */
                ASSERT(su->block_size * num < size);
                new_unit = special_heap_create_unit(su, NULL, size, false /*empty*/);
                prev->next = new_unit;
                if (su->use_lock) {
                    /* if synch bad so is printing */
                    LOG(THREAD, LOG_HEAP, 3,
                        "%s: Creating new heap unit %d " PFX "-" PFX "-" PFX "\n",
                        __FUNCTION__, new_unit->id, new_unit->alloc_pc, new_unit->end_pc,
                        new_unit->reserved_end_pc);
                }
                su->cur_unit = new_unit;
                u = new_unit;
                ASSERT(u->cur_pc + su->block_size * num <= u->end_pc &&
                       !POINTER_OVERFLOW_ON_ADD(u->cur_pc, su->block_size * num));
            }
        }

        p = (void *)u->cur_pc;
        u->cur_pc += su->block_size * num;
        ACCOUNT_FOR_ALLOC(alloc_new, su, ACCT_SPECIAL, su->block_size * num,
                          su->block_size * num);
    } else {
        ACCOUNT_FOR_ALLOC(alloc_reuse, su, ACCT_SPECIAL, su->block_size * num,
                          su->block_size * num);
    }
    if (su->use_lock)
        d_r_mutex_unlock(&su->lock);

#ifdef DEBUG_MEMORY
    DOCHECK(CHKLVL_MEMFILL, memset(p, HEAP_ALLOCATED_BYTE, su->block_size * num););
#endif
    ASSERT(p != NULL);
    return (void *)special_heap_get_executable_addr(su, p);
}

void *
special_heap_alloc(void *special)
{
    return special_heap_calloc(special, 1);
}

void
special_heap_cfree(void *special, void *p, uint num)
{
    special_units_t *su = (special_units_t *)special;
    ASSERT(num > 0);
    ASSERT(p != NULL);
    /* Allow freeing while iterating w/o deadlock (iterator holds lock) */
    ASSERT(!su->in_iterator || OWN_MUTEX(&su->lock));
    if (su->use_lock && !su->in_iterator)
        d_r_mutex_lock(&su->lock);
    p = (void *)special_heap_get_writable_addr(su, p);
#ifdef DEBUG_MEMORY
    /* FIXME: ensure that p is in allocated state */
    DOCHECK(CHKLVL_MEMFILL, memset(p, HEAP_UNALLOCATED_BYTE, su->block_size * num););
#endif
    if (num == 1) {
        /* write next pointer */
        *((heap_pc *)p) = su->free_list;
        su->free_list = (heap_pc)p;
    } else {
        cfree_header_t *cfree = (cfree_header_t *)p;
        cfree->next_cfree = su->cfree_list;
        cfree->count = num;
        su->cfree_list = (cfree_header_t *)p;
    }
#ifdef HEAP_ACCOUNTING
    ACCOUNT_FOR_FREE(su, ACCT_SPECIAL, su->block_size * num);
#endif
    if (su->use_lock && !su->in_iterator)
        d_r_mutex_unlock(&su->lock);
}

void
special_heap_free(void *special, void *p)
{
    special_heap_cfree(special, p, 1);
}

bool
special_heap_can_calloc(void *special, uint num)
{
    special_units_t *su = (special_units_t *)special;
    bool can_calloc = false;

    ASSERT(num > 0);
    if (su->use_lock)
        d_r_mutex_lock(&su->lock);
    if (su->free_list != NULL && num == 1) {
        can_calloc = true;
    } else if (su->cfree_list != NULL && num > 1) {
        cfree_header_t *cfree = su->cfree_list;
        while (cfree != NULL) {
            if (cfree->count >= num) {
                can_calloc = true;
                break;
            }
            cfree = cfree->next_cfree;
        }
    }
    if (!can_calloc) {
        special_heap_unit_t *u = su->cur_unit; /* what if more units are available? */
        can_calloc = (u->cur_pc + su->block_size * num <= u->reserved_end_pc &&
                      !POINTER_OVERFLOW_ON_ADD(u->cur_pc, su->block_size * num));
    }
    if (su->use_lock)
        d_r_mutex_unlock(&su->lock);

    return can_calloc;
}

/* Special heap iterator.  Initialized with special_heap_iterator_start(), which
 * grabs the heap lock (regardless of whether synch is used for allocs), and
 * destroyed with special_heap_iterator_stop() to release the lock.
 * If the special heap uses no lock for alloc, it is up to the caller
 * to prevent race conditions causing problems.
 * Accessor special_heap_iterator_next() should be called only when
 * predicate special_heap_iterator_hasnext() is true.
 * Any mutation of the heap while iterating will result in a deadlock
 * for heaps that use locks for alloc, except for individual freeing,
 * which will proceed w/o trying to grab the lock a second time.
 * FIXME: could generalize to regular heaps if a use arises.
 */
void
special_heap_iterator_start(void *heap, special_heap_iterator_t *shi)
{
    special_units_t *su = (special_units_t *)heap;
    ASSERT(heap != NULL);
    ASSERT(shi != NULL);
    d_r_mutex_lock(&su->lock);
    shi->heap = heap;
    shi->next_unit = (void *)su->top_unit;
    su->in_iterator = true;
}

bool
special_heap_iterator_hasnext(special_heap_iterator_t *shi)
{
    ASSERT(shi != NULL);
    DOCHECK(1, {
        special_units_t *su = (special_units_t *)shi->heap;
        ASSERT(su != NULL);
        ASSERT_OWN_MUTEX(true, &su->lock);
    });
    return (shi->next_unit != NULL);
}

/* Iterator accessor:
 * Has to be initialized with special_heap_iterator_start, and should be
 * called only when special_heap_iterator_hasnext() is true.
 * Sets the area boundaries in area_start and area_end.
 */
void
special_heap_iterator_next(special_heap_iterator_t *shi /* IN/OUT */,
                           app_pc *heap_start /* OUT */, app_pc *heap_end /* OUT */)
{
    special_units_t *su;
    special_heap_unit_t *u;
    ASSERT(shi != NULL);
    su = (special_units_t *)shi->heap;
    ASSERT(su != NULL);
    ASSERT_OWN_MUTEX(true, &su->lock);
    u = (special_heap_unit_t *)shi->next_unit;
    ASSERT(u != NULL);
    if (u != NULL) { /* caller error, but paranoid */
        if (heap_start != NULL)
            *heap_start = special_heap_get_executable_addr(su, u->start_pc);
        ASSERT(u->cur_pc <= u->end_pc);
        if (heap_end != NULL)
            *heap_end = special_heap_get_executable_addr(su, u->cur_pc);
        shi->next_unit = (void *)u->next;
    }
}

void
special_heap_iterator_stop(special_heap_iterator_t *shi)
{
    special_units_t *su;
    ASSERT(shi != NULL);
    su = (special_units_t *)shi->heap;
    ASSERT(su != NULL);
    ASSERT_OWN_MUTEX(true, &su->lock);
    su->in_iterator = false;
    d_r_mutex_unlock(&su->lock);
    DODEBUG({
        shi->heap = NULL;
        shi->next_unit = NULL;
    });
}

#if defined(DEBUG) && defined(HOT_PATCHING_INTERFACE)
/* We leak hotp trampolines as part of fix for case 9593; so, during a detach
 * we can't delete the trampoline heap.  However if that heap's lock isn't
 * deleted, we'll assert.  This routine is used only for that.  Normally, we
 * should call special_heap_exit() which deletes the lock. */
void
special_heap_delete_lock(void *special)
{
    special_units_t *su = (special_units_t *)special;

    /* No one calls this routine unless they have a lock to delete. */
    ASSERT(su != NULL);
    if (su == NULL)
        return;

    ASSERT(su->use_lock);
    if (su->use_lock)
        DELETE_LOCK(su->lock);
}
#endif

/*----------------------------------------------------------------------------*/
#ifdef WINDOWS /* currently not used on linux */
/* Landing pads (introduced as part of work for PR 250294). */

/* landing_pad_areas is a vmvector made up of regions of memory called
 * landing pad areas, each of which contains multiple landing pads.  Landing
 * pads are small trampolines used to jump from the hook point to the main
 * trampoline.  This is used in both 32-bit and 64-bit DR.  In both cases it
 * will handle the problem of hook chaining by 3rd party software and us having
 * to release our hooks (we'll nop the landing pad and free the trampoline).
 * In 64-bit it also solves the problem of reachability of the 5-byte rel jmp
 * we use for hooking, i.e., that 5-byte rel jmp may not reach the main
 * trampoline in DR heap.  We have to maintain the hook a 5-byte jmp because
 * hotp_only assumes it (see PR 250294).
 *
 * A landing pad will have nothing more than a jump (5-byte rel for 32-bit DR
 * and 64-bit abs ind jmp for 64-bit DR) to the trampoline and a 5-byte rel jmp
 * back to the next instruction after the hook, plus the displaced app instrs.
 *
 * To handle hook chaining landing pads won't be released till process exit
 * (not on a detach), their first jump will just be nop'ed.  As landing pads
 * aren't released till exit, all landing pads are just incrementally allocated
 * in a landing pad area.
 *
 * Note: Landing pad areas don't necessarily have to fall within the vm_reserve
 * region or capacity, so aren't accounted by our vmm.
 *
 * Note: If in future other needs for such region specific allocation should
 * arise, then we should convert this into special_heap_alloc_in_region().  For
 * now, landing pads are the only consumers, so was decided to be acceptable.
 *
 * See win32/callback.c for emit_landing_pad_code() and landing pad usage.
 */

typedef struct {
    byte *start;      /* start of reserved region */
    byte *end;        /* end of reserved region */
    byte *commit_end; /* end of committed memory in the reserved region */
    byte *cur_ptr;    /* pointer to next allocatable landing pad memory */
    bool allocated;   /* allocated, or stolen from an app dll? */
} landing_pad_area_t;

/* Allocates a landing pad so that a hook inserted at addr_to_hook can reach
 * its trampoline via the landing pad.  The landing pad will reachable by a
 * 32-bit relative jmp from addr_to_hook.
 * Note: we may want to generalize this at some point such that the size of the
 *       landing pad is passed as an argument.
 *
 * For Windows we assume that landing_pads_to_executable_areas(true) will be
 * called once landing pads are finished being created.
 */
byte *
alloc_landing_pad(app_pc addr_to_hook)
{
    app_pc hook_region_start, hook_region_end;
    app_pc alloc_region_start, alloc_region_end;
    app_pc lpad_area_start = NULL, lpad_area_end;
    app_pc lpad = NULL;
    landing_pad_area_t *lpad_area = NULL;

    /* Allocate the landing pad area such that any hook from within the module
     * or memory region containing addr_to_hook can use the same area for a
     * landing pad.  Makes it more efficient. */
    hook_region_start = get_allocation_base(addr_to_hook);
    if (hook_region_start == NULL) { /* to support raw virtual address hooks */
        ASSERT_CURIOSITY("trying to hook raw or unallocated memory?");
        hook_region_start = addr_to_hook;
        hook_region_end = addr_to_hook;
    } else {
        hook_region_end =
            hook_region_start + get_allocation_size(hook_region_start, NULL);
        ASSERT(hook_region_end > hook_region_start); /* check overflow */
        /* If region size is > 2 GB, then it isn't an image; PE32{,+} restrict
         * images to 2 GB.  Also, if region is > 2 GB the reachability macros
         * called below will return a region smaller (and with start and end
         * inverted) than the region from which the reachability is desired,
         * i.e., some of the areas in [hook_region_start, hook_region_end)
         * won't be able to reach the region computed.
         *
         * A better choice is to pick something smaller (100 MB) because if the
         * region is close to 2 GB in size then we might not be able to
         * allocate memory for a landing pad that is reachable.
         */
        if (hook_region_end - hook_region_start > 100 * 1024 * 1024) {
            /* Try a smaller region of 100 MB around the address to hook. */
            ASSERT_CURIOSITY(false && "seeing patch region > 100 MB - DGC?");
            hook_region_start = MIN(
                addr_to_hook, MAX(hook_region_start, addr_to_hook - 50 * 1024 * 1024));
            hook_region_end =
                MAX(addr_to_hook, MIN(hook_region_end, addr_to_hook + 50 * 1024 * 1024));
        }
    }

    /* Define the region that can be reached from anywhere within the
     * hook region with a 32-bit rel jmp.
     */
    alloc_region_start = REACHABLE_32BIT_START(hook_region_start, hook_region_end);
    alloc_region_end = REACHABLE_32BIT_END(hook_region_start, hook_region_end);
    ASSERT(alloc_region_start < alloc_region_end);

    /* Check if there is an existing landing pad area within the reachable
     * region for the hook location.  If so use it, else allocate one.
     */
    d_r_write_lock(&landing_pad_areas->lock);
    if (vmvector_overlap(landing_pad_areas, alloc_region_start, alloc_region_end)) {
        /* Now we have to get that landing pad area that is FULLY contained
         * within alloc_region_start and alloc_region_end.  If a landing pad
         * area is only partially within the alloc region, then a landing pad
         * created there won't be able to reach the addr_to_hook.  BTW, that
         * landing pad area should have enough space to allocate a landing pad!
         * If these conditions are met allocate a landing pad.
         */
        vmvector_iterator_t lpad_area_iter;
        vmvector_iterator_start(landing_pad_areas, &lpad_area_iter);
        while (vmvector_iterator_hasnext(&lpad_area_iter)) {
            lpad_area =
                vmvector_iterator_next(&lpad_area_iter, &lpad_area_start, &lpad_area_end);
            if (lpad_area_start < alloc_region_end &&
                lpad_area_end > alloc_region_start &&
                (lpad_area->cur_ptr + LANDING_PAD_SIZE) < lpad_area_end) {
                /* See if enough memory in this landing pad area has been
                 * committed, if not commit more memory.
                 */
                if ((lpad_area->cur_ptr + LANDING_PAD_SIZE) >= lpad_area->commit_end) {
                    ASSERT(lpad_area->allocated);
                    extend_commitment(lpad_area->commit_end, PAGE_SIZE,
                                      MEMPROT_READ | MEMPROT_EXEC,
                                      false /* not initial commit */, VMM_SPECIAL_MMAP);
                    lpad_area->commit_end += PAGE_SIZE;
                }

                /* Update the current pointer for the landing pad area, i.e.,
                 * allocate the landing pad.
                 */
                lpad = lpad_area->cur_ptr;
                lpad_area->cur_ptr += LANDING_PAD_SIZE;
                break;
            }
        }
        vmvector_iterator_stop(&lpad_area_iter);
    }

    /* If a landing pad area wasn't found because there wasn't any in the
     * allocation region or none fully contained within the allocation region,
     * then create a new one within the allocation region.  Then allocate a
     * landing pad in it.
     */
    if (lpad == NULL) {
        bool allocated = true;
        heap_error_code_t heap_error;
        lpad_area_end = NULL;
        lpad_area_start = os_heap_reserve_in_region(
            (void *)ALIGN_FORWARD(alloc_region_start, PAGE_SIZE),
            (void *)ALIGN_BACKWARD(alloc_region_end, PAGE_SIZE), LANDING_PAD_AREA_SIZE,
            &heap_error, true /*+x*/);
        if (lpad_area_start == NULL || heap_error == HEAP_ERROR_CANT_RESERVE_IN_REGION) {
            /* Should retry with using just the aligned target address - we may
             * have made the region so large that there's nothing nearby to
             * reserve.
             */
            lpad_area_start = os_heap_reserve(
                (void *)ALIGN_FORWARD(addr_to_hook, LANDING_PAD_AREA_SIZE),
                LANDING_PAD_AREA_SIZE, &heap_error, true /*+x*/);
#    ifdef WINDOWS
            if (lpad_area_start == NULL &&
                /* We can only do this once w/ current interface.
                 * XXX: support multiple "allocs" inside libs.
                 */
                vmvector_empty(landing_pad_areas) &&
                os_find_free_code_space_in_libs(&lpad_area_start, &lpad_area_end)) {
                if (lpad_area_end - lpad_area_start >= LANDING_PAD_SIZE &&
                    /* Mark writable until we're done creating landing pads */
                    make_hookable(lpad_area_start, lpad_area_end - lpad_area_start,
                                  NULL)) {
                    /* Let's take it */
                    allocated = false;
                    /* We assume that landing_pads_to_executable_areas(true) will be
                     * called once landing pads are finished being created and we
                     * can restore to +rx there.
                     */
                    lpad_temp_writable_start = lpad_area_start;
                    lpad_temp_writable_size = lpad_area_end - lpad_area_start;
                } else
                    lpad_area_start = NULL; /* not big enough */
            }
#    endif
            if (lpad_area_start == NULL) {
                /* Even at startup when there will be enough memory,
                 * theoretically 2 GB of dlls might get packed together before
                 * we get control (very unlikely), so we can fail.  If it does,
                 * then say 'oom' and exit.
                 */
                SYSLOG_INTERNAL_WARNING("unable to reserve memory for landing pads");
                report_low_on_memory(VMM_SPECIAL_MMAP | VMM_REACHABLE, OOM_RESERVE,
                                     heap_error);
            }
        }

        /* Allocate the landing pad area as rx, allocate a landing pad in it
         * and add it to landing_pad_areas vector.  Note, we only commit 4k
         * initially even though we reserve 64k (LANDING_PAD_AREA_SIZE), to
         * avoid wastage.
         */
        if (allocated) {
            extend_commitment(lpad_area_start, PAGE_SIZE, MEMPROT_READ | MEMPROT_EXEC,
                              true /* initial commit */, VMM_SPECIAL_MMAP);
        }

        lpad_area =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, landing_pad_area_t, ACCT_VMAREAS, PROTECTED);
        lpad_area->start = lpad_area_start;
        lpad_area->end = (lpad_area_end == NULL ? lpad_area_start + LANDING_PAD_AREA_SIZE
                                                : lpad_area_end);
        lpad_area->commit_end = lpad_area_start + PAGE_SIZE;
        lpad_area->cur_ptr = lpad_area_start;
        lpad_area->allocated = allocated;
        lpad = lpad_area->cur_ptr;
        lpad_area->cur_ptr += LANDING_PAD_SIZE;

        vmvector_add(landing_pad_areas, lpad_area->start, lpad_area->end, lpad_area);
        STATS_INC(num_landing_pad_areas);
    }

    /* Landing pads aren't added to executable_areas here because not all
     * landing pads should be added.  Only the ones used for DR hooks should be
     * added to executable areas (which is done using
     * landing_pads_to_executable_areas() at the end of inserting DR hooks).
     * hotp_only related landing pads shouldn't be added to executable areas as
     * their trampolines aren't added to executable_areas.  This is why landing
     * pads aren't added to executable_areas here, at the point of allocation.
     */

    LOG(GLOBAL, LOG_ALL, 3, "%s: used " PIFX " bytes in " PFX "-" PFX "\n", __FUNCTION__,
        lpad_area->cur_ptr - lpad_area->start, lpad_area->start, lpad_area->end);

    /* Boundary check to make sure the allocation is within the landing pad area. */
    ASSERT(lpad_area->cur_ptr <= lpad_area->end);
    d_r_write_unlock(&landing_pad_areas->lock);
    return lpad;
}

/* Attempts to save space in the landing pad region by trimming the most
 * recently allocated landing pad to the actual space used.
 * Will fail if another landing pad was allocated between lpad_start
 * being allocated and this routine being called.
 */
bool
trim_landing_pad(byte *lpad_start, size_t space_used)
{
    landing_pad_area_t *lpad_area = NULL;
    bool res = false;
    d_r_write_lock(&landing_pad_areas->lock);
    if (vmvector_lookup_data(landing_pad_areas, lpad_start, NULL, NULL, &lpad_area)) {
        if (lpad_start == lpad_area->cur_ptr - LANDING_PAD_SIZE) {
            lpad_area->cur_ptr -= (LANDING_PAD_SIZE - space_used);
            res = true;
        }
    }
    d_r_write_unlock(&landing_pad_areas->lock);
    return res;
}

/* Adds or removes all landing pads from executable_areas by adding whole
 * landing pad areas.  This is done to prevent bb building from considering
 * landing pads to be selfmod code; as such, these don't have to be
 * {add,remov}ed from executable_areas for hotp_only or for thin_client mode.
 */
void
landing_pads_to_executable_areas(bool add)
{
    vmvector_iterator_t lpad_area_iter;
    app_pc lpad_area_start, lpad_area_end;
    DEBUG_DECLARE(landing_pad_area_t * lpad_area;)
    uint lpad_area_size;

    if (RUNNING_WITHOUT_CODE_CACHE())
        return;

#    ifdef WINDOWS
    if (add && lpad_temp_writable_start != NULL) {
        make_unhookable(lpad_temp_writable_start, lpad_temp_writable_size, true);
        lpad_temp_writable_start = NULL;
    }
#    endif

    /* With code cache, there should be only one landing pad area, just for
     * dr hooks in ntdll.  For 64-bit, the image entry hook will result in a
     * new landing pad.
     */
    IF_X64_ELSE(, ASSERT(landing_pad_areas->length == 1);)

    /* Just to be safe, walk through all areas in release build. */
    vmvector_iterator_start(landing_pad_areas, &lpad_area_iter);
    while (vmvector_iterator_hasnext(&lpad_area_iter)) {

        DEBUG_DECLARE(lpad_area =)
        vmvector_iterator_next(&lpad_area_iter, &lpad_area_start, &lpad_area_end);
        lpad_area_size = (uint)(lpad_area_end - lpad_area_start);
        ASSERT(lpad_area_size <= LANDING_PAD_AREA_SIZE);
        /* Current ptr should be within area. */
        ASSERT(lpad_area->cur_ptr < lpad_area_end);
        if (add) {
            add_executable_region(lpad_area_start,
                                  lpad_area_size _IF_DEBUG(
                                      "add landing pad areas after inserting dr hooks"));
        } else {
            remove_executable_region(lpad_area_start, lpad_area_size,
                                     false /* no lock */);
        }
    }
    vmvector_iterator_stop(&lpad_area_iter);
}

/* Delete landing_pad_areas and the landing_pad_area_t allocated for each
 * landing pad area.  However, release all landing pads only on process exit;
 * for detach leave the landing pads in (in case some one hooks after us they
 * shouldn't crash if they chain correctly).
 */
static void
release_landing_pad_mem(void)
{
    vmvector_iterator_t lpad_area_iter;
    app_pc lpad_area_start, lpad_area_end;
    landing_pad_area_t *lpad_area;
    heap_error_code_t heap_error;

    vmvector_iterator_start(landing_pad_areas, &lpad_area_iter);
    while (vmvector_iterator_hasnext(&lpad_area_iter)) {
        bool allocated;
        lpad_area =
            vmvector_iterator_next(&lpad_area_iter, &lpad_area_start, &lpad_area_end);
        allocated = lpad_area->allocated;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, lpad_area, landing_pad_area_t, ACCT_VMAREAS,
                       PROTECTED);
        if (!doing_detach && /* On normal exit release the landing pads. */
            allocated)
            os_heap_free(lpad_area_start, LANDING_PAD_AREA_SIZE, &heap_error);
    }
    vmvector_iterator_stop(&lpad_area_iter);
    vmvector_delete_vector(GLOBAL_DCONTEXT, landing_pad_areas);
}
#endif /* WINDOWS */
/*----------------------------------------------------------------------------*/
